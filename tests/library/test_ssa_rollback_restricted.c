#include "zr_vm_core/hotpatch_rollback.h"
#include "zr_vm_core/hotpatch_profile.h"

#include <stdio.h>
#include <string.h>

/* validated 仍由此文件的栈 fixture 构造；端到端 Validate 信任链属于另一测试切片。 */
static int g_testFailureCount = 0;

#define TEST_CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "rollback restricted check failed at line %d: %s\n", \
                    __LINE__, #condition); \
            ++g_testFailureCount; \
        } \
    } while (0)

typedef enum ETestRollbackFailure {
    TEST_ROLLBACK_INVALID_TARGET = 0,
    TEST_ROLLBACK_MISSING_TARGET,
    TEST_ROLLBACK_MANAGER_CAPACITY,
    TEST_ROLLBACK_GENERATION_OVERFLOW
} ETestRollbackFailure;

typedef struct SZrRollbackRecordSnapshot {
    TZrUInt64 generation;
    TZrUInt64 moduleHash;
    TZrUInt64 contentHash;
    TZrUInt64 publicContractHash;
    TZrUInt32 targetProfile;
    TZrUInt32 state;
    TZrUInt32 leaseCount;
} SZrRollbackRecordSnapshot;

#define CHECK_ROLLBACK_CASE(failureCount, caseName, condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s rollback check failed at line %d: %s\n", \
                    (caseName), __LINE__, #condition); \
            ++(failureCount); \
        } \
    } while (0)

static SZrRollbackRecordSnapshot snapshot_record(
        const SZrHotPatchVersionRecord *record) {
    SZrRollbackRecordSnapshot snapshot;
    snapshot.generation = record->generation;
    snapshot.moduleHash = record->moduleHash;
    snapshot.contentHash = record->contentHash;
    snapshot.publicContractHash = record->publicContractHash;
    snapshot.targetProfile = record->targetProfile;
    snapshot.state = record->state;
    snapshot.leaseCount = atomic_load_explicit(&record->leaseCount,
                                                memory_order_relaxed);
    return snapshot;
}

static void initialize_rollback_fixture(
        SZrValidatedHotPatch *validated,
        SZrArtifactExecIrView *artifact,
        SZrHotPatchCapabilityManifest *manifest,
        TZrByte *bytes,
        TZrUInt32 byteCount) {
    memset(artifact, 0, sizeof(*artifact));
    artifact->buffer = bytes;
    artifact->bufferLength = byteCount;
    artifact->moduleHash = 7u;
    artifact->abiVersion = 16u;

    memset(manifest, 0, sizeof(*manifest));
    manifest->schemaVersion = ZR_HOT_PATCH_CAPABILITY_SCHEMA_VERSION;
    manifest->patchId = 101u;
    manifest->contentHash = ZrCore_ArtifactExecIr_HashBytes(bytes, byteCount);
    manifest->baseModuleHash = 7u;
    manifest->publicContractHash = 55u;
    manifest->targetAbiVersion = 16u;
    manifest->targetProfile = ZR_HOT_PATCH_PROFILE_IOS_INTERPRETER;

    memset(validated, 0, sizeof(*validated));
    validated->artifact = artifact;
    validated->manifest = manifest;
    validated->contentHash = manifest->contentHash;
    validated->contentBytes = bytes;
    validated->contentLength = byteCount;
    validated->patchId = manifest->patchId;
    validated->publicContractHash = manifest->publicContractHash;
    validated->targetProfile = manifest->targetProfile;
    validated->signatureVerified = ZR_TRUE;
    validated->immutableContent = ZR_TRUE;
}

