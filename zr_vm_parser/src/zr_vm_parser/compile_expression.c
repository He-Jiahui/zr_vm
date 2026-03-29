//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/type_inference.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_vm_conf.h"

#include <string.h>

// 前向声明
ZR_PARSER_API void ZrParser_Expression_Compile(SZrCompilerState *cs, SZrAstNode *node);
static void compile_literal(SZrCompilerState *cs, SZrAstNode *node);
static void compile_template_string_literal(SZrCompilerState *cs, SZrAstNode *node);
static void compile_identifier(SZrCompilerState *cs, SZrAstNode *node);
static void compile_unary_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_type_cast_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_binary_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_logical_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_assignment_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_conditional_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_function_call(SZrCompilerState *cs, SZrAstNode *node);
static void compile_member_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_primary_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_prototype_reference_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_construct_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_array_literal(SZrCompilerState *cs, SZrAstNode *node);
static void compile_object_literal(SZrCompilerState *cs, SZrAstNode *node);
static void compile_lambda_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_if_expression(SZrCompilerState *cs, SZrAstNode *node);
static void compile_switch_expression(SZrCompilerState *cs, SZrAstNode *node);
static SZrTypePrototypeInfo *find_compiler_type_prototype(SZrCompilerState *cs, SZrString *typeName);
static SZrTypeMemberInfo *find_compiler_type_member(SZrCompilerState *cs, SZrString *typeName, SZrString *memberName);
static void collapse_stack_to_slot(SZrCompilerState *cs, TZrUInt32 slot);
static TZrUInt32 normalize_top_result_to_slot(SZrCompilerState *cs, TZrUInt32 targetSlot);
static TZrUInt32 emit_string_constant(SZrCompilerState *cs, SZrString *str);
static TZrUInt32 compile_expression_into_slot(SZrCompilerState *cs, SZrAstNode *node, TZrUInt32 targetSlot);
static TZrBool emit_property_getter_call(SZrCompilerState *cs, TZrUInt32 currentSlot, SZrString *propertyName,
                                         TZrBool isStatic, SZrFileRange location);
static TZrUInt32 compile_member_key_into_slot(SZrCompilerState *cs, SZrMemberExpression *memberExpr,
                                              TZrUInt32 targetSlot);
static SZrString *resolve_construct_target_type_name(SZrCompilerState *cs, SZrAstNode *target,
                                                     EZrObjectPrototypeType *outPrototypeType);
static TZrBool resolve_expression_root_type(SZrCompilerState *cs, SZrAstNode *node, SZrString **outTypeName,
                                            TZrBool *outIsTypeReference);
static SZrAstNode *find_function_declaration(SZrCompilerState *cs, SZrString *funcName);
static SZrAstNodeArray *match_named_arguments(SZrCompilerState *cs, SZrFunctionCall *call,
                                              SZrAstNodeArray *paramList);
static void compile_primary_member_chain(SZrCompilerState *cs, SZrAstNode *propertyNode, SZrAstNodeArray *members,
                                         TZrSize memberStartIndex, TZrUInt32 *ioCurrentSlot,
                                         SZrString **ioRootTypeName, TZrBool *ioRootIsTypeReference);
// 辅助函数声明（在 compiler.c 中实现，需要声明为 extern 或包含头文件）
// 为了简化，我们直接在 ZrParser_Expression_Compile.c 中重新声明这些函数
// 注意：这些函数应该在同一编译单元中，或者通过头文件共享

// 前向声明辅助函数（实际实现在 compiler.c 中）
TZrInstruction create_instruction_0(EZrInstructionCode opcode, TZrUInt16 operandExtra);
TZrInstruction create_instruction_1(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrInt32 operand);
TZrInstruction create_instruction_2(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrUInt16 operand1, TZrUInt16 operand2);
void emit_instruction(SZrCompilerState *cs, TZrInstruction instruction);
TZrUInt32 add_constant(SZrCompilerState *cs, SZrTypeValue *value);
TZrUInt32 find_local_var(SZrCompilerState *cs, SZrString *name);
TZrUInt32 find_closure_var(SZrCompilerState *cs, SZrString *name);
TZrUInt32 allocate_closure_var(SZrCompilerState *cs, SZrString *name, TZrBool inStack);
TZrUInt32 allocate_stack_slot(SZrCompilerState *cs);
TZrUInt32 find_child_function_index(SZrCompilerState *cs, SZrString *name);
TZrUInt32 allocate_local_var(SZrCompilerState *cs, SZrString *name);
TZrSize create_label(SZrCompilerState *cs);
void resolve_label(SZrCompilerState *cs, TZrSize labelId);
void add_pending_jump(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId);
void enter_scope(SZrCompilerState *cs);
void exit_scope(SZrCompilerState *cs);
void ZrParser_Statement_Compile(SZrCompilerState *cs, SZrAstNode *node);

static void emit_constant_to_slot_local(SZrCompilerState *cs, TZrUInt32 slot, const SZrTypeValue *value,
                                        SZrFileRange location) {
    if (cs == ZR_NULL || value == ZR_NULL || cs->hasError) {
        return;
    }

    if (!ZrParser_Compiler_ValidateRuntimeProjectionValue(cs, value, location)) {
        return;
    }

    SZrTypeValue constantValue = *value;
    TZrUInt32 constantIndex = add_constant(cs, &constantValue);
    TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)slot, (TZrInt32)constantIndex);
    emit_instruction(cs, inst);
}

static TZrBool zr_string_equals_cstr_local(SZrString *value, const TZrChar *literal) {
    TZrNativeString nativeValue;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeValue = ZrCore_String_GetNativeString(value);
    return nativeValue != ZR_NULL && strcmp(nativeValue, literal) == 0;
}

static TZrBool has_compile_time_variable_binding_local(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < cs->compileTimeVariables.length; index++) {
        SZrCompileTimeVariable **varPtr =
                (SZrCompileTimeVariable **)ZrCore_Array_Get(&cs->compileTimeVariables, index);
        if (varPtr != ZR_NULL && *varPtr != ZR_NULL && (*varPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*varPtr)->name, name)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool has_compile_time_function_binding_local(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < cs->compileTimeFunctions.length; index++) {
        SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get(&cs->compileTimeFunctions, index);
        if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL && (*funcPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*funcPtr)->name, name)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool is_compile_time_projection_candidate(SZrCompilerState *cs, SZrString *rootName) {
    if (cs == ZR_NULL || rootName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (zr_string_equals_cstr_local(rootName, "Assert") ||
        zr_string_equals_cstr_local(rootName, "FatalError") ||
        zr_string_equals_cstr_local(rootName, "import")) {
        return ZR_TRUE;
    }

    return has_compile_time_variable_binding_local(cs, rootName) ||
           has_compile_time_function_binding_local(cs, rootName);
}

static TZrBool try_emit_compile_time_function_call(SZrCompilerState *cs, SZrAstNode *node) {
    SZrPrimaryExpression *primary;
    SZrString *rootName;
    SZrTypeValue compileTimeValue;
    TZrUInt32 destSlot;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primary = &node->data.primaryExpression;
    if (primary->property == ZR_NULL || primary->property->type != ZR_AST_IDENTIFIER_LITERAL ||
        primary->members == ZR_NULL || primary->members->count == 0) {
        return ZR_FALSE;
    }

    rootName = primary->property->data.identifier.name;
    if (zr_string_equals_cstr_local(rootName, "import")) {
        SZrAstNode *tailMember = primary->members->nodes[primary->members->count - 1];
        if (primary->members->count < 2 || tailMember == ZR_NULL ||
            tailMember->type != ZR_AST_FUNCTION_CALL) {
            return ZR_FALSE;
        }
    }

    if (!is_compile_time_projection_candidate(cs, rootName)) {
        return ZR_FALSE;
    }

    if (!ZrParser_Compiler_EvaluateCompileTimeExpression(cs, node, &compileTimeValue)) {
        return cs->hasError || cs->hasCompileTimeError || cs->hasFatalError;
    }

    destSlot = allocate_stack_slot(cs);
    emit_constant_to_slot_local(cs, destSlot, &compileTimeValue, node->location);
    return ZR_TRUE;
}

// 类型转换辅助函数
static void emit_type_conversion(SZrCompilerState *cs, TZrUInt32 destSlot, TZrUInt32 srcSlot, EZrInstructionCode conversionOpcode) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    // 类型转换指令格式: operandExtra = destSlot, operand1[0] = srcSlot, operand1[1] = 0
    TZrInstruction convInst = create_instruction_2(conversionOpcode, (TZrUInt16)destSlot, (TZrUInt16)srcSlot, 0);
    emit_instruction(cs, convInst);
}

// 带原型信息的类型转换辅助函数（用于 TO_STRUCT 和 TO_OBJECT）
static void emit_type_conversion_with_prototype(SZrCompilerState *cs, TZrUInt32 destSlot, TZrUInt32 srcSlot, 
                                                EZrInstructionCode conversionOpcode, TZrUInt32 prototypeConstantIndex) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    // 类型转换指令格式: operandExtra = destSlot, operand1[0] = srcSlot, operand1[1] = prototypeConstantIndex
    TZrInstruction convInst = create_instruction_2(conversionOpcode, (TZrUInt16)destSlot, (TZrUInt16)srcSlot, (TZrUInt16)prototypeConstantIndex);
    emit_instruction(cs, convInst);
}

static EZrValueType binary_expression_effective_type_after_conversion(EZrValueType originalType,
                                                                      EZrInstructionCode conversionOpcode) {
    switch (conversionOpcode) {
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
            return ZR_VALUE_TYPE_BOOL;
        case ZR_INSTRUCTION_ENUM(TO_INT):
            return ZR_VALUE_TYPE_INT64;
        case ZR_INSTRUCTION_ENUM(TO_UINT):
            return ZR_VALUE_TYPE_UINT64;
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
            return ZR_VALUE_TYPE_DOUBLE;
        case ZR_INSTRUCTION_ENUM(TO_STRING):
            return ZR_VALUE_TYPE_STRING;
        default:
            return originalType;
    }
}

static TZrBool binary_expression_type_is_float_like(EZrValueType type) {
    return (TZrBool)(type == ZR_VALUE_TYPE_FLOAT || type == ZR_VALUE_TYPE_DOUBLE);
}

static void update_identifier_assignment_type_environment(SZrCompilerState *cs,
                                                          SZrString *name,
                                                          SZrAstNode *valueExpression) {
    SZrInferredType inferredType;

    if (cs == ZR_NULL || name == ZR_NULL || valueExpression == ZR_NULL || cs->typeEnv == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_ExpressionType_Infer(cs, valueExpression, &inferredType)) {
        ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, name, &inferredType);
    }
    ZrParser_InferredType_Free(cs->state, &inferredType);
}

// 在脚本 AST 中查找类型定义（struct 或 class）
// 返回找到的 AST 节点，如果未找到返回 ZR_NULL
static SZrAstNode *find_type_declaration(SZrCompilerState *cs, SZrString *typeName) {
    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    if (cs->scriptAst->type != ZR_AST_SCRIPT) {
        return ZR_NULL;
    }
    
    SZrScript *script = &cs->scriptAst->data.script;
    if (script->statements == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 遍历顶层语句，查找 struct 或 class 声明
    for (TZrSize i = 0; i < script->statements->count; i++) {
        SZrAstNode *stmt = script->statements->nodes[i];
        if (stmt == ZR_NULL) {
            continue;
        }
        
        // 检查是否是 struct 声明
        if (stmt->type == ZR_AST_STRUCT_DECLARATION) {
            SZrIdentifier *structName = stmt->data.structDeclaration.name;
            if (structName != ZR_NULL && structName->name != ZR_NULL) {
                if (ZrCore_String_Equal(structName->name, typeName)) {
                    return stmt;
                }
            }
        }
        
        // 检查是否是 class 声明
        if (stmt->type == ZR_AST_CLASS_DECLARATION) {
            SZrIdentifier *className = stmt->data.classDeclaration.name;
            if (className != ZR_NULL && className->name != ZR_NULL) {
                if (ZrCore_String_Equal(className->name, typeName)) {
                    return stmt;
                }
            }
        }
    }
    
    return ZR_NULL;
}

static SZrString *create_hidden_property_accessor_name(SZrCompilerState *cs, SZrString *propertyName,
                                                       TZrBool isSetter) {
    if (cs == ZR_NULL || propertyName == ZR_NULL) {
        return ZR_NULL;
    }

    const TZrChar *prefix = isSetter ? "__set_" : "__get_";
    TZrNativeString propertyNameString = ZrCore_String_GetNativeString(propertyName);
    if (propertyNameString == ZR_NULL) {
        return ZR_NULL;
    }

    TZrSize prefixLength = strlen(prefix);
    TZrSize propertyNameLength = propertyName->shortStringLength < ZR_VM_LONG_STRING_FLAG
                                         ? propertyName->shortStringLength
                                         : propertyName->longStringLength;
    TZrSize bufferSize = prefixLength + propertyNameLength + 1;
    TZrChar *buffer = (TZrChar *)ZrCore_Memory_RawMalloc(cs->state->global, bufferSize);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(buffer, prefix, prefixLength);
    memcpy(buffer + prefixLength, propertyNameString, propertyNameLength);
    buffer[bufferSize - 1] = '\0';

    SZrString *result = ZrCore_String_CreateFromNative(cs->state, buffer);
    ZrCore_Memory_RawFree(cs->state->global, buffer, bufferSize);
    return result;
}

// 辅助函数：查找类型成员信息（检查字段是否是 const）
static TZrBool find_type_member_is_const(SZrCompilerState *cs, SZrString *typeName, SZrString *memberName) {
    if (cs == ZR_NULL || typeName == ZR_NULL || memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    SZrTypeMemberInfo *memberInfo = find_compiler_type_member(cs, typeName, memberName);
    if (memberInfo != ZR_NULL &&
        (memberInfo->memberType == ZR_AST_STRUCT_FIELD || memberInfo->memberType == ZR_AST_CLASS_FIELD) &&
        memberInfo->isConst) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static SZrTypePrototypeInfo *find_compiler_type_prototype(SZrCompilerState *cs, SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
        if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, typeName)) {
            return info;
        }
    }

    // Fall back to the in-progress prototype only when the type has not been
    // published into the stable prototype table yet.
    if (cs->currentTypePrototypeInfo != ZR_NULL && cs->currentTypePrototypeInfo->name != ZR_NULL &&
        ZrCore_String_Equal(cs->currentTypePrototypeInfo->name, typeName)) {
        return cs->currentTypePrototypeInfo;
    }

    return ZR_NULL;
}

static SZrTypeMemberInfo *find_compiler_type_member_recursive(SZrCompilerState *cs, SZrTypePrototypeInfo *info,
                                                              SZrString *memberName, TZrUInt32 depth) {
    if (cs == ZR_NULL || info == ZR_NULL || memberName == ZR_NULL || depth > 32) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < info->members.length; i++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, i);
        if (memberInfo != ZR_NULL && memberInfo->name != ZR_NULL && ZrCore_String_Equal(memberInfo->name, memberName)) {
            return memberInfo;
        }
    }

    for (TZrSize i = 0; i < info->inherits.length; i++) {
        SZrString **inheritTypeNamePtr = (SZrString **)ZrCore_Array_Get(&info->inherits, i);
        if (inheritTypeNamePtr == ZR_NULL || *inheritTypeNamePtr == ZR_NULL) {
            continue;
        }

        SZrTypePrototypeInfo *superInfo = find_compiler_type_prototype(cs, *inheritTypeNamePtr);
        if (superInfo == ZR_NULL || superInfo == info) {
            continue;
        }

        SZrTypeMemberInfo *inheritedMember =
                find_compiler_type_member_recursive(cs, superInfo, memberName, depth + 1);
        if (inheritedMember != ZR_NULL) {
            return inheritedMember;
        }
    }

    return ZR_NULL;
}

static SZrTypeMemberInfo *find_compiler_type_member(SZrCompilerState *cs, SZrString *typeName, SZrString *memberName) {
    SZrTypePrototypeInfo *info = find_compiler_type_prototype(cs, typeName);
    if (info == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    return find_compiler_type_member_recursive(cs, info, memberName, 0);
}

static TZrBool type_name_is_registered_prototype(SZrCompilerState *cs, SZrString *typeName) {
    SZrTypePrototypeInfo *info = find_compiler_type_prototype(cs, typeName);
    return info != ZR_NULL && info->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
}

static TZrBool type_has_constructor(SZrCompilerState *cs, SZrString *typeName) {
    SZrTypePrototypeInfo *info = find_compiler_type_prototype(cs, typeName);
    if (info == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < info->members.length; i++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, i);
        if (memberInfo != ZR_NULL && memberInfo->isMetaMethod && memberInfo->metaType == ZR_META_CONSTRUCTOR) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrString *resolve_expression_type_name(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_NULL;
    }

    SZrInferredType inferredType;
    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, node, &inferredType)) {
        return ZR_NULL;
    }

    SZrString *typeName = inferredType.typeName;
    ZrParser_InferredType_Free(cs->state, &inferredType);
    return typeName;
}

