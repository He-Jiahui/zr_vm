#include "zr_vm_parser/exec_ir_profile.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ZR_PROFILE_FNV_OFFSET UINT64_C(1469598103934665603)
#define ZR_PROFILE_FNV_PRIME UINT64_C(1099511628211)

static void zr_profile_diagnostic(SZrExecIrDiagnostic *diagnostic,
                                  EZrExecutionDiagnosticCode code,
                                  TZrUInt64 expected,
                                  TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->expectedHash = expected;
    diagnostic->actualHash = actual;
}

static void zr_profile_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 index;
    for (index = 0u; index < 4u; ++index) {
        *hash ^= (TZrUInt8)(value >> (index * 8u));
        *hash *= ZR_PROFILE_FNV_PRIME;
    }
}

static void zr_profile_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    TZrUInt32 index;
    for (index = 0u; index < 8u; ++index) {
        *hash ^= (TZrUInt8)(value >> (index * 8u));
        *hash *= ZR_PROFILE_FNV_PRIME;
    }
}

static TZrBool zr_profile_storage_valid(const SZrExecIrProfile *profile) {
    TZrUInt32 magic = 0u;
    TZrUInt32 count = 0u;
    TZrUInt32 capacity = 0u;
    SZrExecIrProfileSite *sites = ZR_NULL;
    if (profile == ZR_NULL) return ZR_FALSE;
    memcpy(&magic, &profile->magic, sizeof(magic));
    if (magic != ZR_EXEC_IR_PROFILE_MAGIC) return ZR_FALSE;
    memcpy(&count, &profile->siteCount, sizeof(count));
    memcpy(&capacity, &profile->siteCapacity, sizeof(capacity));
    memcpy(&sites, &profile->sites, sizeof(sites));
    return (TZrBool)(count <= capacity &&
                     ((capacity == 0u && sites == ZR_NULL) ||
                      (capacity != 0u && sites != ZR_NULL)));
}

static void zr_profile_release(SZrExecIrProfile *profile) {
    SZrExecIrProfileSite *sites = ZR_NULL;
    if (!zr_profile_storage_valid(profile)) return;
    memcpy(&sites, &profile->sites, sizeof(sites));
    free(sites);
}

