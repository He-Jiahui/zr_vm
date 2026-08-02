#include "comptime_runtime_contract.h"
#include "zr_vm_parser/comptime_cache.h"

#include <stdio.h>
#include <stdlib.h>
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
    memset(cs->comptimeSourceDigest, 0, sizeof(cs->comptimeSourceDigest));
    cs->hasComptimeSourceDigest = ZR_FALSE;
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
            comptime_cache_mix_text(outKey, "zr.comptime.cache/v5") &&
            comptime_cache_mix_text(outKey, moduleName) &&
            comptime_cache_mix_text(outKey, functionName) &&
            comptime_cache_mix_u64(
                    outKey, cs->hasComptimeSourceDigest ? 1U : 0U) &&
            (!cs->hasComptimeSourceDigest ||
             comptime_cache_mix_bytes(
                     outKey,
                     cs->comptimeSourceDigest,
                     sizeof(cs->comptimeSourceDigest))) &&
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

static TZrBool comptime_cache_canonicalize_value(
        const SZrTypeValue *value,
        SZrTypeValue *outValue) {
    if (!comptime_cache_value_is_supported(value) || outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outValue, 0, sizeof(*outValue));
    outValue->type = value->type;
    outValue->isNative = ZR_TRUE;
    if (value->type == ZR_VALUE_TYPE_BOOL) {
        outValue->value.nativeObject.nativeBool =
                value->value.nativeObject.nativeBool != 0U;
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        outValue->value.nativeObject.nativeInt64 =
                value->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        outValue->value.nativeObject.nativeUInt64 =
                value->value.nativeObject.nativeUInt64;
    } else if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        outValue->value.nativeObject.nativeDouble =
                value->value.nativeObject.nativeDouble;
    }
    return ZR_TRUE;
}

static TZrBool comptime_cache_array_is_well_formed(
        const SZrArray *cache) {
    return cache != ZR_NULL && cache->isValid && cache->head != ZR_NULL &&
           cache->elementSize == sizeof(SZrComptimeCacheEntry) &&
           cache->capacity > 0U && cache->length <= cache->capacity;
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
    if (cs == ZR_NULL ||
        !comptime_cache_canonicalize_value(value, &entry.value) ||
        !comptime_cache_finish_key(key, entry.digest)) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < cs->comptimeCache.length; index++) {
        SZrComptimeCacheEntry *existing =
                (SZrComptimeCacheEntry *)ZrCore_Array_Get(
                        &cs->comptimeCache, index);
        if (existing != ZR_NULL &&
            memcmp(existing->digest,
                   entry.digest,
                   sizeof(entry.digest)) == 0) {
            *existing = entry;
            return ZR_TRUE;
        }
    }
    ZrCore_Array_Push(cs->state, &cs->comptimeCache, &entry);
    return ZR_TRUE;
}

#define ZR_COMPTIME_CACHE_SNAPSHOT_PREFIX_SIZE 16U
#define ZR_COMPTIME_CACHE_SNAPSHOT_HEADER_SIZE \
    (ZR_COMPTIME_CACHE_SNAPSHOT_PREFIX_SIZE + ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT)
#define ZR_COMPTIME_CACHE_SNAPSHOT_ENTRY_SIZE 44U

static const TZrByte g_comptime_cache_snapshot_magic[8] = {
        'Z', 'R', 'C', 'C', 'V', '0', '0', '5'};

static void comptime_cache_write_u32(TZrByte *bytes, TZrUInt32 value) {
    for (TZrSize index = 0U; index < 4U; index++) {
        bytes[3U - index] = (TZrByte)(value >> (index * 8U));
    }
}

static void comptime_cache_write_u64(TZrByte *bytes, TZrUInt64 value) {
    for (TZrSize index = 0U; index < 8U; index++) {
        bytes[7U - index] = (TZrByte)(value >> (index * 8U));
    }
}

