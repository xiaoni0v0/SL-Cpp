#include "NamedSingleton.h"

#include <utility>

NamedSingleton::NamedSingleton(Type *const type, std::string name)
    : Object{type}, name_{std::move(name)} {}
