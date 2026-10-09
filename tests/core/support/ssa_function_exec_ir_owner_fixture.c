#include "ssa_function_exec_ir_owner_fixture.h"
#include "zr_vm_core/gc.h"
#include <stdio.h>
#include <string.h>
#include "unity.h"
SZrFunction *g_fnExecFunctions[ZR_FN_EXEC_FUNCTIONS];
SZrTestsFunctionAotProducer *g_fnAotProducers[ZR_FN_EXEC_PRODUCERS];
static SZrGlobalState *g_fnGlobal;
static TZrBool g_fnSavedStop,g_fnStopValid;
void ZrTests_FnExec_SetUp(void){
    ZrTests_ExecOwner_SetUp();ZrTests_FnExec_LedgerReset();
    memset(g_fnExecFunctions,0,sizeof(g_fnExecFunctions));memset(g_fnAotProducers,0,sizeof(g_fnAotProducers));
    g_fnGlobal=ZR_NULL;g_fnSavedStop=ZR_FALSE;g_fnStopValid=ZR_FALSE;
}
static void open_global(void){
    if(g_fnGlobal!=ZR_NULL)return;
    g_fnGlobal=ZrCore_GlobalState_New(ZrTests_FnExec_VmAllocate,ZR_NULL,0xE017u,ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_fnGlobal);TEST_ASSERT_NOT_NULL(g_fnGlobal->mainThreadState);TEST_ASSERT_NOT_NULL(g_fnGlobal->garbageCollector);
    g_fnSavedStop=g_fnGlobal->garbageCollector->stopGcFlag;g_fnStopValid=ZR_TRUE;
    g_fnGlobal->garbageCollector->stopGcFlag=ZR_TRUE;
    TEST_ASSERT_NULL(g_fnGlobal->compileSource);TEST_ASSERT_NULL(g_fnGlobal->profileRuntime);
    TEST_ASSERT_NULL(g_fnGlobal->sourceLoader);TEST_ASSERT_NULL(g_fnGlobal->nativeModuleLoader);TEST_ASSERT_NULL(g_fnGlobal->aotModuleLoader);
    TEST_ASSERT_NULL(g_fnGlobal->providerModuleNameResolver);TEST_ASSERT_NULL(g_fnGlobal->callBindingModuleResolver);TEST_ASSERT_NULL(g_fnGlobal->typedCallBindingResolver);
    TEST_ASSERT_NULL(g_fnGlobal->nativeRegistryStateCleanup);TEST_ASSERT_NULL(g_fnGlobal->parserModuleInitStateCleanup);TEST_ASSERT_NULL(g_fnGlobal->postGcCleanup);
    TEST_ASSERT_NULL(g_fnGlobal->callbacks.afterStateInitialized);TEST_ASSERT_NULL(g_fnGlobal->callbacks.beforeStateReleased);
    TEST_ASSERT_NULL(g_fnGlobal->callbacks.afterThreadInitialized);TEST_ASSERT_NULL(g_fnGlobal->callbacks.beforeThreadReleased);
    TEST_ASSERT_EQUAL_UINT(0u,ZrTests_FnExec_VmStats().errors);
}
SZrState *ZrTests_FnExec_State(void){open_global();return g_fnGlobal->mainThreadState;}
SZrGlobalState *ZrTests_FnExec_Global(void){open_global();return g_fnGlobal;}
SZrFunction *ZrTests_FnExec_New(unsigned slot){
    SZrState *state=ZrTests_FnExec_State();TEST_ASSERT_LESS_THAN_UINT(ZR_FN_EXEC_FUNCTIONS,slot);TEST_ASSERT_NULL(g_fnExecFunctions[slot]);
    g_fnExecFunctions[slot]=ZrCore_Function_New(state);TEST_ASSERT_NOT_NULL(g_fnExecFunctions[slot]);return g_fnExecFunctions[slot];
}
void ZrTests_FnExec_Free(unsigned slot){
    SZrFunction *live=g_fnExecFunctions[slot];g_fnExecFunctions[slot]=ZR_NULL;
    if(live!=ZR_NULL)ZrCore_Function_Free(g_fnGlobal->mainThreadState,live);
}
void ZrTests_FnExec_Shutdown(void){
    SZrGlobalState *live=g_fnGlobal;ZrTests_ExecOwner_Disarm();
    if(live==ZR_NULL)return;
    if(g_fnStopValid&&live->garbageCollector!=ZR_NULL)live->garbageCollector->stopGcFlag=g_fnSavedStop;
    g_fnStopValid=ZR_FALSE;g_fnGlobal=ZR_NULL;memset(g_fnExecFunctions,0,sizeof(g_fnExecFunctions));
    ZrCore_GlobalState_Free(live);
}
SZrTestsFunctionAotProducer *ZrTests_FnExec_AotProducer(unsigned slot,TZrInt64 literal){
    TEST_ASSERT_LESS_THAN_UINT(ZR_FN_EXEC_PRODUCERS,slot);TEST_ASSERT_NULL(g_fnAotProducers[slot]);
    g_fnAotProducers[slot]=ZrTests_FunctionAotProducer_New(literal);TEST_ASSERT_NOT_NULL(g_fnAotProducers[slot]);return g_fnAotProducers[slot];
}
unsigned ZrTests_FnExec_AttachAot(SZrFunction *function,const SZrAotIrModule *source){
    SZrAotIrDiagnostic d={0};SZrTestsFunctionAotWitness expected;EZrAotIrStatus status;
    unsigned first=ZrTests_FnExec_AotStats().records,attempts=ZrTests_FnExec_AotStats().attempts;
    ZrTests_FunctionAotWitness_Capture(source,&expected);status=ZrCore_Function_SnapshotAotIr(function,source,&d);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK,status);TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK,d.status);
    TEST_ASSERT_EQUAL_UINT(attempts+1u,ZrTests_FnExec_AotStats().attempts);TEST_ASSERT_EQUAL_UINT(first+1u,ZrTests_FnExec_AotStats().records);
    ZrTests_FunctionAot_AssertView(ZrCore_Function_GetAotIr(function),&expected);
    ZrTests_FunctionAot_AssertIndependent(source,ZrCore_Function_GetAotIr(function));return first;
}
const SZrExecIrModule *ZrTests_FnExec_Attach(SZrFunction *function,const SZrExecIrModule *source){
    SZrExecIrDiagnostic d;SZrOwnerContents expected;TZrBool ok;unsigned first=ZrTests_ExecOwner_Stats().records;
    TZrUInt64 digest=ZrTests_ExecOwner_LiveDigest(source);ZrTests_ExecOwner_Capture(source,&expected);memset(&d,0xA5,sizeof(d));
    ok=ZrCore_Function_SnapshotExecIr(function,source,&d);ZrTests_ExecOwner_Disarm();
    if(!ok){fprintf(stderr,"FUNCTION_EXEC_FEATURE code=%u expectedVersion=%u actualVersion=%u\n",
            (unsigned)d.code,(unsigned)d.expectedVersion,(unsigned)d.actualVersion);
        ZrTests_ExecOwner_AssertReleased(first,ZrTests_ExecOwner_Stats().records);}
    ZrTests_ExecOwner_AssertProducerLive(source);TEST_ASSERT_EQUAL_UINT64(digest,ZrTests_ExecOwner_LiveDigest(source));
    TEST_ASSERT_TRUE_MESSAGE(ok,"actual Function ExecIR attachment must succeed");
    {SZrExecIrDiagnostic zero={0};ZrTests_ExecOwner_Diagnostic(&zero,&d);}
    TEST_ASSERT_NOT_NULL(function->ownedExecIr);ZrTests_ExecOwner_Equal(&expected,ZrCore_Function_GetExecIr(function));
    ZrTests_ExecOwner_Independent(source,ZrCore_Function_GetExecIr(function));return ZrCore_Function_GetExecIr(function);
}
SZrFunction *ZrTests_FnExec_Move(SZrFunction *parent,SZrFunction *donor){
    SZrFunction *child=ZrCore_Memory_RawMallocWithType(g_fnGlobal,sizeof(*child),ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    parent->childFunctionList=child;parent->childFunctionLength=child!=ZR_NULL?1u:0u;
    if(child!=ZR_NULL){*child=*donor;child->ownerFunction=parent;ZrCore_Function_DetachOwnedBuffers(donor);}
    TEST_ASSERT_NOT_NULL(child);return child;
}
void ZrTests_FnExec_TearDown(void){
    SZrOwnerHeapStats exec;
    ZrTests_ExecOwner_Disarm();ZrTests_FnExec_Shutdown();
    for(unsigned i=0u;i<ZR_FN_EXEC_PRODUCERS;++i)ZrTests_FunctionAotProducer_Free(&g_fnAotProducers[i]);
    for(unsigned i=0u;i<ZR_OWNER_SLOTS;++i)ZrCore_ExecIr_OwnedModuleFree(&g_execOwners[i]);
    for(unsigned i=0u;i<ZR_OWNER_MODULE_SLOTS;++i)ZrCore_ExecIr_FreeModule(&g_ownerModules[i]);
    /* Healthy balance is proved after real teardown and before recovery. */
    if(ZrTests_FnExec_VmStats().live==0u&&ZrTests_FnExec_VmStats().errors==0u&&
       ZrTests_FnExec_AotStats().live==0u&&ZrTests_FnExec_AotStats().errors==0u)ZrTests_FnExec_LedgerBalanced();
    ZrTests_FnExec_LedgerRecover();
    exec=ZrTests_ExecOwner_Stats();
    if(exec.live==0u&&exec.errors==0u)ZrTests_ExecOwner_AssertBalanced();
    else fprintf(stderr,"EXEC_OWNER_LEDGER errors=%u live=%u before_function_fallback=1\n",exec.errors,exec.live);
    ZrTests_ExecOwner_Recover();
    ZrTests_FnExec_LedgerBalanced();ZrTests_ExecOwner_AssertBalanced();
}
