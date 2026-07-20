#include "reference_loan_nll_test_support.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/receiver_call.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"

typedef struct ZrLibRegisteredModuleRecord ZrLibRegisteredModuleRecord;

ZR_LIBRARY_API SZrObject *native_metadata_make_module_info(
        SZrState *state,
        const ZrLibModuleDescriptor *descriptor,
        const ZrLibRegisteredModuleRecord *record);

typedef struct SParserErrorCapture {
    TZrUInt32 count;
    char firstMessage[192];
} SParserErrorCapture;

static void capture_parser_error(
        TZrPtr userData,
        const SZrFileRange *location,
        const TZrChar *message,
        EZrToken token) {
    SParserErrorCapture *capture = (SParserErrorCapture *)userData;
    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(token);
    if (capture == ZR_NULL) {
        return;
    }
    if (capture->count == 0U && message != ZR_NULL) {
        snprintf(capture->firstMessage, sizeof(capture->firstMessage), "%s", message);
    }
    capture->count++;
}

static SZrAstNode *parse_source(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "receiver_call_boundary.zr");
    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
}

static SZrAstNode *script_statement(SZrAstNode *script, TZrSize index) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(
            (TZrUInt32)index,
            (TZrUInt32)script->data.script.statements->count);
    return script->data.script.statements->nodes[index];
}

static void test_const_fn_and_readonly_view_are_preserved_in_ast(void) {
    const TZrChar *source =
            "class Document { const fn render(): int {} fn clear(): void {} }\n"
            "struct Buffer { const fn length(): int {} fn push(): void {} }\n"
            "interface View { const fn render(): int; fn clear(): void; }\n"
            "fn inspect(value: readonly Document): void {}\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *classNode = script_statement(script, 0U);
    SZrAstNode *structNode = script_statement(script, 1U);
    SZrAstNode *interfaceNode = script_statement(script, 2U);
    SZrFunctionDeclaration *function =
            &script_statement(script, 3U)->data.functionDeclaration;

    TEST_ASSERT_EQUAL_INT(
            ZR_METHOD_RECEIVER_CONST,
            classNode->data.classDeclaration.members->nodes[0]
                    ->data.classMethod.receiverModifier);
    TEST_ASSERT_EQUAL_INT(
            ZR_METHOD_RECEIVER_DEFAULT,
            classNode->data.classDeclaration.members->nodes[1]
                    ->data.classMethod.receiverModifier);
    TEST_ASSERT_EQUAL_INT(
            ZR_METHOD_RECEIVER_CONST,
            structNode->data.structDeclaration.members->nodes[0]
                    ->data.structMethod.receiverModifier);
    TEST_ASSERT_EQUAL_INT(
            ZR_METHOD_RECEIVER_DEFAULT,
            structNode->data.structDeclaration.members->nodes[1]
                    ->data.structMethod.receiverModifier);
    TEST_ASSERT_EQUAL_INT(
            ZR_METHOD_RECEIVER_CONST,
            interfaceNode->data.interfaceDeclaration.members->nodes[0]
                    ->data.interfaceMethodSignature.receiverModifier);
    TEST_ASSERT_EQUAL_INT(
            ZR_METHOD_RECEIVER_DEFAULT,
            interfaceNode->data.interfaceDeclaration.members->nodes[1]
                    ->data.interfaceMethodSignature.receiverModifier);
    TEST_ASSERT_TRUE(
            function->params->nodes[0]->data.parameter.typeInfo->isReadonlyView);
    ZrParser_Ast_Free(g_state, script);
}

static void test_readonly_struct_contextual_declaration_normalizes_instance_contracts(void) {
    const TZrChar *source =
            "readonly struct Snapshot {\n"
            "  pub var value: int;\n"
            "  pub fn read(): int { return this.value; }\n"
            "  pub const fn explicitRead(): int { return this.value; }\n"
            "  pub static fn create(): int { return 1; }\n"
            "}\n"
            "readonly();\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *structNode = script_statement(script, 0U);
    SZrAstNode *expressionNode = script_statement(script, 1U);
    SZrAstNodeArray *members;

    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, structNode->type);
    TEST_ASSERT_TRUE(structNode->data.structDeclaration.isReadonly);
    members = structNode->data.structDeclaration.members;
    TEST_ASSERT_NOT_NULL(members);
    TEST_ASSERT_EQUAL_UINT32(4U, (TZrUInt32)members->count);
    TEST_ASSERT_FALSE(members->nodes[0]->data.structField.isConst);
    TEST_ASSERT_EQUAL_INT(
            ZR_METHOD_RECEIVER_CONST,
            members->nodes[1]->data.structMethod.receiverModifier);
    TEST_ASSERT_TRUE(
            members->nodes[1]->data.structMethod.isImplicitReadonlyReceiver);
    TEST_ASSERT_EQUAL_INT(
            ZR_METHOD_RECEIVER_CONST,
            members->nodes[2]->data.structMethod.receiverModifier);
    TEST_ASSERT_FALSE(
            members->nodes[2]->data.structMethod.isImplicitReadonlyReceiver);
    TEST_ASSERT_TRUE(members->nodes[3]->data.structMethod.isStatic);
    TEST_ASSERT_EQUAL_INT(
            ZR_METHOD_RECEIVER_DEFAULT,
            members->nodes[3]->data.structMethod.receiverModifier);
    TEST_ASSERT_FALSE(
            members->nodes[3]->data.structMethod.isImplicitReadonlyReceiver);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, expressionNode->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_PRIMARY_EXPRESSION,
            expressionNode->data.expressionStatement.expr->type);
    TEST_ASSERT_NOT_NULL(
            expressionNode->data.expressionStatement.expr->data.primaryExpression.members);
    TEST_ASSERT_EQUAL_UINT32(
            1U,
            (TZrUInt32)expressionNode->data.expressionStatement.expr
                    ->data.primaryExpression.members->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_FUNCTION_CALL,
            expressionNode->data.expressionStatement.expr->data.primaryExpression
                    .members->nodes[0]->type);
    ZrParser_Ast_Free(g_state, script);
}

static void test_static_const_fn_is_rejected(void) {
    const TZrChar *source = "class Invalid { static const fn inspect(): void {} }";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "invalid_receiver.zr");
    SZrParserState parserState;
    SParserErrorCapture capture;
    SZrAstNode *script;

    memset(&capture, 0, sizeof(capture));
    ZrParser_State_Init(&parserState, g_state, source, strlen(source), sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    parserState.errorCallback = capture_parser_error;
    parserState.errorUserData = &capture;
    script = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, capture.count);
    TEST_ASSERT_NOT_NULL(strstr(capture.firstMessage, "static const fn"));
    ZrParser_Ast_Free(g_state, script);
    ZrParser_State_Free(&parserState);
}

