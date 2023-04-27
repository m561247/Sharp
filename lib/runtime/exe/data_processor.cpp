//
// Created by bknun on 9/19/2022.
//

#include "data_processor.h"
#include "exe_processor.h"
#include "../../core/exe_macros.h"
#include "../virtual_machine.h"
#include "../main.h"
#include "../types/sharp_field.h"
#include "../types/try_catch_data.h"

void process_function_data(KeyPair<int, string> &result) {
    check_section(sdata, CORRUPT_FILE, result);

    uInt itemsProcessed=0;
    PROCESS_SECTION(data_byte,
        case data_method: {
            if(itemsProcessed >= vm.manifest.methods) {
                result.with(CORRUPT_FILE, "file `" + executable + "` may be corrupt");
                throw runtime_error("");
            }

            sharp_function* sf = &vm.methods[itemsProcessed++];

            sf->address = next_int32();
            sf->guid = next_int32();
            sf->name = next_string();
            sf->fullName = next_string();
            sf->sourceFile = next_int32();
            sf->owner = &vm.classes[next_int32()];
            sf->fnType = (function_type)next_int32();
            sf->stackSize = next_int32();
            sf->bytecodeSize = next_int32();
            sf->flags = next_int32();
            sf->nativeFunc = ((sf->flags >> 9) & 1u);
            sf->delegateAddress = next_int32();
            sf->fpOffset = next_int32();
            sf->spOffset = next_int32();
            sf->frameStackOffset = next_int32();
            sf->returnType = next_type();
            sf->arrayReturn = next_char() == '1';
            sf->paramSize = next_int32();

            if(sf->paramSize > 0) {
                sf->params = (function_param *) calloc(sf->paramSize, sizeof(function_param));
                for (Int i = 0; i < sf->paramSize; i++) {
                    sf->params[i].type = next_type();
                    sf->params[i].isArray = next_char() == '1';
                }
            }

//        if(c_options.jit) {
//            if(sf->address==316) sf->isjit = true;
//            if(sf->address==7) sf->isjit = true;
//        }

            long len = next_int32();
            for(Int i = 0; i < len; i++) {
                Int pc = next_int32();
                Int line = next_int32();
                sf->lineTable
                        .emplace_back(line_data(pc, line));
            }


            len = next_int32();
            for(Int i = 0; i < len; i++) {
                sf->tryCatchTable.emplace_back(try_catch_data());
                try_catch_data &tryCatchData = *sf->tryCatchTable.end();

                tryCatchData.tryStartPc= next_int32();
                tryCatchData.tryEndPc= next_int32();
                tryCatchData.blockStartPc= next_int32();
                tryCatchData.blockEndPc= next_int32();

                Int caughtExceptions = next_int32();
                for(Int j =0; j < caughtExceptions; j++) {
                    tryCatchData.catchTable.emplace_back(catch_data());
                    catch_data &catchData = *tryCatchData.catchTable.end();
                    catchData.handlerPc = next_int32();
                    catchData.exceptionFieldAddress = next_int32();
                    catchData.exception = &vm.classes[next_int32()];
                }

                if(next_int32() == 1) {
                    tryCatchData.finallyData = (finally_data*)malloc(sizeof(finally_data));
                    tryCatchData.finallyData->startPc = next_int32();
                    tryCatchData.finallyData->endPc = next_int32();
                    tryCatchData.finallyData->exceptionFieldAddress = next_int32();
                }
            }
            break;
        }, // on section_end

        if(itemsProcessed != vm.manifest.methods)
            throw std::runtime_error("data section may be corrupt");
        cursor--;
        break;
    )
}