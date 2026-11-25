#include "zr_vm_strict/program.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>

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
    ZrStrictProgram source = {.kinds = kinds, .slotCount = 3, .instructions = instructions, .instructionCount = 7};
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
    ZrStrictProgram source = {.kinds = kinds, .slotCount = 2, .instructions = instructions, .instructionCount = 2};
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
    assert(fseek(file, 8, SEEK_SET) == 0 && fputc(ZR_STRICT_PROGRAM_ABI - 1, file) != EOF);
    rewind(file);
    assert(ZrStrictProgram_Read(file, &reloaded) == ZR_STRICT_PROGRAM_BAD_VERSION);
    assert(fseek(file, 8, SEEK_SET) == 0 && fputc(ZR_STRICT_PROGRAM_ABI, file) != EOF);
    assert(fseek(file, 24 + source.slotCount, SEEK_SET) == 0 && fputc(255, file) != EOF);
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
    ZrStrictProgram source = {.kinds = kinds, .slotCount = 2, .instructions = instructions, .instructionCount = 4};
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

static void test_owned_program_borrow_move_and_weak_lifecycle(void) {
    uint8_t kinds[] = {
        ZR_STRICT_VALUE_I64, ZR_STRICT_VALUE_UNIQUE, ZR_STRICT_VALUE_BORROW, ZR_STRICT_VALUE_SHARED,
        ZR_STRICT_VALUE_WEAK, ZR_STRICT_VALUE_MAYBE_SHARED, ZR_STRICT_VALUE_BOOL, ZR_STRICT_VALUE_I64
    };
    uint32_t types[] = {0, 1, 1, 1, 1, 1, 0, 0};
    ZrStrictRecordLayout layout = {.fieldCount = 1, .fields = {ZR_STRICT_VALUE_I64}, .name = "Student"};
    ZrStrictInstruction instructions[] = {
        {ZR_STRICT_OP_CONST, 0, 0, 0, 42},
        {ZR_STRICT_OP_OWN_RECORD, 1, 0, 0, 0},
        {ZR_STRICT_OP_BORROW, 2, 1, 0, 0},
        {ZR_STRICT_OP_FIELD_READ, 7, 2, 0, 0},
        {ZR_STRICT_OP_END_BORROW, 0, 2, 0, 0},
        {ZR_STRICT_OP_SHARE, 3, 1, 0, 0},
        {ZR_STRICT_OP_DEGRADE, 4, 3, 0, 0},
        {ZR_STRICT_OP_DROP, 0, 3, 0, 0},
        {ZR_STRICT_OP_WAKE, 5, 4, 0, 0},
        {ZR_STRICT_OP_IS_NULL, 6, 5, 0, 0},
        {ZR_STRICT_OP_RETURN, 0, 6, 0, 0},
    };
    ZrStrictProgram source = {.kinds = kinds, .slotCount = 8, .instructions = instructions, .instructionCount = 11,
                              .slotLayouts = types, .layouts = &layout, .layoutCount = 1};
    ZrStrictProgram loaded = {0};
    ZrStrictResult result;
    uint32_t pc;
    FILE *file = tmpfile();
    assert(file && ZrStrictProgram_Write(file, &source) == ZR_STRICT_PROGRAM_OK);
    rewind(file);
    assert(ZrStrictProgram_Read(file, &loaded) == ZR_STRICT_PROGRAM_OK);
    assert(ZrStrictProgram_Run(&loaded, &result, &pc) == ZR_STRICT_PROGRAM_OK && result.scalar == 1);
    ZrStrictProgram_Free(&loaded);
    fclose(file);

    instructions[4] = (ZrStrictInstruction){ZR_STRICT_OP_DROP, 0, 1, 0, 0};
    assert(ZrStrictProgram_Verify(&source, &pc) == ZR_STRICT_PROGRAM_BAD_INSTRUCTION && pc == 4);
    instructions[4] = (ZrStrictInstruction){ZR_STRICT_OP_END_BORROW, 0, 2, 0, 0};
    instructions[6] = (ZrStrictInstruction){ZR_STRICT_OP_FIELD_READ, 7, 1, 0, 0};
    assert(ZrStrictProgram_Verify(&source, &pc) == ZR_STRICT_PROGRAM_BAD_INSTRUCTION && pc == 6);
    instructions[0] = (ZrStrictInstruction){ZR_STRICT_OP_CONST, 1, 0, 0, 42};
    assert(ZrStrictProgram_Verify(&source, &pc) == ZR_STRICT_PROGRAM_BAD_INSTRUCTION && pc == 0);
}

