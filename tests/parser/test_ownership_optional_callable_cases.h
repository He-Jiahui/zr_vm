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

static void test_named_function_optional_call_is_rejected(void) {
    for (TZrSize environmentIndex = 0u; environmentIndex < 2u;
         environmentIndex++) {
        SZrCompilerState *compiler = create_compiler_state();
        SZrAstNode *script = parse_source("callback?.(1);");
        SZrAstNode *expression = statement_expression(script, 0u);
        SZrTypeEnvironment *environment = environmentIndex == 0u
                ? compiler->typeEnv
                : compiler->compileTimeTypeEnv;
        SZrInferredType intType;
        SZrInferredType result;
        SZrArray parameterTypes;

        TEST_ASSERT_NOT_NULL(environment);
        ZrParser_InferredType_Init(g_state, &intType, ZR_VALUE_TYPE_INT64);
        ZrCore_Array_Init(
                g_state, &parameterTypes, sizeof(SZrInferredType), 1u);
        ZrCore_Array_Push(g_state, &parameterTypes, &intType);
        TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunction(
                g_state,
                environment,
                ZrCore_String_CreateFromNative(g_state, "callback"),
                &intType,
                &parameterTypes));

        ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(
                compiler, expression, &result));
        TEST_ASSERT_TRUE(compiler->hasError);
        TEST_ASSERT_NOT_NULL(compiler->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(
                compiler->errorMessage, "redundant_optional_access"));

        ZrParser_InferredType_Free(g_state, &result);
        ZrCore_Array_Free(g_state, &parameterTypes);
        ZrParser_InferredType_Free(g_state, &intType);
        ZrParser_Ast_Free(g_state, script);
        destroy_compiler_state(compiler);
    }
}

static void test_nullable_callable_variable_shadows_named_function(void) {
    for (TZrSize environmentIndex = 0u; environmentIndex < 2u;
         environmentIndex++) {
        SZrCompilerState *compiler = create_compiler_state();
        SZrAstNode *script = parse_source("callback?.(1);");
        SZrAstNode *expression = statement_expression(script, 0u);
        SZrAstNode *callSegment = postfix_segment(expression, 0u);
        SZrTypeEnvironment *functionEnvironment = environmentIndex == 0u
                ? compiler->typeEnv
                : compiler->compileTimeTypeEnv;
        const SZrReceiverGuardFact *guard;
        SZrInferredType callableType;
        SZrInferredType intType;
        SZrInferredType result;
        SZrArray parameterTypes;

        TEST_ASSERT_NOT_NULL(functionEnvironment);
        ZrParser_InferredType_Init(
                g_state, &callableType, ZR_VALUE_TYPE_FUNCTION);
        callableType.isNullable = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
                g_state,
                compiler->typeEnv,
                ZrCore_String_CreateFromNative(g_state, "callback"),
                &callableType));

        ZrParser_InferredType_Init(g_state, &intType, ZR_VALUE_TYPE_INT64);
        ZrCore_Array_Init(
                g_state, &parameterTypes, sizeof(SZrInferredType), 1u);
        ZrCore_Array_Push(g_state, &parameterTypes, &intType);
        TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunction(
                g_state,
                functionEnvironment,
                ZrCore_String_CreateFromNative(g_state, "callback"),
                &intType,
                &parameterTypes));

        ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
                compiler, expression, &result));
        TEST_ASSERT_FALSE(compiler->hasError);
        guard = ZrParser_SemanticFacts_FindReceiverGuardByNode(
                compiler->semanticContext, callSegment);
        TEST_ASSERT_NOT_NULL(guard);
        TEST_ASSERT_EQUAL_INT(ZR_RECEIVER_GUARD_NULL, guard->kind);
        TEST_ASSERT_EQUAL_INT(ZR_RECEIVER_GUARD_OPTIONAL, guard->mode);
        TEST_ASSERT_EQUAL_INT(
                ZR_RECEIVER_GUARD_RESULT_NULLABLE, guard->resultLift);
        TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)guard->chainSegmentStart);
        TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)guard->chainSegmentEnd);
        TEST_ASSERT_EQUAL_INT(
                ZR_VALUE_TYPE_FUNCTION, guard->receiverType.baseType);
        TEST_ASSERT_TRUE(guard->receiverType.isNullable);
        TEST_ASSERT_EQUAL_INT(
                ZR_VALUE_TYPE_FUNCTION, guard->guardedType.baseType);
        TEST_ASSERT_FALSE(guard->guardedType.isNullable);

        ZrParser_InferredType_Free(g_state, &result);
        ZrCore_Array_Free(g_state, &parameterTypes);
        ZrParser_InferredType_Free(g_state, &intType);
        ZrParser_InferredType_Free(g_state, &callableType);
        ZrParser_Ast_Free(g_state, script);
        destroy_compiler_state(compiler);
    }
}

