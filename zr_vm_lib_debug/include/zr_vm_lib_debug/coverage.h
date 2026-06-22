#ifndef ZR_VM_DEBUG_COVERAGE_H
#define ZR_VM_DEBUG_COVERAGE_H

#include "zr_vm_lib_debug/conf.h"
#include "zr_vm_core/debug.h"

#define ZR_DEBUG_COVERAGE_NAME_CAPACITY ZR_DEBUG_NAME_CAPACITY
#define ZR_DEBUG_COVERAGE_SOURCE_CAPACITY ZR_DEBUG_TEXT_CAPACITY

typedef struct ZrDebugCoverageLine {
    const struct SZrFunction *function;
    TZrChar name[ZR_DEBUG_COVERAGE_NAME_CAPACITY];
    TZrChar source[ZR_DEBUG_COVERAGE_SOURCE_CAPACITY];
    TZrUInt32 line;
    TZrBool executable;
    TZrBool executed;
} ZrDebugCoverageLine;

typedef struct ZrDebugCoverage {
    struct SZrState *state;
    FZrDebugHook previous_hook;
    TZrUInt32 previous_mask;
    TZrUInt32 previous_count;
    TZrBool active;
    ZrDebugCoverageLine *lines;
    TZrSize line_count;
    TZrSize line_capacity;
    struct ZrDebugCoverage *next_active;
} ZrDebugCoverage;

ZR_DEBUG_API void ZrDebug_Coverage_Init(ZrDebugCoverage *coverage);
ZR_DEBUG_API void ZrDebug_Coverage_Reset(ZrDebugCoverage *coverage);
ZR_DEBUG_API TZrBool ZrDebug_Coverage_RegisterFunction(ZrDebugCoverage *coverage, const struct SZrFunction *function);
ZR_DEBUG_API TZrBool ZrDebug_Coverage_RegisterFunctionTree(ZrDebugCoverage *coverage,
                                                           const struct SZrFunction *function);
ZR_DEBUG_API TZrBool ZrDebug_Coverage_Start(ZrDebugCoverage *coverage, struct SZrState *state);
ZR_DEBUG_API void ZrDebug_Coverage_Stop(ZrDebugCoverage *coverage);
ZR_DEBUG_API void ZrDebug_Coverage_Destroy(ZrDebugCoverage *coverage);
ZR_DEBUG_API TZrSize ZrDebug_Coverage_GetLineCount(const ZrDebugCoverage *coverage);
ZR_DEBUG_API const ZrDebugCoverageLine *ZrDebug_Coverage_GetLine(const ZrDebugCoverage *coverage, TZrSize index);

#endif
