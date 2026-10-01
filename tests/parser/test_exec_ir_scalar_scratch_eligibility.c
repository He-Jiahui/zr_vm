#include "exec_ir_internal.h"
#include "compiler_semantic_ir_scalar_scratch_rules.h"
#include "semantic_ir_scalar_scratch_internal.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"

#define SCRATCH_CANONICAL_TYPE_ID ((TZrTypeId)191U)

typedef struct SScalarScratchFixture {
    SZrSemanticIrFunction semantic;
    SZrParserPlace places[2];
    SZrParserPlaceProjection projections[2];
    SZrSemanticIrLocal locals[1];
    SZrSemanticIrLoanFact loans[1];
    SZrSemanticEscapeFact escapes[1];
    SZrSemanticContiguousViewFact views[1];
    SZrSemanticBoundsFact bounds[1];
    SZrSemanticIrValue values[2];
    SZrSemanticIrInstruction instructions[8];
    SZrSemanticIrScalarScratchProof proofs[2];
    SZrExecIrValue execValues[4];
    SZrExecIrFunction output;
    SZrState state;
    SZrGlobalState global;
} SScalarScratchFixture;

static void check(TZrBool condition, const char *message) {
    if (condition == ZR_FALSE) {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static SZrArray array_view(void *items, TZrSize length, TZrSize capacity,
                           TZrSize elementSize) {
    SZrArray array;
    memset(&array, 0, sizeof(array));
    array.head = (TZrBytePtr)items;
    array.length = length;
    array.capacity = capacity;
    array.elementSize = elementSize;
    array.isValid = ZR_TRUE;
    return array;
}

static void make_fixture(SScalarScratchFixture *fixture) {
    TZrUInt32 index;
    const TZrTypeId scalarType = SCRATCH_CANONICAL_TYPE_ID;

    memset(fixture, 0, sizeof(*fixture));
    fixture->semantic.symbolId = 401U;
    fixture->semantic.callableTypeId = 402U;
    fixture->semantic.places.places = array_view(
            fixture->places, 1U, 2U, sizeof(fixture->places[0]));
    fixture->semantic.values = array_view(
            fixture->values, 2U, 2U, sizeof(fixture->values[0]));
    fixture->semantic.instructions = array_view(
            fixture->instructions, 5U, 8U, sizeof(fixture->instructions[0]));
    fixture->semantic.locals = array_view(
            ZR_NULL, 0U, 0U, sizeof(fixture->locals[0]));
    fixture->semantic.loanFacts = array_view(
            ZR_NULL, 0U, 0U, sizeof(fixture->loans[0]));
    fixture->semantic.escapeFacts = array_view(
            ZR_NULL, 0U, 0U, sizeof(fixture->escapes[0]));
    fixture->semantic.contiguousViewFacts = array_view(
            ZR_NULL, 0U, 0U, sizeof(fixture->views[0]));
    fixture->semantic.boundsFacts = array_view(
            ZR_NULL, 0U, 0U, sizeof(fixture->bounds[0]));
    fixture->semantic.scalarScratchProofs = array_view(
            fixture->proofs, 1U, 2U, sizeof(fixture->proofs[0]));

    fixture->places[0].id = 1U;
    fixture->places[0].parentId = ZR_PLACE_ID_INVALID;
    fixture->places[0].typeId = scalarType;
    fixture->places[0].base.kind = ZR_PARSER_PLACE_BASE_TEMPORARY;
    fixture->places[0].base.identity = 71U;
    fixture->places[0].projections = array_view(
            &fixture->projections[0], 0U, 1U,
            sizeof(fixture->projections[0]));

    for (index = 0U; index < 2U; ++index) {
        fixture->values[index].id = index + 1U;
        fixture->values[index].typeId = scalarType;
        fixture->values[index].definitionInstructionId = index + 1U;
        fixture->instructions[index].id = index + 1U;
        fixture->instructions[index].opcode = ZR_SEMANTIC_IR_CONSTANT;
        fixture->instructions[index].typeId = scalarType;
        fixture->instructions[index].resultValueId = index + 1U;
        fixture->instructions[index].constantPoolIndex = index;
        fixture->instructions[index].hasConstantPoolIndex = ZR_TRUE;
        fixture->instructions[index].targetBlockId =
                ZR_PARSER_CFG_INVALID_BLOCK_ID;
    }
    fixture->instructions[2].id = 3U;
    fixture->instructions[2].opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    fixture->instructions[2].typeId = scalarType;
    fixture->instructions[2].placeId = 1U;
    fixture->instructions[2].targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    fixture->instructions[3].id = 4U;
    fixture->instructions[3].opcode = ZR_SEMANTIC_IR_INITIALIZE;
    fixture->instructions[3].typeId = scalarType;
    fixture->instructions[3].placeId = 1U;
    fixture->instructions[3].valueId = 1U;
    fixture->instructions[3].targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    fixture->instructions[4].id = 5U;
    fixture->instructions[4].opcode = ZR_SEMANTIC_IR_RETURN;
    fixture->instructions[4].targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;

    fixture->proofs[0].placeId = 1U;
    fixture->proofs[0].constantValueId = 1U;
    fixture->proofs[0].typeId = scalarType;
    fixture->proofs[0].valueType = ZR_VALUE_TYPE_INT64;

    fixture->execValues[0].id = 1U;
    fixture->execValues[0].typeToken = (TZrExecIrTypeToken)scalarType;
    fixture->execValues[1].id = 2U;
    fixture->execValues[1].typeToken = (TZrExecIrTypeToken)scalarType;
    fixture->execValues[2].id = 3U;
    fixture->execValues[2].typeToken = (TZrExecIrTypeToken)scalarType;
    fixture->output.values = fixture->execValues;
    fixture->output.valueCount = 3U;
}

static TZrUInt32 mark_candidate(SScalarScratchFixture *fixture) {
    SZrExecIrDiagnostic diagnostic;
    check(zr_parser_exec_ir_mark_place_values(
                  &fixture->semantic, &fixture->output, 3U, &diagnostic),
          "place eligibility rejected a well-shaped fixture");
    return fixture->execValues[2].flags;
}

static void test_unique_literal_scratch_is_promotable(void) {
    SScalarScratchFixture fixture;
    TZrUInt32 flags;
    make_fixture(&fixture);
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS) != 0U &&
              (flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) != 0U,
          "compiler-proven unique bool/i64 scratch place was not promoted");
}

