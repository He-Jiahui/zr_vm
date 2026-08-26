#ifndef ZR_VM_PARSER_VARIANCE_H
#define ZR_VM_PARSER_VARIANCE_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/diagnostic_builder.h"

typedef enum EZrVarianceContextKind {
    ZR_VARIANCE_CONTEXT_FIELD = 0,
    ZR_VARIANCE_CONTEXT_PARAMETER,
    ZR_VARIANCE_CONTEXT_RETURN,
    ZR_VARIANCE_CONTEXT_PROPERTY,
    ZR_VARIANCE_CONTEXT_GETTER,
    ZR_VARIANCE_CONTEXT_SETTER,
} EZrVarianceContextKind;

typedef struct SZrVarianceViolation {
    SZrAstNode *node;
    SZrString *parameterName;
    EZrGenericVariance declaredVariance;
    EZrVarianceContextKind contextKind;
    TZrBool nestedUsage;
    SZrFileRange location;
    SZrFileRange declarationRange;
} SZrVarianceViolation;

ZR_PARSER_API TZrBool ZrParser_Variance_InterfaceViolationAt(
        SZrCompilerState *compilerState,
        const SZrAstNode *interfaceNode,
        TZrSize violationIndex,
        SZrVarianceViolation *outViolation);
ZR_PARSER_API TZrBool ZrParser_Variance_BuildDiagnostic(
        SZrState *state,
        const SZrVarianceViolation *violation,
        SZrStructuredDiagnostic *outDiagnostic);

#endif