static void test_top_level_const_fn_is_rejected(void) {
    const TZrChar *source = "const fn inspect(): void {}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "invalid_top_level_receiver.zr");
    SZrParserState parserState;
    SParserErrorCapture capture;
    SZrAstNode *script;

    memset(&capture, 0, sizeof(capture));
    ZrParser_State_Init(&parserState, g_state, source, strlen(source), sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    parserState.errorCallback = capture_parser_error;
    parserState.errorUserData = &capture;
    script = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, capture.count);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_State_Free(&parserState);
}

static void assert_receiver_decision(
        SZrSemanticContext *context,
        TZrTypeId receiverTypeId,
        EZrCanonicalReceiverEffect effect,
        EZrReceiverDispatchKind dispatchKind,
        TZrBool addressable,
        TZrBool compilerGenerated,
        TZrBool allowed,
        EZrSemanticLoanAccess access,
        TZrBool autoDeref,
        TZrBool twoPhase) {
    SZrReceiverCallRequest request;
    SZrReceiverCallDecision decision;

    memset(&request, 0, sizeof(request));
    memset(&decision, 0, sizeof(decision));
    request.receiverTypeId = receiverTypeId;
    request.receiverEffect = effect;
    request.dispatchKind = dispatchKind;
    request.receiverIsAddressable = addressable;
    request.compilerGeneratedReceiverBorrow = compilerGenerated;
    TEST_ASSERT_TRUE(ZrParser_ReceiverCall_Analyze(context, &request, &decision));
    TEST_ASSERT_EQUAL_INT(allowed, decision.allowed);
    TEST_ASSERT_EQUAL_INT(access, decision.loanAccess);
    TEST_ASSERT_EQUAL_INT(autoDeref, decision.requiresOwnerAutoDeref);
    TEST_ASSERT_EQUAL_INT(twoPhase, decision.usesTwoPhaseBorrow);
}

static void test_receiver_capability_matrix_and_owner_auto_deref(void) {
    const EZrReceiverDispatchKind dispatchKinds[] = {
            ZR_RECEIVER_DISPATCH_CLASS,
            ZR_RECEIVER_DISPATCH_STRUCT,
            ZR_RECEIVER_DISPATCH_INTERFACE,
            ZR_RECEIVER_DISPATCH_OVERRIDE,
            ZR_RECEIVER_DISPATCH_GENERIC,
            ZR_RECEIVER_DISPATCH_DYNAMIC,
            ZR_RECEIVER_DISPATCH_NATIVE,
    };
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    TZrTypeId nominal = ZrParser_CanonicalType_InternNominal(
            context,
            ZrCore_String_CreateFromNative(g_state, "test"),
            ZrCore_String_CreateFromNative(g_state, "Data"),
            1U);
    TZrTypeId readonlyView =
            ZrParser_CanonicalType_InternReadonlyView(context, nominal);
    TZrTypeId writableRef = ZrParser_CanonicalType_InternRef(
            context, nominal, ZR_CANONICAL_REF_WRITABLE);
    TZrTypeId readonlyRef = ZrParser_CanonicalType_InternRef(
            context, nominal, ZR_CANONICAL_REF_READONLY);
    TZrTypeId uniqueOwner = ZrParser_CanonicalType_InternOwner(
            context, nominal, ZR_CANONICAL_OWNER_UNIQUE);
    TZrTypeId sharedOwner = ZrParser_CanonicalType_InternOwner(
            context, nominal, ZR_CANONICAL_OWNER_SHARED);

    TEST_ASSERT_NOT_NULL(context);
    for (TZrSize index = 0U;
         index < sizeof(dispatchKinds) / sizeof(dispatchKinds[0]);
         index++) {
        assert_receiver_decision(
                context, readonlyView, ZR_CANONICAL_RECEIVER_READONLY,
                dispatchKinds[index], ZR_TRUE, ZR_TRUE, ZR_TRUE,
                ZR_SEMANTIC_LOAN_SHARED, ZR_FALSE, ZR_FALSE);
        assert_receiver_decision(
                context, readonlyView, ZR_CANONICAL_RECEIVER_MUTABLE,
                dispatchKinds[index], ZR_TRUE, ZR_TRUE, ZR_FALSE,
                ZR_SEMANTIC_LOAN_MUTABLE, ZR_FALSE, ZR_FALSE);
        assert_receiver_decision(
                context, writableRef, ZR_CANONICAL_RECEIVER_MUTABLE,
                dispatchKinds[index], ZR_TRUE, ZR_TRUE, ZR_TRUE,
                ZR_SEMANTIC_LOAN_MUTABLE, ZR_FALSE, ZR_TRUE);
        assert_receiver_decision(
                context, readonlyRef, ZR_CANONICAL_RECEIVER_MUTABLE,
                dispatchKinds[index], ZR_TRUE, ZR_TRUE, ZR_FALSE,
                ZR_SEMANTIC_LOAN_MUTABLE, ZR_FALSE, ZR_FALSE);
        assert_receiver_decision(
                context, uniqueOwner, ZR_CANONICAL_RECEIVER_MUTABLE,
                dispatchKinds[index], ZR_TRUE, ZR_TRUE, ZR_TRUE,
                ZR_SEMANTIC_LOAN_MUTABLE, ZR_TRUE, ZR_TRUE);
        assert_receiver_decision(
                context, sharedOwner, ZR_CANONICAL_RECEIVER_READONLY,
                dispatchKinds[index], ZR_TRUE, ZR_TRUE, ZR_TRUE,
                ZR_SEMANTIC_LOAN_SHARED, ZR_TRUE, ZR_FALSE);
        assert_receiver_decision(
                context, sharedOwner, ZR_CANONICAL_RECEIVER_MUTABLE,
                dispatchKinds[index], ZR_TRUE, ZR_TRUE, ZR_FALSE,
                ZR_SEMANTIC_LOAN_MUTABLE, ZR_TRUE, ZR_FALSE);
    }
    assert_receiver_decision(
            context, nominal, ZR_CANONICAL_RECEIVER_MUTABLE,
            ZR_RECEIVER_DISPATCH_STRUCT, ZR_FALSE, ZR_TRUE, ZR_FALSE,
            ZR_SEMANTIC_LOAN_MUTABLE, ZR_FALSE, ZR_FALSE);
    assert_receiver_decision(
            context, nominal, ZR_CANONICAL_RECEIVER_MUTABLE,
            ZR_RECEIVER_DISPATCH_STRUCT, ZR_TRUE, ZR_FALSE, ZR_TRUE,
            ZR_SEMANTIC_LOAN_MUTABLE, ZR_FALSE, ZR_FALSE);
    ZrParser_SemanticContext_Free(context);
}

