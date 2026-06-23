#include "SlObject.h"

SlObject *SlObject::new_object() {
    return new SlObject{};
}

void SlObject::delete_object(const SlObject *obj) {
    delete obj;
}