static TZrBool zr_profile_reserve(SZrExecIrProfile *profile, TZrUInt32 required) {
    TZrUInt32 capacity;
    SZrExecIrProfileSite *sites;
    if (profile == ZR_NULL || required <= profile->siteCapacity) return ZR_TRUE;
    capacity = profile->siteCapacity == 0u ? 8u : profile->siteCapacity;
    while (capacity < required) {
        if (capacity > UINT32_MAX / 2u) {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
#if SIZE_MAX < UINT32_MAX
    if (capacity > (TZrUInt32)(SIZE_MAX / sizeof(*sites))) return ZR_FALSE;
#endif
    sites = (SZrExecIrProfileSite *)realloc(
            profile->sites, (size_t)capacity * sizeof(*sites));
    if (sites == ZR_NULL) return ZR_FALSE;
    profile->sites = sites;
    profile->siteCapacity = capacity;
    return ZR_TRUE;
}

void ZrParser_ExecIr_SpecializationPolicyInit(
        SZrExecIrSpecializationPolicy *policy) {
    if (policy == ZR_NULL) return;
    memset(policy, 0, sizeof(*policy));
    policy->maxVersionsPerSite =
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_VERSIONS_PER_SITE;
    policy->maxVersionsPerFunction =
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_VERSIONS_PER_FUNCTION;
    policy->maxVersionsPerModule =
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_VERSIONS_PER_MODULE;
    policy->maxCodeBytesPerSite =
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CODE_BYTES_PER_SITE;
    policy->maxCodeBytesPerFunction =
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CODE_BYTES_PER_FUNCTION;
    policy->maxCodeBytesPerModule =
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CODE_BYTES_PER_MODULE;
    policy->minSamples = ZR_EXEC_IR_PROFILE_DEFAULT_MIN_SAMPLES;
    policy->minHitRatePermille =
            ZR_EXEC_IR_PROFILE_DEFAULT_MIN_HIT_RATE_PERMILLE;
    policy->maxMissRatePermille =
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_MISS_RATE_PERMILLE;
    policy->maxConsecutiveDeopts =
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CONSECUTIVE_DEOPTS;
    policy->cooldownSamples = ZR_EXEC_IR_PROFILE_DEFAULT_COOLDOWN_SAMPLES;
    policy->maxPolymorphicTargets =
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_POLYMORPHIC_TARGETS;
}

void ZrParser_ExecIr_ProfileInit(SZrExecIrProfile *profile) {
    if (profile == ZR_NULL) return;
    zr_profile_release(profile);
    memset(profile, 0, sizeof(*profile));
    profile->magic = ZR_EXEC_IR_PROFILE_MAGIC;
    profile->schemaVersion = ZR_EXEC_IR_PROFILE_SCHEMA_VERSION;
    ZrParser_ExecIr_SpecializationPolicyInit(&profile->policy);
}

void ZrParser_ExecIr_ProfileFree(SZrExecIrProfile *profile) {
    if (profile == ZR_NULL) return;
    zr_profile_release(profile);
    memset(profile, 0, sizeof(*profile));
}

static TZrBool zr_profile_module_storage_valid(const SZrExecIrModule *module) {
    if (module == ZR_NULL ||
        module->functionCount > module->functionCapacity ||
        module->constantCount > module->constantCapacity ||
        module->layoutCount > module->layoutCapacity ||
        (module->functionCount != 0u && module->functions == ZR_NULL) ||
        (module->constantCount != 0u && module->constants == ZR_NULL) ||
        (module->layoutCount != 0u && module->layouts == ZR_NULL)) return ZR_FALSE;
    return ZR_TRUE;
}

static TZrUInt64 zr_profile_function_hash(const SZrExecIrFunction *function) {
    TZrUInt64 hash = ZR_PROFILE_FNV_OFFSET;
    TZrUInt32 index;
    if (function == ZR_NULL ||
        function->instructionCount > function->instructionCapacity ||
        function->valueCount > function->valueCapacity ||
        function->blockCount > function->blockCapacity ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->valueCount != 0u && function->values == ZR_NULL) ||
        (function->blockCount != 0u && function->blocks == ZR_NULL)) return 0u;
    zr_profile_hash_u32(&hash, function->id);
    zr_profile_hash_u32(&hash, function->functionToken);
    zr_profile_hash_u64(&hash, function->signatureHash);
    zr_profile_hash_u64(&hash, function->contract.generation);
    zr_profile_hash_u64(&hash, function->contract.layoutHash);
    zr_profile_hash_u32(&hash, function->instructionCount);
    zr_profile_hash_u32(&hash, function->valueCount);
    zr_profile_hash_u32(&hash, function->blockCount);
    for (index = 0u; index < function->valueCount; ++index) {
        const SZrExecIrValue *value = &function->values[index];
        zr_profile_hash_u32(&hash, value->id);
        zr_profile_hash_u32(&hash, value->typeToken);
        zr_profile_hash_u32(&hash, (TZrUInt32)value->ownership);
        zr_profile_hash_u32(&hash, (TZrUInt32)value->nullability);
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        TZrUInt32 operandIndex;
        TZrUInt32 resultIndex;
        zr_profile_hash_u32(&hash, instruction->opcode);
        zr_profile_hash_u32(&hash, instruction->flags);
        zr_profile_hash_u32(&hash, instruction->typeToken);
        zr_profile_hash_u32(&hash, instruction->layoutId);
        zr_profile_hash_u32(&hash, instruction->sourceId);
        zr_profile_hash_u32(&hash, instruction->operands.count);
        zr_profile_hash_u32(&hash, instruction->results.count);
        if (function->operands != ZR_NULL) {
            for (operandIndex = 0u; operandIndex < instruction->operands.count;
                 ++operandIndex) {
                TZrUInt32 offset = instruction->operands.start + operandIndex;
                if (offset < function->operandCount)
                    zr_profile_hash_u32(&hash, function->operands[offset]);
            }
        }
        if (function->results != ZR_NULL) {
            for (resultIndex = 0u; resultIndex < instruction->results.count;
                 ++resultIndex) {
                TZrUInt32 offset = instruction->results.start + resultIndex;
                if (offset < function->resultCount)
                    zr_profile_hash_u32(&hash, function->results[offset]);
            }
        }
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        zr_profile_hash_u32(&hash, block->id);
        zr_profile_hash_u32(&hash, block->flags);
        zr_profile_hash_u32(&hash, block->instructionRange.start);
        zr_profile_hash_u32(&hash, block->instructionRange.count);
        zr_profile_hash_u32(&hash, block->predecessorRange.start);
        zr_profile_hash_u32(&hash, block->predecessorRange.count);
        zr_profile_hash_u32(&hash, block->successorRange.start);
        zr_profile_hash_u32(&hash, block->successorRange.count);
    }
    return hash == 0u ? 1u : hash;
}

TZrUInt64 ZrParser_ExecIr_ProfileModuleHash(const SZrExecIrModule *module) {
    TZrUInt64 hash = ZR_PROFILE_FNV_OFFSET;
    TZrUInt32 index;
    if (!zr_profile_module_storage_valid(module)) return 0u;
    zr_profile_hash_u32(&hash, module->id);
    zr_profile_hash_u32(&hash, module->moduleToken);
    zr_profile_hash_u32(&hash, module->contract.schemaVersion);
    zr_profile_hash_u32(&hash, module->contract.abiVersion);
    zr_profile_hash_u32(&hash, module->contract.logicalVersion);
    zr_profile_hash_u64(&hash, module->contract.generation);
    zr_profile_hash_u64(&hash, module->contract.signatureHash);
    zr_profile_hash_u64(&hash, module->contract.layoutHash);
    zr_profile_hash_u32(&hash, module->constantCount);
    for (index = 0u; index < module->constantCount; ++index) {
        zr_profile_hash_u32(&hash, module->constants[index].typeToken);
        zr_profile_hash_u32(&hash, module->constants[index].flags);
        zr_profile_hash_u64(&hash, module->constants[index].bits);
    }
    zr_profile_hash_u32(&hash, module->layoutCount);
    for (index = 0u; index < module->layoutCount; ++index) {
        zr_profile_hash_u32(&hash, module->layouts[index].id);
        zr_profile_hash_u32(&hash, module->layouts[index].typeToken);
        zr_profile_hash_u32(&hash, module->layouts[index].byteSize);
        zr_profile_hash_u32(&hash, module->layouts[index].byteAlign);
        zr_profile_hash_u64(&hash, module->layouts[index].layoutHash);
    }
    zr_profile_hash_u32(&hash, module->functionCount);
    for (index = 0u; index < module->functionCount; ++index)
        zr_profile_hash_u64(&hash, zr_profile_function_hash(&module->functions[index]));
    return hash == 0u ? 1u : hash;
}

static TZrUInt64 zr_profile_module_ir_hash(const SZrExecIrModule *module) {
    TZrUInt64 hash = ZR_PROFILE_FNV_OFFSET;
    TZrUInt32 index;
    if (!zr_profile_module_storage_valid(module)) return 0u;
    zr_profile_hash_u32(&hash, module->functionCount);
    for (index = 0u; index < module->functionCount; ++index)
        zr_profile_hash_u64(&hash, zr_profile_function_hash(&module->functions[index]));
    return hash == 0u ? 1u : hash;
}

TZrBool ZrParser_ExecIr_ProfileBuildKey(const SZrExecIrModule *module,
                                         SZrExecIrProfileKey *key) {
    TZrUInt32 index;
    TZrUInt64 signature = 0u;
    TZrUInt64 layout = 0u;
    if (key == ZR_NULL || !zr_profile_module_storage_valid(module)) return ZR_FALSE;
    memset(key, 0, sizeof(*key));
    key->moduleIdentity = module->moduleToken != 0u ? module->moduleToken : module->id;
    key->moduleHash = ZrParser_ExecIr_ProfileModuleHash(module);
    key->irHash = zr_profile_module_ir_hash(module);
    key->signatureHash = module->contract.signatureHash;
    key->layoutHash = module->contract.layoutHash;
    if (key->signatureHash == 0u) {
        for (index = 0u; index < module->functionCount; ++index)
            signature ^= module->functions[index].signatureHash;
        key->signatureHash = signature;
    }
    if (key->layoutHash == 0u) {
        for (index = 0u; index < module->layoutCount; ++index)
            layout ^= module->layouts[index].layoutHash;
        key->layoutHash = layout;
    }
    key->compilerAbi = module->contract.abiVersion;
    return ZR_TRUE;
}

TZrUInt64 ZrParser_ExecIr_ProfileSiteKey(TZrMetadataToken functionToken,
                                         TZrExecIrInstructionId instructionId,
                                         TZrExecIrSourceId sourceId) {
    TZrUInt64 hash = ZR_PROFILE_FNV_OFFSET;
    zr_profile_hash_u32(&hash, functionToken);
    zr_profile_hash_u32(&hash, instructionId);
    zr_profile_hash_u32(&hash, sourceId);
    return hash == 0u ? 1u : hash;
}

TZrBool ZrParser_ExecIr_ProfileAddSite(SZrExecIrProfile *profile,
                                        const SZrExecIrProfileSite *site,
                                        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (!zr_profile_storage_valid(profile) || site == ZR_NULL ||
        site->siteKey == 0u || site->functionToken == 0u ||
        site->instructionId == 0u ||
        site->kind >= ZR_EXEC_IR_PROFILE_SITE_KIND_COUNT ||
        site->megamorphic > ZR_TRUE) {
        zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, 0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < profile->siteCount; ++index) {
        if (profile->sites[index].siteKey == site->siteKey) {
            zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  site->siteKey, profile->sites[index].siteKey);
            return ZR_FALSE;
        }
    }
    if (profile->siteCount == UINT32_MAX ||
        !zr_profile_reserve(profile, profile->siteCount + 1u)) {
        zr_profile_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                              profile->siteCount + 1u, 0u);
        return ZR_FALSE;
    }
    profile->sites[profile->siteCount++] = *site;
    return ZR_TRUE;
}

