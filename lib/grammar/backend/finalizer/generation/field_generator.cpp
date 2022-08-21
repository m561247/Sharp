//
// Created by bknun on 8/8/2022.
//

#include "field_generator.h"
#include "../../types/types.h"
#include "code/code_info.h"
#include "code/scheme/scheme_processor.h"
#include "generator.h"

uInt threadLocalCount = 0;
void generate_address(sharp_field *field) {
    if (field->ci == NULL) {
        bool isStatic = check_flag(field->flags, flag_static);
        bool threadLocal = field->fieldType == tls_field;

        field->ci = new code_info();
        field->ci->uuid = UUIDGenerator++;

        if(threadLocal) {
            field->ci->address = threadLocalCount++;
        } else {
            Int address = 0;

            for (Int i = 0; i < field->owner->fields.size(); i++) {
                sharp_field *sf = field->owner->fields.get(i);

                if (sf == field) {
                    field->ci->address = address;
                    break;
                } else if (isStatic == check_flag(sf->flags, flag_static) && sf->used) {
                    address++;
                }
            }
        }
    }
}

void generate_address(sharp_field *field, Int localFieldIndex) {
    if (field->ci == NULL && field->used) {
        bool isStatic = check_flag(field->flags, flag_static);
        bool threadLocal = field->fieldType == tls_field;

        field->ci = new code_info();
        field->ci->uuid = UUIDGenerator++;

        field->ci->address = localFieldIndex;
    }
}

code_info* get_or_initialize_code(sharp_field *field) {
    generate_address(field);
    return field->ci;
}