static void test_missing_or_malformed_proof_fails_closed(void) {
    SScalarScratchFixture fixture;
    TZrUInt32 flags;

    make_fixture(&fixture);
    fixture.semantic.scalarScratchProofs.length = 0U;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS) != 0U &&
              (flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "temporary without proof was promoted");

    make_fixture(&fixture);
    fixture.proofs[0].valueType = ZR_VALUE_TYPE_DOUBLE;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "unknown scalar proof type was accepted");

    make_fixture(&fixture);
    fixture.proofs[0].placeId = ZR_PLACE_ID_INVALID;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "malformed scalar proof row was accepted");

    make_fixture(&fixture);
    fixture.proofs[1] = fixture.proofs[0];
    fixture.semantic.scalarScratchProofs.length = 2U;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "duplicate scalar proof rows were accepted");

    make_fixture(&fixture);
    fixture.semantic.scalarScratchProofs.capacity = SIZE_MAX;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "scalar proof array with overflowing capacity was accepted");

    make_fixture(&fixture);
    fixture.semantic.scalarScratchProofs.head = ZR_NULL;
    fixture.semantic.scalarScratchProofs.length = 0U;
    fixture.semantic.scalarScratchProofs.capacity = 1U;
    fixture.semantic.scalarScratchProofs.elementSize = 0U;
    fixture.semantic.scalarScratchProofs.isValid = ZR_FALSE;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "malformed uninitialized scalar proof array was accepted");
}