const SZrExecIrProfileSite *ZrParser_ExecIr_ProfileSiteAt(
        const SZrExecIrProfile *profile, TZrUInt32 index) {
    if (!zr_profile_storage_valid(profile) || index >= profile->siteCount ||
        profile->sites == ZR_NULL) return ZR_NULL;
    return &profile->sites[index];
}

const SZrExecIrProfileSite *ZrParser_ExecIr_ProfileFindSite(
        const SZrExecIrProfile *profile, TZrUInt64 siteKey) {
    TZrUInt32 index;
    if (!zr_profile_storage_valid(profile) || siteKey == 0u) return ZR_NULL;
    for (index = 0u; index < profile->siteCount; ++index) {
        if (profile->sites[index].siteKey == siteKey)
            return &profile->sites[index];
    }
    return ZR_NULL;
}

static int zr_profile_site_compare(const void *left, const void *right) {
    const SZrExecIrProfileSite *a = *(const SZrExecIrProfileSite * const *)left;
    const SZrExecIrProfileSite *b = *(const SZrExecIrProfileSite * const *)right;
    if (a->siteKey < b->siteKey) return -1;
    if (a->siteKey > b->siteKey) return 1;
    if ((TZrUInt32)a->kind < (TZrUInt32)b->kind) return -1;
    if ((TZrUInt32)a->kind > (TZrUInt32)b->kind) return 1;
    return 0;
}