static int test_rollback_failure_preserves_state(
        ETestRollbackFailure failureCase) {
    TZrByte bytes[2] = {0x31u, 0x41u};
    SZrArtifactExecIrView artifact;
    SZrHotPatchCapabilityManifest manifest;
    SZrValidatedHotPatch validated;
    SZrHotPatchVersionRecord records[3];
    SZrRollbackRecordSnapshot before[3];
    SZrHotPatchGenerationManager manager;
    SZrHotPatchGenerationDiagnostic generationDiagnostic;
    SZrHotPatchGenerationHandle prepared;
    SZrHotPatchGenerationHandle outHandle;
    SZrHotPatchApplyDiagnostic applyDiagnostic;
    EZrHotPatchGenerationStatus generationStatus;
    EZrHotPatchApplyStatus expectedStatus;
    EZrHotPatchApplyStatus applyStatus;
    TZrUInt32 capacity = failureCase == TEST_ROLLBACK_MANAGER_CAPACITY ? 2u : 3u;
    TZrUInt32 countBefore;
    TZrUInt64 nextGenerationBefore;
    TZrUInt64 targetGeneration = 1u;
    TZrUInt32 recordCount;
    SZrHotPatchVersionRecord *activeBefore;
    const TZrChar *caseName;
    const TZrChar *expectedStatusName;
    int failures = 0;

    switch (failureCase) {
        case TEST_ROLLBACK_INVALID_TARGET:
            targetGeneration = 0u;
            expectedStatus = ZR_HOT_PATCH_APPLY_INVALID_ARGUMENT;
            expectedStatusName = "invalid-argument";
            caseName = "invalid target";
            break;
        case TEST_ROLLBACK_MISSING_TARGET:
            targetGeneration = 999u;
            expectedStatus = ZR_HOT_PATCH_APPLY_ROLLBACK_NOT_FOUND;
            expectedStatusName = "rollback-not-found";
            caseName = "missing target";
            break;
        case TEST_ROLLBACK_MANAGER_CAPACITY:
            expectedStatus = ZR_HOT_PATCH_APPLY_CAPACITY;
            expectedStatusName = "capacity";
            caseName = "manager capacity";
            break;
        case TEST_ROLLBACK_GENERATION_OVERFLOW:
            expectedStatus = ZR_HOT_PATCH_APPLY_GENERATION_OVERFLOW;
            expectedStatusName = "generation-overflow";
            caseName = "generation overflow";
            break;
        default:
            return 1;
    }

    initialize_rollback_fixture(&validated, &artifact, &manifest, bytes,
                                (TZrUInt32)sizeof(bytes));
    generationStatus = ZrCore_HotPatch_GenerationManager_Init(
            &manager, records, capacity, &generationDiagnostic);
    CHECK_ROLLBACK_CASE(failures, caseName,
                        generationStatus == ZR_HOT_PATCH_GENERATION_OK);
    if (generationStatus != ZR_HOT_PATCH_GENERATION_OK) return failures;

    /* Publish two actual records so generation 1 is retained as RETIRED and
     * generation 2 is active. Capacity and overflow failures then exercise a
     * real rollback target rather than fabricated manager state. */
    for (TZrUInt32 i = 0u; i < 2u; ++i) {
        generationStatus = ZrCore_HotPatch_Generation_Prepare(
                &manager, &validated, 7u, &prepared, &generationDiagnostic);
        CHECK_ROLLBACK_CASE(failures, caseName,
                            generationStatus == ZR_HOT_PATCH_GENERATION_OK);
        if (generationStatus != ZR_HOT_PATCH_GENERATION_OK) goto cleanup;
        generationStatus = ZrCore_HotPatch_Generation_Publish(
                &manager, &prepared, &generationDiagnostic);
        CHECK_ROLLBACK_CASE(failures, caseName,
                            generationStatus == ZR_HOT_PATCH_GENERATION_OK);
        if (generationStatus != ZR_HOT_PATCH_GENERATION_OK) goto cleanup;
    }

    CHECK_ROLLBACK_CASE(failures, caseName,
                        records[0].generation == 1u &&
                        records[0].state == ZR_HOT_PATCH_VERSION_RETIRED);
    CHECK_ROLLBACK_CASE(failures, caseName,
                        records[1].generation == 2u &&
                        records[1].state == ZR_HOT_PATCH_VERSION_ACTIVE);
    if (failureCase == TEST_ROLLBACK_MANAGER_CAPACITY) {
        CHECK_ROLLBACK_CASE(failures, caseName, manager.count == capacity);
    }
    if (failureCase == TEST_ROLLBACK_GENERATION_OVERFLOW) {
        atomic_store_explicit(&manager.nextGeneration, UINT64_MAX,
                              memory_order_relaxed);
    }

    recordCount = manager.capacity;
    for (TZrUInt32 i = 0u; i < recordCount; ++i) {
        before[i] = snapshot_record(&records[i]);
    }
    countBefore = manager.count;
    nextGenerationBefore = atomic_load_explicit(
            &manager.nextGeneration, memory_order_relaxed);
    activeBefore = atomic_load_explicit(&manager.active, memory_order_acquire);
    memset(&outHandle, 0xa5, sizeof(outHandle));
    memset(&applyDiagnostic, 0xa5, sizeof(applyDiagnostic));

    applyStatus = ZrCore_HotPatch_Rollback(
            &manager, targetGeneration, &outHandle, &applyDiagnostic);
    CHECK_ROLLBACK_CASE(failures, caseName, applyStatus == expectedStatus);
    CHECK_ROLLBACK_CASE(failures, caseName,
                        strcmp(ZrCore_HotPatch_ApplyStatusName(applyStatus),
                               expectedStatusName) == 0);
    CHECK_ROLLBACK_CASE(failures, caseName,
                        applyDiagnostic.status == expectedStatus);
    CHECK_ROLLBACK_CASE(failures, caseName, applyDiagnostic.patchId == 0u);
    CHECK_ROLLBACK_CASE(failures, caseName,
                        applyDiagnostic.expectedHash == targetGeneration);
    CHECK_ROLLBACK_CASE(failures, caseName,
                        applyDiagnostic.actualHash == 0u);
    CHECK_ROLLBACK_CASE(failures, caseName,
                        applyDiagnostic.generation == 0u);
    CHECK_ROLLBACK_CASE(failures, caseName, manager.count == countBefore);
    CHECK_ROLLBACK_CASE(failures, caseName,
                        atomic_load_explicit(&manager.nextGeneration,
                                             memory_order_relaxed) ==
                        nextGenerationBefore);
    CHECK_ROLLBACK_CASE(failures, caseName,
                        atomic_load_explicit(&manager.active,
                                             memory_order_acquire) ==
                        activeBefore);
    CHECK_ROLLBACK_CASE(failures, caseName,
                        outHandle.record == ZR_NULL &&
                        outHandle.generation == 0u &&
                        outHandle.leased == ZR_FALSE);
    for (TZrUInt32 i = 0u; i < recordCount; ++i) {
        SZrRollbackRecordSnapshot after = snapshot_record(&records[i]);
        CHECK_ROLLBACK_CASE(failures, caseName,
                            after.generation == before[i].generation &&
                            after.moduleHash == before[i].moduleHash &&
                            after.contentHash == before[i].contentHash &&
                            after.publicContractHash ==
                                    before[i].publicContractHash &&
                            after.targetProfile == before[i].targetProfile &&
                            after.state == before[i].state &&
                            after.leaseCount == before[i].leaseCount);
    }

cleanup:
    ZrCore_HotPatch_GenerationManager_Deinit(&manager);
    return failures;
}

