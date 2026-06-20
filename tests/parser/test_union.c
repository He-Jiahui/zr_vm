#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/gc.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"

#pragma pack(push, 1)
typedef struct SZrUnionCompiledPrototypeInfoView {
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
} SZrUnionCompiledPrototypeInfoView;

typedef struct SZrUnionCompiledMemberInfoView {
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
    TZrUInt32 accessorRole;
} SZrUnionCompiledMemberInfoView;
#pragma pack(pop)

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrAstNode *parse_union_source(const char *source) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(source);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_test.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
}

static void install_union_native_probe(const char *name, FZrNativeFunction nativeFunction) {
    SZrObject *globalObject;
    SZrClosureNative *closure;
    SZrString *nameString;
    SZrTypeValue key;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(g_state);
    TEST_ASSERT_NOT_NULL(g_state->global);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_NOT_NULL(nativeFunction);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, g_state->global->zrObject.type);
    TEST_ASSERT_NOT_NULL(g_state->global->zrObject.value.object);

    globalObject = ZR_CAST_OBJECT(g_state, g_state->global->zrObject.value.object);
    TEST_ASSERT_NOT_NULL(globalObject);

    closure = ZrCore_ClosureNative_New(g_state, 0);
    TEST_ASSERT_NOT_NULL(closure);
    closure->nativeFunction = nativeFunction;
    ZrCore_RawObject_MarkAsPermanent(g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));

    nameString = ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name));
    TEST_ASSERT_NOT_NULL(nameString);

    ZrCore_Value_InitAsRawObject(g_state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(nameString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(g_state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    value.type = ZR_VALUE_TYPE_CLOSURE;
    value.isNative = ZR_TRUE;

    ZrCore_Object_SetValue(g_state, globalObject, &key, &value);
}

static TZrBool inline_union_shape_frame_has_circle_payload(SZrFunction *function,
                                                           TZrStackValuePointer frameBase) {
    TZrUInt8 tag;
    TZrFloat32 radius;

    if (function == ZR_NULL || frameBase == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[index];
        SZrStackFramePlace place;

        if (slotLayout->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
            slotLayout->byteSize != 12u ||
            slotLayout->byteAlign != 4u ||
            !ZrCore_Function_MakeFrameSlotPlace(g_state, function, frameBase, slotLayout->stackSlot, &place) ||
            place.address == ZR_NULL ||
            place.byteSize < 8u) {
            continue;
        }

        memcpy(&tag, place.address, sizeof(tag));
        memcpy(&radius, ((const TZrByte *)place.address) + 4u, sizeof(radius));
        if (tag == 1u && radius > 2.49f && radius < 2.51f) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrInt64 probe_inline_union_shape_frame_native(SZrState *state) {
    SZrCallInfo *nativeCallInfo;
    SZrCallInfo *callerCallInfo;
    TZrStackValuePointer resultSlot;
    TZrBool matched = ZR_FALSE;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    nativeCallInfo = state->callInfoList;
    callerCallInfo = nativeCallInfo->previous;
    resultSlot = nativeCallInfo->functionBase.valuePointer;

    if (callerCallInfo != ZR_NULL && resultSlot != ZR_NULL) {
        SZrFunction *callerFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callerCallInfo);
        TZrStackValuePointer callerFrameBase = callerCallInfo->functionBase.valuePointer + 1;

        matched = inline_union_shape_frame_has_circle_payload(callerFunction, callerFrameBase);
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(resultSlot), matched ? 1 : 0);
        state->stackTop.valuePointer = resultSlot + 1;
        return 1;
    }

    return 0;
}

static TZrInt64 probe_union_weak_alive_native(SZrState *state) {
    SZrCallInfo *nativeCallInfo;
    SZrCallInfo *callerCallInfo;
    SZrFunction *callerFunction;
    TZrStackValuePointer callerFrameBase;
    SZrTypeValue *resultValue;
    TZrBool alive = ZR_FALSE;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    nativeCallInfo = state->callInfoList;
    callerCallInfo = nativeCallInfo->previous;
    resultValue = nativeCallInfo->functionBase.valuePointer != ZR_NULL
                          ? ZrCore_Stack_GetValue(nativeCallInfo->functionBase.valuePointer)
                          : ZR_NULL;
    if (resultValue == ZR_NULL) {
        return 0;
    }

    callerFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callerCallInfo);
    callerFrameBase = callerCallInfo != ZR_NULL ? callerCallInfo->functionBase.valuePointer + 1 : ZR_NULL;
    if (callerFunction != ZR_NULL && callerFrameBase != ZR_NULL) {
        for (TZrUInt32 index = 0u; index < callerFunction->frameSlotLayoutLength; index++) {
            const SZrFunctionFrameSlotLayout *slotLayout = &callerFunction->frameSlotLayouts[index];
            SZrStackFramePlace place;
            SZrTypeValue *value;

            if (slotLayout->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
                slotLayout->byteSize < sizeof(SZrTypeValue) ||
                !ZrCore_Function_MakeFrameSlotPlace(state, callerFunction, callerFrameBase, slotLayout->stackSlot, &place) ||
                place.address == ZR_NULL ||
                place.byteSize < sizeof(SZrTypeValue)) {
                continue;
            }

            value = (SZrTypeValue *)place.address;
            if (value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_WEAK &&
                value->ownershipControl != ZR_NULL) {
                alive = value->ownershipControl->strongRefCount > 0u ? ZR_TRUE : ZR_FALSE;
                break;
            }
        }
    }
    ZrCore_Value_InitAsInt(state, resultValue, alive ? 1 : 0);
    state->stackTop.valuePointer = nativeCallInfo->functionBase.valuePointer + 1;
    return 1;
}

static SZrCompilerState *create_compiler_state(void) {
    SZrCompilerState *cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));

    TEST_ASSERT_NOT_NULL(cs);
    memset(cs, 0, sizeof(*cs));
    ZrParser_CompilerState_Init(cs, g_state);
    TEST_ASSERT_NOT_NULL(cs->typeEnv);
    return cs;
}

static void destroy_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

static const char *identifier_text(const SZrIdentifier *identifier) {
    TEST_ASSERT_NOT_NULL(identifier);
    TEST_ASSERT_NOT_NULL(identifier->name);
    return ZrCore_String_GetNativeString(identifier->name);
}

static void assert_named_type(SZrType *type, const char *expectedName) {
    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_NOT_NULL(type->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, type->name->type);
    TEST_ASSERT_EQUAL_STRING(expectedName, identifier_text(&type->name->data.identifier));
}

static SZrAstNode *statement_value(SZrAstNode *script, TZrSize index) {
    SZrAstNode *statement;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_TRUE(index < script->data.script.statements->count);
    statement = script->data.script.statements->nodes[index];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, statement->type);
    TEST_ASSERT_NOT_NULL(statement->data.variableDeclaration.value);
    return statement->data.variableDeclaration.value;
}

static void assert_primary_identifier_variant_value(SZrAstNode *value,
                                                    const char *typeName,
                                                    const char *variantName,
                                                    TZrSize callArgCount) {
    SZrPrimaryExpression *primary;
    SZrAstNode *memberNode;

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, value->type);
    primary = &value->data.primaryExpression;
    TEST_ASSERT_NOT_NULL(primary->property);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, primary->property->type);
    TEST_ASSERT_EQUAL_STRING(typeName, identifier_text(&primary->property->data.identifier));
    TEST_ASSERT_NOT_NULL(primary->members);
    TEST_ASSERT_EQUAL_UINT32(callArgCount == 0u ? 1u : 2u, (TZrUInt32)primary->members->count);

    memberNode = primary->members->nodes[0];
    TEST_ASSERT_NOT_NULL(memberNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, memberNode->type);
    TEST_ASSERT_FALSE(memberNode->data.memberExpression.computed);
    TEST_ASSERT_NOT_NULL(memberNode->data.memberExpression.property);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, memberNode->data.memberExpression.property->type);
    TEST_ASSERT_EQUAL_STRING(variantName, identifier_text(&memberNode->data.memberExpression.property->data.identifier));

    if (callArgCount > 0u) {
        SZrAstNode *callNode = primary->members->nodes[1];
        TEST_ASSERT_NOT_NULL(callNode);
        TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, callNode->type);
        TEST_ASSERT_NOT_NULL(callNode->data.functionCall.args);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)callArgCount, (TZrUInt32)callNode->data.functionCall.args->count);
    }
}

static void assert_primary_generic_variant_value(SZrAstNode *value,
                                                 const char *typeName,
                                                 const char *genericArgName,
                                                 const char *variantName,
                                                 TZrSize callArgCount) {
    SZrPrimaryExpression *primary;
    SZrType *typeInfo;
    SZrGenericType *genericType;
    SZrAstNode *memberNode;

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, value->type);
    primary = &value->data.primaryExpression;
    TEST_ASSERT_NOT_NULL(primary->property);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, primary->property->type);
    typeInfo = &primary->property->data.type;
    TEST_ASSERT_NOT_NULL(typeInfo->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, typeInfo->name->type);
    genericType = &typeInfo->name->data.genericType;
    TEST_ASSERT_NOT_NULL(genericType->name);
    TEST_ASSERT_EQUAL_STRING(typeName, identifier_text(genericType->name));
    TEST_ASSERT_NOT_NULL(genericType->params);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)genericType->params->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, genericType->params->nodes[0]->type);
    assert_named_type(&genericType->params->nodes[0]->data.type, genericArgName);

    TEST_ASSERT_NOT_NULL(primary->members);
    TEST_ASSERT_EQUAL_UINT32(callArgCount == 0u ? 1u : 2u, (TZrUInt32)primary->members->count);
    memberNode = primary->members->nodes[0];
    TEST_ASSERT_NOT_NULL(memberNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, memberNode->type);
    TEST_ASSERT_FALSE(memberNode->data.memberExpression.computed);
    TEST_ASSERT_NOT_NULL(memberNode->data.memberExpression.property);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, memberNode->data.memberExpression.property->type);
    TEST_ASSERT_EQUAL_STRING(variantName, identifier_text(&memberNode->data.memberExpression.property->data.identifier));

    if (callArgCount > 0u) {
        SZrAstNode *callNode = primary->members->nodes[1];
        TEST_ASSERT_NOT_NULL(callNode);
        TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, callNode->type);
        TEST_ASSERT_NOT_NULL(callNode->data.functionCall.args);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)callArgCount, (TZrUInt32)callNode->data.functionCall.args->count);
    }
}

