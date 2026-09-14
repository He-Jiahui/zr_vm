#ifndef ZR_VM_PARSER_COMPILE_IR_CACHE_H
#define ZR_VM_PARSER_COMPILE_IR_CACHE_H

#include "zr_vm_parser/compile_optimization_profile.h"

#define ZR_COMPILE_IR_CACHE_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_COMPILE_IR_CACHE_DIGEST_SIZE ((TZrSize)32u)

/* Inputs are immutable build facts.  Runtime addresses and generation
 * witnesses are intentionally absent so a key can be moved between hosts. */
typedef struct SZrCompileIrCacheInputs {
    const TZrByte *sourceBytes;
    TZrSize sourceSize;
    const TZrByte *dependencyBytes;
    TZrSize dependencySize;
    TZrUInt64 contractHash;
    TZrUInt64 compilerAbi;
    TZrUInt64 passPipelineHash;
    TZrUInt64 targetHash;
    TZrUInt64 numericPolicyHash;
    TZrUInt64 layoutHash;
    TZrUInt64 importedProfileHash;
} SZrCompileIrCacheInputs;

typedef struct SZrCompileIrCacheKey {
    TZrUInt32 schemaVersion;
    TZrUInt64 profileHash;
    TZrUInt64 contractHash;
    TZrUInt64 compilerAbi;
    TZrUInt64 passPipelineHash;
    TZrUInt64 targetHash;
    TZrUInt64 numericPolicyHash;
    TZrUInt64 layoutHash;
    TZrUInt64 importedProfileHash;
    TZrByte digest[ZR_COMPILE_IR_CACHE_DIGEST_SIZE];
    TZrBool valid;
} SZrCompileIrCacheKey;

typedef enum EZrCompileIrCacheDiagnosticCode {
    ZR_COMPILE_IR_CACHE_DIAGNOSTIC_NONE = 0,
    ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INVALID_KEY,
    ZR_COMPILE_IR_CACHE_DIAGNOSTIC_SCHEMA,
    ZR_COMPILE_IR_CACHE_DIAGNOSTIC_ALLOCATION,
    ZR_COMPILE_IR_CACHE_DIAGNOSTIC_ACTIVE_WRITER,
    ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INCOMPLETE_PAYLOAD,
    ZR_COMPILE_IR_CACHE_DIAGNOSTIC_NOT_FOUND,
    ZR_COMPILE_IR_CACHE_DIAGNOSTIC_CORRUPT
} EZrCompileIrCacheDiagnosticCode;

typedef struct SZrCompileIrCacheDiagnostic {
    EZrCompileIrCacheDiagnosticCode code;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrCompileIrCacheDiagnostic;

typedef struct SZrCompileIrCacheHit {
    const TZrByte *payload; /* borrowed until the cache is mutated or freed */
    TZrSize payloadSize;
    TZrBool accepted;
} SZrCompileIrCacheHit;

typedef struct SZrCompileIrCacheEntry SZrCompileIrCacheEntry;

typedef struct SZrCompileIrCache {
    SZrCompileIrCacheEntry *entries;
    TZrSize count;
    TZrSize capacity;
    TZrUInt64 nextWriterToken;
    TZrUInt32 activeWriters;
} SZrCompileIrCache;

typedef struct SZrCompileIrCacheTransaction {
    SZrCompileIrCache *cache;
    SZrCompileIrCacheKey key;
    TZrUInt64 writerToken;
    TZrBool active;
} SZrCompileIrCacheTransaction;

ZR_PARSER_API TZrBool ZrParser_CompileIrCache_BuildKey(
        const SZrCompileEffectivePolicy *profile,
        const SZrCompileIrCacheInputs *inputs,
        SZrCompileIrCacheKey *key,
        SZrCompileIrCacheDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_CompileIrCache_KeyEquals(
        const SZrCompileIrCacheKey *left,
        const SZrCompileIrCacheKey *right);
ZR_PARSER_API void ZrParser_CompileIrCache_Init(SZrCompileIrCache *cache);
ZR_PARSER_API void ZrParser_CompileIrCache_Free(SZrCompileIrCache *cache);
ZR_PARSER_API void ZrParser_CompileIrCache_TransactionInit(
        SZrCompileIrCacheTransaction *transaction);
ZR_PARSER_API TZrBool ZrParser_CompileIrCache_BeginWrite(
        SZrCompileIrCache *cache,
        const SZrCompileIrCacheKey *key,
        SZrCompileIrCacheTransaction *transaction,
        SZrCompileIrCacheDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_CompileIrCache_Publish(
        SZrCompileIrCache *cache,
        SZrCompileIrCacheTransaction *transaction,
        const TZrByte *payload,
        TZrSize payloadSize,
        TZrBool complete,
        SZrCompileIrCacheDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_CompileIrCache_CancelWrite(
        SZrCompileIrCache *cache,
        SZrCompileIrCacheTransaction *transaction,
        SZrCompileIrCacheDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_CompileIrCache_Lookup(
        const SZrCompileIrCache *cache,
        const SZrCompileIrCacheKey *key,
        SZrCompileIrCacheHit *hit,
        SZrCompileIrCacheDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_CompileIrCache_MarkCorrupt(
        SZrCompileIrCache *cache,
        const SZrCompileIrCacheKey *key,
        SZrCompileIrCacheDiagnostic *diagnostic);
ZR_PARSER_API TZrSize ZrParser_CompileIrCache_Count(
        const SZrCompileIrCache *cache);

#endif /* ZR_VM_PARSER_COMPILE_IR_CACHE_H */
