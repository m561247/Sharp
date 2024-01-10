//
// Created by bknun on 7/4/2023.
//

#include "code_analyzer.h"
#include "../../generation/generator.h"
#include "code_block_analyzer.h"
#include "variable_write_analyzer.h"
#include "new_class_analyzer.h"
#include "debug_info_analyzer.h"
#include "while_block_analyzer.h"
#include "constant_analyzer.h"
#include "if_block_analyzer.h"
#include "analyze_break.h"
#include "analyze_function_call.h"
#include "get_value_analyzer.h"
#include "variable_read_analyzer.h"

code_fragment* analyze_code(operation_schema *scheme) {
    code_fragment *frag = NULL;

    if(scheme != NULL) {
        switch (scheme->schemeType) {
            case scheme_master:
                return analyze_code_block(scheme);
            case scheme_line_info:
                return analyze_debug_info(scheme);
            case scheme_assign_value:
                // need to check multiple fragments
                if((frag = analyze_local_variable_write(scheme)))
                    return frag;
                else {
                    stringstream ss;
                    ss << "unknown scheme type: " << scheme->schemeType;
                    generation_error(ss.str());
                    break;
                }
                break;
            case scheme_new_class:
                return analyze_new_class(scheme);
            case scheme_while:
                return analyze_while_block(scheme);
            case scheme_get_constant:
                return analyze_constant(scheme);
            case scheme_if_single:
                return analyze_if_block(scheme);
            case scheme_break:
                return analyze_break(scheme);
            case scheme_call_instance_function:
                return analyze_function_call(scheme);
            case scheme_get_value:
                return analyze_get_value(scheme);
            case scheme_access_local_field:
                return analyze_local_variable_read(scheme);
            case scheme_access_instance_field:
                return analyze_instance_variable_read(scheme);
            default: {
                stringstream ss;
                ss << "unknown scheme type: " << scheme->schemeType;
                generation_error(ss.str());
                break;
            }
        }
    }

    return frag;
}

code_fragment* require_non_null(code_fragment *frag) {
    if(frag == NULL) {
        generation_error("expected code fragment but 'NULL' was found.");
    }

    return frag;
}

bool is_ignored_scheme(operation_schema *scheme) {
    return scheme->schemeType == scheme_unused_data || scheme->schemeType == scheme_return;
}
