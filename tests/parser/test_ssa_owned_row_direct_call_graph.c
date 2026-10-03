#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/call_binding.h"
#include "zr_vm_parser/exec_ir_interprocedural.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s line %u: %s\n", currentCase, \
            (unsigned)__LINE__, #condition); exit(EXIT_FAILURE); \
} } while (0)

static const char *currentCase = "startup";
static unsigned caseCount;
static unsigned selectedHashMode = UINT32_MAX;
static const TZrUInt64 moduleHash = UINT64_C(0x77224466);
static const TZrUInt64 signatureHash = UINT64_C(0x11335577);

typedef struct Fixture {
    SZrExecIrModule module;
    SZrExecIrCallGraph graph;
    TZrUInt32 callSiteCount;
} Fixture;

static SZrExecIrFunction *function(Fixture *fixture, TZrExecIrFunctionId id) {
    SZrExecIrFunction *result = ZrCore_ExecIr_ModuleFunctionAt(&fixture->module, id);
    CHECK(result != ZR_NULL);
    return result;
}

static TZrExecIrValueId constant_value(SZrExecIrFunction *owner) {
    SZrExecIrInstruction instruction = {0};
    SZrExecIrRange results;
    TZrExecIrInstructionId id;
    TZrExecIrValueId value = ZrCore_ExecIr_FunctionAddValue(owner,
        ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
        ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    CHECK(value != ZR_EXEC_IR_VALUE_ID_INVALID);
    CHECK(ZrCore_ExecIr_FunctionAppendResults(owner, &value, 1u, &results));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = results;
    CHECK(ZrCore_ExecIr_FunctionAppendInstruction(owner, &instruction, &id));
    return value;
}

static void return_value(SZrExecIrFunction *owner, TZrExecIrBlockId blockId,
                         TZrExecIrValueId value) {
    SZrExecIrInstruction instruction = {0};
    SZrExecIrBlock *block = &owner->blocks[blockId - 1u];
    TZrExecIrInstructionId id;
    CHECK(ZrCore_ExecIr_FunctionAppendOperands(owner, &value, 1u,
                                              &instruction.operands));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    CHECK(ZrCore_ExecIr_FunctionAppendInstruction(owner, &instruction, &id));
    block->instructions.count = id - block->instructions.start;
    block->terminatorInstructionId = id;
}

static void call_value(SZrExecIrFunction *owner, TZrBool invoke) {
    SZrExecIrInstruction instruction = {0};
    TZrExecIrInstructionId id;
    TZrExecIrValueId input = constant_value(owner);
    TZrExecIrValueId output = ZrCore_ExecIr_FunctionAddValue(owner,
        ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
        ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    const TZrExecIrMemoryTokenId inputs[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u)
    };
    const TZrExecIrMemoryTokenId outputs[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u)
    };
    CHECK(output != ZR_EXEC_IR_VALUE_ID_INVALID);
    instruction.opcode = invoke ? ZR_EXEC_IR_OPCODE_INVOKE : ZR_EXEC_IR_OPCODE_CALL;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
    instruction.sourceId = 502u;
    /* Both legacy hints name decoy 2; row ref 1 aliases caller 1. */
    instruction.layoutId = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 102u);
    instruction.typeToken = instruction.layoutId;
    CHECK(ZrCore_ExecIr_FunctionAppendOperands(owner, &input, 1u,
                                              &instruction.operands));
    CHECK(ZrCore_ExecIr_FunctionAppendResults(owner, &output, 1u,
                                             &instruction.results));
    CHECK(ZrCore_ExecIr_FunctionAppendMemoryTokens(owner, inputs, 2u,
                                                  &instruction.memoryIn));
    CHECK(ZrCore_ExecIr_FunctionAppendMemoryTokens(owner, outputs, 2u,
                                                  &instruction.memoryOut));
    instruction.effectIn = 1u;
    instruction.effectOut = 2u;
    if (invoke) {
        const TZrExecIrBlockId successors[2] = {2u, 3u};
        const TZrExecIrBlockId predecessor = 1u;
        CHECK(ZrCore_ExecIr_FunctionAddBlock(owner, 0u) == 2u);
        CHECK(ZrCore_ExecIr_FunctionAddBlock(owner,
                    ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) == 3u);
        CHECK(ZrCore_ExecIr_FunctionAppendSuccessors(owner, successors, 2u,
                    &owner->blocks[0].successorRange));
        CHECK(ZrCore_ExecIr_FunctionAppendPredecessors(owner, &predecessor, 1u,
                    &owner->blocks[1].predecessorRange));
        CHECK(ZrCore_ExecIr_FunctionAppendPredecessors(owner, &predecessor, 1u,
                    &owner->blocks[2].predecessorRange));
        instruction.successorRange = owner->blocks[0].successorRange;
    }
    CHECK(ZrCore_ExecIr_FunctionAppendInstruction(owner, &instruction, &id));
    CHECK(id == 2u);
    if (invoke) {
        owner->blocks[0].instructions.count = 2u;
        owner->blocks[0].terminatorInstructionId = id;
        owner->blocks[1].instructions.start = 2u;
        return_value(owner, 2u, output);
        owner->blocks[2].instructions.start = 3u;
        return_value(owner, 3u, input);
    } else {
        return_value(owner, 1u, output);
    }
}

