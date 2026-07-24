#ifndef ZR_VM_PARSER_LEGACY_MIGRATION_H
#define ZR_VM_PARSER_LEGACY_MIGRATION_H

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

typedef enum EZrLegacyMigrationApplicability {
    ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE = 0,
    ZR_LEGACY_MIGRATION_MAYBE_INCORRECT,
    ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
    ZR_LEGACY_MIGRATION_BLOCKED,
    ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED
} EZrLegacyMigrationApplicability;

typedef struct SZrLegacyMigrationItem {
    SZrString *diagnosticCode;
    SZrString *oldConstructKind;
    SZrString *targetConstructKind;
    SZrString *oldTargetBindingKind;
    SZrString *targetPlanId;
    SZrString *reason;
    SZrFileRange range;
    SZrFileRange relatedRange;
    EZrLegacyMigrationApplicability applicability;
    TZrTypeId resolvedTargetTypeId;
    TZrBool hasResolvedTargetTypeId;
    SZrStructuredDiagnosticFix fix;
    TZrBool hasFix;
} SZrLegacyMigrationItem;

typedef struct SZrLegacyMigrationPlan {
    SZrArray items; /* SZrLegacyMigrationItem */
    TZrUInt64 sourceHash;
    TZrBool hasOverlap;
} SZrLegacyMigrationPlan;

ZR_PARSER_API TZrBool ZrParser_LegacyMigration_PlanSource(
        SZrState *state,
        const TZrChar *source,
        TZrSize sourceLength,
        SZrString *sourceName,
        SZrLegacyMigrationPlan *outPlan);
ZR_PARSER_API void ZrParser_LegacyMigration_PlanFree(
        SZrState *state,
        SZrLegacyMigrationPlan *plan);
ZR_PARSER_API TZrBool ZrParser_LegacyMigration_ApplyMachineEdits(
        SZrState *state,
        const SZrLegacyMigrationPlan *plan,
        const TZrChar *source,
        TZrSize sourceLength,
        TZrChar **outText,
        TZrSize *outLength);

#endif /* ZR_VM_PARSER_LEGACY_MIGRATION_H */
