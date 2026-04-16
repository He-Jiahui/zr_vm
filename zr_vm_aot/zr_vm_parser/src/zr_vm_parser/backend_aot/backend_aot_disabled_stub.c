//
// Built when ZR_VM_BUILD_AOT=OFF: satisfies writer.h AOT entry points without linking the full backend.
//

#include "zr_vm_parser/writer.h"

#include "zr_vm_core/log.h"

#include <stdio.h>

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFileWithOptions(SZrState *state,
                                                               SZrFunction *function,
                                                               const TZrChar *filename,
                                                               const SZrAotWriterOptions *options) {
    ZR_UNUSED_PARAMETER(function);
    ZR_UNUSED_PARAMETER(filename);
    ZR_UNUSED_PARAMETER(options);
    if (state != ZR_NULL) {
        ZrCore_Log_Error(state, "AOT C backend is disabled (configure with -DZR_VM_BUILD_AOT=ON)\n");
    } else {
        fprintf(stderr, "AOT C backend is disabled (configure with -DZR_VM_BUILD_AOT=ON)\n");
    }
    return ZR_FALSE;
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFile(SZrState *state, SZrFunction *function, const TZrChar *filename) {
    return ZrParser_Writer_WriteAotCFileWithOptions(state, function, filename, ZR_NULL);
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFileWithOptions(SZrState *state,
                                                                SZrFunction *function,
                                                                const TZrChar *filename,
                                                                const SZrAotWriterOptions *options) {
    ZR_UNUSED_PARAMETER(function);
    ZR_UNUSED_PARAMETER(filename);
    ZR_UNUSED_PARAMETER(options);
    if (state != ZR_NULL) {
        ZrCore_Log_Error(state, "AOT LLVM backend is disabled (configure with -DZR_VM_BUILD_AOT=ON)\n");
    } else {
        fprintf(stderr, "AOT LLVM backend is disabled (configure with -DZR_VM_BUILD_AOT=ON)\n");
    }
    return ZR_FALSE;
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFile(SZrState *state, SZrFunction *function, const TZrChar *filename) {
    return ZrParser_Writer_WriteAotLlvmFileWithOptions(state, function, filename, ZR_NULL);
}
