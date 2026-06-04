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