static void test_native_readonly_receiver_contract_is_serialized(void) {
    const ZrLibMethodDescriptor methods[] = {
            {
                    .name = "read",
                    .minArgumentCount = 0,
                    .maxArgumentCount = 0,
                    .returnTypeName = "int",
                    .isStatic = ZR_FALSE,
                    .dispatchFlags =
                            ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER,
            },
            {
                    .name = "write",
                    .minArgumentCount = 0,
                    .maxArgumentCount = 0,
                    .returnTypeName = "void",
                    .isStatic = ZR_FALSE,
            },
            {
                    .name = "inlineRead",
                    .minArgumentCount = 0,
                    .maxArgumentCount = 0,
                    .returnTypeName = "int",
                    .isStatic = ZR_FALSE,
                    .dispatchFlags =
                            ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT,
            },
    };
    const ZrLibMetaMethodDescriptor metaMethods[] = {
            {
                    .metaType = ZR_META_GET_ITEM,
                    .minArgumentCount = 1,
                    .maxArgumentCount = 1,
                    .returnTypeName = "int",
                    .dispatchFlags =
                            ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER,
            },
            {
                    .metaType = ZR_META_SET_ITEM,
                    .minArgumentCount = 2,
                    .maxArgumentCount = 2,
                    .returnTypeName = "void",
                    .dispatchFlags =
                            ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT,
            },
    };
    const ZrLibTypeDescriptor typeDescriptor = {
            .name = "NativeReceiverProbe",
            .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
            .methods = methods,
            .methodCount = sizeof(methods) / sizeof(methods[0]),
            .metaMethods = metaMethods,
            .metaMethodCount = sizeof(metaMethods) / sizeof(metaMethods[0]),
    };
    const ZrLibModuleDescriptor moduleDescriptor = {
            .moduleName = "test.receiver",
            .types = &typeDescriptor,
            .typeCount = 1U,
    };
    SZrObject *moduleInfo = native_metadata_make_module_info(
            g_state, &moduleDescriptor, ZR_NULL);
    const SZrTypeValue *typesValue;
    const SZrTypeValue *typeValue;
    const SZrTypeValue *methodsValue;
    const SZrTypeValue *readonlyEntryValue;
    const SZrTypeValue *writableEntryValue;
    const SZrTypeValue *inlineEntryValue;
    const SZrTypeValue *metaMethodsValue;
    const SZrTypeValue *metaReadonlyEntryValue;
    const SZrTypeValue *metaWritableEntryValue;
    SZrObject *typesArray;
    SZrObject *typeEntry;
    SZrObject *methodsArray;
    SZrObject *readonlyEntry;
    SZrObject *writableEntry;
    SZrObject *inlineEntry;
    SZrObject *metaMethodsArray;
    SZrObject *metaReadonlyEntry;
    SZrObject *metaWritableEntry;
    const SZrTypeValue *readonlyEffect;
    const SZrTypeValue *writableEffect;
    const SZrTypeValue *inlineEffect;
    const SZrTypeValue *metaReadonlyEffect;
    const SZrTypeValue *metaWritableEffect;

    TEST_ASSERT_NOT_NULL(moduleInfo);
    typesValue = ZrLib_Object_GetFieldCString(g_state, moduleInfo, "types");
    TEST_ASSERT_NOT_NULL(typesValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typesValue->type);
    typesArray = ZR_CAST_OBJECT(g_state, typesValue->value.object);
    TEST_ASSERT_NOT_NULL(typesArray);
    typeValue = ZrLib_Array_Get(g_state, typesArray, 0U);
    TEST_ASSERT_NOT_NULL(typeValue);
    typeEntry = ZR_CAST_OBJECT(g_state, typeValue->value.object);
    TEST_ASSERT_NOT_NULL(typeEntry);
    methodsValue = ZrLib_Object_GetFieldCString(g_state, typeEntry, "methods");
    TEST_ASSERT_NOT_NULL(methodsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, methodsValue->type);
    methodsArray = ZR_CAST_OBJECT(g_state, methodsValue->value.object);
    TEST_ASSERT_NOT_NULL(methodsArray);
    readonlyEntryValue = ZrLib_Array_Get(g_state, methodsArray, 0U);
    writableEntryValue = ZrLib_Array_Get(g_state, methodsArray, 1U);
    inlineEntryValue = ZrLib_Array_Get(g_state, methodsArray, 2U);
    TEST_ASSERT_NOT_NULL(readonlyEntryValue);
    TEST_ASSERT_NOT_NULL(writableEntryValue);
    TEST_ASSERT_NOT_NULL(inlineEntryValue);
    readonlyEntry = ZR_CAST_OBJECT(g_state, readonlyEntryValue->value.object);
    writableEntry = ZR_CAST_OBJECT(g_state, writableEntryValue->value.object);
    inlineEntry = ZR_CAST_OBJECT(g_state, inlineEntryValue->value.object);
    TEST_ASSERT_NOT_NULL(readonlyEntry);
    TEST_ASSERT_NOT_NULL(writableEntry);
    TEST_ASSERT_NOT_NULL(inlineEntry);
    readonlyEffect = ZrLib_Object_GetFieldCString(
            g_state, readonlyEntry, "isReadonlyReceiver");
    writableEffect = ZrLib_Object_GetFieldCString(
            g_state, writableEntry, "isReadonlyReceiver");
    inlineEffect = ZrLib_Object_GetFieldCString(
            g_state, inlineEntry, "isReadonlyReceiver");
    TEST_ASSERT_NOT_NULL(readonlyEffect);
    TEST_ASSERT_NOT_NULL(writableEffect);
    TEST_ASSERT_NOT_NULL(inlineEffect);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, readonlyEffect->type);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, writableEffect->type);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, inlineEffect->type);
    TEST_ASSERT_TRUE(readonlyEffect->value.nativeObject.nativeBool);
    TEST_ASSERT_FALSE(writableEffect->value.nativeObject.nativeBool);
    TEST_ASSERT_FALSE(inlineEffect->value.nativeObject.nativeBool);

    metaMethodsValue = ZrLib_Object_GetFieldCString(
            g_state, typeEntry, "metaMethods");
    TEST_ASSERT_NOT_NULL(metaMethodsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, metaMethodsValue->type);
    metaMethodsArray = ZR_CAST_OBJECT(g_state, metaMethodsValue->value.object);
    TEST_ASSERT_NOT_NULL(metaMethodsArray);
    metaReadonlyEntryValue = ZrLib_Array_Get(g_state, metaMethodsArray, 0U);
    metaWritableEntryValue = ZrLib_Array_Get(g_state, metaMethodsArray, 1U);
    TEST_ASSERT_NOT_NULL(metaReadonlyEntryValue);
    TEST_ASSERT_NOT_NULL(metaWritableEntryValue);
    metaReadonlyEntry = ZR_CAST_OBJECT(
            g_state, metaReadonlyEntryValue->value.object);
    metaWritableEntry = ZR_CAST_OBJECT(
            g_state, metaWritableEntryValue->value.object);
    TEST_ASSERT_NOT_NULL(metaReadonlyEntry);
    TEST_ASSERT_NOT_NULL(metaWritableEntry);
    metaReadonlyEffect = ZrLib_Object_GetFieldCString(
            g_state, metaReadonlyEntry, "isReadonlyReceiver");
    metaWritableEffect = ZrLib_Object_GetFieldCString(
            g_state, metaWritableEntry, "isReadonlyReceiver");
    TEST_ASSERT_NOT_NULL(metaReadonlyEffect);
    TEST_ASSERT_NOT_NULL(metaWritableEffect);
    TEST_ASSERT_TRUE(metaReadonlyEffect->value.nativeObject.nativeBool);
    TEST_ASSERT_FALSE(metaWritableEffect->value.nativeObject.nativeBool);
}