static TZrBool resolve_primary_expression_root_type(SZrCompilerState *cs,
                                                    SZrPrimaryExpression *primary,
                                                    SZrString **outTypeName,
                                                    TZrBool *outIsTypeReference) {
    SZrString *currentTypeName = ZR_NULL;
    TZrBool currentIsTypeReference = ZR_FALSE;

    if (cs == ZR_NULL || primary == ZR_NULL || outTypeName == ZR_NULL || outIsTypeReference == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!resolve_expression_root_type(cs, primary->property, &currentTypeName, &currentIsTypeReference) ||
        currentTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (primary->members != ZR_NULL) {
        for (TZrSize i = 0; i < primary->members->count; i++) {
            SZrAstNode *memberNode = primary->members->nodes[i];
            SZrMemberExpression *memberExpr;
            SZrString *memberName;
            SZrTypeMemberInfo *memberInfo;
            SZrString *nextTypeName = ZR_NULL;
            SZrTypePrototypeInfo *nextTypeInfo = ZR_NULL;

            if (memberNode == ZR_NULL) {
                continue;
            }
            if (memberNode->type != ZR_AST_MEMBER_EXPRESSION) {
                break;
            }

            memberExpr = &memberNode->data.memberExpression;
            if (memberExpr->computed || memberExpr->property == ZR_NULL ||
                memberExpr->property->type != ZR_AST_IDENTIFIER_LITERAL || currentTypeName == ZR_NULL) {
                break;
            }

            memberName = memberExpr->property->data.identifier.name;
            memberInfo = find_compiler_type_member(cs, currentTypeName, memberName);
            if (memberInfo == ZR_NULL) {
                break;
            }

            if ((memberInfo->memberType == ZR_AST_STRUCT_FIELD || memberInfo->memberType == ZR_AST_CLASS_FIELD) &&
                memberInfo->fieldTypeName != ZR_NULL) {
                nextTypeName = memberInfo->fieldTypeName;
            } else if ((memberInfo->memberType == ZR_AST_STRUCT_METHOD ||
                        memberInfo->memberType == ZR_AST_CLASS_METHOD ||
                        memberInfo->memberType == ZR_AST_STRUCT_META_FUNCTION ||
                        memberInfo->memberType == ZR_AST_CLASS_META_FUNCTION) &&
                       memberInfo->returnTypeName != ZR_NULL) {
                nextTypeName = memberInfo->returnTypeName;
            }

            if (nextTypeName == ZR_NULL) {
                break;
            }

            currentTypeName = nextTypeName;
            nextTypeInfo = find_compiler_type_prototype(cs, currentTypeName);
            currentIsTypeReference =
                    nextTypeInfo != ZR_NULL && nextTypeInfo->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
        }
    }

    *outTypeName = currentTypeName;
    *outIsTypeReference = currentIsTypeReference;
    return currentTypeName != ZR_NULL;
}

static TZrBool resolve_expression_root_type(SZrCompilerState *cs, SZrAstNode *node, SZrString **outTypeName,
                                          TZrBool *outIsTypeReference) {
    if (outTypeName == ZR_NULL || outIsTypeReference == ZR_NULL) {
        return ZR_FALSE;
    }

    *outTypeName = ZR_NULL;
    *outIsTypeReference = ZR_FALSE;
    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *candidateType = node->data.identifier.name;
        if (find_compiler_type_prototype(cs, candidateType) != ZR_NULL) {
            *outTypeName = candidateType;
            *outIsTypeReference = ZR_TRUE;
            return ZR_TRUE;
        }
    }

    if (node->type == ZR_AST_PRIMARY_EXPRESSION) {
        return resolve_primary_expression_root_type(cs, &node->data.primaryExpression, outTypeName, outIsTypeReference);
    }

    *outTypeName = resolve_expression_type_name(cs, node);
    return *outTypeName != ZR_NULL;
}

static SZrString *resolve_construct_target_type_name(SZrCompilerState *cs, SZrAstNode *target,
                                                     EZrObjectPrototypeType *outPrototypeType) {
    SZrString *typeName = ZR_NULL;
    TZrBool isTypeReference = ZR_FALSE;
    SZrTypePrototypeInfo *prototypeInfo = ZR_NULL;
    SZrAstNode *typeDecl = ZR_NULL;

    if (outPrototypeType != ZR_NULL) {
        *outPrototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
    }
    if (cs == ZR_NULL || target == ZR_NULL) {
        return ZR_NULL;
    }

    if (target->type == ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION) {
        target = target->data.prototypeReferenceExpression.target;
        if (target == ZR_NULL) {
            return ZR_NULL;
        }
    }

    if (!resolve_expression_root_type(cs, target, &typeName, &isTypeReference) || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    prototypeInfo = find_compiler_type_prototype(cs, typeName);
    if (prototypeInfo != ZR_NULL) {
        if (outPrototypeType != ZR_NULL) {
            *outPrototypeType = prototypeInfo->type;
        }
        return typeName;
    }

    typeDecl = find_type_declaration(cs, typeName);
    if (typeDecl != ZR_NULL) {
        if (outPrototypeType != ZR_NULL) {
            if (typeDecl->type == ZR_AST_STRUCT_DECLARATION) {
                *outPrototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
            } else if (typeDecl->type == ZR_AST_CLASS_DECLARATION) {
                *outPrototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
            }
        }
        return typeName;
    }

    return ZR_NULL;
}

static TZrBool emit_hidden_constructor_call(SZrCompilerState *cs,
                                            TZrUInt32 instanceSlot,
                                            SZrAstNodeArray *constructorArgs,
                                            SZrString *typeName,
                                            SZrFileRange location) {
    TZrUInt32 constructorKeySlot;
    TZrUInt32 functionSlot;
    TZrUInt32 receiverSlot;
    TZrUInt32 argCount = 1;

    if (cs == ZR_NULL || cs->hasError || typeName == ZR_NULL || !type_has_constructor(cs, typeName)) {
        return ZR_TRUE;
    }

    constructorKeySlot = emit_string_constant(cs, ZrCore_String_CreateFromNative(cs->state, "__constructor"));
    if (constructorKeySlot == (TZrUInt32)-1) {
        return ZR_FALSE;
    }

    constructorKeySlot = normalize_top_result_to_slot(cs, instanceSlot + 1);
    functionSlot = allocate_stack_slot(cs);
    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE),
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)instanceSlot,
                                          (TZrUInt16)constructorKeySlot));

    receiverSlot = allocate_stack_slot(cs);
    emit_instruction(cs,
                     create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                          (TZrUInt16)receiverSlot,
                                          (TZrInt32)instanceSlot));

    if (constructorArgs != ZR_NULL) {
        for (TZrSize i = 0; i < constructorArgs->count; i++) {
            SZrAstNode *argNode = constructorArgs->nodes[i];
            if (argNode != ZR_NULL &&
                compile_expression_into_slot(cs, argNode, receiverSlot + 1 + (TZrUInt32)i) == (TZrUInt32)-1) {
                return ZR_FALSE;
            }
        }
        argCount += (TZrUInt32)constructorArgs->count;
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)argCount));
    collapse_stack_to_slot(cs, instanceSlot);
    if (cs->hasError) {
        ZrParser_Compiler_Error(cs, "Failed to invoke prototype constructor", location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool emit_construct_seed_instance(SZrCompilerState *cs,
                                            TZrUInt32 destSlot,
                                            EZrObjectPrototypeType prototypeType,
                                            TZrUInt32 typeNameConstantIndex,
                                            SZrFileRange location) {
    if (cs == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    emit_instruction(cs, create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_OBJECT), (TZrUInt16)destSlot));

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        emit_instruction(cs,
                         create_instruction_2(ZR_INSTRUCTION_ENUM(TO_STRUCT),
                                              (TZrUInt16)destSlot,
                                              (TZrUInt16)destSlot,
                                              (TZrUInt16)typeNameConstantIndex));
        return ZR_TRUE;
    }

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        emit_instruction(cs,
                         create_instruction_2(ZR_INSTRUCTION_ENUM(TO_OBJECT),
                                              (TZrUInt16)destSlot,
                                              (TZrUInt16)destSlot,
                                              (TZrUInt16)typeNameConstantIndex));
        return ZR_TRUE;
    }

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
        ZrParser_Compiler_Error(cs, "Enum construction is not implemented yet", location);
    } else {
        ZrParser_Compiler_Error(cs, "Unsupported construct target prototype kind", location);
    }
    return ZR_FALSE;
}

static TZrUInt32 emit_shorthand_constructor_instance(SZrCompilerState *cs, const TZrChar *op, SZrString *typeName,
                                                     SZrAstNodeArray *constructorArgs, SZrFileRange location) {
    TZrUInt32 destSlot;
    SZrTypePrototypeInfo *prototypeInfo;
    SZrAstNode *typeDecl;
    EZrObjectPrototypeType prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
    TZrBool allowValueConstruction = ZR_FALSE;
    TZrBool allowBoxedConstruction = ZR_FALSE;
    SZrTypeValue typeNameValue;
    TZrUInt32 typeNameConstantIndex;

    if (cs == ZR_NULL || op == ZR_NULL || typeName == ZR_NULL) {
        return (TZrUInt32)-1;
    }

    prototypeInfo = find_compiler_type_prototype(cs, typeName);
    if (prototypeInfo != ZR_NULL) {
        prototypeType = prototypeInfo->type;
        allowValueConstruction = prototypeInfo->allowValueConstruction;
        allowBoxedConstruction = prototypeInfo->allowBoxedConstruction;
    } else {
        typeDecl = find_type_declaration(cs, typeName);
        if (typeDecl != ZR_NULL) {
            if (typeDecl->type == ZR_AST_STRUCT_DECLARATION) {
                prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
            } else if (typeDecl->type == ZR_AST_CLASS_DECLARATION) {
                prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
            }
        }
        allowValueConstruction = prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INVALID &&
                                 prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE &&
                                 prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
        allowBoxedConstruction = allowValueConstruction;
    }

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_INVALID ||
        prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE ||
        prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
        ZrParser_Compiler_Error(cs, "Construct target must resolve to a registered constructible prototype", location);
        return (TZrUInt32)-1;
    }

    if (strcmp(op, "$") == 0 && !allowValueConstruction) {
        ZrParser_Compiler_Error(cs, "Prototype does not allow value construction", location);
        return (TZrUInt32)-1;
    }

    if (strcmp(op, "new") == 0 && !allowBoxedConstruction) {
        ZrParser_Compiler_Error(cs, "Prototype does not allow boxed construction", location);
        return (TZrUInt32)-1;
    }

    destSlot = allocate_stack_slot(cs);
    ZrCore_Value_InitAsRawObject(cs->state, &typeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
    typeNameValue.type = ZR_VALUE_TYPE_STRING;
    typeNameConstantIndex = add_constant(cs, &typeNameValue);

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
        if (constructorArgs == ZR_NULL || constructorArgs->count != 1) {
            ZrParser_Compiler_Error(cs, "Enum construction requires exactly one underlying value argument", location);
            return (TZrUInt32)-1;
        }

        if (compile_expression_into_slot(cs, constructorArgs->nodes[0], destSlot) == (TZrUInt32)-1) {
            return (TZrUInt32)-1;
        }

        emit_instruction(cs,
                         create_instruction_2(ZR_INSTRUCTION_ENUM(TO_OBJECT),
                                              (TZrUInt16)destSlot,
                                              (TZrUInt16)destSlot,
                                              (TZrUInt16)typeNameConstantIndex));
        collapse_stack_to_slot(cs, destSlot);
        return destSlot;
    }

    if (!emit_construct_seed_instance(cs, destSlot, prototypeType, typeNameConstantIndex, location)) {
        return (TZrUInt32)-1;
    }

    if (!emit_hidden_constructor_call(cs, destSlot, constructorArgs, typeName, location)) {
        return (TZrUInt32)-1;
    }

    collapse_stack_to_slot(cs, destSlot);
    return destSlot;
}

static SZrTypeMemberInfo *find_hidden_property_accessor_member(SZrCompilerState *cs, SZrString *typeName,
                                                               SZrString *propertyName, TZrBool isSetter) {
    if (cs == ZR_NULL || typeName == ZR_NULL || propertyName == ZR_NULL) {
        return ZR_NULL;
    }

    SZrString *accessorName = create_hidden_property_accessor_name(cs, propertyName, isSetter);
    if (accessorName == ZR_NULL) {
        return ZR_NULL;
    }

    return find_compiler_type_member(cs, typeName, accessorName);
}

static TZrBool can_use_property_accessor(TZrBool rootIsTypeReference, SZrTypeMemberInfo *accessorMember) {
    if (accessorMember == ZR_NULL) {
        return ZR_FALSE;
    }

    if (rootIsTypeReference && !accessorMember->isStatic) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool member_call_requires_bound_receiver(const SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL || memberInfo->isStatic) {
        return ZR_FALSE;
    }

    return memberInfo->memberType == ZR_AST_STRUCT_METHOD || memberInfo->memberType == ZR_AST_CLASS_METHOD;
}

static void compile_primary_member_chain(SZrCompilerState *cs, SZrAstNode *propertyNode, SZrAstNodeArray *members,
                                         TZrSize memberStartIndex, TZrUInt32 *ioCurrentSlot,
                                         SZrString **ioRootTypeName, TZrBool *ioRootIsTypeReference) {
    TZrUInt32 currentSlot;
    TZrUInt32 pendingReceiverSlot = (TZrUInt32)-1;
    SZrString *pendingCallResultTypeName = ZR_NULL;
    SZrString *rootTypeName = ioRootTypeName != ZR_NULL ? *ioRootTypeName : ZR_NULL;
    TZrBool rootIsTypeReference = ioRootIsTypeReference != ZR_NULL ? *ioRootIsTypeReference : ZR_FALSE;

    if (cs == ZR_NULL || ioCurrentSlot == ZR_NULL || members == ZR_NULL || cs->hasError) {
        return;
    }

    currentSlot = *ioCurrentSlot;
    for (TZrSize i = memberStartIndex; i < members->count; i++) {
        SZrAstNode *member = members->nodes[i];
        if (member == ZR_NULL) {
            continue;
        }

        if (member->type == ZR_AST_MEMBER_EXPRESSION) {
            SZrMemberExpression *memberExpr = &member->data.memberExpression;
            SZrString *memberName = ZR_NULL;
            TZrBool isStaticMember = ZR_FALSE;
            TZrBool bindReceiverForCall = ZR_FALSE;
            SZrTypeMemberInfo *typeMember = ZR_NULL;
            SZrTypeMemberInfo *getterAccessor = ZR_NULL;
            TZrBool nextIsFunctionCall =
                    (i + 1 < members->count &&
                     members->nodes[i + 1] != ZR_NULL &&
                     members->nodes[i + 1]->type == ZR_AST_FUNCTION_CALL);

            if (!memberExpr->computed && memberExpr->property != ZR_NULL &&
                memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                memberName = memberExpr->property->data.identifier.name;
            }

            if (rootTypeName != ZR_NULL && memberName != ZR_NULL) {
                typeMember = find_compiler_type_member(cs, rootTypeName, memberName);
                if (typeMember != ZR_NULL && typeMember->isStatic) {
                    isStaticMember = ZR_TRUE;
                }
                bindReceiverForCall = member_call_requires_bound_receiver(typeMember);
                pendingCallResultTypeName =
                        nextIsFunctionCall && typeMember != ZR_NULL ? typeMember->returnTypeName : ZR_NULL;

                getterAccessor = find_hidden_property_accessor_member(cs, rootTypeName, memberName, ZR_FALSE);
                if (!can_use_property_accessor(rootIsTypeReference, getterAccessor)) {
                    getterAccessor = ZR_NULL;
                }
            }

            if (memberExpr->property != ZR_NULL) {
                if (getterAccessor != ZR_NULL && memberName != ZR_NULL && !memberExpr->computed) {
                    if (!emit_property_getter_call(cs, currentSlot, memberName, getterAccessor->isStatic,
                                                   member->location)) {
                        return;
                    }
                    pendingReceiverSlot = (TZrUInt32)-1;
                    rootTypeName = getterAccessor->returnTypeName;
                    rootIsTypeReference = getterAccessor->isStatic &&
                                          rootTypeName != ZR_NULL &&
                                          type_name_is_registered_prototype(cs, rootTypeName);
                } else {
                    if (nextIsFunctionCall && bindReceiverForCall) {
                        pendingReceiverSlot = allocate_stack_slot(cs);
                        emit_instruction(cs, create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                                                  (TZrUInt16)pendingReceiverSlot,
                                                                  (TZrInt32)currentSlot));
                    } else {
                        pendingReceiverSlot = (TZrUInt32)-1;
                    }

                    TZrUInt32 keyTargetSlot =
                            (pendingReceiverSlot != (TZrUInt32)-1) ? currentSlot + 2 : currentSlot + 1;
                    TZrUInt32 keySlot = compile_member_key_into_slot(cs, memberExpr, keyTargetSlot);
                    if (keySlot == (TZrUInt32)-1) {
                        return;
                    }

                    emit_instruction(cs,
                                     create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE),
                                                          (TZrUInt16)currentSlot,
                                                          (TZrUInt16)currentSlot,
                                                          (TZrUInt16)keySlot));
                    if (pendingReceiverSlot == (TZrUInt32)-1) {
                        collapse_stack_to_slot(cs, currentSlot);
                    }

                    if (typeMember != ZR_NULL &&
                        (typeMember->memberType == ZR_AST_STRUCT_FIELD ||
                         typeMember->memberType == ZR_AST_CLASS_FIELD)) {
                        rootTypeName = typeMember->fieldTypeName;
                        rootIsTypeReference = isStaticMember &&
                                              rootTypeName != ZR_NULL &&
                                              type_name_is_registered_prototype(cs, rootTypeName);
                    } else if (typeMember != ZR_NULL) {
                        rootTypeName = ZR_NULL;
                        rootIsTypeReference = ZR_FALSE;
                    } else if (!isStaticMember) {
                        rootTypeName = ZR_NULL;
                        rootIsTypeReference = ZR_FALSE;
                    }
                }
            }
        } else if (member->type == ZR_AST_FUNCTION_CALL) {
            SZrFunctionCall *call = &member->data.functionCall;
            SZrAstNodeArray *argsToCompile = call->args;

            if (rootIsTypeReference) {
                ZrParser_Compiler_Error(cs,
                                        "Prototype references are not callable; use $target(...) or new target(...)",
                                        member->location);
                return;
            }

            if (propertyNode != ZR_NULL && propertyNode->type == ZR_AST_IDENTIFIER_LITERAL) {
                SZrString *funcName = propertyNode->data.identifier.name;
                TZrUInt32 childFuncIndex = find_child_function_index(cs, funcName);

                if (childFuncIndex != (TZrUInt32)-1) {
                    ZR_UNUSED_PARAMETER(ZrCore_Array_Get(&cs->childFunctions, childFuncIndex));
                }

                {
                    SZrAstNode *funcDecl = find_function_declaration(cs, funcName);
                    if (funcDecl != ZR_NULL && funcDecl->type == ZR_AST_FUNCTION_DECLARATION) {
                        SZrFunctionDeclaration *funcDeclData = &funcDecl->data.functionDeclaration;
                        if (funcDeclData->params != ZR_NULL) {
                            argsToCompile = match_named_arguments(cs, call, funcDeclData->params);
                        }
                    }
                }
            }

            TZrUInt32 argCount = 0;
            TZrUInt32 argBaseSlot = currentSlot + 1;
            if (pendingReceiverSlot != (TZrUInt32)-1) {
                argCount = 1;
                argBaseSlot = pendingReceiverSlot + 1;
            }

            if (argsToCompile != ZR_NULL) {
                for (TZrSize j = 0; j < argsToCompile->count; j++) {
                    SZrAstNode *argNode = argsToCompile->nodes[j];
                    if (argNode != ZR_NULL) {
                        TZrUInt32 argSlot =
                                compile_expression_into_slot(cs, argNode, argBaseSlot + (TZrUInt32)j);
                        if (argSlot == (TZrUInt32)-1 || cs->hasError) {
                            break;
                        }
                    }
                }
                argCount += (TZrUInt32)argsToCompile->count;
            }

            emit_instruction(cs,
                             create_instruction_2(cs->isInTailCallContext ?
                                                          ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL) :
                                                          ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                                  (TZrUInt16)currentSlot,
                                                  (TZrUInt16)currentSlot,
                                                  (TZrUInt16)argCount));
            collapse_stack_to_slot(cs, currentSlot);
            pendingReceiverSlot = (TZrUInt32)-1;
            rootTypeName = pendingCallResultTypeName;
            pendingCallResultTypeName = ZR_NULL;
            rootIsTypeReference = ZR_FALSE;

            if (argsToCompile != call->args && argsToCompile != ZR_NULL) {
                ZrParser_AstNodeArray_Free(cs->state, argsToCompile);
            }
        }
    }

    *ioCurrentSlot = currentSlot;
    if (ioRootTypeName != ZR_NULL) {
        *ioRootTypeName = rootTypeName;
    }
    if (ioRootIsTypeReference != ZR_NULL) {
        *ioRootIsTypeReference = rootIsTypeReference;
    }
}

