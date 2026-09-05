#pragma once

#include "Object.h"

#include <exception>
#include <utility>

/**
 * raise` 用的统一 C++ 包装，装一个 SL 异常对象。
 *
 * 由紧邻它的调用方（不跨 SL 帧）立刻 catch 住，生命周期严格不出那一段 C++ 调用。
 */
class RaisedException final : public std::exception {
    ObjectRef exception_;

  public:
    // 调用方保证 exception 非空且是 BaseException 的实例（难以 assert）
    explicit RaisedException(ObjectRef exception) : exception_{std::move(exception)} {}

    [[nodiscard]] Object *exception() const { return exception_.get(); }

    [[nodiscard]] const char *what() const noexcept override {
        return "RaisedException escaped its catch boundary";
    }
};