static void test_constant_type_and_pool_provenance_are_rechecked(void) {
    SScalarScratchFixture fixture;
    TZrUInt32 flags;

    make_fixture(&fixture);
    fixture.proofs[0].constantValueId = 2U;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "proof naming a different direct constant was accepted");

    make_fixture(&fixture);
    fixture.proofs[0].typeId = SCRATCH_CANONICAL_TYPE_ID + 1U;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "proof with a canonical type inconsistent with SemIR was accepted");

    make_fixture(&fixture);
    fixture.instructions[0].hasConstantPoolIndex = ZR_FALSE;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "constant with missing pool provenance was accepted");

    make_fixture(&fixture);
    fixture.execValues[0].typeToken = (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "ExecIR constant type token inconsistent with proof was accepted");

    check(!compiler_semantic_ir_scalar_scratch_literal_types_match(
                  ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_INT64, ZR_FALSE,
                  ZR_VALUE_TYPE_INT64),
          "compiler classifier accepted an out-of-range constant pool index");
    check(!compiler_semantic_ir_scalar_scratch_literal_types_match(
                  ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_INT64, ZR_TRUE,
                  ZR_VALUE_TYPE_BOOL),
          "compiler classifier accepted a pool entry with the wrong type");
    check(!compiler_semantic_ir_scalar_scratch_literal_types_match(
                  ZR_VALUE_TYPE_DOUBLE, ZR_VALUE_TYPE_DOUBLE, ZR_TRUE,
                  ZR_VALUE_TYPE_DOUBLE),
          "compiler classifier accepted an unsupported literal type");
}

static void inject_place_instruction(
        SScalarScratchFixture *fixture,
        EZrSemanticIrOpcode opcode) {
    fixture->instructions[5] = fixture->instructions[4];
    fixture->instructions[5].id = 6U;
    fixture->instructions[4].id = 5U;
    fixture->instructions[4].opcode = opcode;
    fixture->instructions[4].placeId = 1U;
    fixture->instructions[4].typeId = SCRATCH_CANONICAL_TYPE_ID;
    fixture->instructions[4].targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    fixture->semantic.instructions.length = 6U;
}

static void test_load_store_and_other_place_uses_stay_in_memory(void) {
    SScalarScratchFixture fixture;
    TZrUInt32 flags;
    SZrSemanticIrInstruction initialize;
    SZrSemanticIrInstruction returnInstruction;

    make_fixture(&fixture);
    initialize = fixture.instructions[3];
    returnInstruction = fixture.instructions[4];
    fixture.instructions[3].id = 4U;
    fixture.instructions[3].opcode = ZR_SEMANTIC_IR_LOAD;
    fixture.instructions[3].placeId = 1U;
    fixture.instructions[3].typeId =
            SCRATCH_CANONICAL_TYPE_ID;
    fixture.instructions[4] = initialize;
    fixture.instructions[4].id = 5U;
    fixture.instructions[5] = returnInstruction;
    fixture.instructions[5].id = 6U;
    fixture.semantic.instructions.length = 6U;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "load before initialization was accepted");

    make_fixture(&fixture);
    inject_place_instruction(&fixture, ZR_SEMANTIC_IR_STORE);
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "later store to scalar scratch was accepted");

    make_fixture(&fixture);
    inject_place_instruction(&fixture, ZR_SEMANTIC_IR_BORROW_SHARED);
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "other address use of scalar scratch was accepted");
}

