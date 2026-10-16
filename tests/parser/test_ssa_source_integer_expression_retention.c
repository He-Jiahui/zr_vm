#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "support/ssa_source_returned_retention_fixture.h"
#include "zr_vm_core/function_exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_core/metadata_token.h"

/* Assertions consume the real returned owner. This table is an oracle for
 * existing source operations, not a builder or an expression evaluator. */
typedef struct SExpectedExpressionRow {
    TZrUInt16 opcode;
    unsigned left, right;
    TZrUInt64 bits;
    TZrUInt32 start, end;
} SExpectedExpressionRow;
enum { EXPRESSION_ROWS = 16 };
static const SZrSourceCompileOptions g_exec = {ZR_SOURCE_RETENTION_REQUIRE_SCALAR_EXECIR};
static const SZrSourceCompileOptions g_aot = {ZR_SOURCE_RETENTION_REQUIRE_SCALAR_AOT};
#define C(bits, start, end) {ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, bits, start, end}
#define B(op, left, right, start, end) {ZR_EXEC_IR_OPCODE_##op, left, right, 0u, start, end}
static const SExpectedExpressionRow g_add[] = {C(2u, 7u, 8u), C(3u, 11u, 12u), B(ADD, 0u, 1u, 7u, 12u)};
static const SExpectedExpressionRow g_sub[] = {C(9u, 7u, 8u), C(4u, 11u, 12u), B(SUB, 0u, 1u, 7u, 12u),
        C(1u, 15u, 16u), B(SUB, 2u, 3u, 7u, 16u)};
static const SExpectedExpressionRow g_mul[] = {C(6u, 7u, 8u), C(7u, 11u, 12u), B(MUL, 0u, 1u, 7u, 12u)};
static const SExpectedExpressionRow g_precedence[] = {C(2u, 7u, 8u), C(3u, 11u, 12u), C(4u, 15u, 16u),
        B(MUL, 1u, 2u, 11u, 16u), B(ADD, 0u, 3u, 7u, 16u)};
/* Parentheses do not replace the enclosed parser node's source range. */
static const SExpectedExpressionRow g_grouped[] = {C(2u, 8u, 9u), C(3u, 12u, 13u), B(ADD, 0u, 1u, 8u, 13u),
        C(4u, 17u, 18u), B(MUL, 2u, 3u, 8u, 18u)};
static const SExpectedExpressionRow g_repeated[] = {C(2u, 7u, 8u), C(2u, 11u, 12u), B(MUL, 0u, 1u, 7u, 12u),
        C(2u, 15u, 16u), B(ADD, 2u, 3u, 7u, 16u)};
static const SExpectedExpressionRow g_multiline[] = {C(2u, 7u, 8u), C(3u, 13u, 14u), B(ADD, 0u, 1u, 7u, 14u)};
static const SExpectedExpressionRow g_literal[] = {C(9u, 7u, 8u)};
#undef C
#undef B

void setUp(void) { ZrTests_Returned_Init(); }
void tearDown(void) {
    SZrReturnedVmStats vm = {0};
    SZrReturnedHeapStats crt[2] = {{0}, {0}};
    int vmRead, crtRead[2];
    ZrTests_Returned_FreeFunction(0u); ZrTests_Returned_FreeFunction(1u);
    ZrTests_Returned_Shutdown();
    /* Capture producer cleanup before Close's recovery can release leaked rows. */
    vmRead = ZrTests_Returned_TryVmStats(&vm);
    crtRead[0] = ZrTests_Returned_TryStats(0u, &crt[0]);
    crtRead[1] = ZrTests_Returned_TryStats(1u, &crt[1]);
    ZrTests_Returned_Close();
    TEST_ASSERT_EQUAL_INT(1, vmRead);
    TEST_ASSERT_EQUAL_UINT(0u, vm.live); TEST_ASSERT_EQUAL_UINT(0u, vm.errors);
    TEST_ASSERT_EQUAL_UINT(0u, vm.diagnosticCount); TEST_ASSERT_EQUAL_UINT(0u, vm.dropped);
    for (unsigned i = 0u; i < 2u; ++i) {
        TEST_ASSERT_EQUAL_INT(1, crtRead[i]); TEST_ASSERT_EQUAL_UINT(0u, crt[i].live);
        TEST_ASSERT_EQUAL_UINT(0u, crt[i].errors); TEST_ASSERT_EQUAL_UINT(crt[i].successes, crt[i].frees);
    }
    ZrTests_Returned_AssertBalanced();
}

