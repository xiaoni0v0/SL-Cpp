#pragma once

#include "Object.h"

#include <exception>
#include <utility>

// `raise` 用的统一 C++ 信封：装一个 SL 异常对象，让"决定要抛出这个 SL 异常"这件事能借道
// C++ 的 throw/catch 表达。
//
// **不是 SL 异常跨帧传播的机制**——bytecode.md 明确规定那必须是主循环手写驱动的显式算法，
// 不能靠 C++ 原生异常展开（SL 帧是堆对象，C++ 的自动栈展开够不到别的 SL 帧）。这个信封只用在
// 一段有界的、不回调 SL 的纯 C++ 代码里：比如某个内置函数的 C++ 实现决定"这一步该产生的结果是
// 一个异常"，throw 这个信封，由紧邻它的调用方（不跨 SL 帧）立刻 catch 住，翻译成 Frame 接口
// 那句"接收沿栈传播的异常"，之后就走显式传播了。
//
// 换句话说：它是"内置操作的 C++ 实现"和"主循环的显式异常协议"之间的一次性适配器，生命周期
// 严格不出那一段 C++ 调用。**没接住就说明适配的位置选错了**，属于实现 bug，所以继承
// std::exception 而不是自成一派——万一真的漏接，还能在这层兜底打印 what()
class RaisedException final : public std::exception {
    ObjectRef exception_;

  public:
    // 调用方保证 exception 非空且是 BaseException 的实例；这两条不在这里查
    // （查了也要么依赖 Runtime 要么依赖 isinstance，这两样都不是这个轻量信封该管的）
    explicit RaisedException(ObjectRef exception) : exception_{std::move(exception)} {}

    [[nodiscard]] Object *exception() const { return exception_.get(); }

    [[nodiscard]] const char *what() const noexcept override {
        return "一个 RaisedException 逃出了它该被捕获的边界（说明适配的位置选错了，是实现自己的 "
               "bug）";
    }
};
