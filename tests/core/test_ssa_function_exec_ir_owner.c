#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "unity.h"
#include "aot/aot_ir_owned_module.h"
#include "support/ssa_function_exec_ir_owner_fixture.h"
void setUp(void){ZrTests_FnExec_SetUp();}
void tearDown(void){ZrTests_FnExec_TearDown();}
static void diag_zero(const SZrExecIrDiagnostic *d){SZrExecIrDiagnostic zero={0};ZrTests_ExecOwner_Diagnostic(&zero,d);}
static SZrExecIrModule *model(unsigned slot,unsigned seed){return ZrTests_ExecOwner_Build(slot,seed,2u,0u);}
static void exec_released(unsigned first,unsigned last){ZrTests_ExecOwner_AssertReleased(first,last);}
static void aot_unchanged(SZrFunction *f,const SZrAotIrModule *view,const SZrTestsFunctionAotWitness *w,SZrFnHeapStats before){
    SZrFnHeapStats after=ZrTests_FnExec_AotStats();
    TEST_ASSERT_EQUAL_UINT(before.attempts,after.attempts);TEST_ASSERT_EQUAL_UINT(before.records,after.records);
    TEST_ASSERT_EQUAL_UINT(before.frees,after.frees);TEST_ASSERT_EQUAL_UINT(before.live,after.live);TEST_ASSERT_EQUAL_UINT(0u,after.errors);
    TEST_ASSERT_EQUAL_PTR(view,ZrCore_Function_GetAotIr(f));ZrTests_FunctionAot_AssertView(view,w);
}
static void test_typed_core_and_generic_owner_prerequisite(void){
    SZrExecIrModule *source=model(0u,9u);SZrOwnerContents expected;SZrExecIrDiagnostic d={0};
    SZrTestsFunctionAotProducer *a=ZrTests_FnExec_AotProducer(0u,9);SZrTestsFunctionAotWitness aw;SZrAotIrDiagnostic ad={0};
    ZrTests_ExecOwner_Capture(source,&expected);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_CloneOwnedModule(source,&g_execOwners[0],&d));diag_zero(&d);
    ZrTests_ExecOwner_Equal(&expected,ZrCore_ExecIr_OwnedModuleView(g_execOwners[0]));
    ZrTests_ExecOwner_Independent(source,ZrCore_ExecIr_OwnedModuleView(g_execOwners[0]));
    TEST_ASSERT_EQUAL_UINT32(1u,source->functions[0].values[0].id);TEST_ASSERT_EQUAL_UINT32(1u,source->functions[1].values[0].id);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK,ZrCore_AotIr_ValidateModule(&a->module,&ad));
    ZrTests_FunctionAotWitness_Capture(&a->module,&aw);ZrTests_FunctionAot_AssertView(&a->module,&aw);
}
static void test_new_function_has_no_ir_owners(void){
    SZrFunction *f=ZrTests_FnExec_New(0u);TEST_ASSERT_NULL(f->ownedExecIr);TEST_ASSERT_NULL(f->ownedAotIr);
    TEST_ASSERT_NULL(ZrCore_Function_GetExecIr(f));TEST_ASSERT_NULL(ZrCore_Function_GetExecIr(ZR_NULL));
}
static void test_null_new_and_actual_current_function_size(void){
    SZrFunction *f;SZrFnHeapStats vm,aot;SZrOwnerHeapStats exec;unsigned record;
    (void)ZrTests_FnExec_State();vm=ZrTests_FnExec_VmStats();aot=ZrTests_FnExec_AotStats();exec=ZrTests_ExecOwner_Stats();
    TEST_ASSERT_NULL(ZrCore_Function_New(ZR_NULL));TEST_ASSERT_EQUAL_UINT(vm.attempts,ZrTests_FnExec_VmStats().attempts);
    TEST_ASSERT_EQUAL_UINT(aot.attempts,ZrTests_FnExec_AotStats().attempts);TEST_ASSERT_EQUAL_UINT(exec.attempts,ZrTests_ExecOwner_Stats().attempts);
    f=ZrTests_FnExec_New(0u);record=ZrTests_FnExec_VmRecord(f);
    TEST_ASSERT_EQUAL_UINT64(sizeof(*f),ZrTests_FnExec_VmBytes(record));
    TEST_ASSERT_EQUAL_INT64(ZR_MEMORY_NATIVE_TYPE_OBJECT,ZrTests_FnExec_VmType(record));
    TEST_ASSERT_NULL(f->ownedExecIr);TEST_ASSERT_NULL(f->ownedAotIr);
    fprintf(stderr,"FUNCTION_EXEC_LAYOUT function_size=%llu execir_offset=%llu aot_offset=%llu\n",
            (unsigned long long)sizeof(*f),(unsigned long long)offsetof(SZrFunction,ownedExecIr),
            (unsigned long long)offsetof(SZrFunction,ownedAotIr));
}
static void test_attached_execir_survives_producer_release(void){
    SZrExecIrModule *source=model(0u,9u);SZrOwnerContents expected;SZrFunction *f=ZrTests_FnExec_New(0u);
    unsigned sourceLast=ZrTests_ExecOwner_Stats().records;ZrTests_ExecOwner_Capture(source,&expected);
    (void)ZrTests_FnExec_Attach(f,source);ZrCore_ExecIr_FreeModule(source);exec_released(0u,sourceLast);
    ZrTests_ExecOwner_Equal(&expected,ZrCore_Function_GetExecIr(f));
}
static void test_replace_execir_nine_to_eight_releases_old_once(void){
    SZrExecIrModule *a=model(0u,9u),*b=model(1u,8u);SZrFunction *f=ZrTests_FnExec_New(0u);
    unsigned first=ZrTests_ExecOwner_Stats().records,last;(void)ZrTests_FnExec_Attach(f,a);last=ZrTests_ExecOwner_Stats().records;
    (void)ZrTests_FnExec_Attach(f,b);exec_released(first,last);
}
static void test_exact_old_execir_view_replacement(void){
    SZrExecIrModule *source=model(0u,9u);SZrFunction *f=ZrTests_FnExec_New(0u);SZrOwnerContents expected;SZrExecIrDiagnostic d={0};TZrBool ok;
    unsigned first=ZrTests_ExecOwner_Stats().records,last;const SZrExecIrModule *view=ZrTests_FnExec_Attach(f,source);
    last=ZrTests_ExecOwner_Stats().records;ZrTests_ExecOwner_Capture(view,&expected);
    ok=ZrCore_Function_SnapshotExecIr(f,view,&d);ZrTests_ExecOwner_Disarm();
    TEST_ASSERT_TRUE_MESSAGE(ok,"actual Function ExecIR attachment must succeed");diag_zero(&d);exec_released(first,last);
    ZrTests_ExecOwner_Equal(&expected,ZrCore_Function_GetExecIr(f));
}
static void test_null_function_resets_all_diagnostic_fields(void){
    SZrExecIrModule *source=model(0u,9u);SZrExecIrDiagnostic d,expected={0};TZrBool ok;
    TZrUInt64 before=ZrTests_ExecOwner_LiveDigest(source);unsigned attempts=ZrTests_ExecOwner_Stats().attempts;
    memset(&d,0xA5,sizeof(d));ok=ZrCore_Function_SnapshotExecIr(ZR_NULL,source,&d);
    expected.code=ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;TEST_ASSERT_FALSE(ok);ZrTests_ExecOwner_Diagnostic(&expected,&d);
    TEST_ASSERT_EQUAL_UINT(attempts,ZrTests_ExecOwner_Stats().attempts);ZrTests_ExecOwner_AssertProducerLive(source);
    TEST_ASSERT_EQUAL_UINT64(before,ZrTests_ExecOwner_LiveDigest(source));
}
static void test_null_source_preserves_both_function_owners(void){
    SZrExecIrModule *source=model(0u,9u);SZrFunction *f=ZrTests_FnExec_New(0u);SZrExecIrDiagnostic d,expected={0};
    const SZrExecIrModule *view=ZrTests_FnExec_Attach(f,source);SZrTestsFunctionAotWitness w;const SZrAotIrModule *av;
    SZrFnHeapStats aot;TZrUInt64 before=ZrTests_ExecOwner_LiveDigest(view);unsigned attempts=ZrTests_ExecOwner_Stats().attempts,frees=ZrTests_ExecOwner_Stats().frees;
    ZrTests_FnExec_AttachAot(f,&ZrTests_FnExec_AotProducer(0u,9)->module);av=ZrCore_Function_GetAotIr(f);
    ZrTests_FunctionAotWitness_Capture(av,&w);aot=ZrTests_FnExec_AotStats();memset(&d,0xA5,sizeof(d));
    TEST_ASSERT_FALSE(ZrCore_Function_SnapshotExecIr(f,ZR_NULL,&d));expected.code=ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
    ZrTests_ExecOwner_Diagnostic(&expected,&d);TEST_ASSERT_EQUAL_UINT(frees,ZrTests_ExecOwner_Stats().frees);
    TEST_ASSERT_EQUAL_PTR(view,ZrCore_Function_GetExecIr(f));
    TEST_ASSERT_EQUAL_UINT64(before,ZrTests_ExecOwner_LiveDigest(view));TEST_ASSERT_EQUAL_UINT(attempts,ZrTests_ExecOwner_Stats().attempts);
    aot_unchanged(f,av,&w,aot);
}
static void test_null_diagnostic_success_and_refusal(void){
    SZrExecIrModule *source=model(0u,9u);SZrFunction *f=ZrTests_FnExec_New(0u);SZrOwnerContents expected;
    ZrTests_ExecOwner_Capture(source,&expected);
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_Function_SnapshotExecIr(f,source,ZR_NULL),"actual Function ExecIR attachment must succeed");
    ZrTests_ExecOwner_Equal(&expected,ZrCore_Function_GetExecIr(f));
    TEST_ASSERT_FALSE(ZrCore_Function_SnapshotExecIr(f,ZR_NULL,ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_Function_SnapshotExecIr(ZR_NULL,source,ZR_NULL));
    ZrTests_ExecOwner_Equal(&expected,ZrCore_Function_GetExecIr(f));
}
static void test_lower_refusal_matches_complete_owner_diagnostic(void){
    SZrExecIrModule *source=model(0u,8u),*prior=model(1u,9u);SZrFunction *f=ZrTests_FnExec_New(0u);
    unsigned oldFirst=ZrTests_ExecOwner_Stats().records,oldLast;
    const SZrExecIrModule *old=ZrTests_FnExec_Attach(f,prior);SZrExecIrDiagnostic direct,actual;TZrBool a,b;
    oldLast=ZrTests_ExecOwner_Stats().records;
    TZrUInt32 saved=source->constantCapacity;TZrUInt64 sourceBefore=ZrTests_ExecOwner_LiveDigest(source),oldBefore=ZrTests_ExecOwner_LiveDigest(old);
    SZrExecIrOwnedModule *owner=f->ownedExecIr;unsigned first=ZrTests_ExecOwner_Stats().records;
    source->constantCapacity=0u;a=ZrCore_ExecIr_CloneOwnedModule(source,&g_execOwners[0],&direct);
    b=ZrCore_Function_SnapshotExecIr(f,source,&actual);source->constantCapacity=saved;ZrTests_ExecOwner_Disarm();
    TEST_ASSERT_FALSE(a);TEST_ASSERT_FALSE(b);exec_released(first,ZrTests_ExecOwner_Stats().records);
    ZrTests_ExecOwner_Diagnostic(&direct,&actual);ZrTests_ExecOwner_AssertLive(oldFirst,oldLast);TEST_ASSERT_EQUAL_PTR(owner,f->ownedExecIr);
    ZrTests_ExecOwner_AssertProducerLive(source);TEST_ASSERT_EQUAL_UINT64(sourceBefore,ZrTests_ExecOwner_LiveDigest(source));
    TEST_ASSERT_EQUAL_UINT64(oldBefore,ZrTests_ExecOwner_LiveDigest(old));
}
static void allocation_sweep(TZrBool replacement){
    SZrExecIrModule *source=model(0u,8u),*prior=model(1u,9u);SZrFunction *probe=ZrTests_FnExec_New(0u),*f;
    SZrOwnerContents expected;SZrOwnerHeapStats before,after;unsigned count,oldFirst=0u,oldLast=0u;
    SZrTestsFunctionAotWitness aw;const SZrAotIrModule *av=ZR_NULL;SZrFnHeapStats astats={0};
    ZrTests_ExecOwner_Capture(source,&expected);before=ZrTests_ExecOwner_Stats();
    (void)ZrTests_FnExec_Attach(probe,source);after=ZrTests_ExecOwner_Stats();count=after.attempts-before.attempts;
    TEST_ASSERT_TRUE(count>1u);ZrTests_FnExec_Free(0u);exec_released(before.records,after.records);
    f=ZrTests_FnExec_New(1u);
    if(replacement){oldFirst=ZrTests_ExecOwner_Stats().records;(void)ZrTests_FnExec_Attach(f,prior);oldLast=ZrTests_ExecOwner_Stats().records;
        ZrTests_FnExec_AttachAot(f,&ZrTests_FnExec_AotProducer(0u,9)->module);av=ZrCore_Function_GetAotIr(f);
        ZrTests_FunctionAotWitness_Capture(av,&aw);astats=ZrTests_FnExec_AotStats();}
    for(unsigned index=1u;index<=count;++index){
        SZrExecIrDiagnostic direct,actual;TZrBool lower,ok;SZrExecIrOwnedModule *old=f->ownedExecIr;SZrOwnerHeapEvent directEvent,actualEvent;
        const SZrExecIrModule *oldView=ZrCore_Function_GetExecIr(f);TZrUInt64 oldDigest=0u,sourceDigest=ZrTests_ExecOwner_LiveDigest(source);
        if(replacement)oldDigest=ZrTests_ExecOwner_LiveDigest(oldView);
        before=ZrTests_ExecOwner_Stats();ZrTests_ExecOwner_Arm(index);
        lower=ZrCore_ExecIr_CloneOwnedModule(source,&g_execOwners[0],&direct);
        ZrTests_ExecOwner_Disarm();after=ZrTests_ExecOwner_Stats();
        exec_released(before.records,after.records);TEST_ASSERT_FALSE(lower);TEST_ASSERT_NULL(g_execOwners[0]);
        TEST_ASSERT_EQUAL_UINT(index,after.attempts-before.attempts);
        directEvent=ZrTests_ExecOwner_Event(after.attempts-1u);TEST_ASSERT_EQUAL_UINT(1u,directEvent.failed);
        before=ZrTests_ExecOwner_Stats();memset(&actual,0xA5,sizeof(actual));ZrTests_ExecOwner_Arm(index);
        ok=ZrCore_Function_SnapshotExecIr(f,source,&actual);ZrTests_ExecOwner_Disarm();after=ZrTests_ExecOwner_Stats();
        TEST_ASSERT_FALSE(ok);TEST_ASSERT_EQUAL_UINT(index,after.attempts-before.attempts);exec_released(before.records,after.records);
        actualEvent=ZrTests_ExecOwner_Event(after.attempts-1u);TEST_ASSERT_EQUAL_UINT(1u,actualEvent.failed);
        TEST_ASSERT_EQUAL_UINT(directEvent.kind,actualEvent.kind);TEST_ASSERT_EQUAL_UINT64(directEvent.count,actualEvent.count);
        TEST_ASSERT_EQUAL_UINT64(directEvent.width,actualEvent.width);TEST_ASSERT_EQUAL_UINT64(directEvent.bytes,actualEvent.bytes);
        TEST_ASSERT_EQUAL_UINT(before.live,after.live);TEST_ASSERT_EQUAL_UINT(before.failures+1u,after.failures);
        TEST_ASSERT_EQUAL_UINT(0u,after.errors);ZrTests_ExecOwner_Diagnostic(&direct,&actual);
        if(replacement)ZrTests_ExecOwner_AssertLive(oldFirst,oldLast);
        TEST_ASSERT_EQUAL_PTR(old,f->ownedExecIr);ZrTests_ExecOwner_AssertProducerLive(source);
        TEST_ASSERT_EQUAL_UINT64(sourceDigest,ZrTests_ExecOwner_LiveDigest(source));
        if(replacement){ZrTests_ExecOwner_AssertLive(oldFirst,oldLast);TEST_ASSERT_EQUAL_UINT64(oldDigest,ZrTests_ExecOwner_LiveDigest(oldView));aot_unchanged(f,av,&aw,astats);}
        else TEST_ASSERT_NULL(ZrCore_Function_GetExecIr(f));
        before=ZrTests_ExecOwner_Stats();(void)ZrTests_FnExec_Attach(f,source);after=ZrTests_ExecOwner_Stats();
        TEST_ASSERT_EQUAL_UINT(count,after.attempts-before.attempts);ZrTests_ExecOwner_Equal(&expected,ZrCore_Function_GetExecIr(f));
        if(replacement){exec_released(oldFirst,oldLast);oldFirst=before.records;oldLast=after.records;aot_unchanged(f,av,&aw,astats);}
        else {ZrTests_FnExec_Free(1u);exec_released(before.records,after.records);if(index<count)f=ZrTests_FnExec_New(1u);}
    }
    if(replacement){ZrTests_FnExec_Free(1u);exec_released(oldFirst,oldLast);}
    fprintf(stderr,"FUNCTION_EXEC_FAULT_SWEEP replacement=%u measured=%u completed=%u\n",(unsigned)replacement,count,count);
}
static void test_every_execir_attachment_allocation_fault_cleanup_retry(void){allocation_sweep(ZR_FALSE);}
static void test_every_execir_replacement_allocation_fault_preserves_aot(void){allocation_sweep(ZR_TRUE);}
static void test_execir_replacement_leaves_aot_owner_unchanged(void){
    SZrExecIrModule *a=model(0u,9u),*b=model(1u,8u);SZrFunction *f=ZrTests_FnExec_New(0u);SZrTestsFunctionAotWitness w;
    const SZrAotIrModule *av;SZrFnHeapStats astats;ZrTests_FnExec_AttachAot(f,&ZrTests_FnExec_AotProducer(0u,9)->module);
    av=ZrCore_Function_GetAotIr(f);ZrTests_FunctionAotWitness_Capture(av,&w);astats=ZrTests_FnExec_AotStats();
    (void)ZrTests_FnExec_Attach(f,a);aot_unchanged(f,av,&w,astats);(void)ZrTests_FnExec_Attach(f,b);aot_unchanged(f,av,&w,astats);
}
static void test_aot_replacement_leaves_execir_owner_unchanged(void){
    SZrFunction *f=ZrTests_FnExec_New(0u);const SZrExecIrModule *view=ZrTests_FnExec_Attach(f,model(0u,9u));
    TZrUInt64 digest=ZrTests_ExecOwner_LiveDigest(view);SZrOwnerHeapStats before=ZrTests_ExecOwner_Stats();unsigned record;
    record=ZrTests_FnExec_AttachAot(f,&ZrTests_FnExec_AotProducer(0u,9)->module);
    ZrTests_FnExec_AttachAot(f,&ZrTests_FnExec_AotProducer(1u,8)->module);
    TEST_ASSERT_EQUAL_UINT(1u,ZrTests_FnExec_AotFrees(record));TEST_ASSERT_EQUAL_UINT(before.frees,ZrTests_ExecOwner_Stats().frees);
    TEST_ASSERT_EQUAL_UINT(before.attempts,ZrTests_ExecOwner_Stats().attempts);
    TEST_ASSERT_EQUAL_UINT(before.records,ZrTests_ExecOwner_Stats().records);TEST_ASSERT_EQUAL_UINT(before.frees,ZrTests_ExecOwner_Stats().frees);
    TEST_ASSERT_EQUAL_PTR(view,ZrCore_Function_GetExecIr(f));TEST_ASSERT_EQUAL_UINT64(digest,ZrTests_ExecOwner_LiveDigest(view));
}
typedef struct SBothRecords {unsigned first,last,aot;} SBothRecords;
static SBothRecords attach_both(SZrFunction *f,const SZrExecIrModule *source,unsigned producerSlot,TZrInt64 literal){
    SBothRecords r;r.first=ZrTests_ExecOwner_Stats().records;(void)ZrTests_FnExec_Attach(f,source);
    r.last=ZrTests_ExecOwner_Stats().records;r.aot=ZrTests_FnExec_AttachAot(f,&ZrTests_FnExec_AotProducer(producerSlot,literal)->module);return r;
}
static void both_live(SBothRecords r){ZrTests_ExecOwner_AssertLive(r.first,r.last);TEST_ASSERT_EQUAL_UINT(0u,ZrTests_FnExec_AotFrees(r.aot));}
static void both_released(SBothRecords r){exec_released(r.first,r.last);TEST_ASSERT_EQUAL_UINT(1u,ZrTests_FnExec_AotFrees(r.aot));}
static void test_record_move_detaches_both_donor_owners(void){
    SZrFunction *parent=ZrTests_FnExec_New(0u),*donor=ZrTests_FnExec_New(1u),*child;
    SBothRecords r=attach_both(donor,model(0u,9u),0u,9);SZrExecIrOwnedModule *exec=donor->ownedExecIr;SZrAotIrOwnedModule *aot=donor->ownedAotIr;
    child=ZrTests_FnExec_Move(parent,donor);TEST_ASSERT_NULL(ZrCore_Function_GetExecIr(donor));TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(donor));
    TEST_ASSERT_EQUAL_PTR(exec,child->ownedExecIr);TEST_ASSERT_EQUAL_PTR(aot,child->ownedAotIr);
    ZrTests_FnExec_Free(1u);both_live(r);ZrTests_FnExec_Free(0u);both_released(r);
}
static void test_two_successive_owned_moves_release_only_final_destination(void){
    SZrFunction *root=ZrTests_FnExec_New(0u),*middle=ZrTests_FnExec_New(1u),*donor=ZrTests_FnExec_New(2u),*first,*last;
    SBothRecords r=attach_both(donor,model(0u,9u),0u,9);SZrExecIrOwnedModule *exec=donor->ownedExecIr;SZrAotIrOwnedModule *aot=donor->ownedAotIr;
    first=ZrTests_FnExec_Move(middle,donor);last=ZrTests_FnExec_Move(root,first);
    TEST_ASSERT_NULL(ZrCore_Function_GetExecIr(donor));TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(donor));
    TEST_ASSERT_NULL(ZrCore_Function_GetExecIr(first));TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(first));
    TEST_ASSERT_EQUAL_PTR(exec,last->ownedExecIr);TEST_ASSERT_EQUAL_PTR(aot,last->ownedAotIr);
    ZrTests_FnExec_Free(2u);ZrTests_FnExec_Free(1u);both_live(r);ZrTests_FnExec_Free(0u);both_released(r);
}
static void tree(SZrFunction **root,SZrFunction **child,SZrFunction **grand,SBothRecords r[3]){
    SZrExecIrModule *source=model(0u,9u);SZrFunction *c,*g;
    *root=ZrTests_FnExec_New(0u);c=ZrTests_FnExec_New(1u);g=ZrTests_FnExec_New(2u);
    r[0]=attach_both(*root,source,0u,9);r[1]=attach_both(c,source,1u,8);r[2]=attach_both(g,source,2u,7);
    (void)ZrTests_FnExec_Move(c,g);*child=ZrTests_FnExec_Move(*root,c);*grand=(*child)->childFunctionList;(*grand)->ownerFunction=*child;
    TEST_ASSERT_NULL(c->ownedExecIr);TEST_ASSERT_NULL(c->ownedAotIr);TEST_ASSERT_NULL(g->ownedExecIr);TEST_ASSERT_NULL(g->ownedAotIr);
}
static void test_owned_tree_releases_each_execir_and_aot_once(void){
    SZrFunction *root,*child,*grand;SBothRecords r[3];tree(&root,&child,&grand,r);
    TEST_ASSERT_NOT_NULL(ZrCore_Function_GetExecIr(root));TEST_ASSERT_NOT_NULL(ZrCore_Function_GetExecIr(child));TEST_ASSERT_NOT_NULL(ZrCore_Function_GetExecIr(grand));
    for(unsigned i=0u;i<3u;++i)both_live(r[i]);ZrTests_FnExec_Free(0u);
    for(unsigned i=0u;i<3u;++i)both_released(r[i]);
}
static void test_borrowed_shell_clears_both_aliases_preserves_descendants(void){
    SZrFunction *root,*child,*grand,*borrowed,*shell;SBothRecords r[3],br;unsigned storage;SZrOwnerContents ce,ge;
    SZrTestsFunctionAotWitness ca,ga;tree(&root,&child,&grand,r);borrowed=ZrTests_FnExec_New(3u);
    br=attach_both(borrowed,&g_ownerModules[0],3u,8);ZrTests_ExecOwner_Capture(ZrCore_Function_GetExecIr(child),&ce);
    ZrTests_ExecOwner_Capture(ZrCore_Function_GetExecIr(grand),&ge);ZrTests_FunctionAotWitness_Capture(ZrCore_Function_GetAotIr(child),&ca);
    ZrTests_FunctionAotWitness_Capture(ZrCore_Function_GetAotIr(grand),&ga);
    shell=ZrCore_Memory_RawMallocWithType(ZrTests_FnExec_Global(),sizeof(*shell),ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    borrowed->childFunctionList=shell;borrowed->childFunctionLength=shell!=ZR_NULL?1u:0u;borrowed->childFunctionGraphIsBorrowed=ZR_TRUE;
    if(shell!=ZR_NULL){*shell=*child;shell->ownerFunction=borrowed;
        ZrCore_Function_ForgetCopiedAotIrOwnerAliases(shell,1u);ZrCore_Function_ForgetCopiedExecIrOwnerAliases(shell,1u);}
    TEST_ASSERT_NOT_NULL(shell);storage=ZrTests_FnExec_VmRecord(shell);
    TEST_ASSERT_NULL(shell->ownedExecIr);TEST_ASSERT_NULL(shell->ownedAotIr);TEST_ASSERT_EQUAL_PTR(grand,shell->childFunctionList);
    ZrTests_FnExec_Free(3u);TEST_ASSERT_EQUAL_UINT(1u,ZrTests_FnExec_VmFrees(storage));both_released(br);
    for(unsigned i=0u;i<3u;++i)both_live(r[i]);ZrTests_ExecOwner_Equal(&ce,ZrCore_Function_GetExecIr(child));
    ZrTests_ExecOwner_Equal(&ge,ZrCore_Function_GetExecIr(grand));ZrTests_FunctionAot_AssertView(ZrCore_Function_GetAotIr(child),&ca);
    ZrTests_FunctionAot_AssertView(ZrCore_Function_GetAotIr(grand),&ga);ZrTests_FnExec_Free(0u);
    for(unsigned i=0u;i<3u;++i)both_released(r[i]);
}
static void test_detach_idempotence_and_alias_helpers_are_slot_specific(void){
    SZrFunction *root=ZrTests_FnExec_New(0u),*donor=ZrTests_FnExec_New(1u),*borrowed=ZrTests_FnExec_New(2u),*child,*shell;
    SBothRecords r=attach_both(donor,model(0u,9u),0u,9);SZrOwnerHeapStats exec;SZrFnHeapStats aot;TZrBool aotOnly,execOnly;
    child=ZrTests_FnExec_Move(root,donor);exec=ZrTests_ExecOwner_Stats();aot=ZrTests_FnExec_AotStats();
    ZrCore_Function_DetachOwnedBuffers(donor);ZrCore_Function_DetachOwnedBuffers(donor);
    shell=ZrCore_Memory_RawMallocWithType(ZrTests_FnExec_Global(),sizeof(*shell),ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    borrowed->childFunctionList=shell;borrowed->childFunctionLength=shell!=ZR_NULL?1u:0u;borrowed->childFunctionGraphIsBorrowed=ZR_TRUE;
    aotOnly=execOnly=ZR_FALSE;
    if(shell!=ZR_NULL){*shell=*child;shell->ownerFunction=borrowed;ZrCore_Function_ForgetCopiedAotIrOwnerAliases(shell,1u);
        aotOnly=(TZrBool)(shell->ownedAotIr==ZR_NULL&&shell->ownedExecIr==child->ownedExecIr);
        ZrCore_Function_ForgetCopiedExecIrOwnerAliases(shell,1u);
        *shell=*child;shell->ownerFunction=borrowed;ZrCore_Function_ForgetCopiedExecIrOwnerAliases(shell,1u);
        execOnly=(TZrBool)(shell->ownedExecIr==ZR_NULL&&shell->ownedAotIr==child->ownedAotIr);
        ZrCore_Function_ForgetCopiedAotIrOwnerAliases(shell,1u);}
    ZrCore_Function_ForgetCopiedAotIrOwnerAliases(ZR_NULL,1u);ZrCore_Function_ForgetCopiedExecIrOwnerAliases(ZR_NULL,1u);
    TEST_ASSERT_NOT_NULL(shell);TEST_ASSERT_TRUE(aotOnly);TEST_ASSERT_TRUE(execOnly);
    TEST_ASSERT_NULL(donor->ownedExecIr);TEST_ASSERT_NULL(donor->ownedAotIr);both_live(r);
    TEST_ASSERT_EQUAL_UINT(exec.attempts,ZrTests_ExecOwner_Stats().attempts);TEST_ASSERT_EQUAL_UINT(exec.frees,ZrTests_ExecOwner_Stats().frees);
    TEST_ASSERT_EQUAL_UINT(aot.attempts,ZrTests_FnExec_AotStats().attempts);TEST_ASSERT_EQUAL_UINT(aot.frees,ZrTests_FnExec_AotStats().frees);
}
static void test_explicit_function_free_then_actual_global_shutdown(void){
    SZrExecIrModule *source=model(0u,9u);SZrFunction *a=ZrTests_FnExec_New(0u),*b=ZrTests_FnExec_New(1u);
    unsigned va=ZrTests_FnExec_VmRecord(a),vb=ZrTests_FnExec_VmRecord(b);SBothRecords ar=attach_both(a,source,0u,9),br=attach_both(b,source,1u,8);
    ZrTests_FnExec_Free(0u);both_released(ar);both_live(br);ZrTests_FnExec_Shutdown();
    both_released(ar);both_released(br);TEST_ASSERT_EQUAL_UINT(1u,ZrTests_FnExec_VmFrees(va));TEST_ASSERT_EQUAL_UINT(1u,ZrTests_FnExec_VmFrees(vb));
    TEST_ASSERT_EQUAL_UINT(0u,ZrTests_FnExec_VmStats().live);TEST_ASSERT_EQUAL_UINT(0u,ZrTests_FnExec_AotStats().live);
}
int main(void){
    UNITY_BEGIN();
    RUN_TEST(test_typed_core_and_generic_owner_prerequisite);
    RUN_TEST(test_new_function_has_no_ir_owners);
    RUN_TEST(test_null_new_and_actual_current_function_size);
    RUN_TEST(test_attached_execir_survives_producer_release);
    RUN_TEST(test_replace_execir_nine_to_eight_releases_old_once);
    RUN_TEST(test_exact_old_execir_view_replacement);
    RUN_TEST(test_null_function_resets_all_diagnostic_fields);
    RUN_TEST(test_null_source_preserves_both_function_owners);
    RUN_TEST(test_null_diagnostic_success_and_refusal);
    RUN_TEST(test_lower_refusal_matches_complete_owner_diagnostic);
    RUN_TEST(test_every_execir_attachment_allocation_fault_cleanup_retry);
    RUN_TEST(test_every_execir_replacement_allocation_fault_preserves_aot);
    RUN_TEST(test_execir_replacement_leaves_aot_owner_unchanged);
    RUN_TEST(test_aot_replacement_leaves_execir_owner_unchanged);
    RUN_TEST(test_record_move_detaches_both_donor_owners);
    RUN_TEST(test_two_successive_owned_moves_release_only_final_destination);
    RUN_TEST(test_owned_tree_releases_each_execir_and_aot_once);
    RUN_TEST(test_borrowed_shell_clears_both_aliases_preserves_descendants);
    RUN_TEST(test_detach_idempotence_and_alias_helpers_are_slot_specific);
    RUN_TEST(test_explicit_function_free_then_actual_global_shutdown);
    return UNITY_END();
}
