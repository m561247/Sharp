//
// Created by BNunnally on 9/16/2021.
//

#include "class_processor.h"
#include "../types/sharp_class.h"
#include "../../taskdelegator/task_delegator.h"
#include "../astparser/ast_parser.h"
#include "base_class_processor.h"
#include "interface_processor.h"
#include "import_processor.h"
#include "../../settings/settings.h"
#include "field_processor.h"
#include "alias_processor.h"
#include "../types/sharp_function.h"
#include "function_processor.h"
#include "delegate_processor.h"
#include "mutation_processor.h"
#include "../preprocessor/class_preprocessor.h"
#include "../compiler/functions/init_compiler.h"

void post_process() {
    sharp_file *file = currThread->currTask->file;
    sharp_class *globalClass = NULL;

    for(Int i = 0; i < file->p->size(); i++)
    {
        if(panic) return;

        Ast *trunk = file->p->astAt(i);
        if(i == 0) {
            if(trunk->getType() == ast_module_decl) {
                string package = concat_tokens(trunk);
                currModule = create_module(package);
            } else {
                string package = "__$srt_undefined";
                currModule = create_module(package);

                create_new_error(GENERIC, trunk->line, trunk->col, "module declaration must be ""first in every file");
            }

            globalClass = resolve_class(currModule, global_class_name, false, false);
            create_class_init_functions(globalClass, trunk);
            create_context(globalClass, true);
            continue;
        }

        switch(trunk->getType()) {
            case ast_interface_decl:
            case ast_class_decl:
                process_class(globalClass, NULL, trunk);
                break;
            case ast_variable_decl:
                process_field(globalClass, trunk);
                break;
            case ast_alias_decl:
                process_alias(globalClass, trunk);
                break;
            case ast_generic_class_decl:
            case ast_generic_interface_decl:
                /* ignore */
                break;
            case ast_enum_decl:
                process_class(globalClass, NULL, trunk);
                break;
            case ast_module_decl: /* fail-safe */
                create_new_error(
                        GENERIC, trunk->line, trunk->col, "file module cannot be declared more than once");
                break;
            case ast_method_decl:
                process_function(globalClass, normal_function, trunk);
                break;
            case ast_operator_decl:
                process_function(globalClass, operator_function, trunk);
                break;
            case ast_delegate_decl:
                process_function(globalClass, delegate_function, trunk);
                break;
            case ast_mutate_decl:
            case ast_import_decl:
            case ast_obfuscate_decl:
            case ast_component_decl:
                /* ignore */
                break;
            default:
                stringstream err;
                err << ": unknown ast type: " << trunk->getType();
                create_new_error(
                        INTERNAL_ERROR, trunk->line, trunk->col, err.str());
                break;
        }
    }

    delete_context();
}

void process_generics() {
    sharp_file *file = currThread->currTask->file;
    sharp_class *globalClass = NULL;

    for(Int i = 0; i < file->p->size(); i++)
    {
        if(panic) return;

        Ast *trunk = file->p->astAt(i);
        if(i == 0) {
            if(trunk->getType() == ast_module_decl) {
                string package = concat_tokens(trunk);
                currModule = create_module(package);
            } else {
                string package = "__$srt_undefined";
                currModule = create_module(package);

                create_new_error(GENERIC, trunk->line, trunk->col, "module declaration must be ""first in every file");
            }

            globalClass = resolve_class(currModule, global_class_name, false, false);
            create_class_init_functions(globalClass, trunk);
            create_context(globalClass, true);
            continue;
        }

        switch(trunk->getType()) {
            case ast_generic_class_decl:
            case ast_generic_interface_decl:
                process_generic_class(globalClass, NULL, trunk);
                break;
            case ast_enum_decl:
            case ast_module_decl:
            case ast_method_decl:
            case ast_operator_decl:
            case ast_delegate_decl:
            case ast_interface_decl:
            case ast_class_decl:
            case ast_variable_decl:
            case ast_alias_decl:
            case ast_mutate_decl:
            case ast_import_decl:
            case ast_obfuscate_decl:
            case ast_component_decl:
                /* ignore */
                break;
            default:
                stringstream err;
                err << ": unknown ast type: " << trunk->getType();
                create_new_error(
                        INTERNAL_ERROR, trunk->line, trunk->col, err.str());
                break;
        }
    }

    delete_context();
}


void create_class_init_functions(sharp_class *with_class, Ast *ast) {
    string name;
    sharp_function *function;
    List<sharp_field*> params;
    sharp_type void_type(type_nil);
    GUARD(globalLock)

    function = resolve_function(
            static_init_name(with_class->name), with_class,
            params, initializer_function, exclude_all,
            ast, false, false
    );

    if(function == NULL) {
        name = static_init_name(with_class->name);
        create_function(
                with_class, flag_private | flag_static,
                initializer_function, name,
                false, params,
                void_type, ast, function
        );

        with_class->staticInit = function;
        function->scheme = new operation_schema(scheme_master);
        compile_static_initialization_check(function);
    }


    function = resolve_function(
            instance_init_name(with_class->name), with_class,
            params,initializer_function, exclude_all,
            ast, false, false
    );

    if(function == NULL) {
        name = instance_init_name(with_class->name);
        create_function(
                with_class, flag_private,
                initializer_function, name,
                false, params,
                void_type, ast, function
        );

        function->scheme = new operation_schema(scheme_master);
    }
}