static void collapse_stack_to_slot(SZrCompilerState *cs, TZrUInt32 slot) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrParser_Compiler_TrimStackToSlot(cs, slot);
}

static TZrUInt32 normalize_top_result_to_slot(SZrCompilerState *cs, TZrUInt32 targetSlot) {
    if (cs == ZR_NULL || cs->hasError || cs->stackSlotCount == 0) {
        return (TZrUInt32)-1;
    }

    TZrUInt32 resultSlot = (TZrUInt32)(cs->stackSlotCount - 1);
    if (resultSlot != targetSlot) {
        TZrInstruction copyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)targetSlot, (TZrInt32)resultSlot);
        emit_instruction(cs, copyInst);
    }

    collapse_stack_to_slot(cs, targetSlot);
    return targetSlot;
}

static void compile_expression_non_tail(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    TZrBool oldTailCallContext = cs->isInTailCallContext;
    cs->isInTailCallContext = ZR_FALSE;
    ZrParser_Expression_Compile(cs, node);
    cs->isInTailCallContext = oldTailCallContext;
}

static TZrUInt32 emit_string_constant(SZrCompilerState *cs, SZrString *value) {
    if (cs == ZR_NULL || value == ZR_NULL || cs->hasError) {
        return (TZrUInt32)-1;
    }

    SZrTypeValue constantValue;
    ZrCore_Value_InitAsRawObject(cs->state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(value));
    constantValue.type = ZR_VALUE_TYPE_STRING;

    TZrUInt32 constantIndex = add_constant(cs, &constantValue);
    TZrUInt32 slot = allocate_stack_slot(cs);
    TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), slot, (TZrInt32)constantIndex);
    emit_instruction(cs, inst);
    return slot;
}

static TZrUInt32 compile_expression_into_slot(SZrCompilerState *cs, SZrAstNode *node, TZrUInt32 targetSlot) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return (TZrUInt32)-1;
    }

    compile_expression_non_tail(cs, node);
    if (cs->hasError) {
        return (TZrUInt32)-1;
    }

    return normalize_top_result_to_slot(cs, targetSlot);
}

static TZrBool emit_property_getter_call(SZrCompilerState *cs, TZrUInt32 currentSlot, SZrString *propertyName,
                                       TZrBool isStatic, SZrFileRange location) {
    if (cs == ZR_NULL || propertyName == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    SZrString *accessorName = create_hidden_property_accessor_name(cs, propertyName, ZR_FALSE);
    if (accessorName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create property getter accessor name", location);
        return ZR_FALSE;
    }

    if (!isStatic) {
        TZrUInt32 receiverSlot = allocate_stack_slot(cs);
        TZrInstruction copyReceiverInst =
                create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)receiverSlot, (TZrInt32)currentSlot);
        emit_instruction(cs, copyReceiverInst);
    }

    TZrUInt32 keyTargetSlot = isStatic ? currentSlot + 1 : currentSlot + 2;
    TZrUInt32 keySlot = emit_string_constant(cs, accessorName);
    if (keySlot == (TZrUInt32)-1) {
        return ZR_FALSE;
    }
    keySlot = normalize_top_result_to_slot(cs, keyTargetSlot);

    TZrInstruction getAccessorInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TZrUInt16)currentSlot,
                                                          (TZrUInt16)currentSlot, (TZrUInt16)keySlot);
    emit_instruction(cs, getAccessorInst);

    TZrInstruction callAccessorInst =
            create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), (TZrUInt16)currentSlot, (TZrUInt16)currentSlot,
                                 (TZrUInt16)(isStatic ? 0 : 1));
    emit_instruction(cs, callAccessorInst);
    collapse_stack_to_slot(cs, currentSlot);
    return ZR_TRUE;
}

static TZrUInt32 emit_property_setter_call(SZrCompilerState *cs, TZrUInt32 objectSlot, SZrString *propertyName, TZrBool isStatic,
                                         TZrUInt32 assignedValueSlot, SZrFileRange location) {
    if (cs == ZR_NULL || propertyName == ZR_NULL || cs->hasError) {
        return (TZrUInt32)-1;
    }

    if (!isStatic) {
        TZrUInt32 receiverSlot = allocate_stack_slot(cs);
        TZrInstruction copyReceiverInst =
                create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)receiverSlot, (TZrInt32)objectSlot);
        emit_instruction(cs, copyReceiverInst);
    }

    TZrUInt32 valueArgSlot = allocate_stack_slot(cs);
    TZrInstruction copyValueInst =
            create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)valueArgSlot, (TZrInt32)assignedValueSlot);
    emit_instruction(cs, copyValueInst);

    SZrString *accessorName = create_hidden_property_accessor_name(cs, propertyName, ZR_TRUE);
    if (accessorName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create property setter accessor name", location);
        return (TZrUInt32)-1;
    }

    TZrUInt32 keyTargetSlot = isStatic ? objectSlot + 2 : objectSlot + 3;
    TZrUInt32 keySlot = emit_string_constant(cs, accessorName);
    if (keySlot == (TZrUInt32)-1) {
        return (TZrUInt32)-1;
    }
    keySlot = normalize_top_result_to_slot(cs, keyTargetSlot);

    TZrInstruction getAccessorInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TZrUInt16)objectSlot,
                                                          (TZrUInt16)objectSlot, (TZrUInt16)keySlot);
    emit_instruction(cs, getAccessorInst);

    TZrInstruction callAccessorInst =
            create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), (TZrUInt16)objectSlot, (TZrUInt16)objectSlot,
                                 (TZrUInt16)(isStatic ? 1 : 2));
    emit_instruction(cs, callAccessorInst);

    TZrInstruction preserveAssignedValue =
            create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)objectSlot, (TZrInt32)assignedValueSlot);
    emit_instruction(cs, preserveAssignedValue);
    collapse_stack_to_slot(cs, objectSlot);
    return objectSlot;
}

static TZrUInt32 compile_member_key_into_slot(SZrCompilerState *cs, SZrMemberExpression *memberExpr, TZrUInt32 targetSlot) {
    if (cs == ZR_NULL || memberExpr == ZR_NULL || memberExpr->property == ZR_NULL || cs->hasError) {
        return (TZrUInt32)-1;
    }

    if (!memberExpr->computed && memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *fieldName = memberExpr->property->data.identifier.name;
        if (fieldName == ZR_NULL) {
            return (TZrUInt32)-1;
        }

        if (emit_string_constant(cs, fieldName) == (TZrUInt32)-1) {
            return (TZrUInt32)-1;
        }
        return normalize_top_result_to_slot(cs, targetSlot);
    }

    return compile_expression_into_slot(cs, memberExpr->property, targetSlot);
}

static TZrBool expression_uses_dynamic_object_access(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primary = &node->data.primaryExpression;
            if (primary->members != ZR_NULL && primary->members->count > 0) {
                return ZR_TRUE;
            }
            return expression_uses_dynamic_object_access(primary->property);
        }
        case ZR_AST_MEMBER_EXPRESSION:
        case ZR_AST_FUNCTION_CALL:
            return ZR_TRUE;
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return expression_uses_dynamic_object_access(node->data.prototypeReferenceExpression.target);
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return ZR_TRUE;
        case ZR_AST_BINARY_EXPRESSION:
            return expression_uses_dynamic_object_access(node->data.binaryExpression.left) ||
                   expression_uses_dynamic_object_access(node->data.binaryExpression.right);
        case ZR_AST_UNARY_EXPRESSION:
            return expression_uses_dynamic_object_access(node->data.unaryExpression.argument);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return expression_uses_dynamic_object_access(node->data.conditionalExpression.test) ||
                   expression_uses_dynamic_object_access(node->data.conditionalExpression.consequent) ||
                   expression_uses_dynamic_object_access(node->data.conditionalExpression.alternate);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return expression_uses_dynamic_object_access(node->data.assignmentExpression.left) ||
                   expression_uses_dynamic_object_access(node->data.assignmentExpression.right);
        default:
            return ZR_FALSE;
    }
}

static void record_template_segment_semantics(SZrCompilerState *cs, SZrAstNode *segmentNode) {
    SZrTemplateSegment segment;

    if (cs == ZR_NULL || segmentNode == ZR_NULL || cs->semanticContext == ZR_NULL) {
        return;
    }

    memset(&segment, 0, sizeof(segment));
    if (segmentNode->type == ZR_AST_STRING_LITERAL) {
        segment.isInterpolation = ZR_FALSE;
        segment.staticText = segmentNode->data.stringLiteral.value;
        if (segment.staticText == ZR_NULL) {
            segment.staticText = ZrCore_String_Create(cs->state, "", 0);
        }
    } else if (segmentNode->type == ZR_AST_INTERPOLATED_SEGMENT) {
        segment.isInterpolation = ZR_TRUE;
        segment.expression = segmentNode->data.interpolatedSegment.expression;
    } else {
        return;
    }

    ZrParser_Semantic_AppendTemplateSegment(cs->semanticContext, &segment);
}

static TZrUInt32 compile_template_segment_into_string_slot(SZrCompilerState *cs,
                                                         SZrAstNode *segmentNode,
                                                         TZrUInt32 targetSlot) {
    SZrInferredType inferredType;
    TZrBool hasInferredType = ZR_FALSE;

    if (cs == ZR_NULL || segmentNode == ZR_NULL || cs->hasError) {
        return (TZrUInt32)-1;
    }

    if (segmentNode->type == ZR_AST_STRING_LITERAL) {
        SZrString *value = segmentNode->data.stringLiteral.value;
        if (value == ZR_NULL) {
            value = ZrCore_String_Create(cs->state, "", 0);
            if (value == ZR_NULL) {
                ZrParser_Compiler_Error(cs,
                                "Failed to allocate empty template string segment",
                                segmentNode->location);
                return (TZrUInt32)-1;
            }
        }

        if (emit_string_constant(cs, value) == (TZrUInt32)-1) {
            return (TZrUInt32)-1;
        }
        return normalize_top_result_to_slot(cs, targetSlot);
    }

    if (segmentNode->type != ZR_AST_INTERPOLATED_SEGMENT) {
        ZrParser_Compiler_Error(cs, "Unexpected template string segment", segmentNode->location);
        return (TZrUInt32)-1;
    }

    if (segmentNode->data.interpolatedSegment.expression == ZR_NULL) {
        SZrString *emptyString = ZrCore_String_Create(cs->state, "", 0);
        if (emptyString == ZR_NULL || emit_string_constant(cs, emptyString) == (TZrUInt32)-1) {
            ZrParser_Compiler_Error(cs, "Failed to allocate empty template string segment", segmentNode->location);
            return (TZrUInt32)-1;
        }
        return normalize_top_result_to_slot(cs, targetSlot);
    }

    if (compile_expression_into_slot(cs,
                                     segmentNode->data.interpolatedSegment.expression,
                                     targetSlot) == (TZrUInt32)-1) {
        return (TZrUInt32)-1;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    hasInferredType = ZrParser_ExpressionType_Infer(cs,
                                            segmentNode->data.interpolatedSegment.expression,
                                            &inferredType);
    if (!hasInferredType || inferredType.baseType != ZR_VALUE_TYPE_STRING) {
        emit_type_conversion(cs, targetSlot, targetSlot, ZR_INSTRUCTION_ENUM(TO_STRING));
    }
    if (hasInferredType) {
        ZrParser_InferredType_Free(cs->state, &inferredType);
    }

    collapse_stack_to_slot(cs, targetSlot);
    return targetSlot;
}

static void compile_template_string_literal(SZrCompilerState *cs, SZrAstNode *node) {
    SZrAstNodeArray *segments;
    TZrUInt32 resultSlot;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_TEMPLATE_STRING_LITERAL) {
        ZrParser_Compiler_Error(cs, "Expected template string literal", node->location);
        return;
    }

    segments = node->data.templateStringLiteral.segments;
    if (segments == ZR_NULL || segments->count == 0) {
        SZrString *emptyString = ZrCore_String_Create(cs->state, "", 0);
        if (emptyString == ZR_NULL || emit_string_constant(cs, emptyString) == (TZrUInt32)-1) {
            ZrParser_Compiler_Error(cs, "Failed to allocate empty template string", node->location);
        }
        return;
    }

    for (TZrSize i = 0; i < segments->count; i++) {
        record_template_segment_semantics(cs, segments->nodes[i]);
    }

    resultSlot = allocate_stack_slot(cs);
    if (compile_template_segment_into_string_slot(cs, segments->nodes[0], resultSlot) == (TZrUInt32)-1) {
        return;
    }

    for (TZrSize i = 1; i < segments->count; i++) {
        TZrUInt32 nextSlot = allocate_stack_slot(cs);
        TZrInstruction addInst;

        if (compile_template_segment_into_string_slot(cs, segments->nodes[i], nextSlot) == (TZrUInt32)-1) {
            return;
        }

        addInst = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD_STRING),
                                       (TZrUInt16)resultSlot,
                                       (TZrUInt16)resultSlot,
                                       (TZrUInt16)nextSlot);
        emit_instruction(cs, addInst);
        collapse_stack_to_slot(cs, resultSlot);
    }
}

