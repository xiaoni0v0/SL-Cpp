# 生成 big_dec_cases.inc：BigDec 的交叉验证用例表，期望值来自 CPython 自带的两套 decimal
# 实现。用法：
#
#     python test/numeric/gen_big_dec_cases.py > test/numeric/big_dec_cases.inc
#
# 可以带一个倍数参数（默认 1）把随机用例翻倍，用来临时做更大规模的差分测试；
# 提交进仓库的那份用默认倍数生成，不然表会大到没法看，测试也会跑很久：
#
#     python test/numeric/gen_big_dec_cases.py 20 > /tmp/big_dec_cases.inc
#
# **每一组用例都拿两套实现各算一遍（libmpdec 和 _pydecimal），结果或 flags 不一致就跳过不出题。**
# CPython 这两套自己在少数地方就有分歧，官方扩展测试 `_decimal/tests/deccheck.py` 里的
# SkipHandler 明写了这是已知情况。目前撞见的两处：
#
#   * 非整数指数的 `**`：规范只要求"按 exp(y*ln(x)) 算"，不保证正确舍入。真值恰好可精确表示
#     且用定向舍入时两边差 1 ulp（`9 ** 0.5` 在 ROUND_DOWN 下 libmpdec 给 2.99、_pydecimal 给 3.00）。
#   * `exp` 在 `Emin == 0` 时：次正规判定该在舍入前还是舍入后做，两边选得不一样，值相同、
#     只差 Subnormal/Underflow 两个 flag。规范原文写的是 "before any rounding"。
#   * 陷阱开启的路径上，抛出前记 flags 的顺序两边不一样（1/3 在 Inexact 陷阱下，libmpdec 先记
#     Rounded、_pydecimal 先抛 Inexact）——陷阱表只要求两套实现"抛出的条件名"一致，抛出时的
#     flags 以 _pydecimal 为准（BigDec 是照它移植的）。
#
# BigDec 两处都站 _pydecimal 一边，行为由 02_bigdec 里的手写用例钉住。之所以做成"全表统一
# 过滤"而不是只挡这两处：分歧点是随参数（尤其是 Emin、舍入方式）漂移的，哪天有人往池子里加一档
# 参数，不该因此得到一张 BigDec 永远过不了的表。
#
# SL 的 // 和 % 向负无穷取整，跟 IBM 规范（也就是 Python 的 Decimal）向零截断不同，所以这两个
# 表的期望值是"先在超高精度下取精确的截断商/余数，再整体修正"推出来的——推导路径跟 C++ 那边
# 不一样，两边只在数学定义上一致。凡是超高精度下仍不精确的组合直接跳过，不出题。
import _pydecimal
import decimal
import random
import sys

ROUNDINGS = [
    ("Down", decimal.ROUND_DOWN),
    ("Up", decimal.ROUND_UP),
    ("HalfUp", decimal.ROUND_HALF_UP),
    ("HalfDown", decimal.ROUND_HALF_DOWN),
    ("HalfEven", decimal.ROUND_HALF_EVEN),
    ("Ceiling", decimal.ROUND_CEILING),
    ("Floor", decimal.ROUND_FLOOR),
    ("ZeroFiveUp", decimal.ROUND_05UP),
]
HALF_EVEN = decimal.ROUND_HALF_EVEN

# 顺序必须跟 C++ 里 DecCondition 前八项一致。两套实现的信号是两组不同的类，得各列一份
SIGNAL_ATTRS = [
    "Clamped",
    "DivisionByZero",
    "Inexact",
    "InvalidOperation",
    "Overflow",
    "Rounded",
    "Subnormal",
    "Underflow",
]

HUGE_PREC = 2000

# 每张表因为"两套实现不一致"跳掉了多少组，最后打到 stderr
SKIPPED = {}


def new_ctx(
    prec=28, rounding=HALF_EVEN, emax=999999, emin=-999999, mod=decimal, traps_on=None
):
    c = mod.Context(prec=prec, rounding=rounding, Emax=emax, Emin=emin)
    c.traps = {k: 0 for k in c.traps}  # 陷阱全关，只收 flags
    if traps_on is not None:
        c.traps[getattr(mod, traps_on)] = 1
    c.clear_flags()
    return c


def flags_str(c, mod=decimal):
    return ",".join(n for n in SIGNAL_ATTRS if c.flags[getattr(mod, n)])


def agreed(table, prec, rounding, emax, emin, run):
    """两套实现各跑一遍 run(ctx, mod)；结果和 flags 都一致才返回 (结果串, flags串)，否则 None。"""
    c_ctx = new_ctx(prec, rounding, emax, emin, decimal)
    c_res = run(c_ctx, decimal)
    p_ctx = new_ctx(prec, rounding, emax, emin, _pydecimal)
    p_res = run(p_ctx, _pydecimal)
    if str(c_res) != str(p_res) or flags_str(c_ctx) != flags_str(p_ctx, _pydecimal):
        SKIPPED[table] = SKIPPED.get(table, 0) + 1
        return None
    return str(c_res), flags_str(c_ctx)


def merge_outcome(c_fl, p_fl):
    """把两套实现的 flags 串合成一个期望值：抛出路径上只要求抛出的条件名一致（libmpdec 与
    _pydecimal 在抛出前记 flags 的顺序不一致，Inexact/Rounded 谁先谁后各说各话），抛出时的
    flags 以 _pydecimal 为准——BigDec 是照它移植的；非抛出路径要求完全一致。返回 None 表示
    不一致、整组跳过。"""
    if c_fl.startswith("THROW:"):
        if not p_fl.startswith("THROW:"):
            return None
        c_name = c_fl[6:].split(";", 1)[0]
        p_name = p_fl[6:].split(";", 1)[0]
        if c_name != p_name:
            return None
        return p_fl
    if p_fl.startswith("THROW:"):
        return None
    if c_fl != p_fl:
        return None
    return p_fl


