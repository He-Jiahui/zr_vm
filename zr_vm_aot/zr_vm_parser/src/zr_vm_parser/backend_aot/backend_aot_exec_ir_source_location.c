#include "backend_aot_exec_ir_source_location.h"

TZrUInt32 backend_aot_exec_ir_debug_line_for_instruction(const SZrFunction *function,
                                                         TZrUInt32 execInstructionIndex) {
    TZrUInt32 bestLine = 0;

    if (function == ZR_NULL) {
        return 0;
    }

    if (function->lineInSourceList != ZR_NULL && execInstructionIndex < function->instructionsLength) {
        bestLine = function->lineInSourceList[execInstructionIndex];
        if (bestLine > 0) {
            return bestLine;
        }
    }

    if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        for (TZrUInt32 index = 0; index < function->executionLocationInfoLength; index++) {
            const SZrFunctionExecutionLocationInfo *info = &function->executionLocationInfoList[index];
            if ((TZrUInt32)info->currentInstructionOffset > execInstructionIndex) {
                break;
            }
            bestLine = info->lineInSource;
        }
    }

    if (bestLine == 0) {
        bestLine = function->lineInSourceStart;
    }

    return bestLine;
}

TZrUInt32 backend_aot_exec_ir_debug_line_end_for_instruction(const SZrFunction *function,
                                                             TZrUInt32 execInstructionIndex,
                                                             TZrUInt32 debugLine) {
    TZrUInt32 bestLineEnd = 0;

    if (function == ZR_NULL) {
        return 0;
    }

    if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        for (TZrUInt32 index = 0; index < function->executionLocationInfoLength; index++) {
            const SZrFunctionExecutionLocationInfo *info = &function->executionLocationInfoList[index];
            if ((TZrUInt32)info->currentInstructionOffset > execInstructionIndex) {
                break;
            }
            bestLineEnd = info->lineInSourceEnd > 0 ? info->lineInSourceEnd : info->lineInSource;
        }
    }

    if (bestLineEnd == 0 && function->lineInSourceList != ZR_NULL &&
        execInstructionIndex < function->instructionsLength) {
        bestLineEnd = function->lineInSourceList[execInstructionIndex];
    }
    if (bestLineEnd == 0) {
        bestLineEnd = function->lineInSourceEnd > 0 ? function->lineInSourceEnd : function->lineInSourceStart;
    }
    if (bestLineEnd == 0) {
        bestLineEnd = debugLine;
    }
    if (debugLine > 0 && bestLineEnd < debugLine) {
        bestLineEnd = debugLine;
    }

    return bestLineEnd;
}

static const SZrFunctionExecutionLocationInfo *backend_aot_exec_ir_location_for_instruction(
        const SZrFunction *function,
        TZrUInt32 execInstructionIndex) {
    const SZrFunctionExecutionLocationInfo *bestInfo = ZR_NULL;

    if (function == ZR_NULL || function->executionLocationInfoList == ZR_NULL ||
        function->executionLocationInfoLength == 0) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->executionLocationInfoLength; index++) {
        const SZrFunctionExecutionLocationInfo *info = &function->executionLocationInfoList[index];
        if ((TZrUInt32)info->currentInstructionOffset > execInstructionIndex) {
            break;
        }
        bestInfo = info;
    }

    return bestInfo;
}

TZrUInt32 backend_aot_exec_ir_debug_column_for_instruction(const SZrFunction *function,
                                                           TZrUInt32 execInstructionIndex) {
    const SZrFunctionExecutionLocationInfo *info =
            backend_aot_exec_ir_location_for_instruction(function, execInstructionIndex);
    return info != ZR_NULL ? info->columnInSourceStart : 0u;
}

TZrUInt32 backend_aot_exec_ir_debug_column_end_for_instruction(const SZrFunction *function,
                                                               TZrUInt32 execInstructionIndex,
                                                               TZrUInt32 debugColumn) {
    const SZrFunctionExecutionLocationInfo *info =
            backend_aot_exec_ir_location_for_instruction(function, execInstructionIndex);
    TZrUInt32 debugColumnEnd = info != ZR_NULL ? info->columnInSourceEnd : 0u;

    if (debugColumnEnd == 0u) {
        debugColumnEnd = debugColumn;
    }
    if (debugColumn > 0u && debugColumnEnd < debugColumn) {
        debugColumnEnd = debugColumn;
    }

    return debugColumnEnd;
}
