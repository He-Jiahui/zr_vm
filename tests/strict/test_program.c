#include "zr_vm_strict/program.h"

#include <assert.h>
#include <limits.h>

static void test_verified_control_flow_round_trip(void) {
    uint8_t kinds[] = {ZR_STRICT_VALUE_I64, ZR_STRICT_VALUE_I64, ZR_STRICT_VALUE_BOOL};
    ZrStrictInstruction instructions[] = {
        {ZR_STRICT_OP_CONST, 0, 0, 0, 6},
        {ZR_STRICT_OP_CONST, 1, 0, 0, 7},
        {ZR_STRICT_OP_LT, 2, 0, 1, 0},
        {ZR_STRICT_OP_JUMP_IF_FALSE, 0, 2, 0, 6},
        {ZR_STRICT_OP_MUL, 0, 0, 1, 0},
        {ZR_STRICT_OP_RETURN, 0, 0, 0, 0},
        {ZR_STRICT_OP_HALT, 0, 0, 0, 0},
    };
    ZrStrictProgram source = {kinds, 3, instructions, 7};
    ZrStrictProgram reloaded = {0};
    ZrStrictResult result;
    uint32_t pc;
    FILE *file = tmpfile();
    assert(file);
    assert(ZrStrictProgram_Write(file, &source) == ZR_STRICT_PROGRAM_OK);
    rewind(file);
    assert(ZrStrictProgram_Read(file, &reloaded) == ZR_STRICT_PROGRAM_OK);
    assert(ZrStrictProgram_Run(&reloaded, &result, &pc) == ZR_STRICT_PROGRAM_OK);
    assert(result.kind == ZR_STRICT_VALUE_I64 && result.scalar == 42);
    assert(reloaded.kinds != source.kinds && reloaded.instructions != source.instructions);
    instructions[0].immediate = 8;
    assert(ZrStrictProgram_Run(&source, &result, &pc) == ZR_STRICT_PROGRAM_OK);
    assert(result.kind == ZR_STRICT_VALUE_VOID);
    ZrStrictProgram_Free(&reloaded);
    fclose(file);
}

static void test_unverified_programs_fail_closed(void) {
    uint8_t kinds[] = {ZR_STRICT_VALUE_I64, ZR_STRICT_VALUE_BOOL};
    ZrStrictInstruction instructions[] = {
        {ZR_STRICT_OP_JUMP_IF_FALSE, 0, 0, 0, 1},
        {ZR_STRICT_OP_RETURN, 0, 0, 0, 0},
    };
    ZrStrictProgram source = {kinds, 2, instructions, 2};
    ZrStrictProgram reloaded = {0};
    ZrStrictResult result;
    uint32_t pc;
    FILE *file = tmpfile();
    assert(file);
    assert(ZrStrictProgram_Run(&source, &result, &pc) == ZR_STRICT_PROGRAM_BAD_INSTRUCTION && pc == 0);
    instructions[0].lhs = 1;
    instructions[0].immediate = 2;
    assert(ZrStrictProgram_Verify(&source, &pc) == ZR_STRICT_PROGRAM_BAD_INSTRUCTION && pc == 0);
    instructions[0].immediate = 1;
    assert(ZrStrictProgram_Write(file, &source) == ZR_STRICT_PROGRAM_OK);
    assert(fseek(file, 8, SEEK_SET) == 0 && fputc(255, file) != EOF);
    rewind(file);
    assert(ZrStrictProgram_Read(file, &reloaded) == ZR_STRICT_PROGRAM_BAD_VERSION);
    assert(fseek(file, 8, SEEK_SET) == 0 && fputc(ZR_STRICT_PROGRAM_ABI, file) != EOF);
    assert(fseek(file, 20 + source.slotCount, SEEK_SET) == 0 && fputc(255, file) != EOF);
    rewind(file);
    assert(ZrStrictProgram_Read(file, &reloaded) == ZR_STRICT_PROGRAM_BAD_INSTRUCTION);
    assert(!reloaded.kinds && !reloaded.instructions);
    fclose(file);
}

static void test_arithmetic_fault_is_checked(void) {
    uint8_t kinds[] = {ZR_STRICT_VALUE_I64, ZR_STRICT_VALUE_I64};
    ZrStrictInstruction instructions[] = {
        {ZR_STRICT_OP_CONST, 0, 0, 0, INT64_MAX},
        {ZR_STRICT_OP_CONST, 1, 0, 0, 1},
        {ZR_STRICT_OP_ADD, 0, 0, 1, 0},
        {ZR_STRICT_OP_RETURN, 0, 0, 0, 0},
    };
    ZrStrictProgram source = {kinds, 2, instructions, 4};
    ZrStrictResult result;
    uint32_t pc;
    assert(ZrStrictProgram_Run(&source, &result, &pc) == ZR_STRICT_PROGRAM_FAULT && pc == 2);
    instructions[2].op = ZR_STRICT_OP_DIV;
    instructions[1].immediate = 0;
    assert(ZrStrictProgram_Run(&source, &result, &pc) == ZR_STRICT_PROGRAM_FAULT && pc == 2);
    instructions[0].immediate = (uint64_t)INT64_MIN;
    instructions[1].immediate = UINT64_MAX;
    assert(ZrStrictProgram_Run(&source, &result, &pc) == ZR_STRICT_PROGRAM_FAULT && pc == 2);
    instructions[2].op = ZR_STRICT_OP_MOD;
    assert(ZrStrictProgram_Run(&source, &result, &pc) == ZR_STRICT_PROGRAM_OK && result.scalar == 0);
}

int main(void) {
    test_verified_control_flow_round_trip();
    test_unverified_programs_fail_closed();
    test_arithmetic_fault_is_checked();
    return 0;
}
