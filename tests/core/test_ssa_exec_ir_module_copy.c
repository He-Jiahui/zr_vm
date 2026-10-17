#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "support/ssa_exec_ir_module_copy_fixture.h"
typedef struct SBefore {SZrCopyState source,replacement,output;SZrOwnerHeapStats heap;} SBefore;
void setUp(void){ZrTests_ModuleCopy_SetUp();}
void tearDown(void){ZrTests_ModuleCopy_TearDown();}
static SZrExecIrModule *source(void){return ZrTests_ModuleCopy_Source();}
static SZrExecIrFunction *replacement(void){return ZrTests_ModuleCopy_Replacement();}
static void feature(void){ZrTests_ModuleCopy_Require(source(),1u,replacement(),0u);}
static void before(SBefore *b){ZrTests_ModuleCopy_InputState(0u,&b->source);ZrTests_ModuleCopy_InputState(1u,&b->replacement);
    ZrTests_ModuleCopy_OutputState(0u,&b->output);b->heap=ZrTests_ExecOwner_Stats();}
static void failed(const SBefore *b,TZrBool ok,const SZrExecIrDiagnostic *actual,const SZrExecIrDiagnostic *expected){
    TEST_ASSERT_FALSE(ok);ZrTests_ExecOwner_Diagnostic(expected,actual);
    ZrTests_ModuleCopy_Unchanged(&b->source,source());ZrTests_ModuleCopy_Unchanged(&b->replacement,&g_ownerModules[1]);
    ZrTests_ModuleCopy_Unchanged(&b->output,&g_copyOutputs[0]);
    ZrTests_ExecOwner_AssertReleased(b->heap.records,ZrTests_ExecOwner_Stats().records);
    TEST_ASSERT_EQUAL_UINT(b->heap.live,ZrTests_ExecOwner_Stats().live);TEST_ASSERT_EQUAL_UINT(0u,ZrTests_ExecOwner_Stats().errors);
}
static SZrExecIrDiagnostic diag32(EZrExecutionDiagnosticCode code,TZrUInt32 expected,TZrUInt32 actual){
    SZrExecIrDiagnostic d={0};d.code=code;d.functionToken=source()->functions[0].functionToken;
    d.expectedVersion=expected;d.actualVersion=actual;return d;}
static SZrExecIrDiagnostic diag64(EZrExecutionDiagnosticCode code,TZrUInt64 expected,TZrUInt64 actual){
    SZrExecIrDiagnostic d={0};d.code=code;d.functionToken=source()->functions[0].functionToken;
    d.expectedHash=expected;d.actualHash=actual;return d;}