static void test_builtin_array_like_descriptor_has_receiver_effect_boundary(void) {
    const ZrLibModuleDescriptor *module;
    const ZrLibTypeDescriptor *arrayLike = ZR_NULL;
    const ZrLibMetaMethodDescriptor *getItem = ZR_NULL;
    const ZrLibMetaMethodDescriptor *setItem = ZR_NULL;

    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_Attach(g_state->global));
    module = ZrLibrary_NativeRegistry_FindModule(g_state->global, "zr.builtin");
    TEST_ASSERT_NOT_NULL(module);
    for (TZrSize typeIndex = 0; typeIndex < module->typeCount; typeIndex++) {
        if (strcmp(module->types[typeIndex].name, "IArrayLike") == 0) {
            arrayLike = &module->types[typeIndex];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(arrayLike);
    for (TZrSize metaIndex = 0; metaIndex < arrayLike->metaMethodCount;
         metaIndex++) {
        const ZrLibMetaMethodDescriptor *metaMethod =
                &arrayLike->metaMethods[metaIndex];
        if (metaMethod->metaType == ZR_META_GET_ITEM) {
            getItem = metaMethod;
        } else if (metaMethod->metaType == ZR_META_SET_ITEM) {
            setItem = metaMethod;
        }
    }
    TEST_ASSERT_NOT_NULL(getItem);
    TEST_ASSERT_NOT_NULL(setItem);
    TEST_ASSERT_BITS_HIGH(
            ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER,
            getItem->dispatchFlags);
    TEST_ASSERT_BITS_LOW(
            ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER,
            setItem->dispatchFlags);
}

static TZrLoanId add_two_phase_loan(
        SLoanFixture *fixture,
        TZrPlaceId placeId,
        TZrValueId valueId,
        TZrInt32 line) {
    return ZrParser_SemanticIr_AddLoanEx(
            &fixture->function,
            placeId,
            ZR_SEMANTIC_LOAN_MUTABLE,
            ZR_SEMANTIC_LOAN_TWO_PHASE,
            fixture->regionId,
            test_range(line),
            test_range(line),
            valueId);
}

static void test_two_phase_receiver_reserve_read_activate(void) {
    SLoanFixture fixture;
    TZrPlaceId placeId;
    TZrValueId receiverValue;
    TZrLoanId receiverLoan;
    TZrSemanticInstructionId readInstruction;
    TZrSemanticInstructionId activateInstruction;
    TZrSemanticInstructionId writeInstruction;

    fixture_init(&fixture);
    placeId = add_place(&fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 50U);
    receiverValue = add_value(&fixture, 1);
    receiverLoan = add_two_phase_loan(&fixture, placeId, receiverValue, 1);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_RESERVE_BORROW_MUT, placeId, 0U,
            receiverValue, receiverLoan, 1);
    readInstruction = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_LOAD, placeId, 0U,
            add_value(&fixture, 2), ZR_SEMANTIC_LOAN_ID_INVALID, 2);
    activateInstruction = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_ACTIVATE_LOAN, placeId, 0U,
            0U, receiverLoan, 3);
    writeInstruction = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, add_value(&fixture, 4),
            0U, receiverLoan, 4);
    bind_linear_cfg(&fixture);
    analyze(&fixture);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)fixture.result.diagnostics.length);
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_LoanIsActiveAt(
            &fixture.result, readInstruction, receiverLoan, ZR_TRUE));
    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_LoanIsActiveAt(
            &fixture.result, activateInstruction, receiverLoan, ZR_TRUE));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_LoanIsActiveAt(
            &fixture.result, writeInstruction, receiverLoan, ZR_TRUE));
    fixture_free(&fixture);
}

static void test_reserved_receiver_rejects_direct_write_and_second_reserve(void) {
    SLoanFixture fixture;
    TZrPlaceId placeId;
    TZrValueId firstValue;
    TZrValueId secondValue;
    TZrLoanId firstLoan;
    TZrLoanId secondLoan;
    TZrSemanticInstructionId writeInstruction;
    TZrSemanticInstructionId secondReserveInstruction;

    fixture_init(&fixture);
    placeId = add_place(&fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 60U);
    firstValue = add_value(&fixture, 1);
    secondValue = add_value(&fixture, 2);
    firstLoan = add_two_phase_loan(&fixture, placeId, firstValue, 1);
    secondLoan = add_two_phase_loan(&fixture, placeId, secondValue, 2);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_RESERVE_BORROW_MUT, placeId, 0U,
            firstValue, firstLoan, 1);
    writeInstruction = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, add_value(&fixture, 2),
            0U, ZR_SEMANTIC_LOAN_ID_INVALID, 2);
    secondReserveInstruction = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_RESERVE_BORROW_MUT, placeId, 0U,
            secondValue, secondLoan, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_ACTIVATE_LOAN, placeId, 0U,
            0U, firstLoan, 4);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, add_value(&fixture, 5),
            0U, firstLoan, 5);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_END_LOAN, placeId, 0U,
            0U, firstLoan, 6);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_ACTIVATE_LOAN, placeId, 0U,
            0U, secondLoan, 7);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_STORE, placeId, add_value(&fixture, 8),
            0U, secondLoan, 8);
    bind_linear_cfg(&fixture);
    analyze(&fixture);
    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, writeInstruction));
    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(
            &fixture, secondReserveInstruction));
    fixture_free(&fixture);
}

