# 生成 big_int_cases.inc：BigInt 的交叉验证用例表，期望值来自 Python 内置的 int（也是任意精度、
# // 和 % 向负无穷取整、位运算按无穷位补码语义——语义定义上跟 BigInt 天然一致，不需要像
# gen_big_dec_cases.py 里那样为语义差异单独推导）。用法：
#
#     python test/numeric/gen_big_int_cases.py > test/numeric/big_int_cases.inc
#
# 可以带一个倍数参数（默认 1）把随机用例翻倍：
#
#     python test/numeric/gen_big_int_cases.py 20 > /tmp/big_int_cases.inc
import random
import sys

# 跟 big_int_test.cpp 里 interesting_values() 手写的边界值对齐，量级覆盖 32/64 位 limb 边界
BOUNDARY_POOL = [
    0,
    1,
    -1,
    2,
    -2,
    100,
    -100,
    2**31 - 1,
    2**31,
    2**31 + 1,
    -(2**31),
    2**32 - 1,
    2**32,
    -(2**32),
    2**63 - 1,
    2**63,
    2**63 + 1,
    -(2**63),
    -(2**63) - 1,
    -(2**63) + 1,
    2**64 - 1,
    2**64,
    2**64 + 1,
    -(2**64),
    2**128 - 1,
    -(2**128 - 1),
    123456789012345678901234567890,
    -123456789012345678901234567890,
    10**400,
    -(10**400),  # 远超 double 表示范围，逼 to_double 走溢出成 ±infinity 那条路
]


def rand_int(rng):
    """随机大整数：位数从 1 到 300 位不等（多数集中在小位数，少数铺到很大），正负各半。"""
    digits = rng.choice(
        [rng.randint(1, 6)] * 4 + [rng.randint(7, 30)] * 3 + [rng.randint(31, 300)]
    )
    n = rng.randint(0, 10**digits - 1)
    return -n if rng.random() < 0.5 else n


def emit(name, lines):
    out = ["constexpr const char *const %s[]{" % name]
    for line in lines:
        out.append('    "%s",' % line)
    out.append("};")
    return "\n".join(out)


def to_double_hex(n):
    """float(n) 的精确十六进制表示（C99 strtod 认得），溢出时手动给 ±inf——Python 的 int->float
    在这种情况下会抛 OverflowError，但 BigInt::to_double() 按 IEEE 溢出语义返回 ±infinity，
    不抛异常，这里按 BigInt 的约定生成期望值。"""
    try:
        f = float(n)
    except OverflowError:
        f = float("inf") if n > 0 else float("-inf")
    return f.hex()


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    scale = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    rng = random.Random(20260820)
    pool = list(BOUNDARY_POOL) + [rand_int(rng) for _ in range(150 * scale)]

    out = [
        "// 本文件由 test/numeric/gen_big_int_cases.py 生成，不要手改。",
        "// 期望值来自 Python 内置的 int，跟 BigInt 是各自独立的代码。",
        "// 每张表的行格式见 big_int_test.cpp 里跑这张表的那段。",
        "",
        "#pragma once",
        "",
    ]

    # ---- 四则运算 + floor_div/mod：a|b|结果 --------------------------------
    pairs = [(a, rng.choice(pool)) for a in pool for _ in range(3)]
    rng.shuffle(pairs)

    for op, name, skip_zero_b in (
        ("add", "kAddCases", False),
        ("sub", "kSubCases", False),
        ("mul", "kMulCases", False),
        ("floordiv", "kFloorDivCases", True),
        ("mod", "kModCases", True),
    ):
        lines = []
        for a, b in pairs:
            if skip_zero_b and b == 0:
                continue
            r = {
                "add": a + b,
                "sub": a - b,
                "mul": a * b,
                "floordiv": a // b if skip_zero_b else None,
                "mod": a % b if skip_zero_b else None,
            }[op]
            lines.append("%d|%d|%d" % (a, b, r))
        out.append(emit(name, lines))
        out.append("")

    # ---- 位运算：a|b|结果 ---------------------------------------------------
    for op, name in (("and", "kAndCases"), ("or", "kOrCases"), ("xor", "kXorCases")):
        lines = []
        for a, b in pairs:
            r = {"and": a & b, "or": a | b, "xor": a ^ b}[op]
            lines.append("%d|%d|%d" % (a, b, r))
        out.append(emit(name, lines))
        out.append("")

    # ---- 移位：a|k|结果（k 保持适中，避免生成的期望值本身占满几百 MB）--------
    shift_lines_l, shift_lines_r = [], []
    for a in pool:
        for k in (0, 1, 5, 31, 32, 63, 64, 65, 100, 200):
            shift_lines_l.append("%d|%d|%d" % (a, k, a << k))
            shift_lines_r.append("%d|%d|%d" % (a, k, a >> k))
    out.append(emit("kShiftLeftCases", shift_lines_l))
    out.append("")
    out.append(emit("kShiftRightCases", shift_lines_r))
    out.append("")

    # ---- 幂：a|b|结果（b >= 0，控制结果位数，不然表会爆炸）--------------------
    pow_lines = []
    pow_bases = [
        0,
        1,
        -1,
        2,
        -2,
        3,
        -3,
        5,
        7,
        10,
        -10,
        100,
        123456789012345678901234567890,
    ]
    pow_exps = [0, 1, 2, 3, 5, 8, 10, 16, 20, 30, 40, 100]
    for a in pow_bases:
        for b in pow_exps:
            r = a**b
            if len(str(abs(r))) > 400:  # 结果太大就跳过，避免用例表失控
                continue
            pow_lines.append("%d|%d|%d" % (a, b, r))
    for _ in range(40 * scale):  # 随机底数（位数较小）配小指数，补充覆盖面
        base_digits = rng.randint(1, 8)
        a = rng.randint(-(10**base_digits), 10**base_digits)
        b = rng.randint(0, 12)
        pow_lines.append("%d|%d|%d" % (a, b, a**b))
    out.append(emit("kPowCases", pow_lines))
    out.append("")

    # ---- 比较：a|b|cmp（-1/0/1）---------------------------------------------
    cmp_lines = []
    for a, b in pairs:
        cmp_lines.append("%d|%d|%d" % (a, b, (a > b) - (a < b)))
    out.append(emit("kCompareCases", cmp_lines))
    out.append("")

    # ---- bit_length：a|位数 --------------------------------------------------
    bl_lines = ["%d|%d" % (a, a.bit_length()) for a in pool]
    out.append(emit("kBitLengthCases", bl_lines))
    out.append("")

    # ---- to_double：a|float(a) 的精确十六进制表示 -----------------------------
    dbl_lines = ["%d|%s" % (a, to_double_hex(a)) for a in pool]
    out.append(emit("kToDoubleCases", dbl_lines))

    print("\n".join(out))


if __name__ == "__main__":
    main()