static void test_alias_collisions_and_unknown_overlap_stay_in_memory(void) {
    SScalarScratchFixture fixture;
    TZrUInt32 flags;

    make_fixture(&fixture);
    fixture.loans[0].loanId = 1U;
    fixture.loans[0].sourcePlaceId = 1U;
    fixture.semantic.loanFacts = array_view(
            fixture.loans, 1U, 1U, sizeof(fixture.loans[0]));
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "loaned scalar scratch place was accepted");

    make_fixture(&fixture);
    fixture.escapes[0].escapeFactId = 1U;
    fixture.escapes[0].sourcePlaceId = 1U;
    fixture.semantic.escapeFacts = array_view(
            fixture.escapes, 1U, 1U, sizeof(fixture.escapes[0]));
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "escaping scalar scratch place was accepted");

    make_fixture(&fixture);
    fixture.semantic.places.places.length = 2U;
    fixture.places[1].id = 2U;
    fixture.places[1].parentId = ZR_PLACE_ID_INVALID;
    fixture.places[1].typeId = SCRATCH_CANONICAL_TYPE_ID;
    fixture.places[1].base.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    fixture.places[1].base.identity = fixture.places[0].base.identity;
    fixture.places[1].projections = array_view(
            &fixture.projections[1], 0U, 1U,
            sizeof(fixture.projections[1]));
    fixture.locals[0].symbolId = 9U;
    fixture.locals[0].placeId = 2U;
    fixture.locals[0].typeId = SCRATCH_CANONICAL_TYPE_ID;
    fixture.locals[0].isScalar = ZR_TRUE;
    fixture.semantic.locals = array_view(
            fixture.locals, 1U, 1U, sizeof(fixture.locals[0]));
    fixture.output.valueCount = 4U;
    fixture.execValues[3].id = 4U;
    fixture.execValues[3].typeToken =
            (TZrExecIrTypeToken)SCRATCH_CANONICAL_TYPE_ID;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "TEMPORARY/LOCAL root identity collision was accepted");

    make_fixture(&fixture);
    fixture.semantic.places.places.length = 2U;
    fixture.places[1].id = 2U;
    fixture.places[1].parentId = ZR_PLACE_ID_INVALID;
    fixture.places[1].typeId = SCRATCH_CANONICAL_TYPE_ID;
    fixture.places[1].base.kind = ZR_PARSER_PLACE_BASE_EXTERNAL_HANDLE;
    fixture.places[1].base.identity = 72U;
    fixture.places[1].projections = array_view(
            &fixture.projections[1], 0U, 1U,
            sizeof(fixture.projections[1]));
    fixture.output.valueCount = 4U;
    fixture.execValues[3].id = 4U;
    fixture.execValues[3].typeToken =
            (TZrExecIrTypeToken)SCRATCH_CANONICAL_TYPE_ID;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "candidate with UNKNOWN PlaceGraph overlap was accepted");

    make_fixture(&fixture);
    fixture.places[1].id = 2U;
    fixture.places[1].parentId = 1U;
    fixture.places[1].typeId = SCRATCH_CANONICAL_TYPE_ID;
    fixture.places[1].base = fixture.places[0].base;
    fixture.projections[1].kind = ZR_PARSER_PLACE_PROJECTION_FIELD;
    fixture.places[1].projections = array_view(
            &fixture.projections[1], 1U, 1U,
            sizeof(fixture.projections[1]));
    fixture.semantic.places.places.length = 2U;
    fixture.output.valueCount = 4U;
    fixture.execValues[3].id = 4U;
    fixture.execValues[3].typeToken =
            (TZrExecIrTypeToken)SCRATCH_CANONICAL_TYPE_ID;
    flags = mark_candidate(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0U,
          "projected scratch place was accepted");
}

static TZrPtr always_fail_allocator(TZrPtr userData, TZrPtr pointer,
                                    TZrSize originalSize, TZrSize newSize,
                                    TZrInt64 type) {
    (void)userData;
    (void)pointer;
    (void)originalSize;
    (void)newSize;
    (void)type;
    return ZR_NULL;
}