static void test_ownership_joins_validate_all_incoming_paths(void) {
    uint8_t kinds[] = {ZR_STRICT_VALUE_I64, ZR_STRICT_VALUE_UNIQUE, ZR_STRICT_VALUE_BOOL, ZR_STRICT_VALUE_BORROW};
    uint32_t types[] = {0, 1, 0, 1};
    ZrStrictRecordLayout layout = {.fieldCount = 1, .fields = {ZR_STRICT_VALUE_I64}, .name = "Student"};
    ZrStrictInstruction instructions[] = {
        {ZR_STRICT_OP_CONST, 0, 0, 0, 42},
        {ZR_STRICT_OP_OWN_RECORD, 1, 0, 0, 0},
        {ZR_STRICT_OP_CONST, 2, 0, 0, 1},
        {ZR_STRICT_OP_JUMP_IF_FALSE, 0, 2, 0, 6},
        {ZR_STRICT_OP_BORROW, 3, 1, 0, 0},
        {ZR_STRICT_OP_END_BORROW, 0, 3, 0, 0},
        {ZR_STRICT_OP_FIELD_READ, 0, 1, 0, 0},
        {ZR_STRICT_OP_RETURN, 0, 0, 0, 0},
        {ZR_STRICT_OP_HALT, 0, 0, 0, 0},
    };
    ZrStrictProgram source = {.kinds = kinds, .slotCount = 4, .instructions = instructions, .instructionCount = 9,
                              .slotLayouts = types, .layouts = &layout, .layoutCount = 1};
    ZrStrictResult result;
    uint32_t pc;
    assert(ZrStrictProgram_Run(&source, &result, &pc) == ZR_STRICT_PROGRAM_OK && result.scalar == 42);
    instructions[2].immediate = 0;
    assert(ZrStrictProgram_Run(&source, &result, &pc) == ZR_STRICT_PROGRAM_OK && result.scalar == 42);
    instructions[5] = (ZrStrictInstruction){ZR_STRICT_OP_JUMP, 0, 0, 0, 6};
    assert(ZrStrictProgram_Verify(&source, &pc) == ZR_STRICT_PROGRAM_BAD_INSTRUCTION && pc == 6);
    instructions[5] = (ZrStrictInstruction){ZR_STRICT_OP_END_BORROW, 0, 3, 0, 0};
    instructions[7] = (ZrStrictInstruction){ZR_STRICT_OP_HALT, 0, 0, 0, 0};
    instructions[8] = (ZrStrictInstruction){ZR_STRICT_OP_BORROW, 3, UINT32_MAX, 0, 0};
    /* Keep the malformed operation unreachable but in the executable. */
    ZrStrictInstruction unreachable[] = {
        instructions[0], instructions[1], instructions[2], instructions[3], instructions[4], instructions[5],
        instructions[6], instructions[7], instructions[8], {ZR_STRICT_OP_HALT, 0, 0, 0, 0},
    };
    source.instructions = unreachable;
    source.instructionCount = 10;
    assert(ZrStrictProgram_Verify(&source, &pc) == ZR_STRICT_PROGRAM_BAD_INSTRUCTION && pc == 8);
}

typedef struct FaultAllocator { unsigned attempts, frees; } FaultAllocator;
static void *fail_second(void *context, size_t size) {
    FaultAllocator *fault = context;
    return ++fault->attempts == 2 ? NULL : malloc(size);
}
static void fault_free(void *context, void *memory) {
    FaultAllocator *fault = context;
    ++fault->frees;
    free(memory);
}