static void assert_primary_identifier_struct_variant_value(SZrAstNode *value,
                                                           const char *typeName,
                                                           const char *variantName,
                                                           TZrSize fieldCount) {
    SZrPrimaryExpression *primary;
    SZrAstNode *memberNode;
    SZrAstNode *objectNode;

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, value->type);
    primary = &value->data.primaryExpression;
    TEST_ASSERT_NOT_NULL(primary->property);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, primary->property->type);
    TEST_ASSERT_EQUAL_STRING(typeName, identifier_text(&primary->property->data.identifier));
    TEST_ASSERT_NOT_NULL(primary->members);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)primary->members->count);

    memberNode = primary->members->nodes[0];
    TEST_ASSERT_NOT_NULL(memberNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, memberNode->type);
    TEST_ASSERT_FALSE(memberNode->data.memberExpression.computed);
    TEST_ASSERT_NOT_NULL(memberNode->data.memberExpression.property);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, memberNode->data.memberExpression.property->type);
    TEST_ASSERT_EQUAL_STRING(variantName, identifier_text(&memberNode->data.memberExpression.property->data.identifier));

    objectNode = primary->members->nodes[1];
    TEST_ASSERT_NOT_NULL(objectNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_OBJECT_LITERAL, objectNode->type);
    TEST_ASSERT_NOT_NULL(objectNode->data.objectLiteral.properties);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)fieldCount, (TZrUInt32)objectNode->data.objectLiteral.properties->count);
}

static void assert_inferred_type_name(SZrCompilerState *cs, SZrAstNode *value, const char *expectedName) {
    SZrInferredType result;

    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(expectedName);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, value, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
    TEST_ASSERT_NOT_NULL(result.typeName);
    TEST_ASSERT_EQUAL_STRING(expectedName, ZrCore_String_GetNativeString(result.typeName));
    ZrParser_InferredType_Free(g_state, &result);
}

