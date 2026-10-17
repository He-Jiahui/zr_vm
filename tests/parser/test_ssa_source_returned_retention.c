#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "unity.h"
#include "support/ssa_host_aot_support.h"
#include "support/ssa_source_returned_retention_fixture.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_parser/comptime_cache.h"

static const char g_nine[] = "return 9;";
static const char g_eight[] = "return 8;";
static const char g_bool[] = "return true;";
static const char g_float[] = "return 9.0;";
static const char g_syntax[] = "return 9 + ;";
static const SZrSourceCompileOptions g_require = {ZR_SOURCE_RETENTION_REQUIRE_SCALAR_AOT};
void setUp(void) { ZrTests_Returned_Init(); }
void tearDown(void) { ZrTests_Returned_Close(); ZrTests_Returned_AssertBalanced(); }

static void diagnostic_equal(const SZrSourceCompileDiagnostic *expected, const SZrSourceCompileDiagnostic *actual) {
#define FIELD(f) TEST_ASSERT_EQUAL_UINT64(expected->f, actual->f)
    FIELD(stage); FIELD(result); FIELD(hasExecIr); FIELD(hasAotIr); FIELD(hasRange);
    FIELD(execIr.code); FIELD(execIr.functionToken); FIELD(execIr.blockId); FIELD(execIr.instructionId);
    FIELD(execIr.sourceId); FIELD(execIr.expectedVersion); FIELD(execIr.actualVersion);
    FIELD(execIr.expectedHash); FIELD(execIr.actualHash);
    FIELD(aotIr.status); FIELD(aotIr.functionId); FIELD(aotIr.blockId); FIELD(aotIr.instructionId);
    FIELD(aotIr.index); FIELD(aotIr.expected); FIELD(aotIr.actual);
    FIELD(startOffset); FIELD(endOffset);
#undef FIELD
#define COORD(f) TEST_ASSERT_EQUAL_INT32(expected->f, actual->f)
    COORD(startLine); COORD(startColumn); COORD(endLine); COORD(endColumn);
#undef COORD
}
static void success_diagnostic(const SZrSourceCompileDiagnostic *actual) {
    SZrSourceCompileDiagnostic zero = {0}; diagnostic_equal(&zero, actual);
}
static void no_crt_allocations(void) {
    for (unsigned i = 0u; i < 2u; ++i) {
        SZrReturnedHeapStats s = ZrTests_Returned_Stats(i);
        TEST_ASSERT_EQUAL_UINT(0u, s.attempts); TEST_ASSERT_EQUAL_UINT(0u, s.live);
        TEST_ASSERT_EQUAL_UINT(0u, s.errors);
    }
}
static SZrFunction *ordinary(unsigned slot, const char *source, TZrBool direct,
                             const SZrSourceCompileOptions *options) {
    SZrSourceCompileDiagnostic d;
    SZrFunction *f;
    memset(&d, 0xA5, sizeof(d));
    f = ZrTests_Returned_Compile(slot, source, direct, options, &d);
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "ordinary real source wrapper prerequisite");
    if (!direct) success_diagnostic(&d);
    TEST_ASSERT_NULL(f->ownedAotIr); TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(f));
    TEST_ASSERT_NOT_NULL(f->constantValueList); TEST_ASSERT_EQUAL_UINT32(1u, f->constantValueLength);
    if (f->constantValueList[0].type == ZR_VALUE_TYPE_INT64) {
        TEST_ASSERT_TRUE(f->metadataTokenRecordLength > 0u); TEST_ASSERT_NOT_NULL(f->metadataTokenRecords);
        TEST_ASSERT_NOT_EQUAL_UINT64(0u, f->moduleSignatureHash);
        TEST_ASSERT_TRUE(f->hasSourceCallableIdentity);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1, f->sourceCallableIdentity.schemaVersion);
        TEST_ASSERT_NOT_EQUAL_UINT64(0u, f->sourceCallableIdentity.canonicalSignatureHash);
    }
    no_crt_allocations(); return f;
}
static const SZrAotIrModule *retained(unsigned slot, const char *source, TZrInt64 literal, TZrBool withDiagnostic) {
    SZrSourceCompileDiagnostic d;
    SZrAotIrDiagnostic a = {0}; SZrAotIrCallableAbi abi = {0};
    SZrFunction *runtime;
    const SZrAotIrModule *m;
    const SZrAotIrFunction *f;
    const SZrMetadataTokenRecord *entry = ZR_NULL, *module = ZR_NULL;
    memset(&d, 0xA5, sizeof(d));
    runtime = ZrTests_Returned_Compile(slot, source, ZR_FALSE, &g_require, withDiagnostic ? &d : ZR_NULL);
    {
        SZrReturnedHeapStats descriptor = ZrTests_Returned_Stats(ZR_RETURNED_DESCRIPTOR);
        TEST_ASSERT_EQUAL_UINT(0u, descriptor.live); TEST_ASSERT_EQUAL_UINT(0u, descriptor.errors);
        TEST_ASSERT_EQUAL_UINT(descriptor.successes, descriptor.frees);
    }
    fprintf(stderr, "RETURNED_REQUIRE returned=%u stage=%u result=%u\n", (unsigned)(runtime != ZR_NULL),
            withDiagnostic ? (unsigned)d.stage : 0u, withDiagnostic ? (unsigned)d.result : 0u);
    TEST_ASSERT_NOT_NULL_MESSAGE(runtime, "real options wrapper must return retained literal Function");
    if (withDiagnostic) success_diagnostic(&d);
    TEST_ASSERT_NOT_NULL(runtime->ownedAotIr); m = ZrCore_Function_GetAotIr(runtime); TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(m, &a));
    TEST_ASSERT_EQUAL_UINT32(1u, m->functionCount); TEST_ASSERT_NOT_NULL(m->functions); f = &m->functions[0];
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_RequireExecutableAbi(m, f->id, &abi, &a));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, abi.kind);
    TEST_ASSERT_EQUAL_UINT32(f->callableAbi.returnTypeToken, abi.returnTypeToken);
    TEST_ASSERT_EQUAL_UINT32(1u, m->constantCount); TEST_ASSERT_EQUAL_UINT32(1u, m->layoutCount);
    TEST_ASSERT_NOT_NULL(m->constantPool); TEST_ASSERT_NOT_NULL(m->layoutPool);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)literal, m->constantPool[0].bits);
    TEST_ASSERT_EQUAL_UINT32(0u, m->constantPool[0].flags);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, runtime->constantValueList[0].type);
    TEST_ASSERT_EQUAL_INT64(literal, runtime->constantValueList[0].value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(m->constantPool[0].typeToken, m->layoutPool[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(m->constantPool[0].typeToken, abi.returnTypeToken);
    TEST_ASSERT_EQUAL_UINT64(runtime->moduleSignatureHash, m->moduleHash);
    TEST_ASSERT_EQUAL_UINT64(m->moduleHash, m->contract.moduleHash);
    TEST_ASSERT_EQUAL_UINT64(m->moduleHash, f->contract.moduleHash);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXECUTION_CONTRACT_SCHEMA_VERSION, m->contract.schemaVersion);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXECUTION_CONTRACT_ABI_VERSION, m->contract.abiVersion);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXECUTION_CONTRACT_LOGICAL_VERSION, m->contract.logicalVersion);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXECUTION_CONTRACT_SCHEMA_VERSION, f->contract.schemaVersion);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXECUTION_CONTRACT_ABI_VERSION, f->contract.abiVersion);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXECUTION_CONTRACT_LOGICAL_VERSION, f->contract.logicalVersion);
    TEST_ASSERT_EQUAL_UINT64(runtime->sourceCallableIdentity.canonicalSignatureHash, f->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(f->signatureHash, f->contract.signatureHash);
    TEST_ASSERT_EQUAL_UINT32(f->functionToken, f->contract.targetToken);
    for (TZrUInt32 i = 0u; i < runtime->metadataTokenRecordLength; ++i) {
        const SZrMetadataTokenRecord *r = &runtime->metadataTokenRecords[i];
        if (ZR_METADATA_TOKEN_TABLE(r->token) == ZR_METADATA_TABLE_MODULE) { TEST_ASSERT_NULL(module); module = r; }
        if (ZR_METADATA_TOKEN_TABLE(r->token) == ZR_METADATA_TABLE_MEMBER_DEF &&
            r->reserved0 == ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY) { TEST_ASSERT_NULL(entry); entry = r; }
    }
    TEST_ASSERT_NOT_NULL(entry); TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_UINT32(entry->token, f->functionToken);
    TEST_ASSERT_EQUAL_UINT32(module->token, entry->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(module->token, m->contract.targetToken);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, m->contract.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(1u, m->contract.generation); TEST_ASSERT_EQUAL_UINT64(1u, f->contract.generation);
    TEST_ASSERT_EQUAL_UINT64(0u, m->contract.layoutHash);
    TEST_ASSERT_EQUAL_UINT64(f->frameLayout.layoutHash, f->contract.layoutHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, f->frameLayout.layoutHash);
    TEST_ASSERT_EQUAL_UINT32(1u, f->frameSlotCount); TEST_ASSERT_NOT_NULL(f->frameSlots);
    TEST_ASSERT_EQUAL_UINT32(1u, f->valueSlotCount); TEST_ASSERT_NOT_NULL(f->valueSlotPool);
    TEST_ASSERT_EQUAL_UINT32(1u, f->frameLayout.storageSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, f->frameLayout.logicalSlotCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->valueSlotPool[0]); TEST_ASSERT_EQUAL_UINT32(1u, f->frameSlots[0].slotId);
    TEST_ASSERT_EQUAL_UINT32(m->constantPool[0].typeToken, f->frameSlots[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(m->layoutPool[0].byteSize, f->frameSlots[0].byteSize);
    TEST_ASSERT_EQUAL_UINT32(m->layoutPool[0].byteAlign, f->frameSlots[0].byteAlign);
    TEST_ASSERT_EQUAL_UINT32(3u, f->instructionCount); TEST_ASSERT_EQUAL_UINT32(1u, f->blockCount);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_CONSTANT, f->instructions[0].opcode);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_NOP, f->instructions[1].opcode);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_RETURN, f->instructions[2].opcode);
    TEST_ASSERT_EQUAL_UINT32(0u, f->instructions[0].layoutId);
    TEST_ASSERT_EQUAL_UINT32(3u, f->sourceMapCount); TEST_ASSERT_NOT_NULL(f->sourceMaps);
    for (unsigned i = 0u; i < 3u; ++i) {
        TEST_ASSERT_EQUAL_UINT32(i + 1u, f->sourceMaps[i].instructionId);
        TEST_ASSERT_EQUAL_UINT32(f->instructions[i].sourceId, f->sourceMaps[i].sourceId);
        TEST_ASSERT_TRUE(f->sourceMaps[i].startOffset <= f->sourceMaps[i].endOffset);
        TEST_ASSERT_TRUE(f->sourceMaps[i].endOffset <= strlen(source));
        TEST_ASSERT_TRUE(f->sourceMaps[i].startLine > 0u && f->sourceMaps[i].endLine > 0u);
    }
    TEST_ASSERT_NOT_NULL(f->logicalStateMap);
    TEST_ASSERT_EQUAL_UINT32(f->functionToken, f->logicalStateMap->functionToken);
    TEST_ASSERT_EQUAL_UINT64(f->signatureHash, f->logicalStateMap->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(f->contract.generation, f->logicalStateMap->generation);
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->entryCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->valueCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->rootCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->ownerStateCount);
    TEST_ASSERT_NULL(f->logicalStateMap->entries); TEST_ASSERT_NULL(f->logicalStateMap->valuePool);
    TEST_ASSERT_NULL(f->logicalStateMap->rootPool); TEST_ASSERT_NULL(f->logicalStateMap->ownerStatePool);
    TEST_ASSERT_TRUE((const void *)m->constantPool != (const void *)runtime->constantValueList);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, ZrCore_AotIr_HashModule(m));
    {
        SZrReturnedHeapStats descriptor = ZrTests_Returned_Stats(ZR_RETURNED_DESCRIPTOR);
        SZrReturnedHeapStats owner = ZrTests_Returned_Stats(ZR_RETURNED_OWNER);
        TEST_ASSERT_EQUAL_UINT(0u, descriptor.live); TEST_ASSERT_EQUAL_UINT(0u, descriptor.errors);
        TEST_ASSERT_EQUAL_UINT(descriptor.successes, descriptor.frees);
        TEST_ASSERT_EQUAL_UINT(0u, owner.errors); TEST_ASSERT_TRUE(owner.live > 0u);
    }
    return m;
}
static void free_retained(unsigned slot) {
    unsigned record = ZrTests_Returned_OwnerRecord(slot);
    ZrTests_Returned_FreeFunction(slot);
    TEST_ASSERT_EQUAL_UINT(1u, ZrTests_Returned_RecordFrees(ZR_RETURNED_OWNER, record));
}
static void test_ordinary_none_nine_returns_no_owner(void) {
    SZrFunction *f = ordinary(0u, g_nine, ZR_TRUE, ZR_NULL);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, f->constantValueList[0].type);
    TEST_ASSERT_EQUAL_INT64(9, f->constantValueList[0].value.nativeObject.nativeInt64);
}
static void test_zero_options_eight_returns_no_owner(void) {
    SZrSourceCompileOptions zero = {0}; SZrFunction *f = ordinary(0u, g_eight, ZR_FALSE, &zero);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, f->constantValueList[0].type);
    TEST_ASSERT_EQUAL_INT64(8, f->constantValueList[0].value.nativeObject.nativeInt64);
}
static void test_null_options_nine_returns_no_owner(void) { (void)ordinary(0u, g_nine, ZR_FALSE, ZR_NULL); }
static void test_ordinary_bool_literal_prerequisite(void) {
    SZrFunction *f = ordinary(0u, g_bool, ZR_TRUE, ZR_NULL);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, f->constantValueList[0].type);
}
static void test_ordinary_float_literal_prerequisite(void) {
    SZrFunction *f = ordinary(0u, g_float, ZR_TRUE, ZR_NULL);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, f->constantValueList[0].type);
}
static void test_incomplete_expression_parse_refusal_prerequisite(void) {
    TEST_ASSERT_NULL(ZrTests_Returned_Compile(0u, g_syntax, ZR_TRUE, ZR_NULL, ZR_NULL)); no_crt_allocations();
}
static void test_require_nine_retains_metadata_after_wrapper_cleanup(void) { (void)retained(0u, g_nine, 9, ZR_TRUE); }
static void test_require_eight_retains_metadata_after_wrapper_cleanup(void) { (void)retained(0u, g_eight, 8, ZR_TRUE); }
static void invalid_options(unsigned kind) {
    SZrSourceCompileOptions unknown = {(EZrSourceRetention)2};
    SZrSourceCompileDiagnostic d, expected = {0};
    SZrState *state = ZrTests_Returned_State(); SZrString *name = ZrTests_Returned_Name();
    unsigned attempts = ZrTests_Returned_VmAttempts(); SZrFunction *result;
    memset(&d, 0xA5, sizeof(d));
    result = ZrParser_Source_CompileWithOptions(kind == 1u ? ZR_NULL : state,
            kind == 2u ? ZR_NULL : g_nine, kind == 3u ? 0u : strlen(g_nine), name,
            kind == 0u ? &unknown : &g_require, &d);
    TEST_ASSERT_NULL(result); expected.stage = ZR_SOURCE_STAGE_OPTIONS; expected.result = ZR_SOURCE_RESULT_INVALID_ARGUMENT;
    diagnostic_equal(&expected, &d); TEST_ASSERT_EQUAL_UINT(attempts, ZrTests_Returned_VmAttempts()); no_crt_allocations();
}
static void test_unknown_retention_option_is_rejected_before_parse(void) { invalid_options(0u); }
static void test_null_required_state_is_rejected(void) { invalid_options(1u); }
static void test_null_required_source_is_rejected(void) {
    SZrParserSourceComptimeCache cache = {0};
    TZrByte sentinel = 1u;
    SZrString *name;
    unsigned attempts;
    invalid_options(2u); invalid_options(3u);
    name = ZrTests_Returned_Name(); attempts = ZrTests_Returned_VmAttempts();
    cache.outputSnapshot = &sentinel; cache.outputSnapshotSize = 7u;
    cache.inputSnapshotAccepted = ZR_TRUE; cache.hitCount = 3u; cache.missCount = 5u;
    TEST_ASSERT_NULL(ZrParser_Source_CompileWithComptimeCache(ZrTests_Returned_State(), ZR_NULL, 9u, name, &cache));
    TEST_ASSERT_NULL(cache.outputSnapshot); TEST_ASSERT_EQUAL_UINT64(0u, cache.outputSnapshotSize);
    TEST_ASSERT_FALSE(cache.inputSnapshotAccepted); TEST_ASSERT_EQUAL_UINT64(0u, cache.hitCount);
    TEST_ASSERT_EQUAL_UINT64(0u, cache.missCount); TEST_ASSERT_EQUAL_UINT(attempts, ZrTests_Returned_VmAttempts());
    no_crt_allocations();
}
static void test_require_incomplete_expression_reports_parse_failure(void) {
    SZrSourceCompileDiagnostic d, expected = {0};
    memset(&d, 0xA5, sizeof(d));
    TEST_ASSERT_NULL(ZrTests_Returned_Compile(0u, g_syntax, ZR_FALSE, &g_require, &d));
    TEST_ASSERT_TRUE_MESSAGE(d.hasRange, "actual incomplete-expression callback must provide numeric range");
    TEST_ASSERT_TRUE(d.startOffset <= d.endOffset && d.endOffset <= strlen(g_syntax));
    TEST_ASSERT_TRUE(d.startLine > 0 && d.startColumn > 0 && d.endLine > 0 && d.endColumn > 0);
    expected.stage = ZR_SOURCE_STAGE_PARSE; expected.result = ZR_SOURCE_RESULT_PARSE_FAILED; expected.hasRange = ZR_TRUE;
    expected.startOffset = d.startOffset; expected.endOffset = d.endOffset;
    expected.startLine = d.startLine; expected.startColumn = d.startColumn;
    expected.endLine = d.endLine; expected.endColumn = d.endColumn;
    diagnostic_equal(&expected, &d); no_crt_allocations();
}
static void unsupported(const char *source) {
    SZrSourceCompileDiagnostic d, expected = {0};
    (void)ordinary(0u, source, ZR_TRUE, ZR_NULL); ZrTests_Returned_FreeFunction(0u);
    memset(&d, 0xA5, sizeof(d));
    TEST_ASSERT_NULL(ZrTests_Returned_Compile(0u, source, ZR_FALSE, &g_require, &d));
    fprintf(stderr, "RETURNED_UNSUPPORTED stage=%u result=%u exec=%u aot=%u range=%u\n",
            (unsigned)d.stage, (unsigned)d.result, (unsigned)d.hasExecIr, (unsigned)d.hasAotIr, (unsigned)d.hasRange);
    expected.stage = ZR_SOURCE_STAGE_CONSTANT; expected.result = ZR_SOURCE_RESULT_UNSUPPORTED;
    expected.hasExecIr = ZR_TRUE; expected.execIr.code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
    expected.execIr.expectedHash = 1u;
    diagnostic_equal(&expected, &d);
    no_crt_allocations();
}
static void test_require_bool_literal_is_refused(void) { unsupported(g_bool); }
static void test_require_float_literal_is_refused(void) { unsupported(g_float); }
static void test_require_without_diagnostic_retains_metadata(void) { (void)retained(0u, g_nine, 9, ZR_FALSE); }
static void test_two_live_returned_owners_remain_independent(void) {
    const SZrAotIrModule *a = retained(0u, g_nine, 9, ZR_TRUE);
    const SZrAotIrModule *b = retained(1u, g_eight, 8, ZR_TRUE);
    TZrUInt64 hash = ZrCore_AotIr_HashModule(b);
    TEST_ASSERT_TRUE(a != b); TEST_ASSERT_TRUE(a->constantPool != b->constantPool);
    TEST_ASSERT_TRUE(a->functions != b->functions);
    TEST_ASSERT_TRUE(a->functions[0].instructions != b->functions[0].instructions);
    free_retained(0u); /* a is never observed again. */
    TEST_ASSERT_EQUAL_UINT64(hash, ZrCore_AotIr_HashModule(b));
    TEST_ASSERT_EQUAL_UINT64(8u, b->constantPool[0].bits); free_retained(1u);
    TEST_ASSERT_EQUAL_UINT(0u, ZrTests_Returned_Stats(ZR_RETURNED_OWNER).live);
}
static void test_explicit_free_and_global_shutdown_release_once(void) {
    unsigned a, b;
    (void)retained(0u, g_nine, 9, ZR_TRUE); (void)retained(1u, g_eight, 8, ZR_TRUE);
    a = ZrTests_Returned_OwnerRecord(0u); b = ZrTests_Returned_OwnerRecord(1u);
    free_retained(0u); ZrTests_Returned_Shutdown();
    TEST_ASSERT_EQUAL_UINT(1u, ZrTests_Returned_RecordFrees(ZR_RETURNED_OWNER, a));
    TEST_ASSERT_EQUAL_UINT(1u, ZrTests_Returned_RecordFrees(ZR_RETURNED_OWNER, b)); ZrTests_Returned_AssertBalanced();
}
static void test_sixteen_alternating_compile_free_calls_balance(void) {
    for (unsigned i = 0u; i < 16u; ++i) {
        (void)retained(0u, (i & 1u) ? g_eight : g_nine, (i & 1u) ? 8 : 9, ZR_TRUE);
        free_retained(0u); TEST_ASSERT_EQUAL_UINT(0u, ZrTests_Returned_Stats(ZR_RETURNED_OWNER).live);
        TEST_ASSERT_EQUAL_UINT(0u, ZrTests_Returned_Stats(ZR_RETURNED_DESCRIPTOR).live);
    }
    TEST_ASSERT_EQUAL_UINT(16u, ZrTests_Returned_Stats(ZR_RETURNED_OWNER).successes);
    ZrTests_Returned_Shutdown(); ZrTests_Returned_AssertBalanced();
}
static void integrated_fault(unsigned site, TZrBool ownerFault) {
    SZrSourceCompileDiagnostic d, expected = {0};
    SZrReturnedHeapStats descriptor, owner;
    SZrReturnedHeapEvent failure;
    if (ownerFault) ZrTests_Returned_ArmOwner(); else ZrTests_Returned_ArmDescriptor(site);
    memset(&d, 0xA5, sizeof(d));
    TEST_ASSERT_NULL(ZrTests_Returned_Compile(0u, g_nine, ZR_FALSE, &g_require, &d));
    descriptor = ZrTests_Returned_Stats(ZR_RETURNED_DESCRIPTOR); owner = ZrTests_Returned_Stats(ZR_RETURNED_OWNER);
    TEST_ASSERT_EQUAL_UINT(ownerFault ? 6u : site, descriptor.attempts);
    TEST_ASSERT_EQUAL_UINT(ownerFault ? 6u : site - 1u, descriptor.successes);
    TEST_ASSERT_EQUAL_UINT(descriptor.successes, descriptor.frees); TEST_ASSERT_EQUAL_UINT(0u, descriptor.live);
    TEST_ASSERT_EQUAL_UINT(0u, descriptor.errors); TEST_ASSERT_EQUAL_UINT(ownerFault ? 0u : 1u, descriptor.failures);
    TEST_ASSERT_EQUAL_UINT(ownerFault ? 1u : 0u, owner.attempts);
    TEST_ASSERT_EQUAL_UINT(0u, owner.successes); TEST_ASSERT_EQUAL_UINT(0u, owner.live); TEST_ASSERT_EQUAL_UINT(0u, owner.errors);
    TEST_ASSERT_EQUAL_UINT(ownerFault ? 1u : 0u, owner.failures);
    for (unsigned i = 0u; i < descriptor.successes; ++i)
        TEST_ASSERT_EQUAL_UINT(1u, ZrTests_Returned_RecordFrees(ZR_RETURNED_DESCRIPTOR, i));
    failure = ZrTests_Returned_Event(ownerFault ? ZR_RETURNED_OWNER : ZR_RETURNED_DESCRIPTOR, ownerFault ? 0u : site - 1u);
    TEST_ASSERT_TRUE(failure.injected); TEST_ASSERT_TRUE(failure.bytes > 0u);
    expected.stage = ownerFault ? ZR_SOURCE_STAGE_OWNER : ZR_SOURCE_STAGE_DESCRIPTOR;
    expected.result = ZR_SOURCE_RESULT_OUT_OF_MEMORY; expected.hasAotIr = ZR_TRUE;
    expected.aotIr.status = ZR_AOT_IR_OUT_OF_MEMORY; expected.aotIr.expected = failure.bytes;
    diagnostic_equal(&expected, &d);
    (void)retained(0u, g_nine, 9, ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT(descriptor.attempts + 6u, ZrTests_Returned_Stats(ZR_RETURNED_DESCRIPTOR).attempts);
    TEST_ASSERT_EQUAL_UINT(owner.attempts + 1u, ZrTests_Returned_Stats(ZR_RETURNED_OWNER).attempts);
    TEST_ASSERT_EQUAL_UINT(0u, ZrTests_Returned_Stats(ZR_RETURNED_DESCRIPTOR).live); free_retained(0u);
}
static void test_returned_descriptor_instruction_oom_cleanup_retry(void) { integrated_fault(1u, ZR_FALSE); }
static void test_returned_descriptor_block_oom_cleanup_retry(void) { integrated_fault(2u, ZR_FALSE); }
static void test_returned_descriptor_frame_slot_oom_cleanup_retry(void) { integrated_fault(3u, ZR_FALSE); }
static void test_returned_descriptor_source_map_oom_cleanup_retry(void) { integrated_fault(4u, ZR_FALSE); }
static void test_returned_descriptor_constant_oom_cleanup_retry(void) { integrated_fault(5u, ZR_FALSE); }
static void test_returned_descriptor_layout_oom_cleanup_retry(void) { integrated_fault(6u, ZR_FALSE); }
static void test_returned_owner_oom_cleanup_retry(void) { integrated_fault(0u, ZR_TRUE); }
#if !ZR_TESTS_HOST_SCALAR_AOT_SUPPORTED
static void test_required_aot_reports_unsupported_host_without_owners(void) {
    const char *sources[] = {g_nine, g_eight};
    for (unsigned index = 0u; index < 2u; ++index) {
        SZrSourceCompileDiagnostic d = {0};
        TEST_ASSERT_NULL(ZrTests_Returned_Compile(index, sources[index], ZR_FALSE, &g_require, &d));
        TEST_ASSERT_EQUAL_INT(ZR_SOURCE_STAGE_TARGET, d.stage);
        TEST_ASSERT_EQUAL_INT(ZR_SOURCE_RESULT_UNSUPPORTED, d.result);
        TEST_ASSERT_TRUE(d.hasAotIr);
        TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_UNSUPPORTED, d.aotIr.status);
        no_crt_allocations();
    }
}
#endif
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ordinary_none_nine_returns_no_owner);
    RUN_TEST(test_zero_options_eight_returns_no_owner);
    RUN_TEST(test_null_options_nine_returns_no_owner);
    RUN_TEST(test_ordinary_bool_literal_prerequisite);
    RUN_TEST(test_ordinary_float_literal_prerequisite);
    RUN_TEST(test_incomplete_expression_parse_refusal_prerequisite);
