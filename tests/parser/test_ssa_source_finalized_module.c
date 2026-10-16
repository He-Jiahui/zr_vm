#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "support/ssa_source_finalized_module_fixture.h"
#include "unity.h"
#include "exec_ir/source/exec_ir_source_callable_query_internal.h"
void setUp(void) { Fm_SetUp(); }
void tearDown(void) { Fm_TearDown(); }
static SZrExecIrDiagnostic version(EZrExecutionDiagnosticCode code, TZrUInt32 expected, TZrUInt32 actual) {
    SZrExecIrDiagnostic d = Fm_Error(code);
    d.expectedVersion = expected;
    d.actualVersion = actual;
    return d;
}
static SZrExecIrDiagnostic hash(EZrExecutionDiagnosticCode code, TZrUInt64 expected, TZrUInt64 actual) {
    SZrExecIrDiagnostic d = Fm_Error(code);
    d.expectedHash = expected;
    d.actualHash = actual;
    return d;
}
static SZrExecIrDiagnostic mapped(SZrExecIrDiagnostic d, unsigned role) {
    d.instructionId = role + 1;
    d.sourceId = g_fm.original.functions[0].instructions[role].sourceId;
    return d;
}
static SZrExecIrDiagnostic early(EZrExecutionDiagnosticCode code) {
    SZrExecIrDiagnostic d = {0};
    d.code = code;
    return d;
}
/* All visible facts are restored before any Unity assertion can jump to teardown. */
#define REFUSE(field, value, expected)                                                                                 \
    do {                                                                                                               \
        unsigned char saved[sizeof(field)];                                                                            \
        memcpy(saved, &(field), sizeof(field));                                                                        \
        (field) = (value);                                                                                             \
        SFmResult result = Fm_Call();                                                                                  \
        memcpy(&(field), saved, sizeof(field));                                                                        \
        SZrExecIrDiagnostic diagnostic = (expected);                                                                   \
        Fm_Refused(&result, &diagnostic);                                                                              \
    } while (0)
#define PAIR(first, a, second, b, expected)                                                                            \
    do {                                                                                                               \
        unsigned char savedFirst[sizeof(first)], savedSecond[sizeof(second)];                                          \
        memcpy(savedFirst, &(first), sizeof(first));                                                                   \
        memcpy(savedSecond, &(second), sizeof(second));                                                                \
        (first) = (a);                                                                                                 \
        (second) = (b);                                                                                                \
        SFmResult pairResult = Fm_Call();                                                                              \
        memcpy(&(first), savedFirst, sizeof(first));                                                                   \
        memcpy(&(second), savedSecond, sizeof(second));                                                                \
        SZrExecIrDiagnostic pairExpected = (expected);                                                                 \
        Fm_Refused(&pairResult, &pairExpected);                                                                        \
    } while (0)
