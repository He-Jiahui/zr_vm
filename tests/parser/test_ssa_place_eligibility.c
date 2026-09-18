#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/semantic_ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SPlaceFixture {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[3];
    SZrParserPlace places[2];
    SZrParserPlaceProjection projection;
    SZrSemanticIrLocal local;
    SZrSemanticIrLoanFact loan;
    SZrSemanticEscapeFact escape;
} SPlaceFixture;

static void check(TZrBool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static SZrArray input_array(void *items, TZrSize count, TZrSize width) {
    SZrArray array;
    memset(&array, 0, sizeof(array));
    array.head = (TZrBytePtr)items;
    array.length = count;
    array.capacity = count;
    array.elementSize = width;
    array.isValid = ZR_TRUE;
    return array;
}

static void make_fixture(SPlaceFixture *fixture, TZrBool projected) {
    TZrUInt32 instructionCount = projected ? 3u : 2u;
    TZrUInt32 placeCount = projected ? 2u : 1u;

    memset(fixture, 0, sizeof(*fixture));
    fixture->semantic.symbolId = 71u;
    fixture->semantic.callableTypeId = 72u;
    fixture->semantic.cfg.entryBlockId = 0u;
    fixture->semantic.cfg.blocks = input_array(
            &fixture->block, 1u, sizeof(fixture->block));
    fixture->semantic.instructions = input_array(
            fixture->instructions, instructionCount,
            sizeof(fixture->instructions[0]));
    fixture->semantic.places.places = input_array(
            fixture->places, placeCount, sizeof(fixture->places[0]));
    fixture->semantic.locals = input_array(
            &fixture->local, 1u, sizeof(fixture->local));

    fixture->block.id = 0u;
    fixture->block.kind = ZR_PARSER_CFG_BLOCK_ENTRY;
    fixture->block.instructionCount = instructionCount;

    fixture->places[0].id = 1u;
    fixture->places[0].typeId = 11u;
    fixture->places[0].base.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    fixture->places[0].base.identity = 4u;
    fixture->local.symbolId = 3u;
    fixture->local.placeId = 1u;
    fixture->local.typeId = 11u;
    fixture->local.isScalar = ZR_TRUE;

    fixture->instructions[0].id = 1u;
    fixture->instructions[0].opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    fixture->instructions[0].placeId = 1u;
    fixture->instructions[0].typeId = 11u;
    fixture->instructions[0].targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;

    if (projected) {
        fixture->projection.kind = ZR_PARSER_PLACE_PROJECTION_FIELD;
        fixture->projection.data.symbolId = 9u;
        fixture->places[1].id = 2u;
        fixture->places[1].parentId = 1u;
        fixture->places[1].typeId = 12u;
        fixture->places[1].base = fixture->places[0].base;
        fixture->places[1].projections = input_array(
                &fixture->projection, 1u, sizeof(fixture->projection));
        fixture->instructions[1].id = 2u;
        fixture->instructions[1].opcode = ZR_SEMANTIC_IR_PLACE_PROJECT;
        fixture->instructions[1].placeId = 2u;
        fixture->instructions[1].typeId = 12u;
        fixture->instructions[1].targetBlockId =
                ZR_PARSER_CFG_INVALID_BLOCK_ID;
    }

    fixture->instructions[instructionCount - 1u].id = instructionCount;
    fixture->instructions[instructionCount - 1u].opcode =
            ZR_SEMANTIC_IR_RETURN;
    fixture->instructions[instructionCount - 1u].targetBlockId =
            ZR_PARSER_CFG_INVALID_BLOCK_ID;
}

static TZrUInt32 build_root_flags(SPlaceFixture *fixture) {
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt32 flags;

    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(
                  &fixture->semantic, ZR_NULL, &output, &diagnostic),
          "valid place fixture did not build");
    check(output.valueCount >= fixture->semantic.places.places.length,
          "builder did not materialize place address values");
    flags = output.values[0].flags;
    ZrCore_ExecIr_FreeFunction(&output);
    return flags;
}

static void test_direct_scalar_local_is_promotable(void) {
    SPlaceFixture fixture;
    TZrUInt32 flags;
    make_fixture(&fixture, ZR_FALSE);
    flags = build_root_flags(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS) != 0u &&
              (flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) != 0u,
          "direct scalar local was not marked promotable");
}

static void test_parameter_and_non_scalar_stay_in_memory(void) {
    SPlaceFixture fixture;
    TZrUInt32 flags;

    make_fixture(&fixture, ZR_FALSE);
    fixture.local.isParameter = ZR_TRUE;
    fixture.places[0].base.kind = ZR_PARSER_PLACE_BASE_PARAMETER;
    flags = build_root_flags(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS) != 0u &&
              (flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0u,
          "parameter place was incorrectly marked promotable");

    make_fixture(&fixture, ZR_FALSE);
    fixture.local.isScalar = ZR_FALSE;
    flags = build_root_flags(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0u,
          "non-scalar local was incorrectly marked promotable");
}

static void test_borrowed_and_escaped_locals_stay_in_memory(void) {
    SPlaceFixture fixture;
    TZrUInt32 flags;

    make_fixture(&fixture, ZR_FALSE);
    fixture.loan.loanId = 1u;
    fixture.loan.sourcePlaceId = 1u;
    fixture.semantic.loanFacts = input_array(
            &fixture.loan, 1u, sizeof(fixture.loan));
    flags = build_root_flags(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0u,
          "address-taken local was incorrectly marked promotable");

    make_fixture(&fixture, ZR_FALSE);
    fixture.escape.escapeFactId = 1u;
    fixture.escape.sourcePlaceId = 1u;
    fixture.escape.targetEscape = ZR_SEMANTIC_ESCAPE_CALLER;
    fixture.semantic.escapeFacts = input_array(
            &fixture.escape, 1u, sizeof(fixture.escape));
    flags = build_root_flags(&fixture);
    check((flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0u,
          "escaping local was incorrectly marked promotable");
}

static void test_projection_is_an_address_but_not_a_promotable_root(void) {
    SPlaceFixture fixture;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_fixture(&fixture, ZR_TRUE);
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(
                  &fixture.semantic, ZR_NULL, &output, &diagnostic),
          "projected place fixture did not build");
    check((output.values[0].flags &
           ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0u &&
              (output.values[1].flags &
               ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS) != 0u &&
              (output.values[1].flags &
               ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) == 0u,
          "projection eligibility did not preserve the root/address split");
    ZrCore_ExecIr_FreeFunction(&output);
}

int main(void) {
    test_direct_scalar_local_is_promotable();
    test_parameter_and_non_scalar_stay_in_memory();
    test_borrowed_and_escaped_locals_stay_in_memory();
    test_projection_is_an_address_but_not_a_promotable_root();
    puts("ssa place eligibility PASS");
    return EXIT_SUCCESS;
}
