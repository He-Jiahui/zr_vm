#include "ssa_exec_ir_owned_module_fixture.h"
#include <string.h>
#include "unity.h"
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
    f->stateMap = ZrTests_ExecOwner_Calloc(1u,sizeof(*f->stateMap)); TEST_ASSERT_NOT_NULL(f->stateMap);
    f->frameLayout = ZrTests_ExecOwner_Calloc(1u,sizeof(*f->frameLayout)); TEST_ASSERT_NOT_NULL(f->frameLayout);
    f->gcMap->safepointId = seed + 8u; f->gcMap->sourceId = seed + 17u;
    f->stateMap->functionToken = f->functionToken; f->stateMap->signatureHash = f->signatureHash;
    f->stateMap->generation = f->contract.generation; f->frameLayout->layoutHash = 2000u + seed;
    if (mode == 0u) {
#define NEST(obj,field,count,capacity) do { f->obj->field = ZrTests_ExecOwner_Calloc(1u,sizeof(*f->obj->field)); \
    if(f->obj->field != ZR_NULL){f->obj->count=1u;f->obj->capacity=1u;} TEST_ASSERT_NOT_NULL(f->obj->field); } while(0)
        NEST(gcMap,entries,entryCount,entryCapacity); NEST(gcMap,slotIndexPool,slotIndexCount,slotIndexCapacity);
        NEST(gcMap,inlineRefOffsetPool,inlineRefOffsetCount,inlineRefOffsetCapacity);
        NEST(stateMap,entries,entryCount,entryCapacity); NEST(stateMap,valuePool,valueCount,valueCapacity);
        NEST(stateMap,rootPool,rootCount,rootCapacity); NEST(stateMap,ownerStatePool,ownerStateCount,ownerStateCapacity);
        NEST(frameLayout,slots,slotCount,slotCapacity);