static void test_null_arguments_reset_all_nine_fields(void) {
    for (unsigned i = 0; i < 8; ++i) {
        SFmObservation before, after;
        SZrExecIrDiagnostic d, expected = {0};
        Fm_Observe(&before);
        memset(&d, 0xA5, sizeof(d));
        TZrBool ok = source_snapshot_finalized_primitive_module(
                i == 0 ? ZR_NULL : &g_fm.semantic, i == 1 ? ZR_NULL : &g_fm.context, i == 2 ? ZR_NULL : &g_fm.script,
                i == 3 ? ZR_NULL : &g_fm.identity, i == 4 ? ZR_NULL : &g_fm.constants, g_fm.functionPool, 1,
                i == 5 ? ZR_NULL : &g_fm.original, i == 6 ? ZR_NULL : &g_fm.finalized, i == 7 ? ZR_NULL : &g_fm.output,
                &d);
        ZrTests_ExecOwner_Disarm();
        Fm_Observe(&after);
        expected.code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        TEST_ASSERT_FALSE(ok);
        Fm_Diagnostic(&expected, &d);
        Fm_Unchanged(&before, &after, ZR_TRUE);
        TEST_ASSERT_EQUAL_UINT(before.heap.attempts, after.heap.attempts);
        TEST_ASSERT_EQUAL_UINT(before.heap.frees, after.heap.frees);
    }
    TEST_ASSERT_FALSE(source_snapshot_finalized_primitive_module(ZR_NULL, &g_fm.context, &g_fm.script, &g_fm.identity,
                                                                 &g_fm.constants, g_fm.functionPool, 1, &g_fm.original,
                                                                 &g_fm.finalized, &g_fm.output, ZR_NULL));
}
static void test_finalized_module_publishes_complete_independent_owned_output(void) {
    Fm_Feature();
    Fm_Published();
}
static void test_same_function_and_partial_owner_aliases_refuse_before_traversal(void) {
    Fm_Feature();
    SZrExecIrDiagnostic d, expected = Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    SFmObservation before, after;
    Fm_Observe(&before);
    memset(&d, 0xA5, sizeof(d));
    TZrBool ok = source_snapshot_finalized_primitive_module(&g_fm.semantic, &g_fm.context, &g_fm.script, &g_fm.identity,
                                                            &g_fm.constants, g_fm.functionPool, 1, &g_fm.original,
                                                            &g_fm.original.functions[0], &g_fm.output, &d);
    ZrTests_ExecOwner_Disarm();
    Fm_Observe(&after);
    TEST_ASSERT_FALSE(ok);
    Fm_Diagnostic(&expected, &d);
    Fm_Unchanged(&before, &after, ZR_TRUE);
    REFUSE(g_fm.finalized.values, g_fm.original.functions[0].values, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    REFUSE(g_fm.finalized.sourceMaps, g_fm.original.functions[0].sourceMaps,
           Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
}
static void test_native_pool_and_owner_headers_refuse_invalid_geometry(void) {
    Fm_Feature();
    REFUSE(g_fm.finalized.valueCount, g_fm.finalized.valueCapacity + 1, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    REFUSE(g_fm.finalized.values, ZR_NULL, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    REFUSE(g_fm.finalized.valueCapacity, 0, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    REFUSE(g_fm.finalized.frameLayout->slotCount, g_fm.finalized.frameLayout->slotCapacity + 1,
           Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    REFUSE(g_fm.finalized.frameLayout->slots, ZR_NULL, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    REFUSE(g_fm.original.constantCount, g_fm.original.constantCapacity + 1, early(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    REFUSE(g_fm.original.functions, ZR_NULL, early(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
}
static void test_typed_headers_wide_counts_and_real_spare_storage_are_checked(void) {
    Fm_Feature();
    REFUSE(g_fm.original.functionCount, 0, early(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    TEST_ASSERT_TRUE(g_fm.context.canonicalTypes.capacity > g_fm.context.canonicalTypes.length);
    TEST_ASSERT_TRUE(g_fm.original.functions[0].valueCapacity > g_fm.original.functions[0].valueCount);
    REFUSE(g_fm.semantic.values.isValid, ZR_FALSE, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    REFUSE(g_fm.semantic.values.elementSize, sizeof(SZrSemanticIrValue) - 1,
           Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    REFUSE(g_fm.semantic.values.length, 3, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    REFUSE(g_fm.semantic.values.head, ZR_NULL, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    REFUSE(g_fm.semantic.values.capacity, 0, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    REFUSE(g_fm.semantic.values.capacity, SIZE_MAX, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW));
}
static void test_module_and_entry_contract_domains_are_preserved(void) {
    Fm_Feature();
    TEST_ASSERT_NOT_EQUAL_UINT64(g_fm.original.contract.signatureHash, g_fm.original.functions[0].signatureHash);
    REFUSE(g_fm.original.contract.schemaVersion, 0,
           version(ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH, ZR_EXECUTION_CONTRACT_SCHEMA_VERSION, 0));
    REFUSE(g_fm.original.contract.abiVersion, 0,
           version(ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH, ZR_EXECUTION_CONTRACT_ABI_VERSION, 0));
    REFUSE(g_fm.original.contract.logicalVersion, 0,
           version(ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH, ZR_EXECUTION_CONTRACT_LOGICAL_VERSION, 0));
    REFUSE(g_fm.original.contract.reserved0, 7, version(ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH, 0, 7));
    REFUSE(g_fm.original.contract.reserved1, 8, version(ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH, 0, 8));
    REFUSE(g_fm.original.contract.generation, 2, hash(ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION, 1, 2));
    REFUSE(g_fm.original.contract.targetToken, 0,
           version(ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, g_fm.original.moduleToken, 0));
    REFUSE(g_fm.original.contract.moduleHash, 0,
           hash(ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, g_fm.original.moduleHash, 0));
    REFUSE(g_fm.original.contract.requiredCapabilities, 1, version(ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH, 0, 1));
    REFUSE(g_fm.original.contract.declaredEffects, 1, version(ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH, 0, 1));
    REFUSE(g_fm.original.contract.layoutHash, 1, hash(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, 0, 1));
    REFUSE(g_fm.original.functions[0].contract.targetToken, 0,
           version(ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH, g_fm.original.functions[0].functionToken, 0));
    REFUSE(g_fm.original.functions[0].contract.layoutHash, 1, hash(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, 0, 1));
    REFUSE(g_fm.original.functions[0].sealed, ZR_TRUE, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_SEALED));
    REFUSE(g_fm.finalized.signatureHash, 908, hash(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 907, 908));
    REFUSE(g_fm.finalized.contract.signatureHash, 908, hash(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 907, 908));
}
static void test_source_identity_schema_ids_signature_and_profile_are_checked(void) {
    Fm_Feature();
    REFUSE(g_fm.identity.schemaVersion, 0,
           version(ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH, ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1, 0));
    REFUSE(g_fm.identity.symbolId, 0, version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 42, 0));
    REFUSE(g_fm.identity.typeId, 0, version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, FM_CALLABLE, 0));
    REFUSE(g_fm.identity.canonicalSignatureHash, 908, hash(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 907, 908));
    REFUSE(g_fm.identity.hasExplicitNoArgsI64, ZR_FALSE, version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 1, 0));
    REFUSE(g_fm.identity.parameterCount, 1, version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 0, 1));
    REFUSE(g_fm.identity.receiverFlags, 1, version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 0, 1));
    REFUSE(g_fm.identity.effectFlags, 1, version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 0, 1));
    REFUSE(g_fm.identity.returnPrimitive, ZR_VALUE_TYPE_BOOL,
           version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_BOOL));
    TZrUInt64 savedHash = g_fm.nodes[0].structuralHash;
    SZrExecIrDiagnostic witness;
    SSourceCallableFacts facts;
    g_fm.nodes[0].structuralHash = 0;
    memset(&witness, 0xA5, sizeof(witness));
    TZrBool ok = source_callable_query(&g_fm.context, FM_CALLABLE, g_fm.original.functions[0].functionToken, &facts,
                                       &witness);
    ZrTests_ExecOwner_Disarm();
    SFmResult r = Fm_Call();
    g_fm.nodes[0].structuralHash = savedHash;
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_NOT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_NONE, witness.code);
    Fm_Refused(&r, &witness);
}
static void test_live_state_unique_symbol_and_script_origin_agree(void) {
    Fm_Feature();
    REFUSE(g_fm.semantic.state, ZR_NULL, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.semantic.cfg.state, ZR_NULL, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.semantic.places.state, ZR_NULL, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.symbols[0].id, 43, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.symbols[0].kind, ZR_SEMANTIC_SYMBOL_KIND_UNKNOWN, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.symbols[0].typeId, FM_RETURN,
           version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, FM_CALLABLE, FM_RETURN));
    REFUSE(g_fm.symbols[0].astNode, &g_fm.returned, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    g_fm.symbols[1] = g_fm.symbols[0];
    REFUSE(g_fm.context.symbols.length, 2, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    memset(&g_fm.symbols[1], 0, sizeof(g_fm.symbols[1]));
    REFUSE(g_fm.returned.data.returnStatement.isReferenceReturn, ZR_TRUE, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.statementNodes[0], &g_fm.integer, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    /* Restore each child mutation before assertions, then require a healthy retry. */
    REFUSE(g_fm.script.data.script.statements, ZR_NULL, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    Fm_Feature();
    /* count0 retains the genuine nodes allocation and positive capacity. */
    REFUSE(g_fm.statements.count, 0, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    Fm_Feature();
    REFUSE(g_fm.statementNodes[0], ZR_NULL, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    Fm_Feature();
    REFUSE(g_fm.returned.data.returnStatement.expr, ZR_NULL, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    Fm_Feature();
}
static void test_origin_ranges_are_valid_contained_and_lossless(void) {
    Fm_Feature();
    REFUSE(g_fm.script.location.start.line, 0, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.integer.location.end.offset, 10, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.integer.location.end.column, 1, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.identity.declarationRange.endColumn, 11, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
#if SIZE_MAX > UINT32_MAX
    REFUSE(g_fm.script.location.end.offset, (TZrSize) UINT32_MAX + 1, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
#endif
}
static void test_semantic_cfg_place_region_and_empty_profile_are_checked(void) {
    Fm_Feature();
    REFUSE(g_fm.blocks[0].id, 1, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.blocks[0].instructionCount, 2, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.blocks[0].terminatorKind, ZR_PARSER_CFG_TERMINATOR_NONE, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.semantic.cfg.entryBlockId, 1, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.places[0].parentId, 1, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.places[0].typeId, FM_CALLABLE,
           version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, FM_RETURN, FM_CALLABLE));
    REFUSE(g_fm.regions[0].parentId, 1, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.regions[0].sourceRange.start.line, 1, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    SZrSemanticIrLocal local = {0};
    REFUSE(g_fm.semantic.locals, Fm_Array(&local, sizeof(local), 1, 1), Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
}
static void test_semantic_roles_values_facts_and_operand_bounds_agree(void) {
    Fm_Feature();
    REFUSE(g_fm.instructions[0].hasConstantPoolIndex, ZR_FALSE, version(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 1, 0));
    REFUSE(g_fm.instructions[1].resultValueId, 1, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE));
    REFUSE(g_fm.instructions[2].operandStart, 2, version(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 1, 2));
    REFUSE(g_fm.operands[0], 0, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE));
    REFUSE(g_fm.values[0].facts.typeId, FM_CALLABLE,
           version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, FM_RETURN, FM_CALLABLE));
    REFUSE(g_fm.instructions[1].typeId, FM_CALLABLE,
           version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, FM_RETURN, FM_CALLABLE));
    REFUSE(g_fm.values[0].facts.ownership, ZR_SEMANTIC_VALUE_OWNERSHIP_UNKNOWN,
           Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
    REFUSE(g_fm.instructions[1].symbolId, 42, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
}
static void test_keyed_source_maps_counts_ids_and_ranges_agree(void) {
    Fm_Feature();
    REFUSE(g_fm.semantic.sourceMap.length, 2, version(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 3, 2));
    REFUSE(g_fm.maps[1].instructionId, 1, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE));
    REFUSE(g_fm.maps[0].sourceRange.end.column, 7, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE));
    REFUSE(g_fm.finalized.sourceMapCount, 2, version(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 3, 2));
    REFUSE(g_fm.finalized.sourceMaps[1].endOffset, 7, mapped(Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE), 1));
    REFUSE(g_fm.finalized.instructions[1].sourceId, 3, mapped(Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE), 1));
}
static void test_dual_constant_counts_and_index_are_checked_before_reads(void) {
    Fm_Feature();
    REFUSE(g_fm.constants.length, 0, hash(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0, 1));
    REFUSE(g_fm.instructions[0].constantPoolIndex, 1, mapped(version(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 1, 1), 0));
    SZrExecIrDiagnostic d, expected = Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    SFmObservation before, after;
    Fm_Observe(&before);
    TZrBool ok = source_snapshot_finalized_primitive_module(&g_fm.semantic, &g_fm.context, &g_fm.script, &g_fm.identity,
                                                            &g_fm.constants, ZR_NULL, 1, &g_fm.original,
                                                            &g_fm.finalized, &g_fm.output, &d);
    ZrTests_ExecOwner_Disarm();
    Fm_Observe(&after);
    TEST_ASSERT_FALSE(ok);
    Fm_Diagnostic(&expected, &d);
    Fm_Unchanged(&before, &after, ZR_TRUE);
}
static void test_constant_canonical_types_flags_and_pool_tags_agree(void) {
    Fm_Feature();
    REFUSE(g_fm.compilerPool[0].type, ZR_VALUE_TYPE_BOOL,
           mapped(version(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_BOOL), 0));
    REFUSE(g_fm.functionPool[0].type, ZR_VALUE_TYPE_BOOL,
           mapped(version(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_BOOL), 0));
    REFUSE(g_fm.original.constants[0].flags, 1, mapped(version(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0, 1), 0));
    REFUSE(g_fm.original.layouts[0].typeToken, FM_CALLABLE,
           version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, FM_RETURN, FM_CALLABLE));
    REFUSE(g_fm.original.layouts[0].layoutHash, 0, hash(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, 1, 0));
    REFUSE(g_fm.original.layouts[0].byteAlign, 3, version(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, 0, 3));
}
static void test_negative_minimum_and_mutated_dual_pool_bits_are_checked(void) {
    Fm_Feature();
    TZrInt64 values[] = {-1, INT64_MIN, 8};
    for (unsigned i = 0; i < ZR_ARRAY_COUNT(values); ++i) {
        Fm_Bits(values[i]);
        Fm_Feature();
        TZrUInt64 bits = (TZrUInt64) values[i];
        REFUSE(g_fm.functionPool[0].value.nativeObject.nativeInt64, 7,
               mapped(hash(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, bits, 7), 0));
        REFUSE(g_fm.original.constants[0].bits, 7, mapped(hash(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, bits, 7), 0));
    }
}
static void test_only_dead_place_to_nop_transition_is_accepted(void) {
    Fm_Feature();
    REFUSE(g_fm.finalized.instructions[1].opcode, ZR_EXEC_IR_OPCODE_RETURN,
           mapped(Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE), 1));
    REFUSE(g_fm.finalized.instructions[0].layoutId, 1, mapped(Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE), 0));
    REFUSE(g_fm.finalized.instructions[2].layoutId, 1, mapped(Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE), 2));
    REFUSE(g_fm.finalized.instructions[1].flags, 1, Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
}
static void test_retained_value_address_and_provenance_relationship_is_exact(void) {
    Fm_Feature();
    REFUSE(g_fm.finalized.values[0].typeToken, FM_CALLABLE,
           mapped(version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, FM_RETURN, FM_CALLABLE), 0));
    REFUSE(g_fm.finalized.values[0].nullability, ZR_EXEC_IR_NULLABILITY_UNKNOWN,
           mapped(Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE), 0));
    REFUSE(g_fm.finalized.values[0].flags, ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY,
           mapped(Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE), 0));
    REFUSE(g_fm.finalized.operandPool[0], 0, mapped(Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE), 2));
}
static void test_present_empty_maps_and_sealed_finalized_lifetime_are_preserved(void) {
    Fm_Feature();
    Fm_EmptyMaps();
    g_fm.finalized.sealed = ZR_TRUE;
    Fm_Refresh();
    Fm_Feature();
    const SZrExecIrModule *stored = ZrCore_ExecIr_OwnedModuleView(g_fm.output);
    TEST_ASSERT_NOT_NULL(stored->functions[0].stateMap);
    TEST_ASSERT_TRUE(stored->functions[0].sealed);
    TEST_ASSERT_EQUAL_UINT32(g_fm.finalized.functionToken, stored->functions[0].stateMap->functionToken);
    TEST_ASSERT_EQUAL_UINT(0, stored->functions[0].stateMap->entryCount);
    SZrOwnerContents saved;
    Fm_OutputCapture(&saved);
    Fm_ReleaseInputs();
    Fm_ClearDeclared();
    SZrOwnerContents retained;
    Fm_OutputCapture(&retained);
    TEST_ASSERT_EQUAL_UINT(saved.count, retained.count);
    TEST_ASSERT_EQUAL_MEMORY(saved.words, retained.words, saved.count * sizeof(*saved.words));
    ZrTests_ExecOwner_Equal(&saved, ZrCore_ExecIr_OwnedModuleView(g_fm.output));
}
static void test_actual_frame_failure_preserves_every_lower_diagnostic_field(void) {
    Fm_Feature();
    SZrExecIrDiagnostic witness;
    TZrUInt32 old = g_fm.finalized.frameLayout->slots[0].byteOffset;
    g_fm.finalized.frameLayout->slots[0].byteOffset = old + 8;
    memset(&witness, 0xA5, sizeof(witness));
    TZrBool ok = ZrParser_ExecIr_ValidatePrimitiveSourceFrame(&g_fm.finalized, &g_fm.context, g_fm.original.layouts, 1,
                                                              0, &witness);
    ZrTests_ExecOwner_Disarm();
    SFmResult r = Fm_Call();
    g_fm.finalized.frameLayout->slots[0].byteOffset = old;
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_NOT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_NONE, witness.code);
    Fm_Refused(&r, &witness);
}
static void test_actual_core_failure_precedes_later_source_refusals(void) {
    Fm_Feature();
    SZrExecIrDiagnostic witness;
    TZrUInt32 old = g_fm.original.functions[0].values[0].id;
    TZrUInt32 schema = g_fm.identity.schemaVersion;
    g_fm.original.functions[0].values[0].id = 0;
    g_fm.identity.schemaVersion = 0;
    memset(&witness, 0xA5, sizeof(witness));
    TZrBool ok = ZrCore_ExecIr_VerifyModule(&g_fm.original, &witness);
    ZrTests_ExecOwner_Disarm();
    SFmResult r = Fm_Call();
    g_fm.original.functions[0].values[0].id = old;
    g_fm.identity.schemaVersion = schema;
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_NOT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_NONE, witness.code);
    Fm_Refused(&r, &witness);
}
static void test_complete_candidate_copy_preserves_logical_pools_and_real_capacities(void) {
    Fm_Feature();
    Fm_Published();
    const SZrExecIrModule *m = ZrCore_ExecIr_OwnedModuleView(g_fm.output);
    TEST_ASSERT_EQUAL_UINT32(g_fm.original.constantCount, m->constantCount);
    TEST_ASSERT_EQUAL_UINT32(g_fm.original.layoutCount, m->layoutCount);
    TEST_ASSERT_EQUAL_UINT32(g_fm.finalized.sourceMapCount, m->functions[0].sourceMapCount);
    TEST_ASSERT_TRUE(m->constantCapacity >= m->constantCount);
    TEST_ASSERT_TRUE(m->layoutCapacity >= m->layoutCount);
    TEST_ASSERT_TRUE(m->functions[0].sourceMapCapacity >= m->functions[0].sourceMapCount);
    TEST_ASSERT_NOT_EQUAL_UINT32(g_fm.finalized.valueCapacity, m->functions[0].valueCapacity);
    TEST_ASSERT_NOT_EQUAL_UINT32(g_fm.finalized.frameLayout->slotCapacity, m->functions[0].frameLayout->slotCapacity);
}
static void test_null_diagnostic_and_distinct_paired_error_priority_are_exact(void) {
    Fm_Feature();
    SFmObservation before, after;
    Fm_Observe(&before);
    TEST_ASSERT_EQUAL_UINT(0, before.error);
    unsigned first = ZrTests_ExecOwner_Stats().records;
    TZrBool ok = source_snapshot_finalized_primitive_module(&g_fm.semantic, &g_fm.context, &g_fm.script, &g_fm.identity,
                                                            &g_fm.constants, g_fm.functionPool, 1, &g_fm.original,
                                                            &g_fm.finalized, &g_fm.output, ZR_NULL);
    ZrTests_ExecOwner_Disarm();
    TZrBool released = !ok || Fm_OutputReleased();
    if (ok)
        Fm_RegisterOutput(first, ZrTests_ExecOwner_Stats().records);
    Fm_Observe(&after);
    Fm_Unchanged(&before, &after, !ok);
    TEST_ASSERT_TRUE(released);
    TEST_ASSERT_TRUE_MESSAGE(ok, "actual finalized primitive source module snapshot must succeed");
    Fm_Published();
    g_fm.identity.schemaVersion = 0;
    Fm_Observe(&before);
    ok = source_snapshot_finalized_primitive_module(&g_fm.semantic, &g_fm.context, &g_fm.script, &g_fm.identity,
                                                    &g_fm.constants, g_fm.functionPool, 1, &g_fm.original,
                                                    &g_fm.finalized, &g_fm.output, ZR_NULL);
    ZrTests_ExecOwner_Disarm();
    Fm_Observe(&after);
    g_fm.identity.schemaVersion = ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1;
    Fm_Unchanged(&before, &after, ZR_TRUE);
    TEST_ASSERT_FALSE(ok);
    TZrUInt32 schema = g_fm.identity.schemaVersion;
    TZrUInt64 bits = g_fm.original.constants[0].bits;
    g_fm.identity.schemaVersion = 0;
    g_fm.original.constants[0].bits = 8;
    SFmResult r = Fm_Call();
    g_fm.identity.schemaVersion = schema;
    g_fm.original.constants[0].bits = bits;
    SZrExecIrDiagnostic expected =
            version(ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH, ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1, 0);
    Fm_Refused(&r, &expected);
    TZrTypeId type = g_fm.values[0].facts.typeId;
    g_fm.values[0].facts.typeId = FM_CALLABLE;
    g_fm.functionPool[0].type = ZR_VALUE_TYPE_BOOL;
    r = Fm_Call();
    g_fm.values[0].facts.typeId = type;
    g_fm.functionPool[0].type = ZR_VALUE_TYPE_INT64;
    expected = version(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, FM_RETURN, FM_CALLABLE);
    Fm_Refused(&r, &expected);
    PAIR(g_fm.semantic.values.elementSize, 1, g_fm.identity.schemaVersion, 0,
         Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    PAIR(g_fm.original.contract.generation, 2, g_fm.identity.schemaVersion, 0,
         hash(ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION, 1, 2));
    PAIR(g_fm.semantic.sourceMap.length, 2, g_fm.functionPool[0].type, ZR_VALUE_TYPE_BOOL,
         version(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 3, 2));
    PAIR(g_fm.maps[0].instructionId, 2, g_fm.functionPool[0].type, ZR_VALUE_TYPE_BOOL,
         Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE));
    PAIR(g_fm.functionPool[0].value.nativeObject.nativeInt64, 7, g_fm.original.constants[0].bits, 6,
         mapped(hash(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 9, 7), 0));
    PAIR(g_fm.compilerPool[0].type, ZR_VALUE_TYPE_BOOL, g_fm.functionPool[0].value.nativeObject.nativeInt64, 7,
         mapped(version(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_BOOL), 0));
    PAIR(g_fm.original.layouts[0].byteAlign, 3, g_fm.finalized.frameLayout->slots[0].byteOffset, 8,
         version(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, 0, 3));
}
static void test_old_output_borrow_and_post_source_release_lifetime_are_safe(void) {
    Fm_Feature();
    unsigned completed = 0;
    for (unsigned i = 0; i < 16; ++i) {
        if (i) {
            Fm_SetUp();
            Fm_Feature();
        }
        SFmObservation before, after;
        Fm_Observe(&before);
        TEST_ASSERT_EQUAL_UINT(0, before.error);
        TEST_ASSERT_EQUAL_UINT(0, Fm_DeferredErrors());
        TEST_ASSERT_TRUE(Fm_InputLive());
        TEST_ASSERT_TRUE(Fm_OutputLive());
        const SZrExecIrModule *borrowed = ZrCore_ExecIr_OwnedModuleView(g_fm.output);
        unsigned first = ZrTests_ExecOwner_Stats().records;
        SZrExecIrDiagnostic d, zero = {0};
        TZrBool ok = source_snapshot_finalized_primitive_module(
                &g_fm.semantic, &g_fm.context, &g_fm.script, &g_fm.identity, &g_fm.constants, g_fm.functionPool, 1,
                &g_fm.original, &borrowed->functions[0], &g_fm.output, &d);
        ZrTests_ExecOwner_Disarm();
        TZrBool released = !ok || Fm_OutputReleased();
        if (ok)
            Fm_RegisterOutput(first, ZrTests_ExecOwner_Stats().records);
        Fm_Observe(&after);
        TEST_ASSERT_TRUE(released);
        Fm_Unchanged(&before, &after, !ok);
        TEST_ASSERT_TRUE_MESSAGE(ok, "actual finalized primitive source module snapshot must succeed");
        Fm_Diagnostic(&zero, &d);
        Fm_Published();
        /* The previous borrow is deliberately never read after publication. */
        SZrOwnerContents contents, retained;
        Fm_OutputCapture(&contents);
        Fm_ReleaseInputs();
        Fm_ClearDeclared();
        Fm_OutputCapture(&retained);
        TEST_ASSERT_EQUAL_UINT(contents.count, retained.count);
        TEST_ASSERT_EQUAL_MEMORY(contents.words, retained.words, contents.count * sizeof(*contents.words));
        Fm_TearDown(); /* Exact release/balance before the next actual Reset. */
        ++completed;
    }
    TEST_ASSERT_EQUAL_UINT(16, completed);
    fprintf(stderr, "SOURCE_FINALIZED_LIFETIME cycles=%u declared_storage_cleared=%u\n", completed, completed);
}
static void test_every_measured_finalizer_allocation_failure_preserves_owners_and_retries(void) { Fm_Faults(); }
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_null_arguments_reset_all_nine_fields);
    RUN_TEST(test_finalized_module_publishes_complete_independent_owned_output);
    RUN_TEST(test_same_function_and_partial_owner_aliases_refuse_before_traversal);
    RUN_TEST(test_native_pool_and_owner_headers_refuse_invalid_geometry);
    RUN_TEST(test_typed_headers_wide_counts_and_real_spare_storage_are_checked);
    RUN_TEST(test_module_and_entry_contract_domains_are_preserved);
    RUN_TEST(test_source_identity_schema_ids_signature_and_profile_are_checked);
    RUN_TEST(test_live_state_unique_symbol_and_script_origin_agree);
    RUN_TEST(test_origin_ranges_are_valid_contained_and_lossless);
    RUN_TEST(test_semantic_cfg_place_region_and_empty_profile_are_checked);
    RUN_TEST(test_semantic_roles_values_facts_and_operand_bounds_agree);
    RUN_TEST(test_keyed_source_maps_counts_ids_and_ranges_agree);
    RUN_TEST(test_dual_constant_counts_and_index_are_checked_before_reads);
    RUN_TEST(test_constant_canonical_types_flags_and_pool_tags_agree);
    RUN_TEST(test_negative_minimum_and_mutated_dual_pool_bits_are_checked);
    RUN_TEST(test_only_dead_place_to_nop_transition_is_accepted);
    RUN_TEST(test_retained_value_address_and_provenance_relationship_is_exact);
    RUN_TEST(test_present_empty_maps_and_sealed_finalized_lifetime_are_preserved);
    RUN_TEST(test_actual_frame_failure_preserves_every_lower_diagnostic_field);
    RUN_TEST(test_actual_core_failure_precedes_later_source_refusals);
    RUN_TEST(test_complete_candidate_copy_preserves_logical_pools_and_real_capacities);
    RUN_TEST(test_null_diagnostic_and_distinct_paired_error_priority_are_exact);
    RUN_TEST(test_old_output_borrow_and_post_source_release_lifetime_are_safe);
    RUN_TEST(test_every_measured_finalizer_allocation_failure_preserves_owners_and_retries);
    return UNITY_END();
}
