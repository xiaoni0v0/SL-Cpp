#pragma once

#include <cstdint>
#include <initializer_list>
#include <stdexcept>

// 十进制算术的环境：舍入方式、精度、指数范围、信号的陷阱/标志位。
// 对应 SL 的 decimal.Context；这一层不认识 SL 的异常类体系，陷阱触发时抛 DecTrapped，
// 由调用方映射成 decimal.XXX

// 舍入方式，对应 SL 里 Context 上的 ROUND_* 类属性
enum class DecRounding : uint8_t {
    Down,      // 一律靠近零（截断）
    Up,        // 一律远离零
    HalfUp,    // 四舍五入，恰好一半时远离零
    HalfDown,  // 四舍五入，恰好一半时靠近零
    HalfEven,  // 四舍六入五成双
    Ceiling,   // 一律向 +Infinity
    Floor,     // 一律向 -Infinity
    ZeroFiveUp // 向零截断；但截断后末位是 0 或 5 时改为远离零
};

// 算术过程中出现的"条件"。前 kDecSignalCount 个是信号本身（traps/flags 只认它们）；
// 后面的都是 InvalidOperation 的细分，查 traps/记 flags 时经 signal_of() 折算，
// 只有抛出来的 DecTrapped 带细分条件
enum class DecCondition : uint8_t {
    Clamped,
    DivisionByZero,
    Inexact,
    InvalidOperation,
    Overflow,
    Rounded,
    Subnormal,
    Underflow,
    // ↓ 以下都折算成 InvalidOperation
    ConversionSyntax,   // 字符串不是合法的 decimal
    DivisionImpossible, // 整除的结果位数超过 prec
    DivisionUndefined,  // 0 / 0
    InvalidContext,     // 上下文字段本身不合法；BigDec 不产生它，留给调用方校验用户字段用
};

// 前 kDecSignalCount 项是信号本身，后面的都折算到 InvalidOperation
inline constexpr size_t kDecSignalCount{8};
static_assert(static_cast<size_t>(DecCondition::ConversionSyntax) == kDecSignalCount);

// 把条件折算成 traps/flags 认识的信号
[[nodiscard]] DecCondition signal_of(DecCondition condition);
// 条件的名字，跟 SL 里对应的异常类名一致（如 "DivisionByZero"）
[[nodiscard]] const char *dec_condition_name(DecCondition condition);

// traps / flags 用的信号集合。add/remove/has 收条件，内部先过 signal_of()
class DecSignalSet {
    uint32_t bits_{0};

  public:
    DecSignalSet() = default;
    // 不加 explicit：`{DecCondition::Overflow, ...}` 这种写法本身已经足够明确
    DecSignalSet(std::initializer_list<DecCondition> conditions);

    void add(DecCondition condition);
    void remove(DecCondition condition);
    [[nodiscard]] bool has(DecCondition condition) const;
    [[nodiscard]] bool empty() const { return bits_ == 0; }
    void clear() { bits_ = 0; }

    [[nodiscard]] bool operator==(const DecSignalSet &rhs) const = default;
};

// 某个信号在 traps 里、于是被抛出来。what() 是条件名，具体条件从 condition() 取
class DecTrapped final : public std::runtime_error {
    DecCondition condition_;

  public:
    explicit DecTrapped(DecCondition condition);

    [[nodiscard]] DecCondition condition() const { return condition_; }
};

class DecContext {
    int32_t prec_{28};
    DecRounding rounding_{DecRounding::HalfEven};
    DecSignalSet traps_{
        DecCondition::DivisionByZero, DecCondition::Overflow, DecCondition::InvalidOperation
    };
    DecSignalSet flags_;
    int32_t emax_{999999};
    int32_t emin_{-999999};

  public:
    // 各字段取值上限。取这个量级让 etiny()/etop()、exp ± exp、exp ± prec 在 int64_t 里溢不出来
    static constexpr int32_t kMaxPrec{999999999};
    static constexpr int32_t kMaxExp{999999999};

    // 默认上下文：prec 28、ROUND_HALF_EVEN、陷阱 {DivisionByZero, Overflow, InvalidOperation}、
    // flags 为空、Emax 999999、Emin -999999
    DecContext() = default;

    [[nodiscard]] int32_t prec() const { return prec_; }
    [[nodiscard]] DecRounding rounding() const { return rounding_; }
    [[nodiscard]] int32_t emax() const { return emax_; }
    [[nodiscard]] int32_t emin() const { return emin_; }

    // 越界抛 std::invalid_argument——字段不合法是调用方的编程错误，不是算术信号
    void set_prec(int32_t prec); // 必须在 [1, kMaxPrec]
    void set_rounding(DecRounding rounding);
    void set_emax(int32_t emax); // 必须在 [0, kMaxExp]
    void set_emin(int32_t emin); // 必须在 [-kMaxExp, 0]

    [[nodiscard]] const DecSignalSet &traps() const { return traps_; }
    [[nodiscard]] DecSignalSet &traps() { return traps_; }
    [[nodiscard]] const DecSignalSet &flags() const { return flags_; }
    [[nodiscard]] DecSignalSet &flags() { return flags_; }

    // 非零结果允许的最小指数：Emin - prec + 1
    [[nodiscard]] int64_t etiny() const { return static_cast<int64_t>(emin_) - prec_ + 1; }
    // 结果指数的上限：Emax - prec + 1（超过它就意味着 adjusted() 超过了 Emax）
    [[nodiscard]] int64_t etop() const { return static_cast<int64_t>(emax_) - prec_ + 1; }

    // 记一个信号：无论如何都先把对应的 flags 位置上（粘滞，只能手动 clear），
    // 该位同时在 traps 里则抛 DecTrapped。调用方保证只在条件真的发生时调用
    void raise(DecCondition condition);
};
