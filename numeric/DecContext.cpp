#include "DecContext.h"

#include <cstdint>
#include <initializer_list>
#include <stdexcept>

DecCondition signal_of(const DecCondition condition) {
    switch (condition) {
    case DecCondition::ConversionSyntax:
    case DecCondition::DivisionImpossible:
    case DecCondition::DivisionUndefined:
    case DecCondition::InvalidContext:
        return DecCondition::InvalidOperation;
    case DecCondition::Clamped:
    case DecCondition::DivisionByZero:
    case DecCondition::Inexact:
    case DecCondition::InvalidOperation:
    case DecCondition::Overflow:
    case DecCondition::Rounded:
    case DecCondition::Subnormal:
    case DecCondition::Underflow:
        return condition;
    }
    throw std::logic_error{"DecCondition: invalid value"};
}

const char *dec_condition_name(const DecCondition condition) {
    // clang-format off
    switch (condition) {
    case DecCondition::Clamped:            return "Clamped";
    case DecCondition::DivisionByZero:     return "DivisionByZero";
    case DecCondition::Inexact:            return "Inexact";
    case DecCondition::InvalidOperation:   return "InvalidOperation";
    case DecCondition::Overflow:           return "Overflow";
    case DecCondition::Rounded:            return "Rounded";
    case DecCondition::Subnormal:          return "Subnormal";
    case DecCondition::Underflow:          return "Underflow";
    case DecCondition::ConversionSyntax:   return "ConversionSyntax";
    case DecCondition::DivisionImpossible: return "DivisionImpossible";
    case DecCondition::DivisionUndefined:  return "DivisionUndefined";
    case DecCondition::InvalidContext:     return "InvalidContext";
    }
    // clang-format on
    throw std::logic_error{"DecCondition: invalid value"};
}

DecSignalSet::DecSignalSet(const std::initializer_list<DecCondition> conditions) {
    for (const DecCondition condition : conditions) add(condition);
}

void DecSignalSet::add(const DecCondition condition) {
    bits_ |= std::uint32_t{1} << static_cast<unsigned>(signal_of(condition));
}

void DecSignalSet::remove(const DecCondition condition) {
    bits_ &= ~(std::uint32_t{1} << static_cast<unsigned>(signal_of(condition)));
}

bool DecSignalSet::has(const DecCondition condition) const {
    return (bits_ & (std::uint32_t{1} << static_cast<unsigned>(signal_of(condition)))) != 0;
}

DecTrapped::DecTrapped(const DecCondition condition)
    : std::runtime_error(dec_condition_name(condition)), condition_{condition} {}

void DecContext::set_prec(const std::int32_t prec) {
    if (prec < 1 || prec > kMaxPrec) throw std::invalid_argument("DecContext: prec out of range");
    prec_ = prec;
}

void DecContext::set_rounding(const DecRounding rounding) { rounding_ = rounding; }

void DecContext::set_emax(const std::int32_t emax) {
    if (emax < 0 || emax > kMaxExp) throw std::invalid_argument("DecContext: Emax out of range");
    emax_ = emax;
}

void DecContext::set_emin(const std::int32_t emin) {
    if (emin > 0 || emin < -kMaxExp) throw std::invalid_argument("DecContext: Emin out of range");
    emin_ = emin;
}

void DecContext::raise(const DecCondition condition) {
    const DecCondition signal{signal_of(condition)};
    // 先记 flags 再判陷阱：抛出去之后这次调用就结束了，但信号确实发生过
    flags_.add(signal);
    if (traps_.has(signal)) throw DecTrapped(condition);
}