static void test_branch_activation_does_not_dominate_join(void) {
    SLoanFixture fixture;
    TZrPlaceId placeId;
    TZrValueId receiverValue;
    TZrLoanId receiverLoan;
    TZrSemanticInstructionId joinRead;
    TZrUInt32 entry;
    TZrUInt32 header;
    TZrUInt32 trueBlock;
    TZrUInt32 falseBlock;
    TZrUInt32 join;
    TZrUInt32 exit;

    fixture_init(&fixture);
    placeId = add_place(&fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 70U);
    receiverValue = add_value(&fixture, 1);
    receiverLoan = add_two_phase_loan(
            &fixture, placeId, receiverValue, 1);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_RESERVE_BORROW_MUT, placeId, 0U,
            receiverValue, receiverLoan, 1);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_ACTIVATE_LOAN, placeId, 0U,
            0U, receiverLoan, 2);
    joinRead = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_LOAD, placeId, 0U,
            add_value(&fixture, 3), ZR_SEMANTIC_LOAN_ID_INVALID, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_END_LOAN, placeId, 0U,
            0U, receiverLoan, 4);

    entry = append_block(&fixture, ZR_PARSER_CFG_BLOCK_ENTRY);
    header = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    trueBlock = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    falseBlock = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    join = append_block(&fixture, ZR_PARSER_CFG_BLOCK_JOIN);
    exit = append_block(&fixture, ZR_PARSER_CFG_BLOCK_EXIT);
    fixture.function.cfg.entryBlockId = entry;
    fixture.function.cfg.exitBlockId = exit;
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, entry, header,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, header, trueBlock,
            ZR_PARSER_CFG_EDGE_TRUE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, header, falseBlock,
            ZR_PARSER_CFG_EDGE_FALSE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, trueBlock, join,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, falseBlock, join,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, join, exit,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    bind_block(&fixture, entry, 0U, 1U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, header, 1U, 0U, ZR_PARSER_CFG_TERMINATOR_BRANCH);
    bind_block(&fixture, trueBlock, 1U, 1U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, falseBlock, 2U, 0U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, join, 2U, 2U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, exit, 4U, 0U, ZR_PARSER_CFG_TERMINATOR_EXIT);
    analyze(&fixture);

    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_LoanIsActiveAt(
            &fixture.result, joinRead, receiverLoan, ZR_TRUE));
    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, joinRead));
    fixture_free(&fixture);
}

static void test_loop_backedge_preserves_maybe_active_conflict(void) {
    SLoanFixture fixture;
    TZrPlaceId placeId;
    TZrValueId receiverValue;
    TZrLoanId receiverLoan;
    TZrSemanticInstructionId headerRead;
    TZrUInt32 entry;
    TZrUInt32 header;
    TZrUInt32 body;
    TZrUInt32 exit;

    fixture_init(&fixture);
    placeId = add_place(&fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 80U);
    receiverValue = add_value(&fixture, 1);
    receiverLoan = add_two_phase_loan(
            &fixture, placeId, receiverValue, 1);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_RESERVE_BORROW_MUT, placeId, 0U,
            receiverValue, receiverLoan, 1);
    headerRead = emit_instruction(
            &fixture, ZR_SEMANTIC_IR_LOAD, placeId, 0U,
            add_value(&fixture, 2), ZR_SEMANTIC_LOAN_ID_INVALID, 2);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_ACTIVATE_LOAN, placeId, 0U,
            0U, receiverLoan, 3);
    emit_instruction(
            &fixture, ZR_SEMANTIC_IR_END_LOAN, placeId, 0U,
            0U, receiverLoan, 4);

    entry = append_block(&fixture, ZR_PARSER_CFG_BLOCK_ENTRY);
    header = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    body = append_block(&fixture, ZR_PARSER_CFG_BLOCK_STATEMENT);
    exit = append_block(&fixture, ZR_PARSER_CFG_BLOCK_EXIT);
    fixture.function.cfg.entryBlockId = entry;
    fixture.function.cfg.exitBlockId = exit;
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, entry, header,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, header, body,
            ZR_PARSER_CFG_EDGE_TRUE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, header, exit,
            ZR_PARSER_CFG_EDGE_FALSE_BRANCH, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture.function.cfg, body, header,
            ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL));
    bind_block(&fixture, entry, 0U, 1U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, header, 1U, 1U, ZR_PARSER_CFG_TERMINATOR_BRANCH);
    bind_block(&fixture, body, 2U, 1U, ZR_PARSER_CFG_TERMINATOR_NONE);
    bind_block(&fixture, exit, 3U, 1U, ZR_PARSER_CFG_TERMINATOR_EXIT);
    analyze(&fixture);

    TEST_ASSERT_FALSE(ZrParser_SemanticFlow_LoanIsActiveAt(
            &fixture.result, headerRead, receiverLoan, ZR_TRUE));
    TEST_ASSERT_NOT_NULL(diagnostic_at_instruction(&fixture, headerRead));
    fixture_free(&fixture);
}

static SZrFunction *compile_source(const TZrChar *source, const TZrChar *name) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)name);
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
}

static TZrUInt32 count_instruction_opcode(
        const SZrFunction *function,
        EZrInstructionCode opcode) {
    TZrUInt32 count = 0U;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0U;
    }
    for (TZrUInt32 index = 0U; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index]
                    .instruction.operationCode == opcode) {
            count++;
        }
    }
    return count;
}

static const SZrFunctionFrameSlotLayout *find_inline_receiver_argument_layout(
        const SZrFunction *function) {
    if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrUInt32 index = 0U; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *layout =
                &function->frameSlotLayouts[index];
        if ((layout->reserved0 &
             ZR_FUNCTION_FRAME_SLOT_FLAG_INLINE_RECEIVER_ARGUMENT) != 0U) {
            return layout;
        }
    }
    return ZR_NULL;
}