static TZrBool compiled_instructions_contain_opcode(SZrCompilerState *cs, EZrInstructionCode opcode) {
    TEST_ASSERT_NOT_NULL(cs);

    for (TZrSize index = 0; index < cs->instructions.length; index++) {
        TZrInstruction *instruction = (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, index);

        if (instruction != ZR_NULL && ZR_INSTRUCTION_OPCODE((*instruction)) == opcode) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_graph_instructions_contain_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TEST_ASSERT_NOT_NULL(function);

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        if (ZR_INSTRUCTION_OPCODE(function->instructionsList[index]) == opcode) {
            return ZR_TRUE;
        }
    }

    for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
        if (function_graph_instructions_contain_opcode(&function->childFunctionList[index], opcode)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool compiled_instructions_contain_set_member_name(SZrCompilerState *cs, const char *memberName) {
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);
    TEST_ASSERT_NOT_NULL(memberName);

    for (TZrSize index = 0; index < cs->instructions.length; index++) {
        TZrInstruction *instruction = (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, index);
        TZrUInt32 memberIndex;
        SZrFunctionMemberEntry *entry;

        if (instruction == ZR_NULL || ZR_INSTRUCTION_OPCODE((*instruction)) != ZR_INSTRUCTION_ENUM(SET_MEMBER)) {
            continue;
        }

        memberIndex = instruction->instruction.operand.operand1[1];
        if (memberIndex >= cs->currentFunction->memberEntryLength) {
            continue;
        }

        entry = &cs->currentFunction->memberEntries[memberIndex];
        if (entry->entryKind == ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL &&
            entry->symbol != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(entry->symbol), memberName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool compiled_instructions_contain_get_member_name(SZrCompilerState *cs, const char *memberName) {
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);
    TEST_ASSERT_NOT_NULL(memberName);

    for (TZrSize index = 0; index < cs->instructions.length; index++) {
        TZrInstruction *instruction = (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, index);
        TZrUInt32 memberIndex;
        SZrFunctionMemberEntry *entry;

        if (instruction == ZR_NULL || ZR_INSTRUCTION_OPCODE((*instruction)) != ZR_INSTRUCTION_ENUM(GET_MEMBER)) {
            continue;
        }

        memberIndex = instruction->instruction.operand.operand1[1];
        if (memberIndex >= cs->currentFunction->memberEntryLength) {
            continue;
        }

        entry = &cs->currentFunction->memberEntries[memberIndex];
        if (entry->entryKind == ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL &&
            entry->symbol != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(entry->symbol), memberName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool compiled_local_variables_contain_name(SZrCompilerState *cs, const char *name) {
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(name);

    for (TZrSize index = 0; index < cs->localVars.length; index++) {
        SZrFunctionLocalVariable *localVar =
                (SZrFunctionLocalVariable *)ZrCore_Array_Get(&cs->localVars, index);

        if (localVar != ZR_NULL &&
            localVar->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(localVar->name), name) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrString *compiled_function_string_constant_at(SZrFunction *function, TZrUInt32 index) {
    SZrTypeValue *value;

    if (function == ZR_NULL || function->constantValueList == ZR_NULL ||
        index >= function->constantValueLength) {
        return ZR_NULL;
    }

    value = &function->constantValueList[index];
    if (value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_STRING(g_state, value->value.object);
}

static SZrObject *compiled_function_object_constant_at(SZrFunction *function,
                                                       TZrUInt32 index,
                                                       EZrValueType expectedType) {
    SZrTypeValue *value;

    if (function == ZR_NULL || function->constantValueList == ZR_NULL ||
        index >= function->constantValueLength) {
        return ZR_NULL;
    }

    value = &function->constantValueList[index];
    if (value->type != expectedType || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(g_state, value->value.object);
}

static const SZrTypeValue *compiled_object_field_value(SZrObject *object, const char *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    TEST_ASSERT_NOT_NULL(object);
    TEST_ASSERT_NOT_NULL(fieldName);

    fieldString = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)fieldName);
    TEST_ASSERT_NOT_NULL(fieldString);
    ZrCore_Value_InitAsRawObject(g_state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(g_state, object, &key);
}

static SZrObject *compiled_array_entry_object(SZrObject *array, TZrSize index) {
    SZrTypeValue key;
    const SZrTypeValue *entryValue;

    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_ARRAY, array->internalType);

    ZrCore_Value_InitAsInt(g_state, &key, (TZrInt64)index);
    entryValue = ZrCore_Object_GetValue(g_state, array, &key);
    TEST_ASSERT_NOT_NULL(entryValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, entryValue->type);
    TEST_ASSERT_NOT_NULL(entryValue->value.object);
    return ZR_CAST_OBJECT(g_state, entryValue->value.object);
}

static SZrObject *compiled_object_array_field(SZrObject *object, const char *fieldName) {
    const SZrTypeValue *fieldValue = compiled_object_field_value(object, fieldName);

    TEST_ASSERT_NOT_NULL(fieldValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, fieldValue->type);
    TEST_ASSERT_NOT_NULL(fieldValue->value.object);
    return ZR_CAST_OBJECT(g_state, fieldValue->value.object);
}

static void assert_compiled_object_string_field(SZrObject *object,
                                                const char *fieldName,
                                                const char *expectedValue) {
    const SZrTypeValue *fieldValue = compiled_object_field_value(object, fieldName);

    TEST_ASSERT_NOT_NULL(fieldValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, fieldValue->type);
    TEST_ASSERT_NOT_NULL(fieldValue->value.object);
    TEST_ASSERT_EQUAL_STRING(expectedValue,
                             ZrCore_String_GetNativeString(ZR_CAST_STRING(g_state, fieldValue->value.object)));
}

static void assert_compiled_object_int_field(SZrObject *object,
                                             const char *fieldName,
                                             TZrInt64 expectedValue) {
    const SZrTypeValue *fieldValue = compiled_object_field_value(object, fieldName);

    TEST_ASSERT_NOT_NULL(fieldValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(fieldValue->type));
    TEST_ASSERT_EQUAL_INT64(expectedValue, fieldValue->value.nativeObject.nativeInt64);
}

static void assert_union_payload_field_metadata(SZrObject *fieldObject,
                                                TZrInt64 index,
                                                const char *fieldName,
                                                const char *storageName,
                                                const char *typeName) {
    assert_compiled_object_int_field(fieldObject, "index", index);
    assert_compiled_object_string_field(fieldObject, "name", fieldName);
    assert_compiled_object_string_field(fieldObject, "storageName", storageName);
    assert_compiled_object_string_field(fieldObject, "type", typeName);
}

static const SZrUnionCompiledPrototypeInfoView *find_compiled_prototype_by_name(SZrFunction *function,
                                                                                const char *prototypeName) {
    const TZrByte *current;
    TZrSize remaining;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(prototypeName);

    if (function->prototypeData == ZR_NULL ||
        function->prototypeDataLength <= sizeof(TZrUInt32) ||
        function->prototypeCount == 0) {
        return ZR_NULL;
    }

    current = function->prototypeData + sizeof(TZrUInt32);
    remaining = function->prototypeDataLength - sizeof(TZrUInt32);
    while (remaining >= sizeof(SZrUnionCompiledPrototypeInfoView)) {
        const SZrUnionCompiledPrototypeInfoView *prototypeInfo =
                (const SZrUnionCompiledPrototypeInfoView *)current;
        TZrSize prototypeSize =
                sizeof(SZrUnionCompiledPrototypeInfoView) +
                prototypeInfo->inheritsCount * sizeof(TZrUInt32) +
                prototypeInfo->decoratorsCount * sizeof(TZrUInt32) +
                prototypeInfo->membersCount * sizeof(SZrUnionCompiledMemberInfoView);
        SZrString *actualName;

        if (remaining < prototypeSize) {
            break;
        }

        actualName = compiled_function_string_constant_at(function, prototypeInfo->nameStringIndex);
        if (actualName != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(actualName), prototypeName) == 0) {
            return prototypeInfo;
        }

        current += prototypeSize;
        remaining -= prototypeSize;
    }

    return ZR_NULL;
}

static const SZrUnionCompiledMemberInfoView *find_compiled_member_by_name(
        SZrFunction *function,
        const SZrUnionCompiledPrototypeInfoView *prototypeInfo,
        const char *memberName) {
    const SZrUnionCompiledMemberInfoView *members;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(prototypeInfo);
    TEST_ASSERT_NOT_NULL(memberName);

    members = (const SZrUnionCompiledMemberInfoView *)((const TZrByte *)prototypeInfo +
                                                       sizeof(SZrUnionCompiledPrototypeInfoView) +
                                                       prototypeInfo->inheritsCount * sizeof(TZrUInt32) +
                                                       prototypeInfo->decoratorsCount * sizeof(TZrUInt32));
    for (TZrUInt32 index = 0; index < prototypeInfo->membersCount; index++) {
        SZrString *actualName = compiled_function_string_constant_at(function, members[index].nameStringIndex);
        if (actualName != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(actualName), memberName) == 0) {
            return &members[index];
        }
    }

    return ZR_NULL;
}

static void test_union_declaration_serializes_prototype_and_variant_metadata(void) {
    const char *source =
            "pub union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "    Rect { width: float; height: float; }\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrFunction *function;
    const SZrUnionCompiledPrototypeInfoView *shapePrototype;
    const SZrUnionCompiledMemberInfoView *emptyVariant;
    const SZrUnionCompiledMemberInfoView *circleVariant;
    const SZrUnionCompiledMemberInfoView *rectVariant;

    TEST_ASSERT_NOT_NULL(ast);
    function = ZrParser_Compiler_Compile(g_state, ast);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->prototypeData);
    TEST_ASSERT_EQUAL_UINT32(1u, function->prototypeCount);

    shapePrototype = find_compiled_prototype_by_name(function, "Shape");
    TEST_ASSERT_NOT_NULL(shapePrototype);
    TEST_ASSERT_EQUAL_UINT32(ZR_OBJECT_PROTOTYPE_TYPE_UNION, shapePrototype->type);
    TEST_ASSERT_EQUAL_UINT32(ZR_ACCESS_PUBLIC, shapePrototype->accessModifier);
    TEST_ASSERT_EQUAL_UINT32(3u, shapePrototype->membersCount);
    TEST_ASSERT_EQUAL_UINT32(12u, shapePrototype->layoutByteSize);
    TEST_ASSERT_EQUAL_UINT32(4u, shapePrototype->layoutByteAlign);

    emptyVariant = find_compiled_member_by_name(function, shapePrototype, "Empty");
    circleVariant = find_compiled_member_by_name(function, shapePrototype, "Circle");
    rectVariant = find_compiled_member_by_name(function, shapePrototype, "Rect");
    TEST_ASSERT_NOT_NULL(emptyVariant);
    TEST_ASSERT_NOT_NULL(circleVariant);
    TEST_ASSERT_NOT_NULL(rectVariant);
    TEST_ASSERT_EQUAL_UINT32(ZR_AST_UNION_VARIANT, emptyVariant->memberType);
    TEST_ASSERT_EQUAL_UINT32(ZR_AST_UNION_VARIANT, circleVariant->memberType);
    TEST_ASSERT_EQUAL_UINT32(ZR_AST_UNION_VARIANT, rectVariant->memberType);
    TEST_ASSERT_EQUAL_UINT32(0u, emptyVariant->declarationOrder);
    TEST_ASSERT_EQUAL_UINT32(1u, circleVariant->declarationOrder);
    TEST_ASSERT_EQUAL_UINT32(2u, rectVariant->declarationOrder);
    TEST_ASSERT_EQUAL_UINT32(0u, emptyVariant->parameterCount);
    TEST_ASSERT_EQUAL_UINT32(1u, circleVariant->parameterCount);
    TEST_ASSERT_EQUAL_UINT32(2u, rectVariant->parameterCount);

    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_variant_metadata_serializes_payload_field_names_and_types(void) {
    const char *source =
            "pub union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "    Rect { width: float; height: float; }\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrFunction *function;
    const SZrUnionCompiledPrototypeInfoView *shapePrototype;
    const SZrUnionCompiledMemberInfoView *circleVariant;
    const SZrUnionCompiledMemberInfoView *rectVariant;
    SZrObject *circleMetadata;
    SZrObject *rectMetadata;
    SZrObject *circleFields;
    SZrObject *rectFields;

    TEST_ASSERT_NOT_NULL(ast);
    function = ZrParser_Compiler_Compile(g_state, ast);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->prototypeData);

    shapePrototype = find_compiled_prototype_by_name(function, "Shape");
    TEST_ASSERT_NOT_NULL(shapePrototype);
    circleVariant = find_compiled_member_by_name(function, shapePrototype, "Circle");
    rectVariant = find_compiled_member_by_name(function, shapePrototype, "Rect");
    TEST_ASSERT_NOT_NULL(circleVariant);
    TEST_ASSERT_NOT_NULL(rectVariant);

    TEST_ASSERT_NOT_EQUAL_UINT32(0u, circleVariant->returnTypeNameStringIndex);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, rectVariant->returnTypeNameStringIndex);
    TEST_ASSERT_EQUAL_STRING("Shape",
                             ZrCore_String_GetNativeString(compiled_function_string_constant_at(function,
                                                                                                circleVariant->returnTypeNameStringIndex)));
    TEST_ASSERT_EQUAL_STRING("Shape",
                             ZrCore_String_GetNativeString(compiled_function_string_constant_at(function,
                                                                                                rectVariant->returnTypeNameStringIndex)));

    TEST_ASSERT_TRUE(circleVariant->hasDecoratorMetadata);
    TEST_ASSERT_TRUE(rectVariant->hasDecoratorMetadata);
    circleMetadata = compiled_function_object_constant_at(function,
                                                          circleVariant->decoratorMetadataConstantIndex,
                                                          ZR_VALUE_TYPE_OBJECT);
    rectMetadata = compiled_function_object_constant_at(function,
                                                        rectVariant->decoratorMetadataConstantIndex,
                                                        ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(circleMetadata);
    TEST_ASSERT_NOT_NULL(rectMetadata);

    assert_compiled_object_string_field(circleMetadata, "kind", "unionVariant");
    assert_compiled_object_string_field(circleMetadata, "ownerType", "Shape");
    assert_compiled_object_string_field(circleMetadata, "variantName", "Circle");
    assert_compiled_object_int_field(circleMetadata, "tag", 1);
    assert_compiled_object_int_field(circleMetadata, "variantKind", ZR_UNION_VARIANT_TUPLE);
    assert_compiled_object_int_field(circleMetadata, "payloadFieldCount", 1);
    circleFields = compiled_object_array_field(circleMetadata, "payloadFields");
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)circleFields->nodeMap.elementCount);
    assert_union_payload_field_metadata(compiled_array_entry_object(circleFields, 0u),
                                        0,
                                        "radius",
                                        "__zr_unionPayload0",
                                        "float");

    assert_compiled_object_string_field(rectMetadata, "kind", "unionVariant");
    assert_compiled_object_string_field(rectMetadata, "ownerType", "Shape");
    assert_compiled_object_string_field(rectMetadata, "variantName", "Rect");
    assert_compiled_object_int_field(rectMetadata, "tag", 2);
    assert_compiled_object_int_field(rectMetadata, "variantKind", ZR_UNION_VARIANT_STRUCT);
    assert_compiled_object_int_field(rectMetadata, "payloadFieldCount", 2);
    rectFields = compiled_object_array_field(rectMetadata, "payloadFields");
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)rectFields->nodeMap.elementCount);
    assert_union_payload_field_metadata(compiled_array_entry_object(rectFields, 0u),
                                        0,
                                        "width",
                                        "__zr_unionPayload0",
                                        "float");
    assert_union_payload_field_metadata(compiled_array_entry_object(rectFields, 1u),
                                        1,
                                        "height",
                                        "__zr_unionPayload1",
                                        "float");

    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_variant_metadata_serializes_byte_layout(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "    Rect { width: float; height: float; }\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrFunction *function;
    const SZrUnionCompiledPrototypeInfoView *shapePrototype;
    const SZrUnionCompiledMemberInfoView *circleVariant;
    const SZrUnionCompiledMemberInfoView *rectVariant;
    SZrObject *circleMetadata;
    SZrObject *rectMetadata;
    SZrObject *circleFields;
    SZrObject *rectFields;

    TEST_ASSERT_NOT_NULL(ast);
    function = ZrParser_Compiler_Compile(g_state, ast);
    TEST_ASSERT_NOT_NULL(function);
    shapePrototype = find_compiled_prototype_by_name(function, "Shape");
    TEST_ASSERT_NOT_NULL(shapePrototype);
    TEST_ASSERT_EQUAL_UINT32(12u, shapePrototype->layoutByteSize);
    TEST_ASSERT_EQUAL_UINT32(4u, shapePrototype->layoutByteAlign);

    circleVariant = find_compiled_member_by_name(function, shapePrototype, "Circle");
    rectVariant = find_compiled_member_by_name(function, shapePrototype, "Rect");
    TEST_ASSERT_NOT_NULL(circleVariant);
    TEST_ASSERT_NOT_NULL(rectVariant);
    circleMetadata = compiled_function_object_constant_at(function,
                                                          circleVariant->decoratorMetadataConstantIndex,
                                                          ZR_VALUE_TYPE_OBJECT);
    rectMetadata = compiled_function_object_constant_at(function,
                                                        rectVariant->decoratorMetadataConstantIndex,
                                                        ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(circleMetadata);
    TEST_ASSERT_NOT_NULL(rectMetadata);

    assert_compiled_object_int_field(circleMetadata, "tagSize", 1);
    assert_compiled_object_int_field(circleMetadata, "payloadOffset", 4);
    assert_compiled_object_int_field(circleMetadata, "variantPayloadSize", 4);
    assert_compiled_object_int_field(circleMetadata, "variantPayloadAlign", 4);
    circleFields = compiled_object_array_field(circleMetadata, "payloadFields");
    assert_compiled_object_int_field(compiled_array_entry_object(circleFields, 0u), "byteOffset", 4);
    assert_compiled_object_int_field(compiled_array_entry_object(circleFields, 0u), "byteSize", 4);
    assert_compiled_object_int_field(compiled_array_entry_object(circleFields, 0u), "byteAlign", 4);

    assert_compiled_object_int_field(rectMetadata, "tagSize", 1);
    assert_compiled_object_int_field(rectMetadata, "payloadOffset", 4);
    assert_compiled_object_int_field(rectMetadata, "variantPayloadSize", 8);
    assert_compiled_object_int_field(rectMetadata, "variantPayloadAlign", 4);
    rectFields = compiled_object_array_field(rectMetadata, "payloadFields");
    assert_compiled_object_int_field(compiled_array_entry_object(rectFields, 0u), "byteOffset", 4);
    assert_compiled_object_int_field(compiled_array_entry_object(rectFields, 0u), "byteSize", 4);
    assert_compiled_object_int_field(compiled_array_entry_object(rectFields, 0u), "byteAlign", 4);
    assert_compiled_object_int_field(compiled_array_entry_object(rectFields, 1u), "byteOffset", 8);
    assert_compiled_object_int_field(compiled_array_entry_object(rectFields, 1u), "byteSize", 4);
    assert_compiled_object_int_field(compiled_array_entry_object(rectFields, 1u), "byteAlign", 4);

    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_owner_payload_metadata_marks_value_slot_ownership(void) {
    const char *source =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open(handle: Shared<Box>);\n"
            "}\n"
            "var resource: Resource = Resource.Empty;\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrFunction *function;
    const SZrUnionCompiledPrototypeInfoView *resourcePrototype;
    const SZrUnionCompiledMemberInfoView *openVariant;
    SZrObject *openMetadata;
    SZrObject *openFields;
    SZrObject *handleField;
    const SZrFunctionFrameSlotLayout *resourceLayout;
    const SZrTypeLayout *typeLayout;

    TEST_ASSERT_NOT_NULL(ast);
    function = ZrParser_Compiler_Compile(g_state, ast);
    TEST_ASSERT_NOT_NULL(function);

    resourcePrototype = find_compiled_prototype_by_name(function, "Resource");
    TEST_ASSERT_NOT_NULL(resourcePrototype);
    openVariant = find_compiled_member_by_name(function, resourcePrototype, "Open");
    TEST_ASSERT_NOT_NULL(openVariant);
    TEST_ASSERT_TRUE(openVariant->hasDecoratorMetadata);

    openMetadata = compiled_function_object_constant_at(function,
                                                        openVariant->decoratorMetadataConstantIndex,
                                                        ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(openMetadata);
    openFields = compiled_object_array_field(openMetadata, "payloadFields");
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)openFields->nodeMap.elementCount);
    handleField = compiled_array_entry_object(openFields, 0u);

    assert_union_payload_field_metadata(handleField,
                                        0,
                                        "handle",
                                        "__zr_unionPayload0",
                                        "Box");
    assert_compiled_object_int_field(handleField, "byteSize", (TZrInt64)sizeof(SZrTypeValue));
    assert_compiled_object_int_field(handleField, "byteAlign", (TZrInt64)ZR_ALIGN_SIZE);
    assert_compiled_object_int_field(handleField,
                                     "ownershipQualifier",
                                     ZR_OWNERSHIP_QUALIFIER_SHARED);

    resourceLayout = ZrCore_Function_FindFrameSlotLayout(function, 0u);
    TEST_ASSERT_NOT_NULL(resourceLayout);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT, resourceLayout->slotKind);
    TEST_ASSERT_EQUAL_UINT32(resourcePrototype->layoutByteSize, resourceLayout->byteSize);
    TEST_ASSERT_EQUAL_UINT32(resourcePrototype->layoutByteAlign, resourceLayout->byteAlign);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, resourceLayout->typeLayoutId);

    typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, resourceLayout->typeLayoutId, g_state);
    TEST_ASSERT_NOT_NULL(typeLayout);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_KIND_UNION, typeLayout->kind);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY, typeLayout->copyKind);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP, typeLayout->dropKind);
    TEST_ASSERT_EQUAL_UINT32(1u, typeLayout->fieldCount);
    TEST_ASSERT_EQUAL_UINT32(1u, typeLayout->gcFieldCount);
    TEST_ASSERT_EQUAL_UINT32(1u, typeLayout->ownershipFieldCount);
    TEST_ASSERT_NOT_NULL(typeLayout->fields);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(SZrTypeValue), typeLayout->fields[0].byteSize);
    TEST_ASSERT_TRUE((typeLayout->fields[0].flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) != 0u);
    TEST_ASSERT_TRUE((typeLayout->fields[0].flags & ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE) != 0u);
    TEST_ASSERT_TRUE((typeLayout->fields[0].flags & ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE) != 0u);
    TEST_ASSERT_EQUAL_UINT32(openVariant->fieldOffset, typeLayout->fields[0].activeTag);

    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_typed_local_uses_inline_frame_layout(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var shape: Shape = Shape.Circle(1.0);\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrFunction *function;
    const SZrFunctionFrameSlotLayout *shapeLayout;
    const SZrTypeLayout *typeLayout;

    TEST_ASSERT_NOT_NULL(ast);
    function = ZrParser_Compiler_Compile(g_state, ast);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1u, function->frameSlotLayoutLength);

    shapeLayout = ZrCore_Function_FindFrameSlotLayout(function, 0u);
    TEST_ASSERT_NOT_NULL(shapeLayout);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT, shapeLayout->slotKind);
    TEST_ASSERT_EQUAL_UINT32(12u, shapeLayout->byteSize);
    TEST_ASSERT_EQUAL_UINT32(4u, shapeLayout->byteAlign);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, shapeLayout->typeLayoutId);

    typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, shapeLayout->typeLayoutId, g_state);
    TEST_ASSERT_NOT_NULL(typeLayout);
    TEST_ASSERT_EQUAL_UINT32(12u, typeLayout->byteSize);
    TEST_ASSERT_EQUAL_UINT32(4u, typeLayout->byteAlign);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_KIND_UNION, typeLayout->kind);

    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_typed_local_in_child_function_uses_inline_frame_layout(void) {
    const char *source =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open(handle: Shared<Box>);\n"
            "}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var resource: Resource = Resource.Open(shared);\n"
            "    return 0;\n"
            "}\n";
    SZrString *sourceName;
    SZrFunction *function;
    SZrFunction *keepFunction;
    const SZrFunctionFrameSlotLayout *resourceLayout;
    const SZrTypeLayout *typeLayout;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_child_inline_layout.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1u, function->childFunctionLength);

    keepFunction = &function->childFunctionList[0];
    TEST_ASSERT_NOT_NULL(keepFunction);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, keepFunction->frameSlotLayoutLength);

    resourceLayout = ZrCore_Function_FindFrameSlotLayout(keepFunction, 1u);
    TEST_ASSERT_NOT_NULL(resourceLayout);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT, resourceLayout->slotKind);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, resourceLayout->typeLayoutId);

    typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(keepFunction, resourceLayout->typeLayoutId, g_state);
    TEST_ASSERT_NOT_NULL(typeLayout);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_KIND_UNION, typeLayout->kind);
    TEST_ASSERT_EQUAL_UINT32(1u, typeLayout->ownershipFieldCount);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_constructor_materializes_inline_tag_and_payload_bytes(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var shape: Shape = Shape.Circle(2.5);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_inline_union_shape_frame_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_inline_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_switch_reads_inline_tag_and_payload_from_typed_local(void) {
    const char *source =
            "union Choice {\n"
            "    Empty;\n"
            "    Num(value: int);\n"
            "}\n"
            "var choice: Choice = Choice.Num(42);\n"
            "switch (choice) {\n"
            "    (Choice.Empty) { return 0; }\n"
            "    (Choice.Num(v)) { return v; }\n"
            "}\n"
            "return -1;\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_inline_switch_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_using_guard_reads_inline_tag_and_payload_from_typed_local(void) {
    const char *source =
            "union Choice {\n"
            "    Empty;\n"
            "    Num(value: int);\n"
            "}\n"
            "var choice: Choice = Choice.Num(7);\n"
            "using (var [v]: Choice.Num = choice) {\n"
            "    return v;\n"
            "} else {\n"
            "    return -1;\n"
            "}\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_inline_using_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_using_guard_rejects_legacy_variant_call_binder(void) {
    const char *source =
            "union Choice {\n"
            "    Empty;\n"
            "    Num(value: int);\n"
            "}\n"
            "var choice: Choice = Choice.Num(7);\n"
            "using (var Num(v) = choice) {\n"
            "    var hit = v;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    usingStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(usingStatement);

    ZrParser_Statement_Compile(cs, varStatement);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_TRUE(cs->hasError);

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_using_guard_matches_unit_variant_with_empty_tuple_destructuring(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: int);\n"
            "}\n"
            "var shape: Shape = Shape.Empty;\n"
            "using (var []: Shape.Empty = shape) {\n"
            "    return 11;\n"
            "} else {\n"
            "    return -1;\n"
            "}\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_using_unit_variant_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(11, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_variable_initialization_copies_inline_tag_and_payload_between_typed_locals(void) {
    const char *source =
            "union Choice {\n"
            "    Empty;\n"
            "    Num(value: int);\n"
            "}\n"
            "var source: Choice = Choice.Num(13);\n"
            "var target: Choice = source;\n"
            "switch (target) {\n"
            "    (Choice.Empty) { return 0; }\n"
            "    (Choice.Num(v)) { return v; }\n"
            "}\n"
            "return -1;\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_inline_var_copy_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(13, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_assignment_copies_inline_tag_and_payload_between_typed_locals(void) {
    const char *source =
            "union Choice {\n"
            "    Empty;\n"
            "    Num(value: int);\n"
            "}\n"
            "var source: Choice = Choice.Num(12);\n"
            "var target: Choice = Choice.Empty;\n"
            "target = source;\n"
            "switch (target) {\n"
            "    (Choice.Empty) { return 0; }\n"
            "    (Choice.Num(v)) { return v; }\n"
            "}\n"
            "return -1;\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_inline_assignment_copy_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(12, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_assignment_materializes_constructor_into_existing_typed_local(void) {
    const char *source =
            "union Choice {\n"
            "    Empty;\n"
            "    Num(value: int);\n"
            "}\n"
            "var target: Choice = Choice.Empty;\n"
            "target = Choice.Num(27);\n"
            "switch (target) {\n"
            "    (Choice.Empty) { return 0; }\n"
            "    (Choice.Num(v)) { return v; }\n"
            "}\n"
            "return -1;\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_inline_assignment_constructor_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(27, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_struct_field_assignment_materializes_constructor_payload(void) {
    const char *source =
            "union Choice {\n"
            "    Empty;\n"
            "    Num(value: int);\n"
            "}\n"
            "struct Holder {\n"
            "    pub var choice: Choice;\n"
            "}\n"
            "var holder: Holder = $Holder();\n"
            "holder.choice = Choice.Num(31);\n"
            "switch (holder.choice) {\n"
            "    (Choice.Empty) { return 0; }\n"
            "    (Choice.Num(v)) { return v; }\n"
            "}\n"
            "return -1;\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_struct_field_constructor_assignment_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(31, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_nested_struct_field_assignment_materializes_constructor_payload(void) {
    const char *source =
            "union Choice {\n"
            "    Empty;\n"
            "    Num(value: int);\n"
            "}\n"
            "struct Inner {\n"
            "    pub var choice: Choice;\n"
            "}\n"
            "struct Holder {\n"
            "    pub var inner: Inner;\n"
            "}\n"
            "var holder: Holder = $Holder();\n"
            "holder.inner.choice = Choice.Num(41);\n"
            "switch (holder.inner.choice) {\n"
            "    (Choice.Empty) { return 0; }\n"
            "    (Choice.Num(v)) { return v; }\n"
            "}\n"
            "return -1;\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_nested_struct_field_constructor_assignment_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(41, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_nested_struct_field_using_guard_reads_constructor_payload(void) {
    const char *source =
            "union Choice {\n"
            "    Empty;\n"
            "    Num(value: int);\n"
            "}\n"
            "struct Inner {\n"
            "    pub var choice: Choice;\n"
            "}\n"
            "struct Holder {\n"
            "    pub var inner: Inner;\n"
            "}\n"
            "var holder: Holder = $Holder();\n"
            "holder.inner.choice = Choice.Num(43);\n"
            "using (var [v]: Choice.Num = holder.inner.choice) {\n"
            "    return v;\n"
            "} else {\n"
            "    return -1;\n"
            "}\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_nested_struct_field_using_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(43, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_owner_payload_control_shared_parameter_releases_call_window_owner(void) {
    const char *source =
            "class Box {}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var releasedInner = %release(shared);\n"
            "    return 0;\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var keepResult = keep(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = -1;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_union_weak_alive_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "shared_parameter_call_window_owner_drop_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_owner_payload_releases_active_variant_on_inline_frame_drop(void) {
    const char *insideSource =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open(handle: Shared<Box>);\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var resource: Resource = Resource.Open(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    const char *afterSource =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open(handle: Shared<Box>);\n"
            "}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var resource: Resource = Resource.Open(shared);\n"
            "    var releasedInner = %release(shared);\n"
            "    return 0;\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var keepResult = keep(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_union_weak_alive_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_owner_payload_live_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, insideSource, strlen(insideSource), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrCore_Function_Free(g_state, function);

    result = -1;
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_owner_payload_drop_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, afterSource, strlen(afterSource), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_constructor_assignment_drops_replaced_owner_payload(void) {
    const char *source =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open(handle: Shared<Box>);\n"
            "}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var resource: Resource = Resource.Open(shared);\n"
            "    resource = Resource.Empty;\n"
            "    var releasedInner = %release(shared);\n"
            "    return 0;\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var keepResult = keep(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = -1;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_union_weak_alive_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_constructor_assignment_drop_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_struct_field_assignment_drops_replaced_owner_payload(void) {
    const char *source =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open(handle: Shared<Box>);\n"
            "}\n"
            "struct Holder {\n"
            "    pub var resource: Resource;\n"
            "}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var holder: Holder = $Holder();\n"
            "    holder.resource = Resource.Open(shared);\n"
            "    holder.resource = Resource.Empty;\n"
            "    var releasedInner = %release(shared);\n"
            "    return 0;\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var keepResult = keep(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = -1;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_union_weak_alive_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_struct_field_owner_payload_replace_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_nested_struct_field_assignment_drops_replaced_owner_payload(void) {
    const char *source =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open(handle: Shared<Box>);\n"
            "}\n"
            "struct Inner {\n"
            "    pub var resource: Resource;\n"
            "}\n"
            "struct Holder {\n"
            "    pub var inner: Inner;\n"
            "}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var holder: Holder = $Holder();\n"
            "    holder.inner.resource = Resource.Open(shared);\n"
            "    holder.inner.resource = Resource.Empty;\n"
            "    var releasedInner = %release(shared);\n"
            "    return 0;\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var keepResult = keep(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = -1;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_union_weak_alive_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_nested_struct_field_owner_payload_replace_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_struct_field_assignment_copies_owner_payload_from_typed_local(void) {
    const char *source =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open(handle: Shared<Box>);\n"
            "}\n"
            "struct Holder {\n"
            "    pub var resource: Resource;\n"
            "}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var resource: Resource = Resource.Open(shared);\n"
            "    var holder: Holder = $Holder();\n"
            "    holder.resource = resource;\n"
            "    resource = Resource.Empty;\n"
            "    using (var [handle]: Resource.Open = holder.resource) {\n"
            "        var observedHandle = handle;\n"
            "    } else {\n"
            "        return -1;\n"
            "    }\n"
            "    var releasedInner = %release(shared);\n"
            "    return 0;\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var keepResult = keep(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = -1;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_union_weak_alive_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_struct_field_owner_payload_copy_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_typed_local_initialization_copies_owner_payload_from_struct_field(void) {
    const char *source =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open(handle: Shared<Box>);\n"
            "}\n"
            "struct Holder {\n"
            "    pub var resource: Resource;\n"
            "}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var holder: Holder = $Holder();\n"
            "    holder.resource = Resource.Open(shared);\n"
            "    var copied: Resource = holder.resource;\n"
            "    holder.resource = Resource.Empty;\n"
            "    using (var [handle]: Resource.Open = copied) {\n"
            "        var observedHandle = handle;\n"
            "    } else {\n"
            "        return -1;\n"
            "    }\n"
            "    var releasedInner = %release(shared);\n"
            "    return 0;\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var keepResult = keep(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = -1;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_union_weak_alive_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_local_owner_payload_copy_from_struct_field_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_using_guard_owner_payload_binding_borrows_by_default(void) {
    const char *source =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open(handle: Shared<Box>);\n"
            "}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var resource: Resource = Resource.Open(shared);\n"
            "    using (var [handle]: Resource.Open = resource) {\n"
            "        var observedHandle = handle;\n"
            "    } else {\n"
            "        return -1;\n"
            "    }\n"
            "    var releasedInner = %release(shared);\n"
            "    return 0;\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var keepResult = keep(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = -1;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_union_weak_alive_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_using_owner_payload_binding_borrow_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_graph_instructions_contain_opcode(function, ZR_INSTRUCTION_ENUM(OWN_BORROW)));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_using_guard_owner_payload_move_binding_allows_release(void) {
    const char *source =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open(handle: Shared<Box>);\n"
            "}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var resource: Resource = Resource.Open(shared);\n"
            "    using (var [move handle]: Resource.Open = resource) {\n"
            "        var releasedHandle = %release(handle);\n"
            "    } else {\n"
            "        return -1;\n"
            "    }\n"
            "    var releasedInner = %release(shared);\n"
            "    return 0;\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var keepResult = keep(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = -1;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_union_weak_alive_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_using_owner_payload_move_binding_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(function_graph_instructions_contain_opcode(function, ZR_INSTRUCTION_ENUM(OWN_BORROW)));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_using_guard_struct_owner_payload_move_binding_allows_release(void) {
    const char *source =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open { handle: Shared<Box>; }\n"
            "}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var resource: Resource = Resource.Open { handle: shared };\n"
            "    using (var {move handle}: Resource.Open = resource) {\n"
            "        var releasedHandle = %release(handle);\n"
            "    } else {\n"
            "        return -1;\n"
            "    }\n"
            "    var releasedInner = %release(shared);\n"
            "    return 0;\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var keepResult = keep(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = -1;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_union_weak_alive_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_using_struct_owner_payload_move_binding_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(function_graph_instructions_contain_opcode(function, ZR_INSTRUCTION_ENUM(OWN_BORROW)));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_switch_owner_payload_move_binding_allows_release(void) {
    const char *source =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open(handle: Shared<Box>);\n"
            "}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var resource: Resource = Resource.Open(shared);\n"
            "    switch (resource) {\n"
            "        (Open(move handle)) {\n"
            "            var releasedHandle = %release(handle);\n"
            "        }\n"
            "        (Empty) {\n"
            "            return -1;\n"
            "        }\n"
            "    }\n"
            "    var releasedInner = %release(shared);\n"
            "    return 0;\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var keepResult = keep(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = -1;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_union_weak_alive_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_switch_owner_payload_move_binding_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(function_graph_instructions_contain_opcode(function, ZR_INSTRUCTION_ENUM(OWN_BORROW)));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_switch_struct_owner_payload_move_binding_allows_release(void) {
    const char *source =
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open { handle: Shared<Box>; }\n"
            "}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var resource: Resource = Resource.Open { handle: shared };\n"
            "    switch (resource) {\n"
            "        (Open { handle: move h }) {\n"
            "            var releasedHandle = %release(h);\n"
            "        }\n"
            "        (Empty) {\n"
            "            return -1;\n"
            "        }\n"
            "    }\n"
            "    var releasedInner = %release(shared);\n"
            "    return 0;\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var keepResult = keep(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = -1;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_union_weak_alive_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_switch_struct_owner_payload_move_binding_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(function_graph_instructions_contain_opcode(function, ZR_INSTRUCTION_ENUM(OWN_BORROW)));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_multiple_owner_payloads_release_all_active_variant_fields(void) {
    const char *source =
            "class Box {}\n"
            "union Resource {\n"
            "    Open(a0: Shared<Box>, a1: Shared<Box>, a2: Shared<Box>, a3: Shared<Box>,\n"
            "         a4: Shared<Box>, a5: Shared<Box>, a6: Shared<Box>, a7: Shared<Box>,\n"
            "         a8: Shared<Box>, a9: Shared<Box>, a10: Shared<Box>, a11: Shared<Box>,\n"
            "         a12: Shared<Box>, a13: Shared<Box>, a14: Shared<Box>, a15: Shared<Box>);\n"
            "}\n"
            "func keep(shared: Shared<Box>): int {\n"
            "    var resource: Resource = Resource.Open(shared, shared, shared, shared,\n"
            "                                         shared, shared, shared, shared,\n"
            "                                         shared, shared, shared, shared,\n"
            "                                         shared, shared, shared, shared);\n"
            "    var releasedInner = %release(shared);\n"
            "    return 0;\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var shared = Shared<Box>(seed);\n"
            "var watcher = Weak<Box>(shared);\n"
            "var keepResult = keep(shared);\n"
            "var releasedShared = %release(shared);\n"
            "return zr.__probeInlineUnionShapeFrame();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = -1;

    install_union_native_probe("__probeInlineUnionShapeFrame", probe_union_weak_alive_native);
    sourceName = ZrCore_String_CreateFromNative(g_state, "union_multi_owner_payload_drop_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_declaration_parses_rust_style_variants(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "    Rect { width: float; height: float; }\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrAstNode *decl;
    SZrAstNode *variant;
    SZrParameter *field;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)ast->data.script.statements->count);

    decl = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(decl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNION_DECLARATION, decl->type);
    TEST_ASSERT_EQUAL_STRING("Shape", identifier_text(decl->data.unionDeclaration.name));
    TEST_ASSERT_NULL(decl->data.unionDeclaration.generic);
    TEST_ASSERT_NOT_NULL(decl->data.unionDeclaration.variants);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)decl->data.unionDeclaration.variants->count);

    variant = decl->data.unionDeclaration.variants->nodes[0];
    TEST_ASSERT_NOT_NULL(variant);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNION_VARIANT, variant->type);
    TEST_ASSERT_EQUAL_STRING("Empty", identifier_text(variant->data.unionVariant.name));
    TEST_ASSERT_EQUAL_INT(ZR_UNION_VARIANT_UNIT, variant->data.unionVariant.kind);
    TEST_ASSERT_NULL(variant->data.unionVariant.fields);

    variant = decl->data.unionDeclaration.variants->nodes[1];
    TEST_ASSERT_NOT_NULL(variant);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNION_VARIANT, variant->type);
    TEST_ASSERT_EQUAL_STRING("Circle", identifier_text(variant->data.unionVariant.name));
    TEST_ASSERT_EQUAL_INT(ZR_UNION_VARIANT_TUPLE, variant->data.unionVariant.kind);
    TEST_ASSERT_NOT_NULL(variant->data.unionVariant.fields);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)variant->data.unionVariant.fields->count);
    field = &variant->data.unionVariant.fields->nodes[0]->data.parameter;
    TEST_ASSERT_EQUAL_STRING("radius", identifier_text(field->name));
    assert_named_type(field->typeInfo, "float");

    variant = decl->data.unionDeclaration.variants->nodes[2];
    TEST_ASSERT_NOT_NULL(variant);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNION_VARIANT, variant->type);
    TEST_ASSERT_EQUAL_STRING("Rect", identifier_text(variant->data.unionVariant.name));
    TEST_ASSERT_EQUAL_INT(ZR_UNION_VARIANT_STRUCT, variant->data.unionVariant.kind);
    TEST_ASSERT_NOT_NULL(variant->data.unionVariant.fields);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)variant->data.unionVariant.fields->count);
    field = &variant->data.unionVariant.fields->nodes[0]->data.parameter;
    TEST_ASSERT_EQUAL_STRING("width", identifier_text(field->name));
    assert_named_type(field->typeInfo, "float");
    field = &variant->data.unionVariant.fields->nodes[1]->data.parameter;
    TEST_ASSERT_EQUAL_STRING("height", identifier_text(field->name));
    assert_named_type(field->typeInfo, "float");

    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_declaration_parses_generic_option_and_result(void) {
    const char *source =
            "pub union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n"
            "union Result<T, E> {\n"
            "    Ok(value: T);\n"
            "    Err(error: E);\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrAstNode *optionDecl;
    SZrAstNode *resultDecl;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)ast->data.script.statements->count);

    optionDecl = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(optionDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNION_DECLARATION, optionDecl->type);
    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PUBLIC, optionDecl->data.unionDeclaration.accessModifier);
    TEST_ASSERT_EQUAL_STRING("Option", identifier_text(optionDecl->data.unionDeclaration.name));
    TEST_ASSERT_NOT_NULL(optionDecl->data.unionDeclaration.generic);
    TEST_ASSERT_NOT_NULL(optionDecl->data.unionDeclaration.generic->params);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)optionDecl->data.unionDeclaration.generic->params->count);
    TEST_ASSERT_EQUAL_STRING("T",
                             identifier_text(optionDecl->data.unionDeclaration.generic->params->nodes[0]->data.parameter.name));
    TEST_ASSERT_NOT_NULL(optionDecl->data.unionDeclaration.variants);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)optionDecl->data.unionDeclaration.variants->count);

    resultDecl = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(resultDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNION_DECLARATION, resultDecl->type);
    TEST_ASSERT_EQUAL_STRING("Result", identifier_text(resultDecl->data.unionDeclaration.name));
    TEST_ASSERT_NOT_NULL(resultDecl->data.unionDeclaration.generic);
    TEST_ASSERT_NOT_NULL(resultDecl->data.unionDeclaration.generic->params);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)resultDecl->data.unionDeclaration.generic->params->count);
    TEST_ASSERT_EQUAL_STRING("T",
                             identifier_text(resultDecl->data.unionDeclaration.generic->params->nodes[0]->data.parameter.name));
    TEST_ASSERT_EQUAL_STRING("E",
                             identifier_text(resultDecl->data.unionDeclaration.generic->params->nodes[1]->data.parameter.name));
    TEST_ASSERT_NOT_NULL(resultDecl->data.unionDeclaration.variants);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)resultDecl->data.unionDeclaration.variants->count);

    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_declaration_parses_default_using_variant_marker(void) {
    const char *source =
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrAstNode *decl;
    SZrAstNode *unavailableVariant;
    SZrAstNode *availableVariant;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)ast->data.script.statements->count);

    decl = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(decl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNION_DECLARATION, decl->type);
    TEST_ASSERT_EQUAL_STRING("DynamicModule", identifier_text(decl->data.unionDeclaration.name));
    TEST_ASSERT_NOT_NULL(decl->data.unionDeclaration.generic);
    TEST_ASSERT_NOT_NULL(decl->data.unionDeclaration.variants);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)decl->data.unionDeclaration.variants->count);

    unavailableVariant = decl->data.unionDeclaration.variants->nodes[0];
    availableVariant = decl->data.unionDeclaration.variants->nodes[1];
    TEST_ASSERT_NOT_NULL(unavailableVariant);
    TEST_ASSERT_NOT_NULL(availableVariant);
    TEST_ASSERT_FALSE(unavailableVariant->data.unionVariant.isDefaultUsingVariant);
    TEST_ASSERT_TRUE(availableVariant->data.unionVariant.isDefaultUsingVariant);
    TEST_ASSERT_EQUAL_STRING("Available", identifier_text(availableVariant->data.unionVariant.name));
    TEST_ASSERT_EQUAL_INT(ZR_UNION_VARIANT_TUPLE, availableVariant->data.unionVariant.kind);
    TEST_ASSERT_NOT_NULL(availableVariant->data.unionVariant.fields);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)availableVariant->data.unionVariant.fields->count);

    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_declaration_rejects_duplicate_default_using_variant_marker(void) {
    const char *source =
            "union DynamicModule<T> {\n"
            "    @Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n";
    SZrParserState parserState;
    SZrString *sourceName;
    SZrAstNode *ast;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_default_marker_duplicate.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(&parserState, g_state, source, strlen(source), sourceName);
    ast = ZrParser_ParseWithState(&parserState);

    TEST_ASSERT_TRUE(parserState.hasError);

    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, ast);
    }
    ZrParser_State_Free(&parserState);
}