static void zero_diagnostic(const SZrSourceCompileDiagnostic *d) {
#define Z(field) TEST_ASSERT_EQUAL_UINT64(0u, d->field)
    Z(stage); Z(result); Z(hasExecIr); Z(hasAotIr); Z(hasRange);
    Z(execIr.code); Z(execIr.functionToken); Z(execIr.blockId); Z(execIr.instructionId); Z(execIr.sourceId);
    Z(execIr.expectedVersion); Z(execIr.actualVersion); Z(execIr.expectedHash); Z(execIr.actualHash);
    Z(aotIr.status); Z(aotIr.functionId); Z(aotIr.blockId); Z(aotIr.instructionId);
    Z(aotIr.index); Z(aotIr.expected); Z(aotIr.actual); Z(startOffset); Z(endOffset);
    Z(startLine); Z(startColumn); Z(endLine); Z(endColumn);
#undef Z
}

static void ordinary_prerequisite(const char *source) {
    SZrFunction *f = ZrTests_Returned_Compile(0u, source, ZR_TRUE, ZR_NULL, ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "ordinary SourceCompile prerequisite failed");
    TEST_ASSERT_NULL(f->ownedExecIr); TEST_ASSERT_NULL(f->ownedAotIr);
    ZrTests_Returned_FreeFunction(0u);
}

static SZrFunction *required(unsigned slot, const char *source, TZrBool diagnostic) {
    SZrSourceCompileDiagnostic d;
    SZrFunction *f;
    memset(&d, 0xA5, sizeof(d));
    f = ZrTests_Returned_Compile(slot, source, ZR_FALSE, &g_exec, diagnostic ? &d : ZR_NULL);
    if (f == ZR_NULL && diagnostic)
        fprintf(stderr, "SOURCE_I64_EXPRESSION_REFUSED stage=%u result=%u code=%u\n",
                (unsigned)d.stage, (unsigned)d.result, (unsigned)d.execIr.code);
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "SOURCE_I64_EXPRESSION_MISSING_RETAINED_OWNER");
    if (diagnostic) zero_diagnostic(&d);
    TEST_ASSERT_NOT_NULL(f->ownedExecIr); TEST_ASSERT_NULL(f->ownedAotIr);
    return f;
}

static void source_position(const char *source, TZrUInt32 offset, TZrUInt32 *line, TZrUInt32 *column) {
    *line = 1u; *column = 1u;
    for (TZrUInt32 i = 0u; i < offset; ++i) {
        if (source[i] == '\n') { ++*line; *column = 1u; }
        else ++*column;
    }
}

static const SZrExecIrSourceMap *instruction_map(const SZrExecIrFunction *f, TZrUInt32 index,
        const char *source) {
    const SZrExecIrSourceMap *found = ZR_NULL;
    TZrUInt32 line, column;
    for (TZrUInt32 i = 0u; i < f->sourceMapCount; ++i) {
        if (f->sourceMaps[i].instructionId == index + 1u) {
            TEST_ASSERT_NULL_MESSAGE(found, "duplicate instruction source map"); found = &f->sourceMaps[i];
        }
    }
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, found->sourceId);
    TEST_ASSERT_EQUAL_UINT32(f->instructions[index].sourceId, found->sourceId);
    TEST_ASSERT_TRUE(found->startOffset <= found->endOffset);
    TEST_ASSERT_TRUE(found->endOffset <= strlen(source));
    source_position(source, found->startOffset, &line, &column);
    TEST_ASSERT_EQUAL_UINT32(line, found->startLine); TEST_ASSERT_EQUAL_UINT32(column, found->startColumn);
    source_position(source, found->endOffset, &line, &column);
    TEST_ASSERT_EQUAL_UINT32(line, found->endLine); TEST_ASSERT_EQUAL_UINT32(column, found->endColumn);
    return found;
}

