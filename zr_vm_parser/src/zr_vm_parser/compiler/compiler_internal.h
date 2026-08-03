#ifndef ZR_VM_PARSER_COMPILER_INTERNAL_H
#define ZR_VM_PARSER_COMPILER_INTERNAL_H

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/project_imports.h"
#include "zr_vm_parser/semantic_ir.h"
#include "zr_vm_parser/type_inference.h"
#include "compiler_property.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdio.h>
#include <string.h>

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

TZrBool compiler_test_bind_function(
        SZrCompilerState *cs,
        SZrAstNode *functionNode,
        SZrFunction *function,
        TZrUInt32 callableChildIndex,
        TZrBool *isTest);
TZrBool compiler_test_validate_non_module_roles(
        SZrCompilerState *cs,
        SZrAstNode *node);
TZrBool compiler_test_validate_production_reference(
        SZrCompilerState *cs,
        const SZrAstNode *declarationNode,
        SZrFileRange location);
TZrBool compiler_test_finalize_manifest(SZrCompilerState *cs);
void compiler_test_free_entries(SZrCompilerState *cs);

TZrBool compiler_call_has_spread_argument(const SZrFunctionCall *call);
TZrBool compiler_validate_spread_call_signature(
        SZrCompilerState *cs,
        const SZrFunctionCall *call,
        const SZrArray *parameterTypes,
        const SZrArray *parameterPassingModes,
        SZrFileRange location);
TZrBool compiler_validate_trailing_spread_call(
        SZrCompilerState *cs,
        const SZrFunctionCall *call,
        SZrFileRange location);
typedef struct SZrCompilerSemanticIrSlotIdentity {
    TZrTypeId typeId;
    TZrSymbolId symbolId;
    TZrPlaceId placeId;
    SZrFileRange declarationRange;
} SZrCompilerSemanticIrSlotIdentity;

void compiler_semantic_ir_init(SZrCompilerState *cs);
void compiler_semantic_ir_reset(SZrCompilerState *cs);
void compiler_semantic_ir_free(SZrCompilerState *cs);
TZrBool compiler_semantic_ir_get_slot_identity(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot,
        SZrCompilerSemanticIrSlotIdentity *outIdentity);
TZrBool compiler_semantic_ir_register_local(SZrCompilerState *cs,
                                             SZrString *name,
                                             TZrUInt32 stackSlot,
                                             SZrFileRange sourceRange,
                                             TZrBool initialized);