static SZrExecIrBindingRow direct_row(TZrMetadataToken target) {
    SZrExecIrBindingRow row = {0};
    row.instructionId = 2u;
    row.segmentIndex = ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE;
    row.sourceId = 502u;
    row.contract.bindingKind = ZR_CALL_BINDING_DIRECT;
    row.contract.targetMetadataToken = target;
    row.contract.signatureToken =
        ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    row.contract.signatureHash = signatureHash;
    row.contract.moduleSignatureHash = moduleHash;
    row.contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    row.contract.operation = ZR_CALL_BINDING_OPERATION_CALL;
    row.location.kind = ZR_CALL_BINDING_RELOCATION_NONE;
    row.location.targetIndex = ZR_CALL_BINDING_SLOT_NONE;
    return row;
}

static void set_rows(Fixture *fixture, const SZrExecIrBindingRow *row,
                     TZrUInt32 count) {
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrFunction *caller = function(fixture, 1u);
    caller->sealed = ZR_FALSE;
    CHECK(ZrCore_ExecIr_FunctionSetBindingRows(caller, row, count, &diagnostic));
}

static void fixture_init_mode(Fixture *fixture, TZrBool invoke,
                              TZrBool nestedCall) {
    SZrExecIrConstant constants[2] = {
        {ZR_VALUE_TYPE_INT64, 0u, 42u}, {ZR_VALUE_TYPE_INT64, 0u, 84u}
    };
    SZrExecIrRange range;
    SZrExecIrBindingRow row;
    TZrUInt32 index;
    ZrCore_ExecIr_ModuleInit(&fixture->module);
    ZrParser_ExecIr_CallGraphInit(&fixture->graph);
    fixture->callSiteCount = nestedCall ? 2u : 1u;
    fixture->module.id = 1u;
    fixture->module.moduleHash = moduleHash;
    CHECK(ZrCore_ExecIr_ModuleAppendConstant(&fixture->module, constants, 2u,
                                            &range));
    CHECK(range.start == 0u);
    for (index = 0u; index < 3u; ++index) {
        TZrExecIrFunctionId id;
        CHECK(ZrCore_ExecIr_ModuleAddFunction(&fixture->module,
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 101u + index),
            signatureHash, &id));
        CHECK(id == index + 1u);
    }
    /* All pointers are fetched after the last function-array growth. */
    for (index = 1u; index <= 3u; ++index) {
        SZrExecIrFunction *owner = function(fixture, index);
        owner->contract.moduleHash = moduleHash;
        CHECK(ZrCore_ExecIr_FunctionAddBlock(owner,
                    ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u);
        if (index == 3u && nestedCall) call_value(owner, ZR_FALSE);
        else if (index != 1u) return_value(owner, 1u, constant_value(owner));
    }
    call_value(function(fixture, 1u), invoke);
    row = direct_row(function(fixture, 3u)->contract.targetToken);
    set_rows(fixture, &row, 1u);
    CHECK(function(fixture, 1u)->instructions[1].bindingRow == 1u);
    if (nestedCall) {
        SZrExecIrDiagnostic diagnostic;
        row = direct_row(0u);
        row.contract.bindingKind = ZR_CALL_BINDING_TYPED_FUNCTION;
        row.sourceId = 602u;
        function(fixture, 3u)->instructions[1].sourceId = 602u;
        CHECK(ZrCore_ExecIr_FunctionSetBindingRows(function(fixture, 3u),
                    &row, 1u, &diagnostic));
    }
}

