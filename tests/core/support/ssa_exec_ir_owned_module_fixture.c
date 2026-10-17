#include "ssa_exec_ir_owned_module_fixture.h"
#include <string.h>
#include "unity.h"
#include "exec_ir/exec_ir_mutation_internal.h"
SZrExecIrModule g_ownerModules[ZR_OWNER_MODULE_SLOTS];
SZrExecIrOwnedModule *g_execOwners[ZR_OWNER_SLOTS];
static unsigned g_producerFirst[ZR_OWNER_MODULE_SLOTS],g_producerLast[ZR_OWNER_MODULE_SLOTS];
void ZrTests_ExecOwner_SetUp(void) {
    ZrTests_ExecOwner_Reset();
    memset(g_execOwners, 0, sizeof(g_execOwners));
    memset(g_producerFirst,0,sizeof(g_producerFirst));memset(g_producerLast,0,sizeof(g_producerLast));
    for (unsigned i = 0u; i < ZR_OWNER_MODULE_SLOTS; ++i) ZrCore_ExecIr_ModuleInit(&g_ownerModules[i]);
}
void ZrTests_ExecOwner_TearDown(void) {
    ZrTests_ExecOwner_Disarm();
    for (unsigned i = 0u; i < ZR_OWNER_SLOTS; ++i) ZrCore_ExecIr_OwnedModuleFree(&g_execOwners[i]);
    for (unsigned i = 0u; i < ZR_OWNER_MODULE_SLOTS; ++i) ZrCore_ExecIr_FreeModule(&g_ownerModules[i]);
    ZrTests_ExecOwner_Recover(); ZrTests_ExecOwner_AssertBalanced();
}
static void contract(SZrExecutionContract *c, unsigned token, unsigned seed) {
    c->schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION; c->abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    c->logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION; c->generation = seed + 1u;
    c->targetToken = token; c->signatureHash = 1000u + seed; c->layoutHash = 2000u + seed;
    c->moduleHash = 3000u + seed;
}
static void build_function(SZrExecIrFunction *f, unsigned id, unsigned seed, unsigned mode) {
    SZrExecIrBindingRow row = {0}; SZrExecIrDiagnostic d = {0};
    ZrCore_ExecIr_FunctionInit(f); f->id = id;
    f->functionToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, seed + 1u);
    f->signatureHash = 1000u + seed; f->entryBlockId = 1u; contract(&f->contract, f->functionToken, seed);
#define POOL(field,count,capacity) do { f->field = ZrTests_ExecOwner_Calloc(1u,sizeof(*f->field)); \
    if (f->field != ZR_NULL) { f->count = 1u; f->capacity = 1u; } TEST_ASSERT_NOT_NULL(f->field); } while (0)
    POOL(values,valueCount,valueCapacity); POOL(instructions,instructionCount,instructionCapacity);
    POOL(blocks,blockCount,blockCapacity); POOL(operandPool,operandCount,operandCapacity);
    POOL(resultPool,resultCount,resultCapacity); POOL(memoryTokenPool,memoryTokenCount,memoryTokenCapacity);
    POOL(phiPool,phiCount,phiCapacity); POOL(phiIncoming,phiIncomingCount,phiIncomingCapacity);
    POOL(predecessors,predecessorCount,predecessorCapacity); POOL(successors,successorCount,successorCapacity);
    POOL(gcRoots,gcRootCount,gcRootCapacity); POOL(deoptStates,deoptStateCount,deoptStateCapacity);
    POOL(deoptValues,deoptValueCount,deoptValueCapacity); POOL(deoptAggregates,deoptAggregateCount,deoptAggregateCapacity);
    POOL(deoptAggregateFields,deoptAggregateFieldCount,deoptAggregateFieldCapacity); POOL(sourceMaps,sourceMapCount,sourceMapCapacity);
