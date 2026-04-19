#ifndef ZR_VM_PARSER_COMPILE_EXPRESSION_INTERNAL_H
#define ZR_VM_PARSER_COMPILE_EXPRESSION_INTERNAL_H

#include "zr_vm_parser/compiler.h"
#include "compiler_internal.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/type_inference.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_vm_conf.h"

#include <stdio.h>
#include <string.h>

#ifndef ZR_COMPILE_SLOT_U32
#define ZR_COMPILE_SLOT_U32(value) ((TZrUInt32)(value))
#endif

#ifndef ZR_COMPILE_SLOT_U16
#define ZR_COMPILE_SLOT_U16(value) ((TZrUInt16)(value))
#endif

void compile_template_string_literal(SZrCompilerState *cs, SZrAstNode *node);
void compile_literal(SZrCompilerState *cs, SZrAstNode *node);
void compile_identifier(SZrCompilerState *cs, SZrAstNode *node);
void compile_function_call(SZrCompilerState *cs, SZrAstNode *node);
void compile_member_expression(SZrCompilerState *cs, SZrAstNode *node);
void compile_import_expression(SZrCompilerState *cs, SZrAstNode *node);
void compile_type_query_expression(SZrCompilerState *cs, SZrAstNode *node);
void compile_type_literal_expression(SZrCompilerState *cs, SZrAstNode *node);
void compile_primary_expression(SZrCompilerState *cs, SZrAstNode *node);
TZrUInt32 compile_primary_expression_into_slot(SZrCompilerState *cs, SZrAstNode *node, TZrUInt32 targetSlot);
void compile_prototype_reference_expression(SZrCompilerState *cs, SZrAstNode *node);
void compile_construct_expression(SZrCompilerState *cs, SZrAstNode *node);
void compile_array_literal(SZrCompilerState *cs, SZrAstNode *node);
void compile_object_literal(SZrCompilerState *cs, SZrAstNode *node);
void compile_lambda_expression(SZrCompilerState *cs, SZrAstNode *node);
void compile_block_as_expression(SZrCompilerState *cs, SZrAstNode *blockNode);
void compile_if_expression(SZrCompilerState *cs, SZrAstNode *node);
void compile_switch_expression(SZrCompilerState *cs, SZrAstNode *node);

TZrBool construct_expression_is_ownership_builtin(const SZrConstructExpression *constructExpr);
TZrBool compile_ownership_builtin_expression(SZrCompilerState *cs,
                                             SZrConstructExpression *constructExpr,
                                             SZrFileRange location);
TZrBool wrap_constructed_result_with_ownership_builtin(SZrCompilerState *cs,
                                                       SZrConstructExpression *constructExpr,
                                                       SZrFileRange location);
EZrOwnershipQualifier infer_expression_ownership_qualifier_local(SZrCompilerState *cs, SZrAstNode *node);
TZrBool compiler_is_super_identifier_node(SZrAstNode *node);
TZrBool compiler_resolve_super_member_context(SZrCompilerState *cs,
                                              SZrFileRange location,
                                              SZrString **outSuperTypeName,
                                              TZrUInt32 *outReceiverSlot,
                                              EZrOwnershipQualifier *outReceiverOwnershipQualifier);
TZrBool emit_member_function_constant_to_slot(SZrCompilerState *cs,
                                              TZrUInt32 targetSlot,
                                              const SZrTypeMemberInfo *memberInfo,
                                              SZrFileRange location);
TZrBool emit_super_accessor_call_from_prototype(SZrCompilerState *cs,
                                                TZrUInt32 prototypeSlot,
                                                TZrUInt32 receiverSlot,
                                                SZrString *accessorName,
                                                const TZrUInt32 *argumentSlots,
                                                TZrUInt32 argumentCount,
                                                SZrFileRange location);
TZrBool emit_member_slot_get(SZrCompilerState *cs,
                             TZrUInt32 destinationSlot,
                             TZrUInt32 receiverSlot,
                             TZrUInt32 memberEntryIndex,
                             SZrFileRange location);
TZrBool reserve_member_slot_get_cache(SZrCompilerState *cs,
                                      TZrUInt32 memberEntryIndex,
                                      TZrUInt32 argumentCount,
                                      TZrUInt16 *outCacheIndex,
                                      SZrFileRange location);
TZrBool emit_known_vm_member_call_cached(SZrCompilerState *cs,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt16 cacheIndex,
                                         SZrFileRange location);
TZrBool emit_member_slot_set(SZrCompilerState *cs,
                             TZrUInt32 valueSlot,
                             TZrUInt32 receiverSlot,
                             TZrUInt32 memberEntryIndex,
                             SZrFileRange location);
TZrBool resolve_declared_field_member_access(SZrCompilerState *cs,
                                             SZrString *ownerTypeName,
                                             SZrString *memberName,
                                             SZrString **outFieldTypeName,
                                             TZrBool *outIsStatic,
                                             EZrOwnershipQualifier *outOwnershipQualifier);
TZrUInt32 compiler_get_or_add_member_entry_for_type_member(SZrCompilerState *cs,
                                                           SZrString *memberName,
                                                           const SZrTypeMemberInfo *memberInfo,
                                                           TZrUInt8 flags);
TZrBool emit_null_reset_to_identifier_binding_local(SZrCompilerState *cs,
                                                    SZrString *name,
                                                    SZrFileRange location);
TZrBool try_emit_compile_time_function_call(SZrCompilerState *cs, SZrAstNode *node);
void emit_constant_to_slot_local(SZrCompilerState *cs,
                                 TZrUInt32 slot,
                                 const SZrTypeValue *value,
                                 SZrFileRange location);
TZrBool receiver_ownership_can_call_member_local(EZrOwnershipQualifier receiverQualifier,
                                                 EZrOwnershipQualifier memberQualifier);
