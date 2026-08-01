#ifndef ZR_VM_PARSER_COMPTIME_RUNTIME_CONTRACT_H
#define ZR_VM_PARSER_COMPTIME_RUNTIME_CONTRACT_H

#include "compile_tool_content_hash.h"
#include "compiler_internal.h"

typedef struct SZrComptimeCacheKey {
    SZrParserSha256Context sha256;
    TZrBool valid;
} SZrComptimeCacheKey;

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

ZR_PARSER_API TZrBool ZrParser_ComptimeCache_BeginKey(
        const SZrCompilerState *cs,
        const SZrCompileTimeFunction *function,
        SZrComptimeCacheKey *outKey);
TZrBool ZrParser_ComptimeCache_MixValue(
        SZrComptimeCacheKey *key,
        const SZrTypeValue *value);
ZR_PARSER_API TZrBool ZrParser_ComptimeCache_KeyEquals(
        const SZrComptimeCacheKey *lhs,
        const SZrComptimeCacheKey *rhs);
TZrBool ZrParser_ComptimeCache_Lookup(
        SZrCompilerState *cs,
        const SZrComptimeCacheKey *key,
        SZrTypeValue *result);
TZrBool ZrParser_ComptimeCache_Store(
        SZrCompilerState *cs,
        const SZrComptimeCacheKey *key,
        const SZrTypeValue *value);

#endif