#undef POOL
    f->values[0] = (SZrExecIrValue){0}; f->values[0].id = 1u; f->values[0].definition = 1u;
    f->values[0].typeToken = seed + 7u; f->values[0].ownership = ZR_EXEC_IR_OWNERSHIP_UNIQUE;
    f->values[0].nullability = ZR_EXEC_IR_NULLABILITY_NONNULL;
    f->instructions[0].opcode = ZR_EXEC_IR_OPCODE_CALL; f->instructions[0].sourceId = seed + 17u;
    f->instructions[0].operands.count = 1u; f->instructions[0].results.count = 1u;
    f->operandPool[0] = 1u; f->resultPool[0] = 1u; f->memoryTokenPool[0] = seed + 2u;
    f->phiPool[0].result = 1u; f->phiPool[0].incomings.count = 1u;
    f->phiIncoming[0].predecessor = 1u; f->phiIncoming[0].value = 1u;
    f->predecessors[0] = 1u; f->successors[0] = 1u; f->gcRoots[0] = 1u; f->deoptValues[0] = 1u;
    f->blocks[0].id = 1u; f->blocks[0].flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    f->blocks[0].instructions.count = 1u; f->blocks[0].terminatorInstructionId = 1u;
    f->deoptStates[0].id = seed + 1u; f->deoptStates[0].sourceId = seed + 17u;
    f->deoptStates[0].resumeId = seed + 3u; f->deoptStates[0].valueRange.count = 1u; f->deoptStates[0].aggregates.count = 1u;
    f->deoptAggregates[0].identityId = 1u; f->deoptAggregates[0].typeToken = seed + 7u;
    f->deoptAggregates[0].layoutId = 1u; f->deoptAggregates[0].fields.count = 1u;
    f->deoptAggregateFields[0].kind = ZR_EXEC_IR_DEOPT_FIELD_VALUE; f->deoptAggregateFields[0].valueId = 1u;
    f->sourceMaps[0] = (SZrExecIrSourceMap){seed + 17u,1u,2u,8u,1u,3u,1u,9u};
    row.rowIndex = 0u; row.instructionId = 1u; row.segmentIndex = seed; row.sourceId = seed + 17u;
    row.contract.bindingKind = ZR_CALL_BINDING_DIRECT;
    row.contract.targetMetadataToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, seed + 21u);
    row.contract.signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, seed + 1u);
    row.contract.signatureHash = 4000u + seed; row.contract.moduleSignatureHash = f->contract.moduleHash;
    row.contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE; row.contract.operation = ZR_CALL_BINDING_OPERATION_CALL;
    row.location.kind = ZR_CALL_BINDING_RELOCATION_NONE; row.location.targetIndex = ZR_CALL_BINDING_SLOT_NONE;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionSetBindingRows(f, &row, 1u, &d));
    if(mode==2u){f->sealed=ZR_TRUE;return;}
    f->gcMap = ZrTests_ExecOwner_Calloc(1u,sizeof(*f->gcMap)); TEST_ASSERT_NOT_NULL(f->gcMap);
    f->gcMapCount = f->gcMapCapacity = 1u;
    f->stateMap = ZrTests_ExecOwner_Calloc(1u,sizeof(*f->stateMap)); TEST_ASSERT_NOT_NULL(f->stateMap);
    f->frameLayout = ZrTests_ExecOwner_Calloc(1u,sizeof(*f->frameLayout)); TEST_ASSERT_NOT_NULL(f->frameLayout);
    f->gcMap->safepointId = seed + 8u; f->gcMap->sourceId = seed + 17u;
    f->stateMap->functionToken = f->functionToken; f->stateMap->signatureHash = f->signatureHash;
    f->stateMap->generation = f->contract.generation; f->frameLayout->layoutHash = 2000u + seed;
    if (mode == 0u) {
#define NEST(obj,field,count,capacity) do { f->obj->field = ZrTests_ExecOwner_Calloc(1u,sizeof(*f->obj->field)); \
    if(f->obj->field != ZR_NULL){f->obj->count=1u;f->obj->capacity=1u;} TEST_ASSERT_NOT_NULL(f->obj->field); } while(0)
        f->gcMap->entries = ZrTests_ExecOwner_Calloc(2u,sizeof(*f->gcMap->entries));
        f->gcMap->slotIndexPool = ZrTests_ExecOwner_Calloc(2u,sizeof(*f->gcMap->slotIndexPool));
        f->gcMap->inlineRefOffsetPool = ZrTests_ExecOwner_Calloc(2u,sizeof(*f->gcMap->inlineRefOffsetPool));
        TEST_ASSERT_NOT_NULL(f->gcMap->entries); TEST_ASSERT_NOT_NULL(f->gcMap->slotIndexPool);
        TEST_ASSERT_NOT_NULL(f->gcMap->inlineRefOffsetPool);
        f->gcMap->entryCount = f->gcMap->entryCapacity = 2u;
        f->gcMap->slotIndexCount = f->gcMap->slotIndexCapacity = 2u;
        f->gcMap->inlineRefOffsetCount = f->gcMap->inlineRefOffsetCapacity = 2u;
        NEST(stateMap,entries,entryCount,entryCapacity); NEST(stateMap,valuePool,valueCount,valueCapacity);
        NEST(stateMap,rootPool,rootCount,rootCapacity); NEST(stateMap,ownerStatePool,ownerStateCount,ownerStateCapacity);
        NEST(frameLayout,slots,slotCount,slotCapacity);