static SZrFunction *ownership_optional_find_child_function(
        SZrFunction *function,
        const TZrChar *name) {
    TZrUInt32 index;

    if (function == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    if (function->functionName != ZR_NULL) {
        const TZrChar *functionName =
                ZrCore_String_GetNativeString(function->functionName);
        if (functionName != ZR_NULL && strcmp(functionName, name) == 0) {
            return function;
        }
    }
    for (index = 0u; index < function->childFunctionLength; index++) {
        SZrFunction *match = ownership_optional_find_child_function(
                &function->childFunctionList[index], name);
        if (match != ZR_NULL) {
            return match;
        }
    }
    return ZR_NULL;
}

static TZrUInt32 ownership_optional_count_direct_opcode(
        const SZrFunction *function,
        EZrInstructionCode opcode) {
    TZrUInt32 count = 0u;
    TZrUInt32 index;

    if (function == ZR_NULL) {
        return 0u;
    }
    for (index = 0u; index < function->instructionsLength; index++) {
        if (function->instructionsList[index].instruction.operationCode == opcode) {
            count++;
        }
    }
    return count;
}

static void test_weak_optional_intrinsic_named_members_use_normal_dispatch(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub const fn share(): int { return 1; }\n"
            "    pub const fn degrade(): int { return 2; }\n"
            "    pub const fn wake(): int { return 4; }\n"
            "    pub const fn intoGc(): int { return 8; }\n"
            "    pub const fn drop(): int { return 16; }\n"
            "}\n"
            "fn run(): int {\n"
            "    var seed = own Service();\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    var shareValue = weak?.share();\n"
            "    var degradeValue = weak?.degrade();\n"
            "    var wakeValue = weak?.wake();\n"
            "    var intoGcValue = weak?.intoGc();\n"
            "    var dropValue = weak?.drop();\n"
            "    var mask = 0;\n"
            "    if (shareValue == 1 && degradeValue == 2 && wakeValue == 4 &&\n"
            "        intoGcValue == 8 && dropValue == 16) { mask = mask + 1; }\n"
            "    drop(shared);\n"
            "    var expired = weak?.share();\n"
            "    if (expired == null) { mask = mask + 2; }\n"
            "    return mask;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "weak_optional_intrinsic_named_members.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    SZrFunction *runFunction;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    runFunction = ownership_optional_find_child_function(function, "run");
    TEST_ASSERT_NOT_NULL(runFunction);
    TEST_ASSERT_EQUAL_UINT32(
            1u,
            ownership_optional_count_direct_opcode(
                    runFunction, ZR_INSTRUCTION_ENUM(OWN_SHARE)));
    TEST_ASSERT_EQUAL_UINT32(
            1u,
            ownership_optional_count_direct_opcode(
                    runFunction, ZR_INSTRUCTION_ENUM(OWN_DEGRADE)));
    TEST_ASSERT_EQUAL_UINT32(
            6u,
            ownership_optional_count_direct_opcode(
                    runFunction, ZR_INSTRUCTION_ENUM(OWN_WAKE)));
    TEST_ASSERT_EQUAL_UINT32(
            0u,
            ownership_optional_count_direct_opcode(
                    runFunction, ZR_INSTRUCTION_ENUM(OWN_INTO_GC_BOX)));
    TEST_ASSERT_EQUAL_UINT32(
            7u,
            ownership_optional_count_direct_opcode(
                    runFunction, ZR_INSTRUCTION_ENUM(OWN_DROP)));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(3, result);

    ZrCore_Function_Free(g_state, function);
}

#endif
