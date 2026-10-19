/* Public compilation must retain closure edges when a yield body has no ExecBC. */
static void test_yield_public_compile_preserves_zero_execbc_child_graph(void) {
    static const TZrChar source[] =
            "var iteration = import(\"zr.iteration\");\n"
            "fn gen(): zr.iteration.Iterator<int> { yield 1; yield 2; yield 3; }\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrString *sourceName = ZR_NULL;
    SZrAstNode *ast = ZR_NULL;
    SZrFunction *function = ZR_NULL;
    const SZrFunction *child = ZR_NULL;
    TZrBool stateCreated = (state != ZR_NULL);
    TZrBool sourceNamed = ZR_FALSE;
    TZrBool parsed = ZR_FALSE;
    TZrBool compiled = ZR_FALSE;
    TZrBool childPresent = ZR_FALSE;
    TZrBool childNamedGen = ZR_FALSE;
    TZrBool childExecBcEmpty = ZR_FALSE;
    TZrBool instructionStoragePresent = ZR_FALSE;
    TZrBool closureOperandsInRange = ZR_TRUE;
    TZrBool closureTargetsExactChild = ZR_TRUE;
    TZrBool graphValid = ZR_FALSE;
    TZrUInt32 childCount = 0U;
    TZrUInt32 createClosureCount = 0U;

    if (state != ZR_NULL) {
        sourceName = ZrCore_String_CreateFromNative(state, "yield_public_compile_graph.zr");
        sourceNamed = (sourceName != ZR_NULL);
        if (sourceNamed) {
            ast = ZrParser_Parse(state, source, strlen(source), sourceName);
            parsed = (ast != ZR_NULL);
        }
        if (parsed) {
            function = ZrParser_Compiler_Compile(state, ast);
            compiled = (function != ZR_NULL);
        }
    }

    if (function != ZR_NULL) {
        childCount = function->childFunctionLength;
        if (childCount == 1U && function->childFunctionList != ZR_NULL) {
            child = &function->childFunctionList[0];
            childPresent = ZR_TRUE;
            childNamedGen = ZrContainerTests_StringEqualsCString(child->functionName, "gen");
            childExecBcEmpty = (child->instructionsLength == 0U &&
                                child->instructionsList == ZR_NULL);
        }
        instructionStoragePresent = (function->instructionsList != ZR_NULL);
        if (instructionStoragePresent) {
            for (TZrUInt32 index = 0U; index < function->instructionsLength; index++) {
                const TZrInstruction *instruction = &function->instructionsList[index];
                TZrUInt32 constantIndex;
                const SZrTypeValue *constant;

                if (instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(CREATE_CLOSURE)) {
                    continue;
                }
                createClosureCount++;
                constantIndex = instruction->instruction.operand.operand1[0];
                if (function->constantValueList == ZR_NULL ||
                    constantIndex >= function->constantValueLength) {
                    closureOperandsInRange = ZR_FALSE;
                    closureTargetsExactChild = ZR_FALSE;
                    continue;
                }
                constant = &function->constantValueList[constantIndex];
                if (child == ZR_NULL || constant->type != ZR_VALUE_TYPE_FUNCTION ||
                    constant->value.object == ZR_NULL ||
                    constant->value.object->type != ZR_RAW_OBJECT_TYPE_FUNCTION ||
                    constant->value.object != &child->super) {
                    closureTargetsExactChild = ZR_FALSE;
                }
            }
        }
        graphValid = ZrCore_Function_ValidateCreateClosureTargetsInChildGraph(function);
    }

    /* Preserve observations across cleanup so a Unity failure cannot skip it. */
    if (function != ZR_NULL) {
        ZrCore_Function_Free(state, function);
    }
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    ZrContainerTests_DestroyState(state);

    TEST_ASSERT_TRUE_MESSAGE(stateCreated, "Failed to create public-compile test state");
    TEST_ASSERT_TRUE_MESSAGE(sourceNamed, "Failed to name public-compile source");
    TEST_ASSERT_TRUE_MESSAGE(parsed, "Failed to parse the three-yield generator source");
    TEST_ASSERT_TRUE_MESSAGE(compiled, "Public compilation rejected the three-yield generator");
    TEST_ASSERT_EQUAL_UINT32(1U, childCount);
    TEST_ASSERT_TRUE_MESSAGE(childPresent, "Compiled entry has no single child function");
    TEST_ASSERT_TRUE_MESSAGE(childNamedGen, "Compiled child lost the gen declaration name");
    TEST_ASSERT_TRUE_MESSAGE(childExecBcEmpty, "Yield-only child unexpectedly contains ExecBC");
    TEST_ASSERT_TRUE_MESSAGE(instructionStoragePresent, "Compiled entry has no instruction storage");
    TEST_ASSERT_TRUE_MESSAGE(createClosureCount > 0U, "Compiled entry emitted no CREATE_CLOSURE");
    TEST_ASSERT_TRUE_MESSAGE(closureOperandsInRange, "CREATE_CLOSURE constant operand is out of range");
    TEST_ASSERT_TRUE_MESSAGE(closureTargetsExactChild, "CREATE_CLOSURE does not target the exact entry child");
    TEST_ASSERT_TRUE_MESSAGE(graphValid, "Public graph validation rejected compiled closure targets");
}
