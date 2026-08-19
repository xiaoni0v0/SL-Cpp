# 生成 big_dec_cases.inc：BigDec 的交叉验证用例表，期望值全部来自 CPython 自带的 decimal
# （C 实现 libmpdec，跟 BigDec 是两套独立代码）。用法：
#
#     python test/numeric/gen_big_dec_cases.py > test/numeric/big_dec_cases.inc
#
# 可以带一个倍数参数（默认 1）把随机用例翻倍，用来临时做更大规模的差分测试；
# 提交进仓库的那份用默认倍数生成，不然表会大到没法看：
#
#     python test/numeric/gen_big_dec_cases.py 20 > /tmp/big_dec_cases.inc
#
# SL 的 // 和 % 向负无穷取整，跟 IBM 规范（也就是 Python 的 Decimal）向零截断不同，所以这两个
# 表的期望值是"先在超高精度下取精确的截断商/余数，再整体修正"推出来的——推导路径跟 C++ 那边
# 不一样，两边只在数学定义上一致。凡是超高精度下仍不精确的组合直接跳过，不出题。
import sys
import decimal
from decimal import Decimal, Context, localcontext
import random

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

# 顺序必须跟 C++ 里 DecCondition 前八项一致
SIGNAL_NAMES = [
    (decimal.Clamped, "Clamped"),
    (decimal.DivisionByZero, "DivisionByZero"),
    (decimal.Inexact, "Inexact"),
    (decimal.InvalidOperation, "InvalidOperation"),
    (decimal.Overflow, "Overflow"),
    (decimal.Rounded, "Rounded"),
    (decimal.Subnormal, "Subnormal"),
    (decimal.Underflow, "Underflow"),
]

HUGE_PREC = 2000


def new_ctx(prec=28, rounding=decimal.ROUND_HALF_EVEN, emax=999999, emin=-999999):
    c = Context(prec=prec, rounding=rounding, Emax=emax, Emin=emin)
    c.traps = {k: 0 for k in c.traps}  # 陷阱全关，只收 flags
    c.clear_flags()
    return c


def flags_str(c):
    return ",".join(name for sig, name in SIGNAL_NAMES if c.flags[sig])


# --------------------------------------------------------------------------
# // 和 % 的参考值：向负无穷取整（SL 语义，跟 IBM 规范的向零截断不同）。
# 推导路径跟 C++ 那边不一样——先在超高精度下拿到精确的截断商/余数，再整体修正——
# 两边只在数学定义上一致，不共用代码路径。凡是超高精度下仍不精确的组合直接跳过不出题
# --------------------------------------------------------------------------
def floor_divmod(x, y, prec, rounding, emax=999999, emin=-999999):
    """返回 (商, 商的flags, 余数, 余数的flags) 或 None（表示这组不适合当用例）。"""
    exact = [True]

    def nan_of(ctx):
        if x.is_snan() or y.is_snan():
            which = x if x.is_snan() else y
            ctx.flags[decimal.InvalidOperation] = 1
            return Decimal("-NaN") if which.is_signed() else Decimal("NaN")
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
                    ctx.flags[decimal.InvalidOperation] = 1
                    return Decimal("NaN")
                return Decimal("-Infinity") if sign else Decimal("Infinity")
            ctx.flags[decimal.InvalidOperation] = 1
            return Decimal("NaN")
        if y.is_zero():
            if x.is_zero():
                ctx.flags[decimal.InvalidOperation] = 1  # DivisionUndefined
                return Decimal("NaN")
            if kind == "q":
                ctx.flags[decimal.DivisionByZero] = 1
                return Decimal("-Infinity") if sign else Decimal("Infinity")
            ctx.flags[decimal.InvalidOperation] = 1  # x % 0
            return Decimal("NaN")

        big = new_ctx(HUGE_PREC, rounding, 999999999, -999999999)
        qt, rt = big.divmod(x, y)  # 向零截断
        if qt.is_nan():
            # 超高精度下都装不下的商，目标精度当然更装不下
            ctx.flags[decimal.InvalidOperation] = 1  # DivisionImpossible
            return Decimal("NaN")
        if big.flags[decimal.Inexact]:
            exact[0] = False
            return None
        q, r = qt, rt
        if not rt.is_zero() and rt.is_signed() != y.is_signed():
            q = big.subtract(qt, Decimal(1))
            r = big.add(rt, y)
            if big.flags[decimal.Inexact]:
                exact[0] = False
                return None
        if q.is_finite() and len(q.copy_abs().as_tuple().digits) > prec:
            ctx.flags[decimal.InvalidOperation] = 1  # DivisionImpossible
            return Decimal("NaN")
        if kind == "q":
            return q
        if r.is_zero():
            # fix 对零只做指数夹取
            etiny = emin - prec + 1
            e = r.as_tuple().exponent
            ne = min(max(e, etiny), emax)
            if ne != e:
                ctx.flags[decimal.Clamped] = 1
                return Decimal((1 if r.is_signed() else 0, (0,), ne))
            return r
        return ctx.plus(r)

    q_ctx = new_ctx(prec, rounding, emax, emin)
    q = compute("q", q_ctx)
    r_ctx = new_ctx(prec, rounding, emax, emin)
    r = compute("r", r_ctx)
    if not exact[0]:
        return None
    return str(q), flags_str(q_ctx), str(r), flags_str(r_ctx)


