#ifndef ZR_VM_PARSER_COMPILER_INTERNAL_H
#define ZR_VM_PARSER_COMPILER_INTERNAL_H

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/gc.h"
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

typedef struct ZrExternCompilerTempRoot {
    SZrState *state;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor slotAnchor;
    TZrBool active;
} ZrExternCompilerTempRoot;

#pragma pack(push, 1)
typedef struct SZrCompiledPrototypeInfo {
    TZrUInt32 nameStringIndex;
    TZrUInt32 type;
    TZrUInt32 accessModifier;
    TZrUInt32 inheritsCount;
    TZrUInt32 membersCount;
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
    TZrUInt32 isUsingManaged;
    TZrUInt32 ownershipQualifier;
    TZrUInt32 callsClose;
    TZrUInt32 callsDestructor;
    TZrUInt32 declarationOrder;
} SZrCompiledMemberInfo;
#pragma pack(pop)

EZrOwnershipQualifier get_member_receiver_qualifier(SZrAstNode *node) ;

EZrOwnershipQualifier get_implicit_this_ownership_qualifier(EZrOwnershipQualifier receiverQualifier) ;

void ZrParser_CompilerState_Init(SZrCompilerState *cs, SZrState *state) ;

void ZrParser_CompilerState_Free(SZrCompilerState *cs) ;

void print_error_suggestion(const TZrChar *msg) ;

void ZrParser_CompileTime_Error(SZrCompilerState *cs, EZrCompileTimeErrorLevel level, const TZrChar *message, SZrFileRange location) ;

void ZrParser_Compiler_Error(SZrCompilerState *cs, const TZrChar *msg, SZrFileRange location) ;

TZrInstruction create_instruction_0(EZrInstructionCode opcode, TZrUInt16 operandExtra) ;

TZrInstruction create_instruction_1(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrInt32 operand) ;

TZrInstruction create_instruction_2(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrUInt16 operand1,
                                    TZrUInt16 operand2) ;

TZrInstruction create_instruction_4(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrUInt8 op0, TZrUInt8 op1,
                                           TZrUInt8 op2, TZrUInt8 op3) ;

TZrBool compiler_copy_range_to_raw(SZrCompilerState *cs,
                                          TZrPtr *outMemory,
                                          const TZrPtr source,
                                          TZrSize count,
                                          TZrSize elementSize) ;

TZrBool compiler_copy_function_exception_metadata_slice(SZrCompilerState *cs,
                                                               SZrFunction *function,
                                                               TZrSize executionStart,
                                                               TZrSize catchStart,
                                                               TZrSize handlerStart,
                                                               SZrAstNode *sourceNode) ;

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

void ZrParser_Compiler_TrimStackToCount(SZrCompilerState *cs, TZrSize targetCount) ;

void ZrParser_Compiler_TrimStackToSlot(SZrCompilerState *cs, TZrUInt32 slot) ;

void ZrParser_Compiler_TrimStackBy(SZrCompilerState *cs, TZrSize amount) ;

TZrUInt32 find_local_var(SZrCompilerState *cs, SZrString *name) ;

TZrUInt32 find_closure_var(SZrCompilerState *cs, SZrString *name) ;

TZrUInt32 allocate_closure_var(SZrCompilerState *cs, SZrString *name, TZrBool inStack) ;

TZrUInt32 find_child_function_index(SZrCompilerState *cs, SZrString *name) ;

TZrUInt32 generate_function_reference_path_constant(SZrCompilerState *cs, TZrUInt32 childFunctionIndex) ;

TZrUInt32 allocate_stack_slot(SZrCompilerState *cs) ;

SZrString *extract_simple_type_name_from_type_node(SZrAstNode *typeNode) ;

TZrBool compiler_type_has_constructor(SZrCompilerState *cs, SZrString *typeName) ;

void emit_constant_to_slot(SZrCompilerState *cs, TZrUInt32 slot, const SZrTypeValue *value) ;

void emit_string_constant_to_slot(SZrCompilerState *cs, TZrUInt32 slot, SZrString *value) ;

void compiler_register_function_type_binding(SZrCompilerState *cs, SZrFunctionDeclaration *funcDecl) ;

void compiler_register_named_value_binding_to_env(SZrCompilerState *cs,
                                                         SZrTypeEnvironment *env,
                                                         SZrString *name,
                                                         SZrString *typeName) ;

void compiler_register_extern_function_type_binding_to_env(SZrCompilerState *cs,
                                                                  SZrTypeEnvironment *env,
                                                                  SZrExternFunctionDeclaration *functionDecl) ;

TZrUInt32 find_local_var_in_current_scope(SZrCompilerState *cs, SZrString *name) ;