#undef NEST
        f->gcMapCount = f->gcMapCapacity = 1u; f->gcMap->entries[0].site = 1u;
        f->gcMap->entries[0].liveRefSlots.count = 1u; f->gcMap->entries[0].inlineRefOffsets.count = 1u;
        f->gcMap->slotIndexPool[0] = 0u; f->gcMap->inlineRefOffsetPool[0] = seed + 4u;
        /* Two entries share the existing real instruction site; distinct inner
         * ranges carry independent roots without inventing another outer map. */
        f->gcMap->entries[1].site = 1u;
        f->gcMap->entries[1].liveRefSlots.start = 1u; f->gcMap->entries[1].liveRefSlots.count = 1u;
        f->gcMap->entries[1].inlineRefOffsets.start = 1u; f->gcMap->entries[1].inlineRefOffsets.count = 1u;
        f->gcMap->slotIndexPool[1] = 0u; f->gcMap->inlineRefOffsetPool[1] = seed + 12u;
        f->stateMap->entries[0].sourceId = seed + 17u; f->stateMap->entries[0].instructionId = 1u;
        f->stateMap->entries[0].phase = ZR_EXEC_IR_STATE_BEFORE_EFFECT;
        f->stateMap->entries[0].liveValues.count = f->stateMap->entries[0].rootValues.count = 1u;
        f->stateMap->entries[0].ownerStates.count = 1u;
        f->stateMap->valuePool[0] = f->stateMap->rootPool[0] = 1u;
        f->stateMap->ownerStatePool[0] = ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED;
        f->frameLayout->storageSlotCount = f->frameLayout->logicalSlotCount = 1u;
        f->frameLayout->frameByteSize = 8u; f->frameLayout->frameByteAlign = 8u;
        f->frameLayout->localCount = 1u; f->frameLayout->returnBufferOffset = 8u;
        f->frameLayout->slots[0] = (SZrExecIrFrameSlot){1u,0u,8u,8u,seed + 7u,ZR_EXEC_IR_FRAME_SLOT_VALUE};
    }
    f->sealed = ZR_TRUE;
}
SZrExecIrModule *ZrTests_ExecOwner_Build(unsigned slot, unsigned seed, unsigned functions, unsigned mode) {
    SZrExecIrModule *m;
    TEST_ASSERT_LESS_THAN_UINT(ZR_OWNER_MODULE_SLOTS,slot); TEST_ASSERT_TRUE(functions <= 2u);
    m = &g_ownerModules[slot]; TEST_ASSERT_NULL(m->functions);
    g_producerFirst[slot]=ZrTests_ExecOwner_Stats().records;
    m->id = seed + 1u; m->moduleToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MODULE,seed + 1u);
    m->moduleHash = 3000u + seed; contract(&m->contract,m->moduleToken,seed);
    if (functions == 0u) {g_producerLast[slot]=g_producerFirst[slot];return m;}