static void fixture_init(Fixture *fixture, TZrBool invoke) {
    fixture_init_mode(fixture, invoke, ZR_FALSE);
}

static void fixture_free(Fixture *fixture) {
    ZrParser_ExecIr_CallGraphFree(&fixture->graph);
    ZrCore_ExecIr_FreeModule(&fixture->module);
}

static void verify_and_build(Fixture *fixture) {
    SZrExecIrDiagnostic diagnostic;
    TZrUInt32 index;
    for (index = 1u; index <= 3u; ++index) function(fixture, index)->sealed = ZR_TRUE;
    if (!ZrCore_ExecIr_VerifyModule(&fixture->module, &diagnostic)) {
        fprintf(stderr, "Core precondition: code=%u instruction=%u source=%u expected=%u actual=%u\n",
            (unsigned)diagnostic.code, diagnostic.instructionId, diagnostic.sourceId,
            diagnostic.expectedVersion, diagnostic.actualVersion);
        CHECK(ZR_FALSE);
    }
    CHECK(ZrParser_ExecIr_BuildCallGraph(&fixture->module, &fixture->graph,
                                       &diagnostic));
    CHECK(fixture->graph.edgeCount == fixture->callSiteCount);
    CHECK(ZrParser_ExecIr_CallGraphValidate(&fixture->graph, &fixture->module,
                                          &diagnostic));
}

static void expect_direct(Fixture *fixture, TZrExecIrFunctionId calleeId) {
    const SZrExecIrCallEdge *edge = &fixture->graph.edges[0];
    const SZrExecIrFunction *callee = function(fixture, calleeId);
    CHECK(edge->resolved == ZR_TRUE && edge->calleeId == calleeId);
    CHECK(edge->callerId == 1u && edge->callInstructionId == 2u);
    CHECK(edge->targetToken == (callee->contract.targetToken != 0u
        ? callee->contract.targetToken : callee->functionToken));
    CHECK(edge->expectedSignatureHash == callee->signatureHash);
    CHECK(edge->targetGeneration == callee->contract.generation);
    CHECK(edge->kind == ZR_EXEC_IR_CALL_EDGE_DIRECT && edge->exactReceiver);
    CHECK(!edge->guarded);
    CHECK(edge->patchableTarget == fixture->graph.summaries[calleeId - 1u].patchable);
    CHECK(!edge->inlineEligible &&
          edge->inlineReason == ZR_EXEC_IR_INLINE_REASON_UNSUPPORTED);
}

static void expect_unresolved(Fixture *fixture) {
    const SZrExecIrCallEdge *edge = &fixture->graph.edges[0];
    CHECK(!edge->resolved && edge->calleeId == 0u && edge->targetToken == 0u);
    CHECK(edge->expectedSignatureHash == 0u && edge->targetGeneration == 0u);
    CHECK(edge->kind == ZR_EXEC_IR_CALL_EDGE_UNKNOWN && edge->nativeEffectsUnknown);
    CHECK(!edge->exactReceiver && !edge->guarded && !edge->patchableTarget);
    CHECK(!edge->inlineEligible &&
          edge->inlineReason == ZR_EXEC_IR_INLINE_REASON_UNSUPPORTED);
    CHECK(fixture->graph.summaries[0].unknownEffects &&
          fixture->graph.summaries[0].effects == ZR_EXEC_IR_SUMMARY_EFFECT_ALL);
}

static void test_direct(TZrBool invoke) {
    Fixture fixture;
    fixture_init(&fixture, invoke);
    verify_and_build(&fixture);
    expect_direct(&fixture, 3u);
    /* Explicit CALL tokens are conservative: no purity claim is made. */
    CHECK(!fixture.graph.summaries[0].pure);
    fixture_free(&fixture);
}