// 编译字面量
static void compile_literal(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    SZrTypeValue constantValue;
    ZrCore_Value_ResetAsNull(&constantValue);
    TZrUInt32 constantIndex = 0;
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    
    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL: {
            TZrBool value = node->data.booleanLiteral.value;
            ZrCore_Value_InitAsInt(cs->state, &constantValue, value ? 1 : 0);
            constantValue.type = ZR_VALUE_TYPE_BOOL;
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_INTEGER_LITERAL: {
            TZrInt64 value = node->data.integerLiteral.value;
            ZrCore_Value_InitAsInt(cs->state, &constantValue, value);
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_FLOAT_LITERAL: {
            TZrDouble value = node->data.floatLiteral.value;
            ZrCore_Value_InitAsFloat(cs->state, &constantValue, value);
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_STRING_LITERAL: {
            SZrString *value = node->data.stringLiteral.value;
            if (value != ZR_NULL) {
                ZrCore_Value_InitAsRawObject(cs->state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(value));
                constantValue.type = ZR_VALUE_TYPE_STRING;
                constantIndex = add_constant(cs, &constantValue);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TZrInt32)constantIndex);
                emit_instruction(cs, inst);
            }
            break;
        }
        
        case ZR_AST_CHAR_LITERAL: {
            TZrChar value = node->data.charLiteral.value;
            ZrCore_Value_InitAsInt(cs->state, &constantValue, (TZrInt64)value);
            constantValue.type = ZR_VALUE_TYPE_INT8;
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_NULL_LITERAL: {
            ZrCore_Value_ResetAsNull(&constantValue);
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        default:
            ZrParser_Compiler_Error(cs, "Unexpected literal type", node->location);
            break;
    }
}

// 编译标识符
static void compile_identifier(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_IDENTIFIER_LITERAL) {
        ZrParser_Compiler_Error(cs, "Expected identifier", node->location);
        return;
    }
    
    SZrString *name = node->data.identifier.name;
    if (name == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Identifier name is null", node->location);
        return;
    }
    
    ZR_UNUSED_PARAMETER(ZrCore_String_GetNativeString(name));
    
    // 重要：先查找局部变量（包括闭包变量）
    // 如果存在同名的局部变量，它会覆盖全局的 zr 对象
    // 这是作用域规则：局部变量优先于全局对象
    TZrUInt32 localVarIndex = find_local_var(cs, name);
    if (localVarIndex != (TZrUInt32)-1) {
        // 找到局部变量：使用 GET_STACK
        // 即使这个变量名是 "zr"，也使用局部变量而不是全局 zr 对象
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), destSlot, (TZrInt32)localVarIndex);
        emit_instruction(cs, inst);
        return;
    }
    
    // 查找闭包变量（在局部变量之后，但在全局对象之前）
    TZrUInt32 closureVarIndex = find_closure_var(cs, name);
    if (closureVarIndex != (TZrUInt32)-1) {
        // 闭包变量：根据 inStack 标志选择使用 GET_CLOSURE 还是 GETUPVAL
        // 即使这个变量名是 "zr"，也使用闭包变量而不是全局 zr 对象
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        
        // 获取闭包变量信息以检查 inStack 标志
        SZrFunctionClosureVariable *closureVar = (SZrFunctionClosureVariable *)ZrCore_Array_Get(&cs->closureVars, closureVarIndex);
        if (closureVar != ZR_NULL && closureVar->inStack) {
            // 变量在栈上，使用 GETUPVAL
            // GETUPVAL 格式: operandExtra = destSlot, operand1[0] = closureVarIndex, operand1[1] = 0
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETUPVAL), (TZrUInt16)destSlot, (TZrUInt16)closureVarIndex, 0);
            emit_instruction(cs, inst);
        } else {
            // 变量不在栈上，使用 GET_CLOSURE
            // GET_CLOSURE 格式: operandExtra = destSlot, operand1[0] = closureVarIndex, operand1[1] = 0
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_CLOSURE), (TZrUInt16)destSlot, (TZrUInt16)closureVarIndex, 0);
            emit_instruction(cs, inst);
        }
        return;
    }

    {
        SZrTypeValue compileTimeValue;
        if (ZrParser_Compiler_TryGetCompileTimeValue(cs, name, &compileTimeValue)) {
            TZrUInt32 destSlot = allocate_stack_slot(cs);
            emit_constant_to_slot_local(cs, destSlot, &compileTimeValue, node->location);
            return;
        }
    }
    
    // 只有在没有找到局部变量和闭包变量的情况下，才检查是否是全局关键字 "zr"
    // 这样可以确保局部变量能够覆盖全局的 zr 对象
    TZrNativeString varNameStr;
    TZrSize nameLen;
    if (name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        varNameStr = ZrCore_String_GetNativeStringShort(name);
        nameLen = name->shortStringLength;
    } else {
        varNameStr = ZrCore_String_GetNativeString(name);
        nameLen = name->longStringLength;
    }
    
    // 检查是否是 "zr" 全局对象
    // 注意：只有在没有局部变量覆盖的情况下才会到达这里
    if (nameLen == 2 && varNameStr[0] == 'z' && varNameStr[1] == 'r') {
        // 使用 GET_GLOBAL 指令获取全局 zr 对象
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        // GET_GLOBAL 格式: operandExtra = destSlot, operand1[0] = 0, operand1[1] = 0
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_GLOBAL), (TZrUInt16)destSlot, 0);
        emit_instruction(cs, inst);
        return;
    }
    
    // 尝试作为父函数的子函数访问（使用 GET_SUB_FUNCTION）
    // 在编译时查找子函数索引，而不是使用名称常量
    // GET_SUB_FUNCTION 通过索引直接访问 childFunctionList，这是编译时确定的静态索引
    TZrUInt32 childFunctionIndex = find_child_function_index(cs, name);
    if (childFunctionIndex != (TZrUInt32)-1) {
        // 找到子函数索引，生成 GET_SUB_FUNCTION 指令
        // GET_SUB_FUNCTION 格式: operandExtra = destSlot, operand1[0] = childFunctionIndex, operand1[1] = 0
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction getSubFuncInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), (TZrUInt16)destSlot, (TZrUInt16)childFunctionIndex, 0);
        emit_instruction(cs, getSubFuncInst);
        return;
    }
    
    // 如果不是子函数，尝试作为全局对象（zr）的属性访问（使用 GET_GLOBAL + GETTABLE）
    // 1. 使用 GET_GLOBAL 获取全局 zr 对象
    TZrUInt32 globalObjSlot = allocate_stack_slot(cs);
    TZrInstruction getGlobalInst = create_instruction_0(ZR_INSTRUCTION_ENUM(GET_GLOBAL), (TZrUInt16)globalObjSlot);
    emit_instruction(cs, getGlobalInst);
    
    // 2. 将属性名作为字符串常量压栈
    TZrUInt32 nameSlot = emit_string_constant(cs, name);
    if (nameSlot == (TZrUInt32)-1) {
        return;
    }
    
    // 3. 使用 GETTABLE 访问属性
    TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TZrUInt16)globalObjSlot, (TZrUInt16)globalObjSlot, (TZrUInt16)nameSlot);
    emit_instruction(cs, getTableInst);
    collapse_stack_to_slot(cs, globalObjSlot);
}

// 编译一元表达式
static void compile_unary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_UNARY_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected unary expression", node->location);
        return;
    }
    
    const TZrChar *op = node->data.unaryExpression.op.op;
    SZrAstNode *arg = node->data.unaryExpression.argument;
    
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    
    if (strcmp(op, "$") == 0 || strcmp(op, "new") == 0) {
        ZrParser_Compiler_Error(cs,
                                "Legacy unary constructor syntax is no longer supported; use $target(...) or new target(...)",
                                node->location);
        return;
    } else {
        // 其他一元操作符：先编译操作数
        compile_expression_non_tail(cs, arg);
        TZrUInt32 argSlot = cs->stackSlotCount - 1;
        
        if (strcmp(op, "!") == 0) {
            // 逻辑非
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_NOT), destSlot, (TZrUInt16)argSlot, 0);
            emit_instruction(cs, inst);
        } else if (strcmp(op, "~") == 0) {
            // 位非
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(BITWISE_NOT), destSlot, (TZrUInt16)argSlot, 0);
            emit_instruction(cs, inst);
        } else if (strcmp(op, "-") == 0) {
            // 取负
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(NEG), destSlot, (TZrUInt16)argSlot, 0);
            emit_instruction(cs, inst);
        } else if (strcmp(op, "+") == 0) {
            // 正号：直接使用操作数（不需要额外指令）
            // 将结果复制到目标槽位
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), destSlot, (TZrInt32)argSlot);
            emit_instruction(cs, inst);
        } else {
            ZrParser_Compiler_Error(cs, "Unknown unary operator", node->location);
        }

        if (!cs->hasError) {
            collapse_stack_to_slot(cs, destSlot);
        }
    }
}

// 编译类型转换表达式
static void compile_type_cast_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_TYPE_CAST_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected type cast expression", node->location);
        return;
    }
    
    SZrType *targetType = node->data.typeCastExpression.targetType;
    SZrAstNode *expression = node->data.typeCastExpression.expression;
    
    if (targetType == ZR_NULL || expression == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Invalid type cast expression", node->location);
        return;
    }
    
    // 先编译要转换的表达式
    compile_expression_non_tail(cs, expression);
    
    TZrUInt32 srcSlot = cs->stackSlotCount - 1;
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    
    // 根据目标类型生成相应的转换指令
    // 首先检查类型名称
    if (targetType->name != ZR_NULL && targetType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *typeName = targetType->name->data.identifier.name;
        TZrNativeString typeNameStr;
        TZrSize nameLen;
        
        if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            typeNameStr = ZrCore_String_GetNativeStringShort(typeName);
            nameLen = typeName->shortStringLength;
        } else {
            typeNameStr = ZrCore_String_GetNativeString(typeName);
            nameLen = typeName->longStringLength;
        }
        
        // 检查基本类型
        if (nameLen == 3 && strncmp(typeNameStr, "int", 3) == 0) {
            // 转换为 int
            emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_INT));
            return;
        } else if (nameLen == 5 && strncmp(typeNameStr, "float", 5) == 0) {
            // 转换为 float
            emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_FLOAT));
            return;
        } else if (nameLen == 6 && strncmp(typeNameStr, "string", 6) == 0) {
            // 转换为 string
            emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_STRING));
            return;
        } else if (nameLen == 4 && strncmp(typeNameStr, "bool", 4) == 0) {
            // 转换为 bool
            emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_BOOL));
            return;
        }
        
        // 对于 struct 和 class 类型，查找类型定义
        SZrAstNode *typeDecl = find_type_declaration(cs, typeName);
        if (typeDecl != ZR_NULL) {
            // 将类型名称作为常量存储（运行时通过类型名称查找或创建原型）
            SZrTypeValue typeNameValue;
            ZrCore_Value_ResetAsNull(&typeNameValue);
            ZrCore_Value_InitAsRawObject(cs->state, &typeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
            typeNameValue.type = ZR_VALUE_TYPE_STRING;
            TZrUInt32 typeNameConstantIndex = add_constant(cs, &typeNameValue);
            
            // 根据类型声明类型生成相应的转换指令
            if (typeDecl->type == ZR_AST_STRUCT_DECLARATION) {
                // 生成 TO_STRUCT 指令，将类型名称常量索引作为操作数
                emit_type_conversion_with_prototype(cs, destSlot, srcSlot, 
                                                    ZR_INSTRUCTION_ENUM(TO_STRUCT), 
                                                    typeNameConstantIndex);
                return;
            } else if (typeDecl->type == ZR_AST_CLASS_DECLARATION) {
                // 生成 TO_OBJECT 指令，将类型名称常量索引作为操作数
                emit_type_conversion_with_prototype(cs, destSlot, srcSlot, 
                                                    ZR_INSTRUCTION_ENUM(TO_OBJECT), 
                                                    typeNameConstantIndex);
                return;
            }
        }
        
        // 如果未找到类型定义，使用 TO_STRING 作为默认转换
        // 可能需要支持从其他模块导入的类型
        // 注意：从其他模块导入的类型在运行时通过模块系统查找
        // 编译器无法在编译期确定所有类型，因此使用运行时类型查找
        // 如果类型未在当前模块找到，运行时会在全局模块注册表中查找
        emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_STRING));
    } else {
        // TODO: 对于复杂类型（泛型、元组等），暂时使用 TO_STRING
        // 实现完整的类型转换逻辑
        // 注意：复杂类型的转换需要根据具体类型实现
        // 泛型类型转换需要实例化类型参数
        // 元组类型转换需要逐个元素转换
        // TODO: 这里暂时使用TO_STRING作为默认转换，未来可以扩展支持更多类型
        emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_STRING));
    }
}

// 编译二元表达式
static void compile_binary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_BINARY_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected binary expression", node->location);
        return;
    }
    
    const TZrChar *op = node->data.binaryExpression.op.op;
    SZrAstNode *left = node->data.binaryExpression.left;
    SZrAstNode *right = node->data.binaryExpression.right;
    // 调试：检查操作符字符串
    if (op == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Binary operator string is NULL", node->location);
        return;
    }
    
    // 临时调试：输出操作符字符串的实际值
    // 注意：这里使用 fprintf 来调试，确认 op 的实际值
    // fprintf(stderr, "DEBUG: Binary operator op='%s' (first char=%d, len=%zu)\n", 
    //         op, (int)(unsigned char)op[0], strlen(op));
    
    // 推断左右操作数的类型
    SZrInferredType leftType, rightType, resultType;
    EZrValueType effectiveLeftType = ZR_VALUE_TYPE_OBJECT;
    EZrValueType effectiveRightType = ZR_VALUE_TYPE_OBJECT;
    ZrParser_InferredType_Init(cs->state, &leftType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(cs->state, &rightType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(cs->state, &resultType, ZR_VALUE_TYPE_OBJECT);
    TZrBool hasTypeInfo = ZR_FALSE;
    if (cs->typeEnv != ZR_NULL &&
        !expression_uses_dynamic_object_access(left) &&
        !expression_uses_dynamic_object_access(right)) {
        if (ZrParser_ExpressionType_Infer(cs, left, &leftType) && ZrParser_ExpressionType_Infer(cs, right, &rightType)) {
            hasTypeInfo = ZR_TRUE;
            effectiveLeftType = leftType.baseType;
            effectiveRightType = rightType.baseType;
            // 推断结果类型
            if (!ZrParser_BinaryExpressionType_Infer(cs, node, &resultType)) {
                // 类型推断失败，使用默认类型
                ZrParser_InferredType_Init(cs->state, &resultType, ZR_VALUE_TYPE_OBJECT);
            }
        } else {
            // 类型推断失败，清理已推断的类型
            if (ZrParser_ExpressionType_Infer(cs, left, &leftType)) {
                ZrParser_InferredType_Free(cs->state, &leftType);
            }
            if (ZrParser_ExpressionType_Infer(cs, right, &rightType)) {
                ZrParser_InferredType_Free(cs->state, &rightType);
            }
        }
    }
    
    // 编译左操作数
    compile_expression_non_tail(cs, left);
    TZrUInt32 leftSlot = cs->stackSlotCount - 1;
    
    // 编译右操作数
    compile_expression_non_tail(cs, right);
    TZrUInt32 rightSlot = cs->stackSlotCount - 1;
    
    // 如果需要类型转换，插入转换指令
    // 注意：对于比较操作，需要保留 leftType 和 rightType 用于选择正确的比较指令
    TZrBool isComparisonOp = (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || 
                            strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0 ||
                            strcmp(op, "==") == 0 || strcmp(op, "!=") == 0);
    
    if (hasTypeInfo) {
        if (isComparisonOp) {
            // 对于比较操作，不应该根据 resultType（布尔类型）来转换操作数
            // 比较操作的结果才是布尔类型，但操作数应该保持其原始数值类型
            // 实现完整的类型提升逻辑（例如：int 和 float 比较时，将 int 提升为 float）
            // 类型提升规则：
            // 1. 如果一个是float，另一个是int，将int提升为float
            // 2. 如果一个是更大的整数类型，将较小的整数类型提升为较大的类型
            // 3. 其他情况保持原类型
            EZrValueType leftValueType = leftType.baseType;
            EZrValueType rightValueType = rightType.baseType;
            
            // 检查是否需要类型提升
            if (ZR_VALUE_IS_TYPE_FLOAT(leftValueType) && ZR_VALUE_IS_TYPE_SIGNED_INT(rightValueType)) {
                // 左操作数是float，右操作数是int，将右操作数提升为float
                TZrUInt32 promotedRightSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, promotedRightSlot, rightSlot, ZR_INSTRUCTION_ENUM(TO_FLOAT));
                rightSlot = promotedRightSlot;
                effectiveRightType = ZR_VALUE_TYPE_DOUBLE;
            } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValueType) && ZR_VALUE_IS_TYPE_FLOAT(rightValueType)) {
                // 左操作数是int，右操作数是float，将左操作数提升为float
                TZrUInt32 promotedLeftSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, promotedLeftSlot, leftSlot, ZR_INSTRUCTION_ENUM(TO_FLOAT));
                leftSlot = promotedLeftSlot;
                effectiveLeftType = ZR_VALUE_TYPE_DOUBLE;
            } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValueType) && ZR_VALUE_IS_TYPE_SIGNED_INT(rightValueType)) {
                // 两个都是整数类型，提升到较大的类型
                // 类型提升顺序：INT8 < INT16 < INT32 < INT64
                // 注意：目前只有 TO_INT 指令（提升到 INT32），对于 INT16 和 INT64 的提升需要特殊处理
                if (leftValueType < rightValueType) {
                    // 左操作数类型较小，提升左操作数
                    // 如果目标是 INT32 或更大，使用 TO_INT 提升到 INT32
                    if (rightValueType == ZR_VALUE_TYPE_INT32 || rightValueType == ZR_VALUE_TYPE_INT64) {
                        if (leftValueType < ZR_VALUE_TYPE_INT32) {
                            TZrUInt32 promotedLeftSlot = allocate_stack_slot(cs);
                            emit_type_conversion(cs, promotedLeftSlot, leftSlot, ZR_INSTRUCTION_ENUM(TO_INT));
                            leftSlot = promotedLeftSlot;
                            effectiveLeftType = ZR_VALUE_TYPE_INT64;
                        }
                    }
                    // TODO: 对于 INT16 的提升，需要添加 TO_INT16 指令支持
                } else if (rightValueType < leftValueType) {
                    // 右操作数类型较小，提升右操作数
                    // 如果目标是 INT32 或更大，使用 TO_INT 提升到 INT32
                    if (leftValueType == ZR_VALUE_TYPE_INT32 || leftValueType == ZR_VALUE_TYPE_INT64) {
                        if (rightValueType < ZR_VALUE_TYPE_INT32) {
                            TZrUInt32 promotedRightSlot = allocate_stack_slot(cs);
                            emit_type_conversion(cs, promotedRightSlot, rightSlot, ZR_INSTRUCTION_ENUM(TO_INT));
                            rightSlot = promotedRightSlot;
                            effectiveRightType = ZR_VALUE_TYPE_INT64;
                        }
                    }
                    // TODO: 对于 INT16 的提升，需要添加 TO_INT16 指令支持
                }
            }
            // 其他情况（如都是float，或类型不兼容）保持原类型
        } else {
            // 对于非比较操作，根据结果类型进行转换
            // 检查左操作数是否需要转换
            EZrInstructionCode leftConvOp = ZrParser_InferredType_GetConversionOpcode(&leftType, &resultType);
            if (leftConvOp != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                TZrUInt32 convertedLeftSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, convertedLeftSlot, leftSlot, leftConvOp);
                leftSlot = convertedLeftSlot;
                effectiveLeftType = binary_expression_effective_type_after_conversion(leftType.baseType, leftConvOp);
            }
            
            // 检查右操作数是否需要转换
            EZrInstructionCode rightConvOp = ZrParser_InferredType_GetConversionOpcode(&rightType, &resultType);
            if (rightConvOp != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                TZrUInt32 convertedRightSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, convertedRightSlot, rightSlot, rightConvOp);
                rightSlot = convertedRightSlot;
                effectiveRightType = binary_expression_effective_type_after_conversion(rightType.baseType, rightConvOp);
            }
            
            // 对于非比较操作，可以立即清理类型信息
            ZrParser_InferredType_Free(cs->state, &leftType);
            ZrParser_InferredType_Free(cs->state, &rightType);
        }
        // resultType 在比较操作中需要使用，所以稍后清理
    }
    
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD);  // 默认（通用加法指令）
    
    // 根据操作符和类型选择指令
    // 临时调试输出：检查操作符字符串的实际值
    // fprintf(stderr, "DEBUG: Binary operator op='%s' (first char=%d, len=%zu), hasTypeInfo=%d\n", 
    //         op, (int)(unsigned char)op[0], strlen(op), hasTypeInfo);
    
    if (strcmp(op, "+") == 0) {
        // 根据结果类型选择 ADD/ADD_INT/ADD_FLOAT/ADD_STRING
        if (!hasTypeInfo) {
            // 类型不明确，使用通用 ADD 指令
            opcode = ZR_INSTRUCTION_ENUM(ADD);
        } else if (resultType.baseType == ZR_VALUE_TYPE_STRING ||
                   effectiveLeftType == ZR_VALUE_TYPE_STRING ||
                   effectiveRightType == ZR_VALUE_TYPE_STRING) {
            opcode = ZR_INSTRUCTION_ENUM(ADD_STRING);
        } else if (binary_expression_type_is_float_like(resultType.baseType) ||
                   binary_expression_type_is_float_like(effectiveLeftType) ||
                   binary_expression_type_is_float_like(effectiveRightType)) {
            opcode = ZR_INSTRUCTION_ENUM(ADD_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
        }
    } else if (strcmp(op, "-") == 0) {
        if (!hasTypeInfo) {
            // 类型不明确，使用通用 SUB 指令
            opcode = ZR_INSTRUCTION_ENUM(SUB);
        } else if (binary_expression_type_is_float_like(resultType.baseType) ||
                   binary_expression_type_is_float_like(effectiveLeftType) ||
                   binary_expression_type_is_float_like(effectiveRightType)) {
            opcode = ZR_INSTRUCTION_ENUM(SUB_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
        }
    } else if (strcmp(op, "*") == 0) {
        if (hasTypeInfo &&
            (binary_expression_type_is_float_like(resultType.baseType) ||
             binary_expression_type_is_float_like(effectiveLeftType) ||
             binary_expression_type_is_float_like(effectiveRightType))) {
            opcode = ZR_INSTRUCTION_ENUM(MUL_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
        }
    } else if (strcmp(op, "/") == 0) {
        if (hasTypeInfo &&
            (binary_expression_type_is_float_like(resultType.baseType) ||
             binary_expression_type_is_float_like(effectiveLeftType) ||
             binary_expression_type_is_float_like(effectiveRightType))) {
            opcode = ZR_INSTRUCTION_ENUM(DIV_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
        }
    } else if (strcmp(op, "%") == 0) {
        if (hasTypeInfo &&
            (binary_expression_type_is_float_like(resultType.baseType) ||
             binary_expression_type_is_float_like(effectiveLeftType) ||
             binary_expression_type_is_float_like(effectiveRightType))) {
            opcode = ZR_INSTRUCTION_ENUM(MOD_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
        }
    } else if (strcmp(op, "**") == 0) {
        if (hasTypeInfo &&
            (binary_expression_type_is_float_like(resultType.baseType) ||
             binary_expression_type_is_float_like(effectiveLeftType) ||
             binary_expression_type_is_float_like(effectiveRightType))) {
            opcode = ZR_INSTRUCTION_ENUM(POW_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(POW_SIGNED);
        }
    } else if (strcmp(op, "<<") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT);
    } else if (strcmp(op, ">>") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT);
    } else if (strcmp(op, "==") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL);
    } else if (strcmp(op, "!=") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL);
    } else if (strcmp(op, "<") == 0) {
        // 根据操作数类型选择比较指令
        // 对于比较操作，使用 leftType 或 rightType 来判断操作数类型（它们应该相同或兼容）
        if (hasTypeInfo) {
            // 优先检查是否有浮点数类型
            if (leftType.baseType == ZR_VALUE_TYPE_FLOAT || leftType.baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType.baseType == ZR_VALUE_TYPE_FLOAT || rightType.baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType.baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType.baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED);  // 默认使用有符号整数比较
        }
    } else if (strcmp(op, ">") == 0) {
        if (hasTypeInfo) {
            if (leftType.baseType == ZR_VALUE_TYPE_FLOAT || leftType.baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType.baseType == ZR_VALUE_TYPE_FLOAT || rightType.baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType.baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType.baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED);
        }
    } else if (strcmp(op, "<=") == 0) {
        if (hasTypeInfo) {
            if (leftType.baseType == ZR_VALUE_TYPE_FLOAT || leftType.baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType.baseType == ZR_VALUE_TYPE_FLOAT || rightType.baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType.baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType.baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED);
        }
    } else if (strcmp(op, ">=") == 0) {
        if (hasTypeInfo) {
            if (leftType.baseType == ZR_VALUE_TYPE_FLOAT || leftType.baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType.baseType == ZR_VALUE_TYPE_FLOAT || rightType.baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType.baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType.baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED);
        }
    } else if (strcmp(op, "&&") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_AND);
    } else if (strcmp(op, "||") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_OR);
    } else if (strcmp(op, "&") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(BITWISE_AND);
    } else if (strcmp(op, "|") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(BITWISE_OR);
    } else if (strcmp(op, "^") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(BITWISE_XOR);
    } else {
        ZrParser_Compiler_Error(cs, "Unknown binary operator", node->location);
        return;
    }
    
    TZrInstruction inst = create_instruction_2(opcode, destSlot, (TZrUInt16)leftSlot, (TZrUInt16)rightSlot);
    emit_instruction(cs, inst);
    
    // 清理类型信息
    if (hasTypeInfo) {
        if (isComparisonOp) {
            // 比较操作：清理 leftType, rightType 和 resultType
            ZrParser_InferredType_Free(cs->state, &leftType);
            ZrParser_InferredType_Free(cs->state, &rightType);
        }
        ZrParser_InferredType_Free(cs->state, &resultType);
    }
}

