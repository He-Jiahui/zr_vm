#ifndef ZR_VM_TEST_OWNERSHIP_OPTIONAL_CALLABLE_CASES_H
#define ZR_VM_TEST_OWNERSHIP_OPTIONAL_CALLABLE_CASES_H

static void test_const_meta_call_publishes_readonly_receiver_effect(void) {
    SZrAstNode *script = parse_source(
            "resource class Service {\n"
            "    pub virtual const @call(value: int): int { return value + 10; }\n"
            "}\n");
    SZrAstNode *declaration;
    SZrAstNode *metaCall;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1u, script->data.script.statements->count);
    declaration = script->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.classDeclaration.members);
    TEST_ASSERT_EQUAL_UINT32(
            1u, declaration->data.classDeclaration.members->count);
    metaCall = declaration->data.classDeclaration.members->nodes[0];
    TEST_ASSERT_NOT_NULL(metaCall);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_META_FUNCTION, metaCall->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_METHOD_RECEIVER_CONST,
            metaCall->data.classMetaFunction.receiverModifier);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_DECLARATION_MODIFIER_VIRTUAL,
            metaCall->data.classMetaFunction.modifierFlags);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_READONLY,
            ZrParser_SyntaxCallable_ReceiverEffectFromDeclaration(metaCall));

    ZrParser_Ast_Free(g_state, script);
}

static void test_static_const_meta_call_is_rejected(void) {
    assert_parse_error(
            "class Service { pub static const @call(): int { return 1; } }",
            "static const meta function is invalid");
}

static void test_const_meta_call_rejects_receiver_mutation(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub var value: int;\n"
            "    pub const @call(next: int): int {\n"
            "        this.value = next;\n"
            "        return this.value;\n"
            "    }\n"
            "}\n"
            "return 0;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "const_meta_call_receiver_mutation_rejected.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NULL(function);
}

static void test_weak_callable_optional_and_direct_call_contracts(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub const @call(value: int): int { return value + 10; }\n"
            "}\n"
            "var sideEffects = 0;\n"
            "fn bump(): int { sideEffects = sideEffects + 1; return 1; }\n"
            "fn run(): int {\n"
            "    var liveSeed = own Service();\n"
            "    var liveShared = share(liveSeed);\n"
            "    var liveWeak = degrade(liveShared);\n"
            "    var liveResult = liveWeak?.(bump());\n"
            "    var expiredSeed = own Service();\n"
            "    var expiredShared = share(expiredSeed);\n"
            "    var expiredWeak = degrade(expiredShared);\n"
            "    drop(expiredShared);\n"
            "    var absentResult = expiredWeak?.(bump());\n"
            "    var caught = 0;\n"
            "    try { expiredWeak(bump()); }\n"
            "    catch (error: NullReferenceError) { caught = 1; }\n"
            "    if (liveResult == 11 && absentResult == null && "
            "sideEffects == 1 && caught == 1) { return 1; }\n"
            "    return 0;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    ZrParser_ToGlobalState_Register(g_state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(g_state->global));
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "weak_callable_optional_and_direct.zr");
    function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(g_state, function);
}

#endif
