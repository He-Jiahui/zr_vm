#ifndef ZR_VM_PARSER_TYPE_INFERENCE_INTERNAL_H
#define ZR_VM_PARSER_TYPE_INFERENCE_INTERNAL_H

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic_facts.h"

typedef struct SZrGenericCallBinding {
    const SZrTypeGenericParameterInfo *parameterInfo;
    TZrBool isBound;
    SZrInferredType inferredType;
} SZrGenericCallBinding;

typedef enum EZrGenericCallResolveStatus {
    ZR_GENERIC_CALL_RESOLVE_OK = 0,
    ZR_GENERIC_CALL_RESOLVE_ARITY_MISMATCH,
    ZR_GENERIC_CALL_RESOLVE_KIND_MISMATCH,
    ZR_GENERIC_CALL_RESOLVE_CANNOT_INFER,
    ZR_GENERIC_CALL_RESOLVE_CONFLICTING_INFERENCE,
    ZR_GENERIC_CALL_RESOLVE_CONFLICT
} EZrGenericCallResolveStatus;

const TZrChar *get_base_type_name(EZrValueType baseType);
void ZrParser_TypeError_Report(SZrCompilerState *cs,
                               const TZrChar *message,
                               const SZrInferredType *expectedType,
                               const SZrInferredType *actualType,
                               SZrFileRange location);
ZR_PARSER_API void free_inferred_type_array(SZrState *state, SZrArray *types);
TZrBool zr_string_equals_cstr(SZrString *value, const TZrChar *literal);
ZR_PARSER_API SZrTypePrototypeInfo *find_compiler_type_prototype_inference(SZrCompilerState *cs, SZrString *typeName);
SZrTypeMemberInfo *find_compiler_type_member_inference(SZrCompilerState *cs,
                                                       SZrString *typeName,
                                                       SZrString *memberName);
TZrBool find_compiler_type_member_call_inference(SZrCompilerState *cs,
                                                 SZrString *typeName,
                                                 SZrString *memberName,
                                                 SZrFunctionCall *call,
                                                 SZrFileRange location,
                                                 SZrTypeMemberInfo **outMember);
ZR_PARSER_API TZrBool type_name_is_module_prototype_inference(SZrCompilerState *cs, SZrString *typeName);
ZR_PARSER_API TZrBool inferred_type_from_type_name(SZrCompilerState *cs, SZrString *typeName, SZrInferredType *result);
ZR_PARSER_API TZrBool inferred_type_implements_protocol_mask(SZrCompilerState *cs,
                                                             const SZrInferredType *type,
                                                             TZrUInt64 protocolMask);
ZR_PARSER_API TZrBool infer_member_call_contract_return_type(SZrCompilerState *cs,
                                                             const SZrTypeMemberInfo *memberInfo,
                                                             SZrFunctionCall *call,
                                                             SZrInferredType *result);
ZR_PARSER_API TZrBool type_name_is_explicitly_available_in_context_inference(SZrCompilerState *cs,
                                                                              SZrString *typeName);
TZrBool inferred_type_try_map_primitive_name(const TZrNativeString nameStr,
                                             TZrSize nameLen,
                                             EZrValueType *outBaseType);
ZR_PARSER_API TZrBool try_parse_generic_instance_type_name(SZrState *state,
                                                           SZrString *typeName,
                                                           SZrString **outBaseName,
                                                           SZrArray *outArgumentTypeNames);
ZR_PARSER_API TZrBool ensure_generic_instance_type_prototype(SZrCompilerState *cs, SZrString *typeName);
ZR_PARSER_API SZrString *build_generic_instance_name(SZrState *state,
                                                     SZrString *baseName,
                                                     const SZrArray *typeArguments);
ZR_PARSER_API EZrGenericCallResolveStatus validate_generic_call_bindings_constraints(
        SZrCompilerState *cs,
        const SZrArray *bindings,
        TZrChar *diagnosticBuffer,
        TZrSize diagnosticBufferSize);
ZR_PARSER_API EZrGenericCallResolveStatus resolve_generic_constructed_type_name(
        SZrCompilerState *cs,
        SZrString *baseTypeName,
        const SZrArray *genericParameters,
        const SZrArray *parameterTypes,
        SZrAstNodeArray *arguments,
        SZrString **outTypeName,
        TZrChar *diagnosticBuffer,
        TZrSize diagnosticBufferSize);
TZrBool infer_function_call_argument_types_for_candidate(SZrCompilerState *cs,
                                                         SZrTypeEnvironment *env,
                                                         SZrString *funcName,
                                                         SZrFunctionCall *call,
                                                         const SZrFunctionTypeInfo *funcType,
                                                         SZrArray *argTypes,
                                                         TZrBool *mismatch);
TZrBool validate_call_argument_passing_modes(SZrCompilerState *cs,
                                             const SZrArray *parameterPassingModes,
                                             const SZrArray *parameterTypes,
                                             SZrFunctionCall *call,
                                             const SZrArray *argTypes);
const TZrChar *type_inference_ownership_flow_diagnostic_message(const SZrInferredType *targetType,
                                                                const SZrInferredType *sourceType);
ZR_PARSER_API void free_resolved_call_signature(SZrState *state, SZrResolvedCallSignature *signature);
EZrGenericCallResolveStatus resolve_generic_function_call_signature_detailed(
        SZrCompilerState *cs,
        SZrTypeEnvironment *env,
        SZrString *funcName,
        const SZrFunctionTypeInfo *funcType,
        SZrFunctionCall *call,
        SZrResolvedCallSignature *signature,
        TZrChar *diagnosticBuffer,
        TZrSize diagnosticBufferSize);