// 辅助函数：检查变量名是否在 const 数组中
static TZrBool is_const_variable(SZrCompilerState *cs, SZrString *name, SZrArray *constArray) {
    if (cs == ZR_NULL || name == ZR_NULL || constArray == ZR_NULL) {
        return ZR_FALSE;
    }
    
    for (TZrSize i = 0; i < constArray->length; i++) {
        SZrString **varNamePtr = (SZrString **)ZrCore_Array_Get(constArray, i);
        if (varNamePtr != ZR_NULL && *varNamePtr != ZR_NULL) {
            if (ZrCore_String_Equal(*varNamePtr, name)) {
                return ZR_TRUE;
            }
        }
    }
    
    return ZR_FALSE;
}

// 编译赋值表达式
static void compile_assignment_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected assignment expression", node->location);
        return;
    }
    
    const TZrChar *op = node->data.assignmentExpression.op.op;
    SZrAstNode *left = node->data.assignmentExpression.left;
    SZrAstNode *right = node->data.assignmentExpression.right;
    
    // 编译右值
    ZrParser_Expression_Compile(cs, right);
    TZrUInt32 rightSlot = cs->stackSlotCount - 1;
    
    // 处理左值（标识符）
    if (left->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *name = left->data.identifier.name;
        
        // 检查是否是 const 函数参数
        if (is_const_variable(cs, name, &cs->constParameters)) {
            TZrChar errorMsg[256];
            TZrNativeString nameStr = ZrCore_String_GetNativeStringShort(name);
            if (nameStr != ZR_NULL) {
                snprintf(errorMsg, sizeof(errorMsg), "Cannot assign to const parameter '%s'", nameStr);
            } else {
                snprintf(errorMsg, sizeof(errorMsg), "Cannot assign to const parameter");
            }
            ZrParser_Compiler_Error(cs, errorMsg, node->location);
            return;
        }
        
        // 检查是否是 const 局部变量
        if (is_const_variable(cs, name, &cs->constLocalVars)) {
            TZrChar errorMsg[256];
            TZrNativeString nameStr = ZrCore_String_GetNativeStringShort(name);
            if (nameStr != ZR_NULL) {
                snprintf(errorMsg, sizeof(errorMsg), "Cannot assign to const variable '%s' after declaration", nameStr);
            } else {
                snprintf(errorMsg, sizeof(errorMsg), "Cannot assign to const variable after declaration");
            }
            ZrParser_Compiler_Error(cs, errorMsg, node->location);
            return;
        }
        
        TZrUInt32 localVarIndex = find_local_var(cs, name);
        
        if (localVarIndex != (TZrUInt32)-1) {
            // 简单赋值
            if (strcmp(op, "=") == 0) {
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)localVarIndex, (TZrInt32)rightSlot);
                emit_instruction(cs, inst);
                update_identifier_assignment_type_environment(cs, name, right);
            } else {
                // 复合赋值：先读取左值，执行运算，再赋值
                TZrUInt32 leftSlot = allocate_stack_slot(cs);
                TZrInstruction getInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), leftSlot, (TZrInt32)localVarIndex);
                emit_instruction(cs, getInst);
                
                // 执行运算
                EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                if (strcmp(op, "+=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                } else if (strcmp(op, "-=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
                } else if (strcmp(op, "*=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                } else if (strcmp(op, "/=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                } else if (strcmp(op, "%=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                }
                
                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                TZrInstruction opInst = create_instruction_2(opcode, resultSlot, (TZrUInt16)leftSlot, (TZrUInt16)rightSlot);
                emit_instruction(cs, opInst);
                
                // 赋值
                TZrInstruction setInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)localVarIndex, (TZrInt32)resultSlot);
                emit_instruction(cs, setInst);
            }
        } else {
            // 查找闭包变量
            TZrUInt32 closureVarIndex = find_closure_var(cs, name);
            if (closureVarIndex != (TZrUInt32)-1) {
                // 获取闭包变量信息以检查 inStack 标志
                SZrFunctionClosureVariable *closureVar = (SZrFunctionClosureVariable *)ZrCore_Array_Get(&cs->closureVars, closureVarIndex);
                TZrBool useUpval = (closureVar != ZR_NULL && closureVar->inStack);
                
                // 闭包变量：根据 inStack 标志选择使用 SET_CLOSURE 还是 SETUPVAL
                // 简单赋值
                if (strcmp(op, "=") == 0) {
                    if (useUpval) {
                        // SETUPVAL 格式: operandExtra = sourceSlot, operand1[0] = closureVarIndex, operand1[1] = 0
                        TZrInstruction setUpvalInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETUPVAL), (TZrUInt16)rightSlot, (TZrUInt16)closureVarIndex, 0);
                        emit_instruction(cs, setUpvalInst);
                    } else {
                        // SET_CLOSURE 格式: operandExtra = sourceSlot, operand1[0] = closureVarIndex, operand1[1] = 0
                        TZrInstruction setClosureInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_CLOSURE), (TZrUInt16)rightSlot, (TZrUInt16)closureVarIndex, 0);
                        emit_instruction(cs, setClosureInst);
                    }
                    update_identifier_assignment_type_environment(cs, name, right);
                } else {
                    // 复合赋值：先读取，执行运算，再写入
                    TZrUInt32 leftSlot = allocate_stack_slot(cs);
                    if (useUpval) {
                        TZrInstruction getUpvalInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETUPVAL), (TZrUInt16)leftSlot, (TZrUInt16)closureVarIndex, 0);
                        emit_instruction(cs, getUpvalInst);
                    } else {
                        TZrInstruction getClosureInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_CLOSURE), (TZrUInt16)leftSlot, (TZrUInt16)closureVarIndex, 0);
                        emit_instruction(cs, getClosureInst);
                    }
                    
                    // 执行运算
                    EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                    if (strcmp(op, "+=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                    } else if (strcmp(op, "-=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
                    } else if (strcmp(op, "*=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                    } else if (strcmp(op, "/=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                    } else if (strcmp(op, "%=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                    }
                    
                    TZrUInt32 resultSlot = allocate_stack_slot(cs);
                    TZrInstruction opInst = create_instruction_2(opcode, resultSlot, (TZrUInt16)leftSlot, (TZrUInt16)rightSlot);
                    emit_instruction(cs, opInst);
                    
                    // 写入闭包变量
                    if (useUpval) {
                        TZrInstruction setUpvalInst2 = create_instruction_2(ZR_INSTRUCTION_ENUM(SETUPVAL), (TZrUInt16)resultSlot, (TZrUInt16)closureVarIndex, 0);
                        emit_instruction(cs, setUpvalInst2);
                    } else {
                        TZrInstruction setClosureInst2 = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_CLOSURE), (TZrUInt16)resultSlot, (TZrUInt16)closureVarIndex, 0);
                        emit_instruction(cs, setClosureInst2);
                    }
                    
                    // 释放临时栈槽
                    ZrParser_Compiler_TrimStackBy(cs, 2); // leftSlot 和 resultSlot
                }
                return;
            }
            
            // 尝试作为全局变量访问（使用 SET_TABLE）
            // 1. 获取全局对象（zr 对象）
            TZrUInt32 globalSlot = allocate_stack_slot(cs);
            TZrInstruction getGlobalInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_GLOBAL), (TZrUInt16)globalSlot, 0);
            emit_instruction(cs, getGlobalInst);
            
            // 2. 将变量名转换为字符串常量并压入栈
            SZrTypeValue nameValue;
            ZrCore_Value_InitAsRawObject(cs->state, &nameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
            nameValue.type = ZR_VALUE_TYPE_STRING;
            TZrUInt32 nameConstantIndex = add_constant(cs, &nameValue);
            TZrUInt32 keySlot = allocate_stack_slot(cs);
            TZrInstruction getKeyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)keySlot, (TZrInt32)nameConstantIndex);
            emit_instruction(cs, getKeyInst);
            
            // 对于复合赋值，需要先读取值，执行运算，再写入
            if (strcmp(op, "=") != 0) {
                // 读取全局变量值
                TZrUInt32 leftSlot = allocate_stack_slot(cs);
                TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TZrUInt16)leftSlot, (TZrUInt16)globalSlot, (TZrUInt16)keySlot);
                emit_instruction(cs, getTableInst);
                
                // 执行运算
                EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                if (strcmp(op, "+=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                } else if (strcmp(op, "-=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
                } else if (strcmp(op, "*=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                } else if (strcmp(op, "/=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                } else if (strcmp(op, "%=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                }
                
                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                TZrInstruction opInst = create_instruction_2(opcode, resultSlot, (TZrUInt16)leftSlot, (TZrUInt16)rightSlot);
                emit_instruction(cs, opInst);
                
                // 写入全局变量（使用 SET_TABLE）
                // SET_TABLE 格式: operandExtra = destSlot, operand1[0] = tableSlot, operand1[1] = keySlot
                TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TZrUInt16)resultSlot, (TZrUInt16)globalSlot, (TZrUInt16)keySlot);
                emit_instruction(cs, setTableInst);
                
                // 释放临时栈槽
                ZrParser_Compiler_TrimStackBy(cs, 3); // leftSlot, resultSlot, globalSlot, keySlot (但 resultSlot 会被保留)
            } else {
                // 简单赋值：直接使用 SET_TABLE
                // SET_TABLE 格式: operandExtra = destSlot, operand1[0] = tableSlot, operand1[1] = keySlot
                TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TZrUInt16)rightSlot, (TZrUInt16)globalSlot, (TZrUInt16)keySlot);
                emit_instruction(cs, setTableInst);
                update_identifier_assignment_type_environment(cs, name, right);
                
                // 释放临时栈槽
                ZrParser_Compiler_TrimStackBy(cs, 2); // globalSlot 和 keySlot
            }
        }
    } else {
        // 处理成员访问等复杂左值
        // 支持 obj.prop = value 和 arr[index] = value
        // 注意：成员访问在 primary expression 中处理，这里需要处理 primary expression 作为左值
        if (left->type == ZR_AST_PRIMARY_EXPRESSION) {
            SZrPrimaryExpression *primary = &left->data.primaryExpression;
            
            // 检查是否是成员访问（members 数组不为空且最后一个成员是 MemberExpression）
            if (primary->members != ZR_NULL && primary->members->count > 0) {
                SZrAstNode *lastMember = primary->members->nodes[primary->members->count - 1];
                if (lastMember != ZR_NULL && lastMember->type == ZR_AST_MEMBER_EXPRESSION) {
                    // 编译整个 primary expression 以获取对象和键
                    // 先编译基础属性（对象）
                    if (primary->property != ZR_NULL) {
                        ZrParser_Expression_Compile(cs, primary->property);
                        TZrUInt32 objSlot = cs->stackSlotCount - 1;
                        
                        // 处理成员访问链，获取最后一个成员访问的键
                        SZrMemberExpression *memberExpr = &lastMember->data.memberExpression;
                        if (memberExpr->property != ZR_NULL) {
                            // 检查成员字段是否是 const
                            // 如果字段是 const 且当前不在构造函数中，报告错误
                            SZrString *rootTypeName = ZR_NULL;
                            TZrBool rootIsTypeReference = ZR_FALSE;
                            resolve_expression_root_type(cs, primary->property, &rootTypeName, &rootIsTypeReference);

                            if (memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                                SZrString *fieldName = memberExpr->property->data.identifier.name;
                                if (fieldName != ZR_NULL && !cs->isInConstructor) {
                                    // 查找类型定义，检查字段是否是 const
                                    // 尝试从 primary->property 推断类型（如果是 this）
                                    if (primary->property != ZR_NULL && 
                                        primary->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                                        SZrString *objName = primary->property->data.identifier.name;
                                        if (objName != ZR_NULL) {
                                            TZrNativeString objNameStr = ZrCore_String_GetNativeStringShort(objName);
                                            if (objNameStr != ZR_NULL && strcmp(objNameStr, "this") == 0) {
                                                // 这是 this.field 的赋值
                                                // 使用当前类型名称查找字段是否是 const
                                                if (cs->currentTypeName != ZR_NULL) {
                                                    if (find_type_member_is_const(cs, cs->currentTypeName, fieldName)) {
                                                        TZrChar errorMsg[256];
                                                        TZrNativeString fieldNameStr = ZrCore_String_GetNativeStringShort(fieldName);
                                                        if (fieldNameStr != ZR_NULL) {
                                                            snprintf(errorMsg, sizeof(errorMsg), "Cannot assign to const field '%s' outside constructor", fieldNameStr);
                                                        } else {
                                                            snprintf(errorMsg, sizeof(errorMsg), "Cannot assign to const field outside constructor");
                                                        }
                                                        ZrParser_Compiler_Error(cs, errorMsg, node->location);
                                                        return;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                SZrTypeMemberInfo *setterAccessor =
                                        find_hidden_property_accessor_member(cs, rootTypeName, fieldName, ZR_TRUE);
                                if (can_use_property_accessor(rootIsTypeReference, setterAccessor)) {
                                    if (strcmp(op, "=") != 0) {
                                        ZrParser_Compiler_Error(cs,
                                                        "Compound assignment to property accessors is not supported yet",
                                                        node->location);
                                        return;
                                    }

                                    if (emit_property_setter_call(cs, objSlot, fieldName, setterAccessor->isStatic,
                                                                  rightSlot, node->location) == (TZrUInt32)-1) {
                                        return;
                                    }
                                    return;
                                }
                            }
                            
                            TZrUInt32 keySlot = compile_member_key_into_slot(cs, memberExpr, objSlot + 1);
                            if (keySlot == (TZrUInt32)-1) {
                                return;
                            }
                            
                            // 使用 SETTABLE 设置对象属性
                            // SETTABLE 格式: operandExtra = valueSlot, operand1[0] = tableSlot, operand1[1] = keySlot
                            if (strcmp(op, "=") == 0) {
                                // 简单赋值
                                TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TZrUInt16)rightSlot, (TZrUInt16)objSlot, (TZrUInt16)keySlot);
                                emit_instruction(cs, setTableInst);
                            } else {
                                // 复合赋值：先读取，执行运算，再写入
                                TZrUInt32 leftValueSlot = allocate_stack_slot(cs);
                                TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TZrUInt16)leftValueSlot, (TZrUInt16)objSlot, (TZrUInt16)keySlot);
                                emit_instruction(cs, getTableInst);
                                
                                // 执行运算
                                EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                                if (strcmp(op, "+=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                                } else if (strcmp(op, "-=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
                                } else if (strcmp(op, "*=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                                } else if (strcmp(op, "/=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                                } else if (strcmp(op, "%=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                                }
                                
                                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                                TZrInstruction opInst = create_instruction_2(opcode, resultSlot, (TZrUInt16)leftValueSlot, (TZrUInt16)rightSlot);
                                emit_instruction(cs, opInst);
                                
                                // 使用 SETTABLE 写入结果
                                TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TZrUInt16)resultSlot, (TZrUInt16)objSlot, (TZrUInt16)keySlot);
                                emit_instruction(cs, setTableInst);
                                
                                // 释放临时栈槽
                                ZrParser_Compiler_TrimStackBy(cs, 2); // leftValueSlot 和 resultSlot
                            }
                            
                            // 释放临时栈槽
                            ZrParser_Compiler_TrimStackBy(cs, 2); // objSlot 和 keySlot
                            return;
                        }
                    }
                }
            }
        }
        
        // 其他复杂左值暂不支持
        ZrParser_Compiler_Error(cs, "Complex left-hand side not supported yet", node->location);
    }
}

// 编译逻辑表达式（&& 和 ||，支持短路求值）
static void compile_logical_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_LOGICAL_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected logical expression", node->location);
        return;
    }
    
    const TZrChar *op = node->data.logicalExpression.op;
    SZrAstNode *left = node->data.logicalExpression.left;
    SZrAstNode *right = node->data.logicalExpression.right;
    
    // 编译左操作数
    compile_expression_non_tail(cs, left);
    TZrUInt32 leftSlot = cs->stackSlotCount - 1;
    
    // 分配结果槽位
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    
    // 创建标签用于短路求值
    TZrSize shortCircuitLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    if (strcmp(op, "&&") == 0) {
        // && 运算符：如果左操作数为false，短路返回false
        // 复制左操作数到结果槽位
        TZrInstruction copyLeftInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)destSlot, (TZrInt32)leftSlot);
        emit_instruction(cs, copyLeftInst);
        
        // 如果左操作数为false，跳转到短路标签
        TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)leftSlot, 0);
        TZrSize jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInst);
        add_pending_jump(cs, jumpIfIndex, shortCircuitLabelId);
        
        // 编译右操作数
        compile_expression_non_tail(cs, right);
        TZrUInt32 rightSlot = cs->stackSlotCount - 1;
        
        // 将右操作数复制到结果槽位
        TZrInstruction copyRightInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)destSlot, (TZrInt32)rightSlot);
        emit_instruction(cs, copyRightInst);
        
        // 跳转到结束标签
        TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
        TZrSize jumpEndIndex = cs->instructionCount;
        emit_instruction(cs, jumpEndInst);
        add_pending_jump(cs, jumpEndIndex, endLabelId);
        
        // 短路标签：左操作数为false，结果就是false（已经在destSlot中）
        resolve_label(cs, shortCircuitLabelId);
        
        // 结束标签
        resolve_label(cs, endLabelId);
    } else if (strcmp(op, "||") == 0) {
        // || 运算符：如果左操作数为true，短路返回true
        // 复制左操作数到结果槽位
        TZrInstruction copyLeftInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)destSlot, (TZrInt32)leftSlot);
        emit_instruction(cs, copyLeftInst);
        
        // 如果左操作数为true，跳转到结束标签（短路返回true）
        TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)leftSlot, 0);
        TZrSize jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInst);
        add_pending_jump(cs, jumpIfIndex, endLabelId);
        
        // 左操作数为false，需要计算右操作数
        // 编译右操作数
        compile_expression_non_tail(cs, right);
        TZrUInt32 rightSlot = cs->stackSlotCount - 1;
        
        // 将右操作数复制到结果槽位
        TZrInstruction copyRightInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)destSlot, (TZrInt32)rightSlot);
        emit_instruction(cs, copyRightInst);
        
        // 结束标签：左操作数为true时跳转到这里（结果已经是true）
        resolve_label(cs, endLabelId);
    } else {
        ZrParser_Compiler_Error(cs, "Unknown logical operator", node->location);
        return;
    }
}