static TZrUInt32 count_set_stack_writes_to_slot_from_slot(
        const SZrFunction *function,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot) {
    TZrUInt32 count = 0U;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0U;
    }
    for (TZrUInt32 index = 0U; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        if ((EZrInstructionCode)instruction->instruction.operationCode ==
                    ZR_INSTRUCTION_ENUM(SET_STACK) &&
            instruction->instruction.operandExtra == destinationSlot) {
            if ((TZrUInt32)instruction->instruction.operand.operand2[0] ==
                sourceSlot) {
                count++;
            }
        }
    }
    return count;
}

static void assert_inline_receiver_argument_is_borrowed(
        const SZrFunction *function) {
    const SZrFunctionFrameSlotLayout *argumentLayout =
            find_inline_receiver_argument_layout(function);
    TZrBool sharesSourcePlace = ZR_FALSE;
    TZrUInt32 sourceStackSlot = UINT32_MAX;

    TEST_ASSERT_NOT_NULL(argumentLayout);
    TEST_ASSERT_BITS_HIGH(
            ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS,
            argumentLayout->reserved0);
    for (TZrUInt32 index = 0U; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *candidate =
                &function->frameSlotLayouts[index];
        if (candidate->stackSlot != argumentLayout->stackSlot &&
            candidate->slotKind == ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
            candidate->byteOffset == argumentLayout->byteOffset &&
            candidate->byteSize == argumentLayout->byteSize &&
            candidate->typeLayoutId == argumentLayout->typeLayoutId) {
            sharesSourcePlace = ZR_TRUE;
            sourceStackSlot = candidate->stackSlot;
            break;
        }
    }
    TEST_ASSERT_TRUE(sharesSourcePlace);
    TEST_ASSERT_EQUAL_UINT32(
            0U,
            count_set_stack_writes_to_slot_from_slot(
                    function, argumentLayout->stackSlot, sourceStackSlot));
}

static void assert_readonly_receiver_parameter_is_borrowed(
        const SZrFunction *function) {
    const SZrFunctionFrameSlotLayout *receiverLayout;

    TEST_ASSERT_NOT_NULL(function);
    receiverLayout = ZrCore_Function_FindFrameSlotLayout(function, 0U);
    TEST_ASSERT_NOT_NULL(receiverLayout);
    TEST_ASSERT_TRUE(receiverLayout->isParameter);
    TEST_ASSERT_EQUAL_INT(
            ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT,
            receiverLayout->slotKind);
    TEST_ASSERT_BITS_HIGH(
            ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
                    ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
                    ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS,
            receiverLayout->reserved0);
}

static TZrUInt32 count_non_identity_semir_value_copies(
        const SZrFunction *function) {
    TZrUInt32 count = 0U;

    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return 0U;
    }
    for (TZrUInt32 index = 0U; index < function->semIrInstructionLength; index++) {
        const SZrSemIrInstruction *instruction =
                &function->semIrInstructions[index];
        if (instruction->opcode == ZR_SEMIR_OPCODE_COPY_VALUE &&
            instruction->destinationSlot != instruction->operand0) {
            count++;
        }
    }
    return count;
}

static const SZrFunction *find_named_function_recursive(
        const SZrFunction *function,
        const TZrChar *name,
        TZrUInt32 depth) {
    TZrNativeString functionName;

    if (function == ZR_NULL || name == ZR_NULL || depth > 32U) {
        return ZR_NULL;
    }
    functionName = function->functionName != ZR_NULL
                           ? ZrCore_String_GetNativeString(function->functionName)
                           : ZR_NULL;
    if (functionName != ZR_NULL && strcmp(functionName, name) == 0) {
        return function;
    }
    for (TZrUInt32 index = 0U; index < function->childFunctionLength; index++) {
        const SZrFunction *match = find_named_function_recursive(
                &function->childFunctionList[index], name, depth + 1U);
        if (match != ZR_NULL) {
            return match;
        }
    }
    for (TZrUInt32 index = 0U; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];
        const SZrFunction *candidate;
        const SZrFunction *match;

        if (constant->type != ZR_VALUE_TYPE_FUNCTION ||
            constant->value.object == ZR_NULL || constant->isNative) {
            continue;
        }
        candidate = ZR_CAST_FUNCTION(g_state, constant->value.object);
        if (candidate == function) {
            continue;
        }
        match = find_named_function_recursive(candidate, name, depth + 1U);
        if (match != ZR_NULL) {
            return match;
        }
    }
    return ZR_NULL;
}

