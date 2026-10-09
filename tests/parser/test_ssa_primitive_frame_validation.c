#include <stdio.h>
#include <string.h>
#include "support/ssa_primitive_frame_validation_fixture.h"
#include "unity.h"
#include "zr_vm_core/exec_ir_state_map.h"
void setUp(void) { Pf_SetUp(); }
void tearDown(void) { Pf_TearDown(); }
static SZrExecIrFunction *function(void) { return &g_pfFunctions[0]; }
static SPfResult lower_first(SZrExecIrDiagnostic *expected, TZrUInt32 limit) {
    SZrExecIrDiagnostic direct;
    if (!ZrCore_ExecIr_VerifyFunction(function(), ZR_EXEC_IR_VERIFY_ALL, &direct))
        *expected = direct;
    return Pf_Call(limit);
}
static void change32(TZrUInt32 *field, TZrUInt32 value, SZrExecIrDiagnostic expected, TZrBool lower) {
    TZrUInt32 saved = *field;
    SPfResult r;
    *field = value;
    r = lower ? lower_first(&expected, 0u) : Pf_Call(0u);
    *field = saved;
    Pf_Refused(&r, &expected);
}
static void test_actual_attached_frame_is_accepted(void) {
    Pf_Feature();
    TEST_ASSERT_EQUAL_UINT32(2u, function()->frameLayout->slotCount);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, function()->contract.layoutHash);
    Pf_AttachFaults();
}
static void test_sealed_and_unsealed_frames_are_accepted(void) {
    Pf_Feature();
    TZrBool saved = function()->sealed;
    SPfResult r;
    function()->sealed = ZR_TRUE;
    r = Pf_Call(0u);
    function()->sealed = saved;
    Pf_Healthy(&r.before, &r.after);
    TEST_ASSERT_TRUE(r.success);
    SZrExecIrDiagnostic z = {0};
    Pf_Diagnostic(&z, &r.diagnostic);
    /* Existing Attach still refuses this actual sealed frame before admission. */
    function()->sealed = ZR_TRUE;
    SZrExecIrDiagnostic d;
    TZrBool ok = ZrParser_ExecIr_AttachPrimitiveSourceFrame(function(), &g_pfContext, g_pfRows, 2u, 0u, &d);
    function()->sealed = saved;
    ZrTests_ExecOwner_Disarm();
    SZrExecIrDiagnostic e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_SEALED);
    TEST_ASSERT_FALSE(ok);
    Pf_Diagnostic(&e, &d);
}
static void test_actual_clone_capacity_normalization_is_accepted(void) {
    Pf_Feature();
    SPfObservation a, b;
    SZrExecIrDiagnostic d, z = {0};
    Pf_Observe(&a);
    TZrBool ok = ZrParser_ExecIr_ValidatePrimitiveSourceFrame(&g_pfFunctions[1], &g_pfContext, g_pfRows, 2u, 0u, &d);
    ZrTests_ExecOwner_Disarm();
    Pf_Observe(&b);
    Pf_Healthy(&a, &b);
    TEST_ASSERT_TRUE(ok);
    Pf_Diagnostic(&z, &d);
    TEST_ASSERT_TRUE(g_pfFunctions[1].valueCapacity >= g_pfFunctions[1].valueCount);
    TEST_ASSERT_TRUE(function()->valueCapacity > function()->valueCount);
    TEST_ASSERT_EQUAL_UINT32(function()->valueCount, g_pfFunctions[1].valueCount);
    TEST_ASSERT_TRUE(function()->valueCapacity != g_pfFunctions[1].valueCapacity);
    TEST_ASSERT_TRUE(function()->frameLayout->slotCapacity > function()->frameLayout->slotCount);
    TEST_ASSERT_EQUAL_UINT32(function()->frameLayout->slotCount, g_pfFunctions[1].frameLayout->slotCount);
    TEST_ASSERT_TRUE(g_pfFunctions[1].frameLayout->slotCapacity >= g_pfFunctions[1].frameLayout->slotCount);
    TEST_ASSERT_TRUE(function()->frameLayout->slotCapacity != g_pfFunctions[1].frameLayout->slotCapacity);
    TEST_ASSERT_TRUE(function()->frameLayout != g_pfFunctions[1].frameLayout);
    TEST_ASSERT_TRUE(function()->frameLayout->slots != g_pfFunctions[1].frameLayout->slots);
}
static void test_null_arguments_reset_all_diagnostic_fields(void) {
    for (unsigned mask = 1u; mask < 8u; ++mask) {
        SPfObservation a, b;
        SZrExecIrDiagnostic d, e = {0};
        Pf_Observe(&a);
        memset(&d, 0xA5, sizeof(d));
        TZrBool ok = ZrParser_ExecIr_ValidatePrimitiveSourceFrame((mask & 1u) ? ZR_NULL : function(),
                                                                  (mask & 2u) ? ZR_NULL : &g_pfContext,
                                                                  (mask & 4u) ? ZR_NULL : g_pfRows, 2u, 0u, &d);
        ZrTests_ExecOwner_Disarm();
        Pf_Observe(&b);
        e.code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        Pf_Healthy(&a, &b);
        TEST_ASSERT_FALSE(ok);
        Pf_Diagnostic(&e, &d);
        TEST_ASSERT_EQUAL_UINT(a.heap.attempts, b.heap.attempts);
    }
}
static void test_null_diagnostic_preserves_validation_behavior(void) {
    Pf_Feature();
    SPfObservation a, b;
    Pf_Observe(&a);
    TZrBool ok = ZrParser_ExecIr_ValidatePrimitiveSourceFrame(function(), &g_pfContext, g_pfRows, 2u, 0u, ZR_NULL);
    ZrTests_ExecOwner_Disarm();
    Pf_Observe(&b);
    Pf_Healthy(&a, &b);
    TEST_ASSERT_TRUE(ok);
    Pf_Observe(&a);
    ok = ZrParser_ExecIr_ValidatePrimitiveSourceFrame(ZR_NULL, &g_pfContext, g_pfRows, 2u, 0u, ZR_NULL);
    ZrTests_ExecOwner_Disarm();
    Pf_Observe(&b);
    Pf_Healthy(&a, &b);
    TEST_ASSERT_FALSE(ok);
    TZrUInt64 hash = g_pfNodes[0].structuralHash;
    g_pfNodes[0].structuralHash = 0u;
    Pf_Observe(&a);
    ok = ZrParser_ExecIr_ValidatePrimitiveSourceFrame(function(), &g_pfContext, g_pfRows, 2u, 0u, ZR_NULL);
    ZrTests_ExecOwner_Disarm();
    Pf_Observe(&b);
    g_pfNodes[0].structuralHash = hash;
    Pf_Healthy(&a, &b);
    TEST_ASSERT_FALSE(ok);
}
static void test_readable_storage_headers_and_overlap_are_refused(void) {
    Pf_Feature();
    SZrExecIrDiagnostic e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    change32(&function()->valueCapacity, 1u, e, ZR_FALSE);
    change32(&function()->instructionCapacity, 2u, e, ZR_FALSE);
    change32(&function()->frameLayout->slotCapacity, 1u, e, ZR_FALSE);
    SZrArray saved = g_pfContext.canonicalTypes;
    SPfResult r;
    g_pfContext.canonicalTypes.elementSize = 1u;
    r = Pf_Call(0u);
    g_pfContext.canonicalTypes = saved;
    Pf_Refused(&r, &e);
    g_pfContext.canonicalTypes.isValid = ZR_FALSE;
    r = Pf_Call(0u);
    g_pfContext.canonicalTypes = saved;
    Pf_Refused(&r, &e);
    g_pfContext.canonicalTypes.capacity = 1u;
    r = Pf_Call(0u);
    g_pfContext.canonicalTypes = saved;
    Pf_Refused(&r, &e);
    TZrExecIrValueId *operands = function()->operands;
    function()->operands = function()->results;
    r = Pf_Call(0u);
    function()->operands = operands;
    Pf_Refused(&r, &e);
    SZrExecIrValue *values = function()->values;
    function()->values = ZR_NULL;
    r = Pf_Call(0u);
    function()->values = values;
    Pf_Refused(&r, &e);
    TZrUInt32 capacity = function()->valueCapacity;
    function()->valueCapacity = 0u;
    r = Pf_Call(0u);
    function()->valueCapacity = capacity;
    Pf_Refused(&r, &e);
    g_pfContext.canonicalTypes.head = ZR_NULL;
    r = Pf_Call(0u);
    g_pfContext.canonicalTypes = saved;
    Pf_Refused(&r, &e);
    g_pfContext.canonicalTypes.length = 0u;
    g_pfContext.canonicalTypes.capacity = 0u;
    r = Pf_Call(0u);
    g_pfContext.canonicalTypes = saved;
    Pf_Refused(&r, &e);
}
static void test_declared_canonical_ids_must_be_sorted_and_nonzero(void) {
    Pf_Feature();
    SZrExecIrDiagnostic e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    const TZrUInt32 values[3] = {0u, 113u, 112u};
    for (unsigned i = 0u; i < 3u; ++i)
        change32(&g_pfNodes[1].id, values[i], e, ZR_FALSE);
}
static void test_core_verifier_failure_precedes_later_admission(void) {
    Pf_Feature();
    SZrExecIrDiagnostic e = {0};
    TZrUInt32 saved = function()->values[0].id, flags = function()->instructions[0].flags;
    function()->values[0].id = 0u;
    function()->instructions[0].flags = 1u;
    SPfResult r = lower_first(&e, 0u);
    function()->values[0].id = saved;
    function()->instructions[0].flags = (TZrUInt16) flags;
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_EXECUTION_DIAGNOSTIC_NONE, e.code);
    Pf_Refused(&r, &e);
}
static void test_unsupported_body_profiles_are_refused(void) {
    Pf_Feature();
    SZrExecIrDiagnostic e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    change32(&function()->blocks[0].flags, function()->blocks[0].flags | ZR_EXEC_IR_BLOCK_FLAG_CLEANUP, e, ZR_TRUE);
    change32(&function()->blocks[0].flags, function()->blocks[0].flags | ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION, e, ZR_TRUE);
    change32(&function()->instructions[0].matchTypeToken, 113u, e, ZR_TRUE);
    change32(&function()->instructions[2].layoutId, 21u, e, ZR_TRUE);
    change32(&function()->instructions[0].effectIn, 1u, e, ZR_TRUE);
    change32(&function()->instructions[0].effectOut, 1u, e, ZR_TRUE);
    change32(&function()->instructions[0].deoptId, 1u, e, ZR_TRUE);
    change32(&function()->instructions[0].bindingRow, 1u, e, ZR_TRUE);
    change32(&function()->bindingRowsSchemaVersion, UINT32_MAX, e, ZR_TRUE);
    change32(&function()->instructions[0].memoryIn.count, 1u, e, ZR_TRUE);
    change32(&function()->instructions[0].memoryOut.count, 1u, e, ZR_TRUE);
    change32(&function()->instructionCount, 0u, e, ZR_TRUE);
    change32(&function()->valueCount, 0u, e, ZR_TRUE);
    TZrUInt16 opcode = function()->instructions[0].opcode;
    function()->instructions[0].opcode = ZR_EXEC_IR_OPCODE_INVALID;
    SPfResult opcodeResult = lower_first(&e, 0u);
    function()->instructions[0].opcode = opcode;
    Pf_Refused(&opcodeResult, &e);
    e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    TZrUInt16 flags = function()->instructions[0].flags;
    function()->instructions[0].flags = 1u;
    SPfResult r = lower_first(&e, 0u);
    function()->instructions[0].flags = flags;
    Pf_Refused(&r, &e);
    SZrExecIrGcMap declared = {0};
    SZrExecIrGcMap *saved = function()->gcMap;
    function()->gcMap = &declared;
    e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    r = lower_first(&e, 0u);
    function()->gcMap = saved;
    Pf_Refused(&r, &e);
    SZrExecIrGcMap zero = {0};
    TEST_ASSERT_EQUAL_MEMORY(&zero, &declared, sizeof(zero));
    /* Real readable declared side rows exercise profile guards; direct Verify retains priority. */
    struct SMetadata {
        TZrExecIrMemoryTokenId memory;
        SZrExecIrPhi phi;
        SZrExecIrPhiIncoming incoming;
        TZrExecIrValueId root, value;
        SZrExecIrDeoptState deopt;
        SZrExecIrDeoptAggregate aggregate;
        SZrExecIrDeoptAggregateField field;
        SZrExecIrBindingRow binding;
        SZrExecIrStateMap map;
        SZrExecIrStateMapEntry entry;
        TZrUInt32 owner;
    } metadata;
    memset(&metadata, 0, sizeof(metadata));
    unsigned char beforeMetadata[sizeof(metadata)];
    for (unsigned kind = 0u; kind < 13u; ++kind) {
        SZrExecIrFunction original = *function();
        e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
        memset(&metadata, 0, sizeof(metadata));
        metadata.root = 1u;
        metadata.value = 1u;
#define SIDE(pool, count, capacity, row)                                                                               \
    function()->pool = &metadata.row;                                                                                  \
    function()->count = 1u;                                                                                            \
    function()->capacity = 1u
        switch (kind) {
            case 0:
                SIDE(memoryTokenPool, memoryTokenCount, memoryTokenCapacity, memory);
                break;
            case 1:
                SIDE(phiPool, phiCount, phiCapacity, phi);
                break;
            case 2:
                SIDE(phiIncoming, phiIncomingCount, phiIncomingCapacity, incoming);
                break;
            case 3:
                SIDE(gcRoots, gcRootCount, gcRootCapacity, root);
                break;
            case 4:
                SIDE(deoptStates, deoptStateCount, deoptStateCapacity, deopt);
                break;
            case 5:
                SIDE(deoptValues, deoptValueCount, deoptValueCapacity, value);
                break;
            case 6:
                SIDE(deoptAggregates, deoptAggregateCount, deoptAggregateCapacity, aggregate);
                break;
            case 7:
                SIDE(deoptAggregateFields, deoptAggregateFieldCount, deoptAggregateFieldCapacity, field);
                break;
            case 8:
                SIDE(bindingRows, bindingRowCount, bindingRowCapacity, binding);
                break;
            default:
                function()->stateMap = &metadata.map;
                metadata.map.functionToken = function()->functionToken;
                metadata.map.signatureHash = function()->signatureHash;
                metadata.map.generation = function()->contract.generation;
                if (kind == 9u) {
                    metadata.map.entries = &metadata.entry;
                    metadata.map.entryCount = metadata.map.entryCapacity = 1u;
                } else if (kind == 10u) {
                    metadata.map.valuePool = &metadata.value;
                    metadata.map.valueCount = metadata.map.valueCapacity = 1u;
                } else if (kind == 11u) {
                    metadata.map.rootPool = &metadata.root;
                    metadata.map.rootCount = metadata.map.rootCapacity = 1u;
                } else {
                    metadata.map.ownerStatePool = &metadata.owner;
                    metadata.map.ownerStateCount = metadata.map.ownerStateCapacity = 1u;
                }
                break;
        }
#undef SIDE
        memcpy(beforeMetadata, &metadata, sizeof(metadata));
        r = lower_first(&e, 0u);
        *function() = original;
        Pf_Refused(&r, &e);
        TEST_ASSERT_EQUAL_MEMORY(beforeMetadata, &metadata, sizeof(metadata));
    }
}
static void test_invalid_and_duplicate_layout_rows_are_refused(void) {
    Pf_Feature();
    for (unsigned kind = 0u; kind < 8u; ++kind) {
        SZrExecIrLayout saved = g_pfRows[1];
        SZrExecIrDiagnostic e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
        switch (kind) {
            case 0:
                g_pfRows[1].id = 0u;
                break;
            case 1:
                g_pfRows[1].typeToken = 0u;
                break;
            case 2:
                g_pfRows[1].layoutHash = 0u;
                break;
            case 3:
                g_pfRows[1].byteSize = 0u;
                break;
            case 4:
                g_pfRows[1].byteAlign = 0u;
                break;
            case 5:
                g_pfRows[1].byteAlign = 3u;
                break;
            case 6:
                g_pfRows[1].id = g_pfRows[0].id;
                break;
            default:
                g_pfRows[1].typeToken = g_pfRows[0].typeToken;
                break;
        }
        SPfResult r = Pf_Call(0u);
        g_pfRows[1] = saved;
        Pf_Refused(&r, &e);
    }
}
static void test_value_and_declared_type_predicates_are_refused(void) {
    Pf_Feature();
    for (unsigned kind = 0u; kind < 10u; ++kind) {
        SZrExecIrValue saved = function()->values[0];
        SZrCanonicalTypeNode node = g_pfNodes[0];
        SZrExecIrLayout row = g_pfRows[0];
        SZrExecIrDiagnostic e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
        e.actualVersion = saved.id;
        e.instructionId = saved.definition;
        switch (kind) {
            case 0:
                function()->values[0].flags = ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS;
                break;
            case 1:
                function()->values[0].ownership = ZR_EXEC_IR_OWNERSHIP_UNIQUE;
                break;
            case 2:
                function()->values[0].nullability = ZR_EXEC_IR_NULLABILITY_UNKNOWN;
                break;
            case 3:
                function()->values[0].definition = 0u;
                break;
            case 4:
                function()->values[0].definition = function()->instructionCount + 1u;
                break;
            case 5:
                function()->values[0].typeToken = 271u;
                break;
            case 6:
                g_pfNodes[0].id = 112u;
                break;
            case 7:
                g_pfNodes[0].kind = ZR_CANONICAL_TYPE_ERROR;
                break;
            case 8:
                g_pfNodes[0].structuralHash = 0u;
                break;
            default:
                g_pfNodes[0].data.primitive.valueType = ZR_VALUE_TYPE_BOOL;
                break;
        }
        e.instructionId = function()->values[0].definition;
        if (e.instructionId != 0u && e.instructionId <= function()->instructionCount)
            e.sourceId = function()->instructions[e.instructionId - 1u].sourceId;
        SPfResult r = lower_first(&e, 0u);
        function()->values[0] = saved;
        g_pfNodes[0] = node;
        g_pfRows[0] = row;
        Pf_Refused(&r, &e);
    }
    SZrExecIrLayout saved = g_pfRows[0];
    g_pfRows[0].typeToken = 112u;
    SZrExecIrDiagnostic e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    e.actualVersion = function()->values[0].id;
    e.instructionId = function()->values[0].definition;
    e.sourceId = function()->instructions[e.instructionId - 1u].sourceId;
    SPfResult r = Pf_Call(0u);
    g_pfRows[0] = saved;
    Pf_Refused(&r, &e);
}
static void test_missing_frame_is_refused(void) {
    Pf_Feature();
    SZrExecIrFrameLayout *saved = function()->frameLayout;
    function()->frameLayout = ZR_NULL;
    SPfResult r = Pf_Call(0u);
    function()->frameLayout = saved;
    SZrExecIrDiagnostic e = Pf_Error(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
    e.expectedVersion = 1u;
    Pf_Refused(&r, &e);
}
static void test_each_logical_frame_scalar_mismatch_is_refused(void) {
    Pf_Feature();
    SZrExecIrFrameLayout *f = function()->frameLayout;
    TZrUInt32 *fields[9] = {&f->storageSlotCount,   &f->logicalSlotCount, &f->parameterPrefixCount,
                            &f->returnBufferOffset, &f->frameByteSize,    &f->frameByteAlign,
                            &f->parameterCount,     &f->localCount,       &f->slotCount};
    for (unsigned i = 0u; i < 9u; ++i) {
        SZrExecIrDiagnostic e = Pf_Error(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
        e.expectedVersion = *fields[i];
        e.actualVersion = i == 8u ? 1u : (*fields[i] ^ 1u);
        change32(fields[i], e.actualVersion, e, ZR_TRUE);
    }
}
static void test_frame_and_contract_hash_mismatches_are_refused(void) {
    Pf_Feature();
    for (unsigned i = 0u; i < 2u; ++i) {
        TZrUInt64 *p = i ? &function()->contract.layoutHash : &function()->frameLayout->layoutHash, saved = *p;
        SZrExecIrDiagnostic e = Pf_Error(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
        e.expectedHash = saved;
        e.actualHash = saved ^ 1u;
        *p = e.actualHash;
        SPfResult r = lower_first(&e, 0u);
        *p = saved;
        Pf_Refused(&r, &e);
    }
}
static void test_each_physical_slot_field_mismatch_is_refused(void) {
    Pf_Feature();
    for (unsigned slot = 0u; slot < 2u; ++slot)
        for (unsigned field = 0u; field < 6u; ++field) {
            SZrExecIrFrameSlot saved = function()->frameLayout->slots[slot], *s = &function()->frameLayout->slots[slot];
            SZrExecIrDiagnostic e = Pf_Error(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
            switch (field) {
                case 0:
                    e.expectedVersion = s->slotId;
                    s->slotId ^= 1u;
                    e.actualVersion = s->slotId;
                    break;
                case 1:
                    e.expectedVersion = s->byteOffset;
                    s->byteOffset ^= 1u;
                    e.actualVersion = s->byteOffset;
                    break;
                case 2:
                    e.expectedVersion = s->byteSize;
                    s->byteSize ^= 1u;
                    e.actualVersion = s->byteSize;
                    break;
                case 3:
                    e.expectedVersion = s->byteAlign;
                    s->byteAlign ^= 1u;
                    e.actualVersion = s->byteAlign;
                    break;
                case 4:
                    e.expectedVersion = s->typeToken;
                    s->typeToken ^= 1u;
                    e.actualVersion = s->typeToken;
                    break;
                default:
                    e.expectedVersion = s->kind;
                    s->kind = ZR_EXEC_IR_FRAME_SLOT_REFERENCE;
                    e.actualVersion = s->kind;
                    break;
            }
            SPfResult r = lower_first(&e, 0u);
            *s = saved;
            Pf_Refused(&r, &e);
        }
}
static void test_frame_limit_and_paired_error_order_are_exact(void) {
    Pf_Feature();
    SPfResult r = Pf_Call(function()->frameLayout->frameByteSize);
    SZrExecIrDiagnostic z = {0};
    Pf_Healthy(&r.before, &r.after);
    TEST_ASSERT_TRUE(r.success);
    Pf_Diagnostic(&z, &r.diagnostic);
    SZrExecIrPackedFrameRequest request;
    SZrExecIrPackedValue values[2];
    SZrExecIrDiagnostic direct;
    Pf_Request(&request, values, function()->frameLayout->frameByteSize - 1u);
    TZrBool lower = ZrParser_ExecIr_LayoutPackedFrame(&request, &g_pfPacked, &direct);
    ZrTests_ExecOwner_Disarm();
    ZrParser_ExecIr_PackedFrameLayoutFree(&g_pfPacked);
    r = Pf_Call(request.frameByteLimit);
    Pf_Refused(&r, &direct);
    TEST_ASSERT_FALSE(lower);
    TZrUInt32 cap = function()->valueCapacity;
    TZrUInt64 hash = function()->contract.layoutHash;
    function()->valueCapacity = 1u;
    function()->contract.layoutHash ^= 1u;
    r = Pf_Call(0u);
    function()->valueCapacity = cap;
    function()->contract.layoutHash = hash;
    SZrExecIrDiagnostic e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    Pf_Refused(&r, &e);
    TZrUInt32 row = g_pfRows[0].id;
    g_pfRows[0].id = 0u;
    function()->contract.layoutHash ^= 1u;
    r = Pf_Call(0u);
    g_pfRows[0].id = row;
    function()->contract.layoutHash = hash;
    e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    Pf_Refused(&r, &e);
    SZrCanonicalTypeNode node = g_pfNodes[0];
    SZrExecIrValue value = function()->values[0];
    TZrUInt32 definition = value.definition;
    /* Rows beat value admission with a distinct value payload. */
    row = g_pfRows[0].id;
    g_pfRows[0].id = 0u;
    g_pfNodes[0].structuralHash = 0u;
    r = Pf_Call(0u);
    g_pfRows[0].id = row;
    g_pfNodes[0] = node;
    e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    Pf_Refused(&r, &e);
    TZrUInt32 secondId = g_pfNodes[1].id;
    g_pfNodes[1].id = 113u;
    g_pfNodes[0].structuralHash = 0u;
    r = Pf_Call(0u);
    g_pfNodes[1].id = secondId;
    g_pfNodes[0] = node;
    e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    Pf_Refused(&r, &e);
    SZrExecIrFrameLayout *frame = function()->frameLayout;
    g_pfNodes[0].structuralHash = 0u;
    function()->frameLayout = ZR_NULL;
    r = Pf_Call(0u);
    g_pfNodes[0] = node;
    function()->frameLayout = frame;
    e = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    e.actualVersion = value.id;
    e.instructionId = definition;
    e.sourceId = function()->instructions[definition - 1u].sourceId;
    Pf_Refused(&r, &e);
    TZrUInt32 logical = frame->logicalSlotCount;
    TZrUInt64 frameHash = frame->layoutHash;
    frame->logicalSlotCount ^= 1u;
    frame->layoutHash ^= 1u;
    r = Pf_Call(0u);
    frame->logicalSlotCount = logical;
    frame->layoutHash = frameHash;
    e = Pf_Error(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
    e.expectedVersion = logical;
    e.actualVersion = logical ^ 1u;
    Pf_Refused(&r, &e);
    TZrUInt32 slotId = frame->slots[0].slotId;
    frame->layoutHash ^= 1u;
    frame->slots[0].slotId ^= 1u;
    r = Pf_Call(0u);
    frame->layoutHash = frameHash;
    frame->slots[0].slotId = slotId;
    e = Pf_Error(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
    e.expectedHash = frameHash;
    e.actualHash = frameHash ^ 1u;
    Pf_Refused(&r, &e);
}
static void test_repeated_validation_preserves_all_live_inputs(void) {
    Pf_Feature();
    for (unsigned i = 0u; i < 16u; ++i) {
        SPfResult r = Pf_Call(0u);
        SZrExecIrDiagnostic z = {0};
        Pf_Healthy(&r.before, &r.after);
        TEST_ASSERT_TRUE(r.success);
        Pf_Diagnostic(&z, &r.diagnostic);
    }
}
static void test_every_measured_validation_allocation_fault_cleans_up_and_retries(void) {
    Pf_Feature();
    SZrExecIrDiagnostic d, z = {0};
    SZrOwnerHeapStats start = ZrTests_ExecOwner_Stats();
    TZrBool ok = ZrCore_ExecIr_VerifyFunction(function(), ZR_EXEC_IR_VERIFY_ALL, &d);
    unsigned verify = ZrTests_ExecOwner_Stats().attempts - start.attempts;
    TEST_ASSERT_TRUE(ok);
    Pf_Diagnostic(&z, &d);
    SZrExecIrPackedFrameRequest request;
    SZrExecIrPackedValue values[2];
    Pf_Request(&request, values, 0u);
    start = ZrTests_ExecOwner_Stats();
    ok = ZrParser_ExecIr_LayoutPackedFrame(&request, &g_pfPacked, &d);
    unsigned packed = ZrTests_ExecOwner_Stats().attempts - start.attempts;
    ZrParser_ExecIr_PackedFrameLayoutFree(&g_pfPacked);
    TEST_ASSERT_TRUE(ok);
    Pf_Diagnostic(&z, &d);
    SPfResult measured = Pf_Call(0u);
    Pf_Healthy(&measured.before, &measured.after);
    TEST_ASSERT_TRUE(measured.success);
    Pf_Diagnostic(&z, &measured.diagnostic);
    unsigned count = measured.after.heap.attempts - measured.before.heap.attempts, requestCount = 0u;
    for (unsigned event = measured.before.heap.attempts; event < measured.after.heap.attempts; ++event) {
        SZrOwnerHeapEvent row = ZrTests_ExecOwner_Event(event);
        if (row.kind == 2u && row.count == 2u && row.width == sizeof(SZrExecIrPackedValue))
            ++requestCount;
    }
    TEST_ASSERT_EQUAL_UINT(1u, requestCount);
    TEST_ASSERT_EQUAL_UINT(verify + requestCount + packed, count);
    TEST_ASSERT_TRUE(count > 0u);
    SZrOwnerHeapStats end = ZrTests_ExecOwner_Stats();
    TZrUInt64 budget = 3u * (TZrUInt64) count * count;
    TEST_ASSERT_TRUE(end.records <= 32768u && end.attempts <= 65536u);
    TEST_ASSERT_TRUE(budget <= 32768u - end.records);
    TEST_ASSERT_TRUE(budget <= 65536u - end.attempts);
    for (unsigned site = 1u; site <= count; ++site) {
        SZrExecIrDiagnostic expected;
        SZrOwnerHeapEvent witness = {0};
        TZrBool lower = ZR_FALSE;
        SPfObservation a, b;
        if (site <= verify) {
            Pf_Observe(&a);
            ZrTests_ExecOwner_Arm(site);
            lower = ZrCore_ExecIr_VerifyFunction(function(), ZR_EXEC_IR_VERIFY_ALL, &expected);
            ZrTests_ExecOwner_Disarm();
            Pf_Observe(&b);
            witness = Pf_FailedEvent(a.heap.attempts, b.heap.attempts);
            Pf_Healthy(&a, &b);
            TEST_ASSERT_FALSE(lower);
        } else if (site == verify + 1u) {
            expected = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
        } else {
            Pf_Observe(&a);
            ZrTests_ExecOwner_Arm(site - verify - 1u);
            lower = ZrParser_ExecIr_LayoutPackedFrame(&request, &g_pfPacked, &expected);
            ZrTests_ExecOwner_Disarm();
            unsigned last = ZrTests_ExecOwner_Stats().attempts;
            ZrParser_ExecIr_PackedFrameLayoutFree(&g_pfPacked);
            Pf_Observe(&b);
            witness = Pf_FailedEvent(a.heap.attempts, last);
            Pf_Healthy(&a, &b);
            TEST_ASSERT_FALSE(lower);
        }
        ZrTests_ExecOwner_Arm(site);
        SPfResult failure = Pf_Call(0u);
        SZrOwnerHeapEvent actual = Pf_FailedEvent(failure.before.heap.attempts, failure.after.heap.attempts);
        Pf_Refused(&failure, &expected);
        if (site == verify + 1u) {
            TEST_ASSERT_EQUAL_UINT(2u, actual.kind);
            TEST_ASSERT_EQUAL_UINT64(2u, actual.count);
            TEST_ASSERT_EQUAL_UINT64(sizeof(SZrExecIrPackedValue), actual.width);
            TEST_ASSERT_EQUAL_UINT64(2u * sizeof(SZrExecIrPackedValue), actual.bytes);
        } else {
            TEST_ASSERT_EQUAL_UINT(witness.kind, actual.kind);
            TEST_ASSERT_EQUAL_UINT64(witness.count, actual.count);
            TEST_ASSERT_EQUAL_UINT64(witness.width, actual.width);
            TEST_ASSERT_EQUAL_UINT64(witness.bytes, actual.bytes);
        }
        SPfResult retry = Pf_Call(0u);
        Pf_Healthy(&retry.before, &retry.after);
        TEST_ASSERT_TRUE_MESSAGE(retry.success, "actual const primitive frame validation must succeed");
        Pf_Diagnostic(&z, &retry.diagnostic);
        TEST_ASSERT_EQUAL_UINT(count, retry.after.heap.attempts - retry.before.heap.attempts);
    }
    fprintf(stderr, "PRIMITIVE_FRAME_FAULT_SWEEP verify=%u request=%u packed=%u measured=%u completed=%u\n", verify,
            requestCount, packed, count, count);
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_actual_attached_frame_is_accepted);
    RUN_TEST(test_sealed_and_unsealed_frames_are_accepted);
    RUN_TEST(test_actual_clone_capacity_normalization_is_accepted);
    RUN_TEST(test_null_arguments_reset_all_diagnostic_fields);
    RUN_TEST(test_null_diagnostic_preserves_validation_behavior);
    RUN_TEST(test_readable_storage_headers_and_overlap_are_refused);
    RUN_TEST(test_declared_canonical_ids_must_be_sorted_and_nonzero);
    RUN_TEST(test_core_verifier_failure_precedes_later_admission);
    RUN_TEST(test_unsupported_body_profiles_are_refused);
    RUN_TEST(test_invalid_and_duplicate_layout_rows_are_refused);
    RUN_TEST(test_value_and_declared_type_predicates_are_refused);
    RUN_TEST(test_missing_frame_is_refused);
    RUN_TEST(test_each_logical_frame_scalar_mismatch_is_refused);
    RUN_TEST(test_frame_and_contract_hash_mismatches_are_refused);
    RUN_TEST(test_each_physical_slot_field_mismatch_is_refused);
    RUN_TEST(test_frame_limit_and_paired_error_order_are_exact);
    RUN_TEST(test_repeated_validation_preserves_all_live_inputs);
    RUN_TEST(test_every_measured_validation_allocation_fault_cleans_up_and_retries);
    return UNITY_END();
}