#if ZR_TESTS_HOST_SCALAR_AOT_SUPPORTED
    RUN_TEST(test_require_nine_retains_metadata_after_wrapper_cleanup);
    RUN_TEST(test_require_eight_retains_metadata_after_wrapper_cleanup);
#else
    RUN_TEST(test_required_aot_reports_unsupported_host_without_owners);
#endif
    RUN_TEST(test_unknown_retention_option_is_rejected_before_parse);
    RUN_TEST(test_null_required_state_is_rejected);
    RUN_TEST(test_null_required_source_is_rejected);
    RUN_TEST(test_require_incomplete_expression_reports_parse_failure);
    RUN_TEST(test_require_bool_literal_is_refused);
    RUN_TEST(test_require_float_literal_is_refused);
#if ZR_TESTS_HOST_SCALAR_AOT_SUPPORTED
    RUN_TEST(test_require_without_diagnostic_retains_metadata);
    RUN_TEST(test_two_live_returned_owners_remain_independent);
    RUN_TEST(test_explicit_free_and_global_shutdown_release_once);
    RUN_TEST(test_sixteen_alternating_compile_free_calls_balance);
    RUN_TEST(test_returned_descriptor_instruction_oom_cleanup_retry);
    RUN_TEST(test_returned_descriptor_block_oom_cleanup_retry);
    RUN_TEST(test_returned_descriptor_frame_slot_oom_cleanup_retry);
    RUN_TEST(test_returned_descriptor_source_map_oom_cleanup_retry);
    RUN_TEST(test_returned_descriptor_constant_oom_cleanup_retry);
    RUN_TEST(test_returned_descriptor_layout_oom_cleanup_retry);
    RUN_TEST(test_returned_owner_oom_cleanup_retry);
#endif
    return UNITY_END();
}
