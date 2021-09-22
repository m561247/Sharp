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

void post_process() {
    sharp_file *file = currThread->currTask->file;
    sharp_class *globalClass = NULL;

    process_imports();
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

                currThread->currTask->file->errors->createNewError(GENERIC, trunk->line, trunk->col, "module declaration must be ""first in every file");
            }

            globalClass = resolve_class(currModule, global_class_name, false, false);
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
                /* ignore */
                break;
            case ast_module_decl: /* fail-safe */
                currThread->currTask->file->errors->createNewError(
                        GENERIC, trunk->line, trunk->col, "file module cannot be declared more than once");
                break;
            case ast_method_decl:
                process_function(globalClass, normal_function, trunk);
                break;
            case ast_delegate_decl:
                process_function(globalClass, delegate_function, trunk);
                break;
            case ast_mutate_decl:
            case ast_import_decl:
            case ast_obfuscate_decl:
                /* ignore */
                break;
            default:
                stringstream err;
                err << ": unknown ast type: " << trunk->getType();
                currThread->currTask->file->errors->createNewError(
                        INTERNAL_ERROR, trunk->line, trunk->col, err.str());
                break;
        }
    }

    delete_context();
}

void process_class(sharp_class* parentClass, sharp_class *with_class, Ast *ast) {
    Ast* block = ast->getSubAst(ast_block);

    if(with_class == NULL) {
        string name = ast->getToken(0).getValue();
        with_class = resolve_class(parentClass, name, false, false);
    }

    create_context(with_class, true);
    process_base_class(with_class, ast);
    process_interfaces(with_class, ast);

    for(Int i = 0; i < block->getSubAstCount(); i++) {
        Ast *trunk = block->getSubAst(i);

        switch(trunk->getType()) {
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

            case ast_init_decl:
            case ast_enum_decl:
            case ast_obfuscate_decl:
            case ast_generic_interface_decl:
            case ast_generic_class_decl:
                /* ignore */
                break;
            default:
                stringstream err;
                err << ": unknown ast type: " << trunk->getType();
                currThread->currTask->file->errors->createNewError(INTERNAL_ERROR, trunk->line, trunk->col, err.str());
                break;
        }
    }

    delete_context();
}