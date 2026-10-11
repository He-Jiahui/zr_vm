#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "support/ssa_exec_ir_owner_capture.h"
#include "unity.h"

/* Typed schema observation only. No constructor, verifier, owner or VM provider. */
enum { TEST_ROOTS = 64, TEST_MODULES = 2, TEST_CASES = 12 };
typedef struct SRoot { void *pointer; size_t bytes; } SRoot;
typedef struct SWitness {
    unsigned support, feature, drained, calls, accepted, words, expectedWords;
    unsigned graphPreserved, outputChecked, acquired, freed;
    unsigned priorFailed, priorIgnored, flagsPreserved;
    int result;
    SZrOwnerContents output;
} SWitness;
static SRoot g_roots[TEST_ROOTS];
static SZrExecIrModule g_modules[TEST_MODULES];
static SWitness g_witness, g_prior;
static unsigned g_case, g_support, g_feature, g_closed, g_rootCount;
static unsigned g_totalAcquired, g_totalFreed;
static SZrOwnerContents g_output, g_untouched;
static TZrUInt64 g_expected[ZR_OWNER_WORDS];
static unsigned g_expectedCount;
static void support(int yes) { if (!yes) g_support = 0u; }
static void feature(int yes) { if (!yes) g_feature = 0u; }
static void *pool(size_t count, size_t width) {
    if (count == 0u || width == 0u || count > SIZE_MAX / width ||
            g_rootCount == TEST_ROOTS) { support(0); return NULL; }
    void *p = calloc(count, width);
    if (p == NULL) { support(0); return NULL; }
    /* Register the actual return before all support/feature/Unity checks. */
    g_roots[g_rootCount].pointer = p;
    g_roots[g_rootCount].bytes = count * width;
    ++g_rootCount; ++g_witness.acquired; ++g_totalAcquired;
    return p;
}
static TZrUInt64 hash_bytes(TZrUInt64 h, const void *p, size_t n) {
    const unsigned char *bytes = p;
    for (size_t i = 0u; i < n; ++i) { h ^= bytes[i]; h *= UINT64_C(1099511628211); }
    return h;
}
static TZrUInt64 graph_hash(void) {
    TZrUInt64 h = hash_bytes(UINT64_C(1469598103934665603), g_modules, sizeof(g_modules));
    for (unsigned i = 0u; i < g_rootCount; ++i)
        if (g_roots[i].pointer) h = hash_bytes(h, g_roots[i].pointer, g_roots[i].bytes);
    return h;
}
static int words_equal(const SZrOwnerContents *s, const TZrUInt64 *expected, unsigned count) {
    if (s->count != count || count > ZR_OWNER_WORDS) return 0;
    for (unsigned i = 0u; i < count; ++i) if (s->words[i] != expected[i]) return 0;
    return 1;
}
static void expect_words(const TZrUInt64 *words, unsigned count) {
    if (count > ZR_OWNER_WORDS) { support(0); return; }
    memcpy(g_expected, words, (size_t)count * sizeof(*words)); g_expectedCount = count;
    g_witness.expectedWords = count;
}
static void drain(void) {
    if (g_closed) return;
    unsigned failed = (unsigned)Unity.CurrentTestFailed, ignored = (unsigned)Unity.CurrentTestIgnored;
    for (unsigned i = 0u; i < TEST_ROOTS; ++i) {
        void *p = g_roots[i].pointer; g_roots[i].pointer = NULL;
        if (p) { free(p); ++g_witness.freed; ++g_totalFreed; }
    }
    g_witness.drained = (unsigned)(g_witness.acquired == g_witness.freed);
    for (unsigned i = 0u; i < TEST_ROOTS; ++i) support(g_roots[i].pointer == NULL);
    g_witness.priorFailed = failed; g_witness.priorIgnored = ignored;
    g_witness.flagsPreserved = (unsigned)(failed == (unsigned)Unity.CurrentTestFailed &&
        ignored == (unsigned)Unity.CurrentTestIgnored);
    support(g_witness.drained == 1u && g_witness.flagsPreserved == 1u);
    g_witness.support = g_support; g_witness.feature = g_feature; g_closed = 1u;
}
static void finish(void) {
    drain();
    if (g_support != 1u) TEST_FAIL_MESSAGE("OWNER_TRY_CAPTURE_SUPPORT_FAILURE");
    if (g_feature != 1u) TEST_FAIL_MESSAGE("OWNER_TRY_CAPTURE_MISSING_FEATURE");
}
static int observe(const SZrExecIrModule *m, int wantSuccess) {
    memset(&g_output, 0xA5, sizeof(g_output));
    memcpy(&g_untouched, &g_output, sizeof(g_output));
    TZrUInt64 before = graph_hash();
    unsigned failed = (unsigned)Unity.CurrentTestFailed, ignored = (unsigned)Unity.CurrentTestIgnored;
    int result = ZrTests_ExecOwner_TryCapture(m, &g_output);
    ++g_witness.calls; g_witness.result = result;
    support(result == 0 || result == 1);
    support(before == graph_hash());
    support(failed == (unsigned)Unity.CurrentTestFailed && ignored == (unsigned)Unity.CurrentTestIgnored);
    g_witness.graphPreserved &= (unsigned)(before == graph_hash());
    if (result == 0) {
        support(memcmp(&g_output, &g_untouched, sizeof(g_output)) == 0);
        g_witness.words = 0u;
    } else if (result == 1) {
        ++g_witness.accepted;
        support(g_output.count <= ZR_OWNER_WORDS);
        g_witness.words = g_output.count;
        if (wantSuccess) {
            feature(words_equal(&g_output, g_expected, g_expectedCount));
            if (g_output.count <= ZR_OWNER_WORDS)
                for (unsigned i = g_output.count; i < ZR_OWNER_WORDS; ++i)
                    feature(g_output.words[i] == 0u);
        }
    }
    g_witness.outputChecked = 1u;
    feature(wantSuccess ? result == 1 : result == 0);
    return result;
}
static void contract(SZrExecutionContract *c, unsigned base) {
    c->schemaVersion = base; c->abiVersion = base + 1u; c->logicalVersion = base + 2u;
    c->reserved0 = base + 3u; c->generation = base + 4u; c->targetToken = base + 5u;
    c->reserved1 = base + 6u; c->signatureHash = base + 7u; c->layoutHash = base + 8u;
    c->moduleHash = base + 9u; c->requiredCapabilities = base + 10u; c->declaredEffects = base + 11u;
}
static SZrExecIrModule *module(unsigned slot) {
    SZrExecIrModule *m = &g_modules[slot];
    m->id = 1u; m->moduleToken = 2u; m->moduleHash = 3u; contract(&m->contract, 10u);
    return m;
}
static const TZrUInt64 emptyExpected[] = {1,2,3,10,11,12,13,14,15,16,17,18,19,20,21,0,0,0,0};
static void module_pools(SZrExecIrModule *m) {
    m->constants = pool(1u, sizeof(*m->constants));
    m->layouts = pool(1u, sizeof(*m->layouts));
    m->sourceMaps = pool(1u, sizeof(*m->sourceMaps));
    if (!g_support) return;
    m->constantCount = m->constantCapacity = 1u;
    m->layoutCount = m->layoutCapacity = 1u; m->sourceMapCount = m->sourceMapCapacity = 1u;
    m->constants[0] = (SZrExecIrConstant){30u,31u,32u};
    m->layouts[0] = (SZrExecIrLayout){40u,41u,42u,43u,44u};
    m->sourceMaps[0] = (SZrExecIrSourceMap){50u,51u,52u,53u,54u,55u,56u,57u};
}
static void full_function(SZrExecIrModule *m) {
    m->functions = pool(1u, sizeof(*m->functions)); if (!g_support) return;
    m->functionCount = m->functionCapacity = 1u;
    SZrExecIrFunction *f = m->functions;
    f->id = 100u; f->functionToken = 101u; f->signatureHash = 102u;
    f->entryBlockId = 103u; f->bindingRowsSchemaVersion = 104u; f->sealed = 1u;
    contract(&f->contract, 110u);
#define ALLOC(field,count,capacity) do { f->field = pool(1u, sizeof(*f->field)); \
    if (f->field) f->count = f->capacity = 1u; } while (0)
    ALLOC(values,valueCount,valueCapacity); ALLOC(instructions,instructionCount,instructionCapacity);
    ALLOC(blocks,blockCount,blockCapacity); ALLOC(operandPool,operandCount,operandCapacity);
    ALLOC(resultPool,resultCount,resultCapacity); ALLOC(memoryTokenPool,memoryTokenCount,memoryTokenCapacity);
    ALLOC(phiPool,phiCount,phiCapacity); ALLOC(phiIncoming,phiIncomingCount,phiIncomingCapacity);
    ALLOC(predecessors,predecessorCount,predecessorCapacity); ALLOC(successors,successorCount,successorCapacity);
    ALLOC(gcRoots,gcRootCount,gcRootCapacity); ALLOC(deoptStates,deoptStateCount,deoptStateCapacity);
    ALLOC(deoptValues,deoptValueCount,deoptValueCapacity); ALLOC(deoptAggregates,deoptAggregateCount,deoptAggregateCapacity);
    ALLOC(deoptAggregateFields,deoptAggregateFieldCount,deoptAggregateFieldCapacity);
    ALLOC(sourceMaps,sourceMapCount,sourceMapCapacity); ALLOC(bindingRows,bindingRowCount,bindingRowCapacity);
