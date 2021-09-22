//
// Created by BNunnally on 9/17/2021.
//

#ifndef SHARP_EXPRESSION_COMPILER_H
#define SHARP_EXPRESSION_COMPILER_H

#include "../types/sharp_type.h"
#include "../operation/operation.h"

struct expression {
    expression()
    :
        type(),
        scheme()
    {}

    expression(const expression &e)
            :
            type(e.type),
            scheme(e.scheme)
    {}

    expression(sharp_type t)
            :
            type(t),
            scheme()
    {}

    ~expression() {
        scheme.free();
    }


    sharp_type type;
    operation_scheme scheme;
};

expression compile_expression(Ast*);
sharp_type compile_expression_for_type(Ast*);


#endif //SHARP_EXPRESSION_COMPILER_H