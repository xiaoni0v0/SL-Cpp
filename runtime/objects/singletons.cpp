#include "singletons.h"

#include <utility>

NamedSingleton::NamedSingleton(Type *const type, std::string name)
    : Object{type}, name_{std::move(name)} {}

Bool::Bool(Type *const type, const bool value) : Object{type}, value_{value} {}