#undef ALLOC
    if (!g_support) return;
    SZrExecIrValue *v = f->values;
    v->id=200u;v->definition=201u;v->typeToken=202u;
    v->ownership=ZR_EXEC_IR_OWNERSHIP_SHARED;v->nullability=ZR_EXEC_IR_NULLABILITY_NULLABLE;v->flags=205u;
    SZrExecIrInstruction *i=f->instructions;
    i->opcode=210u;i->flags=211u;i->results=(SZrExecIrRange){{212u},213u};
    i->operands=(SZrExecIrRange){{214u},215u};i->phiRange=(SZrExecIrRange){{216u},217u};
    i->successorRange=(SZrExecIrRange){{218u},219u};i->typeToken=220u;i->matchTypeToken=221u;i->layoutId=222u;
    i->memoryIn=(SZrExecIrRange){{223u},224u};i->memoryOut=(SZrExecIrRange){{225u},226u};
    i->effectIn=227u;i->effectOut=228u;i->sourceId=229u;i->deoptId=230u;i->bindingRow=231u;
    SZrExecIrBlock *b=f->blocks;
    b->id=240u;b->flags=241u;b->instructions=(SZrExecIrRange){{242u},243u};
    b->predecessors=(SZrExecIrRange){{244u},245u};b->successors=(SZrExecIrRange){{246u},247u};
    b->phis=(SZrExecIrRange){{248u},249u};b->effectPhiResult=250u;
    b->effectPhiIncomings=(SZrExecIrRange){{251u},252u};
    for(unsigned j=0u;j<ZR_EXEC_IR_MEMORY_CLASS_COUNT;++j) {
        b->memoryPhiResults[j]=260u+3u*j;
        b->memoryPhiIncomings[j]=(SZrExecIrRange){{261u+3u*j},262u+3u*j};
    }
    b->immediateDominator=284u;b->terminatorInstructionId=285u;
    f->operandPool[0]=300u;f->resultPool[0]=301u;f->memoryTokenPool[0]=302u;
    f->predecessors[0]=303u;f->successors[0]=304u;f->gcRoots[0]=305u;f->deoptValues[0]=306u;
    f->phiPool[0].result=310u;f->phiPool[0].incomings=(SZrExecIrRange){{311u},312u};
    f->phiIncoming[0]=(SZrExecIrPhiIncoming){320u,321u};
    f->deoptStates[0].id=330u;f->deoptStates[0].sourceId=331u;f->deoptStates[0].resumeId=332u;
    f->deoptStates[0].valueRange=(SZrExecIrRange){{333u},334u};f->deoptStates[0].cleanupState=335u;
    f->deoptStates[0].aggregates=(SZrExecIrRange){{336u},337u};
    f->deoptAggregates[0]=(SZrExecIrDeoptAggregate){340u,341u,342u,{{343u},344u}};
    f->deoptAggregateFields[0].fieldIndex=350u;f->deoptAggregateFields[0].kind=ZR_EXEC_IR_DEOPT_FIELD_AGGREGATE;
    f->deoptAggregateFields[0].valueId=352u;f->deoptAggregateFields[0].aggregateId=353u;
    f->sourceMaps[0]=(SZrExecIrSourceMap){360u,361u,362u,363u,364u,365u,366u,367u};
    SZrExecIrBindingRow *r=f->bindingRows;
    r->rowIndex=370u;r->instructionId=371u;r->segmentIndex=372u;r->sourceId=373u;
    r->contract.bindingKind=374u;r->contract.targetMetadataToken=375u;r->contract.signatureToken=376u;
    r->contract.ownerTypeToken=377u;r->contract.signatureHash=378u;r->contract.moduleSignatureHash=379u;
    r->contract.layoutVersion=380u;r->contract.dispatchSlot=381u;r->contract.layoutHash=382u;
    r->contract.operation=383u;r->contract.reserved0=384u;r->contract.reserved1=385u;
    r->location=(SZrCallBindingLocation){386u,387u,388u,389u};
}
static const TZrUInt64 functionExpected[] = {
    100,101,102,103,104,1,0,110,111,112,113,114,115,116,117,118,119,120,121,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    200,201,202,3,2,205,
    210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,
    240,241,242,243,244,245,246,247,248,249,250,251,252,
    260,261,262,263,264,265,266,267,268,269,270,271,
    272,273,274,275,276,277,278,279,280,281,282,283,284,285,
    300,301,302,303,304,305,306,310,311,312,320,321,
    330,331,332,333,334,335,336,337,340,341,342,343,344,350,2,352,353,
    360,361,362,363,364,365,366,367,
    370,371,372,373,374,375,376,377,378,379,380,381,382,383,384,385,386,387,388,389,
    0,0,0
};
static const TZrUInt64 modulePoolsExpected[] = {
    1,2,3,10,11,12,13,14,15,16,17,18,19,20,21,1,1,1,0,
    30,31,32,40,41,42,43,44,50,51,52,53,54,55,56,57
};
static void ordinary_expected(int modulePools) {
    support(ZR_EXEC_IR_MEMORY_CLASS_COUNT==8);
    if(modulePools) expect_words(modulePoolsExpected,35u); else expect_words(emptyExpected,19u);
    g_expected[18]=1u;
    support(sizeof(functionExpected)/sizeof(*functionExpected)==163u);
    memcpy(g_expected+g_expectedCount,functionExpected,sizeof(functionExpected));
    g_expectedCount+=163u;g_witness.expectedWords=g_expectedCount;
}
static void optional_maps(SZrExecIrFunction *f) {
    f->gcMap=pool(1u,sizeof(*f->gcMap));f->stateMap=pool(1u,sizeof(*f->stateMap));
    f->frameLayout=pool(1u,sizeof(*f->frameLayout));if(!g_support)return;
    SZrExecIrGcMap *gc=f->gcMap;SZrExecIrStateMap *sm=f->stateMap;SZrExecIrFrameLayout *fr=f->frameLayout;
    /* Optional GC object with count/capacity zero is still genuine present storage. */
    gc->entries=pool(1u,sizeof(*gc->entries));gc->slotIndexPool=pool(1u,sizeof(*gc->slotIndexPool));
    gc->inlineRefOffsetPool=pool(1u,sizeof(*gc->inlineRefOffsetPool));
    sm->entries=pool(1u,sizeof(*sm->entries));sm->valuePool=pool(1u,sizeof(*sm->valuePool));
    sm->rootPool=pool(1u,sizeof(*sm->rootPool));sm->ownerStatePool=pool(1u,sizeof(*sm->ownerStatePool));
    fr->slots=pool(1u,sizeof(*fr->slots));if(!g_support)return;
    gc->entryCount=gc->entryCapacity=gc->slotIndexCount=gc->slotIndexCapacity=1u;
    gc->inlineRefOffsetCount=gc->inlineRefOffsetCapacity=1u;gc->safepointId=400u;gc->sourceId=401u;
    gc->rootRange=(SZrExecIrRange){{402u},403u};gc->entries[0].site=410u;
    gc->entries[0].liveRefSlots=(SZrExecIrRange){{411u},412u};
    gc->entries[0].inlineRefOffsets=(SZrExecIrRange){{413u},414u};
    gc->slotIndexPool[0]=420u;gc->inlineRefOffsetPool[0]=421u;
    sm->functionToken=430u;sm->signatureHash=431u;sm->generation=432u;
    sm->entryCount=sm->entryCapacity=sm->valueCount=sm->valueCapacity=1u;
    sm->rootCount=sm->rootCapacity=sm->ownerStateCount=sm->ownerStateCapacity=1u;
    SZrExecIrStateMapEntry *e=sm->entries;
    e->sourceId=440u;e->instructionId=441u;e->deoptId=442u;e->resumeId=443u;e->cleanupState=444u;
    e->boundaryFlags=445u;e->phase=ZR_EXEC_IR_STATE_AFTER_EFFECT;
    e->liveValues=(SZrExecIrRange){{447u},448u};e->rootValues=(SZrExecIrRange){{449u},450u};
    e->ownerStates=(SZrExecIrRange){{451u},452u};e->effectIn=453u;e->effectOut=454u;
    e->handlerBlockId=455u;e->exceptionState=456u;
    sm->valuePool[0]=460u;sm->rootPool[0]=461u;sm->ownerStatePool[0]=462u;
    fr->storageSlotCount=470u;fr->logicalSlotCount=471u;fr->parameterPrefixCount=472u;
    fr->returnBufferOffset=473u;fr->frameByteSize=474u;fr->frameByteAlign=475u;
    fr->parameterCount=476u;fr->localCount=477u;fr->slotCount=fr->slotCapacity=1u;fr->layoutHash=479u;
    fr->slots[0]=(SZrExecIrFrameSlot){480u,481u,482u,483u,484u,ZR_EXEC_IR_FRAME_SLOT_TEMPORARY};
}
static const TZrUInt64 optionalExpected[] = {
    1,1,1,1,400,401,402,403,410,411,412,413,414,420,421,
    1,430,431,432,1,1,1,1,440,441,442,443,444,445,1,447,448,449,450,451,452,453,454,455,456,460,461,462,
    1,470,471,472,473,474,475,476,477,1,479,480,481,482,483,484,3
};
static void populated_expected(void) {
    ordinary_expected(1);g_expectedCount-=3u;
    support(sizeof(optionalExpected)/sizeof(*optionalExpected)==60u);
    memcpy(g_expected+g_expectedCount,optionalExpected,sizeof(optionalExpected));
    g_expectedCount+=60u;g_witness.expectedWords=g_expectedCount;support(g_expectedCount==255u);
}
static void constants(SZrExecIrModule *m,unsigned count,unsigned capacity) {
    m->constants=pool(capacity,sizeof(*m->constants));if(!g_support)return;
    m->constantCount=count;m->constantCapacity=capacity;
    for(unsigned i=0u;i<count;++i)m->constants[i]=(SZrExecIrConstant){600u+i,2000u+i,UINT64_C(100000)+i};
}
static void constants_expected(unsigned count) {
    expect_words(emptyExpected,19u);g_expected[15]=count;
    if(count>(ZR_OWNER_WORDS-19u)/3u){g_witness.expectedWords=19u+3u*count;return;}
    for(unsigned i=0u;i<count;++i) {
        /* Independent indexed fact list; no traversal/provider invocation. */
        g_expected[19u+3u*i]=600u+i;g_expected[20u+3u*i]=2000u+i;
        g_expected[21u+3u*i]=UINT64_C(100000)+i;
    }
    g_expectedCount=19u+3u*count;g_witness.expectedWords=g_expectedCount;
}
void setUp(void) {
    ++g_case;g_support=1u;g_feature=1u;g_closed=0u;g_rootCount=0u;
    for(unsigned i=0u;i<TEST_ROOTS;++i)support(g_roots[i].pointer==NULL);
    memset(g_modules,0,sizeof(g_modules));memset(&g_witness,0,sizeof(g_witness));
    memset(g_expected,0,sizeof(g_expected));g_expectedCount=0u;g_witness.graphPreserved=1u;
}
void tearDown(void) {
    if(g_case==11u && !g_closed) {
        support(Unity.CurrentTestFailed==1u && Unity.CurrentTestIgnored==0u);
        support(g_rootCount==1u && g_roots[0].pointer!=NULL);
        (void)observe(&g_modules[0],1);
        memcpy(&g_witness.output,&g_output,sizeof(g_output));
        drain();memcpy(&g_prior,&g_witness,sizeof(g_prior));
    } else drain();
    /* No assertion, flag reset, recovery or inferred ledger health here. */
}
static void test_capture_empty_module(void) {
    SZrExecIrModule *m=module(0u);expect_words(emptyExpected,19u);
    (void)observe(m,1);finish();
}
static void test_capture_scalar_contract_order(void) {
    SZrExecIrModule *m=module(0u);m->id=70u;m->moduleToken=71u;m->moduleHash=72u;contract(&m->contract,80u);
    const TZrUInt64 expected[]={70,71,72,80,81,82,83,84,85,86,87,88,89,90,91,0,0,0,0};
    expect_words(expected,19u);(void)observe(m,1);finish();
}
static void test_capture_module_pools(void) {
    SZrExecIrModule *m=module(0u);module_pools(m);expect_words(modulePoolsExpected,35u);
    if(g_support)(void)observe(m,1);finish();
}
static void test_capture_function_pools(void) {
    SZrExecIrModule *m=module(0u);full_function(m);ordinary_expected(0);support(g_expectedCount==182u);
    if(g_support)(void)observe(m,1);finish();
}
static void test_capture_optional_nested_maps(void) {
    SZrExecIrModule *m=module(0u);module_pools(m);full_function(m);
    if(g_support)optional_maps(m->functions);populated_expected();
    if(g_support)(void)observe(m,1);finish();
}
static void test_capture_excluded_storage_details(void) {
    SZrExecIrModule *a=module(0u),*b=module(1u);constants(a,1u,1u);constants(b,1u,3u);
    constants_expected(1u);if(g_support) {
        (void)observe(a,1);(void)observe(b,1);
        b->constants[0].bits=UINT64_C(200000);g_expected[21]=UINT64_C(200000);
        (void)observe(b,1);
    }
    finish();
}
static void test_capture_exact_word_budget(void) {
    SZrExecIrModule *m=module(0u);constants(m,1359u,1359u);constants_expected(1359u);
    support(g_expectedCount==4096u);if(g_support)(void)observe(m,1);finish();
}
static void test_capture_null_refusal(void) {
    SZrExecIrModule *m=module(0u);g_witness.expectedWords=0u;
    (void)observe(NULL,0);TZrUInt64 before=graph_hash();
    unsigned failed=(unsigned)Unity.CurrentTestFailed,ignored=(unsigned)Unity.CurrentTestIgnored;
    int result=ZrTests_ExecOwner_TryCapture(m,NULL);++g_witness.calls;
    support(result==0||result==1);feature(result==0);support(before==graph_hash());
    support(failed==(unsigned)Unity.CurrentTestFailed && ignored==(unsigned)Unity.CurrentTestIgnored);
    if(result==1)++g_witness.accepted;finish();
}
static void test_capture_budget_refusal(void) {
    SZrExecIrModule *m=module(0u);constants(m,1360u,1360u);constants_expected(1360u);
    support(g_witness.expectedWords==4099u);if(g_support)(void)observe(m,0);finish();
}
static void test_capture_structural_refusal(void) {
    SZrExecIrModule *m=module(0u);module_pools(m);full_function(m);
    if(g_support)optional_maps(m->functions);g_witness.expectedWords=0u;
    if(g_support) {
        SZrExecIrFunction *f=m->functions;
#define REFUSE(obj,field,count,capacity) do { \
    TZrUInt32 savedCount=(obj)->count,savedCapacity=(obj)->capacity; \
    (obj)->count=2u;(obj)->capacity=1u;(void)observe(m,0); \
    (obj)->count=savedCount;(obj)->capacity=savedCapacity; \
    void *savedPointer=(obj)->field;(obj)->field=NULL;(obj)->count=1u;(obj)->capacity=1u; \
    (void)observe(m,0);(obj)->field=savedPointer;(obj)->count=savedCount;(obj)->capacity=savedCapacity; \
} while(0)
        REFUSE(m,constants,constantCount,constantCapacity);REFUSE(m,layouts,layoutCount,layoutCapacity);
        REFUSE(m,sourceMaps,sourceMapCount,sourceMapCapacity);REFUSE(m,functions,functionCount,functionCapacity);
        REFUSE(f,values,valueCount,valueCapacity);REFUSE(f,instructions,instructionCount,instructionCapacity);
        REFUSE(f,blocks,blockCount,blockCapacity);REFUSE(f,operandPool,operandCount,operandCapacity);
        REFUSE(f,resultPool,resultCount,resultCapacity);REFUSE(f,memoryTokenPool,memoryTokenCount,memoryTokenCapacity);
        REFUSE(f,phiPool,phiCount,phiCapacity);REFUSE(f,phiIncoming,phiIncomingCount,phiIncomingCapacity);
        REFUSE(f,predecessors,predecessorCount,predecessorCapacity);REFUSE(f,successors,successorCount,successorCapacity);
        REFUSE(f,gcRoots,gcRootCount,gcRootCapacity);REFUSE(f,deoptStates,deoptStateCount,deoptStateCapacity);
        REFUSE(f,deoptValues,deoptValueCount,deoptValueCapacity);
        REFUSE(f,deoptAggregates,deoptAggregateCount,deoptAggregateCapacity);
        REFUSE(f,deoptAggregateFields,deoptAggregateFieldCount,deoptAggregateFieldCapacity);
        REFUSE(f,sourceMaps,sourceMapCount,sourceMapCapacity);REFUSE(f,bindingRows,bindingRowCount,bindingRowCapacity);
        REFUSE(f->gcMap,entries,entryCount,entryCapacity);REFUSE(f->gcMap,slotIndexPool,slotIndexCount,slotIndexCapacity);
        REFUSE(f->gcMap,inlineRefOffsetPool,inlineRefOffsetCount,inlineRefOffsetCapacity);
        REFUSE(f->stateMap,entries,entryCount,entryCapacity);REFUSE(f->stateMap,valuePool,valueCount,valueCapacity);
        REFUSE(f->stateMap,rootPool,rootCount,rootCapacity);REFUSE(f->stateMap,ownerStatePool,ownerStateCount,ownerStateCapacity);
        REFUSE(f->frameLayout,slots,slotCount,slotCapacity);
#undef REFUSE
        f->gcMapCount=1u;f->gcMapCapacity=0u;(void)observe(m,0);
        SZrExecIrGcMap *saved=f->gcMap;f->gcMap=NULL;f->gcMapCount=f->gcMapCapacity=1u;
        (void)observe(m,0);f->gcMap=saved;f->gcMapCount=f->gcMapCapacity=0u;
    }
    support(g_witness.calls==60u);finish();
}
static void test_capture_prior_unity_failure(void) {
    SZrExecIrModule *m=module(0u);constants(m,1u,1u);constants_expected(1u);
    /* Even unexpected CRT support failure drains before its supporting assert. */
    if(!g_support)finish();
    TEST_FAIL_MESSAGE("OWNER_TRY_CAPTURE_PRIOR_FAILURE");
}
static void test_capture_prior_failure_witness(void) {
    const TZrUInt64 expected[]={1,2,3,10,11,12,13,14,15,16,17,18,19,20,21,1,0,0,0,600,2000,100000};
    g_witness.expectedWords=22u;
    support(g_prior.support==1u && g_prior.drained==1u && g_prior.calls==1u &&
        g_prior.acquired==1u && g_prior.freed==1u && g_prior.graphPreserved==1u &&
        g_prior.outputChecked==1u && g_prior.priorFailed==1u &&
        g_prior.priorIgnored==0u && g_prior.flagsPreserved==1u);
    support(g_prior.result==0||g_prior.result==1);
    support(g_prior.accepted==(unsigned)(g_prior.result==1));
    support(g_prior.words==(g_prior.result==1?g_prior.output.count:0u));
    for(unsigned i=0u;i<TEST_ROOTS;++i)support(g_roots[i].pointer==NULL);
    support(Unity.CurrentTestFailed==0u && Unity.CurrentTestIgnored==0u);
    if(g_prior.result==0) {
        unsigned char *bytes=(unsigned char *)&g_prior.output;
        for(size_t i=0u;i<sizeof(g_prior.output);++i)support(bytes[i]==0xA5u);
    }
    feature(g_prior.result==1 && g_prior.accepted==1u && g_prior.words==22u &&
        g_prior.expectedWords==22u && words_equal(&g_prior.output,expected,22u));
    if(g_prior.result==1 && g_prior.output.count<=ZR_OWNER_WORDS)
        for(unsigned i=g_prior.output.count;i<ZR_OWNER_WORDS;++i)feature(g_prior.output.words[i]==0u);
    g_witness.outputChecked=1u;finish();
}
static void emit_case(void) {
    printf("OWNER_TRY_CAPTURE_CASE case=%u support=%u feature=%u drained=%u calls=%u accepted=%u words=%u expected_words=%u graph_preserved=%u output_checked=%u acquired=%u freed=%u prior_failed=%u prior_ignored=%u flags_preserved=%u\n",
        g_case,g_witness.support,g_witness.feature,g_witness.drained,g_witness.calls,g_witness.accepted,
        g_witness.words,g_witness.expectedWords,g_witness.graphPreserved,g_witness.outputChecked,
        g_witness.acquired,g_witness.freed,g_witness.priorFailed,g_witness.priorIgnored,g_witness.flagsPreserved);
}
static void final_drain(void) {
    unsigned failed=(unsigned)Unity.CurrentTestFailed,ignored=(unsigned)Unity.CurrentTestIgnored;
    unsigned live=0u;
    for(unsigned i=0u;i<TEST_ROOTS;++i)if(g_roots[i].pointer) {
        void *p=g_roots[i].pointer;g_roots[i].pointer=NULL;free(p);++g_totalFreed;
    }
    for(unsigned i=0u;i<TEST_ROOTS;++i)live+=(unsigned)(g_roots[i].pointer!=NULL);
    unsigned flags=(unsigned)(failed==(unsigned)Unity.CurrentTestFailed && ignored==(unsigned)Unity.CurrentTestIgnored);
    printf("OWNER_TRY_CAPTURE_FINAL drain=%u live=%u acquired=%u freed=%u prior_failed=%u prior_ignored=%u flags_preserved=%u\n",
        (unsigned)(live==0u&&g_totalAcquired==g_totalFreed),live,g_totalAcquired,g_totalFreed,
        failed,ignored,flags);
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_capture_empty_module);emit_case();
    RUN_TEST(test_capture_scalar_contract_order);emit_case();
    RUN_TEST(test_capture_module_pools);emit_case();
    RUN_TEST(test_capture_function_pools);emit_case();
    RUN_TEST(test_capture_optional_nested_maps);emit_case();
    RUN_TEST(test_capture_excluded_storage_details);emit_case();
    RUN_TEST(test_capture_exact_word_budget);emit_case();
    RUN_TEST(test_capture_null_refusal);emit_case();
    RUN_TEST(test_capture_budget_refusal);emit_case();
    RUN_TEST(test_capture_structural_refusal);emit_case();
    RUN_TEST(test_capture_prior_unity_failure);emit_case();
    RUN_TEST(test_capture_prior_failure_witness);emit_case();
    final_drain();return UNITY_END();
}
