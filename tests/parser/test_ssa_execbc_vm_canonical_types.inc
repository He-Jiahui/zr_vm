typedef enum EZrCanonicalVmMutation {
    ZR_TEST_CANONICAL_VM_VALID,
    ZR_TEST_CANONICAL_VM_UNKNOWN_VALUE,
    ZR_TEST_CANONICAL_VM_NARROW_VALUE,
    ZR_TEST_CANONICAL_VM_WRONG_CONSTANT,
    ZR_TEST_CANONICAL_VM_UNKNOWN_MATCH,
    ZR_TEST_CANONICAL_VM_MISSING_CONTEXT,
    ZR_TEST_CANONICAL_VM_MISSING_INSTRUCTIONS,
    ZR_TEST_CANONICAL_VM_INVALID_TYPE_ARRAY,
    ZR_TEST_CANONICAL_VM_EXCESSIVE_TYPE_CAPACITY
} EZrCanonicalVmMutation;

typedef struct SZrCanonicalVmReport {
    TZrBool fixtureBuilt;
    TZrBool materialized;
    TZrBool outputEmpty;
    TZrBool projectionUnchanged;
    TZrBool vmReturned;
    TZrInt64 result;
    EZrExecutionDiagnosticCode diagnosticCode;
} SZrCanonicalVmReport;

