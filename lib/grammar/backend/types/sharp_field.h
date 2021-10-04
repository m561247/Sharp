//
// Created by BNunnally on 9/1/2021.
//

#ifndef SHARP_SHARP_FIELD_H
#define SHARP_SHARP_FIELD_H

#include "../../../../stdimports.h"
#include "../meta_data.h"
#include "../access_flag.h"
#include "sharp_type.h"
#include "../dependency/dependancy.h"
#include "../dependency/injection_request.h"

struct sharp_class;

enum field_type {
    normal_field,
    tls_field
};

struct sharp_field {
    sharp_field()
    :
        name(""),
        fullName(""),
        owner(NULL),
        implLocation(),
        dependencies(),
        flags(flag_none),
        fieldType(normal_field),
        closure(NULL),
        type(),
        ast(NULL),
        getter(NULL),
        setter(NULL),
        scheme(NULL),
        request(NULL)
    {}

    sharp_field(const sharp_field &sf)
    :
        name(sf.name),
        fullName(sf.fullName),
        owner(sf.owner),
        implLocation(sf.implLocation),
        dependencies(sf.dependencies),
        type(sf.type),
        flags(sf.flags),
        fieldType(sf.fieldType),
        closure(sf.closure),
        ast(sf.ast),
        getter(sf.getter),
        setter(sf.setter),
        scheme(NULL),
        request(NULL)
    {
        create_scheme(sf.scheme);

        if(sf.request)
            request = new injection_request(*sf.request);
    }

    sharp_field(
            string &name,
            sharp_class *owner,
            impl_location &location,
            sharp_type &type,
            uInt flags,
            field_type ft,
            Ast *ast)
    :
            name(name),
            fullName(""),
            owner(owner),
            implLocation(location),
            dependencies(),
            type(),
            flags(flags),
            fieldType(ft),
            closure(NULL),
            ast(ast),
            getter(NULL),
            setter(NULL),
            scheme(NULL),
            request(NULL)
    {
        set_full_name();
        this->type.copy(type);
    }

    ~sharp_field() {
        free();
    }

    void free();

    void set_full_name();

    void create_scheme(operation_scheme *);

    string name;
    string fullName;
    sharp_class *owner;
    sharp_function *getter;
    sharp_function *setter;
    impl_location implLocation;
    List<dependency> dependencies;
    sharp_field* closure;
    operation_scheme* scheme;
    injection_request *request;
    sharp_type type;
    field_type fieldType;
    uInt flags;
    Ast* ast;
};

sharp_field* create_field(
        sharp_file*,
        sharp_module*,
        string,
        uInt,
        sharp_type,
        field_type,
        Ast*);

sharp_field* create_field(
        sharp_file*,
        sharp_class*,
        string,
        uInt,
        sharp_type,
        field_type,
        Ast*);

sharp_field* create_closure_field(
        sharp_class*,
        string,
        sharp_type,
        Ast*);

bool can_capture_closure(sharp_field*);


#endif //SHARP_SHARP_FIELD_H
