#include "zr_vm_parser/compile_ir_cache.h"

#include "compile_tool_content_hash.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct SZrCompileIrCacheEntry {
    SZrCompileIrCacheKey key;
    TZrByte *payload;
    TZrSize payloadSize;
    TZrByte payloadDigest[ZR_COMPILE_IR_CACHE_DIGEST_SIZE];
    TZrBool accepted;
};

static void cache_diagnostic_clear(SZrCompileIrCacheDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        (void)memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static TZrBool cache_diagnostic_set(
        SZrCompileIrCacheDiagnostic *diagnostic,
        EZrCompileIrCacheDiagnosticCode code,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return ZR_FALSE;
}

static TZrBool cache_key_valid(const SZrCompileIrCacheKey *key) {
    return (TZrBool)(key != ZR_NULL && key->valid &&
                     key->schemaVersion == ZR_COMPILE_IR_CACHE_SCHEMA_VERSION);
}

static TZrBool cache_hash_u64(SZrParserSha256Context *context,
                              TZrUInt64 value) {
    TZrByte bytes[8];
    TZrUInt32 index;
    for (index = 0U; index < 8U; ++index) {
        bytes[index] = (TZrByte)((value >> (index * 8U)) & 0xffU);
    }
    return ZrParser_Sha256_Update(context, bytes, sizeof(bytes));
}

static TZrBool cache_hash_bytes(SZrParserSha256Context *context,
                                const TZrByte *bytes,
                                TZrSize size) {
    if (bytes == ZR_NULL && size != 0U) {
        return ZR_FALSE;
    }
    return cache_hash_u64(context, (TZrUInt64)size) &&
           ZrParser_Sha256_Update(context, bytes, size);
}

static TZrBool cache_payload_digest(const TZrByte *payload,
                                    TZrSize payloadSize,
                                    TZrByte digest[ZR_COMPILE_IR_CACHE_DIGEST_SIZE]) {
    SZrParserSha256Context context;
    if (payload == ZR_NULL || payloadSize == 0U || digest == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrParser_Sha256_Init(&context);
    if (!ZrParser_Sha256_Update(&context, payload, payloadSize)) {
        return ZR_FALSE;
    }
    ZrParser_Sha256_Final(&context, digest);
    return ZR_TRUE;
}

static TZrBool cache_profile_valid(const SZrCompileEffectivePolicy *profile) {
    return (TZrBool)(profile != ZR_NULL &&
                     profile->schemaVersion ==
                             ZR_COMPILE_OPTIMIZATION_PROFILE_SCHEMA_VERSION &&
                     profile->profileHash ==
                             ZrParser_CompileOptimizationProfile_Hash(profile));
}

static SZrCompileIrCacheEntry *cache_find(
        SZrCompileIrCache *cache,
        const SZrCompileIrCacheKey *key) {
    TZrSize index;
    if (cache == ZR_NULL || key == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < cache->count; ++index) {
        if (ZrParser_CompileIrCache_KeyEquals(&cache->entries[index].key, key)) {
            return &cache->entries[index];
        }
    }
    return ZR_NULL;
}

static const SZrCompileIrCacheEntry *cache_find_const(
        const SZrCompileIrCache *cache,
        const SZrCompileIrCacheKey *key) {
    TZrSize index;
    if (cache == ZR_NULL || key == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < cache->count; ++index) {
        if (ZrParser_CompileIrCache_KeyEquals(&cache->entries[index].key, key)) {
            return &cache->entries[index];
        }
    }
    return ZR_NULL;
}

static TZrBool cache_reserve(SZrCompileIrCache *cache, TZrSize required) {
    SZrCompileIrCacheEntry *entries;
    TZrSize capacity;
    if (required <= cache->capacity) {
        return ZR_TRUE;
    }
    capacity = cache->capacity == 0U ? 4U : cache->capacity;
    while (capacity < required) {
        if (capacity > ((TZrSize)-1) / 2U) {
            capacity = required;
            break;
        }
        capacity *= 2U;
    }
    if (capacity > ((TZrSize)-1) / sizeof(*entries)) {
        return ZR_FALSE;
    }
    entries = (SZrCompileIrCacheEntry *)realloc(
            cache->entries, capacity * sizeof(*entries));
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }
    cache->entries = entries;
    cache->capacity = capacity;
    return ZR_TRUE;
}

TZrBool ZrParser_CompileIrCache_BuildKey(
        const SZrCompileEffectivePolicy *profile,
        const SZrCompileIrCacheInputs *inputs,
        SZrCompileIrCacheKey *key,
        SZrCompileIrCacheDiagnostic *diagnostic) {
    SZrParserSha256Context context;
    TZrByte domain[] = "zr.compile.ir-cache/v1";

    cache_diagnostic_clear(diagnostic);
    if (key == ZR_NULL || !cache_profile_valid(profile) || inputs == ZR_NULL) {
        return cache_diagnostic_set(
                diagnostic,
                profile == ZR_NULL || inputs == ZR_NULL
                        ? ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_ARGUMENT
                        : ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_KEY,
                1U,
                0U);
    }
    (void)memset(key, 0, sizeof(*key));
    if ((inputs->sourceBytes == ZR_NULL && inputs->sourceSize != 0U) ||
        (inputs->dependencyBytes == ZR_NULL && inputs->dependencySize != 0U)) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_ARGUMENT,
                0U,
                1U);
    }
    key->schemaVersion = ZR_COMPILE_IR_CACHE_SCHEMA_VERSION;
    key->profileHash = profile->profileHash;
    key->contractHash = inputs->contractHash;
    key->compilerAbi = inputs->compilerAbi;
    key->passPipelineHash = inputs->passPipelineHash;
    key->targetHash = inputs->targetHash;
    key->numericPolicyHash = inputs->numericPolicyHash;
    key->layoutHash = inputs->layoutHash;
    key->importedProfileHash = inputs->importedProfileHash;

    ZrParser_Sha256_Init(&context);
    if (!ZrParser_Sha256_Update(&context, domain, sizeof(domain) - 1U) ||
        !cache_hash_bytes(&context, inputs->sourceBytes, inputs->sourceSize) ||
        !cache_hash_bytes(&context, inputs->dependencyBytes,
                          inputs->dependencySize)) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_ARGUMENT,
                0U,
                1U);
    }
    if (!cache_hash_u64(&context, key->profileHash) ||
        !cache_hash_u64(&context, key->contractHash) ||
        !cache_hash_u64(&context, key->compilerAbi) ||
        !cache_hash_u64(&context, key->passPipelineHash) ||
        !cache_hash_u64(&context, key->targetHash) ||
        !cache_hash_u64(&context, key->numericPolicyHash) ||
        !cache_hash_u64(&context, key->layoutHash) ||
        !cache_hash_u64(&context, key->importedProfileHash)) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_ARGUMENT,
                0U,
                1U);
    }
    ZrParser_Sha256_Final(&context, key->digest);
    key->valid = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrParser_CompileIrCache_KeyEquals(
        const SZrCompileIrCacheKey *left,
        const SZrCompileIrCacheKey *right) {
    if (!cache_key_valid(left) || !cache_key_valid(right)) {
        return ZR_FALSE;
    }
    return (TZrBool)(left->schemaVersion == right->schemaVersion &&
                     left->profileHash == right->profileHash &&
                     left->contractHash == right->contractHash &&
                     left->compilerAbi == right->compilerAbi &&
                     left->passPipelineHash == right->passPipelineHash &&
                     left->targetHash == right->targetHash &&
                     left->numericPolicyHash == right->numericPolicyHash &&
                     left->layoutHash == right->layoutHash &&
                     left->importedProfileHash == right->importedProfileHash &&
                     memcmp(left->digest, right->digest,
                            ZR_COMPILE_IR_CACHE_DIGEST_SIZE) == 0);
}

