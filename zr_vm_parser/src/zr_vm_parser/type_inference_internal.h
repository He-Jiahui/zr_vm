#ifndef ZR_VM_PARSER_TYPE_INFERENCE_INTERNAL_H
#define ZR_VM_PARSER_TYPE_INFERENCE_INTERNAL_H

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/ast.h"

const TZrChar *get_base_type_name(EZrValueType baseType);
void free_inferred_type_array(SZrState *state, SZrArray *types);
TZrBool zr_string_equals_cstr(SZrString *value, const TZrChar *literal);
SZrTypePrototypeInfo *find_compiler_type_prototype_inference(SZrCompilerState *cs, SZrString *typeName);
SZrTypeMemberInfo *find_compiler_type_member_inference(SZrCompilerState *cs,
                                                       SZrString *typeName,
                                                       SZrString *memberName);
TZrBool type_name_is_module_prototype_inference(SZrCompilerState *cs, SZrString *typeName);
TZrBool inferred_type_from_type_name(SZrCompilerState *cs, SZrString *typeName, SZrInferredType *result);
TZrBool inferred_type_try_map_primitive_name(const TZrNativeString nameStr,
                                             TZrSize nameLen,
                                             EZrValueType *outBaseType);
SZrString *build_generic_instance_name(SZrState *state,
                                       SZrString *baseName,
                                       const SZrArray *typeArguments);
TZrBool infer_function_call_argument_types_for_candidate(SZrCompilerState *cs,
                                                         SZrTypeEnvironment *env,
                                                         SZrString *funcName,
                                                         SZrFunctionCall *call,
                                                         const SZrFunctionTypeInfo *funcType,
                                                         SZrArray *argTypes,
                                                         TZrBool *mismatch);
TZrBool resolve_best_function_overload(SZrCompilerState *cs,
                                       SZrTypeEnvironment *env,
                                       SZrString *funcName,
                                       SZrFunctionCall *call,
                                       SZrFileRange location,
                                       SZrFunctionTypeInfo **resolvedFunction);
TZrBool resolve_prototype_target_inference(SZrCompilerState *cs,
                                           SZrAstNode *node,
                                           SZrTypePrototypeInfo **outPrototype,
                                           SZrString **outTypeName);
TZrBool infer_prototype_reference_type(SZrCompilerState *cs,
                                       SZrAstNode *node,
                                       SZrInferredType *result);
TZrBool infer_construct_expression_type(SZrCompilerState *cs,
                                        SZrAstNode *node,
                                        SZrInferredType *result);
const TZrChar *receiver_ownership_call_error(EZrOwnershipQualifier receiverQualifier);
SZrString *extract_imported_module_name(SZrFunctionCall *call);
TZrBool ensure_native_module_compile_info(SZrCompilerState *cs, SZrString *moduleName);
TZrBool infer_import_expression_type(SZrCompilerState *cs,
                                     SZrAstNode *node,
                                     SZrInferredType *result);
TZrBool infer_primary_member_chain_type(SZrCompilerState *cs,
                                        const SZrInferredType *baseType,
                                        SZrAstNodeArray *members,
                                        TZrSize startIndex,
                                        TZrBool baseIsPrototypeReference,
                                        SZrInferredType *result);
TZrBool resolve_compile_time_array_size(SZrCompilerState *cs,
                                        const SZrType *astType,
                                        TZrSize *resolvedSize);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_INTERNAL_H
