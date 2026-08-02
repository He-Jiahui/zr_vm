#ifndef ZR_VM_TEST_COMPTIME_CACHE_SNAPSHOT_CASES_H
#define ZR_VM_TEST_COMPTIME_CACHE_SNAPSHOT_CASES_H

#include "zr_vm_parser/comptime_cache.h"

static void comptime_cache_snapshot_prepare_compiler(
        SZrCompilerState *compiler) {
    ZrParser_CompilerState_Init(compiler, g_state);
}

static void comptime_cache_snapshot_add_entry(
        SZrCompilerState *compiler,
        TZrByte digestSeed,
        const SZrTypeValue *value) {
    SZrComptimeCacheEntry entry;

    memset(&entry, 0, sizeof(entry));
    for (TZrSize index = 0U; index < sizeof(entry.digest); index++) {
        entry.digest[index] = (TZrByte)(digestSeed + index);
    }
    entry.value = *value;
    ZrCore_Array_Push(g_state, &compiler->comptimeCache, &entry);
}

static const SZrComptimeCacheEntry *comptime_cache_snapshot_find_entry(
        SZrCompilerState *compiler,
        TZrByte digestSeed) {
    for (TZrSize index = 0U; index < compiler->comptimeCache.length; index++) {
        const SZrComptimeCacheEntry *entry =
                (const SZrComptimeCacheEntry *)ZrCore_Array_Get(
                        &compiler->comptimeCache, index);
        if (entry != ZR_NULL && entry->digest[0] == digestSeed) {
            return entry;
        }
    }
    return ZR_NULL;
}