static TZrUInt32 comptime_cache_read_u32(const TZrByte *bytes) {
    TZrUInt32 value = 0U;

    for (TZrSize index = 0U; index < 4U; index++) {
        value = (value << 8U) | bytes[index];
    }
    return value;
}

static TZrUInt64 comptime_cache_read_u64(const TZrByte *bytes) {
    TZrUInt64 value = 0U;

    for (TZrSize index = 0U; index < 8U; index++) {
        value = (value << 8U) | bytes[index];
    }
    return value;
}

static void comptime_cache_snapshot_digest(
        const TZrByte *bytes,
        TZrSize size,
        TZrByte digest[ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT]) {
    SZrParserSha256Context context;

    ZrParser_Sha256_Init(&context);
    ZrParser_Sha256_Update(
            &context, bytes, ZR_COMPTIME_CACHE_SNAPSHOT_PREFIX_SIZE);
    if (size > ZR_COMPTIME_CACHE_SNAPSHOT_HEADER_SIZE) {
        ZrParser_Sha256_Update(
                &context,
                bytes + ZR_COMPTIME_CACHE_SNAPSHOT_HEADER_SIZE,
                size - ZR_COMPTIME_CACHE_SNAPSHOT_HEADER_SIZE);
    }
    ZrParser_Sha256_Final(&context, digest);
}

static int comptime_cache_snapshot_entry_compare(
        const void *left,
        const void *right) {
    return memcmp(left,
                  right,
                  ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT);
}

static TZrUInt64 comptime_cache_snapshot_value_payload(
        const SZrTypeValue *value) {
    TZrUInt64 payload = 0U;

    if (value->type == ZR_VALUE_TYPE_BOOL) {
        return value->value.nativeObject.nativeBool != 0U ? 1U : 0U;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return (TZrUInt64)value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeUInt64;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        memcpy(&payload,
               &value->value.nativeObject.nativeDouble,
               sizeof(payload));
    }
    return payload;
}

static TZrBool comptime_cache_snapshot_size(
        TZrSize entryCount,
        TZrSize *outSize) {
    const TZrSize sizeMax = (TZrSize)-1;

    if (outSize == ZR_NULL ||
        entryCount >
                (sizeMax - ZR_COMPTIME_CACHE_SNAPSHOT_HEADER_SIZE) /
                        ZR_COMPTIME_CACHE_SNAPSHOT_ENTRY_SIZE) {
        return ZR_FALSE;
    }
    *outSize = ZR_COMPTIME_CACHE_SNAPSHOT_HEADER_SIZE +
               entryCount * ZR_COMPTIME_CACHE_SNAPSHOT_ENTRY_SIZE;
    return ZR_TRUE;
}

