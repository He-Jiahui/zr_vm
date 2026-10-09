#include "ssa_exec_ir_module_copy_fixture.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "unity.h"
SZrExecIrModule g_copyOutputs[ZR_COPY_OUTPUTS];
SZrExecIrFunction g_copyFunctions[ZR_COPY_FUNCTIONS];
static unsigned g_outputFirst[ZR_COPY_OUTPUTS],g_outputLast[ZR_COPY_OUTPUTS];
static unsigned g_inputFirst[2],g_inputLast[2];
void ZrTests_ModuleCopy_ZeroDiagnostic(const SZrExecIrDiagnostic *d){SZrExecIrDiagnostic z={0};ZrTests_ExecOwner_Diagnostic(&z,d);}
static void address(SZrCopyState *s,const void *p){if(p==ZR_NULL)return;
    TEST_ASSERT_LESS_THAN_UINT(ZR_COPY_RECORDS,s->addressCount);s->addresses[s->addressCount++]=(uintptr_t)p;}
static void addresses(const SZrExecIrModule *m,SZrCopyState *s){
    TEST_ASSERT_TRUE(m->functionCount<=2u);if(m->functionCount)TEST_ASSERT_NOT_NULL(m->functions);
    address(s,m->constants);address(s,m->layouts);address(s,m->sourceMaps);address(s,m->functions);
    for(unsigned i=0u;i<m->functionCount;++i){const SZrExecIrFunction *f=&m->functions[i];
#define A(field) address(s,f->field)
        A(values);A(instructions);A(blocks);A(operandPool);A(resultPool);A(memoryTokenPool);A(phiPool);A(phiIncoming);
        A(predecessors);A(successors);A(gcRoots);A(deoptStates);A(deoptValues);A(deoptAggregates);A(deoptAggregateFields);
        A(sourceMaps);A(bindingRows);A(gcMap);A(stateMap);A(frameLayout);
#undef A
        if(f->gcMap){address(s,f->gcMap->entries);address(s,f->gcMap->slotIndexPool);address(s,f->gcMap->inlineRefOffsetPool);}
        if(f->stateMap){address(s,f->stateMap->entries);address(s,f->stateMap->valuePool);address(s,f->stateMap->rootPool);address(s,f->stateMap->ownerStatePool);}
        if(f->frameLayout)address(s,f->frameLayout->slots);
    }
}
void ZrTests_ModuleCopy_State(const SZrExecIrModule *m,unsigned first,unsigned last,SZrCopyState *s){
    unsigned records=ZrTests_ExecOwner_Stats().records;memset(s,0,sizeof(*s));
    TEST_ASSERT_TRUE(first<=last&&last<=records);
    for(unsigned i=first;i<last;++i)if(ZrTests_ExecOwner_RecordFrees(i)==0u){
        TEST_ASSERT_LESS_THAN_UINT(ZR_COPY_RECORDS,s->count);s->records[s->count++]=i;}
    /* Reused constructed inputs have at most two fixed one-row functions (<4096 words). */
    TEST_ASSERT_TRUE(m->functionCount<=2u);ZrTests_ExecOwner_Capture(m,&s->contents);addresses(m,s);
}
void ZrTests_ModuleCopy_Unchanged(const SZrCopyState *s,const SZrExecIrModule *m){
    SZrCopyState now={0};for(unsigned i=0u;i<s->count;++i)ZrTests_ExecOwner_AssertLive(s->records[i],s->records[i]+1u);
    addresses(m,&now);TEST_ASSERT_EQUAL_UINT(s->addressCount,now.addressCount);
    for(unsigned i=0u;i<s->addressCount;++i)TEST_ASSERT_EQUAL_UINT64(s->addresses[i],now.addresses[i]);
    ZrTests_ExecOwner_Equal(&s->contents,m);
}
void ZrTests_ModuleCopy_Released(const SZrCopyState *s){for(unsigned i=0u;i<s->count;++i)
    ZrTests_ExecOwner_AssertReleased(s->records[i],s->records[i]+1u);}
void ZrTests_ModuleCopy_SetOutputRange(unsigned slot,unsigned first,unsigned last){
    TEST_ASSERT_LESS_THAN_UINT(ZR_COPY_OUTPUTS,slot);g_outputFirst[slot]=first;g_outputLast[slot]=last;}
void ZrTests_ModuleCopy_OutputState(unsigned slot,SZrCopyState *s){
    ZrTests_ModuleCopy_State(&g_copyOutputs[slot],g_outputFirst[slot],g_outputLast[slot],s);}
void ZrTests_ModuleCopy_InputState(unsigned slot,SZrCopyState *s){
    TEST_ASSERT_LESS_THAN_UINT(2u,slot);ZrTests_ModuleCopy_State(&g_ownerModules[slot],g_inputFirst[slot],g_inputLast[slot],s);}