static void identity(const SZrFunction *runtime, const SZrExecIrModule *m, const SZrExecIrFunction *f) {
    const SZrMetadataTokenRecord *module = ZR_NULL, *entry = ZR_NULL;
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, m->moduleHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, f->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(runtime->moduleSignatureHash, m->moduleHash);
    TEST_ASSERT_EQUAL_UINT64(runtime->sourceCallableIdentity.canonicalSignatureHash, f->signatureHash);
    for (TZrUInt32 i = 0u; i < runtime->metadataTokenRecordLength; ++i) {
        const SZrMetadataTokenRecord *r = &runtime->metadataTokenRecords[i];
        if (ZR_METADATA_TOKEN_TABLE(r->token) == ZR_METADATA_TABLE_MODULE) { TEST_ASSERT_NULL(module); module = r; }
        if (ZR_METADATA_TOKEN_TABLE(r->token) == ZR_METADATA_TABLE_MEMBER_DEF &&
                r->reserved0 == ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY) { TEST_ASSERT_NULL(entry); entry = r; }
    }
    TEST_ASSERT_NOT_NULL(module); TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(module->token, entry->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(module->token, m->moduleToken);
    TEST_ASSERT_EQUAL_UINT32(module->token, m->contract.targetToken);
    TEST_ASSERT_EQUAL_UINT32(entry->token, f->functionToken);
    TEST_ASSERT_EQUAL_UINT32(entry->token, f->contract.targetToken);
    TEST_ASSERT_EQUAL_UINT64(m->moduleHash, m->contract.moduleHash);
    TEST_ASSERT_EQUAL_UINT64(m->moduleHash, f->contract.moduleHash);
    TEST_ASSERT_EQUAL_UINT64(f->signatureHash, f->contract.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(0u, f->contract.declaredEffects);
}

static void frame_and_metadata(const SZrExecIrModule *m, const SZrExecIrFunction *f, TZrUInt32 type) {
    const SZrExecIrFrameLayout *frame = f->frameLayout;
    TEST_ASSERT_EQUAL_UINT32(1u, m->layoutCount); TEST_ASSERT_NOT_NULL(m->layouts);
    TEST_ASSERT_EQUAL_UINT32(type, m->layouts[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(sizeof(TZrInt64), m->layouts[0].byteSize);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, m->layouts[0].layoutHash);
    TEST_ASSERT_NOT_NULL(frame); TEST_ASSERT_NOT_NULL(frame->slots);
    TEST_ASSERT_EQUAL_UINT32(f->valueCount, frame->slotCount);
    TEST_ASSERT_EQUAL_UINT32(f->valueCount, frame->logicalSlotCount);
    TEST_ASSERT_EQUAL_UINT32(f->valueCount, frame->storageSlotCount);
    TEST_ASSERT_EQUAL_UINT32(f->valueCount, frame->localCount);
    TEST_ASSERT_EQUAL_UINT32(0u, frame->parameterCount); TEST_ASSERT_EQUAL_UINT32(0u, frame->parameterPrefixCount);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, frame->layoutHash);
    TEST_ASSERT_EQUAL_UINT64(frame->layoutHash, f->contract.layoutHash);
    TEST_ASSERT_EQUAL_UINT32(frame->frameByteSize, frame->returnBufferOffset);
    TEST_ASSERT_TRUE(frame->frameByteAlign > 0u);
    TEST_ASSERT_EQUAL_UINT32(0u, frame->frameByteAlign & (frame->frameByteAlign - 1u));
    TEST_ASSERT_EQUAL_UINT32(0u, frame->frameByteSize % frame->frameByteAlign);
    for (TZrUInt32 i = 0u; i < frame->slotCount; ++i) {
        const SZrExecIrFrameSlot *s = &frame->slots[i];
        unsigned matches = 0u;
        TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_FRAME_SLOT_VALUE, s->kind);
        TEST_ASSERT_EQUAL_UINT32(type, s->typeToken);
        TEST_ASSERT_EQUAL_UINT32(m->layouts[0].byteSize, s->byteSize);
        TEST_ASSERT_EQUAL_UINT32(m->layouts[0].byteAlign, s->byteAlign);
        TEST_ASSERT_TRUE(s->byteAlign > 0u); TEST_ASSERT_EQUAL_UINT32(0u, s->byteOffset % s->byteAlign);
        TEST_ASSERT_TRUE(s->byteOffset <= frame->frameByteSize);
        TEST_ASSERT_TRUE(s->byteSize <= frame->frameByteSize - s->byteOffset);
        for (TZrUInt32 j = 0u; j < f->valueCount; ++j) if (f->values[j].id == s->slotId) ++matches;
        TEST_ASSERT_EQUAL_UINT(1u, matches);
        for (TZrUInt32 j = 0u; j < i; ++j) {
            const SZrExecIrFrameSlot *p = &frame->slots[j];
            TEST_ASSERT_NOT_EQUAL_UINT32(p->slotId, s->slotId);
            TEST_ASSERT_TRUE(s->byteOffset >= p->byteOffset + p->byteSize || p->byteOffset >= s->byteOffset + s->byteSize);
        }
    }
    TEST_ASSERT_NULL(f->gcMap); TEST_ASSERT_EQUAL_UINT32(0u, f->gcMapCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->gcRootCount); TEST_ASSERT_EQUAL_UINT32(0u, f->memoryTokenCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->phiCount); TEST_ASSERT_EQUAL_UINT32(0u, f->phiIncomingCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->predecessorCount); TEST_ASSERT_EQUAL_UINT32(0u, f->successorCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->deoptStateCount); TEST_ASSERT_EQUAL_UINT32(0u, f->deoptValueCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->deoptAggregateCount); TEST_ASSERT_EQUAL_UINT32(0u, f->deoptAggregateFieldCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->bindingRowCount);
    if (f->stateMap != ZR_NULL) {
        TEST_ASSERT_EQUAL_UINT32(f->functionToken, f->stateMap->functionToken);
        TEST_ASSERT_EQUAL_UINT64(f->signatureHash, f->stateMap->signatureHash);
        TEST_ASSERT_EQUAL_UINT64(f->contract.generation, f->stateMap->generation);
        TEST_ASSERT_EQUAL_UINT32(0u, f->stateMap->entryCount); TEST_ASSERT_EQUAL_UINT32(0u, f->stateMap->valueCount);
        TEST_ASSERT_EQUAL_UINT32(0u, f->stateMap->rootCount); TEST_ASSERT_EQUAL_UINT32(0u, f->stateMap->ownerStateCount);
    }
}

static const SZrExecIrModule *assert_expression(const SZrFunction *runtime, const char *source,
        const SExpectedExpressionRow *expected, unsigned count) {
    const SZrExecIrModule *m = ZrCore_Function_GetExecIr(runtime);
    const SZrExecIrFunction *f;
    SZrExecIrDiagnostic d = {0};
    TZrExecIrValueId results[EXPRESSION_ROWS] = {0};
    unsigned row = 0u, nops = 0u, returns = 0u;
    TZrUInt32 type;
    TEST_ASSERT_TRUE(count > 0u && count <= EXPRESSION_ROWS);
    TEST_ASSERT_NOT_NULL(m); TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyModule(m, &d));
    TEST_ASSERT_EQUAL_UINT32(1u, m->functionCount); TEST_ASSERT_NOT_NULL(m->functions);
    f = &m->functions[0]; TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(f, ZR_EXEC_IR_VERIFY_ALL, &d));
    /* Source finalization preserves the genuine unsealed builder profile. */
    TEST_ASSERT_FALSE(f->sealed); TEST_ASSERT_EQUAL_UINT32(1u, f->blockCount); TEST_ASSERT_NOT_NULL(f->blocks);
    TEST_ASSERT_EQUAL_UINT32(f->entryBlockId, f->blocks[0].id);
    TEST_ASSERT_EQUAL_UINT32(0u, f->blocks[0].instructions.offset);
    TEST_ASSERT_EQUAL_UINT32(f->instructionCount, f->blocks[0].instructions.count);
    TEST_ASSERT_EQUAL_UINT32(0u, f->blocks[0].predecessors.count);
    TEST_ASSERT_EQUAL_UINT32(0u, f->blocks[0].successors.count);
    TEST_ASSERT_EQUAL_UINT32(count, f->valueCount); TEST_ASSERT_NOT_NULL(f->values);
    type = f->values[0].typeToken; TEST_ASSERT_NOT_EQUAL_UINT32(0u, type);
    identity(runtime, m, f);
    TEST_ASSERT_EQUAL_UINT32(runtime->constantValueLength, m->constantCount);
    TEST_ASSERT_TRUE(m->constantCount > 0u); TEST_ASSERT_NOT_NULL(m->constants); TEST_ASSERT_NOT_NULL(runtime->constantValueList);
    TEST_ASSERT_TRUE((const void *)m->constants != (const void *)runtime->constantValueList);
    for (TZrUInt32 i = 0u; i < m->constantCount; ++i) {
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, runtime->constantValueList[i].type);
        TEST_ASSERT_EQUAL_UINT32(type, m->constants[i].typeToken); TEST_ASSERT_EQUAL_UINT32(0u, m->constants[i].flags);
        TEST_ASSERT_EQUAL_UINT64((TZrUInt64)runtime->constantValueList[i].value.nativeObject.nativeInt64, m->constants[i].bits);
    }
    TEST_ASSERT_EQUAL_UINT32(f->instructionCount, f->sourceMapCount); TEST_ASSERT_NOT_NULL(f->sourceMaps);
    for (TZrUInt32 i = 0u; i < f->instructionCount; ++i) {
        const SZrExecIrInstruction *in = &f->instructions[i];
        const SZrExecIrSourceMap *map = instruction_map(f, i, source);
        TEST_ASSERT_EQUAL_UINT16(0u, in->flags); TEST_ASSERT_EQUAL_UINT32(0u, in->effectIn);
        TEST_ASSERT_EQUAL_UINT32(0u, in->effectOut); TEST_ASSERT_EQUAL_UINT32(0u, in->memoryIn.count);
        TEST_ASSERT_EQUAL_UINT32(0u, in->memoryOut.count); TEST_ASSERT_EQUAL_UINT32(0u, in->phiRange.count);
        TEST_ASSERT_EQUAL_UINT32(0u, in->successorRange.count); TEST_ASSERT_EQUAL_UINT32(0u, in->deoptId);
        TEST_ASSERT_EQUAL_UINT32(0u, in->bindingRow); TEST_ASSERT_EQUAL_UINT32(0u, in->matchTypeToken);
        if (in->opcode == ZR_EXEC_IR_OPCODE_NOP) {
            ++nops; TEST_ASSERT_EQUAL_UINT32(0u, in->results.count); TEST_ASSERT_EQUAL_UINT32(0u, in->operands.count);
            TEST_ASSERT_EQUAL_UINT32(type, in->typeToken); TEST_ASSERT_EQUAL_UINT32(0u, in->layoutId);
            continue;
        }
        if (in->opcode == ZR_EXEC_IR_OPCODE_RETURN) {
            ++returns; TEST_ASSERT_EQUAL_UINT(count, row); TEST_ASSERT_EQUAL_UINT32(f->instructionCount - 1u, i);
            TEST_ASSERT_EQUAL_UINT32(i + 1u, f->blocks[0].terminatorInstructionId);
            TEST_ASSERT_EQUAL_UINT32(0u, in->results.count); TEST_ASSERT_EQUAL_UINT32(1u, in->operands.count);
            TEST_ASSERT_EQUAL_UINT32(0u, map->startOffset);
            TEST_ASSERT_EQUAL_UINT32(strlen(source), map->endOffset);
            TEST_ASSERT_EQUAL_UINT32(type, in->typeToken); TEST_ASSERT_EQUAL_UINT32(0u, in->layoutId);
            TEST_ASSERT_EQUAL_UINT32(results[count - 1u], f->operandPool[in->operands.offset]);
            continue;
        }
        TEST_ASSERT_TRUE_MESSAGE(row < count, "unexpected extra live operation");
        TEST_ASSERT_EQUAL_UINT16(expected[row].opcode, in->opcode);
        TEST_ASSERT_EQUAL_UINT32(expected[row].start, map->startOffset);
        TEST_ASSERT_EQUAL_UINT32(expected[row].end, map->endOffset);
        TEST_ASSERT_EQUAL_UINT32(type, in->typeToken); TEST_ASSERT_EQUAL_UINT32(1u, in->results.count);
        results[row] = f->resultPool[in->results.offset];
        TEST_ASSERT_TRUE(results[row] > 0u && results[row] <= f->valueCount);
        {
            const SZrExecIrValue *v = &f->values[results[row] - 1u];
            TEST_ASSERT_EQUAL_UINT32(results[row], v->id); TEST_ASSERT_EQUAL_UINT32(i + 1u, v->definition);
            TEST_ASSERT_EQUAL_UINT32(type, v->typeToken); TEST_ASSERT_EQUAL_UINT32(0u, v->flags);
            TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, v->ownership);
            TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_NULLABILITY_NONNULL, v->nullability);
        }
        for (unsigned j = 0u; j < row; ++j) TEST_ASSERT_NOT_EQUAL_UINT32(results[j], results[row]);
        if (in->opcode == ZR_EXEC_IR_OPCODE_CONSTANT) {
            unsigned matches = 0u; TZrUInt32 actualIndex = 0u;
            TEST_ASSERT_EQUAL_UINT32(0u, in->operands.count); TEST_ASSERT_TRUE(in->layoutId < m->constantCount);
            for (TZrUInt32 j = 0u; j < runtime->constantValueLength; ++j)
                if ((TZrUInt64)runtime->constantValueList[j].value.nativeObject.nativeInt64 == expected[row].bits) {
                    ++matches; actualIndex = j;
                }
            TEST_ASSERT_EQUAL_UINT(1u, matches); TEST_ASSERT_EQUAL_UINT32(actualIndex, in->layoutId);
            TEST_ASSERT_EQUAL_UINT64(expected[row].bits, m->constants[in->layoutId].bits);
        } else {
            TEST_ASSERT_TRUE(expected[row].left < row && expected[row].right < row);
            TEST_ASSERT_EQUAL_UINT32(2u, in->operands.count); TEST_ASSERT_EQUAL_UINT32(0u, in->layoutId);
            TEST_ASSERT_EQUAL_UINT32(results[expected[row].left], f->operandPool[in->operands.offset]);
            TEST_ASSERT_EQUAL_UINT32(results[expected[row].right], f->operandPool[in->operands.offset + 1u]);
        }
        ++row;
    }
    TEST_ASSERT_EQUAL_UINT(count, row); TEST_ASSERT_EQUAL_UINT(1u, returns);
    TEST_ASSERT_TRUE_MESSAGE(nops > 0u, "actual source-place tombstone identities must survive");
    frame_and_metadata(m, f, type);
    return m;
}