static int test_rollback_failure_statuses(void) {
    int failures = 0;
    failures += test_rollback_failure_preserves_state(
            TEST_ROLLBACK_INVALID_TARGET);
    failures += test_rollback_failure_preserves_state(
            TEST_ROLLBACK_MISSING_TARGET);
    failures += test_rollback_failure_preserves_state(
            TEST_ROLLBACK_MANAGER_CAPACITY);
    failures += test_rollback_failure_preserves_state(
            TEST_ROLLBACK_GENERATION_OVERFLOW);
    return failures;
}

int main(void) {
    SZrHotPatchVersionRecord records[4];
    SZrHotPatchGenerationManager manager;
    SZrHotPatchGenerationHandle h1, h2;
    SZrHotPatchGenerationDiagnostic gd;
    SZrHotPatchRegistryEntry entries[4] = {0};
    SZrHotPatchRegistry registry = {entries, 4u, 0u};
    SZrHotPatchCapabilityManifest manifest = {1u, 0u, 7u, 55u, 9u, 11u, 0u, 16u, 1u, 0u, ZR_NULL};
    TZrByte bytes[1] = {0x42u};
    SZrArtifactExecIrView artifact;
    SZrValidatedHotPatch validated;
    SZrHotPatchApplyDiagnostic ad;
    SZrHotPatchRestrictedDiagnostic rd;
    EZrHotPatchGenerationStatus generationStatus;
    EZrHotPatchApplyStatus applyStatus;
    EZrHotPatchRestrictedStatus restrictedStatus;
    int failureCount = test_rollback_failure_statuses();
    if (failureCount != 0) return 1;
    memset(&artifact, 0, sizeof(artifact)); artifact.buffer = bytes; artifact.bufferLength = (TZrUInt32)sizeof(bytes);
    manifest.contentHash = ZrCore_ArtifactExecIr_HashBytes(bytes, (TZrUInt32)sizeof(bytes));
    validated.artifact = &artifact; validated.manifest = &manifest;
    validated.contentHash = manifest.contentHash;
    validated.contentBytes = bytes; validated.contentLength = (TZrUInt32)sizeof(bytes);
    validated.patchId = manifest.patchId; validated.publicContractHash = manifest.publicContractHash;
    validated.signatureVerified = ZR_TRUE; validated.immutableContent = ZR_TRUE; validated.targetProfile = 1u;
    /* 已验证令牌的幂等、ID 碰撞及回滚代际均属于同一 manager 生命周期。 */
    generationStatus = ZrCore_HotPatch_GenerationManager_Init(
            &manager, records, 4u, &gd);
    TEST_CHECK(generationStatus == ZR_HOT_PATCH_GENERATION_OK);
    if (generationStatus != ZR_HOT_PATCH_GENERATION_OK) return 1;

    applyStatus = ZrCore_HotPatch_ApplyValidated(
            &manager, &registry, &validated, 9u, &h1, &ad);
    TEST_CHECK(applyStatus == ZR_HOT_PATCH_APPLY_OK);
    if (applyStatus != ZR_HOT_PATCH_APPLY_OK) {
        ZrCore_HotPatch_GenerationManager_Deinit(&manager);
        return 1;
    }
    TEST_CHECK(entries[0].generation == h1.generation);
    applyStatus = ZrCore_HotPatch_ApplyValidated(
            &manager, &registry, &validated, 9u, &h2, &ad);
    TEST_CHECK(applyStatus == ZR_HOT_PATCH_APPLY_ALREADY_APPLIED);
    bytes[0] ^= 0x01u;
    manifest.contentHash = ZrCore_ArtifactExecIr_HashBytes(bytes, (TZrUInt32)sizeof(bytes));
    validated.contentHash = manifest.contentHash;
    applyStatus = ZrCore_HotPatch_ApplyValidated(
            &manager, &registry, &validated, 9u, &h2, &ad);
    TEST_CHECK(applyStatus == ZR_HOT_PATCH_APPLY_ID_COLLISION);
    bytes[0] ^= 0x01u;
    manifest.contentHash = ZrCore_ArtifactExecIr_HashBytes(bytes, (TZrUInt32)sizeof(bytes));
    validated.contentHash = manifest.contentHash;
    applyStatus = ZrCore_HotPatch_Rollback(
            &manager, h1.generation, &h2, &ad);
    TEST_CHECK(applyStatus == ZR_HOT_PATCH_APPLY_OK);
    if (applyStatus == ZR_HOT_PATCH_APPLY_OK) {
        TEST_CHECK(h2.generation != h1.generation);
    }
    /* 受限解释器可接收无 relocation 的产物，新增该 section 后须拒绝。 */
    restrictedStatus = ZrCore_HotPatch_ValidateRestrictedProfile(
            &artifact, ZR_HOT_PATCH_PROFILE_IOS_INTERPRETER, &rd);
    TEST_CHECK(restrictedStatus == ZR_HOT_PATCH_RESTRICTED_OK);
    artifact.sectionCount = 1u; artifact.sections[0].kind = ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS;
    restrictedStatus = ZrCore_HotPatch_ValidateRestrictedProfile(
            &artifact, ZR_HOT_PATCH_PROFILE_WASM_INTERPRETER, &rd);
    TEST_CHECK(restrictedStatus ==
               ZR_HOT_PATCH_RESTRICTED_SECTION_FORBIDDEN);
    ZrCore_HotPatch_GenerationManager_Deinit(&manager);
    return g_testFailureCount == 0 ? 0 : 1;
}