static void test_null_arguments_reset_diagnostic_preserve_output(void){
    for(unsigned mask=1u;mask<8u;++mask){SBefore b;SZrExecIrDiagnostic d,e={0};TZrBool ok;before(&b);memset(&d,0xA5,sizeof(d));
        ok=ZrCore_ExecIr_CloneModuleReplacingFunction((mask&1u)?ZR_NULL:source(),1u,
                (mask&2u)?ZR_NULL:replacement(),(mask&4u)?ZR_NULL:&g_copyOutputs[0],&d);ZrTests_ExecOwner_Disarm();
        e.code=ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;failed(&b,ok,&d,&e);
        TEST_ASSERT_EQUAL_UINT(b.heap.attempts,ZrTests_ExecOwner_Stats().attempts);}
}
static void test_null_diagnostic_supports_success_and_refusal(void){SBefore b;SZrCopyState old;SZrOwnerHeapStats start,end;TZrBool ok;
    feature();ZrTests_ModuleCopy_OutputState(0u,&old);start=ZrTests_ExecOwner_Stats();
    ok=ZrCore_ExecIr_CloneModuleReplacingFunction(source(),1u,replacement(),&g_copyOutputs[0],ZR_NULL);ZrTests_ExecOwner_Disarm();end=ZrTests_ExecOwner_Stats();
    TEST_ASSERT_TRUE_MESSAGE(ok,"actual whole-module selected-function copy must succeed");ZrTests_ModuleCopy_Published(source(),0u,&old,&old.contents);
    ZrTests_ModuleCopy_SetOutputRange(0u,start.records,end.records);before(&b);
    TEST_ASSERT_FALSE(ZrCore_ExecIr_CloneModuleReplacingFunction(ZR_NULL,1u,replacement(),&g_copyOutputs[0],ZR_NULL));
    ZrTests_ModuleCopy_Unchanged(&b.output,&g_copyOutputs[0]);TEST_ASSERT_EQUAL_UINT(b.heap.attempts,ZrTests_ExecOwner_Stats().attempts);
}
static void test_invalid_selected_range_precedes_source_storage(void){
    const TZrUInt32 ids[2]={0u,3u};for(unsigned i=0u;i<2u;++i){SBefore b;SZrExecIrDiagnostic d,e={0};TZrUInt32 cap;TZrBool ok;
        before(&b);cap=source()->constantCapacity;source()->constantCapacity=0u;memset(&d,0xA5,sizeof(d));
        ok=ZrCore_ExecIr_CloneModuleReplacingFunction(source(),ids[i],replacement(),&g_copyOutputs[0],&d);
        source()->constantCapacity=cap;ZrTests_ExecOwner_Disarm();e.code=ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
        e.expectedVersion=source()->functionCount;e.actualVersion=ids[i];failed(&b,ok,&d,&e);
        TEST_ASSERT_EQUAL_UINT(b.heap.attempts,ZrTests_ExecOwner_Stats().attempts);}
}
static void test_source_destination_identity_is_refused(void){SBefore b;SZrExecIrDiagnostic d,e={0};TZrBool ok;before(&b);memset(&d,0xA5,sizeof(d));
    ok=ZrCore_ExecIr_CloneModuleReplacingFunction(source(),1u,replacement(),source(),&d);ZrTests_ExecOwner_Disarm();
    e.code=ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;failed(&b,ok,&d,&e);TEST_ASSERT_EQUAL_UINT(b.heap.attempts,ZrTests_ExecOwner_Stats().attempts);}
static void test_selected_copy_preserves_module_and_other_function(void){feature();
    TEST_ASSERT_EQUAL_UINT32(2u,g_copyOutputs[0].functionCount);
    TEST_ASSERT_EQUAL_UINT32(replacement()->values[0].flags,g_copyOutputs[0].functions[0].values[0].flags);
    TEST_ASSERT_EQUAL_UINT32(source()->functions[1].values[0].flags,g_copyOutputs[0].functions[1].values[0].flags);
    TEST_ASSERT_EQUAL_UINT64(source()->constants[0].bits,g_copyOutputs[0].constants[0].bits);}
static void test_all_reachable_side_arrays_are_independent(void){feature();
    ZrTests_ExecOwner_Independent(source(),&g_copyOutputs[0]);ZrTests_ExecOwner_Independent(&g_ownerModules[1],&g_copyOutputs[0]);
    TEST_ASSERT_NOT_NULL(g_copyOutputs[0].functions[0].gcMap);TEST_ASSERT_NOT_NULL(g_copyOutputs[0].functions[0].stateMap);
    TEST_ASSERT_EQUAL_UINT32(1u,g_copyOutputs[0].functions[0].gcMapCount);
    TEST_ASSERT_EQUAL_UINT32(1u,g_copyOutputs[0].functions[0].gcMapCapacity);
    TEST_ASSERT_EQUAL_UINT32(2u,g_copyOutputs[0].functions[0].gcMap->entryCount);
    TEST_ASSERT_NOT_NULL(g_copyOutputs[0].functions[0].frameLayout);TEST_ASSERT_EQUAL_UINT32(1u,g_copyOutputs[0].functions[0].bindingRowCount);}
static void test_present_empty_headers_preserve_clone_capacity_policy(void){ZrTests_ModuleCopy_Inputs(1u);feature();
    const SZrExecIrFunction *f=&g_copyOutputs[0].functions[0];TEST_ASSERT_NOT_NULL(f->gcMap);TEST_ASSERT_NOT_NULL(f->stateMap);TEST_ASSERT_NOT_NULL(f->frameLayout);
    TEST_ASSERT_EQUAL_UINT32(0u,f->gcMap->entryCount);TEST_ASSERT_EQUAL_UINT32(0u,f->stateMap->entryCount);TEST_ASSERT_EQUAL_UINT32(0u,f->frameLayout->slotCount);
    TEST_ASSERT_EQUAL_UINT32(1u,f->gcMapCount);TEST_ASSERT_EQUAL_UINT32(1u,f->gcMapCapacity);
    TEST_ASSERT_EQUAL_UINT32(f->stateMap->entryCount,f->stateMap->entryCapacity);
    TEST_ASSERT_TRUE(f->valueCapacity>=f->valueCount);TEST_ASSERT_TRUE(f->instructionCapacity>=f->instructionCount);}