static void test_effective_token(void) {
    Fixture fixture;
    SZrExecIrBindingRow row;
    TZrMetadataToken canonical =
        ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 203u);
    fixture_init(&fixture, ZR_FALSE);
    function(&fixture, 3u)->contract.targetToken = canonical;
    function(&fixture, 2u)->functionToken = canonical;
    function(&fixture, 2u)->contract.targetToken =
        ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 202u);
    row = direct_row(canonical);
    set_rows(&fixture, &row, 1u);
    verify_and_build(&fixture);
    expect_direct(&fixture, 3u);
    row.contract.targetMetadataToken = function(&fixture, 3u)->functionToken;
    set_rows(&fixture, &row, 1u);
    verify_and_build(&fixture);
    expect_unresolved(&fixture);
    fixture_free(&fixture);
}

static void test_unsupported(unsigned variant) {
    Fixture fixture;
    SZrExecIrBindingRow row;
    fixture_init(&fixture, ZR_FALSE);
    row = function(&fixture, 1u)->bindingRows[0];
    switch (variant) {
        case 0u: row.contract.bindingKind = ZR_CALL_BINDING_TYPED_FUNCTION;
                 row.contract.targetMetadataToken = 0u; break;
        case 1u: case 2u:
            row.contract.bindingKind = variant == 1u ? ZR_CALL_BINDING_VIRTUAL
                                                       : ZR_CALL_BINDING_INTERFACE;
            row.contract.ownerTypeToken =
                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 601u);
            row.contract.layoutVersion = 1u;
            row.contract.layoutHash = UINT64_C(0xaabb);
            row.contract.dispatchSlot = 0u; break;
        case 3u: case 4u: case 5u:
            row.contract.operation = variant - 2u; break;
        case 6u: case 7u: case 8u: case 9u:
            row.location.kind = variant - 5u;
            row.location.targetIndex = 3u; break;
        case 10u: row.location.ownerDepth = 1u; break;
        case 11u:
            row.contract.ownerTypeToken =
                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 602u);
            row.contract.layoutVersion = 1u;
            row.contract.layoutHash = UINT64_C(0xaabc); break;
        case 12u:
            row.contract.targetMetadataToken =
                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 103u);
            function(&fixture, 3u)->contract.targetToken = row.contract.targetMetadataToken;
            break;
        case 13u:
            row.contract.targetMetadataToken =
                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 404u); break;
        case 14u: row.contract.signatureHash += 1u; break;
        case 15u: function(&fixture, 3u)->contract.signatureHash += 1u; break;
        case 16u: function(&fixture, 3u)->signatureHash += 1u; break;
        case 17u: fixture.module.moduleHash = 0u; break;
        case 18u: fixture.module.moduleHash += 1u; break;
        case 19u: function(&fixture, 3u)->contract.moduleHash = 0u; break;
        case 20u: function(&fixture, 3u)->contract.moduleHash += 1u; break;
        case 21u: function(&fixture, 3u)->contract.generation = 0u; break;
        case 22u:
            function(&fixture, 2u)->contract.targetToken = row.contract.targetMetadataToken;
            function(&fixture, 2u)->signatureHash += 1u;
            function(&fixture, 2u)->contract.signatureHash += 1u; break;
        case 23u: set_rows(&fixture, ZR_NULL, 0u); break;
        default: CHECK(ZR_FALSE);
    }
    if (variant != 23u) set_rows(&fixture, &row, 1u);
    verify_and_build(&fixture);
    expect_unresolved(&fixture);
    fixture_free(&fixture);
}

static void test_patchable(void) {
    Fixture fixture;
    SZrExecIrDiagnostic diagnostic;
    fixture_init(&fixture, ZR_FALSE);
    verify_and_build(&fixture);
    function(&fixture, 3u)->sealed = ZR_FALSE;
    CHECK(ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic));
    CHECK(ZrParser_ExecIr_BuildCallGraph(&fixture.module, &fixture.graph, &diagnostic));
    expect_direct(&fixture, 3u);
    CHECK(fixture.graph.edges[0].patchableTarget &&
          fixture.graph.summaries[0].effects == ZR_EXEC_IR_SUMMARY_EFFECT_ALL);
    CHECK(ZrParser_ExecIr_CallGraphValidate(&fixture.graph, &fixture.module, &diagnostic));
    fixture_free(&fixture);
}