TZrBool ZrParser_ComptimeCache_ExportSnapshot(
        const SZrCompilerState *compiler,
        TZrByte **outBytes,
        TZrSize *outSize) {
    TZrByte *bytes;
    TZrSize size;

    if (outBytes == ZR_NULL || outSize == ZR_NULL) {
        return ZR_FALSE;
    }
    *outBytes = ZR_NULL;
    *outSize = 0U;
    if (compiler == ZR_NULL || compiler->state == ZR_NULL ||
        compiler->state->global == ZR_NULL ||
        !comptime_cache_array_is_well_formed(&compiler->comptimeCache) ||
        !comptime_cache_snapshot_size(compiler->comptimeCache.length, &size)) {
        return ZR_FALSE;
    }
    bytes = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            compiler->state->global,
            size,
            ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
    if (bytes == ZR_NULL) {
        return ZR_FALSE;
    }
    memcpy(bytes,
           g_comptime_cache_snapshot_magic,
           sizeof(g_comptime_cache_snapshot_magic));
    comptime_cache_write_u64(
            bytes + sizeof(g_comptime_cache_snapshot_magic),
            (TZrUInt64)compiler->comptimeCache.length);

    for (TZrSize index = 0U;
         index < compiler->comptimeCache.length;
         index++) {
        const SZrComptimeCacheEntry *entry =
                (const SZrComptimeCacheEntry *)ZrCore_Array_Get(
                        (SZrArray *)&compiler->comptimeCache, index);
        TZrByte *record =
                bytes + ZR_COMPTIME_CACHE_SNAPSHOT_HEADER_SIZE +
                index * ZR_COMPTIME_CACHE_SNAPSHOT_ENTRY_SIZE;

        if (entry == ZR_NULL ||
            !comptime_cache_value_is_supported(&entry->value)) {
            ZrCore_Memory_RawFreeWithType(
                    compiler->state->global,
                    bytes,
                    size,
                    ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
            return ZR_FALSE;
        }
        memcpy(record, entry->digest, sizeof(entry->digest));
        comptime_cache_write_u32(
                record + ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT,
                (TZrUInt32)entry->value.type);
        comptime_cache_write_u64(
                record + ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT + 4U,
                comptime_cache_snapshot_value_payload(&entry->value));
    }

    qsort(bytes + ZR_COMPTIME_CACHE_SNAPSHOT_HEADER_SIZE,
          compiler->comptimeCache.length,
          ZR_COMPTIME_CACHE_SNAPSHOT_ENTRY_SIZE,
          comptime_cache_snapshot_entry_compare);
    for (TZrSize index = 1U;
         index < compiler->comptimeCache.length;
         index++) {
        const TZrByte *previous =
                bytes + ZR_COMPTIME_CACHE_SNAPSHOT_HEADER_SIZE +
                (index - 1U) * ZR_COMPTIME_CACHE_SNAPSHOT_ENTRY_SIZE;
        const TZrByte *current = previous +
                                 ZR_COMPTIME_CACHE_SNAPSHOT_ENTRY_SIZE;

        if (memcmp(previous,
                   current,
                   ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT) == 0) {
            ZrCore_Memory_RawFreeWithType(
                    compiler->state->global,
                    bytes,
                    size,
                    ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
            return ZR_FALSE;
        }
    }
    comptime_cache_snapshot_digest(
            bytes,
            size,
            bytes + ZR_COMPTIME_CACHE_SNAPSHOT_PREFIX_SIZE);
    *outBytes = bytes;
    *outSize = size;
    return ZR_TRUE;
}

static TZrBool comptime_cache_snapshot_decode_value(
        const TZrByte *record,
        SZrTypeValue *outValue) {
    const TZrUInt32 rawType = comptime_cache_read_u32(
            record + ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT);
    const TZrUInt64 payload = comptime_cache_read_u64(
            record + ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT + 4U);
    SZrTypeValue value;

    if (rawType >= (TZrUInt32)ZR_VALUE_TYPE_ENUM_MAX) {
        return ZR_FALSE;
    }
    memset(&value, 0, sizeof(value));
    value.type = (EZrValueType)rawType;
    value.isNative = ZR_TRUE;
    if (!comptime_cache_value_is_supported(&value)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_NULL(value.type)) {
        if (payload != 0U) {
            return ZR_FALSE;
        }
    } else if (value.type == ZR_VALUE_TYPE_BOOL) {
        if (payload > 1U) {
            return ZR_FALSE;
        }
        value.value.nativeObject.nativeBool = payload != 0U;
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(value.type)) {
        value.value.nativeObject.nativeInt64 =
                payload <= 0x7FFFFFFFFFFFFFFFULL
                        ? (TZrInt64)payload
                        : -((TZrInt64)(~payload)) - 1;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value.type)) {
        value.value.nativeObject.nativeUInt64 = payload;
    } else if (ZR_VALUE_IS_TYPE_FLOAT(value.type)) {
        memcpy(&value.value.nativeObject.nativeDouble,
               &payload,
               sizeof(payload));
    }
    if (outValue != ZR_NULL) {
        *outValue = value;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ComptimeCache_ImportSnapshot(
        SZrCompilerState *compiler,
        const TZrByte *bytes,
        TZrSize size) {
    TZrUInt64 rawEntryCount;
    TZrSize entryCount;
    TZrSize expectedSize;
    TZrSize capacity;
    TZrSize allocationSize;
    SZrArray replacement;
    TZrByte integrityDigest[ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT];

    if (compiler == ZR_NULL || compiler->state == ZR_NULL ||
        compiler->state->global == ZR_NULL ||
        !comptime_cache_array_is_well_formed(&compiler->comptimeCache) ||
        bytes == ZR_NULL ||
        size < ZR_COMPTIME_CACHE_SNAPSHOT_HEADER_SIZE ||
        memcmp(bytes,
               g_comptime_cache_snapshot_magic,
               sizeof(g_comptime_cache_snapshot_magic)) != 0) {
        return ZR_FALSE;
    }
    rawEntryCount = comptime_cache_read_u64(
            bytes + sizeof(g_comptime_cache_snapshot_magic));
    entryCount = (TZrSize)rawEntryCount;
    if ((TZrUInt64)entryCount != rawEntryCount ||
        !comptime_cache_snapshot_size(entryCount, &expectedSize) ||
        expectedSize != size) {
        return ZR_FALSE;
    }
    comptime_cache_snapshot_digest(bytes, size, integrityDigest);
    if (memcmp(
                integrityDigest,
                bytes + ZR_COMPTIME_CACHE_SNAPSHOT_PREFIX_SIZE,
                sizeof(integrityDigest)) != 0) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0U; index < entryCount; index++) {
        const TZrByte *record =
                bytes + ZR_COMPTIME_CACHE_SNAPSHOT_HEADER_SIZE +
                index * ZR_COMPTIME_CACHE_SNAPSHOT_ENTRY_SIZE;

        if (!comptime_cache_snapshot_decode_value(record, ZR_NULL) ||
            (index > 0U &&
             memcmp(record - ZR_COMPTIME_CACHE_SNAPSHOT_ENTRY_SIZE,
                    record,
                    ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT) >= 0)) {
            return ZR_FALSE;
        }
    }

    capacity = entryCount > 0U ? entryCount : 1U;
    if (capacity > (TZrSize)-1 / sizeof(SZrComptimeCacheEntry)) {
        return ZR_FALSE;
    }
    allocationSize = capacity * sizeof(SZrComptimeCacheEntry);
    ZrCore_Array_Construct(&replacement);
    replacement.head = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            compiler->state->global,
            allocationSize,
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (replacement.head == ZR_NULL) {
        return ZR_FALSE;
    }
    replacement.elementSize = sizeof(SZrComptimeCacheEntry);
    replacement.length = entryCount;
    replacement.capacity = capacity;
    replacement.isValid = ZR_TRUE;
    for (TZrSize index = 0U; index < entryCount; index++) {
        const TZrByte *record =
                bytes + ZR_COMPTIME_CACHE_SNAPSHOT_HEADER_SIZE +
                index * ZR_COMPTIME_CACHE_SNAPSHOT_ENTRY_SIZE;
        SZrComptimeCacheEntry *entry =
                (SZrComptimeCacheEntry *)(replacement.head +
                        index * sizeof(SZrComptimeCacheEntry));

        memset(entry, 0, sizeof(*entry));
        memcpy(entry->digest, record, sizeof(entry->digest));
        if (!comptime_cache_snapshot_decode_value(
                    record, &entry->value)) {
            ZrCore_Array_Free(compiler->state, &replacement);
            return ZR_FALSE;
        }
    }

    ZrCore_Array_Free(compiler->state, &compiler->comptimeCache);
    compiler->comptimeCache = replacement;
    return ZR_TRUE;
}

void ZrParser_ComptimeCache_FreeSnapshot(
        SZrState *state,
        TZrByte *bytes,
        TZrSize size) {
    if (state == ZR_NULL || state->global == ZR_NULL || bytes == ZR_NULL) {
        return;
    }
    ZrCore_Memory_RawFreeWithType(
            state->global,
            bytes,
            size,
            ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
}
