#ifndef ZR_VM_PARSER_DIAGNOSTIC_BUILDER_H
#define ZR_VM_PARSER_DIAGNOSTIC_BUILDER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

typedef enum EZrStructuredDiagnosticSeverity {
    ZR_STRUCTURED_DIAGNOSTIC_ERROR = 0,
    ZR_STRUCTURED_DIAGNOSTIC_WARNING,
    ZR_STRUCTURED_DIAGNOSTIC_INFO,
    ZR_STRUCTURED_DIAGNOSTIC_HINT
} EZrStructuredDiagnosticSeverity;

typedef struct SZrStructuredDiagnosticRelatedInformation {
    SZrFileRange location;
    SZrString *message;
} SZrStructuredDiagnosticRelatedInformation;

typedef enum EZrDiagnosticFixApplicability {
    ZR_DIAGNOSTIC_FIX_APPLICABILITY_UNKNOWN = 0,
    ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE,
    ZR_DIAGNOSTIC_FIX_HAS_PLACEHOLDERS,
    ZR_DIAGNOSTIC_FIX_MAYBE_INCORRECT
} EZrDiagnosticFixApplicability;

typedef struct SZrStructuredDiagnosticFix {
    SZrString *title;
    SZrFileRange editRange;
    SZrString *editText;
    EZrDiagnosticFixApplicability applicability;
} SZrStructuredDiagnosticFix;

typedef struct SZrStructuredDiagnostic {
    EZrStructuredDiagnosticSeverity severity;
    SZrFileRange location;
    SZrString *code;
    SZrString *message;
    SZrString *cause;
    SZrString *suggestion;
    SZrArray relatedInformation;
    SZrArray fixes;
    TZrUInt32 descriptorId;
} SZrStructuredDiagnostic;

ZR_PARSER_API void ZrParser_StructuredDiagnostic_Init(SZrStructuredDiagnostic *diagnostic);
ZR_PARSER_API void ZrParser_StructuredDiagnostic_Free(SZrState *state, SZrStructuredDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_StructuredDiagnostic_Copy(
        SZrState *state,
        SZrStructuredDiagnostic *outDiagnostic,
        const SZrStructuredDiagnostic *source);
ZR_PARSER_API TZrBool ZrParser_StructuredDiagnostic_AddRelatedInformation(SZrState *state,
                                                                          SZrStructuredDiagnostic *diagnostic,
                                                                          SZrFileRange location,
                                                                          const TZrChar *message);
ZR_PARSER_API TZrBool ZrParser_StructuredDiagnostic_AddFix(
        SZrState *state,
        SZrStructuredDiagnostic *diagnostic,
        const TZrChar *title,
        SZrFileRange editRange,
        const TZrChar *editText,
        EZrDiagnosticFixApplicability applicability);

ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_Build(SZrState *state,
                                                       SZrStructuredDiagnostic *out,
                                                       EZrStructuredDiagnosticSeverity severity,
                                                       SZrFileRange location,
                                                       const TZrChar *code,
                                                       const TZrChar *message,
                                                       const TZrChar *cause,
                                                       const TZrChar *suggestion);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildTypeMismatchDetailed(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        const TZrChar *expectedType,
        const TZrChar *actualType,
        const SZrFileRange *expectedTypeLocation,
        const TZrChar *conversionHint);

ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingExpressionAfterAssignment(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingRightOperand(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    const TZrChar *operatorText);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingCondition(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    const TZrChar *statementKind);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingConditionClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    const TZrChar *statementKind);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingMemberName(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingIndexClose(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        SZrFileRange fixLocation);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingCallClose(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        SZrFileRange fixLocation);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingParameterListClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingGroupClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildArrayElementAssignment(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingArrayClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingArrayElementSeparator(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectComputedKeyClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectPropertyColon(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectPropertySeparator(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingConditionalConsequent(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingConditionalColon(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation,
    TZrBool hasAlternateExpression);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingConditionalAlternate(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingStatementSemicolon(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation,
    const TZrChar *statementKind);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyOpen(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation,
    const TZrChar *declarationKind);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation,
    const TZrChar *declarationKind);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingStatementBodyOpen(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation,
    const TZrChar *statementKind);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingBlockClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingCatchPatternClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingUsingResourceClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildUsingBinderInvalid(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildImportPathNotConstant(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    const TZrChar *directiveName);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildPatternShapeMismatch(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    const TZrChar *message,
    const TZrChar *cause,
    const TZrChar *suggestion);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildPatternUnknownField(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    const TZrChar *fieldName,
    const TZrChar *availableFields);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildPatternArityMismatch(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    TZrSize expectedCount,
    TZrSize actualCount,
    const TZrChar *availableFields);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildPatternVariantMismatch(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    const TZrChar *annotationUnionName,
    const TZrChar *variantName,
    const TZrChar *resourceUnionName);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingForHeaderClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingForHeaderSeparator(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingForeachHeaderClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingForeachInKeyword(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    SZrFileRange fixLocation);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingSwitchCaseHeaderClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingSwitchBodyClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingExternSpecClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingTestNameClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildLegacyOwnershipTypeSyntaxWarning(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    const TZrChar *legacyQualifier,
    const TZrChar *wrapperName);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildLegacyPropertySyntax(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange declarationLocation,
    SZrFileRange nameLocation,
    const SZrFileRange *typeLocation,
    const SZrFileRange *bodyLocation,
    const TZrChar *replacementText);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildWeakUpgrade(SZrState *state,
                                                                  SZrStructuredDiagnostic *out,
                                                                  SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildUseAfterMove(SZrState *state,
                                                                   SZrStructuredDiagnostic *out,
                                                                   SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildBorrowEscape(SZrState *state,
                                                                   SZrStructuredDiagnostic *out,
                                                                   SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildLoanEscape(SZrState *state,
                                                                 SZrStructuredDiagnostic *out,
                                                                 SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildOwnerToPlainEscape(SZrState *state,
                                                                         SZrStructuredDiagnostic *out,
                                                                         SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildOwnershipMismatch(SZrState *state,
                                                                       SZrStructuredDiagnostic *out,
                                                                       SZrFileRange location,
                                                                       const TZrChar *expectedType,
                                                                       const TZrChar *actualType);

#endif
