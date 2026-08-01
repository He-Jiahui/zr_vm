#include "comptime_runtime_contract.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_core/string.h"

static const SZrParserComptimeBudgetLimits g_default_limits = {
        .fuel = 1000000U,
        .callDepth = 128U,
        .heapBytes = 64U * 1024U * 1024U,
        .aggregateCount = 100000U,
        .generatedDeclarationCount = 10000U,
        .diagnosticCount = 1024U,
};

static const TZrChar *comptime_budget_resource_name(
        EZrParserComptimeBudgetResource resource) {
    switch (resource) {
        case ZR_PARSER_COMPTIME_BUDGET_FUEL:
            return "fuel";
        case ZR_PARSER_COMPTIME_BUDGET_CALL_DEPTH:
            return "call_depth";
        case ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES:
            return "heap_bytes";
        case ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT:
            return "aggregate_count";
        case ZR_PARSER_COMPTIME_BUDGET_GENERATED_DECLARATION_COUNT:
            return "generated_declaration_count";
        case ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT:
            return "diagnostic_count";
        case ZR_PARSER_COMPTIME_BUDGET_NONE:
        default:
            return "unknown";
    }
}

static void comptime_report_budget_exceeded(
        SZrCompilerState *cs,
        SZrFileRange location) {
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (cs == ZR_NULL) {
        return;
    }
    snprintf(message,
             sizeof(message),
             "comptime.budget_exceeded: %s requested=%llu limit=%llu",
             comptime_budget_resource_name(cs->comptimeBudget.exceededResource),
             (unsigned long long)cs->comptimeBudget.requestedUsage,
             (unsigned long long)cs->comptimeBudget.exceededLimit);
    ZrParser_CompileTime_Error(
            cs, ZR_COMPILE_TIME_ERROR_ERROR, message, location);
}

void ZrParser_ComptimeRuntime_Init(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return;
    }
    ZrParser_ComptimeBudget_Init(&cs->comptimeBudget, &g_default_limits);
    cs->comptimeContext = ZR_PARSER_COMPTIME_CONTEXT_CHECK;
    cs->comptimeCallDepth = 0U;
    cs->comptimeCacheHitCount = 0U;
    cs->comptimeCacheMissCount = 0U;
    ZrCore_Array_Init(
            cs->state,
            &cs->comptimeCache,
            sizeof(SZrComptimeCacheEntry),
            ZR_PARSER_INITIAL_CAPACITY_SMALL);
}

void ZrParser_ComptimeRuntime_Free(SZrCompilerState *cs) {
    if (cs != ZR_NULL && cs->state != ZR_NULL &&
        cs->comptimeCache.isValid && cs->comptimeCache.head != ZR_NULL &&
        cs->comptimeCache.capacity > 0 && cs->comptimeCache.elementSize > 0) {
        ZrCore_Array_Free(cs->state, &cs->comptimeCache);
    }
}

TZrBool ZrParser_ComptimeRuntime_Consume(
        SZrCompilerState *cs,
        EZrParserComptimeBudgetResource resource,
        TZrUInt64 amount,
        SZrFileRange location) {
    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrParser_ComptimeBudget_TryConsume(
                &cs->comptimeBudget, resource, amount)) {
        return ZR_TRUE;
    }
    comptime_report_budget_exceeded(cs, location);
    return ZR_FALSE;
}

TZrBool ZrParser_ComptimeRuntime_EnterCall(
        SZrCompilerState *cs,
        SZrFileRange location) {
    TZrUInt64 requested;

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    requested = (TZrUInt64)cs->comptimeCallDepth + 1U;
    if (requested > cs->comptimeBudget.limits.callDepth) {
        cs->comptimeBudget.exceededResource =
                ZR_PARSER_COMPTIME_BUDGET_CALL_DEPTH;
        cs->comptimeBudget.exceededLimit =
                cs->comptimeBudget.limits.callDepth;
        cs->comptimeBudget.requestedUsage = requested;
        comptime_report_budget_exceeded(cs, location);
        return ZR_FALSE;
    }
    cs->comptimeCallDepth++;
    if (requested > cs->comptimeBudget.usage.callDepth) {
        cs->comptimeBudget.usage.callDepth = requested;
    }
    return ZR_TRUE;
}