TZrUInt64 ZrParser_ExecIr_ProfileHash(const SZrExecIrProfile *profile) {
    TZrUInt64 hash = ZR_PROFILE_FNV_OFFSET;
    SZrExecIrProfileSite const **ordered = ZR_NULL;
    TZrUInt32 index;
    if (!zr_profile_storage_valid(profile)) return 0u;
    if (profile->siteCount != 0u) {
#if SIZE_MAX < UINT32_MAX
        if (profile->siteCount > (TZrUInt32)(SIZE_MAX / sizeof(*ordered))) return 0u;
#endif
        ordered = (SZrExecIrProfileSite const **)malloc(
                (size_t)profile->siteCount * sizeof(*ordered));
        if (ordered == ZR_NULL) return 0u;
        for (index = 0u; index < profile->siteCount; ++index)
            ordered[index] = &profile->sites[index];
        qsort(ordered, profile->siteCount, sizeof(*ordered), zr_profile_site_compare);
    }
    zr_profile_hash_u32(&hash, profile->schemaVersion);
    zr_profile_hash_u32(&hash, profile->key.moduleIdentity);
    zr_profile_hash_u64(&hash, profile->key.moduleHash);
    zr_profile_hash_u64(&hash, profile->key.irHash);
    zr_profile_hash_u64(&hash, profile->key.signatureHash);
    zr_profile_hash_u64(&hash, profile->key.layoutHash);
    zr_profile_hash_u32(&hash, profile->key.compilerAbi);
    zr_profile_hash_u32(&hash, profile->siteCount);
    zr_profile_hash_u32(&hash, profile->policy.maxVersionsPerSite);
    zr_profile_hash_u32(&hash, profile->policy.maxVersionsPerFunction);
    zr_profile_hash_u32(&hash, profile->policy.maxVersionsPerModule);
    zr_profile_hash_u32(&hash, profile->policy.maxCodeBytesPerSite);
    zr_profile_hash_u32(&hash, profile->policy.maxCodeBytesPerFunction);
    zr_profile_hash_u32(&hash, profile->policy.maxCodeBytesPerModule);
    zr_profile_hash_u64(&hash, profile->policy.minSamples);
    zr_profile_hash_u32(&hash, profile->policy.minHitRatePermille);
    zr_profile_hash_u32(&hash, profile->policy.maxMissRatePermille);
    zr_profile_hash_u32(&hash, profile->policy.maxConsecutiveDeopts);
    zr_profile_hash_u32(&hash, profile->policy.cooldownSamples);
    zr_profile_hash_u32(&hash, profile->policy.maxPolymorphicTargets);
    for (index = 0u; index < profile->siteCount; ++index) {
        const SZrExecIrProfileSite *site = ordered[index];
        zr_profile_hash_u64(&hash, site->siteKey);
        zr_profile_hash_u32(&hash, site->functionToken);
        zr_profile_hash_u32(&hash, site->instructionId);
        zr_profile_hash_u32(&hash, site->sourceId);
        zr_profile_hash_u32(&hash, (TZrUInt32)site->kind);
        zr_profile_hash_u32(&hash, site->targetToken);
        zr_profile_hash_u32(&hash, site->receiverTypeToken);
        zr_profile_hash_u32(&hash, site->numericTypeToken);
        zr_profile_hash_u64(&hash, site->branchTaken);
        zr_profile_hash_u64(&hash, site->branchNotTaken);
        zr_profile_hash_u64(&hash, site->hitCount);
        zr_profile_hash_u64(&hash, site->missCount);
        zr_profile_hash_u64(&hash, site->allocationCount);
        zr_profile_hash_u64(&hash, site->allocationBytes);
        zr_profile_hash_u64(&hash, site->boxingCount);
        zr_profile_hash_u64(&hash, site->cacheHitCount);
        zr_profile_hash_u64(&hash, site->cacheMissCount);
        zr_profile_hash_u64(&hash, site->deoptCount);
        zr_profile_hash_u64(&hash, site->gcCount);
        zr_profile_hash_u64(&hash, site->contentionCount);
        zr_profile_hash_u32(&hash, site->distinctTargets);
        zr_profile_hash_u32(&hash, site->megamorphic);
    }
    free(ordered);
    return hash == 0u ? 1u : hash;
}

static EZrExecutionDiagnosticCode zr_profile_reason_code(
        EZrExecIrProfileReason reason) {
    switch (reason) {
        case ZR_EXEC_IR_PROFILE_REASON_SCHEMA:
            return ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH;
        case ZR_EXEC_IR_PROFILE_REASON_MODULE_IDENTITY:
        case ZR_EXEC_IR_PROFILE_REASON_MODULE_HASH:
            return ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH;
        case ZR_EXEC_IR_PROFILE_REASON_IR_HASH:
            return ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH;
        case ZR_EXEC_IR_PROFILE_REASON_SIGNATURE_HASH:
            return ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH;
        case ZR_EXEC_IR_PROFILE_REASON_LAYOUT_HASH:
            return ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH;
        case ZR_EXEC_IR_PROFILE_REASON_COMPILER_ABI:
            return ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH;
        case ZR_EXEC_IR_PROFILE_REASON_SITE:
        case ZR_EXEC_IR_PROFILE_REASON_DUPLICATE_SITE:
        case ZR_EXEC_IR_PROFILE_REASON_COUNTER_OVERFLOW:
        case ZR_EXEC_IR_PROFILE_REASON_MISSING_FIELD:
        case ZR_EXEC_IR_PROFILE_REASON_NONE:
        default:
            return ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
    }
}

static EZrExecIrProfileImportStatus zr_profile_validate_sites(
        const SZrExecIrProfile *profile, EZrExecIrProfileReason *reason,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (profile->siteCount != 0u && profile->sites == ZR_NULL) {
        if (reason != ZR_NULL) *reason = ZR_EXEC_IR_PROFILE_REASON_MISSING_FIELD;
        zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              profile->siteCount, 0u);
        return ZR_EXEC_IR_PROFILE_IMPORT_INVALID;
    }
    for (index = 0u; index < profile->siteCount; ++index) {
        const SZrExecIrProfileSite *site = &profile->sites[index];
        TZrUInt32 other;
        if (site->siteKey == 0u || site->functionToken == 0u ||
            site->instructionId == 0u ||
            site->kind >= ZR_EXEC_IR_PROFILE_SITE_KIND_COUNT ||
            site->megamorphic > ZR_TRUE) {
            if (reason != ZR_NULL) *reason = ZR_EXEC_IR_PROFILE_REASON_SITE;
            zr_profile_diagnostic(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  site->siteKey, site->instructionId);
            return ZR_EXEC_IR_PROFILE_IMPORT_INVALID;
        }
        for (other = 0u; other < index; ++other) {
            if (profile->sites[other].siteKey == site->siteKey) {
                if (reason != ZR_NULL)
                    *reason = ZR_EXEC_IR_PROFILE_REASON_DUPLICATE_SITE;
                zr_profile_diagnostic(diagnostic,
                                      ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                      site->siteKey, profile->sites[other].siteKey);
                return ZR_EXEC_IR_PROFILE_IMPORT_INVALID;
            }
        }
    }
    return ZR_EXEC_IR_PROFILE_IMPORT_ACCEPTED;
}

