#ifndef ZR_VM_PARSER_DIAGNOSTIC_BUILDER_H
#define ZR_VM_PARSER_DIAGNOSTIC_BUILDER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

typedef enum EZrStructuredDiagnosticSeverity {
    ZR_STRUCTURED_DIAGNOSTIC_ERROR = 0,
    ZR_STRUCTURED_DIAGNOSTIC_WARNING,
    ZR_STRUCTURED_DIAGNOSTIC_INFO,
    ZR_STRUCTURED_DIAGNOSTIC_HINT
} EZrStructuredDiagnosticSeverity;

typedef struct SZrStructuredDiagnostic {
    EZrStructuredDiagnosticSeverity severity;
    SZrFileRange location;
    SZrString *code;
    SZrString *message;
    SZrString *cause;
    SZrString *suggestion;
} SZrStructuredDiagnostic;

ZR_PARSER_API void ZrParser_StructuredDiagnostic_Init(SZrStructuredDiagnostic *diagnostic);
ZR_PARSER_API void ZrParser_StructuredDiagnostic_Free(SZrState *state, SZrStructuredDiagnostic *diagnostic);

ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_Build(SZrState *state,
                                                       SZrStructuredDiagnostic *out,
                                                       EZrStructuredDiagnosticSeverity severity,
                                                       SZrFileRange location,
                                                       const TZrChar *code,
                                                       const TZrChar *message,
                                                       const TZrChar *cause,
                                                       const TZrChar *suggestion);

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
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingCallClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingParameterListClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingGroupClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildArrayElementAssignment(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingArrayClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingArrayElementSeparator(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectComputedKeyClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
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
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingConditionalAlternate(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingStatementSemicolon(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    const TZrChar *statementKind);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyOpen(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    const TZrChar *declarationKind);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingStatementBodyOpen(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location,
    const TZrChar *statementKind);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingBlockClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingCatchPatternClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingUsingResourceClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingForHeaderClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingForHeaderSeparator(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingForeachHeaderClose(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildMissingForeachInKeyword(
    SZrState *state,
    SZrStructuredDiagnostic *out,
    SZrFileRange location);
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
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildWeakUpgrade(SZrState *state,
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
