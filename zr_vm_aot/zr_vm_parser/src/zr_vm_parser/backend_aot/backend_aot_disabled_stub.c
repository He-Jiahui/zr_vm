//
// 历史 AOT 禁用入口；当前 parser CMake 显式排除此文件，只有独立编译时生效。
// TODO: 下列诊断仍提示已移除的 ZR_VM_BUILD_AOT 开关；若重新启用此文件，
// 先核对构建配置与 writer.h 消费者，再更新失效的配置建议。
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

ZR_PARSER_API TZrBool ZrParser_Writer_ResolveTopLevelCallableFlatIndex(SZrState *state,
                                                                       SZrFunction *function,
                                                                       const TZrChar *callableName,
                                                                       TZrUInt32 *outFlatIndex) {
    ZR_UNUSED_PARAMETER(function);
    ZR_UNUSED_PARAMETER(callableName);
    if (outFlatIndex != ZR_NULL) {
        *outFlatIndex = (TZrUInt32)0xFFFFFFFFu;
    }
    if (state != ZR_NULL) {
        ZrCore_Log_Error(state, "AOT C backend is disabled (configure with -DZR_VM_BUILD_AOT=ON)\n");
    } else {
        fprintf(stderr, "AOT C backend is disabled (configure with -DZR_VM_BUILD_AOT=ON)\n");
    }
    return ZR_FALSE;
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