// 编译条件表达式（三元运算符）
static void compile_conditional_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_CONDITIONAL_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected conditional expression", node->location);
        return;
    }
    
    SZrAstNode *test = node->data.conditionalExpression.test;
    SZrAstNode *consequent = node->data.conditionalExpression.consequent;
    SZrAstNode *alternate = node->data.conditionalExpression.alternate;
    
    // 编译条件
    compile_expression_non_tail(cs, test);
    TZrUInt32 testSlot = cs->stackSlotCount - 1;
    
    // 创建 else 和 end 标签
    TZrSize elseLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    // JUMP_IF false -> else
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)testSlot, 0);
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, elseLabelId);
    
    // 编译 then 分支
    compile_expression_non_tail(cs, consequent);
    
    // JUMP -> end
    TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpEndIndex = cs->instructionCount;
    emit_instruction(cs, jumpEndInst);
    add_pending_jump(cs, jumpEndIndex, endLabelId);
    
    // 解析 else 标签
    resolve_label(cs, elseLabelId);
    
    // 编译 else 分支
    compile_expression_non_tail(cs, alternate);
    
    // 解析 end 标签
    resolve_label(cs, endLabelId);
}

// 在脚本 AST 中查找函数声明
static SZrAstNode *find_function_declaration(SZrCompilerState *cs, SZrString *funcName) {
    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL || funcName == ZR_NULL) {
        return ZR_NULL;
    }
    
    if (cs->scriptAst->type != ZR_AST_SCRIPT) {
        return ZR_NULL;
    }
    
    SZrScript *script = &cs->scriptAst->data.script;
    if (script->statements == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 遍历顶层语句，查找函数声明
    for (TZrSize i = 0; i < script->statements->count; i++) {
        SZrAstNode *stmt = script->statements->nodes[i];
        if (stmt != ZR_NULL && stmt->type == ZR_AST_FUNCTION_DECLARATION) {
            SZrFunctionDeclaration *funcDecl = &stmt->data.functionDeclaration;
            if (funcDecl->name != ZR_NULL && funcDecl->name->name != ZR_NULL) {
                if (ZrCore_String_Equal(funcDecl->name->name, funcName)) {
                    return stmt;
                }
            }
        }
    }
    
    return ZR_NULL;
}

// 根据函数参数列表解析调用参数。
// 对已知签名的调用，统一处理命名参数重排和缺失默认值回填。
static SZrAstNodeArray *match_named_arguments(SZrCompilerState *cs, 
                                               SZrFunctionCall *call,
                                               SZrAstNodeArray *paramList) {
    if (cs == ZR_NULL || call == ZR_NULL ||
        call->args == ZR_NULL || call->argNames == ZR_NULL || paramList == ZR_NULL) {
        return call->args;
    }
    
    // 创建参数映射表：参数名 -> 参数索引
    TZrSize paramCount = paramList->count;
    SZrString **paramNames = ZrCore_Memory_RawMallocWithType(cs->state->global, 
                                                       sizeof(SZrString*) * paramCount, 
                                                       ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (paramNames == ZR_NULL) {
        return call->args;  // 内存分配失败，返回原数组
    }
    
    // 提取参数名
    for (TZrSize i = 0; i < paramCount; i++) {
        SZrAstNode *paramNode = paramList->nodes[i];
        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
            SZrParameter *param = &paramNode->data.parameter;
            if (param->name != ZR_NULL) {
                paramNames[i] = param->name->name;
            } else {
                paramNames[i] = ZR_NULL;
            }
        } else {
            paramNames[i] = ZR_NULL;
        }
    }
    
    // 创建重新排列的参数数组
    SZrAstNodeArray *reorderedArgs = ZrParser_AstNodeArray_New(cs->state, paramCount);
    if (reorderedArgs == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return call->args;
    }
    
    // 初始化数组，所有位置设为 ZR_NULL（表示未提供）
    // 注意：不能使用 ZrParser_AstNodeArray_Add 因为当 node 为 ZR_NULL 时会直接返回
    // 所以直接设置数组元素并手动更新 count
    for (TZrSize i = 0; i < paramCount; i++) {
        reorderedArgs->nodes[i] = ZR_NULL;
    }
    reorderedArgs->count = paramCount;  // 手动设置 count
    
    // 标记哪些参数已被提供
    TZrBool *provided = ZrCore_Memory_RawMallocWithType(cs->state->global, 
                                                sizeof(TZrBool) * paramCount, 
                                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (provided == ZR_NULL) {
        ZrParser_AstNodeArray_Free(cs->state, reorderedArgs);
        ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return call->args;
    }
    for (TZrSize i = 0; i < paramCount; i++) {
        provided[i] = ZR_FALSE;
    }
    
    // 处理位置参数（在命名参数之前）
    TZrSize positionalCount = 0;
    for (TZrSize i = 0; i < call->args->count; i++) {
        SZrString **namePtr = (SZrString**)ZrCore_Array_Get(call->argNames, i);
        if (namePtr != ZR_NULL && *namePtr == ZR_NULL) {
            // 位置参数
            if (positionalCount < paramCount) {
                reorderedArgs->nodes[positionalCount] = call->args->nodes[i];
                provided[positionalCount] = ZR_TRUE;
                positionalCount++;
            } else {
                // 位置参数过多
                ZrParser_Compiler_Error(cs, "Too many positional arguments", call->args->nodes[i]->location);
                ZrCore_Memory_RawFreeWithType(cs->state->global, provided, sizeof(TZrBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrParser_AstNodeArray_Free(cs->state, reorderedArgs);
                return call->args;
            }
        } else {
            // 遇到命名参数，停止处理位置参数
            break;
        }
    }
    
    // 处理命名参数
    for (TZrSize i = 0; i < call->args->count; i++) {
        SZrString **namePtr = (SZrString**)ZrCore_Array_Get(call->argNames, i);
        if (namePtr != ZR_NULL && *namePtr != ZR_NULL) {
            // 命名参数
            SZrString *argName = *namePtr;
            TZrBool found = ZR_FALSE;
            
            // 查找参数名对应的位置
            for (TZrSize j = 0; j < paramCount; j++) {
                if (paramNames[j] != ZR_NULL) {
                    if (ZrCore_String_Equal(argName, paramNames[j])) {
                        if (provided[j]) {
                            // 参数重复
                            ZrParser_Compiler_Error(cs, "Duplicate argument name", call->args->nodes[i]->location);
                            ZrCore_Memory_RawFreeWithType(cs->state->global, provided, sizeof(TZrBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                            ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                            ZrParser_AstNodeArray_Free(cs->state, reorderedArgs);
                            return call->args;
                        }
                        reorderedArgs->nodes[j] = call->args->nodes[i];
                        provided[j] = ZR_TRUE;
                        found = ZR_TRUE;
                        break;
                    }
                }
            }
            
            if (!found) {
                // 未找到匹配的参数名
                TZrNativeString nameStr = ZrCore_String_GetNativeString(argName);
                TZrChar errorMsg[256];
                snprintf(errorMsg, sizeof(errorMsg), "Unknown argument name: %s", nameStr ? nameStr : "<null>");
                ZrParser_Compiler_Error(cs, errorMsg, call->args->nodes[i]->location);
                ZrCore_Memory_RawFreeWithType(cs->state->global, provided, sizeof(TZrBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrParser_AstNodeArray_Free(cs->state, reorderedArgs);
                return call->args;
            }
        }
    }
    
    // 回填缺失参数的默认值；命名参数留下的空洞不能直接交给运行时，
    // 否则运行时只会把尾部参数补 null，无法区分“省略且有默认值”和“显式传 null”。
    for (TZrSize i = 0; i < paramCount; i++) {
        if (provided[i]) {
            continue;
        }

        SZrAstNode *paramNode = paramList->nodes[i];
        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
            SZrParameter *param = &paramNode->data.parameter;
            if (param->defaultValue != ZR_NULL) {
                reorderedArgs->nodes[i] = param->defaultValue;
                provided[i] = ZR_TRUE;
            }
        }
    }
    
    ZrCore_Memory_RawFreeWithType(cs->state->global, provided, sizeof(TZrBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    
    return reorderedArgs;
}

// 编译函数调用表达式
static void compile_function_call(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_FUNCTION_CALL) {
        ZrParser_Compiler_Error(cs, "Expected function call", node->location);
        return;
    }
    
    // 函数调用在 primary expression 中处理
    // 这里只处理参数列表
    SZrAstNodeArray *args = node->data.functionCall.args;
    if (args != ZR_NULL) {
        // 编译所有参数表达式（从右到左压栈，或从左到右，取决于调用约定）
        for (TZrSize i = 0; i < args->count; i++) {
            SZrAstNode *arg = args->nodes[i];
            if (arg != ZR_NULL) {
                ZrParser_Expression_Compile(cs, arg);
            }
        }
    }
    
    // 注意：实际的函数调用指令（FUNCTION_CALL）应该在 primary expression 中生成
    // 因为需要先编译 callee（被调用表达式）
}

// 编译成员访问表达式
static void compile_member_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_MEMBER_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected member expression", node->location);
        return;
    }
    
    // 成员访问在 primary expression 中处理
    // 这里只处理属性访问
    // 注意：实际的 GETTABLE/SETTABLE 指令应该在 primary expression 中生成
    // 因为需要先编译对象表达式
}

// 编译主表达式（属性访问链和函数调用链）
static void compile_primary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_PRIMARY_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected primary expression", node->location);
        return;
    }

    if (try_emit_compile_time_function_call(cs, node)) {
        return;
    }

    SZrPrimaryExpression *primary = &node->data.primaryExpression;
    TZrUInt32 currentSlot = (TZrUInt32)-1;
    SZrString *rootTypeName = ZR_NULL;
    TZrBool rootIsTypeReference = ZR_FALSE;
    TZrSize memberStartIndex = 0;

    // 1. 编译基础属性（标识符或表达式）
    if (primary->property != ZR_NULL) {
        compile_expression_non_tail(cs, primary->property);
        if (cs->hasError) {
            return;
        }
    } else {
        ZrParser_Compiler_Error(cs, "Primary expression property is null", node->location);
        return;
    }

    currentSlot = cs->stackSlotCount - 1;
    resolve_expression_root_type(cs, primary->property, &rootTypeName, &rootIsTypeReference);
    compile_primary_member_chain(cs, primary->property, primary->members, memberStartIndex, &currentSlot,
                                 &rootTypeName, &rootIsTypeReference);
}

static void compile_prototype_reference_expression(SZrCompilerState *cs, SZrAstNode *node) {
    SZrPrototypeReferenceExpression *prototypeExpr;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected prototype reference expression", node->location);
        return;
    }

    prototypeExpr = &node->data.prototypeReferenceExpression;
    if (prototypeExpr->target == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Prototype reference target is null", node->location);
        return;
    }

    if (resolve_construct_target_type_name(cs, prototypeExpr->target, ZR_NULL) == ZR_NULL) {
        ZrParser_Compiler_Error(cs,
                                "Prototype reference target must resolve to a registered prototype",
                                node->location);
        return;
    }

    compile_expression_non_tail(cs, prototypeExpr->target);
}

static void compile_construct_expression(SZrCompilerState *cs, SZrAstNode *node) {
    SZrConstructExpression *constructExpr;
    EZrObjectPrototypeType prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
    SZrString *typeName;
    const TZrChar *op;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_CONSTRUCT_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected construct expression", node->location);
        return;
    }

    constructExpr = &node->data.constructExpression;
    typeName = resolve_construct_target_type_name(cs, constructExpr->target, &prototypeType);
    if (typeName == ZR_NULL) {
        ZrParser_Compiler_Error(cs,
                                "Prototype construction target must resolve to a registered prototype",
                                node->location);
        return;
    }

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
        ZrParser_Compiler_Error(cs, "Module values cannot be constructed directly", node->location);
        return;
    }

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
        ZrParser_Compiler_Error(cs, "Interface prototypes cannot be constructed", node->location);
        return;
    }

    {
        TZrSize savedStackCount = cs->stackSlotCount;
        compile_expression_non_tail(cs, constructExpr->target);
        if (cs->hasError) {
            return;
        }
        ZrParser_Compiler_TrimStackToCount(cs, savedStackCount);
    }

    op = constructExpr->isNew ? "new" : "$";
    if (emit_shorthand_constructor_instance(cs, op, typeName, constructExpr->args, node->location) == (TZrUInt32)-1 &&
        !cs->hasError) {
        ZrParser_Compiler_Error(cs, "Failed to compile construct expression", node->location);
    }
}

