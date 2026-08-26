#ifndef ZR_VM_PARSER_INTERFACE_CONTRACT_H
#define ZR_VM_PARSER_INTERFACE_CONTRACT_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/diagnostic_builder.h"

typedef enum EZrInterfaceConstFieldViolationKind {
    ZR_INTERFACE_CONST_FIELD_MISSING = 0,
    ZR_INTERFACE_CONST_FIELD_DROPS_CONST,
} EZrInterfaceConstFieldViolationKind;

typedef struct SZrInterfaceConstFieldViolation {
    SZrAstNode *node;
    SZrString *fieldName;
    EZrInterfaceConstFieldViolationKind kind;
    SZrFileRange location;
    SZrFileRange requiredDeclarationRange;
} SZrInterfaceConstFieldViolation;

ZR_PARSER_API TZrBool ZrParser_InterfaceContract_ConstFieldViolationAt(
        SZrCompilerState *compilerState,
        const SZrAstNode *classNode,
        TZrSize violationIndex,
        SZrInterfaceConstFieldViolation *outViolation);
ZR_PARSER_API TZrBool ZrParser_InterfaceContract_BuildConstFieldDiagnostic(
        SZrState *state,
        const SZrInterfaceConstFieldViolation *violation,
        SZrStructuredDiagnostic *outDiagnostic);

#endif