static void test_replacement_from_selected_source_slot(void){ZrTests_ModuleCopy_Require(source(),1u,&source()->functions[0],0u);}
static void test_absent_gc_maps_survive_module_replacement(void){ZrTests_ModuleCopy_Inputs(2u);feature();
    for(unsigned i=0u;i<g_copyOutputs[0].functionCount;++i){const SZrExecIrFunction *f=&g_copyOutputs[0].functions[i];
        TEST_ASSERT_NULL(f->gcMap);TEST_ASSERT_EQUAL_UINT32(0u,f->gcMapCount);TEST_ASSERT_EQUAL_UINT32(0u,f->gcMapCapacity);}}
static void test_malformed_gc_outer_replacement_cleans_candidate(void){SBefore b;SZrExecIrDiagnostic d,e={0};TZrBool ok;
    feature();before(&b);replacement()->gcMapCount=replacement()->gcMapCapacity=0u;memset(&d,0xA5,sizeof(d));
    ok=ZrCore_ExecIr_CloneModuleReplacingFunction(source(),1u,replacement(),&g_copyOutputs[0],&d);
    ZrTests_ExecOwner_Disarm();replacement()->gcMapCount=replacement()->gcMapCapacity=1u;
    e.code=ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;e.functionToken=replacement()->functionToken;
    /* failed() checks refusal before reading the potentially replaced output. */
    failed(&b,ok,&d,&e);feature();}
static void test_replacement_borrowed_from_old_output(void){SZrExecIrDiagnostic d;unsigned first=ZrTests_ExecOwner_Stats().records;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_CloneModule(source(),&g_copyOutputs[0],&d));ZrTests_ModuleCopy_ZeroDiagnostic(&d);
    ZrTests_ModuleCopy_SetOutputRange(0u,first,ZrTests_ExecOwner_Stats().records);g_copyOutputs[0].functions[0].values[0].flags^=4u;
    ZrTests_ModuleCopy_Require(source(),1u,&g_copyOutputs[0].functions[0],0u);
    TEST_ASSERT_EQUAL_UINT32(4u,g_copyOutputs[0].functions[0].values[0].flags);}
static void test_slot_and_function_identity_first_errors(void){feature();
    for(unsigned which=0u;which<4u;++which){SBefore b;SZrExecIrDiagnostic d,e;TZrBool ok;
        TZrUInt32 sourceId=source()->functions[0].id,rid=replacement()->id,token=replacement()->functionToken;TZrUInt64 sig=replacement()->signatureHash;
        before(&b);memset(&d,0xA5,sizeof(d));
        if(which==0u){source()->functions[0].id=2u;replacement()->id=2u;replacement()->functionToken^=1u;
            e=diag32(ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,1u,2u);}
        else if(which==1u){replacement()->id=2u;replacement()->functionToken^=1u;e=diag32(ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,1u,2u);}
        else if(which==2u){replacement()->functionToken^=1u;replacement()->signatureHash^=1u;e=diag32(ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,source()->functions[0].functionToken,replacement()->functionToken);}
        else {replacement()->signatureHash^=1u;e=diag64(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,source()->functions[0].signatureHash,replacement()->signatureHash);}
        ok=ZrCore_ExecIr_CloneModuleReplacingFunction(source(),1u,replacement(),&g_copyOutputs[0],&d);
        source()->functions[0].id=sourceId;replacement()->id=rid;replacement()->functionToken=token;replacement()->signatureHash=sig;
        ZrTests_ExecOwner_Disarm();failed(&b,ok,&d,&e);}
}
static TZrUInt64 contract_value(const SZrExecutionContract *c,unsigned field){switch(field){
    case 0:return c->schemaVersion;case 1:return c->abiVersion;case 2:return c->logicalVersion;case 3:return c->reserved0;
    case 4:return c->reserved1;case 5:return c->generation;case 6:return c->targetToken;case 7:return c->signatureHash;
    case 8:return c->moduleHash;case 9:return c->requiredCapabilities;default:return c->declaredEffects;}}