EZrExecIrProfileImportStatus ZrParser_ExecIr_ProfileValidate(
        const SZrExecIrModule *module, const SZrExecIrProfile *profile,
        EZrExecIrProfileReason *reason, SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrProfileKey expected;
    EZrExecIrProfileImportStatus status;
    if (reason != ZR_NULL) *reason = ZR_EXEC_IR_PROFILE_REASON_NONE;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (!zr_profile_module_storage_valid(module) ||
        !zr_profile_storage_valid(profile)) {
        if (reason != ZR_NULL) *reason = ZR_EXEC_IR_PROFILE_REASON_MISSING_FIELD;
        zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, 0u);
        return ZR_EXEC_IR_PROFILE_IMPORT_INVALID;
    }
    if (profile->schemaVersion != ZR_EXEC_IR_PROFILE_SCHEMA_VERSION) {
        if (reason != ZR_NULL) *reason = ZR_EXEC_IR_PROFILE_REASON_SCHEMA;
        zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
                              ZR_EXEC_IR_PROFILE_SCHEMA_VERSION,
                              profile->schemaVersion);
        return ZR_EXEC_IR_PROFILE_IMPORT_STALE;
    }
    if (profile->key.moduleIdentity == 0u || profile->key.moduleHash == 0u ||
        profile->key.irHash == 0u || profile->key.signatureHash == 0u ||
        profile->key.layoutHash == 0u || profile->key.compilerAbi == 0u) {
        if (reason != ZR_NULL) *reason = ZR_EXEC_IR_PROFILE_REASON_MISSING_FIELD;
        zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              1u, 0u);
        return ZR_EXEC_IR_PROFILE_IMPORT_MISSING_KEY;
    }
    if (!ZrParser_ExecIr_ProfileBuildKey(module, &expected)) {
        if (reason != ZR_NULL) *reason = ZR_EXEC_IR_PROFILE_REASON_MISSING_FIELD;
        zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, 0u);
        return ZR_EXEC_IR_PROFILE_IMPORT_INVALID;
    }
#define ZR_PROFILE_COMPARE_KEY(field, mismatchReason) \
    do { \
        if (profile->key.field != expected.field) { \
            if (reason != ZR_NULL) *reason = (mismatchReason); \
            zr_profile_diagnostic(diagnostic, \
                                  zr_profile_reason_code((mismatchReason)), \
                                  expected.field, profile->key.field); \
            return ZR_EXEC_IR_PROFILE_IMPORT_STALE; \
        } \
    } while (0)
    ZR_PROFILE_COMPARE_KEY(moduleIdentity, ZR_EXEC_IR_PROFILE_REASON_MODULE_IDENTITY);
    ZR_PROFILE_COMPARE_KEY(moduleHash, ZR_EXEC_IR_PROFILE_REASON_MODULE_HASH);
    ZR_PROFILE_COMPARE_KEY(irHash, ZR_EXEC_IR_PROFILE_REASON_IR_HASH);
    ZR_PROFILE_COMPARE_KEY(signatureHash, ZR_EXEC_IR_PROFILE_REASON_SIGNATURE_HASH);
    ZR_PROFILE_COMPARE_KEY(layoutHash, ZR_EXEC_IR_PROFILE_REASON_LAYOUT_HASH);
    ZR_PROFILE_COMPARE_KEY(compilerAbi, ZR_EXEC_IR_PROFILE_REASON_COMPILER_ABI);
#undef ZR_PROFILE_COMPARE_KEY
    status = zr_profile_validate_sites(profile, reason, diagnostic);
    return status;
}

TZrBool ZrParser_ExecIr_ProfileImport(
        const SZrExecIrModule *module, const SZrExecIrProfile *profile,
        SZrExecIrProfileImportResult *result, SZrExecIrDiagnostic *diagnostic) {
    EZrExecIrProfileReason reason = ZR_EXEC_IR_PROFILE_REASON_NONE;
    EZrExecIrProfileImportStatus status = ZrParser_ExecIr_ProfileValidate(
            module, profile, &reason, diagnostic);
    if (result != ZR_NULL) {
        memset(result, 0, sizeof(*result));
        result->status = status;
        result->reason = reason;
        result->accepted = (TZrBool)(status == ZR_EXEC_IR_PROFILE_IMPORT_ACCEPTED);
        result->ignored = (TZrBool)(status == ZR_EXEC_IR_PROFILE_IMPORT_STALE ||
                                    status == ZR_EXEC_IR_PROFILE_IMPORT_MISSING_KEY);
        result->siteCount = profile != ZR_NULL ? profile->siteCount : 0u;
    }
    /* A stale/missing profile is a valid compilation outcome: the baseline
     * remains available and the caller simply ignores the imported facts. */
    return (TZrBool)(status != ZR_EXEC_IR_PROFILE_IMPORT_INVALID);
}