def agreed_trap(table, prec, rounding, emax, emin, trap, run):
    """带单个陷阱的 agreed：run 抛异常时返回 ('', 'THROW:名字;抛出时的flags')。"""
    c_ctx = new_ctx(prec, rounding, emax, emin, decimal, trap)
    c_res = run(c_ctx, decimal)
    p_ctx = new_ctx(prec, rounding, emax, emin, _pydecimal, trap)
    p_res = run(p_ctx, _pydecimal)
    if c_res[0] != p_res[0] or merge_outcome(c_res[1], p_res[1]) is None:
        SKIPPED[table] = SKIPPED.get(table, 0) + 1
        return None
    return c_res[0], p_res[1]


def trapped(note, run):
    """把 run(ctx, mod) 包成"抛异常就返回 ('', 'THROW:名字;抛出时的flags')"的形式。
    note(mod) 在抛异常时给出 BigDec 那边的细分条件名（或 None 表示不改）——Python 抛的是
    折算后的基类（0/0 抛 InvalidOperation，BigDec 抛 DivisionUndefined，是项目里记录在案的
    设计决定），这里把名字折算回细分条件再比对。"""

    def inner(ctx, mod):
        try:
            return str(run(ctx, mod)), flags_str(ctx, mod)
        except Exception as e:  # noqa: BLE001
            name = type(e).__name__
            if name == "InvalidOperation" and note is not None:
                name = note(mod) or name
            return "", "THROW:" + name + ";" + flags_str(ctx, mod)

    return inner


# --------------------------------------------------------------------------
# // 和 % 的参考值：向负无穷取整（SL 语义，跟 IBM 规范的向零截断不同）。
# 推导路径跟 C++ 那边不一样——先在超高精度下拿到精确的截断商/余数，再整体修正——
# 两边只在数学定义上一致，不共用代码路径。凡是超高精度下仍不精确的组合直接跳过不出题
# --------------------------------------------------------------------------
def floor_divmod_one(x_s, y_s, prec, rounding, emax, emin, mod, traps_on=None):
    """返回 (商, 商的flags, 余数, 余数的flags)，或 None 表示这组推不精确、不适合当参考。
    traps_on 不为 None 时，flags 可能是 'THROW:名字;抛出时的flags'（商/余数各自独立）。"""
    x, y = mod.Decimal(x_s), mod.Decimal(y_s)
    exact = [True]

    def record(ctx, cond):
        """记信号；陷阱开着就抛细分条件本身——Python 抛的是折算后的基类，BigDec 抛的是
        细分条件（见 .ai/context.md），这里跟 BigDec 对齐。"""
        sig = {
            mod.ConversionSyntax: mod.InvalidOperation,
            mod.DivisionImpossible: mod.InvalidOperation,
            mod.DivisionUndefined: mod.InvalidOperation,
            mod.InvalidContext: mod.InvalidOperation,
        }.get(cond, cond)
        ctx.flags[sig] = 1
        if ctx.traps[sig]:
            raise cond()

    def nan_of(ctx):
        if x.is_snan() or y.is_snan():
            which = x if x.is_snan() else y
            record(ctx, mod.InvalidOperation)
            return mod.Decimal("-NaN") if which.is_signed() else mod.Decimal("NaN")
        if x.is_nan():
            return x
        if y.is_nan():
            return y
        return None

    def compute(kind, ctx):
        n = nan_of(ctx)
        if n is not None:
            return n
        sign = x.is_signed() != y.is_signed()
        if x.is_infinite():
            if kind == "q":
                if y.is_infinite():
                    record(ctx, mod.InvalidOperation)
                    return mod.Decimal("NaN")
                return mod.Decimal("-Infinity") if sign else mod.Decimal("Infinity")
            record(ctx, mod.InvalidOperation)
            return mod.Decimal("NaN")
        if y.is_zero():
            if x.is_zero():
                record(ctx, mod.DivisionUndefined)
                return mod.Decimal("NaN")
            if kind == "q":
                record(ctx, mod.DivisionByZero)
                return mod.Decimal("-Infinity") if sign else mod.Decimal("Infinity")
            record(ctx, mod.InvalidOperation)
            return mod.Decimal("NaN")

        if y.is_infinite():
            # 除数无穷：真商恰好是整数 0（不需要向负无穷再修正一格）；余数就是被除数本身。
            # 必须跟 x.is_infinite() 一样提前特判、不能落进下面"截断再修正"的通用算法——
            # 那套算法假设截断商之外还有个非零小数部分要向负无穷收，但除数无穷时截断商
            # 本身就已经是精确的 0，没有"再收一格"的余地（0 × Infinity 才是没定义的）
            q = mod.Decimal("-0") if sign else mod.Decimal("0")
            r = x
        else:
            # 必须比 prec 本身宽出一截，否则"超高精度"这个假设自己先垮了：prec 逼近/超过
            # HUGE_PREC 时，这个参考模型会在精度不够的地方自己触发 Inexact，把本该有解的
            # 组合错判成 DivisionImpossible——踩过这个坑（prec 3000 时），BigDec 那边其实算对了
            assert prec < HUGE_PREC, "floor_divmod_one 的参考精度必须比被测的 prec 更宽"
            big = new_ctx(HUGE_PREC, rounding, 999999999, -999999999, mod)
            qt, rt = big.divmod(x, y)  # 向零截断
            if qt.is_nan():
                # 超高精度下都装不下的商，目标精度当然更装不下
                record(ctx, mod.DivisionImpossible)
                return mod.Decimal("NaN")
            if big.flags[mod.Inexact]:
                exact[0] = False
                return None
            q, r = qt, rt
            if not rt.is_zero() and rt.is_signed() != y.is_signed():
                q = big.subtract(qt, mod.Decimal(1))
                r = big.add(rt, y)
                if big.flags[mod.Inexact]:
                    exact[0] = False
                    return None
        if q.is_finite() and len(q.copy_abs().as_tuple().digits) > prec:
            record(ctx, mod.DivisionImpossible)
            return mod.Decimal("NaN")
        # 商也要过 fix：位数够不代表指数域也够，调整后的指数超过 Emax 时得报 Overflow
        if kind == "q":
            return ctx.plus(q) if not q.is_zero() else q
        if r.is_zero():
            # fix 对零只做指数夹取
            etiny = emin - prec + 1
            e = r.as_tuple().exponent
            ne = min(max(e, etiny), emax)
            if ne != e:
                record(ctx, mod.Clamped)
                return mod.Decimal((1 if r.is_signed() else 0, (0,), ne))
            return r
        return ctx.plus(r)

    results = []
    for kind in ("q", "r"):
        ctx = new_ctx(prec, rounding, emax, emin, mod, traps_on)
        try:
            v = compute(kind, ctx)
            results.append((str(v), flags_str(ctx, mod)))
        except Exception as e:  # noqa: BLE001
            results.append(
                ("", "THROW:" + type(e).__name__ + ";" + flags_str(ctx, mod))
            )
    if not exact[0]:
        return None
    return results[0][0], results[0][1], results[1][0], results[1][1]


