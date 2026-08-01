#include "comptime_runtime_contract.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_core/string.h"

#define ZR_COMPTIME_CACHE_HASH_OFFSET ((TZrUInt64)14695981039346656037ULL)
#define ZR_COMPTIME_CACHE_HASH_PRIME ((TZrUInt64)1099511628211ULL)

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

static void comptime_cache_mix_bytes(
        TZrUInt64 *key,
        const void *data,
        TZrSize size) {
    const TZrByte *bytes = (const TZrByte *)data;

    for (TZrSize index = 0; key != ZR_NULL && index < size; index++) {
        *key ^= (TZrUInt64)bytes[index];
        *key *= ZR_COMPTIME_CACHE_HASH_PRIME;
    }
}

static void comptime_cache_mix_text(
        TZrUInt64 *key,
        const TZrChar *text) {
    TZrBool present = (TZrBool)(text != ZR_NULL);
    TZrUInt64 length = present ? (TZrUInt64)strlen(text) : 0U;

    comptime_cache_mix_bytes(key, &present, sizeof(present));
    comptime_cache_mix_bytes(key, &length, sizeof(length));
    if (present) {
        comptime_cache_mix_bytes(key, text, (TZrSize)length);
    }
}

static void comptime_cache_mix_compile_tool_bindings(
        TZrUInt64 *key,
        const SZrCompilerState *cs) {
    TZrUInt64 bindingCount;

    if (key == ZR_NULL || cs == ZR_NULL) {
        return;
    }
    bindingCount = (TZrUInt64)cs->compileToolBindings.length;
    comptime_cache_mix_bytes(key, &bindingCount, sizeof(bindingCount));
    for (TZrSize index = 0; index < cs->compileToolBindings.length; index++) {
        const SZrCompileToolBinding *binding =
                (const SZrCompileToolBinding *)ZrCore_Array_Get(
                        (SZrArray *)&cs->compileToolBindings, index);
        const SZrParserCompileToolModuleDescriptor *provider;

        if (binding == ZR_NULL) {
            continue;
        }
        comptime_cache_mix_bytes(
                key, &binding->kind, sizeof(binding->kind));
        comptime_cache_mix_text(
                key,
                binding->name != ZR_NULL
                        ? ZrCore_String_GetNativeString(binding->name)
                        : ZR_NULL);
        provider = binding->provider;
        if (provider == ZR_NULL) {
            continue;
        }
        comptime_cache_mix_text(key, provider->moduleName);
        comptime_cache_mix_bytes(
                key, &provider->providerPhase, sizeof(provider->providerPhase));
        comptime_cache_mix_text(key, provider->publicContractHash);
        comptime_cache_mix_bytes(
                key,
                &provider->computedPublicContractHash,
                sizeof(provider->computedPublicContractHash));
        comptime_cache_mix_text(key, binding->providerContentHash);
    }
}

TZrUInt64 ZrParser_ComptimeCache_BeginKey(
        const SZrCompilerState *cs,
        const SZrCompileTimeFunction *function) {
    TZrUInt64 key = ZR_COMPTIME_CACHE_HASH_OFFSET;
    const TZrChar *moduleName = ZR_NULL;
    const TZrChar *functionName = ZR_NULL;

    if (cs == ZR_NULL || function == ZR_NULL || function->isRuntimeProjection) {
        return 0U;
    }
    if (cs->currentModuleKey != ZR_NULL) {
        moduleName = ZrCore_String_GetNativeString(cs->currentModuleKey);
    }
    if (function->name != ZR_NULL) {
        functionName = ZrCore_String_GetNativeString(function->name);
    }
    comptime_cache_mix_text(&key, "zr.comptime.cache/v2");
    comptime_cache_mix_text(&key, moduleName);
    comptime_cache_mix_text(&key, functionName);
    comptime_cache_mix_compile_tool_bindings(&key, cs);
    comptime_cache_mix_bytes(
            &key, &cs->comptimeContext, sizeof(cs->comptimeContext));
    comptime_cache_mix_bytes(
            &key, &function->location.start, sizeof(function->location.start));
    comptime_cache_mix_bytes(
            &key, &function->location.end, sizeof(function->location.end));
    return key;
}

TZrBool ZrParser_ComptimeCache_MixValue(
        TZrUInt64 *key,
        const SZrTypeValue *value) {
    if (key == ZR_NULL || *key == 0U || value == ZR_NULL) {
        return ZR_FALSE;
    }

    comptime_cache_mix_bytes(key, &value->type, sizeof(value->type));
    if (ZR_VALUE_IS_TYPE_NULL(value->type)) {
        return ZR_TRUE;
    }
    if (value->type == ZR_VALUE_TYPE_STRING && value->value.object != ZR_NULL) {
        SZrString *stringValue = (SZrString *)value->value.object;
        comptime_cache_mix_text(
                key, ZrCore_String_GetNativeString(stringValue));
        return ZR_TRUE;
    }
    if (value->type == ZR_VALUE_TYPE_BOOL ||
        ZR_VALUE_IS_TYPE_INT(value->type) ||
        ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type) ||
        ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        comptime_cache_mix_bytes(
                key, &value->value.nativeObject, sizeof(value->value.nativeObject));
        return ZR_TRUE;
    }
    return ZR_FALSE;
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
        TZrUInt64 key,
        SZrTypeValue *result) {
    if (cs == ZR_NULL || key == 0U || result == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < cs->comptimeCache.length; index++) {
        const SZrComptimeCacheEntry *entry =
                (const SZrComptimeCacheEntry *)ZrCore_Array_Get(
                        &cs->comptimeCache, index);
        if (entry != ZR_NULL && entry->key == key) {
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
        TZrUInt64 key,
        const SZrTypeValue *value) {
    SZrComptimeCacheEntry entry;

    if (cs == ZR_NULL || key == 0U ||
        !comptime_cache_value_is_supported(value)) {
        return ZR_FALSE;
    }
    entry.key = key;
    entry.value = *value;
    ZrCore_Array_Push(cs->state, &cs->comptimeCache, &entry);
    return ZR_TRUE;
}