TZrBool ZrParser_ExecIr_ImportProfile(SZrExecIrModule *module,
                                       const SZrExecIrProfile *profile,
                                       SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrProfileImportResult result;
    return ZrParser_ExecIr_ProfileImport(module, profile, &result, diagnostic);
}

static TZrUInt32 zr_profile_effective_u32(TZrUInt32 value, TZrUInt32 fallback) {
    return value == 0u ? fallback : value;
}

static TZrUInt64 zr_profile_effective_u64(TZrUInt64 value, TZrUInt64 fallback) {
    return value == 0u ? fallback : value;
}

static TZrBool zr_profile_policy_valid(const SZrExecIrSpecializationPolicy *policy) {
    if (policy == ZR_NULL || policy->minHitRatePermille > 1000u ||
        policy->maxMissRatePermille > 1000u) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

void ZrParser_ExecIr_SpecializationStateInit(
        SZrExecIrSpecializationState *state, TZrUInt64 siteKey) {
    if (state == ZR_NULL) return;
    memset(state, 0, sizeof(*state));
    state->siteKey = siteKey;
    state->baselineAvailable = ZR_TRUE;
}

void ZrParser_ExecIr_SpecializationStateFree(
        SZrExecIrSpecializationState *state) {
    if (state != ZR_NULL) memset(state, 0, sizeof(*state));
}

static TZrBool zr_profile_add_u64(TZrUInt64 *value, TZrUInt64 amount) {
    if (value == ZR_NULL || amount > UINT64_MAX - *value) return ZR_FALSE;
    *value += amount;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_SpecializationObserve(
        SZrExecIrSpecializationState *state,
        const SZrExecIrSpecializationPolicy *policy,
        TZrBool hit, TZrBool deopt, TZrUInt32 codeBytes,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 maxDeopts;
    TZrUInt32 cooldown;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (state == ZR_NULL || !zr_profile_policy_valid(policy) ||
        hit > ZR_TRUE || deopt > ZR_TRUE || state->siteKey == 0u) {
        zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, 0u);
        return ZR_FALSE;
    }
    if (hit) {
        if (!zr_profile_add_u64(&state->hitCount, 1u)) {
            zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  UINT64_MAX, state->hitCount);
            return ZR_FALSE;
        }
        state->consecutiveDeopts = 0u;
    } else if (!zr_profile_add_u64(&state->missCount, 1u)) {
        zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              UINT64_MAX, state->missCount);
        return ZR_FALSE;
    }
    if (deopt) {
        if (!zr_profile_add_u64(&state->deoptCount, 1u) ||
            state->consecutiveDeopts == UINT32_MAX) {
            zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  UINT64_MAX, state->deoptCount);
            return ZR_FALSE;
        }
        state->consecutiveDeopts++;
    }
    if (codeBytes > state->codeBytes) state->codeBytes = codeBytes;
    maxDeopts = zr_profile_effective_u32(
            policy->maxConsecutiveDeopts,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CONSECUTIVE_DEOPTS);
    cooldown = zr_profile_effective_u32(
            policy->cooldownSamples,
            ZR_EXEC_IR_PROFILE_DEFAULT_COOLDOWN_SAMPLES);
    if (deopt && state->consecutiveDeopts >= maxDeopts) {
        state->disabled = ZR_TRUE;
        state->cooldownRemaining = cooldown;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_SpecializationCommitVersion(
        SZrExecIrSpecializationState *state,
        const SZrExecIrSpecializationPolicy *policy,
        TZrUInt32 codeBytes, TZrUInt32 distinctTargets,
        TZrBool megamorphic, SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_SpecializationCommitVersionEx(
            state, policy, ZR_NULL, codeBytes, distinctTargets,
            megamorphic, diagnostic);
}

TZrBool ZrParser_ExecIr_SpecializationCommitVersionEx(
        SZrExecIrSpecializationState *state,
        const SZrExecIrSpecializationPolicy *policy,
        SZrExecIrSpecializationTotals *totals,
        TZrUInt32 codeBytes, TZrUInt32 distinctTargets,
        TZrBool megamorphic, SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 maxVersions;
    TZrUInt32 maxBytes;
    TZrUInt32 maxFunctionVersions;
    TZrUInt32 maxModuleVersions;
    TZrUInt32 maxFunctionBytes;
    TZrUInt32 maxModuleBytes;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (state == ZR_NULL || !zr_profile_policy_valid(policy) ||
        state->siteKey == 0u || megamorphic > ZR_TRUE) {
        zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, 0u);
        return ZR_FALSE;
    }
    maxVersions = zr_profile_effective_u32(
            policy->maxVersionsPerSite,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_VERSIONS_PER_SITE);
    maxBytes = zr_profile_effective_u32(
            policy->maxCodeBytesPerSite,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CODE_BYTES_PER_SITE);
    maxFunctionVersions = zr_profile_effective_u32(
            policy->maxVersionsPerFunction,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_VERSIONS_PER_FUNCTION);
    maxModuleVersions = zr_profile_effective_u32(
            policy->maxVersionsPerModule,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_VERSIONS_PER_MODULE);
    maxFunctionBytes = zr_profile_effective_u32(
            policy->maxCodeBytesPerFunction,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CODE_BYTES_PER_FUNCTION);
    maxModuleBytes = zr_profile_effective_u32(
            policy->maxCodeBytesPerModule,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CODE_BYTES_PER_MODULE);
    if (state->versions >= maxVersions || codeBytes > maxBytes ||
        codeBytes > UINT32_MAX - state->codeBytes ||
        state->codeBytes + codeBytes > maxBytes ||
        (totals != ZR_NULL &&
         (totals->functionVersions >= maxFunctionVersions ||
          totals->moduleVersions >= maxModuleVersions ||
          codeBytes > UINT32_MAX - totals->functionCodeBytes ||
          codeBytes > UINT32_MAX - totals->moduleCodeBytes ||
          totals->functionCodeBytes + codeBytes > maxFunctionBytes ||
          totals->moduleCodeBytes + codeBytes > maxModuleBytes))) {
        zr_profile_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                              maxBytes, codeBytes);
        return ZR_FALSE;
    }
    state->versions++;
    state->codeBytes += codeBytes;
    state->distinctTargets = distinctTargets;
    state->megamorphic = megamorphic;
    if (megamorphic) state->disabled = ZR_TRUE;
    if (totals != ZR_NULL) {
        totals->functionVersions++;
        totals->moduleVersions++;
        totals->functionCodeBytes += codeBytes;
        totals->moduleCodeBytes += codeBytes;
    }
    return ZR_TRUE;
}

