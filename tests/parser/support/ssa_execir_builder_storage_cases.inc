/* Guard witnesses use genuine small, borrowed storage. No fixture below owns
 * these arrays, and every mutation is restored before a Unity assertion. */
#ifdef realloc
#error "Builder storage cases require an unredirected include site"
#endif
#define realloc ZrTests_ExecOwner_Realloc
#include "exec_ir/exec_ir_build_storage_internal.h"
#undef realloc

typedef struct SBuilderStorageSample {
    SZrExecIrFunction function;
    SZrExecIrValue values[4];
    SZrExecIrBlock blocks[4];
    SZrExecIrInstruction instructions[4];
    TZrExecIrValueId results[4], operands[4];
    SZrExecIrSourceMap maps[8];
} SBuilderStorageSample;

static void builder_storage_sample_init(SBuilderStorageSample *sample) {
    memset(sample, 0, sizeof(*sample));
    ZrCore_ExecIr_FunctionInit(&sample->function);
    sample->function.functionToken = 73u;
    sample->function.values = sample->values;
    sample->function.valueCount = 1u;
    sample->function.valueCapacity = 4u;
    sample->function.blocks = sample->blocks;
    sample->function.blockCount = 1u;
    sample->function.blockCapacity = 4u;
    sample->function.entryBlockId = 1u;
    sample->function.instructions = sample->instructions;
    sample->function.instructionCapacity = 4u;
    sample->function.resultPool = sample->results;
    sample->function.resultCount = 1u;
    sample->function.resultCapacity = 4u;
    sample->function.operandPool = sample->operands;
    sample->function.operandCount = 1u;
    sample->function.operandCapacity = 4u;
    sample->function.sourceMaps = sample->maps;
    sample->function.sourceMapCapacity = 8u;
    sample->results[0] = sample->operands[0] = 1u;
}

static int builder_storage_stats_equal(SZrOwnerHeapStats a, SZrOwnerHeapStats b) {
    return a.attempts == b.attempts && a.records == b.records && a.live == b.live &&
           a.frees == b.frees && a.errors == b.errors && a.failures == b.failures;
}

enum EBuilderStorageGuardCase {
    BSG_SEALED_VALUE, BSG_SEALED_BLOCK, BSG_SEALED_RESULTS,
    BSG_SEALED_OPERANDS, BSG_SEALED_INSTRUCTION, BSG_SEALED_SOURCE,
    BSG_RESULT_ZERO, BSG_RESULT_OUTSIDE, BSG_RESULT_DEFINED, BSG_RESULT_EXTERNAL,
    BSG_RESULT_START, BSG_RESULT_COUNT, BSG_OWNERSHIP, BSG_NULLABILITY,
    BSG_BLOCK_FLAGS, BSG_BLOCK_ENTRY, BSG_NULL_RESULTS, BSG_NULL_OPERANDS,
    BSG_COUNT_CAPACITY, BSG_NULL_STORAGE, BSG_ZERO_CAPACITY, BSG_ALIGNMENT,
    BSG_ZERO_WIDTH, BSG_ZERO_ALIGNMENT, BSG_ADD_OVERFLOW, BSG_REQUEST_BYTES,
    BSG_GROWN_BYTES, BSG_NULL_GROWTH_OUTPUT, BSG_NULL_INSTRUCTION,
    BSG_NULL_SOURCE, BSG_RESULTS_COUNT_OVERFLOW, BSG_OPERANDS_COUNT_OVERFLOW,
    BSG_CASE_COUNT
};