void ZrParser_ComptimeRuntime_LeaveCall(SZrCompilerState *cs) {
    if (cs != ZR_NULL && cs->comptimeCallDepth > 0U) {
        cs->comptimeCallDepth--;
    }
}

TZrBool ZrParser_ComptimeRuntime_RequireEffect(
        SZrCompilerState *cs,
        EZrParserCompileToolEffect effect,
        SZrFileRange location) {
    if (cs != ZR_NULL && ZrParser_ComptimeEffect_IsAllowed(
                                 cs->comptimeContext, effect)) {
        return ZR_TRUE;
    }
    if (cs != ZR_NULL) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "comptime.effect_violation: requested effect is not allowed in this evaluation context",
                location);
    }
    return ZR_FALSE;
}

static TZrBool comptime_cache_mix_bytes(
        SZrComptimeCacheKey *key,
        const void *data,
        TZrSize size) {
    if (key == ZR_NULL || !key->valid ||
        (data == ZR_NULL && size != 0U) ||
        !ZrParser_Sha256_Update(
                &key->sha256, (const TZrByte *)data, size)) {
        if (key != ZR_NULL) {
            key->valid = ZR_FALSE;
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool comptime_cache_mix_u64(
        SZrComptimeCacheKey *key,
        TZrUInt64 value) {
    TZrByte bytes[8];

    for (TZrSize index = 0U; index < sizeof(bytes); index++) {
        bytes[sizeof(bytes) - 1U - index] =
                (TZrByte)(value >> (index * 8U));
    }
    return comptime_cache_mix_bytes(key, bytes, sizeof(bytes));
}

static TZrBool comptime_cache_mix_text(
        SZrComptimeCacheKey *key,
        const TZrChar *text) {
    TZrUInt64 length = text != ZR_NULL ? (TZrUInt64)strlen(text) : 0U;

    if (!comptime_cache_mix_u64(key, text != ZR_NULL ? 1U : 0U) ||
        !comptime_cache_mix_u64(key, length)) {
        return ZR_FALSE;
    }
    return text == ZR_NULL ||
           comptime_cache_mix_bytes(key, text, (TZrSize)length);
}

static TZrBool comptime_cache_mix_compile_tool_bindings(
        SZrComptimeCacheKey *key,
        const SZrCompilerState *cs) {
    if (key == ZR_NULL || cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!comptime_cache_mix_u64(
                key, (TZrUInt64)cs->compileToolBindings.length)) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < cs->compileToolBindings.length; index++) {
        const SZrCompileToolBinding *binding =
                (const SZrCompileToolBinding *)ZrCore_Array_Get(
                        (SZrArray *)&cs->compileToolBindings, index);
        const SZrParserCompileToolModuleDescriptor *provider;

        if (binding == ZR_NULL) {
            return ZR_FALSE;
        }
        if (!comptime_cache_mix_u64(key, (TZrUInt64)binding->kind) ||
            !comptime_cache_mix_text(
                    key,
                    binding->name != ZR_NULL
                            ? ZrCore_String_GetNativeString(binding->name)
                            : ZR_NULL)) {
            return ZR_FALSE;
        }
        provider = binding->provider;
        if (!comptime_cache_mix_u64(
                    key, provider != ZR_NULL ? 1U : 0U)) {
            return ZR_FALSE;
        }
        if (provider != ZR_NULL &&
            (!comptime_cache_mix_text(key, provider->moduleName) ||
             !comptime_cache_mix_u64(
                     key, (TZrUInt64)provider->providerPhase) ||
             !comptime_cache_mix_text(key, provider->publicContractHash) ||
             !comptime_cache_mix_u64(
                     key, provider->computedPublicContractHash) ||
             !comptime_cache_mix_text(
                     key, binding->providerContentHash))) {
            return ZR_FALSE;
        }
        if (!comptime_cache_mix_u64(
                    key,
                    binding->resolvedArtifact != ZR_NULL ? 1U : 0U)) {
            return ZR_FALSE;
        }
        if (binding->resolvedArtifact != ZR_NULL) {
            const SZrParserCompileToolResolvedArtifact *artifact =
                    binding->resolvedArtifact;

            if (!comptime_cache_mix_u64(
                        key, (TZrUInt64)artifact->moduleIdentity.domain) ||
                !comptime_cache_mix_text(
                        key, artifact->moduleIdentity.packageName) ||
                !comptime_cache_mix_text(
                        key, artifact->moduleIdentity.segments) ||
                !comptime_cache_mix_u64(
                        key, (TZrUInt64)artifact->providerSourceKind) ||
                !comptime_cache_mix_text(key, artifact->resolvedVersion) ||
                !comptime_cache_mix_text(
                        key, artifact->packageContentHash) ||
                !comptime_cache_mix_text(key, artifact->lockGraphHash) ||
                !comptime_cache_mix_text(key, artifact->artifactEntry) ||
                !comptime_cache_mix_text(
                        key, artifact->artifactContentHash) ||
                !comptime_cache_mix_text(
                        key, artifact->publicContractHash)) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ComptimeCache_BeginKey(
        const SZrCompilerState *cs,
        const SZrCompileTimeFunction *function,
        SZrComptimeCacheKey *outKey) {
    const TZrChar *moduleName = ZR_NULL;
    const TZrChar *functionName = ZR_NULL;
    const TZrChar *sourceName = ZR_NULL;

    if (outKey == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outKey, 0, sizeof(*outKey));
    if (cs == ZR_NULL || function == ZR_NULL || function->isRuntimeProjection) {
        return ZR_FALSE;
    }
    ZrParser_Sha256_Init(&outKey->sha256);
    outKey->valid = ZR_TRUE;
    if (cs->currentModuleKey != ZR_NULL) {
        moduleName = ZrCore_String_GetNativeString(cs->currentModuleKey);
    }
    if (function->name != ZR_NULL) {
        functionName = ZrCore_String_GetNativeString(function->name);
    }
    if (function->location.source != ZR_NULL) {
        sourceName = ZrCore_String_GetNativeString(function->location.source);
    }
    return (TZrBool)(
            comptime_cache_mix_text(outKey, "zr.comptime.cache/v4") &&
            comptime_cache_mix_text(outKey, moduleName) &&
            comptime_cache_mix_text(outKey, functionName) &&
            comptime_cache_mix_compile_tool_bindings(outKey, cs) &&
            comptime_cache_mix_u64(
                    outKey, (TZrUInt64)cs->comptimeContext) &&
            comptime_cache_mix_u64(
                    outKey, (TZrUInt64)function->location.start.offset) &&
            comptime_cache_mix_u64(
                    outKey, (TZrUInt64)(TZrInt64)function->location.start.line) &&
            comptime_cache_mix_u64(
                    outKey, (TZrUInt64)(TZrInt64)function->location.start.column) &&
            comptime_cache_mix_u64(
                    outKey, (TZrUInt64)function->location.end.offset) &&
            comptime_cache_mix_u64(
                    outKey, (TZrUInt64)(TZrInt64)function->location.end.line) &&
            comptime_cache_mix_u64(
                    outKey, (TZrUInt64)(TZrInt64)function->location.end.column) &&
            comptime_cache_mix_text(outKey, sourceName));
}

TZrBool ZrParser_ComptimeCache_MixValue(
        SZrComptimeCacheKey *key,
        const SZrTypeValue *value) {
    TZrUInt64 bits;

    if (key == ZR_NULL || !key->valid || value == ZR_NULL ||
        !comptime_cache_mix_u64(key, (TZrUInt64)value->type)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_NULL(value->type)) {
        return ZR_TRUE;
    }
    if (value->type == ZR_VALUE_TYPE_STRING && value->value.object != ZR_NULL) {
        SZrString *stringValue = (SZrString *)value->value.object;
        comptime_cache_mix_text(
                key, ZrCore_String_GetNativeString(stringValue));
        return ZR_TRUE;
    }
    if (value->type == ZR_VALUE_TYPE_BOOL) {
        return comptime_cache_mix_u64(
                key, value->value.nativeObject.nativeBool != 0U ? 1U : 0U);
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return comptime_cache_mix_u64(
                key, (TZrUInt64)value->value.nativeObject.nativeInt64);
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return comptime_cache_mix_u64(
                key, value->value.nativeObject.nativeUInt64);
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        memcpy(&bits, &value->value.nativeObject.nativeDouble, sizeof(bits));
        return comptime_cache_mix_u64(key, bits);
    }
    key->valid = ZR_FALSE;
    return ZR_FALSE;
}

static TZrBool comptime_cache_finish_key(
        const SZrComptimeCacheKey *key,
        TZrByte digest[ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT]) {
    SZrParserSha256Context context;

    if (key == ZR_NULL || !key->valid || digest == ZR_NULL) {
        return ZR_FALSE;
    }
    context = key->sha256;
    ZrParser_Sha256_Final(&context, digest);
    return ZR_TRUE;
}

TZrBool ZrParser_ComptimeCache_KeyEquals(
        const SZrComptimeCacheKey *lhs,
        const SZrComptimeCacheKey *rhs) {
    TZrByte lhsDigest[ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT];
    TZrByte rhsDigest[ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT];

    return (TZrBool)(comptime_cache_finish_key(lhs, lhsDigest) &&
                     comptime_cache_finish_key(rhs, rhsDigest) &&
                     memcmp(lhsDigest, rhsDigest, sizeof(lhsDigest)) == 0);
}

static TZrBool comptime_cache_value_is_supported(
        const SZrTypeValue *value) {
    return value != ZR_NULL &&
           (ZR_VALUE_IS_TYPE_NULL(value->type) ||
            value->type == ZR_VALUE_TYPE_BOOL ||
            ZR_VALUE_IS_TYPE_INT(value->type) ||
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type) ||
            ZR_VALUE_IS_TYPE_FLOAT(value->type));
}

TZrBool ZrParser_ComptimeCache_Lookup(
        SZrCompilerState *cs,
        const SZrComptimeCacheKey *key,
        SZrTypeValue *result) {
    TZrByte digest[ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT];

    if (cs == ZR_NULL || result == ZR_NULL ||
        !comptime_cache_finish_key(key, digest)) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < cs->comptimeCache.length; index++) {
        const SZrComptimeCacheEntry *entry =
                (const SZrComptimeCacheEntry *)ZrCore_Array_Get(
                        &cs->comptimeCache, index);
        if (entry != ZR_NULL &&
            memcmp(entry->digest, digest, sizeof(digest)) == 0) {
            *result = entry->value;
            cs->comptimeCacheHitCount++;
            return ZR_TRUE;
        }
    }
    cs->comptimeCacheMissCount++;
    return ZR_FALSE;
}

TZrBool ZrParser_ComptimeCache_Store(
        SZrCompilerState *cs,
        const SZrComptimeCacheKey *key,
        const SZrTypeValue *value) {
    SZrComptimeCacheEntry entry;

    memset(&entry, 0, sizeof(entry));
    if (cs == ZR_NULL || !comptime_cache_value_is_supported(value) ||
        !comptime_cache_finish_key(key, entry.digest)) {
        return ZR_FALSE;
    }
    entry.value = *value;
    ZrCore_Array_Push(cs->state, &cs->comptimeCache, &entry);
    return ZR_TRUE;
}