static void test_owned_frame_unwinds_allocation_failure(void) {
    uint8_t kinds[] = {ZR_STRICT_VALUE_I64, ZR_STRICT_VALUE_UNIQUE, ZR_STRICT_VALUE_UNIQUE, ZR_STRICT_VALUE_RECORD};
    uint32_t types[] = {0, 1, 1, 1};
    ZrStrictRecordLayout layout = {.fieldCount = 1, .fields = {ZR_STRICT_VALUE_I64}, .name = "Student"};
    ZrStrictInstruction instructions[] = {
        {ZR_STRICT_OP_CONST, 0, 0, 0, 42},
        {ZR_STRICT_OP_OWN_RECORD, 1, 0, 0, 0},
        {ZR_STRICT_OP_INIT_RECORD, 3, 0, 0, 0},
        {ZR_STRICT_OP_OWN_RECORD, 2, 0, 0, 0},
        {ZR_STRICT_OP_HALT, 0, 0, 0, 0},
    };
    ZrStrictProgram source = {.kinds = kinds, .slotCount = 4, .instructions = instructions, .instructionCount = 5,
                              .slotLayouts = types, .layouts = &layout, .layoutCount = 1};
    FaultAllocator fault = {0};
    ZrStrictAllocator allocator = {fail_second, fault_free, &fault};
    ZrStrictFrame frame;
    ZrStrictFrameSlot storage[4];
    assert(ZrStrictProgram_Verify(&source, NULL) == ZR_STRICT_PROGRAM_OK);
    assert(ZrStrictFrame_Open(&frame, &source, storage, 3, &allocator) == ZR_STRICT_PROGRAM_BAD_LAYOUT);
    assert(ZrStrictFrame_Open(&frame, &source, storage, 4, &allocator) == ZR_STRICT_PROGRAM_OK);
    assert(fault.attempts == 0);
    assert(ZrStrictFrame_Apply(&frame, &instructions[0]) == ZR_STRICT_PROGRAM_OK);
    assert(ZrStrictFrame_Apply(&frame, &instructions[1]) == ZR_STRICT_PROGRAM_OK);
    assert(ZrStrictFrame_Apply(&frame, &instructions[2]) == ZR_STRICT_PROGRAM_OK && fault.attempts == 1);
    assert(ZrStrictFrame_Apply(&frame, &instructions[3]) == ZR_STRICT_PROGRAM_OUT_OF_MEMORY);
    assert(ZrStrictFrame_Close(&frame) == ZR_STRICT_PROGRAM_OK);
    assert(fault.attempts == 2 && fault.frees == 1);
}

static void test_manual_byte_ownership_and_faults(void) {
    uint8_t kinds[] = {ZR_STRICT_VALUE_I64, ZR_STRICT_VALUE_MANUAL_BLOCK, ZR_STRICT_VALUE_MANUAL_BLOCK,
                       ZR_STRICT_VALUE_BOOL, ZR_STRICT_VALUE_BYTE_VIEW, ZR_STRICT_VALUE_I64};
    ZrStrictInstruction instructions[] = {
        {ZR_STRICT_OP_CONST, 0, 0, 0, 2},
        {ZR_STRICT_OP_MANUAL_TRY_ALLOC, 1, 0, 0, 0},
        {ZR_STRICT_OP_MANUAL_TRY_ALLOC, 2, 0, 0, 0},
        {ZR_STRICT_OP_IS_NULL, 3, 2, 0, 0},
        {ZR_STRICT_OP_BORROW, 4, 1, 0, 0},
        {ZR_STRICT_OP_CONST, 0, 0, 0, 1},
        {ZR_STRICT_OP_BYTE_READ, 5, 4, 0, 0},
        {ZR_STRICT_OP_CONST, 5, 0, 0, 42},
        {ZR_STRICT_OP_BYTE_WRITE, 4, 0, 5, 0},
        {ZR_STRICT_OP_BYTE_READ, 5, 4, 0, 0},
        {ZR_STRICT_OP_CONST, 0, 0, 0, 2},
        {ZR_STRICT_OP_BYTE_READ, 5, 4, 0, 0},
        {ZR_STRICT_OP_END_BORROW, 0, 4, 0, 0},
        {ZR_STRICT_OP_RETURN, 0, 5, 0, 0},
    };
    ZrStrictProgram p = {.kinds = kinds, .slotCount = 6, .instructions = instructions, .instructionCount = 14};
    ZrStrictFrame frame;
    ZrStrictFrameSlot storage[6];
    FaultAllocator failure = {0};
    ZrStrictAllocator allocator = {fail_second, fault_free, &failure};
    ZrStrictProgram loaded = {0};
    ZrStrictResult result;
    uint32_t pc;
    FILE *stream = tmpfile();
    assert(stream && ZrStrictProgram_Write(stream, &p) == ZR_STRICT_PROGRAM_OK);
    rewind(stream);
    assert(ZrStrictProgram_Read(stream, &loaded) == ZR_STRICT_PROGRAM_OK && loaded.layoutCount == 0);
    assert(ZrStrictProgram_Run(&loaded, &result, &pc) == ZR_STRICT_PROGRAM_BYTE_RANGE_FAULT && pc == 11);
    ZrStrictProgram_Free(&loaded);
    fclose(stream);

    assert(ZrStrictFrame_Open(&frame, &p, storage, 6, &allocator) == ZR_STRICT_PROGRAM_OK);
    for (size_t index = 0; index <= 6; ++index)
        assert(ZrStrictFrame_Apply(&frame, &instructions[index]) == ZR_STRICT_PROGRAM_OK);
    assert(ZrStrictFrame_Scalar(&frame, 3) == 1 && ZrStrictFrame_Scalar(&frame, 5) == 0);
    for (size_t index = 7; index <= 10; ++index)
        assert(ZrStrictFrame_Apply(&frame, &instructions[index]) == ZR_STRICT_PROGRAM_OK);
    assert(ZrStrictFrame_Scalar(&frame, 5) == 42);
    assert(ZrStrictFrame_Apply(&frame, &instructions[11]) == ZR_STRICT_PROGRAM_BYTE_RANGE_FAULT);
    assert(ZrStrictFrame_Close(&frame) == ZR_STRICT_PROGRAM_OK);
    assert(failure.attempts == 2 && failure.frees == 1);

    kinds[4] = ZR_STRICT_VALUE_READONLY_BYTE_VIEW;
    assert(ZrStrictProgram_Verify(&p, &pc) == ZR_STRICT_PROGRAM_BAD_INSTRUCTION && pc == 8);
    kinds[4] = ZR_STRICT_VALUE_BYTE_VIEW;
    instructions[8] = (ZrStrictInstruction){ZR_STRICT_OP_DROP, 0, 1, 0, 0};
    assert(ZrStrictProgram_Verify(&p, &pc) == ZR_STRICT_PROGRAM_BAD_INSTRUCTION && pc == 8);
}

