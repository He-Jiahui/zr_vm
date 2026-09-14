#ifndef ZR_VM_PARSER_EXEC_IR_PROFILE_H
#define ZR_VM_PARSER_EXEC_IR_PROFILE_H

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"

/* Stable, address-free profile schema for loop and specialization policy. */
#define ZR_EXEC_IR_PROFILE_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_EXEC_IR_PROFILE_MAGIC ((TZrUInt32)0x50524631u)
#define ZR_EXEC_IR_PROFILE_DEFAULT_MAX_VERSIONS_PER_SITE ((TZrUInt32)4u)
#define ZR_EXEC_IR_PROFILE_DEFAULT_MAX_VERSIONS_PER_FUNCTION ((TZrUInt32)16u)
#define ZR_EXEC_IR_PROFILE_DEFAULT_MAX_VERSIONS_PER_MODULE ((TZrUInt32)64u)
#define ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CODE_BYTES_PER_SITE ((TZrUInt32)4096u)
#define ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CODE_BYTES_PER_FUNCTION ((TZrUInt32)16384u)
#define ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CODE_BYTES_PER_MODULE ((TZrUInt32)65536u)
#define ZR_EXEC_IR_PROFILE_DEFAULT_MIN_SAMPLES ((TZrUInt64)32u)
#define ZR_EXEC_IR_PROFILE_DEFAULT_MIN_HIT_RATE_PERMILLE ((TZrUInt32)700u)
#define ZR_EXEC_IR_PROFILE_DEFAULT_MAX_MISS_RATE_PERMILLE ((TZrUInt32)300u)
#define ZR_EXEC_IR_PROFILE_DEFAULT_MAX_CONSECUTIVE_DEOPTS ((TZrUInt32)3u)
#define ZR_EXEC_IR_PROFILE_DEFAULT_COOLDOWN_SAMPLES ((TZrUInt32)16u)
#define ZR_EXEC_IR_PROFILE_DEFAULT_MAX_POLYMORPHIC_TARGETS ((TZrUInt32)4u)

typedef enum EZrExecIrProfileSiteKind {
    ZR_EXEC_IR_PROFILE_SITE_CALL_TARGET = 0,
    ZR_EXEC_IR_PROFILE_SITE_RECEIVER,
    ZR_EXEC_IR_PROFILE_SITE_BRANCH,
    ZR_EXEC_IR_PROFILE_SITE_NUMERIC_TYPE,
    ZR_EXEC_IR_PROFILE_SITE_ALLOCATION,
    ZR_EXEC_IR_PROFILE_SITE_BOXING,
    ZR_EXEC_IR_PROFILE_SITE_CACHE,
    ZR_EXEC_IR_PROFILE_SITE_DEOPT,
    ZR_EXEC_IR_PROFILE_SITE_GC,
    ZR_EXEC_IR_PROFILE_SITE_CONTENTION,
    ZR_EXEC_IR_PROFILE_SITE_KIND_COUNT
} EZrExecIrProfileSiteKind;

typedef enum EZrExecIrProfileImportStatus {
    ZR_EXEC_IR_PROFILE_IMPORT_ACCEPTED = 0,
    ZR_EXEC_IR_PROFILE_IMPORT_STALE,
    ZR_EXEC_IR_PROFILE_IMPORT_MISSING_KEY,
    ZR_EXEC_IR_PROFILE_IMPORT_INVALID
} EZrExecIrProfileImportStatus;

typedef enum EZrExecIrProfileReason {
    ZR_EXEC_IR_PROFILE_REASON_NONE = 0,
    ZR_EXEC_IR_PROFILE_REASON_SCHEMA,
    ZR_EXEC_IR_PROFILE_REASON_MODULE_IDENTITY,
    ZR_EXEC_IR_PROFILE_REASON_MODULE_HASH,
    ZR_EXEC_IR_PROFILE_REASON_IR_HASH,
    ZR_EXEC_IR_PROFILE_REASON_SIGNATURE_HASH,
    ZR_EXEC_IR_PROFILE_REASON_LAYOUT_HASH,
    ZR_EXEC_IR_PROFILE_REASON_COMPILER_ABI,
    ZR_EXEC_IR_PROFILE_REASON_SITE,
    ZR_EXEC_IR_PROFILE_REASON_DUPLICATE_SITE,
    ZR_EXEC_IR_PROFILE_REASON_COUNTER_OVERFLOW,
    ZR_EXEC_IR_PROFILE_REASON_MISSING_FIELD
} EZrExecIrProfileReason;

typedef struct SZrExecIrProfileKey {
    TZrMetadataToken moduleIdentity;
    TZrUInt64 moduleHash;
    TZrUInt64 irHash;
    TZrUInt64 signatureHash;
    TZrUInt64 layoutHash;
    TZrUInt32 compilerAbi;
} SZrExecIrProfileKey;

