#pragma once

#include "../../builtins/object/SlObject.h"

#include <string>
#include <unordered_map>


class Frame {
    const bool is_global_;
    const Frame *parent_;
    std::unordered_map<std::u32string, SlObject *> variables_;
    std::unordered_map<std::u32string, bool> global_decls;

public:
    explicit Frame(const bool is_global, const Frame *parent)
        : is_global_{is_global}, parent_{parent} {
    }

    void set_variable(const std::u32string &name, SlObject *value) {
        variables_[name] = value;
    }
};
