#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SYNTAX_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SYNTAX_H

#include "zr_vm_parser/type_inference.h"

SZrAstNode *ZrParser_TypeInferenceLoopAssignment_StatementExpression(SZrAstNode *statement);
TZrBool ZrParser_TypeInferenceLoopAssignment_StatementIsPlainBreak(SZrAstNode *statement);
TZrBool ZrParser_TypeInferenceLoopAssignment_StatementGuaranteesPlainBreak(SZrAstNode *statement);
TZrBool ZrParser_TypeInferenceLoopAssignment_AssignmentParts(SZrAstNode *assignmentNode,
                                                             SZrString **outName,
                                                             SZrAstNode **outRight);

#endif