typedef struct SZrExecIrProfileSite {
    TZrUInt64 siteKey;
    TZrMetadataToken functionToken;
    TZrExecIrInstructionId instructionId;
    TZrExecIrSourceId sourceId;
    EZrExecIrProfileSiteKind kind;
    TZrMetadataToken targetToken;
    TZrExecIrTypeToken receiverTypeToken;
    TZrExecIrTypeToken numericTypeToken;
    TZrUInt64 branchTaken;
    TZrUInt64 branchNotTaken;
    TZrUInt64 hitCount;
    TZrUInt64 missCount;
    TZrUInt64 allocationCount;
    TZrUInt64 allocationBytes;
    TZrUInt64 boxingCount;
    TZrUInt64 cacheHitCount;
    TZrUInt64 cacheMissCount;
    TZrUInt64 deoptCount;
    TZrUInt64 gcCount;
    TZrUInt64 contentionCount;
    TZrUInt32 distinctTargets;
    TZrBool megamorphic;
} SZrExecIrProfileSite;

typedef struct SZrExecIrSpecializationPolicy {
    TZrUInt32 maxVersionsPerSite;
    TZrUInt32 maxVersionsPerFunction;
    TZrUInt32 maxVersionsPerModule;
    TZrUInt32 maxCodeBytesPerSite;
    TZrUInt32 maxCodeBytesPerFunction;
    TZrUInt32 maxCodeBytesPerModule;
    TZrUInt64 minSamples;
    TZrUInt32 minHitRatePermille;
    TZrUInt32 maxMissRatePermille;
    TZrUInt32 maxConsecutiveDeopts;
    TZrUInt32 cooldownSamples;
    TZrUInt32 maxPolymorphicTargets;
} SZrExecIrSpecializationPolicy;

typedef SZrExecIrSpecializationPolicy SZrExecIrProfilePolicy;

typedef struct SZrExecIrProfile {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    SZrExecIrProfileKey key;
    SZrExecIrProfileSite *sites;
    TZrUInt32 siteCount;
    TZrUInt32 siteCapacity;
    SZrExecIrSpecializationPolicy policy;
} SZrExecIrProfile;

typedef struct SZrExecIrProfileImportResult {
    EZrExecIrProfileImportStatus status;
    EZrExecIrProfileReason reason;
    TZrBool accepted;
    TZrBool ignored;
    TZrUInt32 siteCount;
} SZrExecIrProfileImportResult;

typedef enum EZrExecIrSpecializationReason {
    ZR_EXEC_IR_SPECIALIZATION_REASON_NONE = 0,
    ZR_EXEC_IR_SPECIALIZATION_REASON_COLD,
    ZR_EXEC_IR_SPECIALIZATION_REASON_LOW_HIT_RATE,
    ZR_EXEC_IR_SPECIALIZATION_REASON_MEGAMORPHIC,
    ZR_EXEC_IR_SPECIALIZATION_REASON_VERSION_LIMIT,
    ZR_EXEC_IR_SPECIALIZATION_REASON_SIZE_LIMIT,
    ZR_EXEC_IR_SPECIALIZATION_REASON_DEOPT_LIMIT,
    ZR_EXEC_IR_SPECIALIZATION_REASON_COOLDOWN,
    ZR_EXEC_IR_SPECIALIZATION_REASON_INVALID
} EZrExecIrSpecializationReason;

typedef struct SZrExecIrSpecializationState {
    TZrUInt64 siteKey;
    TZrUInt32 versions;
    TZrUInt32 codeBytes;
    TZrUInt64 hitCount;
    TZrUInt64 missCount;
    TZrUInt64 deoptCount;
    TZrUInt32 consecutiveDeopts;
    TZrUInt32 cooldownRemaining;
    TZrUInt32 distinctTargets;
    TZrBool megamorphic;
    TZrBool disabled;
    TZrBool baselineAvailable;
} SZrExecIrSpecializationState;

/* Optional function/module ledger supplied by the owner of a profile cache.
 * Keeping the aggregate counters outside a site state prevents one site from
 * silently spending another site's global budget.  The ledger contains only
 * scalar counters and can be reset at a compilation epoch boundary. */
typedef struct SZrExecIrSpecializationTotals {
    TZrUInt32 functionVersions;
    TZrUInt32 moduleVersions;
    TZrUInt32 functionCodeBytes;
    TZrUInt32 moduleCodeBytes;
} SZrExecIrSpecializationTotals;

typedef struct SZrExecIrSpecializationDecision {
    EZrExecIrSpecializationReason reason;
    TZrBool useSpecialized;
    TZrBool retainBaseline;
    TZrBool enterCooldown;
} SZrExecIrSpecializationDecision;

