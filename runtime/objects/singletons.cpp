#include "singletons.h"

#include "../Runtime.h"

#include <utility>

Singleton::Singleton(Type *const type, std::string name) : Object{type}, name_{std::move(name)} {}

Bool::Bool(const bool value) : Object{Runtime::bool_type()}, value_{value} {}