// 编译数组字面量
static void compile_array_literal(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_ARRAY_LITERAL) {
        ZrParser_Compiler_Error(cs, "Expected array literal", node->location);
        return;
    }
    
    SZrArrayLiteral *arrayLiteral = &node->data.arrayLiteral;
    
    // 1. 创建空数组对象
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    TZrInstruction createArrayInst = create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_ARRAY), (TZrUInt16)destSlot);
    emit_instruction(cs, createArrayInst);
    
    // 2. 编译每个元素并设置到数组中
    if (arrayLiteral->elements != ZR_NULL) {
        for (TZrSize i = 0; i < arrayLiteral->elements->count; i++) {
            SZrAstNode *elemNode = arrayLiteral->elements->nodes[i];
            if (elemNode == ZR_NULL) {
                continue;
            }
            
            // 编译元素值
            ZrParser_Expression_Compile(cs, elemNode);
            TZrUInt32 valueSlot = cs->stackSlotCount - 1;
            
            // 创建索引常量
            SZrTypeValue indexValue;
            ZrCore_Value_InitAsInt(cs->state, &indexValue, (TZrInt64)i);
            TZrUInt32 indexConstantIndex = add_constant(cs, &indexValue);
            
            // 将索引压栈
            TZrUInt32 indexSlot = allocate_stack_slot(cs);
            TZrInstruction getIndexInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)indexSlot, (TZrInt32)indexConstantIndex);
            emit_instruction(cs, getIndexInst);
            
            // 使用 SETTABLE 设置数组元素
            // SETTABLE 的格式: operandExtra = valueSlot (destination/value), operand1[0] = tableSlot, operand1[1] = keySlot
            TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TZrUInt16)valueSlot, (TZrUInt16)destSlot, (TZrUInt16)indexSlot);
            emit_instruction(cs, setTableInst);
            
            // 丢弃元素表达式留下的所有临时值，只保留数组对象本身。
            collapse_stack_to_slot(cs, destSlot);
        }
    }
    
    // 3. 数组对象已经在 destSlot，结果留在 destSlot
    collapse_stack_to_slot(cs, destSlot);
}

// 编译对象字面量
static void compile_object_literal(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_OBJECT_LITERAL) {
        ZrParser_Compiler_Error(cs, "Expected object literal", node->location);
        return;
    }
    
    SZrObjectLiteral *objectLiteral = &node->data.objectLiteral;
    
    // 1. 创建空对象
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    TZrInstruction createObjectInst = create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_OBJECT), (TZrUInt16)destSlot);
    emit_instruction(cs, createObjectInst);
    
    // 2. 编译每个键值对并设置到对象中
    if (objectLiteral->properties != ZR_NULL) {
        for (TZrSize i = 0; i < objectLiteral->properties->count; i++) {
            SZrAstNode *kvNode = objectLiteral->properties->nodes[i];
            if (kvNode == ZR_NULL || kvNode->type != ZR_AST_KEY_VALUE_PAIR) {
                continue;
            }
            
            SZrKeyValuePair *kv = &kvNode->data.keyValuePair;
            
            // 编译键
            if (kv->key != ZR_NULL) {
                // 键可能是标识符、字符串字面量或表达式（计算键）
                if (kv->key->type == ZR_AST_IDENTIFIER_LITERAL) {
                    // 标识符键：转换为字符串常量
                    SZrString *keyName = kv->key->data.identifier.name;
                    if (keyName != ZR_NULL) {
                        SZrTypeValue keyValue;
                        ZrCore_Value_InitAsRawObject(cs->state, &keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyName));
                        keyValue.type = ZR_VALUE_TYPE_STRING;
                        TZrUInt32 keyConstantIndex = add_constant(cs, &keyValue);
                        
                        TZrUInt32 keySlot = allocate_stack_slot(cs);
                        TZrInstruction getKeyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)keySlot, (TZrInt32)keyConstantIndex);
                        emit_instruction(cs, getKeyInst);
                        
                        // 编译值
                        ZrParser_Expression_Compile(cs, kv->value);
                        TZrUInt32 valueSlot = cs->stackSlotCount - 1;
                        
                        // 使用 SETTABLE 设置对象属性
                        TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TZrUInt16)valueSlot, (TZrUInt16)destSlot, (TZrUInt16)keySlot);
                        emit_instruction(cs, setTableInst);
                        
                        // 丢弃属性表达式留下的所有临时值，只保留对象本身。
                        collapse_stack_to_slot(cs, destSlot);
                    }
                } else {
                    // 键是表达式（字符串字面量或计算键）
                    ZrParser_Expression_Compile(cs, kv->key);
                    TZrUInt32 keySlot = cs->stackSlotCount - 1;
                    
                    // 编译值
                    ZrParser_Expression_Compile(cs, kv->value);
                    TZrUInt32 valueSlot = cs->stackSlotCount - 1;
                    
                    // 使用 SETTABLE 设置对象属性
                    TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TZrUInt16)valueSlot, (TZrUInt16)destSlot, (TZrUInt16)keySlot);
                    emit_instruction(cs, setTableInst);
                    
                    // 丢弃属性表达式留下的所有临时值，只保留对象本身。
                    collapse_stack_to_slot(cs, destSlot);
                }
            }
        }
    }
    
    // 3. 对象已经在 destSlot，结果留在 destSlot
    collapse_stack_to_slot(cs, destSlot);
}

// 编译 Lambda 表达式
static void compile_lambda_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_LAMBDA_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected lambda expression", node->location);
        return;
    }
    
    SZrLambdaExpression *lambda = &node->data.lambdaExpression;
    
    // Lambda 表达式类似于匿名函数，需要创建一个嵌套函数
    // 1. 创建一个临时的函数声明节点来复用函数编译逻辑
    // 2. 或者直接在这里实现类似的逻辑
    
    // 保存当前编译器状态
    SZrFunction *oldFunction = cs->currentFunction;
    TZrSize oldInstructionCount = cs->instructionCount;
    TZrSize oldStackSlotCount = cs->stackSlotCount;
    TZrSize oldLocalVarCount = cs->localVarCount;
    TZrSize oldConstantCount = cs->constantCount;
    TZrSize oldClosureVarCount = cs->closureVarCount;
    TZrSize oldInstructionLength = cs->instructions.length;
    TZrSize oldLocalVarLength = cs->localVars.length;
    TZrSize oldConstantLength = cs->constants.length;
    TZrSize oldClosureVarLength = cs->closureVars.length;
    TZrBool oldIsInConstructor = cs->isInConstructor;
    SZrAstNode *oldFunctionNode = cs->currentFunctionNode;
    TZrSize oldConstLocalVarLength = cs->constLocalVars.length;
    TZrSize oldConstParameterLength = cs->constParameters.length;
    TZrInstruction *savedParentInstructions = ZR_NULL;
    SZrFunctionLocalVariable *savedParentLocalVars = ZR_NULL;
    SZrTypeValue *savedParentConstants = ZR_NULL;
    SZrFunctionClosureVariable *savedParentClosureVars = ZR_NULL;
    TZrSize savedParentInstructionsSize = oldInstructionLength * sizeof(TZrInstruction);
    TZrSize savedParentLocalVarsSize = oldLocalVarLength * sizeof(SZrFunctionLocalVariable);
    TZrSize savedParentConstantsSize = oldConstantLength * sizeof(SZrTypeValue);
    TZrSize savedParentClosureVarsSize = oldClosureVarLength * sizeof(SZrFunctionClosureVariable);

    if (savedParentInstructionsSize > 0) {
        savedParentInstructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentInstructionsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentInstructions == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "Failed to backup parent instructions for lambda expression", node->location);
            return;
        }
        memcpy(savedParentInstructions, cs->instructions.head, savedParentInstructionsSize);
    }

    if (savedParentLocalVarsSize > 0) {
        savedParentLocalVars = (SZrFunctionLocalVariable *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentLocalVarsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentLocalVars == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent locals for lambda expression", node->location);
            return;
        }
        memcpy(savedParentLocalVars, cs->localVars.head, savedParentLocalVarsSize);
    }

    if (savedParentConstantsSize > 0) {
        savedParentConstants = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentConstantsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentConstants == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentLocalVars != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent constants for lambda expression", node->location);
            return;
        }
        memcpy(savedParentConstants, cs->constants.head, savedParentConstantsSize);
    }

    if (savedParentClosureVarsSize > 0) {
        savedParentClosureVars = (SZrFunctionClosureVariable *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentClosureVarsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentClosureVars == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentLocalVars != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentConstants != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent closures for lambda expression", node->location);
            return;
        }
        memcpy(savedParentClosureVars, cs->closureVars.head, savedParentClosureVarsSize);
    }
    
    // 创建新的函数对象
    cs->isInConstructor = ZR_FALSE;
    cs->currentFunctionNode = node;
    cs->constLocalVars.length = 0;
    cs->constParameters.length = 0;
    cs->currentFunction = ZrCore_Function_New(cs->state);
    if (cs->currentFunction == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create lambda function object", node->location);
        if (savedParentInstructions != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentLocalVars != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentConstants != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentClosureVars != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = oldFunctionNode;
        cs->constLocalVars.length = oldConstLocalVarLength;
        cs->constParameters.length = oldConstParameterLength;
        return;
    }
    
    // 重置编译器状态（为新函数）
    cs->instructionCount = 0;
    cs->stackSlotCount = 0;
    cs->localVarCount = 0;
    cs->constantCount = 0;
    cs->closureVarCount = 0;
    
    // 清空数组（但保留已分配的内存）
    cs->instructions.length = 0;
    cs->localVars.length = 0;
    cs->constants.length = 0;
    cs->closureVars.length = 0;
    
    // 进入函数作用域
    enter_scope(cs);
    
    // 1. 编译参数列表
    TZrUInt32 parameterCount = 0;
    if (lambda->params != ZR_NULL) {
        for (TZrSize i = 0; i < lambda->params->count; i++) {
            SZrAstNode *paramNode = lambda->params->nodes[i];
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                if (param->name != ZR_NULL) {
                    SZrString *paramName = param->name->name;
                    if (paramName != ZR_NULL) {
                        // 分配参数槽位
                        allocate_local_var(cs, paramName);
                        parameterCount++;
                    }
                }
            }
        }
    }

    if (oldFunction != ZR_NULL && lambda->block != ZR_NULL) {
        SZrCompilerState parentCompilerSnapshot = {0};
        parentCompilerSnapshot.localVars.head = (TZrByte *)savedParentLocalVars;
        parentCompilerSnapshot.localVars.elementSize = sizeof(SZrFunctionLocalVariable);
        parentCompilerSnapshot.localVars.length = oldLocalVarLength;
        parentCompilerSnapshot.localVars.capacity = oldLocalVarLength;
        parentCompilerSnapshot.localVars.isValid = ZR_TRUE;
        parentCompilerSnapshot.closureVars.head = (TZrByte *)savedParentClosureVars;
        parentCompilerSnapshot.closureVars.elementSize = sizeof(SZrFunctionClosureVariable);
        parentCompilerSnapshot.closureVars.length = oldClosureVarLength;
        parentCompilerSnapshot.closureVars.capacity = oldClosureVarLength;
        parentCompilerSnapshot.closureVars.isValid = ZR_TRUE;
        ZrParser_ExternalVariables_Analyze(cs, lambda->block, &parentCompilerSnapshot);
    }
    
    // 检查是否有可变参数
    TZrBool hasVariableArguments = (lambda->args != ZR_NULL);
    
    // 2. 编译函数体（block）
    if (lambda->block != ZR_NULL) {
        ZrParser_Statement_Compile(cs, lambda->block);
    }
    
    // 如果没有显式返回，添加隐式返回
    if (!cs->hasError) {
        if (cs->instructions.length == 0) {
            // 如果没有任何指令，添加隐式返回 null
            TZrUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrCore_Value_ResetAsNull(&nullValue);
            TZrUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)resultSlot, (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            
            TZrInstruction returnInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16)resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            // 检查最后一条指令是否是 RETURN
            if (cs->instructions.length > 0) {
                TZrInstruction *lastInst = (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode)lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        // 添加隐式返回 null
                        TZrUInt32 resultSlot = allocate_stack_slot(cs);
                        SZrTypeValue nullValue;
                        ZrCore_Value_ResetAsNull(&nullValue);
                        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)resultSlot, (TZrInt32)constantIndex);
                        emit_instruction(cs, inst);
                        
                        TZrInstruction returnInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16)resultSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            }
        }
    }
    
    // 退出函数作用域
    exit_scope(cs);

    if (cs->hasError) {
        if (cs->currentFunction != ZR_NULL) {
            ZrCore_Function_Free(cs->state, cs->currentFunction);
        }
        if (savedParentInstructions != ZR_NULL && savedParentInstructionsSize > 0) {
            memcpy(cs->instructions.head, savedParentInstructions, savedParentInstructionsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentLocalVars != ZR_NULL && savedParentLocalVarsSize > 0) {
            memcpy(cs->localVars.head, savedParentLocalVars, savedParentLocalVarsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentConstants != ZR_NULL && savedParentConstantsSize > 0) {
            memcpy(cs->constants.head, savedParentConstants, savedParentConstantsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentClosureVars != ZR_NULL && savedParentClosureVarsSize > 0) {
            memcpy(cs->closureVars.head, savedParentClosureVars, savedParentClosureVarsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        cs->currentFunction = oldFunction;
        cs->instructionCount = oldInstructionCount;
        cs->stackSlotCount = oldStackSlotCount;
        cs->localVarCount = oldLocalVarCount;
        cs->constantCount = oldConstantCount;
        cs->closureVarCount = oldClosureVarCount;
        cs->instructions.length = oldInstructionLength;
        cs->localVars.length = oldLocalVarLength;
        cs->constants.length = oldConstantLength;
        cs->closureVars.length = oldClosureVarLength;
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = oldFunctionNode;
        cs->constLocalVars.length = oldConstLocalVarLength;
        cs->constParameters.length = oldConstParameterLength;
        return;
    }
    
    // 3. 将编译结果复制到函数对象
    SZrFunction *newFunc = cs->currentFunction;
    SZrGlobalState *global = cs->state->global;
    
    // 复制指令列表
    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TZrUInt32)cs->instructions.length;
            cs->instructionCount = cs->instructions.length;
        }
    }
    
    // 复制常量列表
    if (cs->constantCount > 0) {
        TZrSize constSize = cs->constantCount * sizeof(SZrTypeValue);
        newFunc->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->constantValueList != ZR_NULL) {
            memcpy(newFunc->constantValueList, cs->constants.head, constSize);
            newFunc->constantValueLength = (TZrUInt32)cs->constantCount;
        }
    }
    
    // 复制局部变量列表
    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *)ZrCore_Memory_RawMallocWithType(global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TZrUInt32)cs->localVars.length;
            cs->localVarCount = cs->localVars.length;
        }
    }
    
    // 复制闭包变量列表
    if (cs->closureVarCount > 0) {
        TZrSize closureVarSize = cs->closureVarCount * sizeof(SZrFunctionClosureVariable);
        newFunc->closureValueList = (SZrFunctionClosureVariable *)ZrCore_Memory_RawMallocWithType(global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->closureValueList != ZR_NULL) {
            memcpy(newFunc->closureValueList, cs->closureVars.head, closureVarSize);
            newFunc->closureValueLength = (TZrUInt32)cs->closureVarCount;
        }
    }
    
    // 设置函数元数据
    newFunc->stackSize = (TZrUInt32)cs->stackSlotCount;
    newFunc->parameterCount = (TZrUInt16)parameterCount;
    newFunc->hasVariableArguments = hasVariableArguments;
    newFunc->lineInSourceStart = (node->location.start.line > 0) ? (TZrUInt32)node->location.start.line : 0;
    newFunc->lineInSourceEnd = (node->location.end.line > 0) ? (TZrUInt32)node->location.end.line : 0;
    
    // 设置函数名（lambda 表达式是匿名函数，所以为 ZR_NULL）
    newFunc->functionName = ZR_NULL;
    
    if (savedParentInstructions != ZR_NULL && savedParentInstructionsSize > 0) {
        memcpy(cs->instructions.head, savedParentInstructions, savedParentInstructionsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (savedParentLocalVars != ZR_NULL && savedParentLocalVarsSize > 0) {
        memcpy(cs->localVars.head, savedParentLocalVars, savedParentLocalVarsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (savedParentConstants != ZR_NULL && savedParentConstantsSize > 0) {
        memcpy(cs->constants.head, savedParentConstants, savedParentConstantsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (savedParentClosureVars != ZR_NULL && savedParentClosureVarsSize > 0) {
        memcpy(cs->closureVars.head, savedParentClosureVars, savedParentClosureVarsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    // 恢复旧的编译器状态
    cs->currentFunction = oldFunction;
    cs->instructionCount = oldInstructionCount;
    cs->stackSlotCount = oldStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;
    cs->instructions.length = oldInstructionLength;
    cs->localVars.length = oldLocalVarLength;
    cs->constants.length = oldConstantLength;
    cs->closureVars.length = oldClosureVarLength;
    cs->isInConstructor = oldIsInConstructor;
    cs->currentFunctionNode = oldFunctionNode;
    cs->constLocalVars.length = oldConstLocalVarLength;
    cs->constParameters.length = oldConstParameterLength;

    // 4. 在父函数上下文中生成 CREATE_CLOSURE。
    // Lambda 运行时值属于外层函数，常量索引和结果槽也必须从外层函数分配。
    SZrTypeValue funcValue;
    ZrCore_Value_InitAsRawObject(cs->state, &funcValue, ZR_CAST_RAW_OBJECT_AS_SUPER(newFunc));
    funcValue.type = ZR_VALUE_TYPE_FUNCTION;

    TZrUInt32 funcConstantIndex = add_constant(cs, &funcValue);
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    TZrUInt32 closureVarCount = (TZrUInt32)newFunc->closureValueLength;

    // CREATE_CLOSURE 的格式: operandExtra = destSlot, operand1[0] = functionConstantIndex, operand1[1] = closureVarCount
    TZrInstruction createClosureInst = create_instruction_2(
            ZR_INSTRUCTION_ENUM(CREATE_CLOSURE),
            (TZrUInt16)destSlot,
            (TZrUInt16)funcConstantIndex,
            (TZrUInt16)closureVarCount);
    emit_instruction(cs, createClosureInst);
}

// 辅助函数：编译块并提取最后一个表达式的值（用于if表达式等场景）
static void compile_block_as_expression(SZrCompilerState *cs, SZrAstNode *blockNode) {
    if (cs == ZR_NULL || blockNode == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (blockNode->type != ZR_AST_BLOCK) {
        // 如果不是块，直接编译为表达式
        ZrParser_Expression_Compile(cs, blockNode);
        return;
    }
    
    SZrBlock *block = &blockNode->data.block;
    
    // 进入新作用域
    enter_scope(cs);
    
    // 编译块内所有语句
    if (block->body != ZR_NULL && block->body->count > 0) {
        // 编译除最后一个语句外的所有语句
        for (TZrSize i = 0; i < block->body->count - 1; i++) {
            SZrAstNode *stmt = block->body->nodes[i];
            if (stmt != ZR_NULL) {
                ZrParser_Statement_Compile(cs, stmt);
                if (cs->hasError) {
                    exit_scope(cs);
                    return;
                }
            }
        }
        
        // 编译最后一个语句，并提取其表达式的值
        SZrAstNode *lastStmt = block->body->nodes[block->body->count - 1];
        if (lastStmt != ZR_NULL) {
            if (lastStmt->type == ZR_AST_EXPRESSION_STATEMENT) {
                // 表达式语句：编译表达式，值留在栈上
                SZrExpressionStatement *exprStmt = &lastStmt->data.expressionStatement;
                if (exprStmt->expr != ZR_NULL) {
                    ZrParser_Expression_Compile(cs, exprStmt->expr);
                } else {
                    // 空表达式语句，返回null
                    SZrTypeValue nullValue;
                    ZrCore_Value_ResetAsNull(&nullValue);
                    TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                    TZrUInt32 destSlot = allocate_stack_slot(cs);
                    TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TZrInt32)constantIndex);
                    emit_instruction(cs, inst);
                }
            } else {
                // 其他类型的语句：编译后返回null
                ZrParser_Statement_Compile(cs, lastStmt);
                SZrTypeValue nullValue;
                ZrCore_Value_ResetAsNull(&nullValue);
                TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                TZrUInt32 destSlot = allocate_stack_slot(cs);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TZrInt32)constantIndex);
                emit_instruction(cs, inst);
            }
        } else {
            // 最后一个语句为空，返回null
            SZrTypeValue nullValue;
            ZrCore_Value_ResetAsNull(&nullValue);
            TZrUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrUInt32 destSlot = allocate_stack_slot(cs);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
        }
    } else {
        // 空块，返回null
        SZrTypeValue nullValue;
        ZrCore_Value_ResetAsNull(&nullValue);
        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TZrInt32)constantIndex);
        emit_instruction(cs, inst);
    }
    
    // 退出作用域
    exit_scope(cs);
}

// 编译 If 表达式
static void compile_if_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_IF_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected if expression", node->location);
        return;
    }
    
    SZrIfExpression *ifExpr = &node->data.ifExpression;
    
    // 编译条件表达式
    ZrParser_Expression_Compile(cs, ifExpr->condition);
    TZrUInt32 condSlot = cs->stackSlotCount - 1;
    
    // 创建 else 和 end 标签
    TZrSize elseLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    // JUMP_IF false -> else
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)condSlot, 0);
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, elseLabelId);
    
    // 编译 then 分支
    if (ifExpr->thenExpr != ZR_NULL) {
        // 检查是否是块，如果是块则编译块并提取最后一个表达式的值
        if (ifExpr->thenExpr->type == ZR_AST_BLOCK) {
            compile_block_as_expression(cs, ifExpr->thenExpr);
        } else {
            ZrParser_Expression_Compile(cs, ifExpr->thenExpr);
        }
    } else {
        // 如果没有then分支，使用null值
        SZrTypeValue nullValue;
        ZrCore_Value_ResetAsNull(&nullValue);
        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TZrInt32)constantIndex);
        emit_instruction(cs, inst);
    }
    
    // JUMP -> end
    TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpEndIndex = cs->instructionCount;
    emit_instruction(cs, jumpEndInst);
    add_pending_jump(cs, jumpEndIndex, endLabelId);
    
    // 解析 else 标签
    resolve_label(cs, elseLabelId);
    
    // 编译 else 分支
    if (ifExpr->elseExpr != ZR_NULL) {
        // 检查是否是块，如果是块则编译块并提取最后一个表达式的值
        if (ifExpr->elseExpr->type == ZR_AST_BLOCK) {
            compile_block_as_expression(cs, ifExpr->elseExpr);
        } else if (ifExpr->elseExpr->type == ZR_AST_IF_EXPRESSION) {
            // else if 情况：递归编译
            compile_if_expression(cs, ifExpr->elseExpr);
        } else {
            ZrParser_Expression_Compile(cs, ifExpr->elseExpr);
        }
    } else {
        // 如果没有else分支，使用null值
        SZrTypeValue nullValue;
        ZrCore_Value_ResetAsNull(&nullValue);
        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), destSlot, (TZrInt32)constantIndex);
        emit_instruction(cs, inst);
    }
    
    // 解析 end 标签
    resolve_label(cs, endLabelId);
}

