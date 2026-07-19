#include "reference_loan_nll_test_support.h"

#include <stdio.h>

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/receiver_call.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_library/native_binding.h"

SZrObject *native_metadata_make_method_entry(
        SZrState *state,
        const ZrLibMethodDescriptor *descriptor);

TZrLoanId compiler_semantic_ir_begin_receiver_call(
        SZrCompilerState *cs,
        TZrUInt32 receiverSlot,
        EZrCanonicalReceiverEffect receiverEffect,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_activate_receiver_call(
        SZrCompilerState *cs,
        TZrLoanId loanId,
        SZrFileRange sourceRange);
TZrBool compiler_semantic_ir_end_receiver_call(
        SZrCompilerState *cs,
        TZrLoanId loanId,
        SZrFileRange sourceRange);

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
    const ZrLibMethodDescriptor readonlyMethod = {
            .name = "read",
            .minArgumentCount = 0,
            .maxArgumentCount = 0,
            .returnTypeName = "int",
            .isStatic = ZR_FALSE,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER,
    };
    const ZrLibMethodDescriptor writableMethod = {
            .name = "write",
            .minArgumentCount = 0,
            .maxArgumentCount = 0,
            .returnTypeName = "void",
            .isStatic = ZR_FALSE,
    };
    SZrObject *readonlyEntry =
            native_metadata_make_method_entry(g_state, &readonlyMethod);
    SZrObject *writableEntry =
            native_metadata_make_method_entry(g_state, &writableMethod);
    const SZrTypeValue *readonlyEffect;
    const SZrTypeValue *writableEffect;

    TEST_ASSERT_NOT_NULL(readonlyEntry);
    TEST_ASSERT_NOT_NULL(writableEntry);
    readonlyEffect = ZrLib_Object_GetFieldCString(
            g_state, readonlyEntry, "isReadonlyReceiver");
    writableEffect = ZrLib_Object_GetFieldCString(
            g_state, writableEntry, "isReadonlyReceiver");
    TEST_ASSERT_NOT_NULL(readonlyEffect);
    TEST_ASSERT_NOT_NULL(writableEffect);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, readonlyEffect->type);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, writableEffect->type);
    TEST_ASSERT_TRUE(readonlyEffect->value.nativeObject.nativeBool);
    TEST_ASSERT_FALSE(writableEffect->value.nativeObject.nativeBool);
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

static SZrFunction *compile_source(const TZrChar *source, const TZrChar *name) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)name);
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
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
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
            1U, *(const TZrUInt32 *)function->prototypeData);
    prototype = (const SZrCompiledPrototypeInfo *)(
            function->prototypeData + sizeof(TZrUInt32));
    members = (const SZrCompiledMemberInfo *)(
            (const TZrByte *)prototype + sizeof(*prototype) +
            prototype->inheritsCount * sizeof(TZrUInt32) +
            prototype->decoratorsCount * sizeof(TZrUInt32));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(3U, prototype->membersCount);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_CANONICAL_RECEIVER_NONE, members[0].receiverEffect);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_CANONICAL_RECEIVER_MUTABLE, members[1].receiverEffect);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_CANONICAL_RECEIVER_READONLY, members[2].receiverEffect);
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

static void test_compiler_receiver_call_records_pre_semir_phase_order(void) {
    SZrCompilerState compiler;
    TZrLoanId loanId;
    const SZrSemanticIrFunction *function;
    TZrSize count;

    ZrParser_CompilerState_Init(&compiler, g_state);
    loanId = compiler_semantic_ir_begin_receiver_call(
            &compiler,
            7U,
            ZR_CANONICAL_RECEIVER_MUTABLE,
            test_range(1));
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_LOAN_ID_INVALID, loanId);
    TEST_ASSERT_TRUE(compiler_semantic_ir_activate_receiver_call(
            &compiler, loanId, test_range(2)));
    TEST_ASSERT_TRUE(compiler_semantic_ir_end_receiver_call(
            &compiler, loanId, test_range(3)));

    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    count = function->instructions.length;
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(5U, (TZrUInt32)count);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_IR_RESERVE_BORROW_MUT,
            ZrParser_SemanticIr_InstructionAt(function, count - 3U)->opcode);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_IR_ACTIVATE_LOAN,
            ZrParser_SemanticIr_InstructionAt(function, count - 2U)->opcode);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_IR_END_LOAN,
            ZrParser_SemanticIr_InstructionAt(function, count - 1U)->opcode);
    ZrParser_CompilerState_Free(&compiler);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_const_fn_and_readonly_view_are_preserved_in_ast);
    RUN_TEST(test_static_const_fn_is_rejected);
    RUN_TEST(test_top_level_const_fn_is_rejected);
    RUN_TEST(test_receiver_capability_matrix_and_owner_auto_deref);
    RUN_TEST(test_native_readonly_receiver_contract_is_serialized);
    RUN_TEST(test_two_phase_receiver_reserve_read_activate);
    RUN_TEST(test_reserved_receiver_rejects_direct_write_and_second_reserve);
    RUN_TEST(test_readonly_view_calls_const_fn_in_source_pipeline);
    RUN_TEST(test_const_fn_cannot_write_receiver_in_source_pipeline);
    RUN_TEST(test_readonly_view_cannot_call_writable_member);
    RUN_TEST(test_readonly_override_cannot_be_strengthened_to_writable);
    RUN_TEST(test_readonly_interface_contract_rejects_writable_implementation);
    RUN_TEST(test_compiler_receiver_call_records_pre_semir_phase_order);
    return UNITY_END();
}