static SZrCanonicalVmReport run_canonical_vm_case(EZrCanonicalVmMutation mutation) {
    SZrCanonicalVmReport report = {0};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrSemanticContext *context = ZR_NULL;
    SZrArray savedCanonicalTypes = {0};
    SZrExecIrFunction function;
    SZrExecBcProjection projection = {0};
    SZrExecBcVmEmission emission = {0};
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrConstant constants[2] = {{0}};
    SZrExecBcInstruction beforeInstructions[4];
    SZrExecIrValue beforeValues[3];
    SZrExecIrConstant beforeConstants[2];
    SZrExecBcInstruction *savedInstructions = ZR_NULL;
    TZrTypeId intId, boolId, narrowId;
    TZrExecIrValueId left, right, predicate;
    TZrExecIrValueId operands[2];
    TZrExecIrBlockId entry;
    TZrBool rooted = ZR_FALSE;
    TZrBool functionInitialized = ZR_FALSE;
    TZrBool contextTypesChanged = ZR_FALSE;
    TZrSize index;

    if (state == ZR_NULL) goto cleanup;
    context = ZrParser_SemanticContext_New(state);
    if (context == ZR_NULL) goto cleanup;
    for (index = 0u; index < 32u; ++index) {
        if (ZrParser_Semantic_ReserveTypeId(context) == ZR_SEMANTIC_ID_INVALID)
            goto cleanup;
    }
    intId = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    boolId = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_BOOL);
    narrowId = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT8);
    if (intId == 0u || boolId == 0u || narrowId == 0u ||
        intId == ZR_VALUE_TYPE_INT64 || boolId == ZR_VALUE_TYPE_BOOL) goto cleanup;

    initialize_function(&function);
    functionInitialized = ZR_TRUE;
    entry = ZrCore_ExecIr_FunctionAddBlock(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    function.entryBlockId = entry;
    left = add_value(&function, intId);
    right = add_value(&function, intId);
    predicate = add_value(&function, boolId);
    operands[0] = left;
    operands[1] = right;
    if (entry != 1u || left == 0u || right == 0u || predicate == 0u ||
        !append_instruction(&function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                intId, 0u, 810u, ZR_NULL, 0u, &left, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(&function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                intId, 1u, 811u, ZR_NULL, 0u, &right, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(&function, entry, ZR_EXEC_IR_OPCODE_COMPARE,
                ZR_TEST_COMPARE_LESS, 0u, 812u, operands, 2u,
                &predicate, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(&function, entry, ZR_EXEC_IR_OPCODE_RETURN,
                intId, 0u, 813u, &left, 1u, ZR_NULL, 0u, ZR_NULL, 0u, ZR_NULL))
        goto cleanup;
    function.instructions[2].matchTypeToken = intId;
    constants[0].typeToken = intId;
    constants[0].bits = 42u;
    constants[1].typeToken = intId;
    constants[1].bits = 43u;
    if (!ZrParser_ExecIr_BuildProjectionWithConstants(
            &function, constants, 2u, &projection, &diagnostic)) goto cleanup;
    if (projection.instructionCount != 4u || projection.physicalSlotCount != 3u ||
        projection.constantCount != 2u) goto cleanup;
    report.fixtureBuilt = ZR_TRUE;

    if (mutation == ZR_TEST_CANONICAL_VM_UNKNOWN_VALUE)
        projection.slotValues[0].typeToken = UINT32_MAX;
    if (mutation == ZR_TEST_CANONICAL_VM_NARROW_VALUE)
        projection.slotValues[0].typeToken = narrowId;
    if (mutation == ZR_TEST_CANONICAL_VM_WRONG_CONSTANT)
        projection.constants[0].typeToken = boolId;
    if (mutation == ZR_TEST_CANONICAL_VM_UNKNOWN_MATCH)
        projection.instructions[2].matchTypeToken = UINT32_MAX;
    savedInstructions = projection.instructions;
    memcpy(beforeInstructions, savedInstructions, sizeof(beforeInstructions));
    memcpy(beforeValues, projection.slotValues, sizeof(beforeValues));
    memcpy(beforeConstants, projection.constants, sizeof(beforeConstants));
    if (mutation == ZR_TEST_CANONICAL_VM_MISSING_INSTRUCTIONS)
        projection.instructions = ZR_NULL;
    savedCanonicalTypes = context->canonicalTypes;
    if (mutation == ZR_TEST_CANONICAL_VM_INVALID_TYPE_ARRAY) {
        context->canonicalTypes.isValid = ZR_FALSE;
        contextTypesChanged = ZR_TRUE;
    }
    if (mutation == ZR_TEST_CANONICAL_VM_EXCESSIVE_TYPE_CAPACITY) {
        context->canonicalTypes.capacity = SIZE_MAX;
        contextTypesChanged = ZR_TRUE;
    }
    report.materialized =
            ZrParser_ExecBcProjection_MaterializeVmFunctionWithCanonicalTypes(
                state, &projection,
                mutation == ZR_TEST_CANONICAL_VM_MISSING_CONTEXT ? ZR_NULL : context,
                &emission, &diagnostic);
    report.diagnosticCode = diagnostic.code;
    report.outputEmpty = (TZrBool)(emission.function == ZR_NULL &&
            emission.pcMap == ZR_NULL && emission.pcMapCount == 0u);
    report.projectionUnchanged = (TZrBool)(
            memcmp(beforeInstructions, savedInstructions, sizeof(beforeInstructions)) == 0 &&
            memcmp(beforeValues, projection.slotValues, sizeof(beforeValues)) == 0 &&
            memcmp(beforeConstants, projection.constants, sizeof(beforeConstants)) == 0 &&
            projection.instructions == (mutation == ZR_TEST_CANONICAL_VM_MISSING_INSTRUCTIONS
                    ? ZR_NULL : savedInstructions));
    projection.instructions = savedInstructions;
    if (report.materialized == ZR_FALSE) goto cleanup;
    rooted = ZrCore_GarbageCollector_IgnoreObject(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(emission.function));
    if (rooted != ZR_FALSE)
        report.vmReturned = ZrTests_Runtime_Function_ExecuteExpectInt64(
                state, emission.function, &report.result);

cleanup:
    if (contextTypesChanged != ZR_FALSE)
        context->canonicalTypes = savedCanonicalTypes;
    if (rooted != ZR_FALSE)
        (void)ZrCore_GarbageCollector_UnignoreObject(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(emission.function));
    ZrParser_ExecBcVmEmission_Free(state, &emission);
    ZrParser_ExecBcProjection_Free(&projection);
    if (functionInitialized != ZR_FALSE) ZrCore_ExecIr_FreeFunction(&function);
    if (context != ZR_NULL) ZrParser_SemanticContext_Free(context);
    if (state != ZR_NULL) ZrTests_Runtime_State_Destroy(state);
    return report;
}

static void test_canonical_scalar_types_and_compare_tags_run_without_mutation(void) {
    SZrCanonicalVmReport report = run_canonical_vm_case(ZR_TEST_CANONICAL_VM_VALID);
    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.materialized);
    TEST_ASSERT_TRUE(report.projectionUnchanged);
    TEST_ASSERT_TRUE(report.vmReturned);
    TEST_ASSERT_EQUAL_INT64(42, report.result);
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, report.diagnosticCode);
}

static void test_canonical_type_errors_do_not_publish_or_modify_projection(void) {
    EZrCanonicalVmMutation mutation;
    for (mutation = ZR_TEST_CANONICAL_VM_UNKNOWN_VALUE;
         mutation <= ZR_TEST_CANONICAL_VM_EXCESSIVE_TYPE_CAPACITY; ++mutation) {
        SZrCanonicalVmReport report = run_canonical_vm_case(mutation);
        TEST_ASSERT_TRUE(report.fixtureBuilt);
        TEST_ASSERT_FALSE(report.materialized);
        TEST_ASSERT_TRUE(report.outputEmpty);
        TEST_ASSERT_TRUE(report.projectionUnchanged);
        TEST_ASSERT_NOT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_NONE, report.diagnosticCode);
    }
}