TZrBool compiler_semantic_ir_lower_load(SZrCompilerState *cs,
                                        TZrUInt32 stackSlot,
                                        TZrUInt32 resultSlot,
                                        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_lower_store(SZrCompilerState *cs,
                                         TZrUInt32 stackSlot,
                                         TZrUInt32 valueSlot,
                                         SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_lower_ownership(
        SZrCompilerState *cs,
        EZrOwnershipBuiltinKind builtinKind,
        TZrUInt32 sourceSlot,
        TZrUInt32 resultSlot,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_lower_value_construct(
        SZrCompilerState *cs,
        TZrUInt32 destinationSlot,
        TZrTypeId typeId,
        TZrSymbolId constructorId,
        const TZrUInt32 *argumentSlots,
        TZrSize argumentCount,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_lower_value_construct_to_place(
        SZrCompilerState *cs,
        TZrUInt32 destinationSlot,
        TZrPlaceId destinationPlaceId,
        TZrTypeId typeId,
        TZrSymbolId constructorId,
        const TZrUInt32 *argumentSlots,
        TZrSize argumentCount,
        SZrFileRange sourceRange);
TZrLoanId compiler_semantic_ir_begin_receiver_call(
        SZrCompilerState *cs,
        TZrUInt32 receiverSlot,
        EZrCanonicalReceiverEffect receiverEffect,
        SZrFileRange sourceRange);
TZrPlaceId compiler_semantic_ir_place_for_slot(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_bind_slot_to_place(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot,
        TZrPlaceId placeId);
const SZrSemanticContiguousViewFact *
compiler_semantic_ir_contiguous_view_fact_for_slot(
        SZrCompilerState *cs,
        TZrUInt32 viewSlot,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_record_contiguous_view(
        SZrCompilerState *cs,
        TZrUInt32 viewSlot,
        TZrPlaceId sourcePlaceId,
        TZrUInt32 startSlot,
        TZrUInt32 lengthSlot,
        EZrSemanticContiguousSourceKind sourceKind,
        TZrRegionId regionId,
        TZrBool isReadOnly,
        TZrBool hasKnownStart,
        TZrInt64 knownStart,
        TZrBool hasKnownLength,
        TZrInt64 knownLength,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_record_bounds_fact(
        SZrCompilerState *cs,
        TZrUInt32 viewSlot,
        TZrUInt32 indexSlot,
        TZrUInt32 lengthSlot,
        TZrBool hasKnownIndex,
        TZrInt64 knownIndex,
        SZrFileRange sourceRange,
        TZrBool *outCheckElided);
TZrPlaceId compiler_semantic_ir_project_field(
        SZrCompilerState *cs,
        TZrPlaceId parentPlaceId,
        TZrSymbolId fieldIdentity,
        SZrFileRange sourceRange);
TZrPlaceId compiler_semantic_ir_project_index(
        SZrCompilerState *cs,
        TZrPlaceId parentPlaceId,
        TZrUInt32 indexStackSlot,
        SZrFileRange sourceRange);
TZrLoanId compiler_semantic_ir_begin_receiver_call_place(
        SZrCompilerState *cs,
        TZrPlaceId receiverPlaceId,
        EZrCanonicalReceiverEffect receiverEffect,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_activate_receiver_call(
        SZrCompilerState *cs,
        TZrLoanId loanId,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_project_receiver_call_result(
        SZrCompilerState *cs,
        TZrLoanId loanId,
        TZrUInt32 resultSlot,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_record_property_ref_get(
        SZrCompilerState *cs,
        TZrUInt32 receiverSlot,
        TZrUInt32 resultSlot,
        const SZrTypeMemberInfo *propertyMember,
        const SZrTypeMemberInfo *getterAccessor,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_record_property_ref_load(
        SZrCompilerState *cs,
        TZrUInt32 referenceSlot,
        TZrUInt32 targetSlot,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_record_property_ref_store(
        SZrCompilerState *cs,
        TZrUInt32 referenceSlot,
        TZrUInt32 sourceSlot,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_end_receiver_call(
        SZrCompilerState *cs,
        TZrLoanId loanId,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_record_iterator_yield(
        SZrCompilerState *cs,
        TZrTypeId elementTypeId,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_record_iterator_complete(
        SZrCompilerState *cs,
        SZrFileRange sourceRange);
TZrBool compiler_iterator_function_contains_yield(const SZrAstNode *body);
TZrBool compiler_iterator_current_function_contains_yield(
        const SZrCompilerState *cs);
void compiler_iterator_compile_yield(SZrCompilerState *cs, SZrAstNode *node);

static ZR_FORCE_INLINE EZrMetaType compiler_resolve_meta_type_name(SZrString *metaName) {
    TZrNativeString metaNameText;

    if (metaName == ZR_NULL) {
        return ZR_META_ENUM_MAX;
    }

    metaNameText = ZrCore_String_GetNativeStringShort(metaName);
    if (metaNameText == ZR_NULL) {
        return ZR_META_ENUM_MAX;
    }

    if (strcmp(metaNameText, "constructor") == 0) {
        return ZR_META_CONSTRUCTOR;
    }
    if (strcmp(metaNameText, "destructor") == 0) {
        return ZR_META_DESTRUCTOR;
    }
    if (strcmp(metaNameText, "add") == 0) {
        return ZR_META_ADD;
    }
    if (strcmp(metaNameText, "sub") == 0) {
        return ZR_META_SUB;
    }
    if (strcmp(metaNameText, "mul") == 0) {
        return ZR_META_MUL;
    }
    if (strcmp(metaNameText, "div") == 0) {
        return ZR_META_DIV;
    }
    if (strcmp(metaNameText, "mod") == 0) {
        return ZR_META_MOD;
    }
    if (strcmp(metaNameText, "pow") == 0) {
        return ZR_META_POW;
    }
    if (strcmp(metaNameText, "neg") == 0) {
        return ZR_META_NEG;
    }
    if (strcmp(metaNameText, "compare") == 0) {
        return ZR_META_COMPARE;
    }
    if (strcmp(metaNameText, "toBool") == 0) {
        return ZR_META_TO_BOOL;
    }
    if (strcmp(metaNameText, "toString") == 0) {
        return ZR_META_TO_STRING;
    }
    if (strcmp(metaNameText, "toInt") == 0) {
        return ZR_META_TO_INT;
    }
    if (strcmp(metaNameText, "toUInt") == 0) {
        return ZR_META_TO_UINT;
    }
    if (strcmp(metaNameText, "toFloat") == 0) {
        return ZR_META_TO_FLOAT;
    }
    if (strcmp(metaNameText, "call") == 0) {
        return ZR_META_CALL;
    }
    if (strcmp(metaNameText, "getter") == 0) {
        return ZR_META_GETTER;
    }
    if (strcmp(metaNameText, "setter") == 0) {
        return ZR_META_SETTER;
    }
    if (strcmp(metaNameText, "shiftLeft") == 0) {
        return ZR_META_SHIFT_LEFT;
    }
    if (strcmp(metaNameText, "shiftRight") == 0) {
        return ZR_META_SHIFT_RIGHT;
    }
    if (strcmp(metaNameText, "bitAnd") == 0) {
        return ZR_META_BIT_AND;
    }
    if (strcmp(metaNameText, "bitOr") == 0) {
        return ZR_META_BIT_OR;
    }
    if (strcmp(metaNameText, "bitXor") == 0) {
        return ZR_META_BIT_XOR;
    }
    if (strcmp(metaNameText, "bitNot") == 0) {
        return ZR_META_BIT_NOT;
    }
    if (strcmp(metaNameText, "getItem") == 0) {
        return ZR_META_GET_ITEM;
    }
    if (strcmp(metaNameText, "setItem") == 0) {
        return ZR_META_SET_ITEM;
    }
    if (strcmp(metaNameText, "close") == 0) {
        return ZR_META_CLOSE;
    }
    return ZR_META_ENUM_MAX;
}

typedef struct ZrExternCompilerTempRoot {
    SZrState *state;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor slotAnchor;
    TZrBool active;
} ZrExternCompilerTempRoot;

typedef struct SZrCompilerArraySnapshot {
    TZrPtr data;
    TZrSize length;
    TZrSize elementSize;
} SZrCompilerArraySnapshot;

#pragma pack(push, 1)
typedef struct SZrCompiledPrototypeInfo {
    TZrUInt32 nameStringIndex;
    TZrUInt32 type;
    TZrUInt32 accessModifier;
    TZrUInt32 inheritsCount;
    TZrUInt32 membersCount;
    TZrUInt64 protocolMask;
    TZrUInt32 hasDecoratorMetadata;
    TZrUInt32 decoratorMetadataConstantIndex;
    TZrUInt32 decoratorsCount;
    TZrUInt32 modifierFlags;
    TZrUInt32 nextVirtualSlotIndex;
    TZrUInt32 nextPropertyIdentity;
    TZrUInt32 layoutByteSize;
    TZrUInt32 layoutByteAlign;
} SZrCompiledPrototypeInfo;

typedef struct SZrCompiledMemberInfo {
    TZrUInt32 memberType;
    TZrUInt32 nameStringIndex;
    TZrUInt32 accessModifier;
    TZrUInt32 isStatic;
    TZrUInt32 isConst;
    TZrUInt32 fieldTypeNameStringIndex;
    TZrUInt32 fieldOffset;
    TZrUInt32 fieldSize;
    TZrUInt32 isMetaMethod;
    TZrUInt32 metaType;
    TZrUInt32 functionConstantIndex;
    TZrUInt32 parameterCount;
    TZrUInt32 returnTypeNameStringIndex;
    TZrUInt32 reservedRemovedUsingManaged;
    TZrUInt32 ownershipQualifier;
    TZrUInt32 callsClose;
    TZrUInt32 callsDestructor;
    TZrUInt32 declarationOrder;
    TZrUInt32 contractRole;
    TZrUInt32 hasDecoratorMetadata;
    TZrUInt32 decoratorMetadataConstantIndex;
    TZrUInt32 hasDecoratorNames;
    TZrUInt32 decoratorNamesConstantIndex;
    TZrUInt32 modifierFlags;
    TZrUInt32 ownerTypeNameStringIndex;
    TZrUInt32 baseDefinitionOwnerTypeNameStringIndex;
    TZrUInt32 baseDefinitionNameStringIndex;
    TZrUInt32 virtualSlotIndex;
    TZrUInt32 interfaceContractSlot;
    TZrUInt32 propertyIdentity;
    TZrUInt32 accessorRole;                 // EZrPropertyAccessorRole numeric payload
} SZrCompiledMemberInfo;
#pragma pack(pop)

typedef char ZrCompilerCompiledMemberInfoV34LayoutMustRemainStable[
        sizeof(SZrCompiledMemberInfo) == 31U * sizeof(TZrUInt32) ? 1 : -1];

EZrOwnershipQualifier get_member_receiver_qualifier(SZrAstNode *node) ;

EZrCanonicalReceiverEffect get_member_receiver_effect(SZrAstNode *node) ;

TZrBool compiler_receiver_effect_can_implement(
        EZrCanonicalReceiverEffect requiredEffect,
        EZrCanonicalReceiverEffect implementationEffect) ;

TZrBool compiler_validate_receiver_call(
        SZrCompilerState *cs,
        SZrAstNode *receiverNode,
        SZrString *receiverTypeName,
        EZrOwnershipQualifier receiverQualifier,
        const SZrTypeMemberInfo *memberInfo,
        TZrBool receiverIsAddressable,
        SZrFileRange location) ;

EZrOwnershipQualifier get_implicit_this_ownership_qualifier(EZrOwnershipQualifier receiverQualifier) ;

void ZrParser_CompilerState_Init(SZrCompilerState *cs, SZrState *state) ;

void ZrParser_CompilerState_Free(SZrCompilerState *cs) ;

void print_error_suggestion(const TZrChar *msg) ;

void ZrParser_CompileTime_Error(SZrCompilerState *cs, EZrCompileTimeErrorLevel level, const TZrChar *message, SZrFileRange location) ;

void ZrParser_Compiler_Error(SZrCompilerState *cs, const TZrChar *msg, SZrFileRange location) ;

ZR_PARSER_API TZrInstruction create_instruction_0(EZrInstructionCode opcode, TZrUInt16 operandExtra) ;

ZR_PARSER_API TZrInstruction create_instruction_1(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrInt32 operand) ;

ZR_PARSER_API TZrInstruction create_instruction_2(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrUInt16 operand1,
                                    TZrUInt16 operand2) ;

ZR_PARSER_API TZrInstruction create_instruction_4(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrUInt8 op0, TZrUInt8 op1,
                                    TZrUInt8 op2, TZrUInt8 op3) ;

TZrBool compiler_copy_range_to_raw(SZrCompilerState *cs,
                                          TZrPtr *outMemory,
                                          const TZrPtr source,
                                          TZrSize count,
                                          TZrSize elementSize) ;

TZrBool compiler_capture_array_snapshot(SZrCompilerState *cs,
                                               const SZrArray *source,
                                               SZrCompilerArraySnapshot *snapshot) ;

void compiler_restore_array_snapshot(SZrCompilerState *cs,
                                            SZrArray *target,
                                            SZrCompilerArraySnapshot *snapshot) ;

void compiler_release_array_snapshot(SZrCompilerState *cs,
                                            SZrCompilerArraySnapshot *snapshot) ;

TZrBool compiler_copy_function_exception_metadata_slice(SZrCompilerState *cs,
                                                                SZrFunction *function,
                                                                TZrSize executionStart,
                                                                TZrSize catchStart,
                                                                TZrSize handlerStart,
                                                                SZrAstNode *sourceNode) ;

ZR_PARSER_API TZrBool compiler_assemble_final_function(SZrCompilerState *cs,
                                                       SZrFunction *function,
                                                       SZrAstNode *sourceNode,
                                                       TZrBool copyCurrentFunctionBuffers,
                                                       TZrBool preserveExistingSignature);

TZrBool compiler_attach_detached_function_prototype_context(SZrState *state,
                                                                   SZrFunction *detachedFunction,
                                                                   SZrFunction *entryFunction) ;

void emit_instruction(SZrCompilerState *cs, TZrInstruction instruction) ;

TZrUInt32 add_constant(SZrCompilerState *cs, SZrTypeValue *value) ;

TZrBool compiler_value_is_compile_time_function_pointer(SZrCompilerState *cs, const SZrTypeValue *value) ;

TZrBool compiler_is_runtime_safe_compile_time_value_internal(SZrCompilerState *cs,
                                                                  const SZrTypeValue *value,
                                                                  SZrRawObject **visitedObjects,
                                                                  TZrSize visitedCount,
                                                                  TZrUInt32 depth) ;

TZrUInt32 allocate_local_var(SZrCompilerState *cs, SZrString *name) ;

TZrSize ZrParser_Compiler_GetLocalStackFloor(const SZrCompilerState *cs) ;

TZrUInt32 compiler_get_cached_null_constant_index(SZrCompilerState *cs) ;

TZrUInt32 compiler_get_or_add_member_entry(SZrCompilerState *cs, SZrString *memberName) ;
TZrBool compiler_property_reference_load(
        SZrCompilerState *cs,
        TZrUInt32 referenceSlot,
        TZrUInt32 targetSlot,
        SZrFileRange location);
TZrBool compiler_property_reference_store(
        SZrCompilerState *cs,
        TZrUInt32 referenceSlot,
        TZrUInt32 sourceSlot,
        SZrFileRange location);
TZrUInt32 compiler_get_or_add_member_entry_with_flags(SZrCompilerState *cs,
                                                      SZrString *memberName,
                                                      TZrUInt8 flags) ;
TZrUInt32 compiler_get_or_add_member_entry_bound_with_flags(SZrCompilerState *cs,
                                                            SZrString *memberName,
                                                            TZrUInt32 prototypeIndex,
                                                            TZrUInt32 descriptorIndex,
                                                            TZrUInt8 flags) ;

void ZrParser_Compiler_TrimStackToCount(SZrCompilerState *cs, TZrSize targetCount) ;

void ZrParser_Compiler_TrimStackToSlot(SZrCompilerState *cs, TZrUInt32 slot) ;

void ZrParser_Compiler_TrimStackBy(SZrCompilerState *cs, TZrSize amount) ;

TZrUInt32 find_local_var(SZrCompilerState *cs, SZrString *name) ;

TZrUInt32 find_closure_var(SZrCompilerState *cs, SZrString *name) ;

TZrUInt32 allocate_closure_var(SZrCompilerState *cs, SZrString *name, TZrBool inStack) ;

TZrUInt32 find_child_function_index(SZrCompilerState *cs, SZrString *name) ;
SZrAstNode *find_function_declaration(SZrCompilerState *cs, SZrString *funcName);

TZrUInt32 generate_function_reference_path_constant(SZrCompilerState *cs, TZrUInt32 childFunctionIndex) ;

TZrUInt32 allocate_stack_slot(SZrCompilerState *cs) ;

void compiler_advance_stack_to_fresh_slot(SZrCompilerState *cs) ;

SZrString *extract_simple_type_name_from_type_node(SZrAstNode *typeNode) ;

TZrBool compiler_type_has_constructor(SZrCompilerState *cs, SZrString *typeName) ;

void emit_constant_to_slot(SZrCompilerState *cs, TZrUInt32 slot, const SZrTypeValue *value) ;

void emit_string_constant_to_slot(SZrCompilerState *cs, TZrUInt32 slot, SZrString *value) ;

void compiler_register_function_type_binding(SZrCompilerState *cs, SZrFunctionDeclaration *funcDecl) ;

void compiler_register_callable_value_binding(SZrCompilerState *cs,
                                                     SZrString *name,
                                                     SZrAstNode *valueNode) ;

void compiler_register_named_value_binding_to_env(SZrCompilerState *cs,
                                                         SZrTypeEnvironment *env,
                                                         SZrString *name,
                                                         SZrString *typeName) ;

void compiler_register_extern_function_type_binding_to_env(SZrCompilerState *cs,
                                                           SZrAstNode *declarationNode,
                                                           SZrTypeEnvironment *env,
                                                           SZrExternFunctionDeclaration *functionDecl) ;

TZrUInt32 find_local_var_in_current_scope(SZrCompilerState *cs, SZrString *name) ;

ZR_PARSER_API void ZrParser_Compiler_PredeclareFunctionBindings(SZrCompilerState *cs, SZrAstNodeArray *statements) ;

TZrBool compiler_is_compile_tool_import_declaration(const SZrState *state,
                                                     const SZrAstNode *node) ;

TZrUInt32 emit_load_global_identifier(SZrCompilerState *cs, SZrString *name) ;
void emit_object_field_assignment_from_expression(SZrCompilerState *cs,
                                                         TZrUInt32 objectSlot,
                                                         SZrString *fieldName,
                                                         SZrAstNode *expression) ;

void emit_class_static_field_initializers(SZrCompilerState *cs, SZrAstNode *classNode) ;

void compiler_begin_constructor_const_field_tracking(SZrCompilerState *cs) ;

void compiler_end_constructor_const_field_tracking(SZrCompilerState *cs) ;

TZrBool compiler_record_constructor_const_field_assignment(SZrCompilerState *cs,
                                                                  SZrString *fieldName,
                                                                  const TZrChar *op,
                                                                  SZrFileRange location) ;

TZrBool compiler_validate_constructor_const_field_initialization(SZrCompilerState *cs,
                                                                 SZrAstNode *body,
                                                                 SZrFileRange location) ;

TZrBool compiler_validate_init_accessor_const_field_writes(SZrCompilerState *cs,
                                                            SZrAstNode *body,
                                                            SZrFileRange location) ;

void compiler_type_members_restore_declaration_order(SZrArray *members) ;

SZrString *compiler_create_hidden_property_accessor_name(SZrCompilerState *cs, SZrString *propertyName,
                                                         TZrBool isSetter) ;

void emit_super_constructor_call(SZrCompilerState *cs, SZrString *superTypeName, SZrAstNodeArray *superArgs) ;

SZrString *create_hidden_extern_local_name(SZrCompilerState *cs, const TZrChar *prefix) ;

TZrBool extern_compiler_string_equals(SZrString *value, const TZrChar *literal) ;

TZrBool extern_compiler_identifier_equals(SZrAstNode *node, const TZrChar *literal) ;

TZrBool extern_compiler_make_string_value(SZrCompilerState *cs, const TZrChar *text, SZrTypeValue *outValue) ;

TZrBool extern_compiler_temp_root_begin(SZrCompilerState *cs, ZrExternCompilerTempRoot *root) ;

SZrTypeValue *extern_compiler_temp_root_value(ZrExternCompilerTempRoot *root) ;

TZrBool extern_compiler_temp_root_set_value(ZrExternCompilerTempRoot *root, const SZrTypeValue *value) ;

TZrBool extern_compiler_temp_root_set_object(ZrExternCompilerTempRoot *root,
                                                    SZrObject *object,
                                                    EZrValueType type) ;

void extern_compiler_temp_root_end(ZrExternCompilerTempRoot *root) ;

TZrBool extern_compiler_set_object_field(SZrCompilerState *cs,
                                                SZrObject *object,
                                                const TZrChar *fieldName,
                                                const SZrTypeValue *value) ;

TZrBool extern_compiler_push_array_value(SZrCompilerState *cs, SZrObject *array, const SZrTypeValue *value) ;

SZrObject *extern_compiler_new_object_constant(SZrCompilerState *cs) ;

SZrObject *extern_compiler_new_array_constant(SZrCompilerState *cs) ;

TZrBool extern_compiler_match_decorator_path(SZrAstNode *decoratorNode,
                                                    const TZrChar *leafName,
                                                    TZrBool requireCall,
                                                    SZrFunctionCall **outCall) ;

SZrAstNode *extern_compiler_decorators_find_call(SZrAstNodeArray *decorators,
                                                        const TZrChar *leafName,
                                                        SZrFunctionCall **outCall) ;

TZrBool extern_compiler_decorators_has_flag(SZrAstNodeArray *decorators, const TZrChar *leafName) ;

TZrBool extern_compiler_extract_string_argument(SZrFunctionCall *call, SZrString **outString) ;

TZrBool extern_compiler_extract_int_argument(SZrFunctionCall *call, TZrInt64 *outValue) ;

SZrString *extern_compiler_decorators_get_string_arg(SZrAstNodeArray *decorators, const TZrChar *leafName) ;

TZrBool extern_compiler_decorators_get_int_arg(SZrAstNodeArray *decorators,
                                                      const TZrChar *leafName,
                                                      TZrInt64 *outValue) ;

SZrAstNode *extern_compiler_find_named_declaration(SZrExternBlock *externBlock, SZrString *name) ;

TZrBool extern_compiler_is_precise_ffi_primitive_name(SZrString *name) ;

TZrBool extern_compiler_wrap_pointer_descriptor(SZrCompilerState *cs,
                                                       SZrTypeValue *baseValue,
                                                       const TZrChar *directionText) ;

TZrBool extern_compiler_descriptor_set_string_field(SZrCompilerState *cs,
                                                           SZrObject *object,
                                                           const TZrChar *fieldName,
                                                           const TZrChar *text) ;

TZrBool extern_compiler_descriptor_set_string_object_field(SZrCompilerState *cs,
                                                                  SZrObject *object,
                                                                  const TZrChar *fieldName,
                                                                  SZrString *text) ;

TZrBool extern_compiler_descriptor_set_int_field(SZrCompilerState *cs,
                                                        SZrObject *object,
                                                        const TZrChar *fieldName,
                                                        TZrInt64 value) ;

const TZrChar *extern_compiler_direction_from_decorators(SZrAstNodeArray *decorators) ;

TZrBool extern_compiler_apply_string_charset(SZrCompilerState *cs,
                                                    SZrTypeValue *descriptorValue,
                                                    SZrString *charsetName) ;

TZrBool extern_compiler_build_type_descriptor_value(SZrCompilerState *cs,
                                                           SZrExternBlock *externBlock,
                                                           SZrType *type,
                                                           SZrAstNodeArray *decorators,
                                                           SZrFileRange location,
                                                           SZrTypeValue *outValue) ;

TZrBool extern_compiler_build_signature_descriptor_value(SZrCompilerState *cs,
                                                                SZrExternBlock *externBlock,
                                                                SZrAstNodeArray *params,
                                                                SZrParameter *args,
                                                                SZrType *returnType,
                                                                SZrAstNodeArray *decorators,
                                                                TZrBool includeKind,
                                                                SZrFileRange location,
                                                                SZrTypeValue *outValue) ;

TZrBool extern_compiler_build_struct_descriptor_value(SZrCompilerState *cs,
                                                             SZrExternBlock *externBlock,
                                                             SZrAstNode *declarationNode,
                                                             SZrTypeValue *outValue) ;

TZrBool extern_compiler_build_enum_descriptor_value(SZrCompilerState *cs,
                                                           SZrAstNode *declarationNode,
                                                           SZrTypeValue *outValue) ;

TZrBool extern_compiler_build_delegate_descriptor_value(SZrCompilerState *cs,
                                                               SZrExternBlock *externBlock,
                                                               SZrAstNode *declarationNode,
                                                               TZrBool includeKind,
                                                               SZrTypeValue *outValue) ;

TZrBool extern_compiler_emit_get_member_to_slot(SZrCompilerState *cs,
                                                       TZrUInt32 destSlot,
                                                       TZrUInt32 objectSlot,
                                                       SZrString *memberName) ;

TZrBool extern_compiler_emit_import_module_to_local(SZrCompilerState *cs,
                                                           SZrString *moduleName,
                                                           TZrUInt32 localSlot,
                                                           SZrFileRange location) ;

TZrBool extern_compiler_emit_module_function_call_to_local(SZrCompilerState *cs,
                                                                  TZrUInt32 moduleSlot,
                                                                  SZrString *functionName,
                                                                  const SZrTypeValue *argumentValues,
                                                                  TZrUInt32 argumentCount,
                                                                  TZrUInt32 localSlot,
                                                                  SZrFileRange location) ;

TZrBool extern_compiler_emit_method_call_to_local(SZrCompilerState *cs,
                                                         TZrUInt32 receiverSlot,
                                                         SZrString *methodName,
                                                         const SZrTypeValue *argumentValues,
                                                         TZrUInt32 argumentCount,
                                                         TZrUInt32 localSlot,
                                                         SZrFileRange location) ;

ZR_PARSER_API void enter_scope(SZrCompilerState *cs) ;

ZR_PARSER_API void exit_scope(SZrCompilerState *cs) ;

TZrBool compiler_has_active_scope_ownership_cleanups(SZrCompilerState *cs) ;

void compiler_emit_active_scope_ownership_cleanups(SZrCompilerState *cs) ;

TZrBool compiler_has_scope_ownership_cleanups_above_depth(SZrCompilerState *cs,
                                                          TZrSize targetScopeStackDepth) ;

void compiler_emit_scope_ownership_cleanups_above_depth(SZrCompilerState *cs,
                                                        TZrSize targetScopeStackDepth) ;

void compiler_register_scope_cleanup_slot(SZrCompilerState *cs,
                                          TZrUInt32 slot,
                                          EZrOwnershipBuiltinKind ownershipBuiltinKind,
                                          TZrUInt32 sourceSlot) ;

void compiler_register_owner_cleanup_slot(SZrCompilerState *cs,
                                          TZrUInt32 slot,
                                          EZrOwnershipQualifier ownershipQualifier) ;

void compiler_register_typed_owner_cleanup_slot(SZrCompilerState *cs,
                                                 TZrUInt32 slot,
                                                 const SZrType *typeInfo) ;

void compiler_register_inferred_close_cleanup_slot(
        SZrCompilerState *cs,
        TZrUInt32 slot,
        const SZrInferredType *typeInfo) ;

void enter_type_scope(SZrCompilerState *cs) ;

void exit_type_scope(SZrCompilerState *cs) ;

TZrSize create_label(SZrCompilerState *cs) ;

void resolve_label(SZrCompilerState *cs, TZrSize labelId) ;

void add_pending_jump(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId) ;

void add_pending_absolute_patch(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId) ;

TZrInstruction compiler_create_jump_if_false_for_condition(SZrCompilerState *cs,
                                                           SZrAstNode *conditionExpression,
                                                           TZrUInt32 conditionSlot) ;

void record_external_var_reference(SZrCompilerState *cs, SZrString *name) ;

void collect_identifiers_from_array(SZrCompilerState *cs, SZrAstNodeArray *nodes, SZrArray *identifierNames) ;

void collect_identifiers_from_node(SZrCompilerState *cs, SZrAstNode *node, SZrArray *identifierNames) ;

void ZrParser_ExternalVariables_Analyze(SZrCompilerState *cs, SZrAstNode *node, SZrCompilerState *parentCompiler) ;

void compress_instructions(SZrCompilerState *cs) ;

void eliminate_redundant_instructions(SZrCompilerState *cs) ;

void optimize_jumps(SZrCompilerState *cs) ;

ZR_PARSER_API void optimize_instructions(SZrCompilerState *cs) ;

void compile_function_declaration(SZrCompilerState *cs, SZrAstNode *node) ;

SZrString *get_type_name_from_inferred_type(SZrCompilerState *cs, const SZrInferredType *inferredType) ;

SZrString *extract_generic_argument_name_string(SZrCompilerState *cs, SZrAstNode *node) ;

SZrString *extract_type_name_string(SZrCompilerState *cs, SZrType *type) ;

void compiler_collect_generic_parameter_info(SZrCompilerState *cs,
                                             SZrArray *genericParameters,
                                             SZrGenericDeclaration *genericDeclaration) ;
void compiler_collect_parameter_passing_modes(SZrState *state,
                                              SZrArray *parameterPassingModes,
                                              SZrAstNodeArray *params) ;
TZrBool compiler_parameter_is_readonly(const SZrParameter *parameter);
void compiler_register_readonly_parameter_name(SZrCompilerState *cs,
                                               const SZrParameter *parameter,
                                               SZrString *parameterName);
TZrBool compiler_expression_is_assignable_storage_location(const SZrAstNode *node);
TZrBool compiler_validate_call_argument_passing_contract(
        SZrCompilerState *cs,
        const SZrArray *parameterPassingModes,
        const SZrAstNodeArray *parameterList,
        const SZrArray *parameterNames,
        const SZrFunctionCall *call);
ZR_PARSER_API TZrBool compiler_validate_out_parameter_definite_assignment(
        SZrCompilerState *cs,
        SZrAstNodeArray *params,
        SZrAstNode *body,
        SZrFileRange fallbackLocation);
TZrBool compiler_validate_interface_variance_rules(SZrCompilerState *cs,
                                                   SZrAstNode *interfaceNode);

TZrUInt32 calculate_type_size(SZrCompilerState *cs, SZrType *type) ;

TZrUInt32 align_offset(TZrUInt32 offset, TZrUInt32 align) ;

TZrUInt32 get_type_alignment(SZrCompilerState *cs, SZrType *type) ;

void compile_struct_declaration(SZrCompilerState *cs, SZrAstNode *node) ;

TZrBool extern_compiler_has_registered_type(SZrCompilerState *cs, SZrString *typeName) ;

void extern_compiler_register_struct_prototype(SZrCompilerState *cs, SZrAstNode *declarationNode) ;

void extern_compiler_register_enum_prototype(SZrCompilerState *cs, SZrAstNode *declarationNode) ;

void compile_enum_declaration(SZrCompilerState *cs, SZrAstNode *node) ;

void compile_union_declaration(SZrCompilerState *cs, SZrAstNode *node) ;

void compiler_register_extern_block_bindings(SZrCompilerState *cs, SZrExternBlock *externBlock) ;

void ZrParser_Compiler_PredeclareExternBindings(SZrCompilerState *cs, SZrAstNodeArray *statements) ;

void compile_extern_block_declaration(SZrCompilerState *cs, SZrAstNode *node) ;

void ZrParser_Compiler_CompileExternBlock(SZrCompilerState *cs, SZrAstNode *node) ;

void compile_meta_function(SZrCompilerState *cs, SZrAstNode *node, EZrMetaType metaType) ;

SZrFunction *compile_class_member_function(SZrCompilerState *cs, SZrAstNode *node,
                                                  SZrString *superTypeName,
                                                  TZrBool injectThis, TZrUInt32 *outParameterCount,
                                                  SZrString **outInferredReturnTypeName) ;
SZrFunction *compile_property_accessor_function(
        SZrCompilerState *cs,
        SZrAstNode *propertyNode,
        SZrAstNode *accessorNode,
        SZrString *superTypeName,
        TZrBool injectThis,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 *outParameterCount);

void compile_class_declaration(SZrCompilerState *cs, SZrAstNode *node) ;
void compiler_class_lint_process_local_shared_cycles(SZrCompilerState *cs,
                                                     SZrTypePrototypeInfo *info);

void compile_interface_declaration(SZrCompilerState *cs, SZrAstNode *node) ;
void compiler_finalize_interface_decorators(
        SZrCompilerState *cs,
        SZrAstNode *node);
TZrBool compiler_type_member_capture_structured_return_type(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *memberInfo,
        SZrType *returnType);

TZrBool serialize_prototype_info_to_binary(SZrCompilerState *cs, SZrTypePrototypeInfo *info, 
                                                 TZrByte **outData, TZrSize *outSize) ;

TZrBool compiler_build_typed_local_bindings(SZrCompilerState *cs,
                                            SZrFunctionTypedLocalBinding **outBindings,
                                            TZrUInt32 *outCount);
void compiler_typed_type_ref_from_inferred(SZrFunctionTypedTypeRef *dest,
                                           const SZrInferredType *src);
TZrBool compiler_build_typed_closure_bindings(SZrCompilerState *cs,
                                              SZrFunctionTypedClosureBinding **outBindings,
                                              TZrUInt32 *outCount);
TZrBool compiler_build_function_frame_layout_metadata(SZrCompilerState *cs, SZrFunction *function);
TZrBool compiler_register_stack_slot_type_hint(SZrCompilerState *cs,
                                               TZrUInt32 stackSlot,
                                               const SZrInferredType *type);
TZrBool compiler_register_stack_slot_inline_receiver_argument_alias(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot,
        TZrUInt32 receiverStackSlot);
TZrBool compiler_register_stack_slot_field_alias(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot,
        TZrUInt32 parentStackSlot,
        TZrUInt32 memberEntryIndex);
TZrBool compiler_register_stack_slot_array_element_alias(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot,
        TZrUInt32 arrayStackSlot);
TZrBool compiler_find_inline_type_layout_for_inferred(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        TZrUInt32 *outLayoutId,
        TZrUInt32 *outSize,
        TZrUInt32 *outAlign);
void compiler_clear_stack_slot_type_hints_from(SZrCompilerState *cs, TZrSize startIndex);
TZrSize compiler_enter_stack_slot_type_hint_scope(SZrCompilerState *cs);
void compiler_restore_stack_slot_type_hint_scope(SZrCompilerState *cs, TZrSize previousScopeStart);
TZrBool compiler_build_function_parameter_metadata(SZrCompilerState *cs,
                                                   SZrAstNodeArray *params,
                                                   TZrBool includeDefaultValues,
                                                   SZrFunctionMetadataParameter **outParameters,
                                                   TZrUInt32 *outParameterCount);
void compiler_free_function_parameter_metadata(
        SZrState *state,
        SZrFunctionMetadataParameter *parameters,
        TZrUInt32 parameterCount);
TZrBool compiler_build_callable_return_type_metadata(SZrCompilerState *cs,
                                                     SZrType *declaredReturnType,
                                                     SZrAstNode *bodyNode,
                                                     SZrFunctionTypedTypeRef *outType,
                                                     TZrBool *outHasType);

TZrBool compiler_build_script_typed_metadata(SZrCompilerState *cs);
TZrBool ZrParser_Compiler_PublishSemanticQueryDiagnostics(SZrCompilerState *cs);
ZR_PARSER_API TZrBool compiler_build_function_metadata_tokens(SZrCompilerState *cs, SZrFunction *function);
TZrBool compiler_scheduler_artifact_record_resolved_call(
        SZrCompilerState *cs,
        const SZrInferredType *receiverType,
        const SZrTypeMemberInfo *memberInfo,
        const SZrResolvedCallSignature *resolvedSignature);

ZR_PARSER_API TZrBool compiler_build_function_semir_metadata(SZrState *state, SZrFunction *function);
ZR_PARSER_API TZrBool compiler_build_function_semir_metadata_shallow(SZrState *state, SZrFunction *function);

ZR_PARSER_API TZrBool compiler_quicken_execbc_function(SZrState *state, SZrFunction *function);
ZR_PARSER_API TZrBool compiler_quicken_execbc_function_shallow(SZrState *state, SZrFunction *function);

ZR_PARSER_API void compile_script(SZrCompilerState *cs, SZrAstNode *node);

TZrBool compiler_validate_task_effects(SZrCompilerState *cs, SZrAstNode *node);
ZR_PARSER_API TZrBool compiler_validate_reference_escapes(
        SZrCompilerState *cs,
        SZrAstNode *node);
ZR_PARSER_API TZrBool compiler_validate_ref_struct_rules(
        SZrCompilerState *cs,
        SZrAstNode *node);

typedef struct SZrRefStructTypeSet {
    SZrArray refLikeTypeNames; /* SZrString * */
    SZrArray interfaceTypeNames; /* SZrString * */
    SZrSemanticContext *semanticContext;
} SZrRefStructTypeSet;

TZrBool compiler_ref_struct_type_set_init(
        SZrRefStructTypeSet *typeSet,
        SZrState *state,
        SZrSemanticContext *semanticContext,
        SZrAstNode *root);
void compiler_ref_struct_type_set_free(
        SZrRefStructTypeSet *typeSet,
        SZrState *state);
TZrBool compiler_ref_struct_type_is_ref_like(
        const SZrRefStructTypeSet *typeSet,
        const SZrType *type);
TZrBool compiler_ref_struct_type_is_boxing_target(
        const SZrRefStructTypeSet *typeSet,
        const SZrType *type);

SZrFunction *ZrParser_Compiler_Compile(SZrState *state, SZrAstNode *ast) ;

void ZrParser_ToGlobalState_Register(struct SZrState *state) ;

ZR_PARSER_API TZrUInt32 ZrParser_Compiler_EmitImportModuleExpression(SZrCompilerState *cs,
                                                                     SZrString *moduleName,
                                                                     SZrFileRange location);
ZR_PARSER_API TZrUInt32 ZrParser_Compiler_EmitImportGuardModuleExpression(SZrCompilerState *cs,
                                                                          SZrString *moduleName,
                                                                          SZrFileRange location);
ZR_PARSER_API TZrUInt32 ZrParser_Compiler_EmitTypeQueryExpression(SZrCompilerState *cs,
                                                                  SZrAstNode *operand,
                                                                  SZrFileRange location);

#endif // ZR_VM_PARSER_COMPILER_INTERNAL_H