ZR_PARSER_API void ZrParser_ExecIr_ProfileInit(SZrExecIrProfile *profile);
ZR_PARSER_API void ZrParser_ExecIr_ProfileFree(SZrExecIrProfile *profile);
ZR_PARSER_API TZrUInt64 ZrParser_ExecIr_ProfileModuleHash(
        const SZrExecIrModule *module);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ProfileBuildKey(
        const SZrExecIrModule *module, SZrExecIrProfileKey *key);
ZR_PARSER_API TZrUInt64 ZrParser_ExecIr_ProfileSiteKey(
        TZrMetadataToken functionToken,
        TZrExecIrInstructionId instructionId,
        TZrExecIrSourceId sourceId);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ProfileAddSite(
        SZrExecIrProfile *profile, const SZrExecIrProfileSite *site,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API const SZrExecIrProfileSite *ZrParser_ExecIr_ProfileSiteAt(
        const SZrExecIrProfile *profile, TZrUInt32 index);
ZR_PARSER_API const SZrExecIrProfileSite *ZrParser_ExecIr_ProfileFindSite(
        const SZrExecIrProfile *profile, TZrUInt64 siteKey);
ZR_PARSER_API TZrUInt64 ZrParser_ExecIr_ProfileHash(
        const SZrExecIrProfile *profile);
ZR_PARSER_API EZrExecIrProfileImportStatus ZrParser_ExecIr_ProfileValidate(
        const SZrExecIrModule *module, const SZrExecIrProfile *profile,
        EZrExecIrProfileReason *reason, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ProfileImport(
        const SZrExecIrModule *module, const SZrExecIrProfile *profile,
        SZrExecIrProfileImportResult *result, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ImportProfile(
        SZrExecIrModule *module, const SZrExecIrProfile *profile,
        SZrExecIrDiagnostic *diagnostic);

ZR_PARSER_API void ZrParser_ExecIr_SpecializationPolicyInit(
        SZrExecIrSpecializationPolicy *policy);
ZR_PARSER_API void ZrParser_ExecIr_SpecializationStateInit(
        SZrExecIrSpecializationState *state, TZrUInt64 siteKey);
ZR_PARSER_API void ZrParser_ExecIr_SpecializationStateFree(
        SZrExecIrSpecializationState *state);
ZR_PARSER_API TZrBool ZrParser_ExecIr_SpecializationObserve(
        SZrExecIrSpecializationState *state,
        const SZrExecIrSpecializationPolicy *policy,
        TZrBool hit, TZrBool deopt, TZrUInt32 codeBytes,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_SpecializationCommitVersion(
        SZrExecIrSpecializationState *state,
        const SZrExecIrSpecializationPolicy *policy,
        TZrUInt32 codeBytes, TZrUInt32 distinctTargets,
        TZrBool megamorphic, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_SpecializationCommitVersionEx(
        SZrExecIrSpecializationState *state,
        const SZrExecIrSpecializationPolicy *policy,
        SZrExecIrSpecializationTotals *totals,
        TZrUInt32 codeBytes, TZrUInt32 distinctTargets,
        TZrBool megamorphic, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_SpecializationDecide(
        const SZrExecIrSpecializationState *state,
        const SZrExecIrSpecializationPolicy *policy,
        TZrUInt32 requestedCodeBytes, TZrUInt32 requestedTargets,
        SZrExecIrSpecializationDecision *decision,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_SpecializationDecideEx(
        const SZrExecIrSpecializationState *state,
        const SZrExecIrSpecializationPolicy *policy,
        const SZrExecIrSpecializationTotals *totals,
        TZrUInt32 requestedCodeBytes, TZrUInt32 requestedTargets,
        SZrExecIrSpecializationDecision *decision,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_SpecializationTick(
        SZrExecIrSpecializationState *state,
        TZrUInt32 samples, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_ProfileReasonName(
        EZrExecIrProfileReason reason);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_ProfileImportStatusName(
        EZrExecIrProfileImportStatus status);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_SpecializationReasonName(
        EZrExecIrSpecializationReason reason);

/* Alternate spellings used by profile consumers. */
#define ZrParser_ExecIr_Profile_Init ZrParser_ExecIr_ProfileInit
#define ZrParser_ExecIr_Profile_Free ZrParser_ExecIr_ProfileFree
#define ZrParser_ExecIr_Profile_Hash ZrParser_ExecIr_ProfileHash
#define ZrParser_ExecIr_Profile_Import ZrParser_ExecIr_ProfileImport
#define ZrParser_ExecIr_Profile_Validate ZrParser_ExecIr_ProfileValidate
#define ZrParser_ExecIr_Specialization_Observe ZrParser_ExecIr_SpecializationObserve
#define ZrParser_ExecIr_Specialization_Decide ZrParser_ExecIr_SpecializationDecide
#define ZrParser_ExecIr_Specialization_Commit ZrParser_ExecIr_SpecializationCommitVersion

#endif /* ZR_VM_PARSER_EXEC_IR_PROFILE_H */