static void forge_target(Fixture *fixture) {
    SZrExecIrCallEdge *edge = &fixture->graph.edges[0];
    SZrExecIrFunction *decoy = function(fixture, 2u);
    edge->calleeId = 2u;
    edge->resolved = ZR_TRUE;
    edge->targetToken = decoy->contract.targetToken;
    edge->expectedSignatureHash = decoy->signatureHash;
    edge->targetGeneration = decoy->contract.generation;
    edge->kind = ZR_EXEC_IR_CALL_EDGE_DIRECT;
    edge->exactReceiver = ZR_TRUE;
    edge->guarded = ZR_FALSE;
    edge->patchableTarget = ZR_FALSE;
    edge->nativeEffectsUnknown = ZR_FALSE;
    edge->inlineEligible = ZR_FALSE;
    edge->inlineReason = ZR_EXEC_IR_INLINE_REASON_UNSUPPORTED;
}

static void expect_mismatch(Fixture *fixture) {
    SZrExecIrDiagnostic diagnostic;
    CHECK(!ZrParser_ExecIr_CallGraphValidate(&fixture->graph, &fixture->module,
                                           &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH);
    CHECK(diagnostic.functionToken == function(fixture, 1u)->functionToken);
    CHECK(diagnostic.instructionId == 2u && diagnostic.sourceId == 502u &&
          diagnostic.blockId == 1u);
}

/* mode 0: zero hash; 1: recomputed hash; 2: original nonzero hash. */
static void test_validator(unsigned schema, unsigned mutation) {
    Fixture fixture;
    SZrExecIrBindingRow row;
    SZrExecIrCallEdge original;
    TZrUInt64 hash;
    unsigned mode;
    fixture_init(&fixture, ZR_FALSE);
    if (schema == 1u) {
        row = function(&fixture, 1u)->bindingRows[0];
        row.contract.bindingKind = ZR_CALL_BINDING_TYPED_FUNCTION;
        row.contract.targetMetadataToken = 0u;
        set_rows(&fixture, &row, 1u);
    } else if (schema == 2u) {
        set_rows(&fixture, ZR_NULL, 0u);
    }
    verify_and_build(&fixture);
    puts("PRECONDITION: VerifyModule and BuildCallGraph accepted validator fixture");
    original = fixture.graph.edges[0];
    hash = fixture.graph.graphHash;
    CHECK(hash != 0u);
    for (mode = 0u; mode < 3u; ++mode) {
        if (selectedHashMode != UINT32_MAX && mode != selectedHashMode) continue;
        fixture.graph.edges[0] = original;
        fixture.graph.graphHash = hash;
        if (mutation == 0u) forge_target(&fixture);
        else if (mutation == 1u) fixture.graph.edges[0].targetToken += 7u;
        else if (mutation == 2u) fixture.graph.edges[0].expectedSignatureHash += 1u;
        else if (mutation == 3u) fixture.graph.edges[0].targetGeneration += 1u;
        else if (mutation == 4u) fixture.graph.edges[0].inlineReason =
                                    ZR_EXEC_IR_INLINE_REASON_EFFECTS;
        else if (mutation == 5u) fixture.graph.edges[0].nativeEffectsUnknown =
                                    (TZrBool)!original.nativeEffectsUnknown;
        else CHECK(ZR_FALSE);
        if (mode == 0u) fixture.graph.graphHash = 0u;
        else if (mode == 1u) fixture.graph.graphHash =
                                ZrParser_ExecIr_CallGraphHash(&fixture.graph);
        expect_mismatch(&fixture);
    }
    fixture.graph.edges[0] = original;
    fixture_free(&fixture);
}

static void test_preflight(unsigned variant) {
    Fixture fixture;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrCallGraph previous;
    SZrExecIrCallEdge previousEdge;
    SZrExecIrFunctionSummary previousSummaries[3];
    SZrExecIrFunction *caller;
    fixture_init(&fixture, ZR_FALSE);
    verify_and_build(&fixture);
    previous = fixture.graph;
    previousEdge = fixture.graph.edges[0];
    memcpy(previousSummaries, fixture.graph.summaries, sizeof(previousSummaries));
    caller = function(&fixture, 1u);
    if (variant == 0u) caller->bindingRows[0].contract.signatureHash = 0u;
    else if (variant == 1u) caller->bindingRows[0].contract.moduleSignatureHash = 0u;
    else if (variant == 2u) caller->contract.moduleHash = 0u;
    else if (variant == 3u) caller->bindingRows[0].contract.moduleSignatureHash += 1u;
    else if (variant == 4u) caller->bindingRows[0].location.flags = 1u;
    else if (variant == 5u) caller->instructions[1].bindingRow = 0u;
    else CHECK(ZR_FALSE);
    CHECK(!ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    CHECK(diagnostic.functionToken == caller->functionToken &&
          diagnostic.instructionId == 2u && diagnostic.sourceId == 502u);
    CHECK(!ZrParser_ExecIr_BuildCallGraph(&fixture.module, &fixture.graph,
                                        &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT &&
          diagnostic.instructionId == 2u && diagnostic.sourceId == 502u);
    CHECK(memcmp(&previous, &fixture.graph, sizeof(previous)) == 0);
    CHECK(memcmp(&previousEdge, fixture.graph.edges, sizeof(previousEdge)) == 0);
    CHECK(memcmp(previousSummaries, fixture.graph.summaries,
                  sizeof(previousSummaries)) == 0);
    fixture_free(&fixture);
}

static void test_hashes(void) {
    Fixture fixture;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrBindingRow row;
    TZrUInt64 graphHash, revision, imported;
    fixture_init(&fixture, ZR_FALSE);
    verify_and_build(&fixture);
    expect_direct(&fixture, 3u);
    graphHash = fixture.graph.graphHash;
    revision = fixture.graph.revision;
    imported = fixture.graph.summaries[0].importedHash;
    verify_and_build(&fixture);
    CHECK(fixture.graph.graphHash == graphHash && fixture.graph.revision == revision);
    function(&fixture, 3u)->instructions[0].layoutId = 1u;
    verify_and_build(&fixture);
    CHECK(fixture.graph.graphHash != graphHash && fixture.graph.revision != revision);
    CHECK(fixture.graph.summaries[0].importedHash != imported);
    graphHash = fixture.graph.graphHash;
    row = function(&fixture, 1u)->bindingRows[0];
    row.contract.signatureHash += 1u;
    set_rows(&fixture, &row, 1u);
    verify_and_build(&fixture);
    expect_unresolved(&fixture);
    CHECK(fixture.graph.graphHash != graphHash);
    row = direct_row(function(&fixture, 2u)->contract.targetToken);
    set_rows(&fixture, &row, 1u);
    verify_and_build(&fixture);
    expect_direct(&fixture, 2u);
    function(&fixture, 2u)->contract.generation += 1u;
    CHECK(ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic));
    CHECK(ZrParser_ExecIr_BuildCallGraph(&fixture.module, &fixture.graph, &diagnostic));
    expect_direct(&fixture, 2u);
    fixture_free(&fixture);
}

static void test_pass_rejection(unsigned schema) {
    Fixture fixture;
    SZrExecIrBindingRow row, beforeRow;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt64 graphHash, moduleIdentity;
    TZrUInt32 reference;
    fixture_init(&fixture, ZR_FALSE);
    if (schema == 1u) {
        row = function(&fixture, 1u)->bindingRows[0];
        row.contract.bindingKind = ZR_CALL_BINDING_TYPED_FUNCTION;
        row.contract.targetMetadataToken = 0u;
        set_rows(&fixture, &row, 1u);
    } else if (schema == 2u) set_rows(&fixture, ZR_NULL, 0u);
    verify_and_build(&fixture);
    reference = function(&fixture, 1u)->instructions[1].bindingRow;
    graphHash = fixture.graph.graphHash;
    moduleIdentity = fixture.module.moduleHash;
    memset(&beforeRow, 0, sizeof(beforeRow));
    if (schema != 2u) beforeRow = function(&fixture, 1u)->bindingRows[0];
    CHECK(!ZrParser_ExecIr_InlineCalls(&fixture.module, &fixture.graph, &diagnostic));
    CHECK(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    CHECK(!ZrParser_ExecIr_DevirtualizeCalls(&fixture.module, &fixture.graph, &diagnostic));
    CHECK(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    CHECK(function(&fixture, 1u)->instructions[1].bindingRow == reference);
    CHECK(fixture.graph.graphHash == graphHash && fixture.module.moduleHash == moduleIdentity);
    if (schema != 2u) CHECK(memcmp(&beforeRow,
        function(&fixture, 1u)->bindingRows, sizeof(beforeRow)) == 0);
    fixture_free(&fixture);
}

static void test_site_correspondence(unsigned schema, TZrBool duplicate) {
    Fixture fixture;
    SZrExecIrBindingRow row;
    SZrExecIrCallEdge original;
    TZrUInt64 hash;
    unsigned mode;
    fixture_init(&fixture, ZR_FALSE);
    if (schema == 1u) {
        row = function(&fixture, 1u)->bindingRows[0];
        row.contract.bindingKind = ZR_CALL_BINDING_TYPED_FUNCTION;
        row.contract.targetMetadataToken = 0u;
        set_rows(&fixture, &row, 1u);
    } else if (schema == 2u) set_rows(&fixture, ZR_NULL, 0u);
    verify_and_build(&fixture);
    puts("PRECONDITION: VerifyModule and BuildCallGraph accepted site fixture");
    original = fixture.graph.edges[0];
    hash = fixture.graph.graphHash;
    if (duplicate) {
        SZrExecIrCallEdge *buffer = (SZrExecIrCallEdge *)malloc(2u * sizeof(*buffer));
        CHECK(buffer != ZR_NULL);
        free(fixture.graph.edges);
        fixture.graph.edges = buffer;
        fixture.graph.edgeCapacity = 2u;
    }
    for (mode = 0u; mode < 3u; ++mode) {
        if (selectedHashMode != UINT32_MAX && mode != selectedHashMode) continue;
        fixture.graph.edges[0] = original;
        if (duplicate) fixture.graph.edges[1] = original;
        fixture.graph.edgeCount = duplicate ? 2u : 0u;
        fixture.graph.graphHash = mode == 0u ? 0u : mode == 1u
            ? ZrParser_ExecIr_CallGraphHash(&fixture.graph) : hash;
        expect_mismatch(&fixture);
    }
    fixture_free(&fixture);
}

static void test_imported_native(void) {
    Fixture fixture;
    fixture_init_mode(&fixture, ZR_FALSE, ZR_TRUE);
    verify_and_build(&fixture);
    expect_direct(&fixture, 3u);
    CHECK(fixture.graph.summaries[2].unknownEffects &&
          fixture.graph.summaries[2].unknownReason == ZR_EXEC_IR_SUMMARY_UNKNOWN_NATIVE);
    CHECK(fixture.graph.edges[0].nativeEffectsUnknown);
    fixture.graph.edges[0].nativeEffectsUnknown = ZR_FALSE;
    fixture.graph.graphHash = 0u;
    expect_mismatch(&fixture);
    fixture_free(&fixture);
}

static void test_recursive_sites(void) {
    Fixture fixture;
    SZrExecIrBindingRow row;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrCallEdge before[2];
    unsigned mode;
    fixture_init_mode(&fixture, ZR_FALSE, ZR_TRUE);
    row = direct_row(function(&fixture, 1u)->contract.targetToken);
    row.sourceId = 602u;
    CHECK(ZrCore_ExecIr_FunctionSetBindingRows(function(&fixture, 3u),
                                               &row, 1u, &diagnostic));
    verify_and_build(&fixture);
    expect_direct(&fixture, 3u);
    CHECK(fixture.graph.edges[1].resolved && fixture.graph.edges[1].calleeId == 1u);
    CHECK(fixture.graph.summaries[0].sccId == fixture.graph.summaries[2].sccId);
    memcpy(before, fixture.graph.edges, sizeof(before));
    for (mode = 0u; mode < 2u; ++mode) {
        fixture.graph.edges[1] = before[0]; /* balanced duplicate + missing site */
        fixture.graph.graphHash = mode == 0u ? 0u
            : ZrParser_ExecIr_CallGraphHash(&fixture.graph);
        expect_mismatch(&fixture);
    }
    memcpy(fixture.graph.edges, before, sizeof(before));
    fixture.graph.edgeCount = 1u;
    fixture.graph.graphHash = 0u;
    CHECK(!ZrParser_ExecIr_CallGraphValidate(&fixture.graph, &fixture.module,
                                           &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH &&
          diagnostic.functionToken == function(&fixture, 3u)->functionToken &&
          diagnostic.instructionId == 2u && diagnostic.sourceId == 602u);
    fixture_free(&fixture);
}
static void run_case(const char *name, const char *selected, unsigned kind,
                     unsigned argument, unsigned extra) {
    if (selected != ZR_NULL && strcmp(name, selected) != 0) return;
    currentCase = name;
    ++caseCount;
    if (kind == 0u) test_direct((TZrBool)argument);
    else if (kind == 1u) test_effective_token();
    else if (kind == 2u) test_unsupported(argument);
    else if (kind == 3u) test_patchable();
    else if (kind == 4u) test_validator(argument, extra);
    else if (kind == 5u) test_preflight(argument);
    else if (kind == 6u) test_hashes();
    else if (kind == 7u) test_pass_rejection(argument);
    else if (kind == 8u) test_site_correspondence(argument, (TZrBool)extra);
    else if (kind == 9u) test_imported_native();
    else if (kind == 10u) test_recursive_sites();
    else CHECK(ZR_FALSE);
}

int main(int argc, char **argv) {
    const char *selected = (argc == 3 || argc == 5) && strcmp(argv[1], "--case") == 0
                               ? argv[2] : ZR_NULL;
    unsigned index;
    char name[80];
    CHECK(argc == 1 || selected != ZR_NULL);
    if (argc == 5) {
        CHECK(strcmp(argv[3], "--hash-mode") == 0);
        selectedHashMode = (unsigned)strtoul(argv[4], ZR_NULL, 10);
        CHECK(selectedHashMode <= 2u);
    }
    run_case("direct-call", selected, 0u, 0u, 0u);
    run_case("direct-invoke", selected, 0u, 1u, 0u);
    run_case("effective-publication-token", selected, 1u, 0u, 0u);
    for (index = 0u; index < 24u; ++index) {
        (void)snprintf(name, sizeof(name), "unsupported-%u", index);
        run_case(name, selected, 2u, index, 0u);
    }
    run_case("patchable-publication", selected, 3u, 0u, 0u);
    run_case("validator-forged-target", selected, 4u, 0u, 0u);
    run_case("validator-unresolved-target", selected, 4u, 1u, 0u);
    run_case("validator-typed-empty-target", selected, 4u, 2u, 0u);
    for (index = 1u; index <= 5u; ++index) {
        (void)snprintf(name, sizeof(name), "validator-unresolved-field-%u", index);
        run_case(name, selected, 4u, 1u, index);
        (void)snprintf(name, sizeof(name), "validator-typed-empty-field-%u", index);
        run_case(name, selected, 4u, 2u, index);
    }
    for (index = 0u; index < 6u; ++index) {
        (void)snprintf(name, sizeof(name), "malformed-preflight-%u", index);
        run_case(name, selected, 5u, index, 0u);
    }
    run_case("semantic-hash-rebuild", selected, 6u, 0u, 0u);
    run_case("imported-native-effect", selected, 9u, 0u, 0u);
    run_case("typed-direct-recursion", selected, 10u, 0u, 0u);
    for (index = 0u; index < 3u; ++index) {
        (void)snprintf(name, sizeof(name), "validator-missing-site-%u", index);
        run_case(name, selected, 8u, index, 0u);
        (void)snprintf(name, sizeof(name), "validator-duplicate-site-%u", index);
        run_case(name, selected, 8u, index, 1u);
    }
    for (index = 0u; index < 3u; ++index) {
        (void)snprintf(name, sizeof(name), "rewrite-rejection-%u", index);
        run_case(name, selected, 7u, index, 0u);
    }
    CHECK(caseCount != 0u);
    printf("owned-row direct graph: %u cases, 0 failures\n", caseCount);
    return EXIT_SUCCESS;
}