void ZrParser_CompileIrCache_Init(SZrCompileIrCache *cache) {
    if (cache == ZR_NULL) {
        return;
    }
    (void)memset(cache, 0, sizeof(*cache));
    cache->nextWriterToken = 1U;
}

void ZrParser_CompileIrCache_Free(SZrCompileIrCache *cache) {
    TZrSize index;
    if (cache == ZR_NULL) {
        return;
    }
    for (index = 0U; index < cache->count; ++index) {
        free(cache->entries[index].payload);
    }
    free(cache->entries);
    (void)memset(cache, 0, sizeof(*cache));
}

void ZrParser_CompileIrCache_TransactionInit(
        SZrCompileIrCacheTransaction *transaction) {
    if (transaction != ZR_NULL) {
        (void)memset(transaction, 0, sizeof(*transaction));
    }
}

TZrBool ZrParser_CompileIrCache_BeginWrite(
        SZrCompileIrCache *cache,
        const SZrCompileIrCacheKey *key,
        SZrCompileIrCacheTransaction *transaction,
        SZrCompileIrCacheDiagnostic *diagnostic) {
    cache_diagnostic_clear(diagnostic);
    if (cache == ZR_NULL || transaction == ZR_NULL) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_ARGUMENT,
                1U,
                0U);
    }
    if (!cache_key_valid(key)) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_KEY,
                ZR_COMPILE_IR_CACHE_SCHEMA_VERSION,
                key == ZR_NULL ? 0U : key->schemaVersion);
    }
    if (transaction->active) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_ACTIVE_WRITER,
                0U,
                transaction->writerToken);
    }
    (void)memset(transaction, 0, sizeof(*transaction));
    transaction->cache = cache;
    transaction->key = *key;
    transaction->writerToken = cache->nextWriterToken++;
    if (transaction->writerToken == 0U) {
        transaction->writerToken = cache->nextWriterToken++;
    }
    if (cache->activeWriters == UINT32_MAX) {
        transaction->cache = ZR_NULL;
        transaction->writerToken = 0U;
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_ACTIVE_WRITER,
                UINT32_MAX,
                cache->activeWriters);
    }
    cache->activeWriters++;
    transaction->active = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrParser_CompileIrCache_Publish(
        SZrCompileIrCache *cache,
        SZrCompileIrCacheTransaction *transaction,
        const TZrByte *payload,
        TZrSize payloadSize,
        TZrBool complete,
        SZrCompileIrCacheDiagnostic *diagnostic) {
    SZrCompileIrCacheEntry *entry;
    TZrByte *copy;
    TZrByte payloadDigest[ZR_COMPILE_IR_CACHE_DIGEST_SIZE];

    cache_diagnostic_clear(diagnostic);
    if (cache == ZR_NULL || transaction == ZR_NULL ||
        transaction->cache != cache || !transaction->active) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_ARGUMENT,
                1U,
                0U);
    }
    if (!complete || payload == ZR_NULL || payloadSize == 0U) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INCOMPLETE_PAYLOAD,
                1U,
                complete ? payloadSize : 0U);
    }
    if (!cache_payload_digest(payload, payloadSize, payloadDigest)) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_ARGUMENT,
                payloadSize,
                0U);
    }
    copy = (TZrByte *)malloc(payloadSize);
    if (copy == ZR_NULL) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_ALLOCATION,
                payloadSize,
                0U);
    }
    (void)memcpy(copy, payload, payloadSize);
    entry = cache_find(cache, &transaction->key);
    if (entry == ZR_NULL) {
        if (cache->count == (TZrSize)-1 ||
            !cache_reserve(cache, cache->count + 1U)) {
            free(copy);
            return cache_diagnostic_set(
                    diagnostic,
                    ZR_COMPILE_IR_CACHE_DIAGNOSTIC_ALLOCATION,
                    cache->count + 1U,
                    cache->capacity);
        }
        entry = &cache->entries[cache->count++];
        (void)memset(entry, 0, sizeof(*entry));
        entry->key = transaction->key;
    }
    free(entry->payload);
    entry->payload = copy;
    entry->payloadSize = payloadSize;
    (void)memcpy(entry->payloadDigest, payloadDigest, sizeof(payloadDigest));
    entry->accepted = ZR_TRUE;
    if (cache->activeWriters > 0U) {
        cache->activeWriters--;
    }
    transaction->active = ZR_FALSE;
    transaction->cache = ZR_NULL;
    return ZR_TRUE;
}