static void contract_set(SZrExecutionContract *c,unsigned field,TZrUInt64 value){switch(field){
    case 0:c->schemaVersion=(TZrUInt32)value;break;case 1:c->abiVersion=(TZrUInt32)value;break;case 2:c->logicalVersion=(TZrUInt32)value;break;
    case 3:c->reserved0=(TZrUInt32)value;break;case 4:c->reserved1=(TZrUInt32)value;break;case 5:c->generation=value;break;
    case 6:c->targetToken=(TZrUInt32)value;break;case 7:c->signatureHash=value;break;case 8:c->moduleHash=value;break;
    case 9:c->requiredCapabilities=(TZrUInt32)value;break;default:c->declaredEffects=(TZrUInt32)value;break;}}
static void test_all_immutable_contract_first_errors(void){
    const EZrExecutionDiagnosticCode codes[11]={ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
        ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
        ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION,ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
        ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH,ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH};
    feature();for(unsigned field=0u;field<11u;++field)for(unsigned simultaneous=0u;simultaneous<2u;++simultaneous){
        SBefore b;SZrExecIrDiagnostic d,e;SZrExecutionContract saved=replacement()->contract;TZrBool ok;
        TZrUInt64 expected=contract_value(&source()->functions[0].contract,field),actual=expected^1u;before(&b);
        contract_set(&replacement()->contract,field,actual);
        if(simultaneous)for(unsigned later=field+1u;later<11u;++later)contract_set(&replacement()->contract,later,contract_value(&saved,later)^1u);
        e=(field==5u||field==7u||field==8u)?diag64(codes[field],expected,actual):diag32(codes[field],(TZrUInt32)expected,(TZrUInt32)actual);
        memset(&d,0xA5,sizeof(d));ok=ZrCore_ExecIr_CloneModuleReplacingFunction(source(),1u,replacement(),&g_copyOutputs[0],&d);
        replacement()->contract=saved;ZrTests_ExecOwner_Disarm();failed(&b,ok,&d,&e);}
}
static void test_changed_function_frame_hash_preserves_module_hash(void){TZrUInt64 moduleHash=source()->contract.layoutHash;
    replacement()->contract.layoutHash^=1u;replacement()->frameLayout->layoutHash=replacement()->contract.layoutHash;feature();
    TEST_ASSERT_EQUAL_UINT64(replacement()->contract.layoutHash,g_copyOutputs[0].functions[0].contract.layoutHash);
    TEST_ASSERT_EQUAL_UINT64(replacement()->frameLayout->layoutHash,g_copyOutputs[0].functions[0].frameLayout->layoutHash);
    TEST_ASSERT_EQUAL_UINT64(moduleHash,g_copyOutputs[0].contract.layoutHash);}
static void test_equal_unknown_contract_values_copy_without_admission(void){
    SZrExecutionContract *a=&source()->functions[0].contract,*b=&replacement()->contract;
    a->schemaVersion=b->schemaVersion=77u;a->abiVersion=b->abiVersion=78u;a->logicalVersion=b->logicalVersion=79u;
    a->reserved0=b->reserved0=11u;a->reserved1=b->reserved1=12u;
    a->requiredCapabilities=b->requiredCapabilities=UINT32_C(0x80000000);a->declaredEffects=b->declaredEffects=UINT32_C(0x80000000);
    feature();TEST_ASSERT_EQUAL_UINT32(77u,g_copyOutputs[0].functions[0].contract.schemaVersion);
    TEST_ASSERT_EQUAL_UINT32(UINT32_C(0x80000000),g_copyOutputs[0].functions[0].contract.requiredCapabilities);}
