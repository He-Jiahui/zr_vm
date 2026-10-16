#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_DELTA_PRODUCT_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_DELTA_PRODUCT_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"

typedef TZrBool (*FZrLoopAssignmentDeltaProductTermEqual)(SZrAstNode *left,
                                                         SZrAstNode *right,
                                                         SZrString *targetName);

TZrBool ZrParser_TypeInferenceLoopAssignment_DeltaProductTermsEqual(
        SZrAstNode *left,
        SZrAstNode *right,
        SZrString *targetName,
        FZrLoopAssignmentDeltaProductTermEqual termEqual);

#endif