void ZrParser_Compiler_PredeclareFunctionBindings(SZrCompilerState *cs, SZrAstNodeArray *statements) ;

TZrUInt32 emit_load_global_identifier(SZrCompilerState *cs, SZrString *name) ;

TZrInt64 compiler_internal_import_native_entry(SZrState *state) ;

void emit_object_field_assignment_from_expression(SZrCompilerState *cs,
                                                         TZrUInt32 objectSlot,
                                                         SZrString *fieldName,
                                                         SZrAstNode *expression) ;

void emit_class_static_field_initializers(SZrCompilerState *cs, SZrAstNode *classNode) ;

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

void enter_scope(SZrCompilerState *cs) ;

void exit_scope(SZrCompilerState *cs) ;

void enter_type_scope(SZrCompilerState *cs) ;

void exit_type_scope(SZrCompilerState *cs) ;

TZrSize create_label(SZrCompilerState *cs) ;

void resolve_label(SZrCompilerState *cs, TZrSize labelId) ;

void add_pending_jump(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId) ;

void add_pending_absolute_patch(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId) ;

void record_external_var_reference(SZrCompilerState *cs, SZrString *name) ;

void collect_identifiers_from_array(SZrCompilerState *cs, SZrAstNodeArray *nodes, SZrArray *identifierNames) ;

void collect_identifiers_from_node(SZrCompilerState *cs, SZrAstNode *node, SZrArray *identifierNames) ;

void ZrParser_ExternalVariables_Analyze(SZrCompilerState *cs, SZrAstNode *node, SZrCompilerState *parentCompiler) ;

void compress_instructions(SZrCompilerState *cs) ;

void eliminate_redundant_instructions(SZrCompilerState *cs) ;

void optimize_jumps(SZrCompilerState *cs) ;

void optimize_instructions(SZrCompilerState *cs) ;

void compile_function_declaration(SZrCompilerState *cs, SZrAstNode *node) ;

void compile_test_declaration(SZrCompilerState *cs, SZrAstNode *node) ;

SZrString *get_type_name_from_inferred_type(SZrCompilerState *cs, const SZrInferredType *inferredType) ;

SZrString *extract_type_name_string(SZrCompilerState *cs, SZrType *type) ;

TZrUInt32 calculate_type_size(SZrCompilerState *cs, SZrType *type) ;

TZrUInt32 align_offset(TZrUInt32 offset, TZrUInt32 align) ;

TZrUInt32 get_type_alignment(SZrCompilerState *cs, SZrType *type) ;

void compile_struct_declaration(SZrCompilerState *cs, SZrAstNode *node) ;

TZrBool extern_compiler_has_registered_type(SZrCompilerState *cs, SZrString *typeName) ;

void extern_compiler_register_struct_prototype(SZrCompilerState *cs, SZrAstNode *declarationNode) ;

void extern_compiler_register_enum_prototype(SZrCompilerState *cs, SZrAstNode *declarationNode) ;

void compiler_register_extern_block_bindings(SZrCompilerState *cs, SZrExternBlock *externBlock) ;

void ZrParser_Compiler_PredeclareExternBindings(SZrCompilerState *cs, SZrAstNodeArray *statements) ;

void compile_extern_block_declaration(SZrCompilerState *cs, SZrAstNode *node) ;

void ZrParser_Compiler_CompileExternBlock(SZrCompilerState *cs, SZrAstNode *node) ;

void compile_meta_function(SZrCompilerState *cs, SZrAstNode *node, EZrMetaType metaType) ;

SZrFunction *compile_class_member_function(SZrCompilerState *cs, SZrAstNode *node,
                                                  SZrString *superTypeName,
                                                  TZrBool injectThis, TZrUInt32 *outParameterCount) ;

void compile_class_declaration(SZrCompilerState *cs, SZrAstNode *node) ;

TZrBool serialize_prototype_info_to_binary(SZrCompilerState *cs, SZrTypePrototypeInfo *info, 
                                                 TZrByte **outData, TZrSize *outSize) ;

void compile_script(SZrCompilerState *cs, SZrAstNode *node) ;

SZrFunction *ZrParser_Compiler_Compile(SZrState *state, SZrAstNode *ast) ;

TZrBool ZrParser_Compiler_CompileWithTests(SZrState *state, SZrAstNode *ast, SZrCompileResult *result) ;

void ZrParser_CompileResult_Free(SZrState *state, SZrCompileResult *result) ;

void ZrParser_ToGlobalState_Register(struct SZrState *state) ;

ZR_PARSER_API TZrUInt32 ZrParser_Compiler_EmitImportModuleExpression(SZrCompilerState *cs,
                                                                     SZrString *moduleName,
                                                                     SZrFileRange location);

#endif // ZR_VM_PARSER_COMPILER_INTERNAL_H