static void test_source_storage_failure_matches_module_clone(void){feature();SBefore b;SZrExecIrDiagnostic d,direct;TZrBool lower,ok;TZrUInt32 cap;
    before(&b);cap=source()->constantCapacity;source()->constantCapacity=0u;
    lower=ZrCore_ExecIr_CloneModule(source(),&g_copyOutputs[3],&direct);ZrTests_ExecOwner_Disarm();
    replacement()->id=2u;ok=ZrCore_ExecIr_CloneModuleReplacingFunction(source(),1u,replacement(),&g_copyOutputs[0],&d);
    source()->constantCapacity=cap;replacement()->id=1u;ZrTests_ExecOwner_Disarm();TEST_ASSERT_FALSE(lower);failed(&b,ok,&d,&direct);}
static void test_replacement_storage_failure_matches_function_clone(void){feature();
    for(unsigned kind=0u;kind<3u;++kind){SBefore b;SZrExecIrDiagnostic d,direct;TZrBool lower,ok;TZrUInt32 saved;
        before(&b);if(kind==0u){saved=replacement()->valueCapacity;replacement()->valueCapacity=0u;}
        else if(kind==1u){saved=replacement()->stateMap->entryCapacity;replacement()->stateMap->entryCapacity=0u;}
        else {saved=replacement()->bindingRows[0].rowIndex;replacement()->bindingRows[0].rowIndex=1u;}
        lower=ZrCore_ExecIr_CloneFunction(replacement(),&g_copyFunctions[0],&direct);ZrTests_ExecOwner_Disarm();
        ok=ZrCore_ExecIr_CloneModuleReplacingFunction(source(),1u,replacement(),&g_copyOutputs[0],&d);
        if(kind==0u)replacement()->valueCapacity=saved;else if(kind==1u)replacement()->stateMap->entryCapacity=saved;
        else replacement()->bindingRows[0].rowIndex=saved;ZrTests_ExecOwner_Disarm();TEST_ASSERT_FALSE(lower);failed(&b,ok,&d,&direct);
    }
    {SBefore b;SZrExecIrDiagnostic d,e;TZrBool ok;TZrUInt32 cap=replacement()->valueCapacity,token=replacement()->functionToken;
        before(&b);replacement()->valueCapacity=0u;replacement()->functionToken^=1u;
        e=diag32(ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,source()->functions[0].functionToken,replacement()->functionToken);
        ok=ZrCore_ExecIr_CloneModuleReplacingFunction(source(),1u,replacement(),&g_copyOutputs[0],&d);
        replacement()->valueCapacity=cap;replacement()->functionToken=token;ZrTests_ExecOwner_Disarm();failed(&b,ok,&d,&e);}
}
static void test_output_survives_both_producer_releases(void){SZrCopyState output;feature();ZrTests_ModuleCopy_OutputState(0u,&output);
    ZrCore_ExecIr_FreeModule(source());ZrCore_ExecIr_FreeModule(&g_ownerModules[1]);ZrTests_ModuleCopy_Unchanged(&output,&g_copyOutputs[0]);}
static void test_two_outputs_own_independent_storage(void){SZrCopyState a,b;feature();ZrTests_ModuleCopy_OutputState(0u,&a);
    ZrTests_ModuleCopy_Require(source(),1u,replacement(),1u);ZrTests_ModuleCopy_OutputState(1u,&b);
    ZrTests_ExecOwner_Independent(&g_copyOutputs[0],&g_copyOutputs[1]);ZrCore_ExecIr_FreeModule(&g_copyOutputs[0]);
    ZrTests_ModuleCopy_Released(&a);ZrTests_ModuleCopy_Unchanged(&b,&g_copyOutputs[1]);}
static void test_repeated_replacement_and_idempotent_free(void){feature();for(unsigned i=0u;i<3u;++i){SZrCopyState old;
    ZrTests_ModuleCopy_OutputState(0u,&old);replacement()->values[0].flags^=2u;feature();ZrTests_ModuleCopy_Released(&old);}
    SZrCopyState last;ZrTests_ModuleCopy_OutputState(0u,&last);ZrCore_ExecIr_FreeModule(&g_copyOutputs[0]);ZrTests_ModuleCopy_Released(&last);
    unsigned frees=ZrTests_ExecOwner_Stats().frees;ZrCore_ExecIr_FreeModule(&g_copyOutputs[0]);TEST_ASSERT_EQUAL_UINT(frees,ZrTests_ExecOwner_Stats().frees);}