static void expression_case(const char *source, const SExpectedExpressionRow *expected, unsigned count) {
    ordinary_prerequisite(source);
    (void)assert_expression(required(0u, source, ZR_TRUE), source, expected, count);
}
static void test_add_preserves_real_operator_and_operands(void) { expression_case("return 2 + 3;", g_add, 3u); }
static void test_subtract_preserves_left_associativity(void) { expression_case("return 9 - 4 - 1;", g_sub, 5u); }
static void test_multiply_preserves_real_operator(void) { expression_case("return 6 * 7;", g_mul, 3u); }
static void test_precedence_retains_mul_then_add_without_evaluation(void) { expression_case("return 2 + 3 * 4;", g_precedence, 5u); }
static void test_parentheses_change_actual_operand_graph(void) { expression_case("return (2 + 3) * 4;", g_grouped, 5u); }
static void test_duplicate_literals_preserve_actual_pool_indices(void) {
    const char *source = "return 2 * 2 + 2;"; const SZrExecIrModule *m;
    ordinary_prerequisite(source);
    m = assert_expression(required(0u, source, ZR_TRUE), source, g_repeated, 5u);
    TEST_ASSERT_EQUAL_UINT32(1u, m->constantCount);
    for (TZrUInt32 i = 0u; i < m->functions[0].instructionCount; ++i)
        if (m->functions[0].instructions[i].opcode == ZR_EXEC_IR_OPCODE_CONSTANT)
            TEST_ASSERT_EQUAL_UINT32(0u, m->functions[0].instructions[i].layoutId);
}
static void test_multiline_source_maps_preserve_coordinates(void) { expression_case("return 2 +\n  3;", g_multiline, 3u); }
static void test_existing_literal_execir_profile_remains_valid(void) { expression_case("return 9;", g_literal, 1u); }