void ZrTests_ModuleCopy_Expected(const SZrExecIrModule *source,TZrExecIrFunctionId id,const SZrExecIrFunction *replacement,SZrOwnerContents *contents){
    SZrExecIrDiagnostic d;ZrCore_ExecIr_FreeModule(&g_copyOutputs[2]);
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_CloneModule(source,&g_copyOutputs[2],&d),"real lower CloneModule prerequisite");
    ZrTests_ModuleCopy_ZeroDiagnostic(&d);TEST_ASSERT_TRUE(id>0u&&id<=g_copyOutputs[2].functionCount);
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_CloneFunction(replacement,&g_copyOutputs[2].functions[id-1u],&d),"real lower CloneFunction prerequisite");
    ZrTests_ModuleCopy_ZeroDiagnostic(&d);ZrTests_ExecOwner_Capture(&g_copyOutputs[2],contents);
}
static void not_old(const SZrCopyState *old,const void *p){if(p!=ZR_NULL)
    for(unsigned i=0u;i<old->addressCount;++i)TEST_ASSERT_TRUE((uintptr_t)p!=old->addresses[i]);}
static void independent_old(const SZrCopyState *old,const SZrExecIrModule *out){
    /* Compare numeric witnesses before following any possibly old nested storage. */
    TEST_ASSERT_TRUE(out->functionCount<=2u);if(out->functionCount)TEST_ASSERT_NOT_NULL(out->functions);
    not_old(old,out->constants);not_old(old,out->layouts);not_old(old,out->sourceMaps);not_old(old,out->functions);
    for(unsigned i=0u;i<out->functionCount;++i){const SZrExecIrFunction *f=&out->functions[i];
#define N(field) not_old(old,f->field)
        N(values);N(instructions);N(blocks);N(operandPool);N(resultPool);N(memoryTokenPool);N(phiPool);N(phiIncoming);
        N(predecessors);N(successors);N(gcRoots);N(deoptStates);N(deoptValues);N(deoptAggregates);N(deoptAggregateFields);
        N(sourceMaps);N(bindingRows);N(gcMap);N(stateMap);N(frameLayout);
#undef N
        if(f->gcMap){not_old(old,f->gcMap->entries);not_old(old,f->gcMap->slotIndexPool);not_old(old,f->gcMap->inlineRefOffsetPool);}
        if(f->stateMap){not_old(old,f->stateMap->entries);not_old(old,f->stateMap->valuePool);not_old(old,f->stateMap->rootPool);not_old(old,f->stateMap->ownerStatePool);}
        if(f->frameLayout)not_old(old,f->frameLayout->slots);
    }
}
void ZrTests_ModuleCopy_Published(const SZrExecIrModule *source,unsigned output,const SZrCopyState *old,const SZrOwnerContents *expected){
    ZrTests_ModuleCopy_Released(old);independent_old(old,&g_copyOutputs[output]);
    ZrTests_ExecOwner_Independent(source,&g_copyOutputs[output]);ZrTests_ExecOwner_Independent(&g_ownerModules[1],&g_copyOutputs[output]);
    ZrTests_ExecOwner_Equal(expected,&g_copyOutputs[output]);
}
void ZrTests_ModuleCopy_Require(const SZrExecIrModule *source,TZrExecIrFunctionId id,const SZrExecIrFunction *replacement,unsigned output){
    SZrOwnerContents expected;SZrCopyState old,sourceState,replacementState;SZrExecIrDiagnostic d;TZrBool ok;SZrOwnerHeapStats before,after;
    ZrTests_ModuleCopy_InputState(0u,&sourceState);ZrTests_ModuleCopy_InputState(1u,&replacementState);
    ZrTests_ModuleCopy_OutputState(output,&old);ZrTests_ModuleCopy_Expected(source,id,replacement,&expected);
    before=ZrTests_ExecOwner_Stats();memset(&d,0xA5,sizeof(d));
    ok=ZrCore_ExecIr_CloneModuleReplacingFunction(source,id,replacement,&g_copyOutputs[output],&d);
    ZrTests_ExecOwner_Disarm();after=ZrTests_ExecOwner_Stats();
    ZrTests_ModuleCopy_Unchanged(&sourceState,&g_ownerModules[0]);ZrTests_ModuleCopy_Unchanged(&replacementState,&g_ownerModules[1]);
    if(!ok){SZrExecIrDiagnostic unsupported={0};unsupported.code=ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
        ZrTests_ExecOwner_Diagnostic(&unsupported,&d);TEST_ASSERT_EQUAL_UINT(before.attempts,after.attempts);
        TEST_ASSERT_EQUAL_UINT(0u,before.errors);TEST_ASSERT_EQUAL_UINT(0u,after.errors);TEST_ASSERT_EQUAL_UINT(before.live,after.live);
        ZrTests_ModuleCopy_Unchanged(&old,&g_copyOutputs[output]);ZrTests_ExecOwner_AssertReleased(before.records,after.records);
        fprintf(stderr,"MODULE_COPY_FEATURE code=%u attempts=%u\n",(unsigned)d.code,after.attempts-before.attempts);}
    TEST_ASSERT_TRUE_MESSAGE(ok,"actual whole-module selected-function copy must succeed");
    ZrTests_ModuleCopy_ZeroDiagnostic(&d);ZrTests_ModuleCopy_Published(source,output,&old,&expected);
    /* Both source and direct expected storage are live for independence checks. */
    ZrTests_ExecOwner_Independent(source,&g_copyOutputs[output]);ZrTests_ExecOwner_Independent(&g_copyOutputs[2],&g_copyOutputs[output]);
    ZrTests_ModuleCopy_SetOutputRange(output,before.records,after.records);ZrCore_ExecIr_FreeModule(&g_copyOutputs[2]);
}
SZrExecIrModule *ZrTests_ModuleCopy_Source(void){return &g_ownerModules[0];}
SZrExecIrFunction *ZrTests_ModuleCopy_Replacement(void){return &g_ownerModules[1].functions[0];}
void ZrTests_ModuleCopy_Inputs(unsigned mode){
    ZrCore_ExecIr_FreeModule(&g_ownerModules[0]);ZrCore_ExecIr_FreeModule(&g_ownerModules[1]);
    g_inputFirst[0]=ZrTests_ExecOwner_Stats().records;ZrTests_ExecOwner_Build(0u,9u,2u,mode);g_inputLast[0]=ZrTests_ExecOwner_Stats().records;
    g_inputFirst[1]=ZrTests_ExecOwner_Stats().records;ZrTests_ExecOwner_Build(1u,9u,2u,mode);g_inputLast[1]=ZrTests_ExecOwner_Stats().records;
    ZrTests_ModuleCopy_Replacement()->values[0].flags^=1u;
}
void ZrTests_ModuleCopy_SetUp(void){
    SZrExecIrDiagnostic d;unsigned first;ZrTests_ExecOwner_AssertBalanced();ZrTests_ExecOwner_Reset();
    memset(g_outputFirst,0,sizeof(g_outputFirst));memset(g_outputLast,0,sizeof(g_outputLast));
    for(unsigned i=0u;i<ZR_OWNER_MODULE_SLOTS;++i)ZrCore_ExecIr_ModuleInit(&g_ownerModules[i]);
    for(unsigned i=0u;i<ZR_COPY_OUTPUTS;++i)ZrCore_ExecIr_ModuleInit(&g_copyOutputs[i]);
    for(unsigned i=0u;i<ZR_COPY_FUNCTIONS;++i)ZrCore_ExecIr_FunctionInit(&g_copyFunctions[i]);
    ZrTests_ModuleCopy_Inputs(0u);ZrTests_ExecOwner_Build(2u,29u,2u,0u);
    first=ZrTests_ExecOwner_Stats().records;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_CloneModule(&g_ownerModules[2],&g_copyOutputs[0],&d));ZrTests_ModuleCopy_ZeroDiagnostic(&d);
    ZrTests_ModuleCopy_SetOutputRange(0u,first,ZrTests_ExecOwner_Stats().records);
    {SZrOwnerContents witness;ZrTests_ModuleCopy_Expected(ZrTests_ModuleCopy_Source(),1u,ZrTests_ModuleCopy_Replacement(),&witness);}
    ZrCore_ExecIr_FreeModule(&g_copyOutputs[2]);
}
void ZrTests_ModuleCopy_TearDown(void){
    SZrOwnerHeapStats s;ZrTests_ExecOwner_Disarm();
    for(unsigned i=0u;i<ZR_COPY_FUNCTIONS;++i)ZrCore_ExecIr_FreeFunction(&g_copyFunctions[i]);
    for(unsigned i=0u;i<ZR_COPY_OUTPUTS;++i)ZrCore_ExecIr_FreeModule(&g_copyOutputs[i]);
    for(unsigned i=0u;i<ZR_OWNER_MODULE_SLOTS;++i)ZrCore_ExecIr_FreeModule(&g_ownerModules[i]);
    s=ZrTests_ExecOwner_Stats();if(s.errors||s.live)fprintf(stderr,"MODULE_COPY_LEDGER errors=%u live=%u before_fallback=1\n",s.errors,s.live);
    else ZrTests_ExecOwner_AssertBalanced();
    ZrTests_ExecOwner_Recover();ZrTests_ExecOwner_AssertBalanced();
}