const TZrChar *receiver_ownership_call_error_local(EZrOwnershipQualifier receiverQualifier);
void emit_type_conversion(SZrCompilerState *cs,
                          TZrUInt32 destSlot,
                          TZrUInt32 srcSlot,
                          EZrInstructionCode conversionOpcode);
void emit_type_conversion_with_prototype(SZrCompilerState *cs,
                                         TZrUInt32 destSlot,
                                         TZrUInt32 srcSlot,
                                         EZrInstructionCode conversionOpcode,
                                         TZrUInt32 prototypeConstantIndex);
EZrValueType binary_expression_effective_type_after_conversion(EZrValueType originalType,
                                                               EZrInstructionCode conversionOpcode);
TZrBool binary_expression_type_is_float_like(EZrValueType type);
TZrBool compiler_try_infer_expression_base_type(SZrCompilerState *cs, SZrAstNode *expression, EZrValueType *outType);
EZrInstructionCode compiler_select_binary_arithmetic_opcode(const TZrChar *op,
                                                            TZrBool hasTypeInfo,
                                                            EZrValueType resultType,
                                                            EZrValueType leftType,
                                                            EZrValueType rightType);
EZrInstructionCode compiler_select_binary_equality_opcode(TZrBool isNotEqual,
                                                          TZrBool hasTypeInfo,
                                                          EZrValueType leftType,
                                                          EZrValueType rightType);
void update_identifier_assignment_type_environment(SZrCompilerState *cs,
                                                   SZrString *name,
                                                   SZrAstNode *right);
SZrString *create_hidden_property_accessor_name(SZrCompilerState *cs,
                                                SZrString *propertyName,
                                                TZrBool isSetter);
void collapse_stack_to_slot(SZrCompilerState *cs, TZrUInt32 slot);
TZrUInt32 normalize_top_result_to_slot(SZrCompilerState *cs, TZrUInt32 targetSlot);
void compile_expression_non_tail(SZrCompilerState *cs, SZrAstNode *node);
TZrUInt32 emit_string_constant(SZrCompilerState *cs, SZrString *value);
TZrUInt32 compile_expression_into_slot(SZrCompilerState *cs, SZrAstNode *node, TZrUInt32 targetSlot);
TZrBool emit_property_getter_call(SZrCompilerState *cs,
                                  TZrUInt32 currentSlot,
                                  SZrString *propertyName,
                                  TZrBool isStatic,
                                  SZrFileRange location);
TZrUInt32 emit_property_setter_call(SZrCompilerState *cs,
                                    TZrUInt32 objectSlot,
                                    SZrString *propertyName,
                                    TZrBool isStatic,
                                    TZrUInt32 assignedValueSlot,
                                    SZrFileRange location);
TZrUInt32 compile_member_key_into_slot(SZrCompilerState *cs,
                                       SZrMemberExpression *memberExpr,
                                       TZrUInt32 targetSlot);
SZrString *resolve_member_expression_symbol(SZrCompilerState *cs,
                                            SZrMemberExpression *memberExpr);
TZrBool expression_uses_dynamic_object_access(SZrAstNode *node);

SZrAstNode *find_type_declaration(SZrCompilerState *cs, SZrString *typeName);
TZrBool find_type_member_is_const(SZrCompilerState *cs, SZrString *typeName, SZrString *memberName);
TZrBool find_current_type_field_metadata(SZrCompilerState *cs,
                                                SZrString *typeName,
                                                SZrString *memberName,
                                                TZrBool *outIsConst,
                                                TZrBool *outIsStatic);
SZrTypePrototypeInfo *find_compiler_type_prototype(SZrCompilerState *cs, SZrString *typeName);
SZrTypeMemberInfo *find_compiler_type_member(SZrCompilerState *cs,
                                             SZrString *typeName,
                                             SZrString *memberName);
TZrBool resolve_expression_root_type(SZrCompilerState *cs,
                                     SZrAstNode *node,
                                     SZrString **outTypeName,
                                     TZrBool *outIsTypeReference);
SZrString *resolve_construct_target_type_name(SZrCompilerState *cs,
                                              SZrAstNode *target,
                                              EZrObjectPrototypeType *outPrototypeType);
TZrUInt32 emit_shorthand_constructor_instance(SZrCompilerState *cs,
                                              const TZrChar *op,
                                              SZrString *typeName,
                                              SZrAstNodeArray *constructorArgs,
                                              SZrFileRange location);
SZrTypeMemberInfo *find_hidden_property_accessor_member(SZrCompilerState *cs,
                                                        SZrString *typeName,
                                                        SZrString *propertyName,
                                                        TZrBool isSetter);
TZrBool can_use_property_accessor(TZrBool rootIsTypeReference, SZrTypeMemberInfo *accessorMember);
void compile_primary_member_chain(SZrCompilerState *cs,
                                  SZrAstNode *propertyNode,
                                  SZrAstNodeArray *members,
                                  TZrSize memberStartIndex,
                                  TZrUInt32 *ioCurrentSlot,
                                  SZrString **ioRootTypeName,
                                  TZrBool *ioRootIsTypeReference,
                                  EZrOwnershipQualifier *ioRootOwnershipQualifier,
                                  TZrBool rootUsesSuperLookup,
                                  TZrUInt32 superReceiverSlot,
                                  TZrUInt32 preferredDirectMemberCallResultSlot);

SZrAstNode *find_function_declaration(SZrCompilerState *cs, SZrString *funcName);
SZrAstNodeArray *match_named_arguments(SZrCompilerState *cs,
                                       SZrFunctionCall *call,
                                       SZrAstNodeArray *paramList);

#endif // ZR_VM_PARSER_COMPILE_EXPRESSION_INTERNAL_H
