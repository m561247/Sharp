//
// Created by BNunnally on 11/27/2021.
//

#include "vector_array_expression.h"
#include "../../types/types.h"
#include "expression.h"
#include "../../../compiler_info.h"


void compile_vector_array_expression(expression *e, Ast *ast) {
    sharp_type arrayType;
    List<expression*> arrayItems;

    for(Int i = 0; i < ast->getSubAstCount(); i++) {
        compile_vector_item(ast->getSubAst(i), &arrayType, arrayItems);
    }

    if(has_type(arrayType)) {
        for (Int i = 0; i < arrayItems.size(); i++) {
            if (!is_implicit_type_match(arrayType, arrayItems.at(i)->type,
                                        constructor_only)) {
                current_file->errors->createNewError(GENERIC, ast->line, ast->col,
                                                     " expected array item of type `" + type_to_str(arrayType) +
                                                     "` but found `" + type_to_str(arrayItems.at(i)->type) + "`.");
            }
        }

        operation_scheme *arraySizeScheme = new operation_scheme();
        arraySizeScheme->schemeType = scheme_get_constant;
        arraySizeScheme->steps.add(
                new operation_step(operation_get_integer_constant, (Int) arrayItems.size()));



        if(arrayType.type == type_class)
            create_new_class_array_operation(&e->scheme, arraySizeScheme, e->type._class);
        else if(arrayType.type >= type_int8 && arrayType.type <= type_var)
            create_new_number_array_operation(&e->scheme, arraySizeScheme, e->type.type);
        else if(arrayType.type == type_object)
            create_new_object_array_operation(&e->scheme, arraySizeScheme);
        else {
            currThread->currTask->file->errors->createNewError(GENERIC, ast->line, ast->col,
                                                               "cannot create array of type `" + type_to_str(e->type) +"`.");
        }

        Int matchResult;
        create_push_to_stack_operation(&e->scheme);
        for(Int i = 0; i < arrayItems.size(); i++) {
            expression *expr = arrayItems.get(i);
            sharp_function *matchedConstructor = NULL;
            operation_scheme *setArrayItem = new operation_scheme();
            setArrayItem->schemeType = scheme_get_array_value;
            matchResult = is_explicit_type_match(arrayType, expr->type);
            if (matchResult == no_match_found) {
                matchResult = is_implicit_type_match(
                        arrayType, expr->type,
                        constructor_only,
                        matchedConstructor);

            }

            if(matchResult != no_match_found) {
                if(matchResult == match_normal) {
                    setArrayItem->steps.add(new operation_step(operation_get_value, &expr->scheme));
                } else { // match_constructor
                    operation_scheme *arrayItemScheme = new operation_scheme(), resultScheme;
                    arrayItemScheme->schemeType = scheme_new_class;
                    arrayItemScheme->sc = arrayType._class;

                    arrayItemScheme->steps.add(
                            new operation_step(
                                    operation_create_class,
                                    e->type._class
                            )
                    );

                    List<operation_scheme*> scheme;
                    scheme.add(arrayItemScheme);
                    create_instance_function_call_operation(
                            &resultScheme, scheme, matchedConstructor);
                    deleteList(scheme);
                    setArrayItem->steps.add(new operation_step(operation_step_scheme, &resultScheme));
                }

                create_push_to_stack_operation(setArrayItem);
                create_assign_array_element_operation(setArrayItem, i);
            }

            e->scheme.steps.add(new operation_step(operation_assign_array_value, setArrayItem));
        }
    } else {
        currThread->currTask->file->errors->createNewError(GENERIC, ast->line, ast->col,
                                                           " could not determine the type of the array, please ensure all array values are typed.");
    }
}

void compile_vector_item(Ast *ast, sharp_type *arrayType, List<expression*>& arrayItems) {
    expression *item = new expression();

    compile_expression(*item, ast);
    convert_expression_type_to_real_type(*item);

    if (!has_type(*arrayType) && has_type(item->type)) {
        arrayType->copy(item->type);
    } else if (has_type(item->type)) {
        if (!is_implicit_type_match(
                *arrayType, item->type,
                constructor_only)) {
            if (is_implicit_type_match(
                    item->type, *arrayType,
                    constructor_only)) {
                arrayType->copy(item->type);
            }
        }
    }

    arrayItems.add(item);
}