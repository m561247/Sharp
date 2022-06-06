//
// Created by BNunnally on 9/1/2021.
//

#ifndef SHARP_SHARP_FUNCTION_H
#define SHARP_SHARP_FUNCTION_H

#include "../../../../stdimports.h"
#include "../../List.h"
#include "../dependency/dependancy.h"
#include "../meta_data.h"
#include "../access_flag.h"
#include "sharp_type.h"
#include "function_type.h"

struct sharp_class;
struct sharp_field;

void set_full_name(sharp_function*);

struct sharp_function {
    sharp_function()
    :
            name(""),
            fullName(""),
            owner(NULL),
            implLocation(),
            dependencies(),
            flags(flag_none),
            ast(NULL),
            closure(NULL),
            scheme(NULL),
            parameters(),
            returnType(),
            locals(),
            labels(),
            type(undefined_function),
            directlyCopyParams(false)
    {}

    sharp_function(const sharp_function &sf)
            :
            name(sf.name),
            fullName(sf.fullName),
            owner(sf.owner),
            implLocation(sf.implLocation),
            dependencies(sf.dependencies),
            flags(sf.flags),
            ast(sf.ast),
            parameters(),
            locals(),
            labels(),
            returnType(sf.returnType),
            type(sf.type),
            closure(sf.closure),
            directlyCopyParams(sf.directlyCopyParams),
            scheme(NULL)
    {
        copy_parameters(sf.parameters);
        copy_locals(sf.locals);
        copy_scheme(sf.scheme);
    }

    sharp_function(
            string &name,
            sharp_class *owner,
            impl_location location,
            uInt flags,
            Ast *ast,
            List<sharp_field*> &parameters,
            sharp_type &returnType,
            function_type type,
            bool directlyCopyParams = false)
    :
            name(name),
            fullName(""),
            owner(owner),
            implLocation(location),
            dependencies(),
            parameters(),
            flags(flags),
            ast(ast),
            returnType(),
            type(type),
            locals(),
            labels(),
            directlyCopyParams(directlyCopyParams),
            scheme(NULL),
            closure(NULL)
    {
        this->returnType.copy(returnType);
        if(!directlyCopyParams)
            copy_parameters(parameters);
        else this->parameters.addAll(parameters);
        set_full_name(this);
    }

    ~sharp_function() {
        free();
    }

    void free();
    void copy_parameters(const List<sharp_field*> &params);
    void copy_scheme(operation_schema *operations);
    void copy_locals(const List<sharp_field*> &params);

    string name;
    string fullName;
    sharp_class *owner;
    impl_location implLocation;
    List<dependency> dependencies;
    List<sharp_field*> parameters;
    List<sharp_field*> locals;
    List<sharp_label*> labels;
    operation_schema *scheme;
    sharp_field* closure;
    sharp_type returnType;
    function_type type;

    uInt flags;
    Ast* ast;
    bool directlyCopyParams; // we need to do this for get() expressions to preserve the resolved type definition found
};

bool is_fully_qualified_function(sharp_function*);
bool function_parameters_match(List<sharp_field*>&, List<sharp_field*>&, bool, uInt excludedMateches = 0);


bool create_function(
        sharp_class *sc,
        uInt flags,
        function_type type,
        string &name,
        bool checkBaseClass,
        List<sharp_field*> &params,
        sharp_type &returnType,
        Ast *createLocation);

bool create_function(
        sharp_class *sc,
        uInt flags,
        function_type type,
        string &name,
        bool checkBaseClass,
        List<sharp_field*> &params,
        sharp_type &returnType,
        Ast *createLocation,
        sharp_function *&createdFun);

void create_default_constructor(sharp_class*, uInt, Ast*);

string function_to_str(sharp_function*);

sharp_label* create_label(
        string name,
        context *context,
        Ast *createLocation,
        operation_schema *scheme);

#endif //SHARP_SHARP_FUNCTION_H