static void test_strict_reborrow_keeps_parent_view_live(void) {
    uint8_t kinds[] = {ZR_STRICT_VALUE_I64, ZR_STRICT_VALUE_UNIQUE, ZR_STRICT_VALUE_BORROW, ZR_STRICT_VALUE_READONLY_BORROW};
    uint32_t types[] = {0, 1, 1, 1};
    ZrStrictRecordLayout layout = {.fieldCount = 1, .fields = {ZR_STRICT_VALUE_I64}, .name = "Counter"};
    ZrStrictInstruction instructions[] = {
        {ZR_STRICT_OP_CONST, 0, 0, 0, 42},
        {ZR_STRICT_OP_OWN_RECORD, 1, 0, 0, 0},
        {ZR_STRICT_OP_BORROW, 2, 1, 0, 0},
        {ZR_STRICT_OP_BORROW, 3, 2, 0, 0},
        {ZR_STRICT_OP_FIELD_READ, 0, 3, 0, 0},
        {ZR_STRICT_OP_END_BORROW, 0, 3, 0, 0},
        {ZR_STRICT_OP_END_BORROW, 0, 2, 0, 0},
        {ZR_STRICT_OP_DROP, 0, 1, 0, 0},
        {ZR_STRICT_OP_RETURN, 0, 0, 0, 0},
    };
    ZrStrictProgram p = {.kinds = kinds, .slotCount = 4, .instructions = instructions, .instructionCount = 9,
                         .slotLayouts = types, .layouts = &layout, .layoutCount = 1};
    ZrStrictResult result;
    uint32_t pc;
    assert(ZrStrictProgram_Run(&p, &result, &pc) == ZR_STRICT_PROGRAM_OK && result.scalar == 42);
    instructions[4] = (ZrStrictInstruction){ZR_STRICT_OP_END_BORROW, 0, 2, 0, 0};
    assert(ZrStrictProgram_Verify(&p, &pc) == ZR_STRICT_PROGRAM_BAD_INSTRUCTION && pc == 4);
}

int main(void) {
    test_verified_control_flow_round_trip();
    test_unverified_programs_fail_closed();
    test_arithmetic_fault_is_checked();
    test_owned_program_borrow_move_and_weak_lifecycle();
    test_ownership_joins_validate_all_incoming_paths();
    test_owned_frame_unwinds_allocation_failure();
    test_manual_byte_ownership_and_faults();
    test_strict_reborrow_keeps_parent_view_live();
    return 0;
}
