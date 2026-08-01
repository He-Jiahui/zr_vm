#ifndef ZR_VM_PARSER_COMPTIME_RUNTIME_CONTRACT_H
#define ZR_VM_PARSER_COMPTIME_RUNTIME_CONTRACT_H

#include "compiler_internal.h"

void ZrParser_ComptimeRuntime_Init(SZrCompilerState *cs);
void ZrParser_ComptimeRuntime_Free(SZrCompilerState *cs);
TZrBool ZrParser_ComptimeRuntime_Consume(
        SZrCompilerState *cs,
        EZrParserComptimeBudgetResource resource,
        TZrUInt64 amount,
        SZrFileRange location);
TZrBool ZrParser_ComptimeRuntime_EnterCall(
        SZrCompilerState *cs,
        SZrFileRange location);
void ZrParser_ComptimeRuntime_LeaveCall(SZrCompilerState *cs);
TZrBool ZrParser_ComptimeRuntime_RequireEffect(
        SZrCompilerState *cs,
        EZrParserCompileToolEffect effect,
        SZrFileRange location);

ZR_PARSER_API TZrUInt64 ZrParser_ComptimeCache_BeginKey(
        const SZrCompilerState *cs,
        const SZrCompileTimeFunction *function);
TZrBool ZrParser_ComptimeCache_MixValue(
        TZrUInt64 *key,
        const SZrTypeValue *value);
TZrBool ZrParser_ComptimeCache_Lookup(
        SZrCompilerState *cs,
        TZrUInt64 key,
        SZrTypeValue *result);
TZrBool ZrParser_ComptimeCache_Store(
        SZrCompilerState *cs,
        TZrUInt64 key,
        const SZrTypeValue *value);

#endif
