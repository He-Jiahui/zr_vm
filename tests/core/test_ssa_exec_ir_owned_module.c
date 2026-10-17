#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "support/ssa_exec_ir_owned_module_fixture.h"
void setUp(void) { ZrTests_ExecOwner_SetUp(); }
void tearDown(void) { ZrTests_ExecOwner_TearDown(); }
static void zero_diagnostic(const SZrExecIrDiagnostic *d) {
    SZrExecIrDiagnostic zero={0};ZrTests_ExecOwner_Diagnostic(&zero,d);
}
static const SZrExecIrModule *owned(unsigned slot,const SZrExecIrModule *source) {
    SZrExecIrDiagnostic d;TZrBool ok;
    SZrOwnerContents expected;
    TZrUInt64 before=ZrTests_ExecOwner_LiveDigest(source);
    unsigned records=ZrTests_ExecOwner_Stats().records;
    ZrTests_ExecOwner_Capture(source,&expected);memset(&d,0xA5,sizeof(d));
    ok=ZrCore_ExecIr_CloneOwnedModule(source,&g_execOwners[slot],&d);
    ZrTests_ExecOwner_Disarm();
    if(!ok){
        fprintf(stderr,"EXEC_OWNER_FEATURE code=%u expectedVersion=%u actualVersion=%u\n",
                (unsigned)d.code,(unsigned)d.expectedVersion,(unsigned)d.actualVersion);
        ZrTests_ExecOwner_AssertReleased(records,ZrTests_ExecOwner_Stats().records);
    }
    ZrTests_ExecOwner_AssertProducerLive(source);
    TEST_ASSERT_EQUAL_UINT64(before,ZrTests_ExecOwner_LiveDigest(source));
    TEST_ASSERT_TRUE_MESSAGE(ok,"real owned-module constructor must succeed");
    TEST_ASSERT_NOT_NULL(g_execOwners[slot]);zero_diagnostic(&d);
    ZrTests_ExecOwner_Equal(&expected,ZrCore_ExecIr_OwnedModuleView(g_execOwners[slot]));
    ZrTests_ExecOwner_Independent(source,ZrCore_ExecIr_OwnedModuleView(g_execOwners[slot]));
    return ZrCore_ExecIr_OwnedModuleView(g_execOwners[slot]);
}
static void test_full_constructed_module_clone_prerequisite(void) {
    SZrExecIrModule *source=ZrTests_ExecOwner_Build(0u,9u,2u,0u);
    SZrOwnerContents expected;SZrExecIrDiagnostic d={0};TZrUInt64 digest;
    ZrTests_ExecOwner_Capture(source,&expected);digest=ZrTests_ExecOwner_LiveDigest(source);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_CloneModule(source,&g_ownerModules[1],&d));zero_diagnostic(&d);
    ZrTests_ExecOwner_Equal(&expected,&g_ownerModules[1]);ZrTests_ExecOwner_Independent(source,&g_ownerModules[1]);
    ZrTests_ExecOwner_AssertProducerLive(source);TEST_ASSERT_EQUAL_UINT64(digest,ZrTests_ExecOwner_LiveDigest(source));
    for(unsigned i=0u;i<2u;++i){
        TEST_ASSERT_TRUE(ZrCore_ExecIr_StateMapStorageValid(source->functions[i].stateMap));
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionValidateBindingRows(&source->functions[i],&d));}
    ZrCore_ExecIr_FreeModule(&g_ownerModules[1]);
    source=ZrTests_ExecOwner_Build(2u,8u,1u,1u);ZrTests_ExecOwner_Capture(source,&expected);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_CloneModule(source,&g_ownerModules[1],&d));
    ZrTests_ExecOwner_Equal(&expected,&g_ownerModules[1]);ZrTests_ExecOwner_Independent(source,&g_ownerModules[1]);
    ZrCore_ExecIr_FreeModule(&g_ownerModules[1]);
    source=ZrTests_ExecOwner_Build(3u,7u,1u,2u);ZrTests_ExecOwner_Capture(source,&expected);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_CloneModule(source,&g_ownerModules[1],&d));
    ZrTests_ExecOwner_Equal(&expected,&g_ownerModules[1]);ZrTests_ExecOwner_Independent(source,&g_ownerModules[1]);
}
static void test_empty_owned_module_and_null_view(void) {
    unsigned first=ZrTests_ExecOwner_Stats().records,last;
    TEST_ASSERT_NULL(ZrCore_ExecIr_OwnedModuleView(ZR_NULL));
    (void)owned(0u,ZrTests_ExecOwner_Build(0u,1u,0u,0u));last=ZrTests_ExecOwner_Stats().records;
    ZrCore_ExecIr_OwnedModuleFree(ZR_NULL);ZrCore_ExecIr_OwnedModuleFree(&g_execOwners[0]);
    TEST_ASSERT_NULL(g_execOwners[0]);ZrTests_ExecOwner_AssertReleased(first,last);
    ZrCore_ExecIr_OwnedModuleFree(&g_execOwners[0]);ZrTests_ExecOwner_AssertReleased(first,last);
}
static void test_owned_two_functions_preserve_local_value_ids(void) {
    const SZrExecIrModule *view=owned(0u,ZrTests_ExecOwner_Build(0u,9u,2u,0u));
    TEST_ASSERT_EQUAL_UINT32(2u,view->functionCount);
    TEST_ASSERT_EQUAL_UINT32(1u,view->functions[0].values[0].id);
    TEST_ASSERT_EQUAL_UINT32(1u,view->functions[1].values[0].id);
    TEST_ASSERT_EQUAL_UINT32(1u,view->functions[0].gcMapCount);
    TEST_ASSERT_EQUAL_UINT32(1u,view->functions[0].gcMapCapacity);
    TEST_ASSERT_EQUAL_UINT32(2u,view->functions[0].gcMap->entryCount);
    TEST_ASSERT_EQUAL_UINT32(1u,view->functions[0].gcMap->entries[1].liveRefSlots.start);
    TEST_ASSERT_TRUE(view->functions[0].values != view->functions[1].values);
    TEST_ASSERT_NOT_EQUAL_UINT32(view->functions[0].functionToken,view->functions[1].functionToken);
}
static void test_owned_view_survives_full_producer_teardown(void) {
    SZrExecIrModule *source=ZrTests_ExecOwner_Build(0u,9u,2u,0u);SZrOwnerContents expected;
    unsigned producerLast=ZrTests_ExecOwner_Stats().records;
    ZrTests_ExecOwner_Capture(source,&expected);(void)owned(0u,source);
    ZrCore_ExecIr_FreeModule(source);ZrTests_ExecOwner_AssertReleased(0u,producerLast);
    ZrTests_ExecOwner_Equal(&expected,ZrCore_ExecIr_OwnedModuleView(g_execOwners[0]));
}
static void test_present_empty_maps_preserve_header_identity(void) {
    const SZrExecIrModule *view=owned(0u,ZrTests_ExecOwner_Build(0u,8u,1u,1u));
    const SZrExecIrFunction *f=&view->functions[0];
    TEST_ASSERT_NOT_NULL(f->stateMap);TEST_ASSERT_NOT_NULL(f->gcMap);TEST_ASSERT_NOT_NULL(f->frameLayout);
    TEST_ASSERT_EQUAL_UINT32(0u,f->stateMap->entryCount);TEST_ASSERT_NULL(f->stateMap->entries);
    TEST_ASSERT_EQUAL_UINT32(0u,f->gcMap->entryCount);TEST_ASSERT_NULL(f->gcMap->entries);
    TEST_ASSERT_EQUAL_UINT32(1u,f->gcMapCount);TEST_ASSERT_EQUAL_UINT32(1u,f->gcMapCapacity);
    TEST_ASSERT_EQUAL_UINT32(0u,f->frameLayout->slotCount);TEST_ASSERT_NULL(f->frameLayout->slots);
    TEST_ASSERT_EQUAL_UINT32(f->functionToken,f->stateMap->functionToken);
    TEST_ASSERT_EQUAL_UINT64(f->signatureHash,f->stateMap->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(f->contract.generation,f->stateMap->generation);
    view=owned(1u,ZrTests_ExecOwner_Build(1u,7u,1u,2u));
    TEST_ASSERT_NULL(view->functions[0].stateMap);TEST_ASSERT_NULL(view->functions[0].gcMap);
    TEST_ASSERT_EQUAL_UINT32(0u,view->functions[0].gcMapCount);
    TEST_ASSERT_EQUAL_UINT32(0u,view->functions[0].gcMapCapacity);
    TEST_ASSERT_NULL(view->functions[0].frameLayout);
}
static void test_two_owned_modules_release_independently(void) {
    SZrExecIrModule *a=ZrTests_ExecOwner_Build(0u,9u,2u,0u),*b=ZrTests_ExecOwner_Build(1u,8u,2u,0u);
    SZrOwnerContents expected;unsigned first=ZrTests_ExecOwner_Stats().records,last;
    (void)owned(0u,a);last=ZrTests_ExecOwner_Stats().records;
    ZrTests_ExecOwner_Capture(b,&expected);(void)owned(1u,b);
    ZrTests_ExecOwner_Independent(ZrCore_ExecIr_OwnedModuleView(g_execOwners[0]),ZrCore_ExecIr_OwnedModuleView(g_execOwners[1]));
    ZrCore_ExecIr_OwnedModuleFree(&g_execOwners[0]);ZrTests_ExecOwner_AssertReleased(first,last);
    ZrTests_ExecOwner_Equal(&expected,ZrCore_ExecIr_OwnedModuleView(g_execOwners[1]));
}
static void test_successful_replacement_releases_prior_owner_once(void) {
    SZrExecIrModule *a=ZrTests_ExecOwner_Build(0u,9u,2u,0u),*b=ZrTests_ExecOwner_Build(1u,8u,2u,0u);
    unsigned first=ZrTests_ExecOwner_Stats().records,last;
    (void)owned(0u,a);last=ZrTests_ExecOwner_Stats().records;(void)owned(0u,b);
    ZrTests_ExecOwner_AssertReleased(first,last);
    (void)owned(0u,ZrTests_ExecOwner_Build(2u,7u,1u,1u));
    (void)owned(0u,ZrTests_ExecOwner_Build(3u,6u,1u,2u));
}
static void test_malformed_gc_outer_after_prior_function_preserves_owner(void) {
    SZrExecIrModule *source=ZrTests_ExecOwner_Build(0u,9u,2u,0u);
    const SZrExecIrModule *view=owned(0u,source);
    SZrOwnerContents expected;SZrExecIrDiagnostic d,e={0};SZrOwnerHeapStats start,end;
    TZrUInt64 sourceDigest,keeperDigest;TZrBool ok;
    ZrTests_ExecOwner_Capture(view,&expected);keeperDigest=ZrTests_ExecOwner_LiveDigest(view);
    source->functions[1].gcMapCount=source->functions[1].gcMapCapacity=0u;
    sourceDigest=ZrTests_ExecOwner_LiveDigest(source);start=ZrTests_ExecOwner_Stats();
    memset(&d,0xA5,sizeof(d));ok=ZrCore_ExecIr_CloneOwnedModule(source,&g_execOwners[0],&d);
    end=ZrTests_ExecOwner_Stats();
    int unchanged=sourceDigest==ZrTests_ExecOwner_LiveDigest(source);
    /* Successful replacement invalidates view: follow it only on refusal. */
    int keeperUnchanged=ok?0:keeperDigest==ZrTests_ExecOwner_LiveDigest(view);
    source->functions[1].gcMapCount=source->functions[1].gcMapCapacity=1u;
    e.code=ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;e.functionToken=source->functions[1].functionToken;
    TEST_ASSERT_FALSE(ok);TEST_ASSERT_TRUE(unchanged);TEST_ASSERT_TRUE(keeperUnchanged);
    ZrTests_ExecOwner_Diagnostic(&e,&d);ZrTests_ExecOwner_Equal(&expected,view);
    ZrTests_ExecOwner_AssertReleased(start.records,end.records);
    TEST_ASSERT_EQUAL_UINT(start.live,end.live);TEST_ASSERT_EQUAL_UINT(0u,end.errors);
    /* Earlier candidate allocations are allowed; cleanup is the contract. */
    (void)owned(0u,source);
}
static void test_old_view_self_source_replacement_is_atomic(void) {
    SZrOwnerContents expected;SZrExecIrDiagnostic d={0};TZrBool ok;
    SZrExecIrModule *producer=ZrTests_ExecOwner_Build(0u,9u,2u,0u);
    unsigned first=ZrTests_ExecOwner_Stats().records,last;
    const SZrExecIrModule *view=owned(0u,producer);last=ZrTests_ExecOwner_Stats().records;
    ZrTests_ExecOwner_Capture(view,&expected);
    ok=ZrCore_ExecIr_CloneOwnedModule(view,&g_execOwners[0],&d);ZrTests_ExecOwner_Disarm();
    /* The old view is never read or compared after successful replacement. */
    TEST_ASSERT_TRUE(ok);zero_diagnostic(&d);ZrTests_ExecOwner_AssertReleased(first,last);
    ZrTests_ExecOwner_Equal(&expected,ZrCore_ExecIr_OwnedModuleView(g_execOwners[0]));
}
static void test_null_arguments_preserve_existing_owner(void) {
    SZrExecIrModule *source=ZrTests_ExecOwner_Build(0u,9u,2u,0u);
    SZrExecIrDiagnostic d,expected={0};TZrBool a,b;TZrUInt64 digest,sourceDigest;unsigned attempts;
    SZrExecIrOwnedModule *old;const SZrExecIrModule *view;
    (void)owned(0u,source);old=g_execOwners[0];view=ZrCore_ExecIr_OwnedModuleView(old);
    digest=ZrTests_ExecOwner_LiveDigest(view);sourceDigest=ZrTests_ExecOwner_LiveDigest(source);
    attempts=ZrTests_ExecOwner_Stats().attempts;expected.code=ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
    memset(&d,0xA5,sizeof(d));a=ZrCore_ExecIr_CloneOwnedModule(ZR_NULL,&g_execOwners[0],&d);
    TEST_ASSERT_FALSE(a);ZrTests_ExecOwner_Diagnostic(&expected,&d);TEST_ASSERT_EQUAL_PTR(old,g_execOwners[0]);
    memset(&d,0xA5,sizeof(d));b=ZrCore_ExecIr_CloneOwnedModule(source,ZR_NULL,&d);
    TEST_ASSERT_FALSE(b);ZrTests_ExecOwner_Diagnostic(&expected,&d);TEST_ASSERT_EQUAL_PTR(old,g_execOwners[0]);
    TEST_ASSERT_EQUAL_UINT64(digest,ZrTests_ExecOwner_LiveDigest(view));
    ZrTests_ExecOwner_AssertProducerLive(source);TEST_ASSERT_EQUAL_UINT64(sourceDigest,ZrTests_ExecOwner_LiveDigest(source));
    TEST_ASSERT_EQUAL_UINT(attempts,ZrTests_ExecOwner_Stats().attempts);
}
static void test_null_diagnostic_supports_success_and_refusal(void) {
    SZrExecIrModule *source=ZrTests_ExecOwner_Build(0u,9u,2u,0u);SZrOwnerContents expected;
    TZrBool ok;ZrTests_ExecOwner_Capture(source,&expected);
    ok=ZrCore_ExecIr_CloneOwnedModule(source,&g_execOwners[0],ZR_NULL);
    TEST_ASSERT_TRUE(ok);ZrTests_ExecOwner_Equal(&expected,ZrCore_ExecIr_OwnedModuleView(g_execOwners[0]));
    TEST_ASSERT_FALSE(ZrCore_ExecIr_CloneOwnedModule(ZR_NULL,&g_execOwners[0],ZR_NULL));
    ZrTests_ExecOwner_Equal(&expected,ZrCore_ExecIr_OwnedModuleView(g_execOwners[0]));
}
static void test_invalid_bounded_source_matches_direct_clone_refusal(void) {
    SZrExecIrModule *source=ZrTests_ExecOwner_Build(0u,9u,2u,0u);
    SZrExecIrDiagnostic direct,actual;TZrBool a,b;TZrUInt64 sourceBefore,oldBefore;
    TZrUInt32 saved=source->constantCapacity;SZrExecIrOwnedModule *old;
    const SZrExecIrModule *view;(void)owned(0u,ZrTests_ExecOwner_Build(1u,8u,1u,0u));
    old=g_execOwners[0];view=ZrCore_ExecIr_OwnedModuleView(old);
    sourceBefore=ZrTests_ExecOwner_LiveDigest(source);oldBefore=ZrTests_ExecOwner_LiveDigest(view);
    source->constantCapacity=0u;
    a=ZrCore_ExecIr_CloneModule(source,&g_ownerModules[2],&direct);
    b=ZrCore_ExecIr_CloneOwnedModule(source,&g_execOwners[0],&actual);
    source->constantCapacity=saved;ZrTests_ExecOwner_Disarm();
    TEST_ASSERT_FALSE(a);TEST_ASSERT_FALSE(b);ZrTests_ExecOwner_Diagnostic(&direct,&actual);
    TEST_ASSERT_EQUAL_PTR(old,g_execOwners[0]);TEST_ASSERT_EQUAL_UINT64(sourceBefore,ZrTests_ExecOwner_LiveDigest(source));
    TEST_ASSERT_EQUAL_UINT64(oldBefore,ZrTests_ExecOwner_LiveDigest(view));
}
static void fault_sweep(TZrBool selfView) {
    SZrExecIrModule *producer=ZrTests_ExecOwner_Build(0u,9u,2u,0u);
    SZrExecIrModule *prior=ZrTests_ExecOwner_Build(1u,8u,2u,0u);
    SZrOwnerContents expected;
    SZrOwnerHeapStats start,end;
    unsigned count,cloneCount,measureBase,cloneBase,oldFirst,oldLast;
    const SZrExecIrModule *source=producer;
    SZrExecIrDiagnostic d={0};TZrBool ok;
    ZrTests_ExecOwner_Capture(producer,&expected);
    start=ZrTests_ExecOwner_Stats();measureBase=start.attempts;
    (void)owned(2u,producer);end=ZrTests_ExecOwner_Stats();count=end.attempts-start.attempts;
    ZrCore_ExecIr_OwnedModuleFree(&g_execOwners[2]);ZrTests_ExecOwner_AssertReleased(start.records,end.records);
    start=ZrTests_ExecOwner_Stats();cloneBase=start.attempts;
    ok=ZrCore_ExecIr_CloneModule(producer,&g_ownerModules[2],&d);
    TEST_ASSERT_TRUE(ok);end=ZrTests_ExecOwner_Stats();cloneCount=end.attempts-start.attempts;
    ZrCore_ExecIr_FreeModule(&g_ownerModules[2]);ZrTests_ExecOwner_AssertReleased(start.records,end.records);
    TEST_ASSERT_TRUE(count > 1u);TEST_ASSERT_EQUAL_UINT(cloneCount+1u,count);
    TEST_ASSERT_EQUAL_UINT(2u,ZrTests_ExecOwner_Event(measureBase).kind);
    for(unsigned j=0u;j<cloneCount;++j){
        SZrOwnerHeapEvent a=ZrTests_ExecOwner_Event(measureBase+j+1u),b=ZrTests_ExecOwner_Event(cloneBase+j);
        TEST_ASSERT_EQUAL_UINT(a.kind,b.kind);TEST_ASSERT_EQUAL_UINT64(a.count,b.count);
        TEST_ASSERT_EQUAL_UINT64(a.width,b.width);TEST_ASSERT_EQUAL_UINT64(a.bytes,b.bytes);}
    oldFirst=ZrTests_ExecOwner_Stats().records;(void)owned(0u,selfView ? producer : prior);
    oldLast=ZrTests_ExecOwner_Stats().records;
    for(unsigned index=1u;index<=count;++index){
        SZrExecIrDiagnostic actual,witness={0};TZrBool failed,directFailed=ZR_FALSE;
        const SZrExecIrModule *oldView=ZrCore_ExecIr_OwnedModuleView(g_execOwners[0]);
        SZrExecIrOwnedModule *old=g_execOwners[0];TZrUInt64 sourceBefore,oldBefore;
        SZrOwnerHeapStats before,after;SZrOwnerHeapEvent event;
        source=selfView ? oldView : producer;
        sourceBefore=ZrTests_ExecOwner_LiveDigest(source);oldBefore=ZrTests_ExecOwner_LiveDigest(oldView);
        if(index>1u){
            before=ZrTests_ExecOwner_Stats();ZrTests_ExecOwner_Arm(index-1u);
            directFailed=ZrCore_ExecIr_CloneModule(source,&g_ownerModules[2],&witness);
            ZrTests_ExecOwner_Disarm();after=ZrTests_ExecOwner_Stats();
            /* Failed CloneModule must already release its private candidate. */
            ZrTests_ExecOwner_AssertReleased(before.records,after.records);
            ZrCore_ExecIr_FreeModule(&g_ownerModules[2]);
            TEST_ASSERT_FALSE(directFailed);TEST_ASSERT_EQUAL_UINT(index-1u,after.attempts-before.attempts);
            ZrTests_ExecOwner_AssertReleased(before.records,after.records);
        }else{witness.code=ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;witness.expectedVersion=1u;}
        before=ZrTests_ExecOwner_Stats();memset(&actual,0xA5,sizeof(actual));ZrTests_ExecOwner_Arm(index);
        failed=ZrCore_ExecIr_CloneOwnedModule(source,&g_execOwners[0],&actual);
        ZrTests_ExecOwner_Disarm();after=ZrTests_ExecOwner_Stats();
        TEST_ASSERT_FALSE(failed);TEST_ASSERT_EQUAL_UINT(index,after.attempts-before.attempts);
        event=ZrTests_ExecOwner_Event(after.attempts-1u);TEST_ASSERT_EQUAL_UINT(1u,event.failed);
        ZrTests_ExecOwner_AssertReleased(before.records,after.records);
        TEST_ASSERT_EQUAL_UINT(before.live,after.live);TEST_ASSERT_EQUAL_UINT(before.failures+1u,after.failures);
        TEST_ASSERT_EQUAL_UINT(0u,after.errors);TEST_ASSERT_EQUAL_PTR(old,g_execOwners[0]);
        ZrTests_ExecOwner_AssertLive(oldFirst,oldLast);
        if(!selfView)ZrTests_ExecOwner_AssertProducerLive(producer);
        TEST_ASSERT_EQUAL_UINT64(sourceBefore,ZrTests_ExecOwner_LiveDigest(source));
        TEST_ASSERT_EQUAL_UINT64(oldBefore,ZrTests_ExecOwner_LiveDigest(oldView));
        ZrTests_ExecOwner_Diagnostic(&witness,&actual);
        before=ZrTests_ExecOwner_Stats();
        ok=ZrCore_ExecIr_CloneOwnedModule(source,&g_execOwners[0],&d);ZrTests_ExecOwner_Disarm();
        /* No old/source-view pointer observation after retry publication. */
        TEST_ASSERT_TRUE(ok);zero_diagnostic(&d);after=ZrTests_ExecOwner_Stats();
        TEST_ASSERT_EQUAL_UINT(count,after.attempts-before.attempts);ZrTests_ExecOwner_AssertReleased(oldFirst,oldLast);
        oldFirst=before.records;oldLast=after.records;
        ZrTests_ExecOwner_Equal(&expected,ZrCore_ExecIr_OwnedModuleView(g_execOwners[0]));
    }
    ZrCore_ExecIr_OwnedModuleFree(&g_execOwners[0]);ZrTests_ExecOwner_AssertReleased(oldFirst,oldLast);
    fprintf(stderr,"EXEC_OWNER_FAULT_SWEEP self_view=%u measured=%u completed=%u clone=%u\n",
            (unsigned)selfView,count,count,cloneCount);
}
static void test_every_measured_owner_allocation_failure_preserves_old_owner(void) { fault_sweep(ZR_FALSE); }
static void test_every_measured_self_view_failure_preserves_old_owner(void) { fault_sweep(ZR_TRUE); }
static void test_repeated_clone_replace_free_balances_all_records(void) {
    SZrExecIrModule *a=ZrTests_ExecOwner_Build(0u,9u,2u,0u),*b=ZrTests_ExecOwner_Build(1u,8u,2u,0u);
    unsigned first=0u,last=0u;
    for(unsigned i=0u;i<8u;++i){
        unsigned nextFirst=ZrTests_ExecOwner_Stats().records;
        (void)owned(0u,(i&1u) ? b : a);
        if(i>0u)ZrTests_ExecOwner_AssertReleased(first,last);
        first=nextFirst;last=ZrTests_ExecOwner_Stats().records;
    }
    ZrCore_ExecIr_OwnedModuleFree(&g_execOwners[0]);TEST_ASSERT_NULL(g_execOwners[0]);
    ZrTests_ExecOwner_AssertReleased(first,last);
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_full_constructed_module_clone_prerequisite);
    RUN_TEST(test_empty_owned_module_and_null_view);
    RUN_TEST(test_owned_two_functions_preserve_local_value_ids);
    RUN_TEST(test_owned_view_survives_full_producer_teardown);
    RUN_TEST(test_present_empty_maps_preserve_header_identity);
    RUN_TEST(test_two_owned_modules_release_independently);
    RUN_TEST(test_successful_replacement_releases_prior_owner_once);
    RUN_TEST(test_malformed_gc_outer_after_prior_function_preserves_owner);
    RUN_TEST(test_old_view_self_source_replacement_is_atomic);
    RUN_TEST(test_null_arguments_preserve_existing_owner);
    RUN_TEST(test_null_diagnostic_supports_success_and_refusal);
    RUN_TEST(test_invalid_bounded_source_matches_direct_clone_refusal);
    RUN_TEST(test_every_measured_owner_allocation_failure_preserves_old_owner);
    RUN_TEST(test_every_measured_self_view_failure_preserves_old_owner);
    RUN_TEST(test_repeated_clone_replace_free_balances_all_records);
    return UNITY_END();
}
