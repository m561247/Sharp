//
// Created by bknun on 8/8/2022.
//

#include "scheme_processor.h"
#include "../code_context.h"
#include "../../generator.h"
#include "step_processor.h"
#include "scheme_access_static_field.h"
#include "scheme_access_primary_instance_field.h"
#include "scheme_access_tls_field.h"
#include "scheme_master.h"
#include "scheme_return.h"
#include "scheme_negate_value.h"
#include "scheme_line_info.h"
#include "scheme_inc_value.h"
#include "scheme_dec_value.h"
#include "scheme_not_value.h"
#include "scheme_call_static_function.h"
#include "scheme_get_constant.h"
#include "scheme_call_instance_function.h"
#include "scheme_new_class.h"
#include "scheme_call_primary_class_instance_function.h"
#include "scheme_call_dynamic_function.h"
#include "scheme_get_primary_class_instance.h"
#include "scheme_access_local_field.h"
#include "scheme_get_address.h"
#include "scheme_nullify_value.h"
#include "scheme_new_class_array.h"
#include "scheme_new_number_array.h"
#include "scheme_new_object_array.h"
#include "scheme_assign_array_value.h"
#include "scheme_get_size_of.h"
#include "scheme_get_casted_value.h"
#include "scheme_null_fallback.h"
#include "scheme_inline_if.h"
#include "scheme_assign_value.h"
#include "scheme_compound_assign_value.h"
#include "scheme_binary_math.h"
#include "scheme_accelerated_binary_math.h"
#include "scheme_or.h"
#include "scheme_null_check.h"
#include "scheme_instance_check.h"
#include "scheme_post_increment.h"
#include "scheme_unused_data.h"
#include "scheme_get_value.h"
#include "scheme_for.h"
#include "scheme_for_cond.h"
#include "scheme_for_iter.h"
#include "scheme_for_infinite.h"
#include "scheme_lock.h"
#include "scheme_unlock_data.h"
#include "scheme_if_single.h"
#include "scheme_compound_if.h"
#include "scheme_elseif.h"
#include "scheme_else.h"
#include "scheme_increment_for_index_value.h"
#include "scheme_for_each.h"
#include "scheme_for_each_position_check.h"
#include "scheme_for_each_get_array_item.h"
#include "scheme_when.h"
#include "scheme_when_clause.h"
#include "scheme_when_else_clause.h"

void process_scheme(operation_schema *scheme) {
    if(scheme != NULL) {
        switch (scheme->schemeType) {
            case scheme_access_primary_instance_field:
                return process_access_primary_instance_field(scheme);
            case scheme_access_instance_field:
                return process_access_primary_instance_field(scheme);
            case scheme_access_static_field:
                return process_access_static_field_scheme(scheme);
            case scheme_access_tls_field:
                return process_access_tls_field_scheme(scheme);
            case scheme_master:
                return process_master_scheme(scheme);
            case scheme_return:
                return process_return_scheme(scheme);
            case scheme_negate_value:
                return process_negate_value_scheme(scheme);
            case scheme_line_info:
                return process_line_info_scheme(scheme);
            case scheme_inc_value:
                return process_increment_value_scheme(scheme);
            case scheme_dec_value:
                return process_decrement_value_scheme(scheme);
            case scheme_not_value:
                return process_not_value_scheme(scheme);
            case scheme_call_static_function:
                return process_call_static_function_scheme(scheme);
            case scheme_get_constant:
                return process_get_constant_scheme(scheme);
            case scheme_call_instance_function:
                return process_call_instance_function_scheme(scheme);
            case scheme_call_primary_class_instance_function:
                return process_primary_class_instance_function_scheme(scheme);
            case scheme_new_class:
                return process_new_class_scheme(scheme);
            case scheme_call_dynamic_function:
                return process_call_dynamic_function_scheme(scheme);
            case scheme_get_primary_class_instance:
                return process_get_primary_class_instance_scheme(scheme);
            case scheme_access_local_field:
                return process_access_local_field_scheme(scheme);
            case scheme_get_address:
                return process_get_address_scheme(scheme);
            case scheme_nullify_value:
                return process_nullify_value_scheme(scheme);
            case scheme_new_class_array:
                return process_new_class_array_scheme(scheme);
            case scheme_new_number_array:
                return process_new_number_array_scheme(scheme);
            case scheme_new_object_array:
                return process_new_object_array_scheme(scheme);
            case scheme_assign_array_value:
                return process_assign_array_value_scheme(scheme);
            case scheme_get_size_of:
                return process_get_size_of_scheme(scheme);
            case scheme_get_casted_value:
                return process_get_casted_value_scheme(scheme);
            case scheme_null_fallback:
                return process_null_fallback_scheme(scheme);
            case scheme_inline_if:
                return process_inline_if_scheme(scheme);
            case scheme_assign_value:
                return process_assign_value_scheme(scheme);
            case scheme_compound_assign_value:
                return process_compound_assign_value_scheme(scheme);
            case scheme_binary_math:
                return process_binary_math_scheme(scheme);
            case scheme_accelerated_binary_math:
                return process_accelerated_binary_math_scheme(scheme);
            case scheme_or:
                return process_or_scheme(scheme);
            case scheme_null_check:
                return process_null_check_scheme(scheme);
            case scheme_instance_check:
                return process_instance_check_scheme(scheme);
            case scheme_post_increment:
                return process_post_increment_scheme(scheme);
            case scheme_unused_data:
                return process_unused_data_scheme(scheme);
            case scheme_get_value:
                return process_get_value_scheme(scheme);
            case scheme_for:
                return process_for_scheme(scheme);
            case scheme_for_cond:
                return process_for_cond_scheme(scheme);
            case scheme_for_iter:
                return process_for_iter_scheme(scheme);
            case scheme_for_infinite:
                return process_for_infinite(scheme);
            case scheme_lock_data:
                return process_lock_data_scheme(scheme);
            case scheme_unlock_data:
                return process_unlock_data_scheme(scheme);
            case scheme_if_single:
                return process_if_single_scheme(scheme);
            case scheme_compound_if:
                return process_compound_if_scheme(scheme);
            case scheme_elseif:
                return process_elseif_scheme(scheme);
            case scheme_else:
                return process_else_scheme(scheme);
            case scheme_increment_for_index_value:
                return process_increment_for_index_value_scheme(scheme);
            case scheme_for_each:
                return process_for_each_scheme(scheme);
            case scheme_for_each_position_check:
                return process_for_each_position_check_scheme(scheme);
            case scheme_for_each_get_array_item:
                return process_for_each_get_array_item_scheme(scheme);
            case scheme_when:
                return process_when_scheme(scheme);
            case scheme_when_clause:
                return process_when_clause_scheme(scheme);
            case scheme_when_else_clause:
                return process_when_else_clause_scheme(scheme);
            default:
                generation_error("attempt to execute unknown scheme!");
                break;
        }
    }
}

/**
 * A very simplistic way to process a scheme that dosen't require any additional reflection/logic
 * to ensure the correct coe is generated
 *
 * @param scheme
 */
void process_scheme_steps(operation_schema *scheme) {
    for(Int i = 0; i < scheme->steps.size(); i++) {
        process_step(scheme->steps.get(i));
    }
}

void process_scheme(operation_schema *scheme, code_info* ci, sharp_function *container) {
    update_context(ci, container);
    process_scheme(scheme);
    flush_context();
}