static void test_comptime_cache_snapshot_is_atomic_and_byte_stable(void) {
    SZrCompilerState firstCompiler;
    SZrCompilerState reorderedCompiler;
    SZrCompilerState loadedCompiler;
    const SZrComptimeCacheEntry *loadedEntry;
    SZrTypeValue firstValue;
    SZrTypeValue secondValue;
    SZrTypeValue sentinelValue;
    TZrByte *firstSnapshot = ZR_NULL;
    TZrByte *reorderedSnapshot = ZR_NULL;
    TZrByte *roundTripSnapshot = ZR_NULL;
    TZrByte *trailingSnapshot = ZR_NULL;
    TZrByte *payloadCorruptionSnapshot = ZR_NULL;
    TZrSize firstSnapshotSize = 0U;
    TZrSize reorderedSnapshotSize = 0U;
    TZrSize roundTripSnapshotSize = 0U;

    comptime_cache_snapshot_prepare_compiler(&firstCompiler);
    memset(&firstValue, 0, sizeof(firstValue));
    memset(&secondValue, 0, sizeof(secondValue));
    ZrCore_Value_InitAsInt(g_state, &firstValue, -41);
    ZrCore_Value_InitAsBool(g_state, &secondValue, ZR_TRUE);
    comptime_cache_snapshot_add_entry(&firstCompiler, 2U, &secondValue);
    comptime_cache_snapshot_add_entry(&firstCompiler, 1U, &firstValue);
    TEST_ASSERT_TRUE(ZrParser_ComptimeCache_ExportSnapshot(
            &firstCompiler, &firstSnapshot, &firstSnapshotSize));
    TEST_ASSERT_NOT_NULL(firstSnapshot);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, (TZrUInt32)firstSnapshotSize);

    comptime_cache_snapshot_prepare_compiler(&reorderedCompiler);
    comptime_cache_snapshot_add_entry(
            &reorderedCompiler, 1U, &firstValue);
    comptime_cache_snapshot_add_entry(
            &reorderedCompiler, 2U, &secondValue);
    TEST_ASSERT_TRUE(ZrParser_ComptimeCache_ExportSnapshot(
            &reorderedCompiler,
            &reorderedSnapshot,
            &reorderedSnapshotSize));
    TEST_ASSERT_EQUAL_UINT64(firstSnapshotSize, reorderedSnapshotSize);
    TEST_ASSERT_EQUAL_MEMORY(
            firstSnapshot, reorderedSnapshot, firstSnapshotSize);

    comptime_cache_snapshot_prepare_compiler(&loadedCompiler);
    memset(&sentinelValue, 0, sizeof(sentinelValue));
    ZrCore_Value_InitAsInt(g_state, &sentinelValue, 99);
    comptime_cache_snapshot_add_entry(
            &loadedCompiler, 99U, &sentinelValue);
    TEST_ASSERT_FALSE(ZrParser_ComptimeCache_ImportSnapshot(
            &loadedCompiler,
            firstSnapshot,
            firstSnapshotSize - 1U));
    firstSnapshot[0] ^= 0xFFU;
    TEST_ASSERT_FALSE(ZrParser_ComptimeCache_ImportSnapshot(
            &loadedCompiler, firstSnapshot, firstSnapshotSize));
    firstSnapshot[0] ^= 0xFFU;
    trailingSnapshot = (TZrByte *)malloc(firstSnapshotSize + 1U);
    TEST_ASSERT_NOT_NULL(trailingSnapshot);
    memcpy(trailingSnapshot, firstSnapshot, firstSnapshotSize);
    trailingSnapshot[firstSnapshotSize] = 0U;
    TEST_ASSERT_FALSE(ZrParser_ComptimeCache_ImportSnapshot(
            &loadedCompiler,
            trailingSnapshot,
            firstSnapshotSize + 1U));
    payloadCorruptionSnapshot = (TZrByte *)malloc(firstSnapshotSize);
    TEST_ASSERT_NOT_NULL(payloadCorruptionSnapshot);
    memcpy(payloadCorruptionSnapshot, firstSnapshot, firstSnapshotSize);
    payloadCorruptionSnapshot[firstSnapshotSize - 1U] ^= 0x01U;
    TEST_ASSERT_FALSE_MESSAGE(
            ZrParser_ComptimeCache_ImportSnapshot(
                    &loadedCompiler,
                    payloadCorruptionSnapshot,
                    firstSnapshotSize),
            "A structurally valid payload mutation must fail snapshot integrity validation");
    loadedEntry = comptime_cache_snapshot_find_entry(&loadedCompiler, 99U);
    TEST_ASSERT_NOT_NULL(loadedEntry);
    TEST_ASSERT_EQUAL_INT64(
            99, loadedEntry->value.value.nativeObject.nativeInt64);

    TEST_ASSERT_TRUE(ZrParser_ComptimeCache_ImportSnapshot(
            &loadedCompiler, firstSnapshot, firstSnapshotSize));
    loadedEntry = comptime_cache_snapshot_find_entry(&loadedCompiler, 1U);
    TEST_ASSERT_NOT_NULL(loadedEntry);
    TEST_ASSERT_EQUAL_INT64(
            -41, loadedEntry->value.value.nativeObject.nativeInt64);
    loadedEntry = comptime_cache_snapshot_find_entry(&loadedCompiler, 2U);
    TEST_ASSERT_NOT_NULL(loadedEntry);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, loadedEntry->value.type);
    TEST_ASSERT_TRUE(loadedEntry->value.value.nativeObject.nativeBool);
    TEST_ASSERT_NULL(comptime_cache_snapshot_find_entry(
            &loadedCompiler, 99U));

    TEST_ASSERT_TRUE(ZrParser_ComptimeCache_ExportSnapshot(
            &loadedCompiler,
            &roundTripSnapshot,
            &roundTripSnapshotSize));
    TEST_ASSERT_EQUAL_UINT64(firstSnapshotSize, roundTripSnapshotSize);
    TEST_ASSERT_EQUAL_MEMORY(
            firstSnapshot, roundTripSnapshot, firstSnapshotSize);

    free(payloadCorruptionSnapshot);
    free(trailingSnapshot);
    ZrParser_ComptimeCache_FreeSnapshot(
            g_state, roundTripSnapshot, roundTripSnapshotSize);
    ZrParser_ComptimeCache_FreeSnapshot(
            g_state, reorderedSnapshot, reorderedSnapshotSize);
    ZrParser_ComptimeCache_FreeSnapshot(
            g_state, firstSnapshot, firstSnapshotSize);
    ZrParser_CompilerState_Free(&loadedCompiler);
    ZrParser_CompilerState_Free(&reorderedCompiler);
    ZrParser_CompilerState_Free(&firstCompiler);
}

#endif