// 编译 Switch 表达式
static void compile_switch_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_SWITCH_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected switch expression", node->location);
        return;
    }
    
    SZrSwitchExpression *switchExpr = &node->data.switchExpression;
    
    // 编译 switch 表达式
    ZrParser_Expression_Compile(cs, switchExpr->expr);
    TZrUInt32 exprSlot = cs->stackSlotCount - 1;
    
    // 分配结果槽位（用于存储匹配的值）
    TZrUInt32 resultSlot = allocate_stack_slot(cs);
    
    // 创建结束标签
    TZrSize endLabelId = create_label(cs);
    
    // 编译所有 case
    TZrBool hasMatchedCase = ZR_FALSE;
    if (switchExpr->cases != ZR_NULL) {
        for (TZrSize i = 0; i < switchExpr->cases->count; i++) {
            SZrAstNode *caseNode = switchExpr->cases->nodes[i];
            if (caseNode == ZR_NULL || caseNode->type != ZR_AST_SWITCH_CASE) {
                continue;
            }
            
            SZrSwitchCase *switchCase = &caseNode->data.switchCase;
            
            // 编译 case 值
            ZrParser_Expression_Compile(cs, switchCase->value);
            TZrUInt32 caseValueSlot = cs->stackSlotCount - 1;
            
            // 比较表达式和 case 值
            TZrUInt32 compareSlot = allocate_stack_slot(cs);
            TZrInstruction compareInst = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL), compareSlot, (TZrUInt16)exprSlot, (TZrUInt16)caseValueSlot);
            emit_instruction(cs, compareInst);
            
            // 创建下一个 case 标签
            TZrSize nextCaseLabelId = create_label(cs);
            
            // JUMP_IF false -> next case
            TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)compareSlot, 0);
            TZrSize jumpIfIndex = cs->instructionCount;
            emit_instruction(cs, jumpIfInst);
            add_pending_jump(cs, jumpIfIndex, nextCaseLabelId);
            
            // 释放临时栈槽（compareSlot 和 caseValueSlot）
            ZrParser_Compiler_TrimStackBy(cs, 2);
            
            // 编译 case 块（作为表达式，需要返回值）
            if (switchExpr->isStatement) {
                // 作为语句：编译块
                if (switchCase->block != ZR_NULL) {
                    ZrParser_Statement_Compile(cs, switchCase->block);
                }
                // 语句不需要返回值，直接跳转到结束
                TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
                TZrSize jumpEndIndex = cs->instructionCount;
                emit_instruction(cs, jumpEndInst);
                add_pending_jump(cs, jumpEndIndex, endLabelId);
            } else {
                // 作为表达式：编译块，最后一个表达式作为返回值
                if (switchCase->block != ZR_NULL) {
                    SZrBlock *block = &switchCase->block->data.block;
                    if (block->body != ZR_NULL && block->body->count > 0) {
                        // 编译块中所有语句
                        for (TZrSize j = 0; j < block->body->count - 1; j++) {
                            SZrAstNode *stmt = block->body->nodes[j];
                            if (stmt != ZR_NULL) {
                                ZrParser_Statement_Compile(cs, stmt);
                            }
                        }
                        // 最后一个语句作为返回值（如果是表达式）
                        SZrAstNode *lastStmt = block->body->nodes[block->body->count - 1];
                        if (lastStmt != ZR_NULL) {
                            // 尝试作为表达式编译
                            ZrParser_Expression_Compile(cs, lastStmt);
                            TZrUInt32 lastValueSlot = cs->stackSlotCount - 1;
                            // 复制到结果槽位
                            TZrInstruction copyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), resultSlot, (TZrInt32)lastValueSlot);
                            emit_instruction(cs, copyInst);
                            ZrParser_Compiler_TrimStackBy(cs, 1); // 释放 lastValueSlot
                        }
                    } else {
                        // 空块，返回 null
                        SZrTypeValue nullValue;
                        ZrCore_Value_ResetAsNull(&nullValue);
                        TZrUInt32 nullConstantIndex = add_constant(cs, &nullValue);
                        TZrInstruction nullInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), resultSlot, (TZrInt32)nullConstantIndex);
                        emit_instruction(cs, nullInst);
                    }
                } else {
                    // 空块，返回 null
                    SZrTypeValue nullValue;
                    ZrCore_Value_ResetAsNull(&nullValue);
                    TZrUInt32 nullConstantIndex = add_constant(cs, &nullValue);
                    TZrInstruction nullInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), resultSlot, (TZrInt32)nullConstantIndex);
                    emit_instruction(cs, nullInst);
                }
                hasMatchedCase = ZR_TRUE;
                
                // 跳转到结束标签
                TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
                TZrSize jumpEndIndex = cs->instructionCount;
                emit_instruction(cs, jumpEndInst);
                add_pending_jump(cs, jumpEndIndex, endLabelId);
            }
            
            // 解析下一个 case 标签
            resolve_label(cs, nextCaseLabelId);
        }
    }
    
    // 编译 default case
    if (switchExpr->defaultCase != ZR_NULL) {
        SZrSwitchDefault *defaultCase = &switchExpr->defaultCase->data.switchDefault;
        if (switchExpr->isStatement) {
            // 作为语句：编译块
            if (defaultCase->block != ZR_NULL) {
                ZrParser_Statement_Compile(cs, defaultCase->block);
            }
        } else {
            // 作为表达式：编译块，最后一个表达式作为返回值
            if (defaultCase->block != ZR_NULL) {
                SZrBlock *block = &defaultCase->block->data.block;
                if (block->body != ZR_NULL && block->body->count > 0) {
                    // 编译块中所有语句
                    for (TZrSize j = 0; j < block->body->count - 1; j++) {
                        SZrAstNode *stmt = block->body->nodes[j];
                        if (stmt != ZR_NULL) {
                            ZrParser_Statement_Compile(cs, stmt);
                        }
                    }
                    // 最后一个语句作为返回值
                    SZrAstNode *lastStmt = block->body->nodes[block->body->count - 1];
                    if (lastStmt != ZR_NULL) {
                        ZrParser_Expression_Compile(cs, lastStmt);
                        TZrUInt32 lastValueSlot = cs->stackSlotCount - 1;
                        // 复制到结果槽位
                        TZrInstruction copyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), resultSlot, (TZrInt32)lastValueSlot);
                        emit_instruction(cs, copyInst);
                        ZrParser_Compiler_TrimStackBy(cs, 1); // 释放 lastValueSlot
                    }
                } else {
                    // 空块，返回 null
                    SZrTypeValue nullValue;
                    ZrCore_Value_ResetAsNull(&nullValue);
                    TZrUInt32 nullConstantIndex = add_constant(cs, &nullValue);
                    TZrInstruction nullInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), resultSlot, (TZrInt32)nullConstantIndex);
                    emit_instruction(cs, nullInst);
                }
            } else {
                // 空块，返回 null
                SZrTypeValue nullValue;
                ZrCore_Value_ResetAsNull(&nullValue);
                TZrUInt32 nullConstantIndex = add_constant(cs, &nullValue);
                TZrInstruction nullInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), resultSlot, (TZrInt32)nullConstantIndex);
                emit_instruction(cs, nullInst);
            }
            hasMatchedCase = ZR_TRUE;
        }
    } else if (!switchExpr->isStatement && !hasMatchedCase) {
        // 作为表达式但没有匹配的 case 也没有 default，返回 null
        SZrTypeValue nullValue;
        ZrCore_Value_ResetAsNull(&nullValue);
        TZrUInt32 nullConstantIndex = add_constant(cs, &nullValue);
        TZrInstruction nullInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), resultSlot, (TZrInt32)nullConstantIndex);
        emit_instruction(cs, nullInst);
    }
    
    // 释放表达式栈槽
    ZrParser_Compiler_TrimStackBy(cs, 1);
    
    // 解析结束标签
    resolve_label(cs, endLabelId);
}

// 主编译表达式函数
ZR_PARSER_API void ZrParser_Expression_Compile(SZrCompilerState *cs, SZrAstNode *node) {
    SZrAstNode *oldCurrentAst;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    oldCurrentAst = cs->currentAst;
    cs->currentAst = node;
    
    switch (node->type) {
        // 字面量
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
            compile_literal(cs, node);
            break;

        case ZR_AST_TEMPLATE_STRING_LITERAL:
            compile_template_string_literal(cs, node);
            break;
        
        case ZR_AST_IDENTIFIER_LITERAL:
            compile_identifier(cs, node);
            break;
        
        // 表达式
        case ZR_AST_UNARY_EXPRESSION:
            compile_unary_expression(cs, node);
            break;
        
        case ZR_AST_TYPE_CAST_EXPRESSION:
            compile_type_cast_expression(cs, node);
            break;
        
        case ZR_AST_BINARY_EXPRESSION:
            compile_binary_expression(cs, node);
            break;
        
        case ZR_AST_LOGICAL_EXPRESSION:
            compile_logical_expression(cs, node);
            break;
        
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            compile_assignment_expression(cs, node);
            break;
        
        case ZR_AST_CONDITIONAL_EXPRESSION:
            compile_conditional_expression(cs, node);
            break;
        
        case ZR_AST_FUNCTION_CALL:
            compile_function_call(cs, node);
            break;
        
        case ZR_AST_MEMBER_EXPRESSION:
            compile_member_expression(cs, node);
            break;

        case ZR_AST_PRIMARY_EXPRESSION:
            compile_primary_expression(cs, node);
            break;

        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            compile_prototype_reference_expression(cs, node);
            break;

        case ZR_AST_CONSTRUCT_EXPRESSION:
            compile_construct_expression(cs, node);
            break;

        case ZR_AST_ARRAY_LITERAL:
            compile_array_literal(cs, node);
            break;
        
        case ZR_AST_OBJECT_LITERAL:
            compile_object_literal(cs, node);
            break;
        
        case ZR_AST_LAMBDA_EXPRESSION:
            compile_lambda_expression(cs, node);
            break;
        
        case ZR_AST_BLOCK:
            // 块作为表达式使用时，编译块并提取最后一个表达式的值
            compile_block_as_expression(cs, node);
            break;
        
        // 控制流结构和语句不应该作为表达式编译，应该先转换为语句
        case ZR_AST_IF_EXPRESSION:
        case ZR_AST_SWITCH_EXPRESSION:
        case ZR_AST_WHILE_LOOP:
        case ZR_AST_FOR_LOOP:
        case ZR_AST_FOREACH_LOOP:
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
        case ZR_AST_OUT_STATEMENT:
        case ZR_AST_THROW_STATEMENT:
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            ZrParser_Compiler_Error(cs, "Loop or statement cannot be used as expression", node->location);
            break;
        
        // 解构对象和数组不是表达式，不应该在这里处理
        case ZR_AST_DESTRUCTURING_OBJECT:
        case ZR_AST_DESTRUCTURING_ARRAY:
            // 这些类型应该在变量声明中处理，不应该作为表达式编译
            ZrParser_Compiler_Error(cs, "Destructuring pattern cannot be used as expression", node->location);
            break;
        
        default:
            // 未处理的表达式类型
            if (node->type == ZR_AST_DESTRUCTURING_OBJECT || node->type == ZR_AST_DESTRUCTURING_ARRAY) {
                // 这不应该作为表达式编译，应该在变量声明中处理
                ZrParser_Compiler_Error(cs, "Destructuring pattern cannot be used as expression", node->location);
            } else {
                // 创建详细的错误消息，包含类型名称和位置信息
                static TZrChar errorMsg[256];
                const TZrChar *typeName = "UNKNOWN";
                switch (node->type) {
                    case ZR_AST_INTERFACE_METHOD_SIGNATURE: typeName = "INTERFACE_METHOD_SIGNATURE"; break;
                    case ZR_AST_INTERFACE_FIELD_DECLARATION: typeName = "INTERFACE_FIELD_DECLARATION"; break;
                    case ZR_AST_INTERFACE_PROPERTY_SIGNATURE: typeName = "INTERFACE_PROPERTY_SIGNATURE"; break;
                    case ZR_AST_INTERFACE_META_SIGNATURE: typeName = "INTERFACE_META_SIGNATURE"; break;
                    case ZR_AST_STRUCT_FIELD: typeName = "STRUCT_FIELD"; break;
                    case ZR_AST_STRUCT_METHOD: typeName = "STRUCT_METHOD"; break;
                    case ZR_AST_STRUCT_META_FUNCTION: typeName = "STRUCT_META_FUNCTION"; break;
                    case ZR_AST_CLASS_FIELD: typeName = "CLASS_FIELD"; break;
                    case ZR_AST_CLASS_METHOD: typeName = "CLASS_METHOD"; break;
                    case ZR_AST_CLASS_PROPERTY: typeName = "CLASS_PROPERTY"; break;
                    case ZR_AST_CLASS_META_FUNCTION: typeName = "CLASS_META_FUNCTION"; break;
                    case ZR_AST_FUNCTION_DECLARATION: typeName = "FUNCTION_DECLARATION"; break;
                    case ZR_AST_STRUCT_DECLARATION: typeName = "STRUCT_DECLARATION"; break;
                    case ZR_AST_CLASS_DECLARATION: typeName = "CLASS_DECLARATION"; break;
                    case ZR_AST_INTERFACE_DECLARATION: typeName = "INTERFACE_DECLARATION"; break;
                    case ZR_AST_ENUM_DECLARATION: typeName = "ENUM_DECLARATION"; break;
                    case ZR_AST_ENUM_MEMBER: typeName = "ENUM_MEMBER"; break;
                    case ZR_AST_MODULE_DECLARATION: typeName = "MODULE_DECLARATION"; break;
                    case ZR_AST_SCRIPT: typeName = "SCRIPT"; break;
                    default: break;
                }
                snprintf(errorMsg, sizeof(errorMsg), 
                        "Unexpected expression type: %s (type %d) at line %d:%d. "
                        "This node type should not be compiled as an expression. "
                        "Please check if it was incorrectly placed in an expression context.",
                        typeName, node->type, 
                        node->location.start.line, node->location.start.column);
                ZrParser_Compiler_Error(cs, errorMsg, node->location);
            }
            break;
    }

    cs->currentAst = oldCurrentAst;
}