static void test_ordinary_expression_has_no_retained_owners(void) { ordinary_prerequisite("return 2 + 3 * 4;"); }
static void test_null_and_zero_options_do_not_retain_expression(void) {
    SZrSourceCompileOptions none = {0}; SZrSourceCompileDiagnostic d;
    const SZrSourceCompileOptions *options[2] = {ZR_NULL, &none};
    for (unsigned i = 0u; i < 2u; ++i) {
        SZrFunction *f; memset(&d, 0xA5, sizeof(d));
        f = ZrTests_Returned_Compile(i, "return 2 + 3;", ZR_FALSE, options[i], &d);
        TEST_ASSERT_NOT_NULL(f); zero_diagnostic(&d); TEST_ASSERT_NULL(f->ownedExecIr); TEST_ASSERT_NULL(f->ownedAotIr);
    }
}
static void unsupported(const char *source) {
    SZrSourceCompileDiagnostic d; ordinary_prerequisite(source); memset(&d, 0xA5, sizeof(d));
    TEST_ASSERT_NULL(ZrTests_Returned_Compile(0u, source, ZR_FALSE, &g_exec, &d));
    TEST_ASSERT_EQUAL_INT(ZR_SOURCE_RESULT_UNSUPPORTED, d.result);
    TEST_ASSERT_NULL(ZrTests_Returned_Function(0u));
    TEST_ASSERT_FALSE(d.hasAotIr);
}
static void test_bool_is_outside_canonical_i64_profile(void) { unsupported("return true;"); }
static void test_float_expression_is_not_admitted(void) { unsupported("return 2.0 + 3.0;"); }
static void test_division_is_not_admitted(void) { unsupported("return 8 / 2;"); }
static void test_comparison_is_not_admitted(void) { unsupported("return 2 < 3;"); }
static void test_local_read_is_not_admitted(void) { unsupported("var value = 2; return value + 3;"); }
static void test_control_flow_is_not_admitted(void) { unsupported("if (true) { return 2; } return 3;"); }