def floor_divmod(x_s, y_s, prec, rounding, emax=999999, emin=-999999, traps_on=None):
    """同上，但两套实现都推一遍，不一致就跳过。"""
    c = floor_divmod_one(x_s, y_s, prec, rounding, emax, emin, decimal, traps_on)
    p = floor_divmod_one(x_s, y_s, prec, rounding, emax, emin, _pydecimal, traps_on)
    if c is None or p is None:
        return None
    q = merge_outcome(c[1], p[1])
    r = merge_outcome(c[3], p[3])
    if c[0] != p[0] or q is None or r is None:
        SKIPPED["floordiv/mod"] = SKIPPED.get("floordiv/mod", 0) + 1
        return None
    return c[0], q, p[2], r


# --------------------------------------------------------------------------
SPECIALS = ["Infinity", "-Infinity", "NaN", "-NaN", "sNaN", "0", "-0", "1", "-1"]

FINITE_POOL = [
    "0",
    "-0",
    "0.00",
    "-0.00",
    "0.000",
    "0E+5",
    "-0E+5",
    "0E-5",
    "0E+999999",
    "0E-999999",
    "-0E+999999",
    "-0E-999999",
    "-0.0E+7",
    "1",
    "-1",
    "1.0",
    "1.50",
    "-1.50",
    "0.1",
    "0.2",
    "0.3",
    "2.5",
    "-2.5",
    "3.5",
    "0.5",
    "-0.5",
    "1E+10",
    "1E-10",
    "12345.6789",
    "-12345.6789",
    "1234567890123456789012345678901234",
    "9.999999999999999999999999999E+15",
    "1E+999999",
    "1E-999999",
    "-1E+999999",
    "100",
    "1.000",
    "0.000001",
    "7",
    "-7",
    "3",
    "-3",
    "6",
    "-6",
    "1E-28",
    "9999999999999999999999999999",
]


def rand_decimal(rng):
    """随机十进制串。指数分两档：多数落在 ±40 让两个操作数量级接近，少数直接铺到 ±999999，
    好把 align_for_add 的粘滞位、trunc_divmod 的量级短路这些只有极端指数才走到的路压出来。"""
    digits = rng.randint(1, 34)
    coeff = "".join(rng.choice("0123456789") for _ in range(digits))
    exp = rng.randint(-40, 40) if rng.random() < 0.75 else rng.randint(-999999, 999999)
    sign = "-" if rng.random() < 0.5 else ""
    return "%s%sE%+d" % (sign, coeff, exp)


def emit(name, lines):
    out = ["constexpr const char *const %s[]{" % name]
    for line in lines:
        out.append('    "%s",' % line)
    out.append("};")
    return "\n".join(out)


