#include "NamedSingleton.h"

#include "../Object.h"

#include <string>
#include <utility>

NamedSingleton::NamedSingleton(Type *type, std::string name)
    : Object{type}, name_{std::move(name)} {}
