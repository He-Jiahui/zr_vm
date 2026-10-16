#define ZR_TEST_RETURNED_EXEC_NO_REDIRECT
#include "ssa_source_returned_execir_owner_shim.h"
#include <stdio.h>
#include <string.h>
#include "unity.h"
static SZrReturnedExecObservation g_observed;
static unsigned g_fault;
static SZrExecIrOwnedModule *g_directOwner;
/* Independent caller storage sized for the current real private owner ledger.
 * This is not an allocator API capacity or a promise about a future provider.
 * CleanupBegin/ledgers still refuse insufficient storage without writing past it.
 */
enum { RETURNED_EXEC_OWNER_WITNESSES=32768 };
static SZrReturnedExecCleanupRecord g_crtWitnesses[2][ZR_RETURNED_CRT_ROWS];
static SZrReturnedExecCleanupRecord g_ownerWitnesses[RETURNED_EXEC_OWNER_WITNESSES];
static SZrReturnedExecTeardown g_teardown;
static unsigned g_closed;
static unsigned g_lifecycleErrors;
static int record_frees(unsigned record,unsigned *frees){
    int result=ZrTests_ExecOwner_TryRecordFrees(record,frees);
    ++g_observed.recordQueries;
    if(result!=1){++g_observed.recordRefusals;++g_observed.errors;}
    return result;
}
static int failed_event(unsigned first,unsigned last,SZrOwnerHeapEvent *output){
    int found=0,accepted=1;
    for(unsigned i=first;i<last;++i){SZrOwnerHeapEvent event={0};
        int result=ZrTests_ExecOwner_TryEvent(i,&event);++g_observed.eventQueries;
        if(result!=1){++g_observed.eventRefusals;++g_observed.errors;accepted=0;continue;}
        if(event.failed){if(found)++g_observed.errors;else *output=event;found=1;}
    }
    if(!found)++g_observed.errors;
    return found&&accepted;
}
static void address(SZrReturnedExecObservation *o,const void *pointer){
    if(pointer==ZR_NULL)return;
    if(o->addressCount==ZR_RETURNED_EXEC_ADDRESSES){++o->errors;return;}
    o->addresses[o->addressCount++]=(uintptr_t)pointer;
}
static void addresses(SZrReturnedExecObservation *o,const SZrExecIrModule *m){
    address(o,m->constants);address(o,m->layouts);address(o,m->sourceMaps);address(o,m->functions);
    for(unsigned i=0u;i<m->functionCount;++i){const SZrExecIrFunction *f=&m->functions[i];
#define A(field) address(o,f->field)
        A(values);A(instructions);A(blocks);A(operandPool);A(resultPool);A(memoryTokenPool);A(phiPool);A(phiIncoming);
        A(predecessors);A(successors);A(gcRoots);A(deoptStates);A(deoptValues);A(deoptAggregates);A(deoptAggregateFields);
        A(sourceMaps);A(bindingRows);A(gcMap);A(stateMap);A(frameLayout);
#undef A
        if(f->gcMap){address(o,f->gcMap->entries);address(o,f->gcMap->slotIndexPool);address(o,f->gcMap->inlineRefOffsetPool);}
        if(f->stateMap){address(o,f->stateMap->entries);address(o,f->stateMap->valuePool);address(o,f->stateMap->rootPool);address(o,f->stateMap->ownerStatePool);}
        if(f->frameLayout)address(o,f->frameLayout->slots);
    }
}
static TZrBool words_equal(const SZrOwnerContents *a,const SZrOwnerContents *b){
    if(a->count!=b->count)return ZR_FALSE;
    for(unsigned i=0u;i<a->count;++i)if(a->words[i]!=b->words[i])return ZR_FALSE;
    return ZR_TRUE;
}
static void success_storage(SZrFunction *function,SZrOwnerHeapStats before,SZrOwnerHeapStats end){
    const SZrExecIrModule *view;SZrReturnedExecObservation copied={0};
    unsigned first=g_observed.ownerFirst,last=g_observed.ownerLast,live=0u;
    if(first>=last||last>end.records||end.live<before.live){
        ++g_observed.errors;return;}
    /* Real finalization may free temporary candidates within this window. */
    for(unsigned i=first;i<last;++i){unsigned frees=0u;
        if(record_frees(i,&frees)!=1)return;
        if(frees==0u)++live;else if(frees!=1u){++g_observed.errors;return;}}
    if(end.live-before.live!=live){++g_observed.errors;return;}
    if(function==ZR_NULL||function->ownedExecIr==ZR_NULL){++g_observed.errors;return;}
    view=ZrCore_Function_GetExecIr(function);
    if(view==ZR_NULL||(uintptr_t)view==g_observed.moduleAddress||
       (uintptr_t)function->ownedExecIr==g_observed.moduleAddress){++g_observed.errors;return;}
    for(unsigned i=0u;i<g_observed.addressCount;++i)
        if((uintptr_t)view==g_observed.addresses[i]||(uintptr_t)function->ownedExecIr==g_observed.addresses[i]){
            ++g_observed.errors;return;}
    /* Caller supplied a genuine readable Function and its live module graph. */
    g_observed.viewCaptureResult=ZrTests_ExecOwner_TryCapture(view,&copied.contents);
    if(g_observed.viewCaptureResult!=1){++g_observed.errors;return;}
    addresses(&copied,view);g_observed.errors+=copied.errors;
    for(unsigned i=0u;i<copied.addressCount;++i){
        if(copied.addresses[i]==g_observed.moduleAddress)++g_observed.errors;
        for(unsigned j=0u;j<g_observed.addressCount;++j)
            if(copied.addresses[i]==g_observed.addresses[j])++g_observed.errors;
    }
}
TZrBool ZrTests_ReturnedExec_AppendLayout(SZrExecIrModule *module,const SZrExecIrLayout *layout,TZrSize count,SZrExecIrRange *range){
    TZrBool ok=ZrCore_ExecIr_ModuleAppendLayout(module,layout,count,range);
    g_observed.appendResult=ok;
    ++g_observed.appendCalls;
    if(!ok)return ok;
    g_observed.appendCaptureResult=ZrTests_ExecOwner_TryCapture(module,&g_observed.contents);
    if(g_observed.appendCaptureResult!=1||module->functionCount!=1u){++g_observed.errors;return ok;}
    g_observed.constantCount=module->constantCount;g_observed.layoutCount=module->layoutCount;g_observed.functionCount=module->functionCount;
    if(module->constantCount!=0u){g_observed.literalBits=module->constants[0].bits;g_observed.typeToken=module->constants[0].typeToken;}
    if(module->layoutCount!=0u)g_observed.layoutTypeToken=module->layouts[0].typeToken;
    g_observed.moduleToken=module->moduleToken;g_observed.moduleHash=module->moduleHash;
    {const SZrExecIrFunction *f=&module->functions[0];const SZrExecutionContract *m=&module->contract,*c=&f->contract;
        g_observed.entryToken=f->functionToken;g_observed.signatureHash=f->signatureHash;
        g_observed.identityValid=(TZrBool)(m->schemaVersion==ZR_EXECUTION_CONTRACT_SCHEMA_VERSION&&
                c->schemaVersion==ZR_EXECUTION_CONTRACT_SCHEMA_VERSION&&m->abiVersion==ZR_EXECUTION_CONTRACT_ABI_VERSION&&
                c->abiVersion==ZR_EXECUTION_CONTRACT_ABI_VERSION&&m->logicalVersion==ZR_EXECUTION_CONTRACT_LOGICAL_VERSION&&
                c->logicalVersion==ZR_EXECUTION_CONTRACT_LOGICAL_VERSION&&m->targetToken==module->moduleToken&&
                c->targetToken==f->functionToken&&m->moduleHash==module->moduleHash&&c->moduleHash==module->moduleHash&&
                c->signatureHash==f->signatureHash&&m->signatureHash!=0u&&m->generation!=0u&&m->generation==c->generation);}
    g_observed.moduleAddress=(uintptr_t)module;g_observed.addressCount=0u;addresses(&g_observed,module);
    return ok;
}
TZrBool ZrTests_ReturnedExec_Snapshot(SZrFunction *function,const SZrExecIrModule *source,SZrExecIrDiagnostic *diagnostic){
    SZrOwnerContents before={0},after={0};SZrOwnerHeapStats s;TZrBool ok;
    ++g_observed.snapshotCalls;
    g_observed.beforeCaptureResult=ZrTests_ExecOwner_TryCapture(source,&before);
    if((uintptr_t)source!=g_observed.moduleAddress||g_observed.beforeCaptureResult!=1||
            source->functionCount!=1u||g_observed.appendCalls!=1u){
        ++g_observed.errors;g_fault=0u;
        ok=ZrCore_Function_SnapshotExecIr(function,source,diagnostic);ZrTests_ExecOwner_Disarm();
        g_observed.snapshotResult=ok;if(diagnostic!=ZR_NULL)g_observed.actual=*diagnostic;
        return ok;}
    g_observed.functionAddress=(uintptr_t)function;
    if(function==ZR_NULL||function->ownedExecIr!=ZR_NULL||function->ownedAotIr!=ZR_NULL||
       !function->hasSourceCallableIdentity||function->moduleSignatureHash!=source->moduleHash||
       function->sourceCallableIdentity.canonicalSignatureHash!=source->functions[0].signatureHash){
        ++g_observed.errors;g_fault=0u;}
    if(!words_equal(&before,&g_observed.contents))++g_observed.errors;
    if(g_fault!=0u){
        unsigned beforeLive,beforeAttempts;
        s=ZrTests_ExecOwner_Stats();g_observed.witnessFirst=s.records;beforeLive=s.live;beforeAttempts=s.attempts;
        g_observed.directArmResult=ZrTests_ExecOwner_TryArm(g_fault);
        if(g_observed.directArmResult!=1){++g_observed.errors;g_fault=0u;}
        else {
            g_observed.directResult=ZrCore_ExecIr_CloneOwnedModule(source,&g_directOwner,&g_observed.direct);
            ZrTests_ExecOwner_Disarm();s=ZrTests_ExecOwner_Stats();g_observed.witnessLast=s.records;
            g_observed.directAttempts=s.attempts-beforeAttempts;
            g_observed.directEventResult=failed_event(beforeAttempts,s.attempts,&g_observed.directEvent);
            ZrCore_ExecIr_OwnedModuleFree(&g_directOwner);
            {SZrOwnerHeapStats released=ZrTests_ExecOwner_Stats();
                if(g_observed.directResult||released.live!=beforeLive)++g_observed.errors;}
            for(unsigned i=g_observed.witnessFirst;i<g_observed.witnessLast;++i){unsigned frees=0u;
                if(record_frees(i,&frees)==1&&frees!=1u)++g_observed.errors;}
        }
    }
    s=ZrTests_ExecOwner_Stats();g_observed.ownerFirst=s.records;
    if(g_fault!=0u){g_observed.actualArmResult=ZrTests_ExecOwner_TryArm(g_fault);
        if(g_observed.actualArmResult!=1){++g_observed.errors;g_fault=0u;}}
    ok=ZrCore_Function_SnapshotExecIr(function,source,diagnostic);
    ZrTests_ExecOwner_Disarm();g_observed.snapshotResult=ok;
    if(diagnostic!=ZR_NULL)g_observed.actual=*diagnostic;
    {SZrOwnerHeapStats end=ZrTests_ExecOwner_Stats();g_observed.ownerLast=end.records;
        g_observed.ownerAttempts=end.attempts-s.attempts;
        if(g_fault!=0u)g_observed.actualEventResult=failed_event(s.attempts,end.attempts,&g_observed.actualEvent);
        if(!ok){if(end.live!=s.live)++g_observed.errors;
            for(unsigned i=g_observed.ownerFirst;i<g_observed.ownerLast;++i){unsigned frees=0u;
                if(record_frees(i,&frees)==1&&frees!=1u)++g_observed.errors;}}
        else success_storage(function,s,end);}
    g_observed.afterCaptureResult=ZrTests_ExecOwner_TryCapture(source,&after);
    if(g_observed.afterCaptureResult!=1||!words_equal(&before,&after))++g_observed.errors;
    g_fault=0u;return ok;
}
static int prior_health(void){
    SZrReturnedVmStats vm={0};SZrOwnerHeapStats owner=ZrTests_ExecOwner_Stats();
    SZrReturnedHeapStats crt[2];
    if(g_directOwner!=ZR_NULL||g_fault!=0u||ZrTests_Returned_TryVmStats(&vm)!=1||
            vm.live||vm.errors||vm.diagnosticCount||vm.dropped||owner.live||owner.errors||
            owner.records!=owner.frees||owner.records>RETURNED_EXEC_OWNER_WITNESSES)return 0;
    for(unsigned k=0u;k<2u;++k){
        if(ZrTests_Returned_TryStats(k,&crt[k])!=1||crt[k].live||crt[k].errors||
                crt[k].successes!=crt[k].frees||crt[k].successes>ZR_RETURNED_CRT_ROWS)return 0;
        for(unsigned i=0u;i<crt[k].successes;++i){unsigned frees=0u;
            if(ZrTests_Returned_TryRecordFrees(k,i,&frees)!=1||frees!=1u)return 0;}
    }
    for(unsigned i=0u;i<owner.records;++i){unsigned frees=0u;
        if(ZrTests_ExecOwner_TryRecordFrees(i,&frees)!=1||frees!=1u)return 0;}
    if(g_teardown.ledgers.begun==0u)return g_closed==0u&&g_lifecycleErrors==0u&&g_observed.errors==0u;
    if(g_closed!=1u||g_teardown.beginResult!=1||g_teardown.vmResult!=1||
            g_teardown.observerErrors||g_teardown.vm.live||g_teardown.vm.errors||
            g_teardown.vm.diagnosticCount||g_teardown.vm.dropped||
            g_teardown.ledgers.closed!=1u||g_teardown.ledgers.completed!=1u||
            g_teardown.ledgers.healthyBeforeRecovery!=1u||g_teardown.ledgers.refusedQueries||
            g_teardown.ledgers.observationErrors)return 0;
#define SAME(a,b,f) if((a).f!=(b).f)return 0
    SAME(vm,g_teardown.vm,attempts);SAME(vm,g_teardown.vm,frees);
    SAME(owner,g_teardown.ledgers.ownerAfter,attempts);SAME(owner,g_teardown.ledgers.ownerAfter,records);
    SAME(owner,g_teardown.ledgers.ownerAfter,frees);SAME(owner,g_teardown.ledgers.ownerAfter,failures);
    for(unsigned k=0u;k<2u;++k){
        if(g_teardown.ledgers.crtBeforeResult[k]!=1||g_teardown.ledgers.crtAfterResult[k]!=1||
                g_teardown.ledgers.crtRecordCount[k]!=crt[k].successes)return 0;
        SAME(crt[k],g_teardown.ledgers.crtAfter[k],attempts);
        SAME(crt[k],g_teardown.ledgers.crtAfter[k],successes);
        SAME(crt[k],g_teardown.ledgers.crtAfter[k],frees);
        SAME(crt[k],g_teardown.ledgers.crtAfter[k],failures);
        for(unsigned i=0u;i<crt[k].successes;++i){const SZrReturnedExecCleanupRecord *w=&g_crtWitnesses[k][i];
            if(w->beforeResult!=1||w->afterResult!=1||w->beforeFrees!=1u||w->afterFrees!=1u)return 0;}
    }
#undef SAME
    if(g_teardown.ledgers.ownerRecordCount!=owner.records)return 0;
    for(unsigned i=0u;i<owner.records;++i){const SZrReturnedExecCleanupRecord *w=&g_ownerWitnesses[i];
        if(w->beforeResult!=1||w->afterResult!=1||w->beforeFrees!=1u||w->afterFrees!=1u)return 0;}
    return 1;
}
void ZrTests_ReturnedExec_Init(void){
    int verified=prior_health();
    SZrReturnedExecCleanupStorage storage={{g_crtWitnesses[0],g_crtWitnesses[1]},
            {ZR_RETURNED_CRT_ROWS,ZR_RETURNED_CRT_ROWS},g_ownerWitnesses,RETURNED_EXEC_OWNER_WITNESSES};
    TEST_ASSERT_TRUE_MESSAGE(verified,"prior returned ExecIR teardown health/witness refused; preserve ledgers");
    if(!verified)return;
    ZrTests_Returned_Init();ZrTests_ExecOwner_Reset();
    memset(&g_observed,0,sizeof(g_observed));memset(&g_teardown,0,sizeof(g_teardown));
    memset(g_crtWitnesses,0,sizeof(g_crtWitnesses));memset(g_ownerWitnesses,0,sizeof(g_ownerWitnesses));
    g_fault=0u;g_directOwner=ZR_NULL;g_lifecycleErrors=0u;g_closed=0u;
    g_teardown.beginResult=ZrTests_ReturnedExec_CleanupBegin(&g_teardown.ledgers,&storage,verified);
    TEST_ASSERT_EQUAL_INT(1,g_teardown.beginResult);
}
void ZrTests_ReturnedExec_Fault(unsigned index){TEST_ASSERT_TRUE(index>0u);TEST_ASSERT_EQUAL_UINT(0u,g_fault);g_fault=index;}
SZrReturnedExecObservation ZrTests_ReturnedExec_Observation(void){return g_observed;}
SZrReturnedExecTeardown ZrTests_ReturnedExec_Teardown(void){return g_teardown;}
SZrFunction *ZrTests_ReturnedExec_Compile(unsigned slot,const char *source,TZrBool ordinary,const SZrSourceCompileOptions *options,SZrSourceCompileDiagnostic *diagnostic){
    SZrOwnerHeapStats producer=ZrTests_ExecOwner_Stats();
    g_lifecycleErrors+=g_observed.errors;
    memset(&g_observed,0,sizeof(g_observed));g_observed.producerFirst=producer.records;
    return ZrTests_Returned_Compile(slot,source,ordinary,options,diagnostic);
}
void ZrTests_ReturnedExec_AssertObservation(void){TEST_ASSERT_EQUAL_UINT(0u,g_observed.errors);
    TEST_ASSERT_EQUAL_UINT(1u,g_observed.appendCalls);TEST_ASSERT_TRUE(g_observed.contents.count>0u);}