void process_generic_identifier(sharp_class *genericClass, generic_type_identifier &gt, Ast *ast) {
    if(ast->hasSubAst(ast_utype)) {
        sharp_type baseType = resolve(ast->getSubAst(ast_utype));
        gt.baseType.copy(baseType);
        sharp_class *sc = get_top_level_class(get_class_type(gt.type));

        if(sc && sc->genericBuilder != NULL && !sc->genericProcessed) {
            process_generic_class(NULL, sc, sc->ast);
        }

        if(!is_match_normal(is_implicit_type_match(gt.baseType, gt.type, exclude_all))) {

            stringstream ss;
            ss << "for generic class `" <<  genericClass->genericBuilder->fullName << "` type ("
               << gt.name << ") must contain base type `" << type_to_str(gt.baseType) << "` but type `" +
                                         type_to_str(gt.type) + "` was found.";
            create_new_error(GENERIC, genericClass->genericBuilder->ast->line, genericClass->genericBuilder->ast->col, ss.str());
        }
    }
}

void process_generic_identifier_list(sharp_class *genericClass, Ast *ast) {

    for(Int i = 0; i < genericClass->genericTypes.size(); i++) {
        process_generic_identifier(genericClass, genericClass->genericTypes.get(i), ast->getSubAst(i));
    }
}

void process_generic_class(sharp_class* parentClass, sharp_class *with_class, Ast *ast) {
    if(with_class == NULL) {
        string name = ast->getToken(0).getValue();
        with_class = resolve_class(parentClass, name, true, false);
    }

    if(with_class->blueprintClass) {
        for (Int i = 0; i < with_class->genericClones.size(); i++) {
            process_generic_class(NULL, with_class->genericClones.get(i), ast);
        }
    } else if(with_class->genericBuilder != NULL && !with_class->genericProcessed){
        with_class->genericProcessed = true;
        pre_process_class(NULL, with_class, with_class->ast);
        process_class(NULL, with_class, with_class->ast);
        process_generic_extension_functions(with_class, with_class->genericBuilder);
        process_generic_mutations(with_class, with_class->genericBuilder);
//        process_delegates(with_class);
    }

    for(Int i = 0; i < with_class->children.size(); i++) {
        process_generic_class(NULL, with_class->children.get(i), ast);
    }

    for(Int i = 0; i < with_class->generics.size(); i++) {
        process_generic_class(NULL, with_class->generics.get(i), ast);
    }
}

void process_class(sharp_class* parentClass, sharp_class *with_class, Ast *ast) {
    Ast* block = ast->getSubAst(ast_block);

    if(with_class == NULL) {
        string name = ast->getToken(0).getValue();
        with_class = resolve_class(parentClass, name, false, false);
    }

    create_context(with_class, true);
    if(with_class->genericBuilder) {
        process_generic_identifier_list(with_class, with_class->ast->getSubAst(ast_generic_identifier_list));
    }


    create_static_init_flag_field(with_class, ast);
    process_base_class(with_class, ast);
    process_interfaces(with_class, ast);
    create_class_init_functions(with_class, ast);

    if(block != NULL) {
        for (Int i = 0; i < block->getSubAstCount(); i++) {
            Ast *trunk = block->getSubAst(i);

            switch (trunk->getType()) {
                case ast_interface_decl:
                case ast_class_decl:
                    process_class(with_class, NULL, trunk);
                    break;
                case ast_variable_decl:
                    process_field(with_class, trunk);
                    break;
                case ast_alias_decl:
                    process_alias(with_class, trunk);
                    break;
                case ast_method_decl:
                    process_function(with_class, normal_function, trunk);
                    break;
                case ast_operator_decl:
                    process_function(with_class, operator_function, trunk);
                    break;
                case ast_delegate_decl:
                    process_function(with_class, delegate_function, trunk);
                    break;
                case ast_construct_decl:
                    process_function(with_class, constructor_function, trunk);
                    break;
                case ast_init_func_decl:
                    process_function(with_class, initializer_function, trunk);
                    break;
                case ast_mutate_decl:
                    process_mutation(trunk);
                    break;

                case ast_enum_decl:
                    process_class(with_class, NULL, trunk);
                    break;

                case ast_init_decl:
                case ast_obfuscate_decl:
                case ast_generic_interface_decl:
                case ast_generic_class_decl:
                case ast_enum_identifier:
                    /* ignore */
                    break;
                default:
                    stringstream err;
                    err << ": unknown ast type: " << trunk->getType();
                    create_new_error(INTERNAL_ERROR, trunk->line, trunk->col,
                                                                       err.str());
                    break;
            }
        }
    }


    create_default_constructor(with_class, flag_public, with_class->ast);
    delete_context();
}