def main():
    sys.stdout.reconfigure(encoding="utf-8")  # 默认是本地编码，生成出来的注释会变乱码
    scale = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    rng = random.Random(20240819)
    out = [
        "// 本文件由 test/numeric/gen_big_dec_cases.py 生成，不要手改。",
        "// 期望值来自 CPython 自带的两套 decimal 实现，跟 BigDec 是各自独立的代码。",
        "// 每张表的行格式见 02_bigdec/python_cross_test.cpp 里跑这张表的那段。",
        "",
        "#pragma once",
        "",
    ]

    # ---- 四则运算：a|b|结果|flags ----------------------------------------
    special_pairs = [(a, b) for a in SPECIALS for b in SPECIALS]
    rand_pairs = [(rand_decimal(rng), rand_decimal(rng)) for _ in range(180 * scale)]
    for _ in range(80 * scale):  # 量级相近的组合，加减法最容易翻车的地方
        a = rand_decimal(rng)
        exp = int(a.split("E")[1])
        digits = "".join(rng.choice("0123456789") for _ in range(rng.randint(1, 34)))
        sign = "-" if rng.random() < 0.5 else ""
        rand_pairs.append((a, "%s%sE%+d" % (sign, digits, exp + rng.randint(-3, 3))))
    pool_pairs = [(a, b) for a in FINITE_POOL for b in FINITE_POOL]
    rng.shuffle(pool_pairs)
    pool_pairs = pool_pairs[: 300 * scale]
    all_pairs = special_pairs + rand_pairs + pool_pairs

    for op, name in (
        ("add", "kAddCases"),
        ("sub", "kSubCases"),
        ("mul", "kMulCases"),
        ("div", "kDivCases"),
    ):
        lines = []
        for a_s, b_s in all_pairs:
            got = agreed(
                op,
                28,
                HALF_EVEN,
                999999,
                -999999,
                lambda c, m, a=a_s, b=b_s, o=op: {
                    "add": c.add,
                    "sub": c.subtract,
                    "mul": c.multiply,
                    "div": c.divide,
                }[o](m.Decimal(a), m.Decimal(b)),
            )
            if got is None:
                continue
            lines.append("%s|%s|%s|%s" % (a_s, b_s, got[0], got[1]))
        out.append(emit(name, lines))
        out.append("")

    # ---- // 和 %：a|b|结果|flags ------------------------------------------
    fd_lines, mod_lines, divmod_lines = [], [], []
    for a_s, b_s in all_pairs:
        got = floor_divmod(a_s, b_s, 28, HALF_EVEN)
        if got is None:
            SKIPPED["floordiv/mod 不精确"] = SKIPPED.get("floordiv/mod 不精确", 0) + 1
            continue
        q, qf, r, rf = got
        fd_lines.append("%s|%s|%s|%s" % (a_s, b_s, q, qf))
        mod_lines.append("%s|%s|%s|%s" % (a_s, b_s, r, rf))
        # divmod 本身没有独立的期望值来源（IBM 规范没有这个复合操作），沿用同一组 q/r：divmod
        # 该等价于分别算 // 和 %、flags 取两边的并集，用这张表把这条等价关系铺在跟 // /% 一样
        # 大的值池上
        names = (set(qf.split(",")) | set(rf.split(","))) - {""}
        union = ",".join(n for n in SIGNAL_ATTRS if n in names)
        divmod_lines.append("%s|%s|%s|%s|%s" % (a_s, b_s, q, r, union))
    out.append(emit("kFloorDivCases", fd_lines))
    out.append("")
    out.append(emit("kModCases", mod_lines))
    out.append("")
    out.append(emit("kDivmodCases", divmod_lines))
    out.append("")

    # ---- 舍入：a|prec|rounding|结果|flags ---------------------------------
    round_lines = []
    round_pool = [
        "0",
        "-0",
        "1",
        "-1",
        "0.5",
        "-0.5",
        "1.5",
        "-1.5",
        "2.5",
        "-2.5",
        "0.05",
        "-0.05",
        "1.05",
        "1.15",
        "1.25",
        "1.35",
        "9.99",
        "-9.99",
        "99.5",
        "999.5",
        "0.999",
        "1.0001",
        "12345",
        "-12345",
        "1.000000000000000000000000000001",
        "999999999999999999999999999999",
        "0.000000000000000000000000000001",
        "5",
        "-5",
        "50",
        "55",
        "45",
        "-45",
        "104",
        "105",
        "106",
        "-105",
        "1000",
        "1005",
        "Infinity",
        "-Infinity",
        "NaN",
        "sNaN",
    ]
    for a_s in round_pool:
        for rname, rmode in ROUNDINGS:
            for prec in (1, 2, 3, 7):
                got = agreed(
                    "round",
                    prec,
                    rmode,
                    999999,
                    -999999,
                    lambda c, m, a=a_s: c.plus(m.Decimal(a)),
                )
                if got is None:
                    continue
                round_lines.append(
                    "%s|%d|%s|%s|%s" % (a_s, prec, rname, got[0], got[1])
                )
    out.append(emit("kRoundCases", round_lines))
    out.append("")

    # ---- 一元 - / abs：a|op|prec|rounding|结果|flags -----------------------
    # 上面那张 kRoundCases 走的是 plus，minus/abs 各有自己的零符号规则（ROUND_FLOOR 下 -0 才
    # 留得住负号），单独铺一张；值池在 round_pool 基础上补带标度的零和贴着 Emax/Emin 的值
    unary_lines = []
    unary_pool = round_pool + [
        "0.00",
        "-0.00",
        "0E+5",
        "-0E+5",
        "0E-5",
        "-0E-999999",
        "1E+999999",
        "-1E+999999",
        "1E-999999",
        "1.50",
        "-1.50",
    ]
    for a_s in unary_pool:
        for op in ("minus", "abs"):
            for rname, rmode in ROUNDINGS:
                for prec in (1, 2, 3, 7):
                    got = agreed(
                        "unary",
                        prec,
                        rmode,
                        999999,
                        -999999,
                        lambda c, m, a=a_s, o=op: {"minus": c.minus, "abs": c.abs}[o](
                            m.Decimal(a)
                        ),
                    )
                    if got is None:
                        continue
                    unary_lines.append(
                        "%s|%s|%d|%s|%s|%s" % (a_s, op, prec, rname, got[0], got[1])
                    )
    out.append(emit("kUnaryCases", unary_lines))
    out.append("")

    # ---- 指数边界：a|b|op|prec|rounding|emax|emin|结果|flags ---------------
    # 前半段是"指数域比精度窄"的各种组合（Emin 取到 0，那是次正规判定最容易出事的地方）；
    # 后半段专挑刚好越界的值 × 八种舍入，为的是把 raise_overflow 里"给 ±Infinity 还是给
    # prec 个 9"那张决策表整个走一遍
    edge_lines = []
    edge_values = [
        "1",
        "9",
        "9.99",
        "1E+2",
        "1E-2",
        "1E+5",
        "1E-5",
        "-1E+5",
        "-1E-5",
        "1.234E+3",
        "1.234E-3",
        "0",
        "-0",
        "5E-7",
        "9.999E+2",
    ]
    for a_s in edge_values:
        for b_s in edge_values:
            for prec, emax, emin in ((3, 4, -4), (2, 2, -2), (5, 9, -9), (3, 4, 0)):
                for op in ("mul", "div", "add", "sub"):
                    got = agreed(
                        "edge",
                        prec,
                        HALF_EVEN,
                        emax,
                        emin,
                        lambda c, m, a=a_s, b=b_s, o=op: {
                            "mul": c.multiply,
                            "div": c.divide,
                            "add": c.add,
                            "sub": c.subtract,
                        }[o](m.Decimal(a), m.Decimal(b)),
                    )
                    if got is None:
                        continue
                    edge_lines.append(
                        "%s|%s|%s|%d|%s|%d|%d|%s|%s"
                        % (a_s, b_s, op, prec, "HalfEven", emax, emin, got[0], got[1])
                    )
                for op in ("floordiv", "mod"):
                    got = floor_divmod(a_s, b_s, prec, HALF_EVEN, emax, emin)
                    if got is None:
                        continue
                    q, qf, r, rf = got
                    res, fl = (q, qf) if op == "floordiv" else (r, rf)
                    edge_lines.append(
                        "%s|%s|%s|%d|%s|%d|%d|%s|%s"
                        % (a_s, b_s, op, prec, "HalfEven", emax, emin, res, fl)
                    )
    for a_s in [
        "9.99E+3",
        "-9.99E+3",
        "1E+4",
        "-1E+4",
        "9E+3",
        "-9E+3",
        "1E-4",
        "-1E-4",
    ]:
        for b_s in ["1E+2", "10", "1E+5", "-1E+2", "1E-5"]:
            for prec, emax, emin in ((3, 4, -4), (5, 4, -4), (2, 2, -2)):
                for rname, rmode in ROUNDINGS:
                    for op in ("mul", "add"):
                        got = agreed(
                            "edge",
                            prec,
                            rmode,
                            emax,
                            emin,
                            lambda c, m, a=a_s, b=b_s, o=op: {
                                "mul": c.multiply,
                                "add": c.add,
                            }[o](m.Decimal(a), m.Decimal(b)),
                        )
                        if got is None:
                            continue
                        edge_lines.append(
                            "%s|%s|%s|%d|%s|%d|%d|%s|%s"
                            % (a_s, b_s, op, prec, rname, emax, emin, got[0], got[1])
                        )
    out.append(emit("kEdgeCases", edge_lines))
    out.append("")

    # ---- 混合：a|b|op|prec|rounding|结果|flags -----------------------------
    # 上面几张表大多固定在 prec 28 + HalfEven，这张专门覆盖"非默认精度下的每个运算"
    mixed_lines = []
    mixed_pool = FINITE_POOL + SPECIALS
    for _ in range(400 * scale):
        a_s = rand_decimal(rng) if rng.random() < 0.6 else rng.choice(mixed_pool)
        b_s = rand_decimal(rng) if rng.random() < 0.6 else rng.choice(mixed_pool)
        op = rng.choice(["add", "sub", "mul", "div", "floordiv", "mod"])
        prec = rng.randint(1, 30)
        rname, rmode = rng.choice(ROUNDINGS)
        if op in ("floordiv", "mod"):
            got = floor_divmod(a_s, b_s, prec, rmode)
            if got is None:
                continue
            q, qf, r, rf = got
            res, fl = (q, qf) if op == "floordiv" else (r, rf)
        else:
            pair = agreed(
                "mixed",
                prec,
                rmode,
                999999,
                -999999,
                lambda c, m, a=a_s, b=b_s, o=op: {
                    "add": c.add,
                    "sub": c.subtract,
                    "mul": c.multiply,
                    "div": c.divide,
                }[o](m.Decimal(a), m.Decimal(b)),
            )
            if pair is None:
                continue
            res, fl = pair
        mixed_lines.append(
            "%s|%s|%s|%d|%s|%s|%s" % (a_s, b_s, op, prec, rname, res, fl)
        )
    out.append(emit("kMixedCases", mixed_lines))
    out.append("")

    # ---- 陷阱：op|a|b|prec|rounding|emax|emin|trap|结果|flags -----------------
    # 陷阱开着时"抛不抛、抛哪个、抛之前 flags 走到哪一步"同样是规范的一部分，上面各表全是
    # 陷阱全关的路径。结果为空、flags 形如 "THROW:名字;抛出时的flags" 的行表示真的抛了，
    # 否则两个字段照常是"结果|flags"。值池刻意塞带非零指数的零（0E+999999 这类）和极端指数，
    # 专打 fix 的零夹取、add 的零操作数、trunc_divmod 的量级短路这些只有极端输入才走的路径
    trap_lines = []
    trap_pool = FINITE_POOL + SPECIALS
    for _ in range(500 * scale):
        a_s = rand_decimal(rng) if rng.random() < 0.7 else rng.choice(trap_pool)
        b_s = rand_decimal(rng) if rng.random() < 0.7 else rng.choice(trap_pool)
        op = rng.choice(["add", "sub", "mul", "div"])
        prec = rng.randint(1, 30)
        rname, rmode = rng.choice(ROUNDINGS)
        emax, emin = rng.choice(
            [
                (999999, -999999),
                (4, -4),
                (9, -9),
                (2, -2),
                (3, 0),
                (0, 0),
                (999999, 0),
                (3, -999999),
            ]
        )
        trap = rng.choice(SIGNAL_ATTRS)
        got = agreed_trap(
            "trap",
            prec,
            rmode,
            emax,
            emin,
            trap,
            trapped(
                lambda m: (
                    "DivisionUndefined"
                    if op == "div"
                    and m.Decimal(a_s).is_zero()
                    and m.Decimal(b_s).is_zero()
                    else None
                ),
                lambda c, m, x=a_s, y=b_s, o=op: {
                    "add": c.add,
                    "sub": c.subtract,
                    "mul": c.multiply,
                    "div": c.divide,
                }[o](m.Decimal(x), m.Decimal(y)),
            ),
        )
        if got is None:
            continue
        res, fl = got
        trap_lines.append(
            "%s|%s|%s|%d|%s|%d|%d|%s|%s|%s"
            % (a_s, b_s, op, prec, rname, emax, emin, trap, res, fl)
        )
    out.append(emit("kTrappedArith", trap_lines))
    out.append("")

    # ---- 陷阱 × // / %：a|b|op|prec|rounding|emax|emin|trap|结果|flags ---------
    # 期望值还是 floor_divmod 那条推导路径，只是带上陷阱；q、r 各自独立出题
    trap_fd_lines = []
    trap_fd_pairs = [(a, b) for a in trap_pool[:12] for b in trap_pool[:12]]
    rng.shuffle(trap_fd_pairs)
    for a_s, b_s in trap_fd_pairs[: 240 * scale]:
        prec = rng.randint(1, 30)
        rname, rmode = rng.choice(ROUNDINGS)
        emax, emin = rng.choice(
            [
                (999999, -999999),
                (4, -4),
                (9, -9),
                (2, -2),
                (3, 0),
                (0, 0),
                (999999, 0),
                (3, -999999),
            ]
        )
        trap = rng.choice(SIGNAL_ATTRS)
        got = floor_divmod(a_s, b_s, prec, rmode, emax, emin, trap)
        if got is None:
            continue
        q, qf, r, rf = got
        for op, res, fl in (("floordiv", q, qf), ("mod", r, rf)):
            trap_fd_lines.append(
                "%s|%s|%s|%d|%s|%d|%d|%s|%s|%s"
                % (a_s, b_s, op, prec, rname, emax, emin, trap, res, fl)
            )
    out.append(emit("kTrappedFloorDivMod", trap_fd_lines))
    out.append("")

    # ---- 陷阱 × 幂/超越函数：a|b|op|prec|rounding|emax|emin|trap|结果|flags ----
    trap_trans_lines = []
    trap_trans_pool = (
        FINITE_POOL[:22]
        + SPECIALS
        + [
            "1E+30",
            "1E-30",
            "2.718281828459045235360287471",
            "0.9999999999999999999999999999",
            "0.5",
            "1.5",
            "2",
            "4",
            "9",
            "1E+100",
            "1E-100",
            "1E+1999999998",
            "1E-1999999998",
        ]
    )
    for _ in range(160 * scale):
        op = rng.choice(["pow", "sqrt", "exp", "ln", "log10"])
        a_s = rng.choice(trap_trans_pool)
        b_s = ""
        if op == "pow":
            b_s = rng.choice(
                trap_trans_pool[:30]
                + ["0.3333333333333333", "28", "29", "1E+6", "-1E+6", "1E+3", "1E-3"]
            )
        prec = rng.choice([1, 2, 3, 5, 7, 16, 28, 50, 100])
        rname, rmode = rng.choice(ROUNDINGS)
        emax, emin = rng.choice([(999999, -999999), (4, -4), (9, -9), (3, 0), (0, 0)])
        trap = rng.choice(SIGNAL_ATTRS)
        got = agreed_trap(
            "trap-trans",
            prec,
            rmode,
            emax,
            emin,
            trap,
            trapped(
                None,
                lambda c, m, a=a_s, b=b_s, o=op: (
                    {
                        "pow": c.power,
                        "sqrt": c.sqrt,
                        "exp": c.exp,
                        "ln": c.ln,
                        "log10": c.log10,
                    }[o](m.Decimal(a), m.Decimal(b))
                    if b
                    else {
                        "pow": c.power,
                        "sqrt": c.sqrt,
                        "exp": c.exp,
                        "ln": c.ln,
                        "log10": c.log10,
                    }[o](m.Decimal(a))
                ),
            ),
        )
        if got is None:
            continue
        res, fl = got
        trap_trans_lines.append(
            "%s|%s|%s|%d|%s|%d|%d|%s|%s|%s"
            % (a_s, b_s, op, prec, rname, emax, emin, trap, res, fl)
        )
    out.append(emit("kTrappedTrans", trap_trans_lines))
    out.append("")

    # ---- 陷阱 × 比较：a|b|trap|eq;eqf|rel;orf --------------------------------
    # == 只有 sNaN 会抛；序比较任何 NaN 都会抛（SL 语义，Python 两边一致）
    trap_cmp_lines = []
    trap_cmp_pool = SPECIALS + [
        "0",
        "-0",
        "0E+5",
        "0E-999999",
        "1.5",
        "1.50",
        "1E+10",
        "-1E+10",
        "1E-10",
        "2",
        "-2",
        "1E+999999",
        "-1E+999999",
    ]
    for a_s in trap_cmp_pool:
        for b_s in trap_cmp_pool:
            trap = "InvalidOperation"
            results = {}
            for mod in (decimal, _pydecimal):
                a, b = mod.Decimal(a_s), mod.Decimal(b_s)
                with mod.localcontext(
                    new_ctx(28, HALF_EVEN, 999999, -999999, mod, trap)
                ) as c:
                    try:
                        eq = a == b
                        eqf = flags_str(c, mod)
                    except Exception as e:  # noqa: BLE001
                        eqf = "THROW:" + type(e).__name__ + ";" + flags_str(c, mod)
                with mod.localcontext(
                    new_ctx(28, HALF_EVEN, 999999, -999999, mod, trap)
                ) as c:
                    try:
                        lt = a < b
                        gt = a > b
                        orf = flags_str(c, mod)
                    except Exception as e:  # noqa: BLE001
                        orf = "THROW:" + type(e).__name__ + ";" + flags_str(c, mod)
                if a.is_nan() or b.is_nan():
                    rel = "un"
                elif lt:
                    rel = "lt"
                elif gt:
                    rel = "gt"
                else:
                    rel = "eq"
                results[mod] = (eq, eqf, rel, orf)
            c_r, p_r = results[decimal], results[_pydecimal]
            eqf = merge_outcome(c_r[1], p_r[1])
            orf = merge_outcome(c_r[3], p_r[3])
            if c_r[0] != p_r[0] or c_r[2] != p_r[2] or eqf is None or orf is None:
                SKIPPED["trap-cmp"] = SKIPPED.get("trap-cmp", 0) + 1
                continue
            eq, rel = c_r[0], p_r[2]
            o1 = eqf if eqf.startswith("THROW:") else ("1" if eq else "0") + ";" + eqf
            o2 = orf if orf.startswith("THROW:") else rel + ";" + orf
            trap_cmp_lines.append("%s|%s|%s|%s|%s" % (a_s, b_s, trap, o1, o2))
    out.append(emit("kTrappedCmp", trap_cmp_lines))
    out.append("")

    # ---- 幂运算：a|b|prec|rounding|emax|emin|结果|flags --------------------
    # 底数/指数的池子刻意覆盖 power_exact 的每条分支：10 的幂、2 的幂、5 的幂、开 n 次方、
    # 以及一堆只能走 exp(y*log(x)) 的
    pow_bases = [
        "0",
        "-0",
        "1",
        "-1",
        "1.000",
        "-1.000",
        "2",
        "-2",
        "10",
        "-10",
        "0.5",
        "-0.5",
        "4",
        "9",
        "16",
        "25",
        "100",
        "1024",
        "0.0625",
        "0.04",
        "2.25",
        "1E+10",
        "1E-10",
        "0.1",
        "1.5",
        "-1.5",
        "3",
        "5",
        "8",
        "2.5",
        "1.05",
        "0.2",
        "1.25",
        "1000000",
        "1E+999999",
        "1E-999999",
        "6.25E-2",
        "Infinity",
        "-Infinity",
        "NaN",
        "sNaN",
    ]
    pow_exps = [
        "0",
        "-0",
        "1",
        "-1",
        "2",
        "-2",
        "3",
        "-3",
        "0.5",
        "-0.5",
        "0.25",
        "-0.25",
        "1.5",
        "10",
        "-10",
        "100",
        "0.1",
        "-0.1",
        "1E+3",
        "1E-3",
        "2.5",
        "0.3333333333333333",
        "28",
        "29",
        "1E+6",
        "-1E+6",
        "0.2",
        "1.0",
        "Infinity",
        "-Infinity",
        "NaN",
        "sNaN",
    ]
    pow_lines = []

    def pow_case(a_s, b_s, prec, rname, rmode, emax, emin):
        got = agreed(
            "pow",
            prec,
            rmode,
            emax,
            emin,
            lambda c, m, a=a_s, b=b_s: c.power(m.Decimal(a), m.Decimal(b)),
        )
        if got is None:
            return
        pow_lines.append(
            "%s|%s|%d|%s|%d|%d|%s|%s"
            % (a_s, b_s, prec, rname, emax, emin, got[0], got[1])
        )

    for a_s in pow_bases:
        for b_s in pow_exps:
            pow_case(a_s, b_s, 28, "HalfEven", HALF_EVEN, 999999, -999999)
    small_bases = [
        "2",
        "-2",
        "0.5",
        "4",
        "9",
        "10",
        "0.1",
        "1E+10",
        "1",
        "0",
        "Infinity",
    ]
    small_exps = ["0", "1", "-1", "2", "0.5", "-0.5", "3", "10", "0.25", "1.5", "NaN"]
    for a_s in small_bases:
        for b_s in small_exps:
            for prec, emax, emin in (
                (1, 999999, -999999),
                (3, 999999, -999999),
                (16, 999999, -999999),
                (5, 9, -9),
                (100, 999999, -999999),  # prec > 50：默认规模生成器此前完全没铺到的量级
            ):
                for rname, rmode in (ROUNDINGS if prec == 3 else [ROUNDINGS[4]]):
                    pow_case(a_s, b_s, prec, rname, rmode, emax, emin)
    for _ in range(60 * scale):  # 随机底数/指数，冲着舍入边界去
        base_digits = "".join(
            rng.choice("0123456789") for _ in range(rng.randint(1, 12))
        )
        base = "%s%sE%+d" % (
            "-" if rng.random() < 0.25 else "",
            base_digits,
            rng.randint(-8, 8),
        )
        if rng.random() < 0.5:
            exponent = "%s%d" % ("-" if rng.random() < 0.5 else "", rng.randint(0, 40))
        else:
            exponent = "%s%d.%d" % (
                "-" if rng.random() < 0.5 else "",
                rng.randint(0, 6),
                rng.randint(0, 999),
            )
        rname, rmode = rng.choice(ROUNDINGS)
        pow_case(base, exponent, rng.randint(1, 30), rname, rmode, 999999, -999999)
    out.append(emit("kPowCases", pow_lines))
    out.append("")

    # ---- 超越函数：a|op|prec|rounding|emax|emin|结果|flags -----------------
    trans_lines = []
    trans_pool = [
        "0",
        "-0",
        "1",
        "-1",
        "1.000",
        "2",
        "-2",
        "0.5",
        "-0.5",
        "10",
        "-10",
        "100",
        "1000",
        "0.1",
        "0.01",
        "1E-7",
        "1E+7",
        "1E-30",
        "1E+30",
        "-1E+30",
        "-1E-30",
        "3",
        "7",
        "2.718281828459045235360287471",
        "0.6931471805599453094172321215",
        "1.0000000000000000000000000001",
        "0.9999999999999999999999999999",
        "1E+999999",
        "1E-999999",
        "-1E+999999",
        "123456789",
        "0.000123456789",
        "4",
        "9",
        "16",
        "25",
        "1.44",
        "2.25",
        "0.04",
        "1E+100",
        "1E-100",
        "6.25E-3",
        "1E+2",
        "1E+4",
        "1E+8",
        "12345.6789",
        "-12345.6789",
        "0.3",
        "1.5",
        "-1.5",
        "Infinity",
        "-Infinity",
        "NaN",
        "-NaN",
        "sNaN",
    ]
    trans_ctxs = [
        (28, 999999, -999999),
        (1, 999999, -999999),
        (3, 999999, -999999),
        (16, 999999, -999999),
        (50, 999999, -999999),
        (100, 999999, -999999),  # prec > 50：默认规模生成器此前完全没铺到的量级
        (5, 9, -9),
        (3, 999999, 0),
    ]

    def trans_case(a_s, op, prec, rname, rmode, emax, emin):
        got = agreed(
            "transcendental",
            prec,
            rmode,
            emax,
            emin,
            lambda c, m, a=a_s, o=op: {
                "sqrt": c.sqrt,
                "exp": c.exp,
                "ln": c.ln,
                "log10": c.log10,
            }[o](m.Decimal(a)),
        )
        if got is None:
            return
        trans_lines.append(
            "%s|%s|%d|%s|%d|%d|%s|%s"
            % (a_s, op, prec, rname, emax, emin, got[0], got[1])
        )

    for a_s in trans_pool:
        for op in ("sqrt", "exp", "ln", "log10"):
            for prec, emax, emin in trans_ctxs:
                for rname, rmode in (ROUNDINGS if prec == 3 else [ROUNDINGS[4]]):
                    trans_case(a_s, op, prec, rname, rmode, emax, emin)
    for _ in range(60 * scale):  # 随机参数，同样冲着舍入边界；正数才有 ln/log10/sqrt
        digits = "".join(rng.choice("0123456789") for _ in range(rng.randint(1, 30)))
        exp = rng.randint(-30, 30)
        positive = "%sE%+d" % (digits, exp)
        signed = ("-" if rng.random() < 0.3 else "") + positive
        prec = rng.randint(1, 40)
        rname, rmode = rng.choice(ROUNDINGS)
        for op, arg in (
            ("sqrt", signed),
            ("exp", signed),
            ("ln", positive),
            ("log10", positive),
        ):
            trans_case(arg, op, prec, rname, rmode, 999999, -999999)
    out.append(emit("kTranscendentalCases", trans_lines))
    out.append("")

    # ---- 比较：a|b|序关系(lt/eq/gt/un)|equals(0/1)|equals 的 flags|序比较的 flags ----
    cmp_lines = []
    cmp_pool = SPECIALS + [
        "1.5",
        "1.50",
        "1.500",
        "0.1",
        "1E+10",
        "-1E+10",
        "1E-10",
        "9999999999999999999999999999999",
        "2",
        "-2",
        "0.0",
        "1E+999999",
    ]
    for a_s in cmp_pool:
        for b_s in cmp_pool:
            a, b = decimal.Decimal(a_s), decimal.Decimal(b_s)
            eq_ctx = new_ctx()
            with decimal.localcontext(eq_ctx) as c:
                eq = a == b
                eq_flags = flags_str(c)
            with decimal.localcontext(new_ctx()) as c:
                lt = a < b
                ord_flags = flags_str(c)
            with decimal.localcontext(new_ctx()):
                gt = a > b
            if a.is_nan() or b.is_nan():
                rel = "un"
            elif lt:
                rel = "lt"
            elif gt:
                rel = "gt"
            else:
                rel = "eq"
            cmp_lines.append(
                "%s|%s|%s|%d|%s|%s"
                % (a_s, b_s, rel, 1 if eq else 0, eq_flags, ord_flags)
            )
    out.append(emit("kCompareCases", cmp_lines))

    print("\n".join(out))
    for table, count in sorted(SKIPPED.items()):
        print("// 跳过的组合（%s）：%d" % (table, count), file=sys.stderr)


if __name__ == "__main__":
    main()