#undef NEST
        f->gcMapCount = f->gcMapCapacity = 1u; f->gcMap->entries[0].site = 1u;
        f->gcMap->entries[0].liveRefSlots.count = 1u; f->gcMap->entries[0].inlineRefOffsets.count = 1u;
        f->gcMap->slotIndexPool[0] = 0u; f->gcMap->inlineRefOffsetPool[0] = seed + 4u;
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
#define W(f) word(s,(TZrUInt64)(o->f))
#define R(f) W(f.offset);W(f.count)
static void capture_contract(const SZrExecutionContract *o,SZrOwnerContents *s) {
    W(schemaVersion);W(abiVersion);W(logicalVersion);W(reserved0);W(generation);W(targetToken);W(reserved1);
    W(signatureHash);W(layoutHash);W(moduleHash);W(requiredCapabilities);W(declaredEffects);
}
static void capture_function(const SZrExecIrFunction *f,SZrOwnerContents *s) {
    { const SZrExecIrFunction *o=f;
    W(id);W(functionToken);W(signatureHash);W(entryBlockId);W(bindingRowsSchemaVersion);W(sealed);W(gcMapCount);
    capture_contract(&f->contract,s);
#define COUNT(field) W(field)
    COUNT(valueCount);COUNT(instructionCount);COUNT(blockCount);COUNT(operandCount);COUNT(resultCount);
    COUNT(memoryTokenCount);COUNT(phiCount);COUNT(phiIncomingCount);COUNT(predecessorCount);COUNT(successorCount);
    COUNT(gcRootCount);COUNT(deoptStateCount);COUNT(deoptValueCount);COUNT(deoptAggregateCount);COUNT(deoptAggregateFieldCount);
    COUNT(sourceMapCount);COUNT(bindingRowCount);
#undef COUNT
    }
    for(TZrUInt32 i=0u;i<f->valueCount;++i){const SZrExecIrValue *o=&f->values[i];W(id);W(definition);W(typeToken);W(ownership);W(nullability);W(flags);}
    for(TZrUInt32 i=0u;i<f->instructionCount;++i){const SZrExecIrInstruction *o=&f->instructions[i];
        W(opcode);W(flags);R(results);R(operands);R(phiRange);R(successorRange);W(typeToken);W(matchTypeToken);W(layoutId);
        R(memoryIn);R(memoryOut);W(effectIn);W(effectOut);W(sourceId);W(deoptId);W(bindingRow);}
    for(TZrUInt32 i=0u;i<f->blockCount;++i){const SZrExecIrBlock *o=&f->blocks[i];
        W(id);W(flags);R(instructions);R(predecessors);R(successors);R(phis);W(effectPhiResult);R(effectPhiIncomings);
        for(unsigned j=0u;j<ZR_EXEC_IR_MEMORY_CLASS_COUNT;++j){word(s,o->memoryPhiResults[j]);word(s,o->memoryPhiIncomings[j].offset);word(s,o->memoryPhiIncomings[j].count);}
        W(immediateDominator);W(terminatorInstructionId);}
#define NUM(field,count) for(TZrUInt32 i=0u;i<f->count;++i)word(s,f->field[i])
    NUM(operandPool,operandCount);NUM(resultPool,resultCount);NUM(memoryTokenPool,memoryTokenCount);
    NUM(predecessors,predecessorCount);NUM(successors,successorCount);NUM(gcRoots,gcRootCount);NUM(deoptValues,deoptValueCount);
#undef NUM
    for(TZrUInt32 i=0u;i<f->phiCount;++i){const SZrExecIrPhi *o=&f->phiPool[i];W(result);R(incomings);}
    for(TZrUInt32 i=0u;i<f->phiIncomingCount;++i){const SZrExecIrPhiIncoming *o=&f->phiIncoming[i];W(predecessor);W(value);}
    for(TZrUInt32 i=0u;i<f->deoptStateCount;++i){const SZrExecIrDeoptState *o=&f->deoptStates[i];W(id);W(sourceId);W(resumeId);R(valueRange);W(cleanupState);R(aggregates);}
    for(TZrUInt32 i=0u;i<f->deoptAggregateCount;++i){const SZrExecIrDeoptAggregate *o=&f->deoptAggregates[i];W(identityId);W(typeToken);W(layoutId);R(fields);}
    for(TZrUInt32 i=0u;i<f->deoptAggregateFieldCount;++i){const SZrExecIrDeoptAggregateField *o=&f->deoptAggregateFields[i];W(fieldIndex);W(kind);W(valueId);W(aggregateId);}
    for(TZrUInt32 i=0u;i<f->sourceMapCount;++i){const SZrExecIrSourceMap *o=&f->sourceMaps[i];W(sourceId);W(instructionId);W(startOffset);W(endOffset);W(startLine);W(startColumn);W(endLine);W(endColumn);}
    for(TZrUInt32 i=0u;i<f->bindingRowCount;++i){const SZrExecIrBindingRow *o=&f->bindingRows[i];
        W(rowIndex);W(instructionId);W(segmentIndex);W(sourceId);
        W(contract.bindingKind);W(contract.targetMetadataToken);W(contract.signatureToken);W(contract.ownerTypeToken);
        W(contract.signatureHash);W(contract.moduleSignatureHash);W(contract.layoutVersion);W(contract.dispatchSlot);
        W(contract.layoutHash);W(contract.operation);W(contract.reserved0);W(contract.reserved1);
        W(location.kind);W(location.targetIndex);W(location.ownerDepth);W(location.flags);}
    word(s,f->gcMap != ZR_NULL);
    if(f->gcMap != ZR_NULL){
        {const SZrExecIrGcMap *o=f->gcMap;W(entryCount);W(slotIndexCount);W(inlineRefOffsetCount);W(safepointId);W(sourceId);R(rootRange);}
        for(TZrUInt32 i=0u;i<f->gcMap->entryCount;++i){const SZrExecIrGcMapEntry *o=&f->gcMap->entries[i];W(site);R(liveRefSlots);R(inlineRefOffsets);}
        for(TZrUInt32 i=0u;i<f->gcMap->slotIndexCount;++i)word(s,f->gcMap->slotIndexPool[i]);
        for(TZrUInt32 i=0u;i<f->gcMap->inlineRefOffsetCount;++i)word(s,f->gcMap->inlineRefOffsetPool[i]);}
    word(s,f->stateMap != ZR_NULL);
    if(f->stateMap != ZR_NULL){
        {const SZrExecIrStateMap *o=f->stateMap;W(functionToken);W(signatureHash);W(generation);W(entryCount);W(valueCount);W(rootCount);W(ownerStateCount);}
        for(TZrUInt32 i=0u;i<f->stateMap->entryCount;++i){const SZrExecIrStateMapEntry *o=&f->stateMap->entries[i];
            W(sourceId);W(instructionId);W(deoptId);W(resumeId);W(cleanupState);W(boundaryFlags);W(phase);
            R(liveValues);R(rootValues);R(ownerStates);W(effectIn);W(effectOut);W(handlerBlockId);W(exceptionState);}
        for(TZrUInt32 i=0u;i<f->stateMap->valueCount;++i)word(s,f->stateMap->valuePool[i]);
        for(TZrUInt32 i=0u;i<f->stateMap->rootCount;++i)word(s,f->stateMap->rootPool[i]);
        for(TZrUInt32 i=0u;i<f->stateMap->ownerStateCount;++i)word(s,f->stateMap->ownerStatePool[i]);}
    word(s,f->frameLayout != ZR_NULL);
    if(f->frameLayout != ZR_NULL){ {const SZrExecIrFrameLayout *o=f->frameLayout;
        W(storageSlotCount);W(logicalSlotCount);W(parameterPrefixCount);W(returnBufferOffset);W(frameByteSize);
        W(frameByteAlign);W(parameterCount);W(localCount);W(slotCount);W(layoutHash);}
        for(TZrUInt32 i=0u;i<f->frameLayout->slotCount;++i){const SZrExecIrFrameSlot *o=&f->frameLayout->slots[i];W(slotId);W(byteOffset);W(byteSize);W(byteAlign);W(typeToken);W(kind);}}
}
void ZrTests_ExecOwner_Capture(const SZrExecIrModule *o,SZrOwnerContents *s) {
    s->count=0u;W(id);W(moduleToken);W(moduleHash);capture_contract(&o->contract,s);
    W(constantCount);W(layoutCount);W(sourceMapCount);W(functionCount);
    for(TZrUInt32 i=0u;i<o->constantCount;++i){word(s,o->constants[i].typeToken);word(s,o->constants[i].flags);word(s,o->constants[i].bits);}
    for(TZrUInt32 i=0u;i<o->layoutCount;++i){const SZrExecIrLayout *r=&o->layouts[i];word(s,r->id);word(s,r->typeToken);word(s,r->byteSize);word(s,r->byteAlign);word(s,r->layoutHash);}
    for(TZrUInt32 i=0u;i<o->sourceMapCount;++i){const SZrExecIrSourceMap *r=&o->sourceMaps[i];word(s,r->sourceId);word(s,r->instructionId);word(s,r->startOffset);word(s,r->endOffset);word(s,r->startLine);word(s,r->startColumn);word(s,r->endLine);word(s,r->endColumn);}
    for(TZrUInt32 i=0u;i<o->functionCount;++i)capture_function(&o->functions[i],s);
}
#undef W
#undef R
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
