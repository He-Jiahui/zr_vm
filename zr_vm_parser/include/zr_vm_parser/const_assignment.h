#ifndef ZR_VM_PARSER_CONST_ASSIGNMENT_H
#define ZR_VM_PARSER_CONST_ASSIGNMENT_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/diagnostic_builder.h"

typedef enum EZrConstAssignmentTargetKind {
    ZR_CONST_ASSIGNMENT_TARGET_NONE = 0,
    ZR_CONST_ASSIGNMENT_TARGET_LOCAL,
    ZR_CONST_ASSIGNMENT_TARGET_PARAMETER,
    ZR_CONST_ASSIGNMENT_TARGET_INSTANCE_FIELD,
    ZR_CONST_ASSIGNMENT_TARGET_STATIC_FIELD,
} EZrConstAssignmentTargetKind;

typedef struct SZrConstAssignmentResult {
    EZrConstAssignmentTargetKind targetKind;
    SZrString *targetName;
    SZrFileRange assignmentRange;
    SZrFileRange declarationRange;
    TZrBool isConstTarget;
    TZrBool isViolation;
} SZrConstAssignmentResult;

ZR_PARSER_API TZrBool ZrParser_ConstAssignment_DescribeTarget(
        const SZrAstNode *targetDeclaration,
        SZrConstAssignmentResult *outResult);
ZR_PARSER_API TZrBool ZrParser_ConstAssignment_Evaluate(
        const SZrAstNode *moduleRoot,
        const SZrAstNode *assignment,
        const SZrAstNode *targetDeclaration,
        SZrConstAssignmentResult *outResult);
ZR_PARSER_API TZrBool ZrParser_ConstAssignment_EvaluateContext(
        const SZrCompilerState *compilerState,
        const SZrAstNode *moduleRoot,
        const SZrAstNode *assignment,
        const SZrAstNode *resolvedTargetDeclaration,
        SZrConstAssignmentResult *outResult);
ZR_PARSER_API TZrBool ZrParser_ConstAssignment_BuildDiagnostic(
        SZrState *state,
        const SZrConstAssignmentResult *result,
        SZrStructuredDiagnostic *outDiagnostic);

#endif