TZrBool resolve_generic_function_call_signature(SZrCompilerState *cs,
                                                const SZrFunctionTypeInfo *funcType,
                                                SZrFunctionCall *call,
                                                SZrResolvedCallSignature *signature);
ZR_PARSER_API EZrGenericCallResolveStatus resolve_generic_member_call_signature_detailed(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        SZrFunctionCall *call,
        SZrResolvedCallSignature *signature,
        TZrChar *diagnosticBuffer,
        TZrSize diagnosticBufferSize);
TZrBool resolve_generic_member_call_signature(SZrCompilerState *cs,
                                              const SZrTypeMemberInfo *memberInfo,
                                              SZrFunctionCall *call,
                                              SZrResolvedCallSignature *signature);
ZR_PARSER_API TZrBool resolve_best_function_overload(SZrCompilerState *cs,
                                                     SZrTypeEnvironment *env,
                                                     SZrString *funcName,
                                                     SZrFunctionCall *call,
                                                     SZrFileRange location,
                                                     SZrFunctionTypeInfo **resolvedFunction,
                                                     SZrResolvedCallSignature *resolvedSignature);
TZrBool inferred_type_can_use_named_constraint_fallback(SZrCompilerState *cs,
                                                        const SZrInferredType *actualType,
                                                        const SZrInferredType *expectedType);
ZR_PARSER_API TZrBool resolve_prototype_target_inference(SZrCompilerState *cs,
                                                         SZrAstNode *node,
                                                         SZrTypePrototypeInfo **outPrototype,
                                                         SZrString **outTypeName);
ZR_PARSER_API TZrBool resolve_source_type_declaration_target_inference(SZrCompilerState *cs,
                                                                       SZrAstNode *node,
                                                                       SZrString **outTypeName,
                                                                       EZrObjectPrototypeType *outPrototypeType,
                                                                       TZrBool *outAllowValueConstruction,
                                                                       TZrBool *outAllowBoxedConstruction);
TZrBool infer_prototype_reference_type(SZrCompilerState *cs,
                                       SZrAstNode *node,
                                       SZrInferredType *result);
ZR_PARSER_API TZrBool infer_construct_expression_type(SZrCompilerState *cs,
                                                      SZrAstNode *node,
                                                      SZrInferredType *result);
TZrBool bind_foreach_element_type_from_inferred_iterable(SZrCompilerState *cs,
                                                         const SZrInferredType *iterableType,
                                                         SZrInferredType *outType);
const TZrChar *receiver_ownership_call_error(EZrOwnershipQualifier receiverQualifier);
SZrString *extract_imported_module_name(SZrFunctionCall *call);
TZrBool ensure_import_module_compile_info(SZrCompilerState *cs, SZrString *moduleName);
TZrBool ensure_native_module_compile_info(SZrCompilerState *cs, SZrString *moduleName);
ZR_PARSER_API void ensure_builtin_reflection_compile_type(SZrCompilerState *cs, SZrString *typeName);
TZrBool infer_import_expression_type(SZrCompilerState *cs,
                                     SZrAstNode *node,
                                     SZrInferredType *result);
TZrBool infer_type_query_expression_type(SZrCompilerState *cs,
                                         SZrAstNode *node,
                                         SZrInferredType *result);
TZrBool infer_primary_member_chain_type(SZrCompilerState *cs,
                                        const SZrInferredType *baseType,
                                        SZrAstNodeArray *members,
                                        TZrSize startIndex,
                                        TZrBool baseIsPrototypeReference,
                                        SZrInferredType *result);
TZrBool infer_ffi_member_call_type(SZrCompilerState *cs,
                                   const SZrInferredType *receiverType,
                                   const SZrTypeMemberInfo *memberInfo,
                                   SZrFunctionCall *call,
                                   SZrInferredType *result,
                                   TZrBool *outHandled);
TZrBool ffi_function_call_argument_is_native_boundary_compatible(SZrCompilerState *cs,
                                                                 const SZrFunctionTypeInfo *funcType,
                                                                 TZrSize parameterIndex,
                                                                 const SZrInferredType *argType,
                                                                 const SZrInferredType *paramType);
TZrBool resolve_compile_time_array_size(SZrCompilerState *cs,
                                        const SZrType *astType,
                                        TZrSize *resolvedSize);
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
void type_inference_record_primary_call_reference_fact(SZrCompilerState *cs,
                                                       SZrAstNode *node,
                                                       SZrAstNode *callNode,
                                                       const SZrFunctionTypeInfo *funcTypeInfo);
void type_inference_record_identifier_write_reference_fact(SZrCompilerState *cs,
                                                           SZrAstNode *node,
                                                           const SZrTypeBinding *binding);
void type_inference_record_member_write_reference_fact(SZrCompilerState *cs, SZrAstNode *node);
void type_inference_record_constant_conditional_branch_facts(SZrCompilerState *cs,
                                                             SZrAstNode *node,
                                                             TZrBool conditionValue);
void type_inference_apply_literal_numeric_range(SZrAstNode *node,
                                                SZrInferredType *type);
void type_inference_apply_primitive_numeric_range(SZrInferredType *type);
void type_inference_apply_binary_numeric_range(const TZrChar *op,
                                               const SZrInferredType *leftType,
                                               const SZrInferredType *rightType,
                                               SZrInferredType *result);
void type_inference_apply_unary_numeric_range(const TZrChar *op,
                                              const SZrInferredType *operandType,
                                              SZrInferredType *result);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_INTERNAL_H