TZrBool ZrParser_CompileIrCache_CancelWrite(
        SZrCompileIrCache *cache,
        SZrCompileIrCacheTransaction *transaction,
        SZrCompileIrCacheDiagnostic *diagnostic) {
    cache_diagnostic_clear(diagnostic);
    if (cache == ZR_NULL || transaction == ZR_NULL ||
        transaction->cache != cache || !transaction->active) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_ARGUMENT,
                1U,
                0U);
    }
    transaction->active = ZR_FALSE;
    if (cache->activeWriters > 0U) {
        cache->activeWriters--;
    }
    transaction->cache = ZR_NULL;
    return ZR_TRUE;
}

TZrBool ZrParser_CompileIrCache_Lookup(
        const SZrCompileIrCache *cache,
        const SZrCompileIrCacheKey *key,
        SZrCompileIrCacheHit *hit,
        SZrCompileIrCacheDiagnostic *diagnostic) {
    const SZrCompileIrCacheEntry *entry;

    cache_diagnostic_clear(diagnostic);
    if (hit != ZR_NULL) {
        (void)memset(hit, 0, sizeof(*hit));
    }
    if (cache == ZR_NULL || hit == ZR_NULL) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_ARGUMENT,
                1U,
                0U);
    }
    if (!cache_key_valid(key)) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_KEY,
                ZR_COMPILE_IR_CACHE_SCHEMA_VERSION,
                key == ZR_NULL ? 0U : key->schemaVersion);
    }
    entry = cache_find_const(cache, key);
    if (entry == ZR_NULL || !entry->accepted || entry->payload == ZR_NULL ||
        entry->payloadSize == 0U) {
        return cache_diagnostic_set(
                diagnostic,
                entry == ZR_NULL ? ZR_COMPILE_IR_CACHE_DIAGNOSTIC_NOT_FOUND
                                 : ZR_COMPILE_IR_CACHE_DIAGNOSTIC_CORRUPT,
                1U,
                entry == ZR_NULL ? 0U : entry->payloadSize);
    }
    {
        TZrByte payloadDigest[ZR_COMPILE_IR_CACHE_DIGEST_SIZE];
        if (!cache_payload_digest(entry->payload, entry->payloadSize,
                                  payloadDigest) ||
            memcmp(payloadDigest, entry->payloadDigest,
                   sizeof(payloadDigest)) != 0) {
            return cache_diagnostic_set(
                    diagnostic,
                    ZR_COMPILE_IR_CACHE_DIAGNOSTIC_CORRUPT,
                    1U,
                    0U);
        }
    }
    hit->payload = entry->payload;
    hit->payloadSize = entry->payloadSize;
    hit->accepted = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrParser_CompileIrCache_MarkCorrupt(
        SZrCompileIrCache *cache,
        const SZrCompileIrCacheKey *key,
        SZrCompileIrCacheDiagnostic *diagnostic) {
    SZrCompileIrCacheEntry *entry;
    TZrSize index;

    cache_diagnostic_clear(diagnostic);
    if (cache == ZR_NULL || !cache_key_valid(key)) {
        return cache_diagnostic_set(
                diagnostic,
                cache == ZR_NULL ? ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_ARGUMENT
                                 : ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_KEY,
                ZR_COMPILE_IR_CACHE_SCHEMA_VERSION,
                key == ZR_NULL ? 0U : key->schemaVersion);
    }
    entry = cache_find(cache, key);
    if (entry == ZR_NULL) {
        return cache_diagnostic_set(
                diagnostic,
                ZR_COMPILE_IR_CACHE_DIAGNOSTIC_NOT_FOUND,
                1U,
                0U);
    }
    free(entry->payload);
    index = (TZrSize)(entry - cache->entries);
    if (index + 1U < cache->count) {
        cache->entries[index] = cache->entries[cache->count - 1U];
    }
    cache->count--;
    return ZR_TRUE;
}

TZrSize ZrParser_CompileIrCache_Count(const SZrCompileIrCache *cache) {
    return cache == ZR_NULL ? 0U : cache->count;
}