static void test_union_variant_constructors_parse_as_member_paths(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n"
            "var s = Shape.Circle(2.0);\n"
            "var e = Shape.Empty;\n"
            "var some = Option<int>.Some(42);\n"
            "var none = Option<int>.None;\n";
    SZrAstNode *ast = parse_union_source(source);

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(6u, (TZrUInt32)ast->data.script.statements->count);
    assert_primary_identifier_variant_value(statement_value(ast, 2u), "Shape", "Circle", 1u);
    assert_primary_identifier_variant_value(statement_value(ast, 3u), "Shape", "Empty", 0u);
    assert_primary_generic_variant_value(statement_value(ast, 4u), "Option", "int", "Some", 1u);
    assert_primary_generic_variant_value(statement_value(ast, 5u), "Option", "int", "None", 0u);

    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_variant_constructors_infer_union_type(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n"
            "var s = Shape.Circle(2.0);\n"
            "var e = Shape.Empty;\n"
            "var some = Option<int>.Some(42);\n"
            "var none = Option<int>.None;\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;

    assert_inferred_type_name(cs, statement_value(ast, 2u), "Shape");
    assert_inferred_type_name(cs, statement_value(ast, 3u), "Shape");
    assert_inferred_type_name(cs, statement_value(ast, 4u), "Option<int>");
    assert_inferred_type_name(cs, statement_value(ast, 5u), "Option<int>");

    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_struct_variant_constructor_parses_and_infers_union_type(void) {
    const char *source =
            "union Shape {\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var r = Shape.Rect { width: 3.0, height: 4.0 };\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)ast->data.script.statements->count);
    assert_primary_identifier_struct_variant_value(statement_value(ast, 1u), "Shape", "Rect", 2u);

    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    assert_inferred_type_name(cs, statement_value(ast, 1u), "Shape");

    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_variant_constructor_compiles_to_runtime_carrier_object(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "var s = Shape.Circle(2.0);\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    ZrParser_Expression_Compile(cs, statement_value(ast, 1u));

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_PARSER_SLOT_NONE, cs->lastExpressionSlot);
    TEST_ASSERT_TRUE(compiled_instructions_contain_opcode(cs, ZR_INSTRUCTION_ENUM(CREATE_OBJECT)));
    TEST_ASSERT_TRUE(compiled_instructions_contain_set_member_name(cs, "__zr_unionType"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_set_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_set_member_name(cs, "__zr_unionPayload0"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_struct_variant_constructor_compiles_to_runtime_carrier_object(void) {
    const char *source =
            "union Shape {\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var r = Shape.Rect { width: 3.0, height: 4.0 };\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    ZrParser_Expression_Compile(cs, statement_value(ast, 1u));

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_PARSER_SLOT_NONE, cs->lastExpressionSlot);
    TEST_ASSERT_TRUE(compiled_instructions_contain_opcode(cs, ZR_INSTRUCTION_ENUM(CREATE_OBJECT)));
    TEST_ASSERT_TRUE(compiled_instructions_contain_set_member_name(cs, "__zr_unionType"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_set_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_set_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_set_member_name(cs, "__zr_unionPayload1"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_variant_switch_case_compiles_to_tag_comparison(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "var s = Shape.Circle(2.0);\n"
            "switch (s) {\n"
            "    (Shape.Empty) { var miss = 0; }\n"
            "    (Shape.Circle) { var hit = 1; }\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *switchStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    switchStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(switchStatement);

    ZrParser_Statement_Compile(cs, varStatement);
    ZrParser_Statement_Compile(cs, switchStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_variant_switch_payload_pattern_binds_payload_member(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "var s = Shape.Circle(2.0);\n"
            "switch (s) {\n"
            "    (Shape.Circle(r)) { var hit = 1; }\n"
            "    () { var miss = 0; }\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *switchStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    switchStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(switchStatement);

    ZrParser_Statement_Compile(cs, varStatement);
    ZrParser_Statement_Compile(cs, switchStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "r"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_switch_binds_unqualified_tuple_variant_pattern_from_subject_type(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "var s: Shape = Shape.Circle(2.0);\n"
            "switch (s) {\n"
            "    (Circle(r)) { var hit = r; }\n"
            "    (Empty) { var miss = 0; }\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *switchStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    switchStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(switchStatement);

    ZrParser_Statement_Compile(cs, varStatement);
    ZrParser_Statement_Compile(cs, switchStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "r"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_switch_binds_unqualified_struct_variant_pattern_from_subject_type(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var s: Shape = Shape.Rect { width: 3.0, height: 4.0 };\n"
            "switch (s) {\n"
            "    (Rect { width: w, height: h }) { var area = w; }\n"
            "    (Empty) { var miss = 0; }\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *switchStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    switchStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(switchStatement);

    ZrParser_Statement_Compile(cs, varStatement);
    ZrParser_Statement_Compile(cs, switchStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload1"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "w"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "h"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_struct_variant_switch_payload_pattern_binds_named_payload_members(void) {
    const char *source =
            "union Shape {\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var s = Shape.Rect { width: 3.0, height: 4.0 };\n"
            "switch (s) {\n"
            "    (Shape.Rect { width: w, height: h }) { var area = w; }\n"
            "    () { var miss = 0; }\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *switchStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    switchStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(switchStatement);

    ZrParser_Statement_Compile(cs, varStatement);
    ZrParser_Statement_Compile(cs, switchStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload1"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "w"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "h"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_variant_using_guard_binds_tuple_destructuring_payload_member(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "var s = Shape.Circle(2.0);\n"
            "using (var [r]: Shape.Circle = s) {\n"
            "    var hit = 1;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    usingStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_NOT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DESTRUCTURING_ARRAY, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, varStatement);
    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "r"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_struct_variant_using_guard_binds_object_destructuring_payload_member_aliases(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var s = Shape.Rect { width: 3.0, height: 4.0 };\n"
            "using (var {w: width, h: height}: Shape.Rect = s) {\n"
            "    var area = w;\n"
            "    var tall = h;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    usingStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_NOT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_OBJECT_LITERAL, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, varStatement);
    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload1"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "w"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "h"));
    TEST_ASSERT_FALSE(compiled_local_variables_contain_name(cs, "width"));
    TEST_ASSERT_FALSE(compiled_local_variables_contain_name(cs, "height"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_struct_variant_using_guard_binds_object_destructuring_mixed_aliases(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var s = Shape.Rect { width: 3.0, height: 4.0 };\n"
            "using (var {width, h: height}: Shape.Rect = s) {\n"
            "    var area = width;\n"
            "    var tall = h;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    usingStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_NOT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_OBJECT_LITERAL, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, varStatement);
    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload1"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "width"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "h"));
    TEST_ASSERT_FALSE(compiled_local_variables_contain_name(cs, "height"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_switch_rejects_tuple_variant_object_payload_pattern(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "var s = Shape.Circle(2.0);\n"
            "switch (s) {\n"
            "    (Shape.Circle { radius: r }) { var hit = 1; }\n"
            "    () { var miss = 0; }\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *switchStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    switchStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(switchStatement);

    ZrParser_Statement_Compile(cs, varStatement);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_Statement_Compile(cs, switchStatement);

    TEST_ASSERT_TRUE(cs->hasError);

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_switch_rejects_struct_variant_tuple_payload_pattern(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var s = Shape.Rect { width: 3.0, height: 4.0 };\n"
            "switch (s) {\n"
            "    (Shape.Rect(w, h)) { var hit = 1; }\n"
            "    () { var miss = 0; }\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *switchStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    switchStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(switchStatement);

    ZrParser_Statement_Compile(cs, varStatement);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_Statement_Compile(cs, switchStatement);

    TEST_ASSERT_TRUE(cs->hasError);

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_using_guard_rejects_tuple_variant_object_destructuring(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "var s = Shape.Circle(2.0);\n"
            "using (var {r: radius}: Shape.Circle = s) {\n"
            "    var hit = r;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    usingStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_NOT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_OBJECT_LITERAL, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, varStatement);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_TRUE(cs->hasError);

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_using_guard_rejects_struct_variant_tuple_destructuring(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var s = Shape.Rect { width: 3.0, height: 4.0 };\n"
            "using (var [w, h]: Shape.Rect = s) {\n"
            "    var hit = w;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    usingStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_NOT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DESTRUCTURING_ARRAY, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, varStatement);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_TRUE(cs->hasError);

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_default_using_variant_binds_tuple_payload_without_annotation(void) {
    const char *source =
            "union Option {\n"
            "    None;\n"
            "    @Some(value: int);\n"
            "}\n"
            "var o = Option.Some(42);\n"
            "using (var [value] = o) {\n"
            "    var hit = value;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    usingStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DESTRUCTURING_ARRAY, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, varStatement);
    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "value"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_no_block_default_using_variant_binds_tuple_payload_without_annotation(void) {
    const char *source =
            "union Option {\n"
            "    None;\n"
            "    @Some(value: int);\n"
            "}\n"
            "var o: Option = Option.Some(42);\n"
            "using [value] = o;\n"
            "return value;\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_no_block_using_tuple_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_no_block_default_using_variant_binds_generic_tuple_payload_without_annotation(void) {
    const char *source =
            "union Option<T> {\n"
            "    None;\n"
            "    @Some(value: T);\n"
            "}\n"
            "using [directGeneric] = Option<int>.Some(76);\n"
            "return directGeneric;\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_no_block_using_generic_tuple_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(76, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_no_block_var_default_using_variant_binds_generic_tuple_payload_without_annotation(void) {
    const char *source =
            "union Option<T> {\n"
            "    None;\n"
            "    @Some(value: T);\n"
            "}\n"
            "using var [directGeneric] = Option<int>.Some(76);\n"
            "return directGeneric;\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_no_block_using_var_generic_tuple_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(76, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_default_using_variant_binds_generic_tuple_payload_inside_using_block(void) {
    const char *source =
            "union Option<T> {\n"
            "    None;\n"
            "    @Some(value: T);\n"
            "}\n"
            "using (var [holder] = Option<int>.Some(1)) {\n"
            "    using [nestedGeneric] = Option<int>.Some(41);\n"
            "    return nestedGeneric + 1;\n"
            "} else {\n"
            "    return -1;\n"
            "}\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_using_block_generic_tuple_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_no_block_using_variant_binds_tuple_payload_with_annotation(void) {
    const char *source =
            "union Choice {\n"
            "    Empty;\n"
            "    Num(value: int);\n"
            "}\n"
            "var choice: Choice = Choice.Num(7);\n"
            "using [value]: Choice.Num = choice;\n"
            "return value;\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_no_block_using_tuple_annotated_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_default_using_variant_rejects_tuple_object_destructuring_without_annotation(void) {
    const char *source =
            "union Option {\n"
            "    None;\n"
            "    @Some(value: int);\n"
            "}\n"
            "var o = Option.Some(42);\n"
            "using (var {v: value} = o) {\n"
            "    var hit = v;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    usingStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_OBJECT_LITERAL, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, varStatement);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_TRUE(cs->hasError);

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_default_using_variant_rejects_struct_tuple_destructuring_without_annotation(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    @Rect { width: int; height: int; }\n"
            "}\n"
            "var s = Shape.Rect { width: 3, height: 4 };\n"
            "using (var [w, h] = s) {\n"
            "    var hit = w;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    usingStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DESTRUCTURING_ARRAY, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, varStatement);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_TRUE(cs->hasError);

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_default_using_variant_binds_struct_payload_without_annotation(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    @Rect { width: float; height: float; }\n"
            "}\n"
            "var s = Shape.Rect { width: 3.0, height: 4.0 };\n"
            "using (var {w: width, height} = s) {\n"
            "    var wide = w;\n"
            "    var tall = height;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    usingStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_OBJECT_LITERAL, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, varStatement);
    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload1"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "w"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "height"));
    TEST_ASSERT_FALSE(compiled_local_variables_contain_name(cs, "width"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_default_using_variant_binds_struct_payload_mixed_aliases_without_annotation(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    @Rect { width: float; height: float; }\n"
            "}\n"
            "var s = Shape.Rect { width: 3.0, height: 4.0 };\n"
            "using (var {width, h: height} = s) {\n"
            "    var wide = width;\n"
            "    var tall = h;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    usingStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_OBJECT_LITERAL, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, varStatement);
    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload1"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "width"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "h"));
    TEST_ASSERT_FALSE(compiled_local_variables_contain_name(cs, "height"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_no_block_default_using_variant_binds_struct_payload_mixed_aliases_without_annotation(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    @Rect { width: int; height: int; }\n"
            "}\n"
            "var shape: Shape = Shape.Rect { width: 4, height: 5 };\n"
            "using {width, h: height} = shape;\n"
            "return width + h;\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    sourceName = ZrCore_String_CreateFromNative(g_state, "union_no_block_using_struct_runtime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(9, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_union_dynamic_module_import_guard_binds_default_variant_payload(void) {
    const char *source =
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "using (var [m]: DynamicModule<Plugins> = %import(\"zr.plugins\")) {\n"
            "    var hit = 1;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    usingStatement = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_USING_STATEMENT, usingStatement->type);
    TEST_ASSERT_EQUAL_INT(ZR_USING_GUARD_PATTERN, usingStatement->data.usingStatement.guardKind);
    TEST_ASSERT_NOT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IMPORT_EXPRESSION, usingStatement->data.usingStatement.resource->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DESTRUCTURING_ARRAY, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_opcode(cs, ZR_INSTRUCTION_ENUM(FUNCTION_CALL)));
    TEST_ASSERT_FALSE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_FALSE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "m"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_dynamic_module_import_guard_binds_explicit_variant_payload(void) {
    const char *source =
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "using (var [m]: DynamicModule<Plugins>.Available = %import(\"zr.plugins\")) {\n"
            "    var hit = 1;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    usingStatement = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_USING_STATEMENT, usingStatement->type);
    TEST_ASSERT_EQUAL_INT(ZR_USING_GUARD_PATTERN, usingStatement->data.usingStatement.guardKind);
    TEST_ASSERT_NOT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IMPORT_EXPRESSION, usingStatement->data.usingStatement.resource->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DESTRUCTURING_ARRAY, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_opcode(cs, ZR_INSTRUCTION_ENUM(FUNCTION_CALL)));
    TEST_ASSERT_FALSE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_FALSE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "m"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_dynamic_module_import_guard_accepts_generic_where_constraint(void) {
    const char *source =
            "union DynamicModule<T> where T: zr.Module {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "using(var [m]:DynamicModule<Plugins>.Available = %import(\"zr.plugins\")){\n"
            "    var hit = 1;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    usingStatement = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_USING_STATEMENT, usingStatement->type);
    TEST_ASSERT_EQUAL_INT(ZR_USING_GUARD_PATTERN, usingStatement->data.usingStatement.guardKind);
    TEST_ASSERT_NOT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IMPORT_EXPRESSION, usingStatement->data.usingStatement.resource->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DESTRUCTURING_ARRAY, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_opcode(cs, ZR_INSTRUCTION_ENUM(FUNCTION_CALL)));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "m"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_dynamic_module_import_guard_rejects_non_default_variant_annotation(void) {
    const char *source =
            "union DynamicModule<T> {\n"
            "    @Unavailable;\n"
            "    Available(m: Module);\n"
            "}\n"
            "using (var [m]: DynamicModule<Plugins>.Available = %import(\"zr.plugins\")) {\n"
            "    var hit = 1;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    usingStatement = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_USING_STATEMENT, usingStatement->type);
    TEST_ASSERT_EQUAL_INT(ZR_USING_GUARD_PATTERN, usingStatement->data.usingStatement.guardKind);
    TEST_ASSERT_NOT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IMPORT_EXPRESSION, usingStatement->data.usingStatement.resource->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DESTRUCTURING_ARRAY, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_TRUE(cs->hasError);

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_dynamic_module_import_guard_binds_unannotated_default_variant_payload(void) {
    const char *source =
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "using (var [m] = %import(\"zr.plugins\")) {\n"
            "    var hit = 1;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *usingStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    usingStatement = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(usingStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_USING_STATEMENT, usingStatement->type);
    TEST_ASSERT_EQUAL_INT(ZR_USING_GUARD_PATTERN, usingStatement->data.usingStatement.guardKind);
    TEST_ASSERT_NULL(usingStatement->data.usingStatement.guardTypeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IMPORT_EXPRESSION, usingStatement->data.usingStatement.resource->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DESTRUCTURING_ARRAY, usingStatement->data.usingStatement.pattern->type);

    ZrParser_Statement_Compile(cs, usingStatement);

    TEST_ASSERT_FALSE(cs->hasError);
    TEST_ASSERT_TRUE(compiled_instructions_contain_opcode(cs, ZR_INSTRUCTION_ENUM(FUNCTION_CALL)));
    TEST_ASSERT_FALSE(compiled_instructions_contain_get_member_name(cs, "__zr_unionVariant"));
    TEST_ASSERT_FALSE(compiled_instructions_contain_get_member_name(cs, "__zr_unionPayload0"));
    TEST_ASSERT_TRUE(compiled_local_variables_contain_name(cs, "m"));

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_switch_missing_variant_reports_non_exhaustive(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "var s = Shape.Empty;\n"
            "switch (s) {\n"
            "    (Shape.Empty) { var hit = 1; }\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *switchStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    switchStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(switchStatement);

    ZrParser_Statement_Compile(cs, varStatement);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_Statement_Compile(cs, switchStatement);

    TEST_ASSERT_TRUE(cs->hasError);

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_union_switch_rejects_duplicate_variant_case_as_unreachable(void) {
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "var s: Shape = Shape.Circle(2.0);\n"
            "switch (s) {\n"
            "    (Circle(r)) { var first = r; }\n"
            "    (Circle(other)) { var second = other; }\n"
            "    (Empty) { var miss = 0; }\n"
            "}\n";
    SZrAstNode *ast = parse_union_source(source);
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *varStatement;
    SZrAstNode *switchStatement;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(cs);
    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    varStatement = ast->data.script.statements->nodes[1];
    switchStatement = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(varStatement);
    TEST_ASSERT_NOT_NULL(switchStatement);

    ZrParser_Statement_Compile(cs, varStatement);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_Statement_Compile(cs, switchStatement);

    TEST_ASSERT_TRUE(cs->hasError);

    ZrCore_Function_Free(g_state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_union_declaration_serializes_prototype_and_variant_metadata);
    RUN_TEST(test_union_variant_metadata_serializes_payload_field_names_and_types);
    RUN_TEST(test_union_variant_metadata_serializes_byte_layout);
    RUN_TEST(test_union_owner_payload_metadata_marks_value_slot_ownership);
    RUN_TEST(test_union_typed_local_uses_inline_frame_layout);
    RUN_TEST(test_union_typed_local_in_child_function_uses_inline_frame_layout);
    RUN_TEST(test_union_constructor_materializes_inline_tag_and_payload_bytes);
    RUN_TEST(test_union_switch_reads_inline_tag_and_payload_from_typed_local);
    RUN_TEST(test_union_using_guard_reads_inline_tag_and_payload_from_typed_local);
    RUN_TEST(test_union_using_guard_rejects_legacy_variant_call_binder);
    RUN_TEST(test_union_using_guard_matches_unit_variant_with_empty_tuple_destructuring);
    RUN_TEST(test_union_variable_initialization_copies_inline_tag_and_payload_between_typed_locals);
    RUN_TEST(test_union_assignment_copies_inline_tag_and_payload_between_typed_locals);
    RUN_TEST(test_union_assignment_materializes_constructor_into_existing_typed_local);
    RUN_TEST(test_union_struct_field_assignment_materializes_constructor_payload);
    RUN_TEST(test_union_nested_struct_field_assignment_materializes_constructor_payload);
    RUN_TEST(test_union_nested_struct_field_using_guard_reads_constructor_payload);
    RUN_TEST(test_union_owner_payload_control_shared_parameter_releases_call_window_owner);
    RUN_TEST(test_union_owner_payload_releases_active_variant_on_inline_frame_drop);
    RUN_TEST(test_union_constructor_assignment_drops_replaced_owner_payload);
    RUN_TEST(test_union_struct_field_assignment_drops_replaced_owner_payload);
    RUN_TEST(test_union_nested_struct_field_assignment_drops_replaced_owner_payload);
    RUN_TEST(test_union_struct_field_assignment_copies_owner_payload_from_typed_local);
    RUN_TEST(test_union_typed_local_initialization_copies_owner_payload_from_struct_field);
    RUN_TEST(test_union_using_guard_owner_payload_binding_borrows_by_default);
    RUN_TEST(test_union_using_guard_owner_payload_move_binding_allows_release);
    RUN_TEST(test_union_using_guard_struct_owner_payload_move_binding_allows_release);
    RUN_TEST(test_union_switch_owner_payload_move_binding_allows_release);
    RUN_TEST(test_union_switch_struct_owner_payload_move_binding_allows_release);
    RUN_TEST(test_union_multiple_owner_payloads_release_all_active_variant_fields);
    RUN_TEST(test_union_declaration_parses_rust_style_variants);
    RUN_TEST(test_union_declaration_parses_generic_option_and_result);
    RUN_TEST(test_union_declaration_parses_default_using_variant_marker);
    RUN_TEST(test_union_declaration_rejects_duplicate_default_using_variant_marker);
    RUN_TEST(test_union_variant_constructors_parse_as_member_paths);
    RUN_TEST(test_union_variant_constructors_infer_union_type);
    RUN_TEST(test_union_struct_variant_constructor_parses_and_infers_union_type);
    RUN_TEST(test_union_variant_constructor_compiles_to_runtime_carrier_object);
    RUN_TEST(test_union_struct_variant_constructor_compiles_to_runtime_carrier_object);
    RUN_TEST(test_union_variant_switch_case_compiles_to_tag_comparison);
    RUN_TEST(test_union_variant_switch_payload_pattern_binds_payload_member);
    RUN_TEST(test_union_switch_binds_unqualified_tuple_variant_pattern_from_subject_type);
    RUN_TEST(test_union_switch_binds_unqualified_struct_variant_pattern_from_subject_type);
    RUN_TEST(test_union_struct_variant_switch_payload_pattern_binds_named_payload_members);
    RUN_TEST(test_union_variant_using_guard_binds_tuple_destructuring_payload_member);
    RUN_TEST(test_union_struct_variant_using_guard_binds_object_destructuring_payload_member_aliases);
    RUN_TEST(test_union_struct_variant_using_guard_binds_object_destructuring_mixed_aliases);
    RUN_TEST(test_union_switch_rejects_tuple_variant_object_payload_pattern);
    RUN_TEST(test_union_switch_rejects_struct_variant_tuple_payload_pattern);
    RUN_TEST(test_union_using_guard_rejects_tuple_variant_object_destructuring);
    RUN_TEST(test_union_using_guard_rejects_struct_variant_tuple_destructuring);
    RUN_TEST(test_union_default_using_variant_binds_tuple_payload_without_annotation);
    RUN_TEST(test_union_no_block_default_using_variant_binds_tuple_payload_without_annotation);
    RUN_TEST(test_union_no_block_default_using_variant_binds_generic_tuple_payload_without_annotation);
    RUN_TEST(test_union_no_block_var_default_using_variant_binds_generic_tuple_payload_without_annotation);
    RUN_TEST(test_union_default_using_variant_binds_generic_tuple_payload_inside_using_block);
    RUN_TEST(test_union_no_block_using_variant_binds_tuple_payload_with_annotation);
    RUN_TEST(test_union_default_using_variant_rejects_tuple_object_destructuring_without_annotation);
    RUN_TEST(test_union_default_using_variant_rejects_struct_tuple_destructuring_without_annotation);
    RUN_TEST(test_union_default_using_variant_binds_struct_payload_without_annotation);
    RUN_TEST(test_union_default_using_variant_binds_struct_payload_mixed_aliases_without_annotation);
    RUN_TEST(test_union_no_block_default_using_variant_binds_struct_payload_mixed_aliases_without_annotation);
    RUN_TEST(test_union_dynamic_module_import_guard_binds_default_variant_payload);
    RUN_TEST(test_union_dynamic_module_import_guard_binds_explicit_variant_payload);
    RUN_TEST(test_union_dynamic_module_import_guard_accepts_generic_where_constraint);
    RUN_TEST(test_union_dynamic_module_import_guard_rejects_non_default_variant_annotation);
    RUN_TEST(test_union_dynamic_module_import_guard_binds_unannotated_default_variant_payload);
    RUN_TEST(test_union_switch_missing_variant_reports_non_exhaustive);
    RUN_TEST(test_union_switch_rejects_duplicate_variant_case_as_unreachable);
    return UNITY_END();
}