static void test_every_measured_composite_allocation_fault_cleanup_retry(void){
    unsigned moduleCount,functionCount,count;SZrOwnerHeapStats start,end;SZrExecIrDiagnostic d;SZrOwnerContents expected;SZrCopyState old;
    feature();start=ZrTests_ExecOwner_Stats();TEST_ASSERT_TRUE(ZrCore_ExecIr_CloneModule(source(),&g_copyOutputs[3],&d));
    moduleCount=ZrTests_ExecOwner_Stats().attempts-start.attempts;ZrTests_ModuleCopy_ZeroDiagnostic(&d);ZrCore_ExecIr_FreeModule(&g_copyOutputs[3]);
    start=ZrTests_ExecOwner_Stats();TEST_ASSERT_TRUE(ZrCore_ExecIr_CloneFunction(replacement(),&g_copyFunctions[0],&d));
    functionCount=ZrTests_ExecOwner_Stats().attempts-start.attempts;ZrTests_ModuleCopy_ZeroDiagnostic(&d);ZrCore_ExecIr_FreeFunction(&g_copyFunctions[0]);
    ZrTests_ModuleCopy_Expected(source(),1u,replacement(),&expected);ZrCore_ExecIr_FreeModule(&g_copyOutputs[2]);
    ZrTests_ModuleCopy_OutputState(0u,&old);start=ZrTests_ExecOwner_Stats();
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_CloneModuleReplacingFunction(source(),1u,replacement(),&g_copyOutputs[0],&d),"actual whole-module selected-function copy must succeed");
    end=ZrTests_ExecOwner_Stats();count=end.attempts-start.attempts;ZrTests_ModuleCopy_ZeroDiagnostic(&d);ZrTests_ModuleCopy_Published(source(),0u,&old,&expected);
    ZrTests_ModuleCopy_SetOutputRange(0u,start.records,end.records);TEST_ASSERT_EQUAL_UINT(moduleCount+functionCount,count);TEST_ASSERT_TRUE(count>moduleCount&&moduleCount>0u);
    {TZrUInt64 worst=3u*(TZrUInt64)count*count;TEST_ASSERT_TRUE(end.records<=32768u&&end.attempts<=65536u);
        TEST_ASSERT_TRUE(worst<=32768u-end.records);TEST_ASSERT_TRUE(worst<=65536u-end.attempts);}
    for(unsigned index=1u;index<=count;++index){SBefore b;SZrExecIrDiagnostic direct,actual;SZrOwnerHeapEvent de,ae;TZrBool lower,ok;SZrOwnerHeapStats directStart,directEnd;
        before(&b);directStart=ZrTests_ExecOwner_Stats();
        if(index<=moduleCount){ZrTests_ExecOwner_Arm(index);lower=ZrCore_ExecIr_CloneModule(source(),&g_copyOutputs[3],&direct);ZrTests_ExecOwner_Disarm();}
        else {TEST_ASSERT_TRUE(ZrCore_ExecIr_CloneModule(source(),&g_copyOutputs[3],&d));ZrTests_ModuleCopy_ZeroDiagnostic(&d);
            ZrTests_ExecOwner_Arm(index-moduleCount);lower=ZrCore_ExecIr_CloneFunction(replacement(),&g_copyOutputs[3].functions[0],&direct);ZrTests_ExecOwner_Disarm();}
        directEnd=ZrTests_ExecOwner_Stats();de=ZrTests_ExecOwner_Event(directEnd.attempts-1u);
        ZrCore_ExecIr_FreeModule(&g_copyOutputs[3]);ZrTests_ExecOwner_AssertReleased(directStart.records,directEnd.records);TEST_ASSERT_FALSE(lower);
        TEST_ASSERT_EQUAL_UINT(index,directEnd.attempts-directStart.attempts);TEST_ASSERT_EQUAL_UINT(1u,de.failed);
        b.heap=ZrTests_ExecOwner_Stats();memset(&actual,0xA5,sizeof(actual));ZrTests_ExecOwner_Arm(index);
        ok=ZrCore_ExecIr_CloneModuleReplacingFunction(source(),1u,replacement(),&g_copyOutputs[0],&actual);ZrTests_ExecOwner_Disarm();
        end=ZrTests_ExecOwner_Stats();ae=ZrTests_ExecOwner_Event(end.attempts-1u);failed(&b,ok,&actual,&direct);
        TEST_ASSERT_EQUAL_UINT(index,end.attempts-b.heap.attempts);TEST_ASSERT_EQUAL_UINT(1u,ae.failed);
        TEST_ASSERT_EQUAL_UINT(de.kind,ae.kind);TEST_ASSERT_EQUAL_UINT64(de.count,ae.count);TEST_ASSERT_EQUAL_UINT64(de.width,ae.width);TEST_ASSERT_EQUAL_UINT64(de.bytes,ae.bytes);
        start=ZrTests_ExecOwner_Stats();ok=ZrCore_ExecIr_CloneModuleReplacingFunction(source(),1u,replacement(),&g_copyOutputs[0],&d);ZrTests_ExecOwner_Disarm();end=ZrTests_ExecOwner_Stats();
        TEST_ASSERT_TRUE_MESSAGE(ok,"actual whole-module selected-function copy must succeed");ZrTests_ModuleCopy_ZeroDiagnostic(&d);ZrTests_ModuleCopy_Published(source(),0u,&b.output,&expected);
        ZrTests_ModuleCopy_Unchanged(&b.source,source());ZrTests_ModuleCopy_Unchanged(&b.replacement,&g_ownerModules[1]);
        TEST_ASSERT_EQUAL_UINT(count,end.attempts-start.attempts);ZrTests_ModuleCopy_SetOutputRange(0u,start.records,end.records);
    }
    fprintf(stderr,"MODULE_COPY_FAULT_SWEEP module=%u replacement=%u composite=%u completed=%u\n",moduleCount,functionCount,count,count);
}
static void test_empty_and_populated_destination_cleanup_balances(void){SZrCopyState old;ZrTests_ModuleCopy_OutputState(0u,&old);
    feature();ZrTests_ModuleCopy_Released(&old);ZrTests_ModuleCopy_Require(source(),1u,replacement(),1u);
    for(unsigned i=0u;i<2u;++i){SZrCopyState output;ZrTests_ModuleCopy_OutputState(i,&output);ZrCore_ExecIr_FreeModule(&g_copyOutputs[i]);ZrTests_ModuleCopy_Released(&output);}
    for(unsigned i=0u;i<ZR_OWNER_MODULE_SLOTS;++i)ZrCore_ExecIr_FreeModule(&g_ownerModules[i]);ZrTests_ExecOwner_AssertBalanced();}