void ZrTests_ReturnedExec_AssertProducerReleased(void){
    SZrOwnerHeapStats s=ZrTests_ExecOwner_Stats();
    for(unsigned i=g_observed.producerFirst;i<s.records;++i)if(i<g_observed.ownerFirst||i>=g_observed.ownerLast)
        TEST_ASSERT_EQUAL_UINT(1u,ZrTests_ExecOwner_RecordFrees(i));
}
void ZrTests_ReturnedExec_AssertView(const SZrReturnedExecObservation *observed,const SZrExecIrModule *view){
    SZrReturnedExecObservation retained={0};int result;
    TEST_ASSERT_NOT_NULL(view);result=ZrTests_ExecOwner_TryCapture(view,&retained.contents);
    TEST_ASSERT_EQUAL_INT(1,result);if(result!=1)return;
    TEST_ASSERT_TRUE(words_equal(&observed->contents,&retained.contents));addresses(&retained,view);
    TEST_ASSERT_EQUAL_UINT(0u,retained.errors);
    for(unsigned i=0u;i<retained.addressCount;++i)for(unsigned j=0u;j<observed->addressCount;++j)
        TEST_ASSERT_TRUE(retained.addresses[i]!=observed->addresses[j]);
}
void ZrTests_ReturnedExec_Close(void){
    if(g_closed==1u)return;
    ZrTests_ExecOwner_Disarm();g_fault=0u;ZrCore_ExecIr_OwnedModuleFree(&g_directOwner);
    ZrTests_Returned_Shutdown();
    g_teardown.vmResult=ZrTests_Returned_TryVmStats(&g_teardown.vm);
    g_teardown.observerErrors=g_lifecycleErrors+g_observed.errors;
    ZrTests_ReturnedExec_CleanupLedgers(&g_teardown.ledgers);
    g_closed=1u;
    if(g_teardown.observerErrors||g_teardown.vmResult!=1||g_teardown.vm.live||g_teardown.vm.errors||
            g_teardown.vm.diagnosticCount||g_teardown.vm.dropped||
            g_teardown.ledgers.completed!=1u||g_teardown.ledgers.healthyBeforeRecovery!=1u||
            g_teardown.ledgers.refusedQueries||g_teardown.ledgers.observationErrors)
        fprintf(stderr,"RETURNED_EXEC_LEDGER observer_errors=%u vm_result=%d vm_live=%u vm_errors=%u completed=%u healthy=%u refused=%u observation_errors=%u\n",
                g_teardown.observerErrors,g_teardown.vmResult,g_teardown.vm.live,g_teardown.vm.errors,
                g_teardown.ledgers.completed,g_teardown.ledgers.healthyBeforeRecovery,
                g_teardown.ledgers.refusedQueries,g_teardown.ledgers.observationErrors);
}