static void test_two_returned_owners_are_independent(void) {
    const char *source = "return 2 + 3;";
    SZrFunction *a = required(0u, source, ZR_TRUE), *b = required(1u, source, ZR_TRUE);
    const SZrExecIrModule *ma = assert_expression(a, source, g_add, 3u);
    const SZrExecIrModule *mb = assert_expression(b, source, g_add, 3u);
    TEST_ASSERT_TRUE(ma != mb); TEST_ASSERT_TRUE(ma->constants != mb->constants);
    TEST_ASSERT_TRUE(ma->functions != mb->functions);
    TEST_ASSERT_TRUE(ma->functions[0].instructions != mb->functions[0].instructions);
    TEST_ASSERT_TRUE(ma->functions[0].operandPool != mb->functions[0].operandPool);
    TEST_ASSERT_TRUE(ma->functions[0].frameLayout != mb->functions[0].frameLayout);
    TEST_ASSERT_TRUE(ma->functions[0].frameLayout->slots != mb->functions[0].frameLayout->slots);
    ZrTests_Returned_FreeFunction(0u);
    (void)assert_expression(b, source, g_add, 3u);
}
static void test_repeated_compile_free_preserves_graph(void) {
    for (unsigned i = 0u; i < 8u; ++i) {
        (void)assert_expression(required(0u, "return 2 + 3;", ZR_TRUE), "return 2 + 3;", g_add, 3u);
        ZrTests_Returned_FreeFunction(0u);
    }
}
static void test_null_diagnostic_still_retains_complete_expression(void) {
    (void)assert_expression(required(0u, "return 2 + 3 * 4;", ZR_FALSE), "return 2 + 3 * 4;", g_precedence, 5u);
}
static void test_refusal_preserves_other_returned_owner_and_retry(void) {
    const char *source = "return 2 + 3;";
    SZrSourceCompileDiagnostic d; SZrFunction *f = required(0u, source, ZR_TRUE);
    const SZrExecIrModule *before = assert_expression(f, source, g_add, 3u);
    memset(&d, 0xA5, sizeof(d));
    TEST_ASSERT_NULL(ZrTests_Returned_Compile(1u, "return true;", ZR_FALSE, &g_exec, &d));
    TEST_ASSERT_EQUAL_INT(ZR_SOURCE_RESULT_UNSUPPORTED, d.result);
    TEST_ASSERT_EQUAL_PTR(before, ZrCore_Function_GetExecIr(f));
    (void)assert_expression(f, source, g_add, 3u);
    (void)assert_expression(required(1u, source, ZR_TRUE), source, g_add, 3u);
}
static void test_literal_aot_mode_remains_separate(void) {
    SZrSourceCompileDiagnostic d; SZrAotIrDiagnostic ad = {0}; SZrFunction *f;
    const SZrAotIrModule *m; memset(&d, 0xA5, sizeof(d));
    f = ZrTests_Returned_Compile(0u, "return 9;", ZR_FALSE, &g_aot, &d);
    TEST_ASSERT_NOT_NULL(f); zero_diagnostic(&d); TEST_ASSERT_NULL(f->ownedExecIr);
    m = ZrCore_Function_GetAotIr(f); TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(m, &ad));
    TEST_ASSERT_EQUAL_UINT32(1u, m->constantCount); TEST_ASSERT_EQUAL_UINT64(9u, m->constantPool[0].bits);
}
static void test_expression_does_not_silently_expand_aot_mode(void) {
    SZrSourceCompileDiagnostic d; ordinary_prerequisite("return 2 + 3;"); memset(&d, 0xA5, sizeof(d));
    TEST_ASSERT_NULL(ZrTests_Returned_Compile(0u, "return 2 + 3;", ZR_FALSE, &g_aot, &d));
    TEST_ASSERT_EQUAL_INT(ZR_SOURCE_RESULT_UNSUPPORTED, d.result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_add_preserves_real_operator_and_operands);
    RUN_TEST(test_subtract_preserves_left_associativity);
    RUN_TEST(test_multiply_preserves_real_operator);
    RUN_TEST(test_precedence_retains_mul_then_add_without_evaluation);
    RUN_TEST(test_parentheses_change_actual_operand_graph);
    RUN_TEST(test_duplicate_literals_preserve_actual_pool_indices);
    RUN_TEST(test_multiline_source_maps_preserve_coordinates);
    RUN_TEST(test_existing_literal_execir_profile_remains_valid);
    RUN_TEST(test_ordinary_expression_has_no_retained_owners);
    RUN_TEST(test_null_and_zero_options_do_not_retain_expression);
    RUN_TEST(test_bool_is_outside_canonical_i64_profile);
    RUN_TEST(test_float_expression_is_not_admitted);
    RUN_TEST(test_division_is_not_admitted);
    RUN_TEST(test_comparison_is_not_admitted);
    RUN_TEST(test_local_read_is_not_admitted);
    RUN_TEST(test_control_flow_is_not_admitted);
    RUN_TEST(test_two_returned_owners_are_independent);
    RUN_TEST(test_repeated_compile_free_preserves_graph);
    RUN_TEST(test_null_diagnostic_still_retains_complete_expression);
    RUN_TEST(test_refusal_preserves_other_returned_owner_and_retry);
    RUN_TEST(test_literal_aot_mode_remains_separate);
    RUN_TEST(test_expression_does_not_silently_expand_aot_mode);
    return UNITY_END();
}