static void test_readonly_struct_source_pipeline_freezes_contract_without_copy(void) {
    const TZrChar *source =
            "readonly struct Snapshot {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "  pub fn read(): int { return this.value; }\n"
            "}\n"
            "var snapshot: Snapshot = init Snapshot(7);\n"
            "return snapshot.read();\n";
    SZrFunction *function = compile_source(
            source, "receiver_readonly_struct_contract.zr");
    const SZrCompiledPrototypeInfo *prototype;
    const SZrCompiledMemberInfo *members;
    const SZrFunction *readFunction;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->prototypeData);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
            1U, *(const TZrUInt32 *)function->prototypeData);
    prototype = (const SZrCompiledPrototypeInfo *)(
            function->prototypeData + sizeof(TZrUInt32));
    members = (const SZrCompiledMemberInfo *)(
            (const TZrByte *)prototype + sizeof(*prototype) +
            prototype->inheritsCount * sizeof(TZrUInt32) +
            prototype->decoratorsCount * sizeof(TZrUInt32));
    TEST_ASSERT_BITS_HIGH(
            ZR_DECLARATION_MODIFIER_READONLY,
            prototype->modifierFlags);
    TEST_ASSERT_EQUAL_UINT32(3U, prototype->membersCount);
    TEST_ASSERT_TRUE(members[0].isConst);
    TEST_ASSERT_FALSE(members[1].isConst);
    TEST_ASSERT_TRUE(members[2].isConst);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U,
            count_instruction_opcode(
                    function, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)) +
                    count_instruction_opcode(
                            function,
                            ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8)) +
                    count_instruction_opcode(
                            function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)) +
                    count_instruction_opcode(
                            function, ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL)));
    TEST_ASSERT_EQUAL_UINT32(
            0U,
            count_instruction_opcode(
                    function, ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL)));
    assert_inline_receiver_argument_is_borrowed(function);
    readFunction = find_named_function_recursive(function, "read", 0U);
    assert_readonly_receiver_parameter_is_borrowed(readFunction);
    TEST_ASSERT_EQUAL_UINT32(0U, count_non_identity_semir_value_copies(function));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_readonly_struct_reference_parameters_avoid_defensive_copy(void) {
    const TZrChar *source =
            "readonly struct Snapshot {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "  pub fn read(): int { return this.value; }\n"
            "}\n"
            "fn inspectIn(value: in Snapshot): int { return value.read(); }\n"
            "fn inspectRef(value: ref readonly Snapshot): int { return value.read(); }\n"
            "var snapshot: Snapshot = init Snapshot(7);\n"
            "return inspectIn(snapshot) + inspectRef(ref snapshot);\n";
    SZrFunction *function = compile_source(
            source, "receiver_readonly_struct_reference_parameters.zr");
    const SZrFunction *inspectIn;
    const SZrFunction *inspectRef;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    inspectIn = find_named_function_recursive(function, "inspectIn", 0U);
    inspectRef = find_named_function_recursive(function, "inspectRef", 0U);
    TEST_ASSERT_NOT_NULL(inspectIn);
    TEST_ASSERT_NOT_NULL(inspectRef);
    assert_inline_receiver_argument_is_borrowed(inspectIn);
    assert_inline_receiver_argument_is_borrowed(inspectRef);
    TEST_ASSERT_EQUAL_UINT32(0U, count_non_identity_semir_value_copies(inspectIn));
    TEST_ASSERT_EQUAL_UINT32(0U, count_non_identity_semir_value_copies(inspectRef));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(14, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_readonly_struct_field_receiver_borrows_projected_place(void) {
    const TZrChar *source =
            "readonly struct Snapshot {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "  pub fn read(): int { return this.value; }\n"
            "}\n"
            "struct Holder {\n"
            "  pub var snapshot: Snapshot;\n"
            "  pub @constructor(snapshot: Snapshot) { this.snapshot = snapshot; }\n"
            "}\n"
            "var snapshot: Snapshot = init Snapshot(7);\n"
            "var holder: Holder = init Holder(snapshot);\n"
            "return holder.snapshot.read();\n";
    SZrFunction *function = compile_source(
            source, "receiver_readonly_struct_field_place.zr");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    assert_inline_receiver_argument_is_borrowed(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_mutable_struct_receiver_preserves_copy_writeback_boundary(void) {
    const TZrChar *source =
            "struct Buffer {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "  pub fn write(next: int): int { this.value = next; return this.value; }\n"
            "}\n"
            "var buffer: Buffer = init Buffer(1);\n"
            "buffer.write(9);\n"
            "return buffer.value;\n";
    SZrFunction *function = compile_source(
            source, "receiver_mutable_struct_copy_writeback.zr");
    const SZrFunction *writeFunction;
    const SZrFunctionFrameSlotLayout *receiverLayout;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NULL(find_inline_receiver_argument_layout(function));
    writeFunction = find_named_function_recursive(function, "write", 0U);
    TEST_ASSERT_NOT_NULL(writeFunction);
    receiverLayout = ZrCore_Function_FindFrameSlotLayout(writeFunction, 0U);
    TEST_ASSERT_NOT_NULL(receiverLayout);
    TEST_ASSERT_BITS_LOW(
            ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS,
            receiverLayout->reserved0);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(9, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_readonly_struct_rejects_instance_field_writes_after_construction(void) {
    const TZrChar *source =
            "readonly struct Snapshot {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "var snapshot: Snapshot = init Snapshot(7);\n"
            "snapshot.value = 8;\n";

    TEST_ASSERT_NULL(compile_source(
            source, "receiver_readonly_struct_post_init_write.zr"));
}

static void test_readonly_struct_rejects_writes_from_implicitly_readonly_method(void) {
    const TZrChar *source =
            "readonly struct Snapshot {\n"
            "  pub var value: int;\n"
            "  pub fn reset(): void { this.value = 0; }\n"
            "}\n";

    TEST_ASSERT_NULL(compile_source(
            source, "receiver_readonly_struct_method_write.zr"));
}

static void test_readonly_view_calls_const_fn_in_source_pipeline(void) {
    const TZrChar *source =
            "class Counter {\n"
            "  pub var value: int;\n"
            "  pub fn bump(delta: int): int { this.value = this.value + delta; return this.value; }\n"
            "  pub const fn read(): int { return this.value; }\n"
            "}\n"
            "var counter = new Counter();\n"
            "counter.value = 3;\n"
            "var view: readonly Counter = counter;\n"
            "return view.read();\n";
    SZrFunction *function = compile_source(
            source, "receiver_readonly_const_call.zr");
    const SZrCompiledPrototypeInfo *prototype;
    const SZrCompiledMemberInfo *members;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->prototypeData);
    TEST_ASSERT_EQUAL_UINT64(
            31U * sizeof(TZrUInt32), sizeof(SZrCompiledMemberInfo));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
            1U, *(const TZrUInt32 *)function->prototypeData);
    prototype = (const SZrCompiledPrototypeInfo *)(
            function->prototypeData + sizeof(TZrUInt32));
    members = (const SZrCompiledMemberInfo *)(
            (const TZrByte *)prototype + sizeof(*prototype) +
            prototype->inheritsCount * sizeof(TZrUInt32) +
            prototype->decoratorsCount * sizeof(TZrUInt32));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(3U, prototype->membersCount);
    TEST_ASSERT_FALSE(members[0].isConst);
    TEST_ASSERT_FALSE(members[1].isConst);
    TEST_ASSERT_TRUE(members[2].isConst);
    ZrCore_Function_Free(g_state, function);
}

static void test_const_fn_cannot_write_receiver_in_source_pipeline(void) {
    const TZrChar *source =
            "class Counter {\n"
            "  pub var value: int;\n"
            "  pub const fn reset(): void { this.value = 0; }\n"
            "}\n";
    SZrFunction *function = compile_source(
            source, "receiver_const_write_rejected.zr");

    TEST_ASSERT_NULL(function);
}

static void test_readonly_view_cannot_call_writable_member(void) {
    const TZrChar *source =
            "class Counter {\n"
            "  pub fn bump(): void {}\n"
            "}\n"
            "var counter = new Counter();\n"
            "var view: readonly Counter = counter;\n"
            "view.bump();\n";
    SZrFunction *function = compile_source(
            source, "receiver_readonly_view_write_rejected.zr");

    TEST_ASSERT_NULL(function);
}

static void test_readonly_view_property_getter_preserves_readonly_receiver(void) {
    const TZrChar *source =
            "class Score {\n"
            "  pub var stored: int;\n"
            "  pub get value: int { return this.stored; }\n"
            "  pub set value(next: int) { this.stored = next; }\n"
            "}\n"
            "var score = new Score();\n"
            "score.value = 9;\n"
            "var view: readonly Score = score;\n"
            "return view.value;\n";
    SZrFunction *function = compile_source(
            source, "receiver_readonly_property_getter.zr");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(9, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_readonly_view_property_setter_requires_writable_receiver(void) {
    const TZrChar *source =
            "class Score {\n"
            "  pub var stored: int;\n"
            "  pub get value: int { return this.stored; }\n"
            "  pub set value(next: int) { this.stored = next; }\n"
            "}\n"
            "var score = new Score();\n"
            "var view: readonly Score = score;\n"
            "view.value = 9;\n";
    SZrFunction *function = compile_source(
            source, "receiver_readonly_property_setter.zr");

    TEST_ASSERT_NULL(function);
}

static void test_readonly_override_cannot_be_strengthened_to_writable(void) {
    const TZrChar *source =
            "abstract class Base {\n"
            "  pub abstract const fn read(): int;\n"
            "}\n"
            "class Derived : Base {\n"
            "  pub override fn read(): int { return 1; }\n"
            "}\n";
    SZrFunction *function = compile_source(
            source, "receiver_override_effect_rejected.zr");

    TEST_ASSERT_NULL(function);
}

static void test_readonly_interface_contract_rejects_writable_implementation(void) {
    const TZrChar *source =
            "interface Readable { pub const fn read(): int; }\n"
            "class Device : Readable {\n"
            "  pub fn read(): int { return 1; }\n"
            "}\n";
    SZrFunction *function = compile_source(
            source, "receiver_interface_effect_rejected.zr");

    TEST_ASSERT_NULL(function);
}

static void test_compiler_two_phase_receiver_allows_readonly_argument_call(void) {
    const TZrChar *source =
            "class Buffer {\n"
            "  pub var value: int;\n"
            "  pub const fn read(): int { return this.value; }\n"
            "  pub fn push(next: int): void { this.value = next; }\n"
            "}\n"
            "var buffer = new Buffer();\n"
            "buffer.value = 1;\n"
            "buffer.push(buffer.read());\n"
            "return buffer.value;\n";
    SZrFunction *function = compile_source(
            source, "receiver_two_phase_argument_read.zr");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_compiler_two_phase_receiver_rejects_writable_argument_call(void) {
    const TZrChar *source =
            "class Buffer {\n"
            "  pub var value: int;\n"
            "  pub fn mutate(): int { this.value = this.value + 1; return this.value; }\n"
            "  pub fn push(next: int): void { this.value = next; }\n"
            "}\n"
            "var buffer = new Buffer();\n"
            "buffer.push(buffer.mutate());\n";
    SZrFunction *function = compile_source(
            source, "receiver_two_phase_argument_write.zr");

    TEST_ASSERT_NULL(function);
}

static void test_compiler_two_phase_receiver_rejects_projected_writable_argument(void) {
    const TZrChar *source =
            "class Buffer {\n"
            "  pub var value: int;\n"
            "  pub fn mutate(): int { this.value = this.value + 1; return this.value; }\n"
            "  pub fn push(next: int): void { this.value = next; }\n"
            "}\n"
            "class Holder { pub var buffer: Buffer; }\n"
            "var holder = new Holder();\n"
            "holder.buffer = new Buffer();\n"
            "holder.buffer.push(holder.buffer.mutate());\n";
    SZrFunction *function = compile_source(
            source, "receiver_two_phase_projected_argument_write.zr");

    TEST_ASSERT_NULL(function);
}

static void test_compiler_readonly_receiver_rejects_writable_argument_call(void) {
    const TZrChar *source =
            "class Buffer {\n"
            "  pub var value: int;\n"
            "  pub fn mutate(): int { this.value = this.value + 1; return this.value; }\n"
            "  pub const fn observe(next: int): int { return this.value + next; }\n"
            "}\n"
            "var buffer = new Buffer();\n"
            "return buffer.observe(buffer.mutate());\n";
    SZrFunction *function = compile_source(
            source, "receiver_readonly_argument_write.zr");

    TEST_ASSERT_NULL(function);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_const_fn_and_readonly_view_are_preserved_in_ast);
    RUN_TEST(test_readonly_struct_contextual_declaration_normalizes_instance_contracts);
    RUN_TEST(test_static_const_fn_is_rejected);
    RUN_TEST(test_top_level_const_fn_is_rejected);
    RUN_TEST(test_receiver_capability_matrix_and_owner_auto_deref);
    RUN_TEST(test_native_readonly_receiver_contract_is_serialized);
    RUN_TEST(test_builtin_array_like_descriptor_has_receiver_effect_boundary);
    RUN_TEST(test_two_phase_receiver_reserve_read_activate);
    RUN_TEST(test_reserved_receiver_rejects_direct_write_and_second_reserve);
    RUN_TEST(test_branch_activation_does_not_dominate_join);
    RUN_TEST(test_loop_backedge_preserves_maybe_active_conflict);
    RUN_TEST(test_readonly_struct_source_pipeline_freezes_contract_without_copy);
    RUN_TEST(test_readonly_struct_reference_parameters_avoid_defensive_copy);
    RUN_TEST(test_readonly_struct_field_receiver_borrows_projected_place);
    RUN_TEST(test_mutable_struct_receiver_preserves_copy_writeback_boundary);
    RUN_TEST(test_readonly_struct_rejects_instance_field_writes_after_construction);
    RUN_TEST(test_readonly_struct_rejects_writes_from_implicitly_readonly_method);
    RUN_TEST(test_readonly_view_calls_const_fn_in_source_pipeline);
    RUN_TEST(test_const_fn_cannot_write_receiver_in_source_pipeline);
    RUN_TEST(test_readonly_view_cannot_call_writable_member);
    RUN_TEST(test_readonly_view_property_getter_preserves_readonly_receiver);
    RUN_TEST(test_readonly_view_property_setter_requires_writable_receiver);
    RUN_TEST(test_readonly_override_cannot_be_strengthened_to_writable);
    RUN_TEST(test_readonly_interface_contract_rejects_writable_implementation);
    RUN_TEST(test_compiler_two_phase_receiver_allows_readonly_argument_call);
    RUN_TEST(test_compiler_two_phase_receiver_rejects_writable_argument_call);
    RUN_TEST(test_compiler_two_phase_receiver_rejects_projected_writable_argument);
    RUN_TEST(test_compiler_readonly_receiver_rejects_writable_argument_call);
    return UNITY_END();
}