#define MPOOL(field,count,capacity,n) do { m->field=ZrTests_ExecOwner_Calloc(n,sizeof(*m->field)); \
    if(m->field != ZR_NULL){m->count=n;m->capacity=n;} TEST_ASSERT_NOT_NULL(m->field); }while(0)
    MPOOL(constants,constantCount,constantCapacity,1u); MPOOL(layouts,layoutCount,layoutCapacity,1u);
    MPOOL(sourceMaps,sourceMapCount,sourceMapCapacity,1u); MPOOL(functions,functionCount,functionCapacity,functions);
#undef MPOOL
    m->constants[0]=(SZrExecIrConstant){seed + 7u,seed,(TZrUInt64)seed + 9000u};
    m->layouts[0]=(SZrExecIrLayout){1u,seed + 7u,8u,8u,2000u + seed};
    m->sourceMaps[0]=(SZrExecIrSourceMap){seed + 17u,1u,2u,8u,1u,3u,1u,9u};
    for(unsigned i=0u;i<functions;++i) build_function(&m->functions[i],i+1u,seed+i,mode);
    g_producerLast[slot]=ZrTests_ExecOwner_Stats().records;
    return m;
}
void ZrTests_ExecOwner_AssertProducerLive(const SZrExecIrModule *m) {
    for(unsigned i=0u;i<ZR_OWNER_MODULE_SLOTS;++i)
        if(m==&g_ownerModules[i]){ZrTests_ExecOwner_AssertLive(g_producerFirst[i],g_producerLast[i]);return;}
    TEST_FAIL_MESSAGE("producer must be a registered constructed module");
}
static void word(SZrOwnerContents *s,TZrUInt64 value) {
    TEST_ASSERT_LESS_THAN_UINT(ZR_OWNER_WORDS,s->count); s->words[s->count++]=value;
}
static int legacy_word(void *context,TZrUInt64 value) {
    word((SZrOwnerContents *)context,value);
    return 1;
}
void ZrTests_ExecOwner_Capture(const SZrExecIrModule *o,SZrOwnerContents *s) {
    s->count=0u;
    (void)ZrTests_ExecOwner_VisitContents(o,0,legacy_word,s);
}
void ZrTests_ExecOwner_Equal(const SZrOwnerContents *expected,const SZrExecIrModule *m) {
    SZrOwnerContents actual; TEST_ASSERT_NOT_NULL(m);ZrTests_ExecOwner_Capture(m,&actual);
    TEST_ASSERT_EQUAL_UINT(expected->count,actual.count);
    for(unsigned i=0u;i<actual.count;++i)TEST_ASSERT_EQUAL_UINT64(expected->words[i],actual.words[i]);
}
void ZrTests_ExecOwner_Independent(const SZrExecIrModule *a,const SZrExecIrModule *b) {
#define PTR(objA,objB,field,count) do{TEST_ASSERT_EQUAL_UINT((objA)->count,(objB)->count); \
    if((objA)->count != 0u){TEST_ASSERT_NOT_NULL((objB)->field);TEST_ASSERT_TRUE((objA)->field != (objB)->field);}}while(0)
    PTR(a,b,constants,constantCount);PTR(a,b,layouts,layoutCount);PTR(a,b,sourceMaps,sourceMapCount);PTR(a,b,functions,functionCount);
    for(TZrUInt32 i=0u;i<a->functionCount;++i){const SZrExecIrFunction *x=&a->functions[i],*y=&b->functions[i];
        PTR(x,y,values,valueCount);PTR(x,y,instructions,instructionCount);PTR(x,y,blocks,blockCount);
        PTR(x,y,operandPool,operandCount);PTR(x,y,resultPool,resultCount);PTR(x,y,memoryTokenPool,memoryTokenCount);
        PTR(x,y,phiPool,phiCount);PTR(x,y,phiIncoming,phiIncomingCount);PTR(x,y,predecessors,predecessorCount);
        PTR(x,y,successors,successorCount);PTR(x,y,gcRoots,gcRootCount);PTR(x,y,deoptStates,deoptStateCount);
        PTR(x,y,deoptValues,deoptValueCount);PTR(x,y,deoptAggregates,deoptAggregateCount);
        PTR(x,y,deoptAggregateFields,deoptAggregateFieldCount);PTR(x,y,sourceMaps,sourceMapCount);PTR(x,y,bindingRows,bindingRowCount);
        if(x->gcMap != ZR_NULL){TEST_ASSERT_NOT_NULL(y->gcMap);TEST_ASSERT_TRUE(x->gcMap != y->gcMap);
            PTR(x->gcMap,y->gcMap,entries,entryCount);PTR(x->gcMap,y->gcMap,slotIndexPool,slotIndexCount);PTR(x->gcMap,y->gcMap,inlineRefOffsetPool,inlineRefOffsetCount);}
        if(x->stateMap != ZR_NULL){TEST_ASSERT_NOT_NULL(y->stateMap);TEST_ASSERT_TRUE(x->stateMap != y->stateMap);
            PTR(x->stateMap,y->stateMap,entries,entryCount);PTR(x->stateMap,y->stateMap,valuePool,valueCount);
            PTR(x->stateMap,y->stateMap,rootPool,rootCount);PTR(x->stateMap,y->stateMap,ownerStatePool,ownerStateCount);}
        if(x->frameLayout != ZR_NULL){TEST_ASSERT_NOT_NULL(y->frameLayout);TEST_ASSERT_TRUE(x->frameLayout != y->frameLayout);PTR(x->frameLayout,y->frameLayout,slots,slotCount);}}
