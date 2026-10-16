#include "ssa_source_finalized_module_fixture.h"
#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "exec_ir/source/exec_ir_source_callable_query_internal.h"
SFmFixture g_fm;
SZrExecIrModule g_fmModuleOwner;
SZrExecIrFunction g_fmFunctionOwner;
SZrExecIrFunction g_fmOriginalFunctionOwner;
SZrExecIrFrameLayout g_fmFrameOwner;
static unsigned g_inputs[FM_LIVE], g_inputCount, g_outputIds[FM_LIVE], g_outputCount;
static unsigned g_deferredErrors;
unsigned Fm_DeferredErrors(void) { return g_deferredErrors; }
static TZrBool records_have(const unsigned *ids, unsigned count, unsigned frees) {
    unsigned records = ZrTests_ExecOwner_Stats().records;
    if (count > FM_LIVE || records > 32768u)
        return ZR_FALSE;
    for (unsigned i = 0; i < count; ++i) {
        unsigned actual;
        if (ids[i] >= records || !ZrTests_ExecOwner_TryRecordFrees(ids[i], &actual) || actual != frees)
            return ZR_FALSE;
    }
    return ZR_TRUE;
}
void Fm_RegisterOutput(unsigned first, unsigned last) {
    unsigned records = ZrTests_ExecOwner_Stats().records;
    /* Record the previous owner's exact release before replacing its IDs. */
    if (!Fm_OutputReleased())
        ++g_deferredErrors;
    if (first > last || last > records || records > 32768u) {
        ++g_deferredErrors;
        return;
    }
    g_outputCount = 0;
    for (unsigned i = first; i < last; ++i) {
        unsigned frees;
        if (!ZrTests_ExecOwner_TryRecordFrees(i, &frees)) {
            ++g_deferredErrors;
            return;
        }
        if (!frees) {
            if (g_outputCount == FM_LIVE) {
                ++g_deferredErrors;
                return;
            }
            g_outputIds[g_outputCount++] = i;
        }
    }
}
TZrBool Fm_InputLive(void) {
    unsigned records = ZrTests_ExecOwner_Stats().records;
    if (g_deferredErrors || g_inputCount > FM_LIVE || records > 32768u)
        return ZR_FALSE;
    for (unsigned i = 0; i < g_inputCount; ++i) {
        unsigned frees;
        if (g_inputs[i] >= records || !ZrTests_ExecOwner_TryRecordFrees(g_inputs[i], &frees) || frees)
            return ZR_FALSE;
    }
    return ZR_TRUE;
}
TZrBool Fm_OutputLive(void) {
    unsigned records = ZrTests_ExecOwner_Stats().records;
    if (g_deferredErrors || g_outputCount > FM_LIVE || records > 32768u)
        return ZR_FALSE;
    for (unsigned i = 0; i < g_outputCount; ++i) {
        unsigned frees;
        if (g_outputIds[i] >= records || !ZrTests_ExecOwner_TryRecordFrees(g_outputIds[i], &frees) || frees)
            return ZR_FALSE;
    }
    return ZR_TRUE;
}
TZrBool Fm_OutputReleased(void) {
    unsigned records = ZrTests_ExecOwner_Stats().records;
    if (g_deferredErrors || g_outputCount > FM_LIVE || records > 32768u)
        return ZR_FALSE;
    for (unsigned i = 0; i < g_outputCount; ++i) {
        unsigned frees;
        if (g_outputIds[i] >= records || !ZrTests_ExecOwner_TryRecordFrees(g_outputIds[i], &frees) || frees != 1)
            return ZR_FALSE;
    }
    return ZR_TRUE;
}
SZrArray Fm_Array(void *head, size_t width, size_t length, size_t capacity) {
    SZrArray a = {0};
    a.head = head;
    a.elementSize = width;
    a.length = length;
    a.capacity = capacity;
    a.isValid = ZR_TRUE;
    return a;
}
static SZrFileRange range(size_t start, size_t end) {
    SZrFileRange r = {0};
    r.start.offset = start;
    r.end.offset = end;
    r.start.line = r.end.line = 1;
    r.start.column = (TZrInt32) start + 1;
    r.end.column = (TZrInt32) end + 1;
    return r;
}
static void contract(SZrExecutionContract *c, TZrMetadataToken token, TZrUInt64 hash) {
    memset(c, 0, sizeof(*c));
    c->schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    c->abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    c->logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    c->generation = 1;
    c->targetToken = token;
    c->signatureHash = hash;
    c->moduleHash = 1103;
}
void Fm_Spare(SZrExecIrFunction *f) {
#define SPARE(field, count, capacity)                                                                                  \
    do {                                                                                                               \
        if (f->capacity > f->count)                                                                                    \
            memset(f->field + f->count, 0, (size_t) (f->capacity - f->count) * sizeof(*f->field));                     \
    } while (0)
    SPARE(values, valueCount, valueCapacity);
    SPARE(instructions, instructionCount, instructionCapacity);
    SPARE(blocks, blockCount, blockCapacity);
    SPARE(operandPool, operandCount, operandCapacity);
    SPARE(resultPool, resultCount, resultCapacity);
    SPARE(sourceMaps, sourceMapCount, sourceMapCapacity);
#undef SPARE
    if (f->frameLayout && f->frameLayout->slotCapacity > f->frameLayout->slotCount)
        memset(f->frameLayout->slots + f->frameLayout->slotCount, 0,
               (size_t) (f->frameLayout->slotCapacity - f->frameLayout->slotCount) * sizeof(*f->frameLayout->slots));
}
static void body(SZrExecIrFunction *f, TZrBool original) {
    SZrExecIrInstruction ins = {0};
    SZrExecIrRange slice;
    TZrExecIrInstructionId id;
    TZrExecIrValueId constant, address = 0, provenance = 0;
    SZrExecIrDiagnostic d, zero = {0};
    SZrExecIrSourceMap maps[3] = {{0}};
    f->id = 1;
    f->functionToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 49);
    f->signatureHash = 907;
    contract(&f->contract, f->functionToken, f->signatureHash);
    f->entryBlockId = ZrCore_ExecIr_FunctionAddBlock(f, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    TEST_ASSERT_EQUAL_UINT32(1, f->entryBlockId);
    constant =
            ZrCore_ExecIr_FunctionAddValue(f, FM_RETURN, ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_NONNULL);
    TEST_ASSERT_EQUAL_UINT32(1, constant);
    if (original) {
        address = ZrCore_ExecIr_FunctionAddValue(f, FM_RETURN, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                                                 ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        provenance = ZrCore_ExecIr_FunctionAddValue(f, FM_RETURN, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                                                    ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        TEST_ASSERT_EQUAL_UINT32(2, address);
        TEST_ASSERT_EQUAL_UINT32(3, provenance);
        f->values[address - 1].flags = ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS;
        f->values[provenance - 1].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    }
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendResults(f, &constant, 1, &slice));
    ins.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    ins.typeToken = FM_RETURN;
    ins.results = slice;
    ins.sourceId = 1;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(f, &ins, &id));
    TEST_ASSERT_EQUAL_UINT32(1, id);
    TEST_ASSERT_EQUAL_UINT32(id, f->values[0].definition);
    memset(&ins, 0, sizeof(ins));
    ins.typeToken = FM_RETURN;
    ins.sourceId = 2;
    ins.opcode = original ? ZR_EXEC_IR_OPCODE_PLACE_BASE : ZR_EXEC_IR_OPCODE_NOP;
    if (original) {
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(f, &provenance, 1, &ins.operands));
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendResults(f, &address, 1, &ins.results));
    }
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(f, &ins, &id));
    TEST_ASSERT_EQUAL_UINT32(2, id);
    memset(&ins, 0, sizeof(ins));
    ins.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    ins.typeToken = FM_RETURN;
    ins.sourceId = 3;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(f, &constant, 1, &ins.operands));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(f, &ins, &id));
    TEST_ASSERT_EQUAL_UINT32(3, id);
    f->blocks[0].instructions.start = 0;
    f->blocks[0].instructions.count = 3;
    f->blocks[0].terminatorInstructionId = id;
    for (unsigned i = 0; i < 3; ++i) {
        SZrFileRange r = g_fm.instructions[i].sourceRange;
        maps[i].instructionId = i + 1;
        maps[i].sourceId = i + 1;
        maps[i].startOffset = (TZrUInt32) r.start.offset;
        maps[i].endOffset = (TZrUInt32) r.end.offset;
        maps[i].startLine = (TZrUInt32) r.start.line;
        maps[i].endLine = (TZrUInt32) r.end.line;
        maps[i].startColumn = (TZrUInt32) r.start.column;
        maps[i].endColumn = (TZrUInt32) r.end.column;
    }
    /* Borrowed header supplies declared typed rows. Only the actual clone owns their storage. */
    SZrExecIrFunction borrowed = *f;
    borrowed.sourceMaps = maps;
    borrowed.sourceMapCount = borrowed.sourceMapCapacity = 3;
    TZrBool ok = ZrCore_ExecIr_CloneFunction(&borrowed, &g_fm.temporary, &d);
    if (ok) {
        SZrExecIrFunction prior = *f;
        *f = g_fm.temporary;
        g_fm.temporary = prior;
        ZrCore_ExecIr_FreeFunction(&g_fm.temporary);
    }
    TEST_ASSERT_TRUE_MESSAGE(ok, "actual Core source-map clone prerequisite");
    Fm_Diagnostic(&zero, &d);
    Fm_Spare(f);
}
static void declared(void) {
    SFmFixture *f = &g_fm;
    SZrFileRange integer = range(7, 8), returned = range(0, 8), script = range(0, 9);
    f->nodes[0].id = FM_RETURN;
    f->nodes[0].kind = ZR_CANONICAL_TYPE_PRIMITIVE;
    f->nodes[0].structuralHash = 701;
    f->nodes[0].data.primitive.valueType = ZR_VALUE_TYPE_INT64;
    f->nodes[1].id = FM_CALLABLE;
    f->nodes[1].kind = ZR_CANONICAL_TYPE_FUNCTION;
    f->nodes[1].structuralHash = 907;
    f->nodes[1].data.function.returnTypeId = FM_RETURN;
    f->nodes[1].data.function.parameterContracts = Fm_Array(ZR_NULL, sizeof(SZrCanonicalParameterContract), 0, 0);
    f->context.state = &f->state;
    f->context.canonicalTypes = Fm_Array(f->nodes, sizeof(*f->nodes), 2, 3);
    f->context.symbols = Fm_Array(f->symbols, sizeof(*f->symbols), 1, 3);
    f->script.type = ZR_AST_SCRIPT;
    f->script.location = script;
    f->script.data.script.statements = &f->statements;
    f->returned.type = ZR_AST_RETURN_STATEMENT;
    f->returned.location = returned;
    f->returned.data.returnStatement.expr = &f->integer;
    f->integer.type = ZR_AST_INTEGER_LITERAL;
    f->integer.location = integer;
    f->statementNodes[0] = &f->returned;
    f->statements.nodes = f->statementNodes;
    f->statements.count = 1;
    f->statements.capacity = 2;
    f->symbols[0].id = 42;
    f->symbols[0].kind = ZR_SEMANTIC_SYMBOL_KIND_FUNCTION;
    f->symbols[0].typeId = FM_CALLABLE;
    f->symbols[0].astNode = &f->script;
    f->symbols[0].location = script;
    f->identity.schemaVersion = ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1;
    f->identity.symbolId = 42;
    f->identity.typeId = FM_CALLABLE;
    f->identity.canonicalSignatureHash = 907;
    f->identity.returnPrimitive = ZR_VALUE_TYPE_INT64;
    f->identity.hasExplicitNoArgsI64 = ZR_TRUE;
    f->identity.declarationRange.startLine = 1;
    f->identity.declarationRange.startColumn = 1;
    f->identity.declarationRange.endLine = 1;
    f->identity.declarationRange.endColumn = 10;
    f->semantic.state = &f->state;
    f->semantic.symbolId = 42;
    f->semantic.callableTypeId = FM_CALLABLE;
    f->semantic.places.state = f->semantic.cfg.state = &f->state;
    f->semantic.places.places = Fm_Array(f->places, sizeof(*f->places), 1, 2);
    f->semantic.cfg.blocks = Fm_Array(f->blocks, sizeof(*f->blocks), 1, 2);
#define ARRAY(field, storage, count)                                                                                   \
    f->semantic.field = Fm_Array(f->storage, sizeof(*f->storage), count, ZR_ARRAY_COUNT(f->storage))
    ARRAY(values, values, 1);
    ARRAY(instructions, instructions, 3);
    ARRAY(sourceMap, maps, 3);
    ARRAY(valueOperands, operands, 1);
    ARRAY(regions, regions, 1);
#undef ARRAY
#define EMPTY(field, type) f->semantic.field = Fm_Array(ZR_NULL, sizeof(type), 0, 0)
    EMPTY(locals, SZrSemanticIrLocal);
    EMPTY(cleanupScopes, SZrSemanticIrCleanupScope);
    EMPTY(loanFacts, SZrSemanticIrLoanFact);
    EMPTY(escapeFacts, SZrSemanticEscapeFact);
    EMPTY(contiguousViewFacts, SZrSemanticContiguousViewFact);
    EMPTY(boundsFacts, SZrSemanticBoundsFact);
    EMPTY(scalarScratchProofs, SZrSemanticIrScalarScratchProof);
#undef EMPTY
    f->blocks[0].kind = ZR_PARSER_CFG_BLOCK_ENTRY;
    f->blocks[0].instructionCount = 3;
    f->blocks[0].terminatorKind = ZR_PARSER_CFG_TERMINATOR_RETURN;
    f->blocks[0].outgoingEdges = Fm_Array(ZR_NULL, sizeof(SZrParserCfgEdge), 0, 0);
    f->places[0].id = 1;
    f->places[0].typeId = FM_RETURN;
    f->places[0].base.kind = ZR_PARSER_PLACE_BASE_TEMPORARY;
    f->places[0].projections = Fm_Array(ZR_NULL, sizeof(SZrParserPlaceProjection), 0, 0);
    f->places[0].sourceRange = integer;
    f->regions[0].id = 1;
    f->regions[0].escapeBound = ZR_SEMANTIC_ESCAPE_FUNCTION;
    f->values[0].id = 1;
    f->values[0].typeId = FM_RETURN;
    f->values[0].definitionInstructionId = 1;
    f->values[0].sourceRange = integer;
    f->values[0].facts.typeId = FM_RETURN;
    f->values[0].facts.ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_VALUE;
    f->values[0].facts.nullability = ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL;
    f->operands[0] = 1;
    for (unsigned i = 0; i < 3; ++i) {
        SZrSemanticIrInstruction *s = &f->instructions[i];
        s->id = i + 1;
        s->typeId = FM_RETURN;
        s->targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        s->sourceRange = i == 2 ? returned : integer;
        s->opcode = i == 0 ? ZR_SEMANTIC_IR_CONSTANT : i == 1 ? ZR_SEMANTIC_IR_PLACE_BASE : ZR_SEMANTIC_IR_RETURN;
        f->maps[i].instructionId = s->id;
        f->maps[i].sourceRange = s->sourceRange;
    }
    f->instructions[0].hasConstantPoolIndex = ZR_TRUE;
    f->instructions[0].resultValueId = 1;
    f->instructions[1].placeId = 1;
    f->instructions[2].operandCount = 1;
}
void Fm_Bits(TZrInt64 value) {
    g_fm.compilerPool[0].type = g_fm.functionPool[0].type = ZR_VALUE_TYPE_INT64;
    g_fm.compilerPool[0].value.nativeObject.nativeInt64 = value;
    g_fm.functionPool[0].value.nativeObject.nativeInt64 = value;
    g_fm.original.constants[0].bits = (TZrUInt64) value;
}
void Fm_Refresh(void) {
    g_fmModuleOwner = g_fm.original;
    g_fmFunctionOwner = g_fm.finalized;
    memset(&g_fmOriginalFunctionOwner, 0, sizeof(g_fmOriginalFunctionOwner));
    if (g_fm.original.functionCount)
        g_fmOriginalFunctionOwner = g_fm.original.functions[0];
    memset(&g_fmFrameOwner, 0, sizeof(g_fmFrameOwner));
    if (g_fm.finalized.frameLayout)
        g_fmFrameOwner = *g_fm.finalized.frameLayout;
    g_inputCount = 0;
    for (unsigned i = 0; i < ZrTests_ExecOwner_Stats().records; ++i) {
        TZrBool output = ZR_FALSE;
        for (unsigned j = 0; j < g_outputCount; ++j)
            if (i == g_outputIds[j])
                output = ZR_TRUE;
        unsigned frees;
        if (!ZrTests_ExecOwner_TryRecordFrees(i, &frees)) {
            ++g_deferredErrors;
            return;
        }
        if (!output && !frees) {
            if (g_inputCount == FM_LIVE) {
                ++g_deferredErrors;
                return;
            }
            g_inputs[g_inputCount++] = i;
        }
    }
}
void Fm_SetUp(void) {
    SZrExecIrDiagnostic d, zero = {0};
    SZrExecIrRange r;
    TZrExecIrFunctionId id;
    SZrExecIrConstant constant = {FM_RETURN, 0, 9};
    SZrExecIrLayout layout = {21, FM_RETURN, 8, 8, 901};
    ZrTests_ExecOwner_AssertBalanced();
    TEST_ASSERT_EQUAL_UINT(0, g_deferredErrors);
    ZrTests_ExecOwner_Reset();
    memset(&g_fm, 0, sizeof(g_fm));
    g_inputCount = g_outputCount = 0;
    ZrCore_ExecIr_ModuleInit(&g_fm.original);
    ZrCore_ExecIr_ModuleInit(&g_fm.candidate);
    ZrCore_ExecIr_ModuleInit(&g_fm.scratch);
    ZrCore_ExecIr_FunctionInit(&g_fm.finalized);
    ZrCore_ExecIr_FunctionInit(&g_fm.temporary);
    Fm_Refresh();
    declared();
    g_fm.compilerPool = ZrTests_ExecOwner_Calloc(2, sizeof(*g_fm.compilerPool));
    g_fm.functionPool = ZrTests_ExecOwner_Calloc(1, sizeof(*g_fm.functionPool));
    TEST_ASSERT_NOT_NULL(g_fm.compilerPool);
    TEST_ASSERT_NOT_NULL(g_fm.functionPool);
    g_fm.constants = Fm_Array(g_fm.compilerPool, sizeof(*g_fm.compilerPool), 1, 2);
    g_fm.original.moduleToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MODULE, 17);
    g_fm.original.moduleHash = 1103;
    contract(&g_fm.original.contract, g_fm.original.moduleToken, 1201);
    TZrBool ok = ZrCore_ExecIr_ModuleAddFunction(&g_fm.original,
                                                 ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 49), 907, &id);
    Fm_Refresh();
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(1, id);
    body(&g_fm.original.functions[0], ZR_TRUE);
    Fm_Refresh();
    body(&g_fm.finalized, ZR_FALSE);
    Fm_Refresh();
    ok = ZrCore_ExecIr_ModuleAppendConstant(&g_fm.original, &constant, 1, &r);
    Fm_Refresh();
    TEST_ASSERT_TRUE(ok);
    ok = ZrCore_ExecIr_ModuleAppendLayout(&g_fm.original, &layout, 1, &r);
    Fm_Refresh();
    TEST_ASSERT_TRUE(ok);
    Fm_Bits(9);
    /* Actual reserve buffers differ from the clone's initial-capacity policy. */
    SZrExecIrValue *values = ZrTests_ExecOwner_Realloc(g_fm.finalized.values, 2 * sizeof(*values));
    if (values) {
        g_fm.finalized.values = values;
        g_fm.finalized.valueCapacity = 2;
    }
    Fm_Refresh();
    TEST_ASSERT_NOT_NULL(values);
    Fm_Spare(&g_fm.finalized);
    memset(g_fm.original.constants + 1, 0, (g_fm.original.constantCapacity - 1) * sizeof(constant));
    memset(g_fm.original.layouts + 1, 0, (g_fm.original.layoutCapacity - 1) * sizeof(layout));
    memset(g_fm.original.functions + 1, 0, (g_fm.original.functionCapacity - 1) * sizeof(*g_fm.original.functions));
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_VerifyModule(&g_fm.original, &d),
                             "actual original Core Verify prerequisite");
    Fm_Diagnostic(&zero, &d);
    ok = ZrParser_ExecIr_AttachPrimitiveSourceFrame(&g_fm.finalized, &g_fm.context, g_fm.original.layouts, 1, 0, &d);
    Fm_Refresh();
    TEST_ASSERT_TRUE_MESSAGE(ok, "actual finalized Attach prerequisite");
    Fm_Diagnostic(&zero, &d);
    Fm_Spare(&g_fm.finalized);
    Fm_Refresh();
    SZrExecIrFrameLayout *frame = g_fm.finalized.frameLayout;
    SZrExecIrFrameSlot *slots = ZrTests_ExecOwner_Realloc(frame->slots, 2 * sizeof(*slots));
    if (slots) {
        frame->slots = slots;
        frame->slotCapacity = 2;
        memset(slots + frame->slotCount, 0, (2 - frame->slotCount) * sizeof(*slots));
    }
    Fm_Refresh();
    TEST_ASSERT_NOT_NULL(slots);
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_VerifyFunction(&g_fm.finalized, ZR_EXEC_IR_VERIFY_ALL, &d),
                             "actual finalized Core Verify prerequisite");
    Fm_Diagnostic(&zero, &d);
    SSourceCallableFacts facts;
    TEST_ASSERT_TRUE_MESSAGE(
            source_callable_query(&g_fm.context, FM_CALLABLE, g_fm.finalized.functionToken, &facts, &d),
            "actual callable query prerequisite");
    Fm_Diagnostic(&zero, &d);
    TEST_ASSERT_EQUAL_UINT32(FM_RETURN, facts.returnTypeId);
    TEST_ASSERT_EQUAL_UINT64(907, facts.signatureHash);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_ValidatePrimitiveSourceFrame(&g_fm.finalized, &g_fm.context,
                                                                          g_fm.original.layouts, 1, 0, &d),
                             "actual const frame prerequisite");
    Fm_Diagnostic(&zero, &d);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrCore_ExecIr_CloneModuleReplacingFunction(&g_fm.original, 1, &g_fm.finalized, &g_fm.candidate, &d),
            "actual whole module copy prerequisite");
    Fm_Diagnostic(&zero, &d);
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_VerifyModule(&g_fm.candidate, &d),
                             "actual candidate Core Verify prerequisite");
    Fm_Diagnostic(&zero, &d);
    unsigned outputFirst = ZrTests_ExecOwner_Stats().records;
    TZrBool owned = ZrCore_ExecIr_CloneOwnedModule(&g_fm.candidate, &g_fm.output, &d);
    Fm_RegisterOutput(outputFirst, ZrTests_ExecOwner_Stats().records);
    TEST_ASSERT_TRUE_MESSAGE(owned, "actual old owned output prerequisite");
    Fm_Diagnostic(&zero, &d);
    ZrCore_ExecIr_FreeModule(&g_fm.candidate);
    Fm_Refresh();
    TEST_ASSERT_EQUAL_UINT(0, g_deferredErrors);
}
void Fm_ClearDeclared(void) {
    /* This clears declared inline facts only, after real native inputs are freed.
     * It is not SemanticContext/State lifecycle or interner provenance evidence. */
    memset(&g_fm, 0, offsetof(SFmFixture, original));
}
void Fm_ReleaseInputs(void) {
    ZrCore_ExecIr_FreeModule(&g_fm.original);
    ZrCore_ExecIr_FreeFunction(&g_fm.finalized);
    ZrTests_ExecOwner_Free(g_fm.compilerPool);
    g_fm.compilerPool = ZR_NULL;
    ZrTests_ExecOwner_Free(g_fm.functionPool);
    g_fm.functionPool = ZR_NULL;
    Fm_Refresh();
}
void Fm_TearDown(void) {
    SZrOwnerHeapStats health;
    ZrTests_ExecOwner_Disarm();
    if (records_have(g_outputIds, g_outputCount, 0))
        ZrCore_ExecIr_OwnedModuleFree(&g_fm.output);
    else {
        ++g_deferredErrors;
        g_fm.output = ZR_NULL;
    }
    ZrCore_ExecIr_OwnedModuleFree(&g_fm.directOwner);
    ZrCore_ExecIr_FreeModule(&g_fm.candidate);
    ZrCore_ExecIr_FreeModule(&g_fm.scratch);
    ZrCore_ExecIr_FreeFunction(&g_fm.temporary);
    if (records_have(g_inputs, g_inputCount, 0))
        Fm_ReleaseInputs();
    else {
        ++g_deferredErrors;
        /* Preserve numeric failure evidence, avoid traversing released buffers,
         * and leave surviving allocations to the existing safety recovery. */
        ZrCore_ExecIr_ModuleInit(&g_fm.original);
        ZrCore_ExecIr_FunctionInit(&g_fm.finalized);
        g_fm.compilerPool = g_fm.functionPool = ZR_NULL;
    }
    health = ZrTests_ExecOwner_Stats();
    unsigned deferred = g_deferredErrors;
    if (health.errors || health.live || deferred)
        fprintf(stderr, "SOURCE_FINALIZED_LEDGER errors=%u live=%u support=%u\n", health.errors, health.live, deferred);
    ZrTests_ExecOwner_Recover();
    g_inputCount = g_outputCount = 0;
    TEST_ASSERT_EQUAL_UINT(0, health.errors);
    TEST_ASSERT_EQUAL_UINT(0, health.live);
    TEST_ASSERT_EQUAL_UINT(0, deferred);
    TEST_ASSERT_EQUAL_UINT(health.records, health.frees);
    ZrTests_ExecOwner_AssertBalanced();
}
