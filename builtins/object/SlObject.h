#pragma once


class SlObject {
public:
    static SlObject *new_object();
    static void delete_object(const SlObject *obj);
};