#undef PTR
}
static TZrUInt64 bytes(TZrUInt64 h,const void *p,size_t n) {
    const unsigned char *b=p;for(size_t i=0u;i<n;++i){h^=b[i];h*=UINT64_C(1099511628211);}return h;
}
TZrUInt64 ZrTests_ExecOwner_LiveDigest(const SZrExecIrModule *m) {
    TZrUInt64 h=bytes(UINT64_C(1469598103934665603),m,sizeof(*m));
#define B(obj,field,count) h=bytes(h,(obj)->field,(size_t)(obj)->count*sizeof(*(obj)->field))
    B(m,constants,constantCount);B(m,layouts,layoutCount);B(m,sourceMaps,sourceMapCount);B(m,functions,functionCount);
    for(TZrUInt32 i=0u;i<m->functionCount;++i){const SZrExecIrFunction *f=&m->functions[i];
        B(f,values,valueCount);B(f,instructions,instructionCount);B(f,blocks,blockCount);B(f,operandPool,operandCount);
        B(f,resultPool,resultCount);B(f,memoryTokenPool,memoryTokenCount);B(f,phiPool,phiCount);B(f,phiIncoming,phiIncomingCount);
        B(f,predecessors,predecessorCount);B(f,successors,successorCount);B(f,gcRoots,gcRootCount);B(f,deoptStates,deoptStateCount);
        B(f,deoptValues,deoptValueCount);B(f,deoptAggregates,deoptAggregateCount);B(f,deoptAggregateFields,deoptAggregateFieldCount);
        B(f,sourceMaps,sourceMapCount);B(f,bindingRows,bindingRowCount);
        if(f->gcMap != ZR_NULL){h=bytes(h,f->gcMap,sizeof(*f->gcMap));B(f->gcMap,entries,entryCount);B(f->gcMap,slotIndexPool,slotIndexCount);B(f->gcMap,inlineRefOffsetPool,inlineRefOffsetCount);}
        if(f->stateMap != ZR_NULL){h=bytes(h,f->stateMap,sizeof(*f->stateMap));B(f->stateMap,entries,entryCount);B(f->stateMap,valuePool,valueCount);B(f->stateMap,rootPool,rootCount);B(f->stateMap,ownerStatePool,ownerStateCount);}
        if(f->frameLayout != ZR_NULL){h=bytes(h,f->frameLayout,sizeof(*f->frameLayout));B(f->frameLayout,slots,slotCount);}}
#undef B
    return h;
}
void ZrTests_ExecOwner_Diagnostic(const SZrExecIrDiagnostic *e,const SZrExecIrDiagnostic *a) {
#define D(f) TEST_ASSERT_EQUAL_UINT64(e->f,a->f)
    D(code);D(functionToken);D(blockId);D(instructionId);D(sourceId);D(expectedVersion);D(actualVersion);D(expectedHash);D(actualHash);
#undef D
}