static TZrBool zr_profile_rate_below(TZrUInt64 numerator,
                                     TZrUInt64 denominator,
                                     TZrUInt32 thresholdPermille) {
    long double left;
    long double right;
    if (denominator == 0u) return ZR_TRUE;
    left = (long double)numerator / (long double)denominator;
    right = (long double)thresholdPermille / 1000.0L;
    return (TZrBool)(left < right);
}

static TZrBool zr_profile_rate_above(TZrUInt64 numerator,
                                     TZrUInt64 denominator,
                                     TZrUInt32 thresholdPermille) {
    long double left;
    long double right;
    if (denominator == 0u) return ZR_FALSE;
    left = (long double)numerator / (long double)denominator;
    right = (long double)thresholdPermille / 1000.0L;
    return (TZrBool)(left > right);
}

TZrBool ZrParser_ExecIr_SpecializationDecide(
        const SZrExecIrSpecializationState *state,
        const SZrExecIrSpecializationPolicy *policy,
        TZrUInt32 requestedCodeBytes, TZrUInt32 requestedTargets,
        SZrExecIrSpecializationDecision *decision,
        SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_SpecializationDecideEx(
            state, policy, ZR_NULL, requestedCodeBytes, requestedTargets,
            decision, diagnostic);
}

TZrBool ZrParser_ExecIr_SpecializationDecideEx(
        const SZrExecIrSpecializationState *state,
        const SZrExecIrSpecializationPolicy *policy,
        const SZrExecIrSpecializationTotals *totals,
        TZrUInt32 requestedCodeBytes, TZrUInt32 requestedTargets,
        SZrExecIrSpecializationDecision *decision,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt64 samples;
    TZrUInt32 maxVersions;
    TZrUInt32 maxBytes;
    TZrUInt32 maxTargets;
    TZrUInt32 maxFunctionVersions;
    TZrUInt32 maxModuleVersions;
    TZrUInt32 maxFunctionBytes;
    TZrUInt32 maxModuleBytes;
    TZrUInt64 minSamples;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (decision != ZR_NULL) {
        memset(decision, 0, sizeof(*decision));
        decision->retainBaseline = ZR_TRUE;
    }
    if (state == ZR_NULL || !zr_profile_policy_valid(policy) ||
        decision == ZR_NULL || state->siteKey == 0u) {
        zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, 0u);
        return ZR_FALSE;
    }
    maxVersions = zr_profile_effective_u32(
            policy->maxVersionsPerSite,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_VERSIONS_PER_SITE);
    maxBytes = zr_profile_effective_u32(
            policy->maxCodeBytesPerSite,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CODE_BYTES_PER_SITE);
    maxTargets = zr_profile_effective_u32(
            policy->maxPolymorphicTargets,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_POLYMORPHIC_TARGETS);
    maxFunctionVersions = zr_profile_effective_u32(
            policy->maxVersionsPerFunction,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_VERSIONS_PER_FUNCTION);
    maxModuleVersions = zr_profile_effective_u32(
            policy->maxVersionsPerModule,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_VERSIONS_PER_MODULE);
    maxFunctionBytes = zr_profile_effective_u32(
            policy->maxCodeBytesPerFunction,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CODE_BYTES_PER_FUNCTION);
    maxModuleBytes = zr_profile_effective_u32(
            policy->maxCodeBytesPerModule,
            ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CODE_BYTES_PER_MODULE);
    minSamples = zr_profile_effective_u64(
            policy->minSamples, ZR_EXEC_IR_PROFILE_DEFAULT_MIN_SAMPLES);
    samples = state->hitCount;
    if (state->missCount > UINT64_MAX - samples) samples = UINT64_MAX;
    else samples += state->missCount;
    if (state->cooldownRemaining != 0u || state->disabled) {
        decision->reason = ZR_EXEC_IR_SPECIALIZATION_REASON_COOLDOWN;
        decision->enterCooldown = ZR_TRUE;
        return ZR_TRUE;
    }
    if (state->megamorphic ||
        (requestedTargets != 0u ? requestedTargets : state->distinctTargets) > maxTargets) {
        decision->reason = ZR_EXEC_IR_SPECIALIZATION_REASON_MEGAMORPHIC;
        return ZR_TRUE;
    }
    if (samples < minSamples) {
        decision->reason = ZR_EXEC_IR_SPECIALIZATION_REASON_COLD;
        return ZR_TRUE;
    }
    if (zr_profile_rate_below(state->hitCount, samples,
                              policy->minHitRatePermille) ||
        zr_profile_rate_above(state->missCount, samples,
                              policy->maxMissRatePermille)) {
        decision->reason = ZR_EXEC_IR_SPECIALIZATION_REASON_LOW_HIT_RATE;
        return ZR_TRUE;
    }
    if (state->consecutiveDeopts >= zr_profile_effective_u32(
                policy->maxConsecutiveDeopts,
                ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CONSECUTIVE_DEOPTS)) {
        decision->reason = ZR_EXEC_IR_SPECIALIZATION_REASON_DEOPT_LIMIT;
        return ZR_TRUE;
    }
    if (state->versions >= maxVersions) {
        decision->reason = ZR_EXEC_IR_SPECIALIZATION_REASON_VERSION_LIMIT;
        return ZR_TRUE;
    }
    if (totals != ZR_NULL &&
        (totals->functionVersions >= maxFunctionVersions ||
         totals->moduleVersions >= maxModuleVersions)) {
        decision->reason = ZR_EXEC_IR_SPECIALIZATION_REASON_VERSION_LIMIT;
        return ZR_TRUE;
    }
    if (requestedCodeBytes > maxBytes ||
        requestedCodeBytes > UINT32_MAX - state->codeBytes ||
        state->codeBytes + requestedCodeBytes > maxBytes) {
        decision->reason = ZR_EXEC_IR_SPECIALIZATION_REASON_SIZE_LIMIT;
        return ZR_TRUE;
    }
    if (totals != ZR_NULL &&
        (requestedCodeBytes > UINT32_MAX - totals->functionCodeBytes ||
         requestedCodeBytes > UINT32_MAX - totals->moduleCodeBytes ||
         totals->functionCodeBytes + requestedCodeBytes > maxFunctionBytes ||
         totals->moduleCodeBytes + requestedCodeBytes > maxModuleBytes)) {
        decision->reason = ZR_EXEC_IR_SPECIALIZATION_REASON_SIZE_LIMIT;
        return ZR_TRUE;
    }
    decision->reason = ZR_EXEC_IR_SPECIALIZATION_REASON_NONE;
    decision->useSpecialized = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_SpecializationTick(
        SZrExecIrSpecializationState *state, TZrUInt32 samples,
        SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (state == ZR_NULL) {
        zr_profile_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, 0u);
        return ZR_FALSE;
    }
    if (samples >= state->cooldownRemaining) {
        state->cooldownRemaining = 0u;
        if (!state->megamorphic) {
            state->disabled = ZR_FALSE;
            state->consecutiveDeopts = 0u;
        }
    } else {
        state->cooldownRemaining -= samples;
    }
    return ZR_TRUE;
}

