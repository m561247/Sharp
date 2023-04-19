//
// Created by bknun on 9/16/2022.
//

#include "manifest_processor.h"
#include "exe_processor.h"
#include "../../core/exe_macros.h"
#include "../virtual_machine.h"
#include "../main.h"

void process_manifest(KeyPair<int, string> &result) {
    check_section(manif, CORRUPT_MANIFEST, result);

    int processedFlags = 0;
    PROCESS_SECTION(eoh,

         case 0x2:
            vm.manifest.application = next_string();
            break;
         case 0x4:
             vm.manifest.version = next_string();
            break;
         case 0x5:
             vm.manifest.debug = next_char() == '1';
            break;
         case 0x6:
             vm.manifest.entryMethod = next_int32();
            break;
         case 0x7:
             vm.manifest.methods = next_int32();
            break;
         case 0x8:
             vm.manifest.classes = next_int32();
            break;
         case 0x9:
             vm.manifest.fvers = parse_int();
            break;
         case 0x0c:
             vm.manifest.strings = next_int32();
            break;
         case 0x0e:
             vm.manifest.target = parse_int();
            break;
         case 0x0f:
             vm.manifest.sourceFiles = next_int32();
            break;
         case 0x1b:
             vm.manifest.threadLocals = next_int32();
             break;
         case 0x1c:
             vm.manifest.constants = next_int32();
            break;
         , // on section_end

         if(processedFlags != HEADER_SIZE) {
             result.key = CORRUPT_MANIFEST;
             throw runtime_error("");
         }
         else if(vm.manifest.target > BUILD_VERS) {
             result.key = UNSUPPORTED_BUILD_VERSION;
             throw runtime_error("");
         }

         if(!(vm.manifest.fvers >= min_file_vers && vm.manifest.fvers <= file_vers)) {
             stringstream err;
             err << "unsupported file version of: " << vm.manifest.fvers << ". Sharp supports file versions from `"
                 << min_file_vers << "-" << file_vers << "` that can be processed. Are you possibly targeting the incorrect virtual machine?";

             result.with(UNSUPPORTED_FILE_VERS,  err.str());
             throw runtime_error("");
         }

         break;
    )
}