# --------------------------------------------------------------------------
SPECIALS = ["Infinity", "-Infinity", "NaN", "-NaN", "sNaN", "0", "-0", "1", "-1"]

FINITE_POOL = [
    "0",
    "-0",
    "0.00",
    "-0.00",
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
    digits = rng.randint(1, 34)
    coeff = "".join(rng.choice("0123456789") for _ in range(digits))
    exp = rng.randint(-40, 40)
    sign = "-" if rng.random() < 0.5 else ""
    return "%s%sE%+d" % (sign, coeff, exp)


def emit(name, lines):
    out = ["const char *const %s[]{" % name]
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
        "// 期望值来自 CPython 自带的 decimal（C 实现 libmpdec），跟 BigDec 是两套独立代码。",
        "// 每张表的行格式见 big_dec_test.cpp 里跑这张表的那段。",
        "",
    ]

    special_pairs = [(a, b) for a in SPECIALS for b in SPECIALS]
    rand_pairs = []
    for _ in range(180 * scale):
        rand_pairs.append((rand_decimal(rng), rand_decimal(rng)))
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
            c = new_ctx()
            a, b = Decimal(a_s), Decimal(b_s)
            res = {"add": c.add, "sub": c.subtract, "mul": c.multiply, "div": c.divide}[
                op
            ](a, b)
            lines.append("%s|%s|%s|%s" % (a_s, b_s, res, flags_str(c)))
        out.append(emit(name, lines))
        out.append("")

    fd_lines, mod_lines, skipped = [], [], 0
    for a_s, b_s in all_pairs:
        got = floor_divmod(Decimal(a_s), Decimal(b_s), 28, decimal.ROUND_HALF_EVEN)
        if got is None:
            skipped += 1
            continue
        q, qf, r, rf = got
        fd_lines.append("%s|%s|%s|%s" % (a_s, b_s, q, qf))
        mod_lines.append("%s|%s|%s|%s" % (a_s, b_s, r, rf))
    out.append(emit("kFloorDivCases", fd_lines))
    out.append("")
    out.append(emit("kModCases", mod_lines))
    out.append("")

    # 舍入表：a|prec|rounding|结果|flags
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
                c = new_ctx(prec, rmode)
                res = c.plus(Decimal(a_s))
                round_lines.append(
                    "%s|%d|%s|%s|%s" % (a_s, prec, rname, res, flags_str(c))
                )
    out.append(emit("kRoundCases", round_lines))
    out.append("")

    # 指数边界：a|b|op|prec|emax|emin|结果|flags
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
            for prec, emax, emin in ((3, 4, -4), (2, 2, -2), (5, 9, -9)):
                for op in ("mul", "div", "add"):
                    c = new_ctx(prec, decimal.ROUND_HALF_EVEN, emax, emin)
                    a, b = Decimal(a_s), Decimal(b_s)
                    res = {"mul": c.multiply, "div": c.divide, "add": c.add}[op](a, b)
                    edge_lines.append(
                        "%s|%s|%s|%d|%d|%d|%s|%s"
                        % (a_s, b_s, op, prec, emax, emin, res, flags_str(c))
                    )
    out.append(emit("kEdgeCases", edge_lines))
    out.append("")

    # 混合表：随机操作数 × 随机 op/prec/rounding。上面几张表都固定在 prec 28 + HalfEven，
    # 这张专门覆盖"非默认精度下的每个运算"。格式 a|b|op|prec|rounding|结果|flags
    mixed_lines = []
    mixed_pool = FINITE_POOL + SPECIALS
    for _ in range(400 * scale):
        a_s = rand_decimal(rng) if rng.random() < 0.6 else rng.choice(mixed_pool)
        b_s = rand_decimal(rng) if rng.random() < 0.6 else rng.choice(mixed_pool)
        op = rng.choice(["add", "sub", "mul", "div", "floordiv", "mod"])
        prec = rng.randint(1, 30)
        rname, rmode = rng.choice(ROUNDINGS)
        if op in ("floordiv", "mod"):
            got = floor_divmod(Decimal(a_s), Decimal(b_s), prec, rmode)
            if got is None:
                continue
            q, qf, r, rf = got
            res, fl = (q, qf) if op == "floordiv" else (r, rf)
        else:
            c = new_ctx(prec, rmode)
            a, b = Decimal(a_s), Decimal(b_s)
            res = {"add": c.add, "sub": c.subtract, "mul": c.multiply, "div": c.divide}[
                op
            ](a, b)
            fl = flags_str(c)
        mixed_lines.append(
            "%s|%s|%s|%d|%s|%s|%s" % (a_s, b_s, op, prec, rname, res, fl)
        )
    out.append(emit("kMixedCases", mixed_lines))
    out.append("")

    # 比较：a|b|序关系(lt/eq/gt/un)|equals(0/1)|equals 的 flags|序比较的 flags
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
            a, b = Decimal(a_s), Decimal(b_s)
            with localcontext(new_ctx()) as c:
                eq = a == b
                eq_flags = flags_str(c)
            with localcontext(new_ctx()) as c:
                lt = a < b
                ord_flags = flags_str(c)
            with localcontext(new_ctx()):
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
    print(
        "// 跳过（超高精度下仍不精确、不适合当参考）的除法组合：%d" % skipped,
        file=sys.stderr,
    )


main()