int main(void){UNITY_BEGIN();
    RUN_TEST(test_null_arguments_reset_diagnostic_preserve_output);
    RUN_TEST(test_null_diagnostic_supports_success_and_refusal);
    RUN_TEST(test_invalid_selected_range_precedes_source_storage);
    RUN_TEST(test_source_destination_identity_is_refused);
    RUN_TEST(test_selected_copy_preserves_module_and_other_function);
    RUN_TEST(test_all_reachable_side_arrays_are_independent);
    RUN_TEST(test_present_empty_headers_preserve_clone_capacity_policy);
    RUN_TEST(test_absent_gc_maps_survive_module_replacement);
    RUN_TEST(test_malformed_gc_outer_replacement_cleans_candidate);
    RUN_TEST(test_replacement_from_selected_source_slot);
    RUN_TEST(test_replacement_borrowed_from_old_output);
    RUN_TEST(test_slot_and_function_identity_first_errors);
    RUN_TEST(test_all_immutable_contract_first_errors);
    RUN_TEST(test_changed_function_frame_hash_preserves_module_hash);
    RUN_TEST(test_equal_unknown_contract_values_copy_without_admission);
    RUN_TEST(test_source_storage_failure_matches_module_clone);
    RUN_TEST(test_replacement_storage_failure_matches_function_clone);
    RUN_TEST(test_output_survives_both_producer_releases);
    RUN_TEST(test_two_outputs_own_independent_storage);
    RUN_TEST(test_repeated_replacement_and_idempotent_free);
    RUN_TEST(test_every_measured_composite_allocation_fault_cleanup_retry);
    RUN_TEST(test_empty_and_populated_destination_cleanup_balances);
    return UNITY_END();}