const TZrChar *ZrParser_ExecIr_ProfileReasonName(EZrExecIrProfileReason reason) {
    switch (reason) {
        case ZR_EXEC_IR_PROFILE_REASON_NONE: return "none";
        case ZR_EXEC_IR_PROFILE_REASON_SCHEMA: return "schema";
        case ZR_EXEC_IR_PROFILE_REASON_MODULE_IDENTITY: return "module-identity";
        case ZR_EXEC_IR_PROFILE_REASON_MODULE_HASH: return "module-hash";
        case ZR_EXEC_IR_PROFILE_REASON_IR_HASH: return "ir-hash";
        case ZR_EXEC_IR_PROFILE_REASON_SIGNATURE_HASH: return "signature-hash";
        case ZR_EXEC_IR_PROFILE_REASON_LAYOUT_HASH: return "layout-hash";
        case ZR_EXEC_IR_PROFILE_REASON_COMPILER_ABI: return "compiler-abi";
        case ZR_EXEC_IR_PROFILE_REASON_SITE: return "site";
        case ZR_EXEC_IR_PROFILE_REASON_DUPLICATE_SITE: return "duplicate-site";
        case ZR_EXEC_IR_PROFILE_REASON_COUNTER_OVERFLOW: return "counter-overflow";
        case ZR_EXEC_IR_PROFILE_REASON_MISSING_FIELD: return "missing-field";
        default: return "invalid";
    }
}

const TZrChar *ZrParser_ExecIr_ProfileImportStatusName(
        EZrExecIrProfileImportStatus status) {
    switch (status) {
        case ZR_EXEC_IR_PROFILE_IMPORT_ACCEPTED: return "accepted";
        case ZR_EXEC_IR_PROFILE_IMPORT_STALE: return "stale";
        case ZR_EXEC_IR_PROFILE_IMPORT_MISSING_KEY: return "missing-key";
        case ZR_EXEC_IR_PROFILE_IMPORT_INVALID:
        default: return "invalid";
    }
}

const TZrChar *ZrParser_ExecIr_SpecializationReasonName(
        EZrExecIrSpecializationReason reason) {
    switch (reason) {
        case ZR_EXEC_IR_SPECIALIZATION_REASON_NONE: return "none";
        case ZR_EXEC_IR_SPECIALIZATION_REASON_COLD: return "cold";
        case ZR_EXEC_IR_SPECIALIZATION_REASON_LOW_HIT_RATE: return "low-hit-rate";
        case ZR_EXEC_IR_SPECIALIZATION_REASON_MEGAMORPHIC: return "megamorphic";
        case ZR_EXEC_IR_SPECIALIZATION_REASON_VERSION_LIMIT: return "version-limit";
        case ZR_EXEC_IR_SPECIALIZATION_REASON_SIZE_LIMIT: return "size-limit";
        case ZR_EXEC_IR_SPECIALIZATION_REASON_DEOPT_LIMIT: return "deopt-limit";
        case ZR_EXEC_IR_SPECIALIZATION_REASON_COOLDOWN: return "cooldown";
        case ZR_EXEC_IR_SPECIALIZATION_REASON_INVALID:
        default: return "invalid";
    }
}