static void builder_storage_guard_case(unsigned which) {
    SBuilderStorageSample sample, before;
    SZrExecIrInstruction instruction = {0};
    SZrSemanticIrInstruction source = {0};
    SZrExecIrDiagnostic diagnostic, expected;
    SZrExecIrRange range = {.start = 91u, .count = 93u};
    TZrExecIrInstructionId outputId = 97u;
    TZrBool needGrowth = ZR_TRUE, accepted = ZR_TRUE;
    EZrExecutionDiagnosticCode code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
    SZrOwnerHeapStats statsBefore, statsAfter;
    int unchanged, outputsUnchanged, retryGood;
    builder_storage_sample_init(&sample);
    SExecIrBuildStorageSite site = {&diagnostic, &sample.function, 1u, 0u, 1u};
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.resultRange.start = 0u;
    instruction.resultRange.count = 1u;
    source.id = 1u;
    memset(&diagnostic, 0xA5, sizeof(diagnostic));
    if (which <= BSG_SEALED_SOURCE) {
        sample.function.sealed = ZR_TRUE;
        code = ZR_EXEC_IR_DIAGNOSTIC_SEALED;
    }
    switch (which) {
    case BSG_RESULT_ZERO: sample.results[0] = 0u; break;
    case BSG_RESULT_OUTSIDE: sample.results[0] = 2u; break;
    case BSG_RESULT_DEFINED: sample.values[0].definition = 1u; break;
    case BSG_RESULT_EXTERNAL: sample.values[0].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY; break;
    case BSG_RESULT_START: instruction.resultRange.start = 2u; break;
    case BSG_RESULT_COUNT: instruction.resultRange.count = 2u; break;
    case BSG_COUNT_CAPACITY: sample.function.valueCount = 5u; break;
    case BSG_NULL_STORAGE: sample.function.values = ZR_NULL; break;
    case BSG_ZERO_CAPACITY:
        sample.function.valueCount = 0u; sample.function.valueCapacity = 0u; break;
    default: break;
    }
    if (which >= BSG_RESULT_ZERO && which <= BSG_RESULT_EXTERNAL)
        code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE;
    if (which == BSG_RESULT_START || which == BSG_RESULT_COUNT ||
        (which >= BSG_COUNT_CAPACITY && which <= BSG_ALIGNMENT))
        code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
    if (which == BSG_ADD_OVERFLOW || which == BSG_REQUEST_BYTES ||
        which == BSG_GROWN_BYTES || which == BSG_RESULTS_COUNT_OVERFLOW ||
        which == BSG_OPERANDS_COUNT_OVERFLOW)
        code = ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
    memcpy(&before, &sample, sizeof(before));
    statsBefore = ZrTests_ExecOwner_Stats();
    switch (which) {
    case BSG_SEALED_VALUE: case BSG_OWNERSHIP: case BSG_NULLABILITY:
    case BSG_COUNT_CAPACITY: case BSG_NULL_STORAGE: case BSG_ZERO_CAPACITY:
        accepted = (TZrBool)(build_storage_add_value(&sample.function, 11u,
            which == BSG_OWNERSHIP ? (EZrExecIrOwnership)ZR_EXEC_IR_OWNERSHIP_COUNT : ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            which == BSG_NULLABILITY ? (EZrExecIrNullability)ZR_EXEC_IR_NULLABILITY_COUNT : ZR_EXEC_IR_NULLABILITY_UNKNOWN,
            ZR_FALSE, &diagnostic) != ZR_EXEC_IR_VALUE_ID_INVALID);
        break;
    case BSG_SEALED_BLOCK: case BSG_BLOCK_FLAGS: case BSG_BLOCK_ENTRY:
        accepted = (TZrBool)(build_storage_add_block(&sample.function,
            which == BSG_BLOCK_FLAGS ? UINT32_MAX : ZR_EXEC_IR_BLOCK_FLAG_ENTRY,
            &diagnostic) != ZR_EXEC_IR_BLOCK_ID_INVALID);
        break;
    case BSG_SEALED_RESULTS: case BSG_NULL_RESULTS: case BSG_RESULTS_COUNT_OVERFLOW:
        accepted = build_storage_append_results(&sample.function,
            which == BSG_NULL_RESULTS ? ZR_NULL : sample.results,
            which == BSG_RESULTS_COUNT_OVERFLOW ? (TZrSize)UINT32_MAX : 1u, &range, &site);
        break;
    case BSG_SEALED_OPERANDS: case BSG_NULL_OPERANDS: case BSG_OPERANDS_COUNT_OVERFLOW:
        accepted = build_storage_append_operands(&sample.function,
            which == BSG_NULL_OPERANDS ? ZR_NULL : sample.operands,
            which == BSG_OPERANDS_COUNT_OVERFLOW ? (TZrSize)UINT32_MAX : 1u, &range, &site);
        break;
    case BSG_SEALED_SOURCE: case BSG_NULL_SOURCE:
        accepted = build_storage_append_source(&sample.function,
            which == BSG_NULL_SOURCE ? ZR_NULL : &source, &site); break;
    case BSG_ALIGNMENT: case BSG_ZERO_WIDTH: case BSG_ZERO_ALIGNMENT:
        accepted = build_storage_geometry(&site,
            which == BSG_ALIGNMENT ? (const void *)((unsigned char *)sample.values + 1u) : sample.values, 1u, 4u,
            which == BSG_ZERO_WIDTH ? 0u : sizeof(*sample.values),
            which == BSG_ZERO_ALIGNMENT ? 0u : _Alignof(SZrExecIrValue)); break;
    case BSG_ADD_OVERFLOW:
        accepted = build_storage_growth(&site, sample.results, 1u, 4u,
            (TZrSize)UINT32_MAX, sizeof(*sample.results), _Alignof(TZrExecIrValueId), &needGrowth); break;
    case BSG_REQUEST_BYTES:
        accepted = build_storage_growth(&site, ZR_NULL, 0u, 0u, 2u, SIZE_MAX, 1u, &needGrowth); break;
    case BSG_GROWN_BYTES:
        accepted = build_storage_growth(&site, ZR_NULL, 0u, 0u, 1u, SIZE_MAX / 4u + 1u, 1u, &needGrowth); break;
    case BSG_NULL_GROWTH_OUTPUT:
        accepted = build_storage_growth(&site, ZR_NULL, 0u, 0u, 1u, 1u, 1u, ZR_NULL); break;
    default:
        accepted = build_storage_append_instruction(&sample.function,
            which == BSG_NULL_INSTRUCTION ? ZR_NULL : &instruction, &outputId, &site); break;
    }
    statsAfter = ZrTests_ExecOwner_Stats();
    unchanged = memcmp(&sample, &before, sizeof(sample)) == 0;
    outputsUnchanged = outputId == 97u && range.start == 91u && range.count == 93u && needGrowth == ZR_TRUE;
    memset(&expected, 0, sizeof(expected));
    expected.code = code; expected.functionToken = 73u;
    /* AddValue/AddBlock have no row coordinates in their actual call contract. */
    if (!(which == BSG_SEALED_VALUE || which == BSG_SEALED_BLOCK ||
          which == BSG_OWNERSHIP || which == BSG_NULLABILITY ||
          which == BSG_BLOCK_FLAGS || which == BSG_BLOCK_ENTRY ||
          (which >= BSG_COUNT_CAPACITY && which <= BSG_ZERO_CAPACITY))) {
        expected.blockId = 1u; expected.sourceId = 1u;
    }
    /* Restore every malformed shape, then exercise a real no-growth retry.
     * The saved observations outlive the borrowed fixture's complete unbinding. */
    memcpy(&sample, &before, sizeof(sample));
    builder_storage_sample_init(&sample);
    SZrOwnerHeapStats retryBefore = ZrTests_ExecOwner_Stats();
    TZrExecIrInstructionId retryId = 0u;
    SZrExecIrDiagnostic retryDiagnostic = {0};
    SExecIrBuildStorageSite retrySite = {&retryDiagnostic, &sample.function, 1u, 0u, 1u};
    instruction.resultRange.start = 0u; instruction.resultRange.count = 1u;
    retryGood = build_storage_append_instruction(&sample.function, &instruction, &retryId, &retrySite) &&
                retryId == 1u && sample.function.instructionCount == 1u &&
                sample.values[0].definition == 1u;
    SZrOwnerHeapStats retryAfter = ZrTests_ExecOwner_Stats();
    ZrCore_ExecIr_FunctionInit(&sample.function); /* Never free borrowed stack arrays. */
    TEST_ASSERT_FALSE_MESSAGE(accepted, "builder storage guard accepted malformed input");
    TEST_ASSERT_TRUE_MESSAGE(unchanged, "builder storage refusal changed function/backing bytes");
    TEST_ASSERT_TRUE_MESSAGE(outputsUnchanged, "builder storage refusal published output");
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&expected, &diagnostic, sizeof(expected), "builder storage diagnostic differs");
    TEST_ASSERT_TRUE_MESSAGE(builder_storage_stats_equal(statsBefore, statsAfter), "builder guard allocated or changed ledger");
    TEST_ASSERT_TRUE_MESSAGE(retryGood, "restored small storage did not accept a real instruction");
    TEST_ASSERT_TRUE_MESSAGE(builder_storage_stats_equal(retryBefore, retryAfter), "no-growth retry allocated");
    TEST_ASSERT_EQUAL_UINT(ZR_EXECUTION_DIAGNOSTIC_NONE, retryDiagnostic.code);
}

