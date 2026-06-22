#ifndef ZR_VM_PARSER_TYPE_INFERENCE_SEMANTIC_FACTS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_SEMANTIC_FACTS_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/semantic_facts.h"

void type_inference_record_expression_fact(SZrCompilerState *cs,
                                           SZrAstNode *node,
                                           const SZrInferredType *type);

void type_inference_record_numeric_fact(SZrCompilerState *cs,
                                        SZrAstNode *node,
                                        const SZrInferredType *type,
                                        EZrSemanticNumericFactKind kind);

void type_inference_record_expression_and_numeric_facts(SZrCompilerState *cs,
                                                        SZrAstNode *node,
                                                        const SZrInferredType *type,
                                                        EZrSemanticNumericFactKind numericKind);

void type_inference_record_interval_comparison_logical_fact(SZrCompilerState *cs,
                                                            SZrAstNode *node);

TZrBool type_inference_logical_fact_known_bool_value(SZrCompilerState *cs,
                                                     SZrAstNode *node,
                                                     TZrBool *outValue,
                                                     SZrAstNode **outEvidenceNode);

void type_inference_record_primary_call_reference_fact(SZrCompilerState *cs,
                                                       SZrAstNode *node,
                                                       SZrAstNode *callNode,
                                                       const SZrFunctionTypeInfo *funcTypeInfo);

void type_inference_record_identifier_write_reference_fact(SZrCompilerState *cs,
                                                           SZrAstNode *node,
                                                           const SZrTypeBinding *binding);

void type_inference_record_member_access_reference_fact(SZrCompilerState *cs,
                                                        SZrAstNode *node);

void type_inference_record_member_write_reference_fact(SZrCompilerState *cs,
                                                       SZrAstNode *node);

void type_inference_record_array_index_bounds_diagnostic_fact(SZrCompilerState *cs,
                                                              SZrAstNode *memberNode,
                                                              const SZrInferredType *elementType,
                                                              TZrInt64 indexValue,
                                                              TZrSize arraySize,
                                                              TZrBool hasFixedSize);

void type_inference_record_array_index_range_bounds_diagnostic_fact(SZrCompilerState *cs,
                                                                    SZrAstNode *memberNode,
                                                                    const SZrInferredType *elementType,
                                                                    TZrInt64 minValue,
                                                                    TZrInt64 maxValue,
                                                                    TZrSize arraySize,
                                                                    TZrBool hasFixedSize);

void type_inference_record_array_index_possible_range_bounds_diagnostic_fact(SZrCompilerState *cs,
                                                                             SZrAstNode *memberNode,
                                                                             const SZrInferredType *elementType,
                                                                             TZrInt64 minValue,
                                                                             TZrInt64 maxValue,
                                                                             TZrSize arraySize,
                                                                             TZrBool hasFixedSize);

void type_inference_record_array_index_type_mismatch_diagnostic_fact(
        SZrCompilerState *cs,
        SZrAstNode *memberNode,
        const SZrInferredType *elementType,
        const SZrInferredType *indexType);

void type_inference_record_constant_conditional_branch_facts(SZrCompilerState *cs,
                                                             SZrAstNode *node,
                                                             TZrBool conditionValue);

void type_inference_apply_literal_numeric_range(SZrAstNode *node,
                                                SZrInferredType *type);

void type_inference_apply_primitive_numeric_range(SZrInferredType *type);

void type_inference_apply_binary_numeric_range(SZrState *state,
                                               const TZrChar *op,
                                               const SZrInferredType *leftType,
                                               const SZrInferredType *rightType,
                                               SZrInferredType *result);

void type_inference_apply_unary_numeric_range(SZrState *state,
                                              const TZrChar *op,
                                              const SZrInferredType *operandType,
                                              SZrInferredType *result);

void type_inference_record_ownership_builtin_fact(SZrCompilerState *cs,
                                                  SZrAstNode *node,
                                                  EZrOwnershipBuiltinKind builtinKind,
                                                  EZrOwnershipQualifier qualifier);

#endif