static void test_proof_array_growth_failure_keeps_old_rows(void) {
    SScalarScratchFixture fixture;
    SZrSemanticIrScalarScratchProof before;
    TZrBytePtr beforeHead;
    TZrSize beforeLength;
    TZrSize beforeCapacity;
    SZrSemanticIrInstruction returnInstruction;

    make_fixture(&fixture);
    fixture.semantic.places.places.length = 2U;
    fixture.places[1].id = 2U;
    fixture.places[1].parentId = ZR_PLACE_ID_INVALID;
    fixture.places[1].typeId = SCRATCH_CANONICAL_TYPE_ID;
    fixture.places[1].base.kind = ZR_PARSER_PLACE_BASE_TEMPORARY;
    fixture.places[1].base.identity = 72U;
    fixture.places[1].projections = array_view(
            &fixture.projections[1], 0U, 1U,
            sizeof(fixture.projections[1]));
    returnInstruction = fixture.instructions[4];
    fixture.instructions[4].id = 5U;
    fixture.instructions[4].opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    fixture.instructions[4].typeId = SCRATCH_CANONICAL_TYPE_ID;
    fixture.instructions[4].placeId = 2U;
    fixture.instructions[4].targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    fixture.instructions[5].id = 6U;
    fixture.instructions[5].opcode = ZR_SEMANTIC_IR_INITIALIZE;
    fixture.instructions[5].typeId = SCRATCH_CANONICAL_TYPE_ID;
    fixture.instructions[5].placeId = 2U;
    fixture.instructions[5].valueId = 2U;
    fixture.instructions[5].targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    fixture.instructions[6] = returnInstruction;
    fixture.instructions[6].id = 7U;
    fixture.semantic.instructions.length = 7U;

    fixture.semantic.scalarScratchProofs = array_view(
            fixture.proofs, 1U, 1U, sizeof(fixture.proofs[0]));
    fixture.state.global = &fixture.global;
    fixture.global.allocator = always_fail_allocator;
    fixture.semantic.state = &fixture.state;

    before = fixture.proofs[0];
    beforeHead = fixture.semantic.scalarScratchProofs.head;
    beforeLength = fixture.semantic.scalarScratchProofs.length;
    beforeCapacity = fixture.semantic.scalarScratchProofs.capacity;
    fixture.proofs[1].placeId = 2U;
    fixture.proofs[1].constantValueId = 2U;
    fixture.proofs[1].typeId = SCRATCH_CANONICAL_TYPE_ID;
    fixture.proofs[1].valueType = ZR_VALUE_TYPE_INT64;

    check(!zr_parser_semantic_ir_scalar_scratch_append_proof(
                  &fixture.semantic, &fixture.proofs[1]),
          "proof append unexpectedly succeeded when allocator returned null");
    check(fixture.semantic.scalarScratchProofs.head == beforeHead &&
              fixture.semantic.scalarScratchProofs.length == beforeLength &&
              fixture.semantic.scalarScratchProofs.capacity == beforeCapacity &&
              memcmp(&fixture.proofs[0], &before, sizeof(before)) == 0,
          "failed proof growth changed the existing proof array");
}

static void test_malformed_places_fail_without_marking_values(void) {
    SScalarScratchFixture fixture;
    SZrExecIrDiagnostic diagnostic;

    make_fixture(&fixture);
    fixture.semantic.places.places.head = ZR_NULL;
    check(!zr_parser_exec_ir_mark_place_values(
                  &fixture.semantic, &fixture.output, 3U, &diagnostic) &&
                  fixture.execValues[2].flags == 0U,
          "missing place backing must fail before marking values");

    make_fixture(&fixture);
    fixture.semantic.places.places.elementSize = 1U;
    check(!zr_parser_exec_ir_mark_place_values(
                  &fixture.semantic, &fixture.output, 3U, &diagnostic) &&
                  fixture.execValues[2].flags == 0U,
          "wrong place element size must fail before marking values");
}

int main(void) {
    test_unique_literal_scratch_is_promotable();
    test_missing_or_malformed_proof_fails_closed();
    test_constant_type_and_pool_provenance_are_rechecked();
    test_load_store_and_other_place_uses_stay_in_memory();
    test_alias_collisions_and_unknown_overlap_stay_in_memory();
    test_proof_array_growth_failure_keeps_old_rows();
    test_malformed_places_fail_without_marking_values();
    (void)puts("ExecIR scalar scratch eligibility PASS");
    return EXIT_SUCCESS;
}
