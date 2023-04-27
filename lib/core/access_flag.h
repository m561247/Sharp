//
// Created by BNunnally on 8/31/2021.
//

#ifndef SHARP_ACCESS_FLAG_H
#define SHARP_ACCESS_FLAG_H

#include "../../stdimports.h"

enum access_flag {
    flag_none = 0x000,

    flag_public = 0x001,
    flag_private = (flag_public << 1),
    flag_protected = (flag_public << 2),

    flag_override = (flag_public << 3),

    flag_thread_safe = (flag_public << 4),

    // todo: add errors for non-nullable fields not being assigned
    flag_excuse = (flag_public << 4), // this flag excuses a non-nullable fields not being initialized

    flag_local = (flag_public << 6),

    flag_const = (flag_public << 7),
    flag_static = (flag_public << 8),

    flag_stable = (flag_public << 9),
    flag_unstable = (flag_public << 10),

    flag_extension = (flag_public << 11),

    flag_native = (flag_public << 12),

    flag_global = (flag_public << 13)
};

void set_flag(uInt &flags, access_flag flag, bool enable);
bool check_flag(uInt flags, access_flag flag);

#endif //SHARP_ACCESS_FLAG_H