static void builder_storage_zero_places_public_retry(void) {
    /* Match the genuine empty canonical CFG shape in test_ssa_builder_cfg.c.
     * The public transactional API must accept a sealed old destination. */
    SZrSemanticIrFunction semantic = {0}, semanticBefore;
    SZrParserCfgBlock block = {0}, blockBefore;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic, clear = {0};
    SZrOwnerLiveSpan oldSpan = {0};
    SZrOwnerHeapStats before = ZrTests_ExecOwner_Stats(), prepared, finished;
    unsigned oldFrees = 0u;
    int oldTracked, oldReleased, inputsUnchanged, outputGood, allReleased = 1;
    semantic.symbolId = (TZrSymbolId)42u;
    semantic.callableTypeId = (TZrTypeId)55u;
    semantic.cfg.entryBlockId = 0u;
    semantic.cfg.blocks.head = (TZrBytePtr)&block;
    semantic.cfg.blocks.elementSize = sizeof(block);
    semantic.cfg.blocks.length = semantic.cfg.blocks.capacity = 1u;
    semantic.cfg.blocks.isValid = ZR_TRUE;
    semantic.instructions.elementSize = sizeof(SZrSemanticIrInstruction);
    semantic.instructions.isValid = ZR_TRUE;
    block.id = 0u; block.kind = ZR_PARSER_CFG_BLOCK_ENTRY;
    memcpy(&semanticBefore, &semantic, sizeof(semantic));
    memcpy(&blockBefore, &block, sizeof(block));
    ZrCore_ExecIr_FunctionInit(&output);
    TZrExecIrBlockId oldId = ZrCore_ExecIr_FunctionAddBlock(&output, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    prepared = ZrTests_ExecOwner_Stats();
    uintptr_t oldBlocksAddress = (uintptr_t)output.blocks;
    oldTracked = oldId == 1u && ZrTests_ExecOwner_TryLiveSpan(before.records, prepared.records,
        output.blocks, output.blockCapacity, sizeof(*output.blocks), &oldSpan);
    output.sealed = ZR_TRUE;
    memset(&diagnostic, 0xA5, sizeof(diagnostic));
    TZrBool built = ZrParser_ExecIr_Build(&semantic, ZR_NULL, &output, &diagnostic);
    inputsUnchanged = memcmp(&semantic, &semanticBefore, sizeof(semantic)) == 0 &&
                      memcmp(&block, &blockBefore, sizeof(block)) == 0;
    outputGood = built && !output.sealed && output.functionToken == 42u && output.signatureHash == 55u &&
                 output.blockCount == 1u && output.entryBlockId == 1u && output.blocks != ZR_NULL &&
                 (uintptr_t)output.blocks != oldBlocksAddress && output.blocks[0].id == 1u && output.valueCount == 0u &&
                 output.instructionCount == 0u && output.sourceMapCount == 0u;
    oldReleased = oldTracked && ZrTests_ExecOwner_TryRecordFrees(oldSpan.record, &oldFrees) && oldFrees == 1u;
    ZrCore_ExecIr_FreeFunction(&output);
    finished = ZrTests_ExecOwner_Stats();
    for (unsigned n = before.records; n < finished.records; ++n) {
        unsigned frees = 0u;
        if (!ZrTests_ExecOwner_TryRecordFrees(n, &frees) || frees != 1u) allReleased = 0;
    }
    memcpy(&semantic, &semanticBefore, sizeof(semantic));
    memcpy(&block, &blockBefore, sizeof(block));
    memset(&clear, 0, sizeof(clear));
    /* All actual owning output has been released before any Unity longjmp. */
    TEST_ASSERT_TRUE_MESSAGE(oldTracked, "zero-place old destination was not a real tracked owner");
    TEST_ASSERT_TRUE_MESSAGE(built, "public Build rejected a genuine zero-place CFG");
    TEST_ASSERT_TRUE_MESSAGE(outputGood, "zero-place Build did not transactionally replace sealed destination");
    TEST_ASSERT_TRUE(inputsUnchanged);
    TEST_ASSERT_EQUAL_MEMORY(&clear, &diagnostic, sizeof(clear));
    TEST_ASSERT_TRUE(oldReleased);
    TEST_ASSERT_TRUE(allReleased);
    TEST_ASSERT_EQUAL_UINT(before.live, finished.live);
    TEST_ASSERT_EQUAL_UINT(before.errors, finished.errors);
    TEST_ASSERT_EQUAL_UINT(before.failures, finished.failures);
    fprintf(stderr, "INTEGER_PROOF storage_zero_places build=1 sealed_destination=1 released=1\n");
}

static void test_builder_storage_guards_preserve_outputs_without_allocation(void) {
    for (unsigned which = 0u; which < BSG_CASE_COUNT; ++which)
        builder_storage_guard_case(which);
    /* Numeric-only growth proofs do not advertise any live giant pool. No
     * pointer is read and no reserve/mutator is called in these boundary rows. */
    const TZrSize requests[] = {0u, 1u, 4u, 5u, (TZrSize)UINT32_MAX};
    for (unsigned i = 0u; i < sizeof(requests) / sizeof(requests[0]); ++i) {
        SZrExecIrDiagnostic diagnostic, before;
        TZrBool needGrowth = ZR_FALSE;
        memset(&diagnostic, 0xA5, sizeof(diagnostic)); memcpy(&before, &diagnostic, sizeof(before));
        SExecIrBuildStorageSite site = {&diagnostic, ZR_NULL, 0u, 0u, 0u};
        SZrOwnerHeapStats statsBefore = ZrTests_ExecOwner_Stats();
        TZrBool ok = build_storage_growth(&site, ZR_NULL, 0u, 0u, requests[i], 1u, 1u, &needGrowth);
        SZrOwnerHeapStats statsAfter = ZrTests_ExecOwner_Stats();
        TEST_ASSERT_TRUE(ok);
        TEST_ASSERT_EQUAL_UINT(requests[i] != 0u, needGrowth);
        TEST_ASSERT_EQUAL_MEMORY(&before, &diagnostic, sizeof(before));
        TEST_ASSERT_TRUE(builder_storage_stats_equal(statsBefore, statsAfter));
    }
    fprintf(stderr, "INTEGER_PROOF storage refusals=%u retries=%u math=5 allocations=0\n",
            (unsigned)BSG_CASE_COUNT, (unsigned)BSG_CASE_COUNT);
    builder_storage_zero_places_public_retry();
}
