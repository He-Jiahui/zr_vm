#ifndef ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_DIAGNOSTICS_H
#define ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_DIAGNOSTICS_H

#include "compiler_internal.h"

/**
 * @brief 在声明 patch 提交前验证并发布其 typed CompileDiagnostic 列表。
 *
 * 生产调用者应将 false 视为整个 patch 失败；true 仍需检查 hasErrorDiagnostic，
 * 因为 error 消息会被发布但必须阻止后续原子 commit。
 * @pre cs 已初始化，diagnosticsValue 是运行时数组，输出标记非空。
 */
ZR_PARSER_API TZrBool ZrParser_CompileTime_ProcessPatchDiagnostics(
        SZrCompilerState *cs,
        const SZrTypeValue *diagnosticsValue,
        TZrSymbolId patchTargetSymbolId,
        SZrFileRange location,
        TZrBool *hasErrorDiagnostic);

#endif // ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_DIAGNOSTICS_H
