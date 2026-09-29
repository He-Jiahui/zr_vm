#include "zr_vm_parser/compile_ir_cache.h"

#include "compile_tool_content_hash.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 本模块把可复现的编译输入键与经摘要校验的内存载荷绑定，并通过显式事务发布。 */

/* 缓存键记录可复现构建事实；缓存项拥有载荷副本，命中结果则借用该副本。 */
struct SZrCompileIrCacheEntry {
    SZrCompileIrCacheKey key;
    TZrByte *payload;
    TZrSize payloadSize;
    TZrByte payloadDigest[ZR_COMPILE_IR_CACHE_DIGEST_SIZE];
    TZrBool accepted;
};

/* 清空可选诊断输出，避免成功或空诊断路径遗留上次调用的错误。 */
static void cache_diagnostic_clear(SZrCompileIrCacheDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        (void)memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

/* 各拒绝路径共用 bool 失败和可选诊断，让调用方区分输入、分配、未命中与损坏等原因。 */
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

/* 只接受显式有效且匹配当前 schema 的键，供查找入口共用。 */
static TZrBool cache_key_valid(const SZrCompileIrCacheKey *key) {
    return (TZrBool)(key != ZR_NULL && key->valid &&
                     key->schemaVersion == ZR_COMPILE_IR_CACHE_SCHEMA_VERSION);
}

/* 固定宽度值按小端序进入摘要，避免缓存键依赖宿主字节序。 */
static TZrBool cache_hash_u64(SZrParserSha256Context *context,
                              TZrUInt64 value) {
    TZrByte bytes[8];
    TZrUInt32 index;
    for (index = 0U; index < 8U; ++index) {
        bytes[index] = (TZrByte)((value >> (index * 8U)) & 0xffU);
    }
    return ZrParser_Sha256_Update(context, bytes, sizeof(bytes));
}

/* 长度前缀保留变长字段边界；空字段允许用空指针和零长度表示。 */
static TZrBool cache_hash_bytes(SZrParserSha256Context *context,
                                const TZrByte *bytes,
                                TZrSize size) {
    if (bytes == ZR_NULL && size != 0U) {
        return ZR_FALSE;
    }
    return cache_hash_u64(context, (TZrUInt64)size) &&
           ZrParser_Sha256_Update(context, bytes, size);
}

/* 发布前为调用方借出的载荷生成指纹，命中时用同一规约核验缓存持有的副本。 */
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

/* 仅接受当前策略 schema 且其声明 hash 与策略字段重算结果一致。 */
static TZrBool cache_profile_valid(const SZrCompileEffectivePolicy *profile) {
    return (TZrBool)(profile != ZR_NULL &&
                     profile->schemaVersion ==
                             ZR_COMPILE_OPTIMIZATION_PROFILE_SCHEMA_VERSION &&
                     profile->profileHash ==
                             ZrParser_CompileOptimizationProfile_Hash(profile));
}

/* 发布与显式剔除共用键匹配规约，使替换和删除针对同一缓存身份。 */
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

/* 命中读取借用条目而不剔除损坏项，由调用方通过 MarkCorrupt 明确决定缓存变更。 */
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

/* 扩容先检查字节数上界；realloc 失败时旧数组仍由 cache 持有。 */
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

/* TODO: 核查旧 schema 是否应使用专用 SCHEMA 诊断码；当前失配统一归为 INVALID_KEY。 */
/**
 * @brief 用规范化策略和不可变构建输入生成跨主机稳定的 IR 缓存键。
 * @pre profile 属于当前策略 schema；空 source/dependency 可用空指针和零长度表示。
 * @return 成功时 key 可用于缓存查找；失败时通过 diagnostic 报告，调用者不得使用 key。
 * @note 摘要覆盖源与依赖字节、策略 hash、编译 ABI、pass、target、numeric/layout 和 imported profile。
 */
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

/**
 * @brief 比较两个缓存键的 schema、所有构建字段和内容摘要。
 * @return 只有两个键都有效且当前 schema 一致时才可能返回 true。
 */
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

/**
 * @brief 初始化空缓存并从非零值开始分配 writer token。
 * @pre cache 尚未持有需要释放的条目，或已先调用 Free。
 */
void ZrParser_CompileIrCache_Init(SZrCompileIrCache *cache) {
    if (cache == ZR_NULL) {
        return;
    }
    (void)memset(cache, 0, sizeof(*cache));
    cache->nextWriterToken = 1U;
}

/**
 * @brief 释放缓存拥有的载荷副本和条目数组，并将缓存状态清零。
 * @pre cache 为 null 或已初始化且没有仍需完成的写事务。
 */
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

/**
 * @brief 清空写事务，使其可供 BeginWrite 使用。
 * @pre transaction 为 null 或当前不处于 active 状态；先取消已有事务再重置。
 */
void ZrParser_CompileIrCache_TransactionInit(
        SZrCompileIrCacheTransaction *transaction) {
    if (transaction != ZR_NULL) {
        (void)memset(transaction, 0, sizeof(*transaction));
    }
}

/* TODO: 核查写事务是否允许按值复制；writerToken 当前不参与发布或取消时的身份验证。 */
/**
 * @brief 为有效键开启一个可发布或取消的写事务。
 * @pre cache 已初始化，transaction 已初始化且未激活，key 使用当前 schema。
 * @return 成功时复制 key 并增加 activeWriters；失败时 diagnostic 描述拒绝原因。
 * @note 此对象不提供并发互斥；调用方须串行化同一 cache 的访问。
 */
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

/**
 * @brief 校验并复制完整非空载荷，再新增或替换该键的已发布项。
 * @pre transaction active 且属于 cache；complete 仅在调用方已验证载荷完整时为 true。
 * @return 成功时结束事务；失败时保留事务供重试或 CancelWrite，并保持原已发布项不变。
 * @note 本函数只校验完整标志、长度和载荷摘要，不解析 IR 格式；同一 cache 的访问须串行化。
 */
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

/**
 * @brief 取消属于 cache 的 active 写事务并撤销其活动计数。
 * @pre transaction active 且 transaction->cache 指向 cache。
 * @return 成功时清空事务关联；失败时不改变已发布条目。
 */
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

/**
 * @brief 按键查找已发布载荷，并在返回前复算摘要以发现载荷损坏。
 * @return 成功时填充 accepted hit；未找到、无效或损坏时清空 hit 并报告诊断。
 * @note hit->payload 借用缓存内存，在缓存后续变更或释放前有效；损坏项需显式 MarkCorrupt。
 */
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

/**
 * @brief 显式移除一个已知损坏的键及其载荷，不清空其他缓存项。
 * @return 成功表示找到并删除该键；缺失或无效键通过 diagnostic 返回。
 * @note 删除使用末项补位，会改变条目次序并使相关借用命中失效。
 */
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

/**
 * @brief 返回已发布条目数；空缓存指针按零项处理。
 * @return 当前条目数，不包含未完成写事务。
 */
TZrSize ZrParser_CompileIrCache_Count(const SZrCompileIrCache *cache) {
    return cache == ZR_NULL ? 0U : cache->count;
}
