//
// Created by BNunnally on 8/31/2021.
//

#ifndef SHARP_DEPENDANCY_H
#define SHARP_DEPENDANCY_H

#include "../../../../stdimports.h"
#include "../types/sharp_type.h"
#include "../../List.h"
#include "../../frontend/parser/Ast.h"

struct sharp_file;
struct sharp_class;
struct sharp_module;
struct sharp_function;
struct sharp_field;
struct sharp_alias;
struct import_group;

enum dependency_type {
    no_dependency,
    dependency_file,
    dependency_class,
    dependency_function,
    dependency_field
};

/**
 *
 * File dependencies:
 *  - Files
 *
 * class dependencies: File Owner -> files holding each class
 *  - Classes
 *
 * Field dependencies: Class Owner -> classes holding each item
 *  - Classes
 *  - Fields
 *  - Functions
 *
 * Function Dependencies: Class Owner -> classes holding each item
 *  - Fields
 *  - Functions
 *  - Classes
 *
 */
struct dependency {
    dependency()
    :
        fileDependency(NULL),
        classDependency(NULL),
        functionDependency(NULL),
        fieldDependency(NULL),
        type(no_dependency)
    {}

    dependency(const dependency &d)
            :
            fileDependency(d.fileDependency),
            classDependency(d.classDependency),
            functionDependency(d.functionDependency),
            fieldDependency(d.fieldDependency),
            type(d.type)
    {}

    dependency(sharp_file *file)
            :
            fileDependency(file),
            classDependency(NULL),
            functionDependency(NULL),
            fieldDependency(NULL),
            type(dependency_file)
    {}

    dependency(sharp_class *sc)
            :
            fileDependency(NULL),
            classDependency(sc),
            functionDependency(NULL),
            fieldDependency(NULL),
            type(dependency_class)
    {}

    dependency(sharp_function *sf)
            :
            fileDependency(NULL),
            classDependency(NULL),
            functionDependency(sf),
            fieldDependency(NULL),
            type(dependency_function)
    {}

    dependency(sharp_field *sf)
            :
            fileDependency(NULL),
            classDependency(NULL),
            functionDependency(NULL),
            fieldDependency(sf),
            type(dependency_field)
    {}

    bool operator==(const dependency &d) {
        if(type == d.type) {
            switch(type) {
                case no_dependency:
                    return true;
                case dependency_file:
                    return d.fileDependency == fileDependency;
                case dependency_class:
                    return d.classDependency == classDependency;
                case dependency_function:
                    return d.functionDependency == functionDependency;
                case dependency_field:
                    return d.fieldDependency == fieldDependency;
                default: return false;
            }
        } else return false;
    }

    sharp_file *fileDependency;
    sharp_class *classDependency;
    sharp_function *functionDependency;
    sharp_field *fieldDependency;
    dependency_type type;
};

void create_dependency(sharp_class* depender, sharp_class* dependee);
void create_dependency(sharp_file* depender, sharp_file* dependee);
void create_dependency(sharp_function* depender, sharp_function* dependee);
void create_dependency(sharp_function* depender, sharp_class* dependee);
void create_dependency(sharp_function* depender, sharp_field* dependee);
void create_dependency(sharp_field* depender, sharp_function* dependee);
void create_dependency(sharp_field* depender, sharp_class* dependee);
void create_dependency(sharp_field* depender, sharp_field* dependee);

sharp_class* resolve_class(import_group*, string, bool, bool);
sharp_class* resolve_class(sharp_module*, string, bool, bool);
sharp_class* resolve_class(sharp_file*, string, bool, bool);
sharp_class* resolve_class(sharp_class*, string, bool, bool);
sharp_class* resolve_class(string, bool, bool);

import_group* resolve_import_group(sharp_file*, string);

sharp_function* resolve_function(
        string name,
        import_group *group,
        List<sharp_field*> &parameters,
        Int functionType,
        uInt excludeMatches,
        Ast *resolveLocation,
        bool checkBaseClass,
        bool implicitCheck);

sharp_function* resolve_function(
        string name,
        sharp_file *file,
        List<sharp_field*> &parameters,
        Int functionType,
        uInt excludeMatches,
        Ast *resolveLocation,
        bool checkBaseClass,
        bool implicitCheck);

sharp_function* resolve_function(
        string name,
        sharp_module *module,
        List<sharp_field*> &parameters,
        Int functionType,
        uInt excludeMatches,
        Ast *resolveLocation,
        bool checkBaseClass,
        bool implicitCheck);

sharp_function* resolve_function(
        string name,
        sharp_class *searchClass,
        List<sharp_field*> &parameters,
        Int functionType,
        uInt excludeMatches,
        Ast *resolveLocation,
        bool checkBaseClass,
        bool implicitCheck);

sharp_alias* resolve_alias(string, sharp_module*);
sharp_alias* resolve_alias(string, sharp_file*);
sharp_alias* resolve_alias(string, import_group*);
sharp_alias* resolve_alias(string, sharp_class*);

sharp_field* resolve_field(string, sharp_module*);
sharp_field* resolve_field(string, sharp_file*);
sharp_field* resolve_field(string, import_group*);
sharp_field* resolve_field(string, sharp_class*);

sharp_type resolve(Ast*);

#endif //SHARP_DEPENDANCY_H
