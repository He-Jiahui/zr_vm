#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "ssa_sccp_pooled_int64_fixture.h"
#include "ssa_exec_ir_owned_module_shim.h"
enum { PI_TRACE_LIMIT = 2048 };
static SZrOwnerHeapEvent g_trace[PI_TRACE_LIMIT],g_prefix[PI_TRACE_LIMIT];
static unsigned g_trace_count,g_prefix_count,g_remark_site;
static int pi_result_zero(const SZrExecIrOptimizationResult *r) {
    return !r->functionsVisited&&!r->functionsChanged&&!r->passesRun&&!r->budgetExhausted&&!r->beforeHash&&!r->afterHash;
}
static int pi_remark_prefix(const SPiFixture *f) {
    const SZrExecIrOptimizationRemark *p=f->remarks.items;
    return f->remarks.count==1u&&p&&p->sourceId==777u&&p->reasonCode==123u&&!p->pass&&
        p->outcome==ZR_EXEC_IR_REMARK_SUCCESS&&!p->beforeHash&&!p->afterHash&&!p->elapsedTicks&&!p->verifierTicks&&!p->verifierChecks;
}
static int pi_old_releases(const SPiSnapshot *s,unsigned *replaced) {
    unsigned i,freed;*replaced=0u;
    for(i=0u;i<s->ownerCount;++i) {
        if(s->owners[i].record!=UINT32_MAX) {
            if(!ZrTests_ExecOwner_TryRecordFrees(s->owners[i].record,&freed)||freed>1u)return 0;
            if(freed==1u)++*replaced;
        }
    }
    return 1;
}
static int pi_copy_trace(unsigned begin,SZrOwnerHeapEvent *out,unsigned *count) {
    SZrOwnerHeapStats st=ZrTests_ExecOwner_Stats();unsigned i;
    if(st.attempts<begin||st.attempts-begin>PI_TRACE_LIMIT)return 0;
    *count=st.attempts-begin;
    for(i=0u;i<*count;++i)if(!ZrTests_ExecOwner_TryEvent(begin+i,&out[i]))return 0;
    return 1;
}
/* Independently measured actual Optimize's Verify+Clone prefix, not a guessed ordinal. */
static int pi_measure_prefix(void) {
    SPiFixture f;SZrExecIrModule clone;unsigned begin;int good=1;
    ZrCore_ExecIr_ModuleInit(&clone);
    if(!Pi_Build(&f,49u,0u)||!Pi_PrepareRemarks(&f)||!Pi_Verify(&f)){good=0;goto done;}
    begin=ZrTests_ExecOwner_Stats().attempts;
    good=ZrCore_ExecIr_VerifyModule(&f.module,&f.diagnostic)&&ZrCore_ExecIr_CloneModule(&f.module,&clone,&f.diagnostic);
    if(good)good=pi_copy_trace(begin,g_prefix,&g_prefix_count);
 done:ZrCore_ExecIr_FreeModule(&clone);good&=Pi_Dispose(&f);ZrTests_ExecOwner_Reset();return good;
}
static SPiResult pi_measure(void) {
    SPiFixture f;SPiResult r={1u,1u};unsigned begin,i;SZrOwnerLiveSpan sink;
    if(!Pi_Build(&f,49u,0u)||!Pi_PrepareRemarks(&f)||!Pi_Verify(&f)){r.safety=0u;goto done;}
    if(!ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,f.remarks.items,1u,sizeof(*f.remarks.items),&sink)){r.safety=0u;goto done;}
    begin=ZrTests_ExecOwner_Stats().attempts;
    if(!Pi_Optimize(&f,1u)||!Pi_CurrentLive(&f)){r.safety=0u;goto done;}
    if(!pi_copy_trace(begin,g_trace,&g_trace_count)||!g_trace_count){r.safety=0u;goto done;}
    r.feature=(unsigned)Pi_CheckTargets(&f);r.safety&=(unsigned)(Pi_Verify(&f)&&Pi_CheckPrefix(&f)&&Pi_NoOrphans(&f));
    g_remark_site=0u;
    for(i=0u;i<g_trace_count;++i)if(g_trace[i].kind==3u&&g_trace[i].record==sink.record&&
        g_trace[i].bytes==2u*sizeof(*f.remarks.items)&&!g_remark_site)g_remark_site=i+1u;
    if(g_prefix_count>g_trace_count){r.safety=0u;goto done;}
    for(i=0u;i<g_prefix_count;++i)if(g_prefix[i].kind!=g_trace[i].kind||g_prefix[i].count!=g_trace[i].count||
            g_prefix[i].width!=g_trace[i].width||g_prefix[i].bytes!=g_trace[i].bytes)r.safety=0u;
    printf("POOLED_INT64_TRACE measured=%u prefix=%u remark_site=%u healthy_fold=%u\n",g_trace_count,g_prefix_count,g_remark_site,r.feature);
 done:r.safety&=(unsigned)Pi_Dispose(&f);ZrTests_ExecOwner_Reset();return r;
}
static SPiResult pi_site(unsigned caseid,unsigned site,unsigned *late,unsigned *remark) {
    SPiFixture f;SPiSnapshot old;SPiResult r={1u,1u};SZrOwnerHeapStats st;
    SZrOwnerHeapEvent event={0};SZrOwnerLiveSpan sink;unsigned start=0u,owner=0u,content=0u,zero=0u,prefix=0u,replaced=0u;
    unsigned j,trace_ok=1u,triggered=0u,retry=0u,logical_retry=0u,token=0u,kind=0u,released=0u;int ok=1;
    *late=*remark=0u;
    if(!Pi_Build(&f,49u,0u)||!Pi_PrepareRemarks(&f)||!Pi_Verify(&f)||!Pi_Snapshot(&f,&old)){r.safety=0u;goto done;}
    if(!ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,f.remarks.items,1u,sizeof(*f.remarks.items),&sink)){r.safety=0u;goto done;}
    start=ZrTests_ExecOwner_Stats().attempts;
    if(!ZrTests_ExecOwner_TryArm(site)){r.safety=0u;goto done;}
    ok=Pi_Optimize(&f,1u);ZrTests_ExecOwner_Disarm();
    if(ZrTests_ExecOwner_TryEvent(start+site-1u,&event))triggered=event.failed;
    for(j=0u;j<site;++j){SZrOwnerHeapEvent e;
        if(!ZrTests_ExecOwner_TryEvent(start+j,&e)||e.kind!=g_trace[j].kind||e.count!=g_trace[j].count||e.width!=g_trace[j].width||e.bytes!=g_trace[j].bytes)trace_ok=0u;
        if(j+1u<site&&trace_ok&&e.record!=g_trace[j].record)trace_ok=0u;
    }
    kind=(unsigned)f.diagnostic.code;token=f.diagnostic.functionToken;
    owner=(unsigned)Pi_OwnersEqual(&f,&old);
    if(Pi_CurrentLive(&f)) {
        content=(unsigned)(Pi_Hash(&f)==old.hash);
        prefix=(unsigned)pi_remark_prefix(&f);zero=(unsigned)pi_result_zero(&f.result);
        r.safety&=(unsigned)Pi_Verify(&f);
    }else r.safety=0u;
    r.safety&=(unsigned)(triggered&&trace_ok&&!ok&&kind==ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY&&content&&prefix&&zero&&pi_old_releases(&old,&replaced));
    /* A known baseline replacement is feature RED only; stale bytes are never read. */
    if(!owner&&!replaced)r.safety=0u;
    r.feature&=owner;
    if(site>g_prefix_count&&token==ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF,91u))*late=1u;
    if(site==g_remark_site&&event.kind==3u&&event.record==sink.record&&
        event.bytes==2u*sizeof(*f.remarks.items)&&token==0u)*remark=1u;
    if(r.safety){logical_retry=(unsigned)(Pi_Optimize(&f,1u)&&Pi_CurrentLive(&f)&&Pi_Verify(&f)&&Pi_CheckPrefix(&f)&&Pi_NoOrphans(&f));
        retry=logical_retry?(unsigned)Pi_CheckTargets(&f):0u;r.safety&=logical_retry;r.feature&=retry;}
 done:
    released=(unsigned)Pi_Dispose(&f);r.safety&=released;st=ZrTests_ExecOwner_Stats();r.safety&=(unsigned)(st.failures==1u);
    Pi_NoteFault(1u,r.safety?1u:0u);
    printf("POOLED_INT64_FAULT id=%u site=%u measured=%u triggered=%u optimize=%d owner=%u content=%u remarks=%u resultzero=%u replaced=%u retry=%u retry_valid=%u released=%u late=%u remark=%u code=%u token=%u live=%u errors=%u safety=%u\n",
        caseid,site,g_trace_count,triggered,ok,owner,content,prefix,zero,replaced,retry,logical_retry,released,*late,*remark,kind,token,st.live,st.errors,r.safety);
    ZrTests_ExecOwner_Reset();return r;
}
static SPiResult pi_fault_case(unsigned id) {
    SPiResult r={1u,1u},m;unsigned site,late=0u,remark=0u,found=0u;
    if(!pi_measure_prefix()){r.safety=0u;Pi_Profile(id,"trace_prefix",r);return r;}
    m=pi_measure();r.safety&=m.safety;r.feature&=m.feature;
    if(!r.safety){Pi_Profile(id,"trace_measure",r);return r;}
    for(site=1u;site<=g_trace_count;++site){
        if(id==50u&&site<=g_prefix_count)continue;
        if(id==51u&&site!=g_remark_site)continue;
        m=pi_site(id,site,&late,&remark);r.safety&=m.safety;r.feature&=m.feature;
        if(id==49u|| (id==50u&&late) || (id==51u&&remark)){++found;if(id!=49u)break;}
    }
    r.safety&=(unsigned)(found>0u);
    Pi_Profile(id,id==49u?"all_actual_allocations":id==50u?"after_clone_function2":"sink_record_growth",r);return r;
}
static SPiResult pi_budget(unsigned id) {
    SPiFixture f;SPiResult r={1u,1u},q;SZrExecIrPassBudget budget={0};
    const SZrExecIrPassInfo *passes;unsigned count;TZrUInt64 measured=0u,b,upper;
    if(!Pi_Build(&f,1u,0u)||!Pi_Verify(&f)){r.safety=0u;goto measured_done;}
    passes=ZrParser_ExecIr_GetScalarPasses(&count);
    if(!passes||!count){r.safety=0u;goto measured_done;}
    f.context.cache=&f.cache;f.context.budget=&budget;f.context.constants=f.module.constants;f.context.constantCount=f.module.constantCount;
    r.safety=(unsigned)(ZrParser_ExecIr_RunPassPipeline(f.module.functions,passes,1u,&f.context,&f.diagnostic)&&Pi_Verify(&f));
    measured=f.context.workUsed;r.safety&=(unsigned)(measured>0u&&measured<=4096u);
 measured_done:r.safety&=(unsigned)Pi_Dispose(&f);ZrTests_ExecOwner_Reset();
    if(!r.safety){Pi_Profile(id,"public_budget_measure",r);return r;}
    /* Explicit finite public sweep, not claimed exhaustive public workUsed. */
    upper=id==48u?1u:measured+32u;
    for(b=1u;b<=upper;++b){unsigned appended=0u,exhausted=0u,valid=0u,orphan=0u;int ok=0;
        q=(SPiResult){1u,1u};budget.maxWork=id==48u?0u:b;budget.maxInstructions=id==48u?1u:0u;
        if(!Pi_Build(&f,id==48u?48u:1u,0u)||!Pi_Verify(&f)){q.safety=0u;goto budget_done;}
        f.options.budget=&budget;ok=Pi_Optimize(&f,1u);valid=(unsigned)(Pi_CurrentLive(&f)&&Pi_Verify(&f));
        orphan=(unsigned)!Pi_NoOrphans(&f);appended=f.module.constantCount-f.initialCount;exhausted=f.result.budgetExhausted;
        q.safety=(unsigned)(ok&&valid&&!orphan&&Pi_CheckPrefix(&f));
        if(id==48u)q.safety&=exhausted;
        if(b==upper&&id==47u)q.feature=(unsigned)Pi_CheckTargets(&f);
 budget_done:q.safety&=(unsigned)Pi_Dispose(&f);r.safety&=q.safety;r.feature&=q.feature;
        printf("POOLED_INT64_BUDGET id=%u maxwork=%" PRIu64 " maxinstructions=%" PRIu64 " measured_existing=%" PRIu64 " public_upper=%" PRIu64 " optimize=%d exhausted=%u verify=%u appended=%u unreferenced_new=%u safety=%u\n",
                id,budget.maxWork,budget.maxInstructions,measured,upper,ok,exhausted,valid,appended,orphan,q.safety);
        ZrTests_ExecOwner_Reset();
    }
    Pi_Profile(id,id==47u?"finite_public_work_sweep":"maxinstructions",r);return r;
}
/* These private calls own an independent mutable candidate. FALSE discards it;
 * only public Optimize's fault matrix claims original-owner rollback. */
static int pi_owner_call(SPiFixture *f,unsigned entry,unsigned writable) {
    TZrBool changed=ZR_TRUE;TZrUInt32 count;const SZrExecIrPassInfo *passes;
    if(entry)return ZrParser_ExecIr_RunModuleSccpPass(&f->module,f->module.functions,
            &f->context,&changed,writable?&f->diagnostic:NULL);
    passes=ZrParser_ExecIr_GetScalarPasses(&count);
    if(!passes||!count||passes[0].run!=ZrParser_ExecIr_RunSccpPass)return 0;
    return ZrParser_ExecIr_RunModulePassPipeline(&f->module,f->module.functions,
            passes,1u,&f->context,writable?&f->diagnostic:NULL);
}
static SPiResult pi_owner_once(unsigned entry,unsigned writable,TZrUInt64 limit,
                               TZrUInt64 measured,TZrUInt64 *observed) {
    SPiFixture f;SPiResult r={1u,1u};SZrExecIrPassBudget budget={0};SZrOwnerHeapStats stats;
    unsigned char oldDiagnostic[sizeof(f.diagnostic)];unsigned pre=0u,valid=0u,prefix=0u,view=0u;
    unsigned orphan=0u,partial=0u,matched=0u,appended=0u,exhausted=0u,nullUnchanged=0u,targets=0u;
    TZrUInt64 work=0u;int ok=0;
    *observed=0u;
    /* Two equal ADDs force one append and one exact reuse in this same run. */
    if(!Pi_Build(&f,15u,0u)||!Pi_CurrentLive(&f)){r.safety=0u;goto done;}
    targets=f.targetCount;if(targets!=2u){r.safety=0u;goto done;}
    pre=(unsigned)Pi_Verify(&f);if(!pre){r.safety=0u;goto done;}
    budget.maxWork=limit;f.context.budget=&budget;f.context.cache=&f.cache;
    f.context.constants=f.module.constants;f.context.constantCount=f.module.constantCount;
    f.context.scratchArena=&f.initial;
    memset(&f.diagnostic,0xA5,sizeof(f.diagnostic));memcpy(oldDiagnostic,&f.diagnostic,sizeof(oldDiagnostic));
    ok=pi_owner_call(&f,entry,writable);
    work=f.context.workUsed;*observed=work;exhausted=f.context.budgetExhausted;
    nullUnchanged=writable||(memcmp(oldDiagnostic,&f.diagnostic,sizeof(oldDiagnostic))==0);
    /* A failed private candidate has no rollback promise. Do not inspect its
     * pool or call it a budget partial-success; discard it at done. */
    if(!ok){r.safety=0u;goto done;}
    valid=(unsigned)(Pi_CurrentLive(&f)&&Pi_Verify(&f));
    if(!valid){r.safety=0u;goto done;}
    prefix=(unsigned)Pi_CheckPrefix(&f);orphan=(unsigned)!Pi_NoOrphans(&f);
    partial=(unsigned)Pi_CheckPartialTargets(&f);matched=(unsigned)Pi_CheckTargets(&f);
    view=(unsigned)(f.context.constants==f.module.constants&&f.context.constantCount==f.module.constantCount&&
            f.context.scratchArena==&f.initial);
    if(f.module.constantCount<f.initialCount){r.safety=0u;goto done;}
    appended=f.module.constantCount-f.initialCount;
    r.safety=(unsigned)(prefix&&!orphan&&view&&nullUnchanged&&appended<=1u);
    r.feature=partial;
    if(!limit){
        r.feature&=(unsigned)(work>0u&&work<=4096u&&!exhausted&&matched&&appended==1u);
    }else{
        r.safety&=(unsigned)(work<=limit);
        r.feature&=(unsigned)(measured>0u&&limit<=measured&&
                (limit<measured?exhausted:(!exhausted&&matched&&appended==1u)));
    }
 done:
    r.safety&=(unsigned)Pi_Dispose(&f);stats=ZrTests_ExecOwner_Stats();
    if(!limit){
        printf("POOLED_INT64_OWNER_MEASURE id=47 entry=%u writable=%u maxwork=0 measured=%" PRIu64 " optimize=%d preverify=%u verify=%u targets=%u matched=%u appended=%u prefix=%u view=%u null_diag_unchanged=%u exhausted=%u live=%u errors=%u safety=%u feature=%u\n",
                entry,writable,work,ok,pre,valid,targets,matched,appended,prefix,view,nullUnchanged,exhausted,stats.live,stats.errors,r.safety,r.feature);
    }else{
        printf("POOLED_INT64_OWNER_BUDGET id=47 entry=%u writable=%u limit=%" PRIu64 " measured=%" PRIu64 " work=%" PRIu64 " optimize=%d exhausted=%u preverify=%u verify=%u targets=%u appended=%u unreferenced_new=%u partial=%u matched=%u view=%u prefix=%u null_diag_unchanged=%u live=%u errors=%u safety=%u feature=%u\n",
                entry,writable,limit,measured,work,ok,exhausted,pre,valid,targets,appended,orphan,partial,matched,view,prefix,nullUnchanged,stats.live,stats.errors,r.safety,r.feature);
    }
    ZrTests_ExecOwner_Reset();return r;
}
static SPiResult pi_owner_budget(unsigned entry) {
    SPiResult all={1u,1u},r;unsigned writable;TZrUInt64 measured,actual,limit;
    /* Each entry and diagnostic mode gets its own measured W and every fresh
     * candidate cutoff 1..W. Zero means unlimited measurement, never a cutoff. */
    for(writable=0u;writable<2u;++writable){
        r=pi_owner_once(entry,writable,0u,0u,&measured);all.safety&=r.safety;all.feature&=r.feature;
        if(!r.safety||!r.feature)continue;
        for(limit=1u;limit<=measured;++limit){
            r=pi_owner_once(entry,writable,limit,measured,&actual);
            all.safety&=r.safety;all.feature&=r.feature;
        }
    }
    return all;
}
static SPiResult pi_invalid(unsigned profile,unsigned writable) {
    SPiFixture f;SPiResult r={1u,1u};SPiSnapshot old,healthy;unsigned saved=0u,initial=0u,rejected=0u,owner=0u,zero=0u,restored=0u;
    int ok=1;SZrExecIrInstruction *in;unsigned v,source=0u;EZrExecutionDiagnosticCode code;
    if(!Pi_Build(&f,1u,0u)){r.safety=0u;goto done;}
    initial=(unsigned)Pi_Verify(&f);
    if(!initial||!Pi_Snapshot(&f,&healthy)){r.safety=0u;goto done;}
    in=&f.module.functions[0].instructions[0];source=in->sourceId;v=f.module.functions[0].results[in->results.start];
    if(profile==0u){saved=in->layoutId;in->layoutId=f.module.constantCount;}
    else if(profile==1u){saved=f.module.functions[0].values[v-1u].typeToken;f.module.functions[0].values[v-1u].typeToken=ZR_VALUE_TYPE_UINT64;}
    else {saved=in->typeToken;in->typeToken=ZR_VALUE_TYPE_UINT64;}
    /* Snapshot the readable deliberately invalid candidate, not the healthy parent. */
    if(!Pi_Snapshot(&f,&old)){r.safety=0u;goto restore;}
    rejected=(unsigned)!Pi_Verify(&f);code=f.diagnostic.code;
    r.safety&=(unsigned)(rejected&&code==(profile?ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE:ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    ok=Pi_Optimize(&f,writable);owner=(unsigned)Pi_OwnersEqual(&f,&old);zero=(unsigned)pi_result_zero(&f.result);
    r.safety&=(unsigned)(!ok&&owner&&zero&&Pi_CurrentLive(&f)&&Pi_Hash(&f)==old.hash);
    if(writable)r.safety&=(unsigned)(f.diagnostic.code==code&&f.diagnostic.functionToken==f.module.functions[0].functionToken&&
            f.diagnostic.instructionId==1u&&f.diagnostic.sourceId==source&&f.diagnostic.blockId==1u&&
            f.diagnostic.expectedVersion==(profile==0u?f.module.constantCount:profile==1u?ZR_VALUE_TYPE_UINT64:ZR_VALUE_TYPE_INT64)&&
            f.diagnostic.actualVersion==(profile==0u?f.module.constantCount:profile==1u?ZR_VALUE_TYPE_INT64:ZR_VALUE_TYPE_UINT64)&&
            !f.diagnostic.expectedHash&&!f.diagnostic.actualHash);
 restore:
    if(!Pi_CurrentLive(&f)){r.safety=0u;goto done;}
    in=&f.module.functions[0].instructions[0];
    if(profile==0u)in->layoutId=saved;else if(profile==1u)f.module.functions[0].values[v-1u].typeToken=saved;else in->typeToken=saved;
    restored=(unsigned)(Pi_CurrentLive(&f)&&Pi_Hash(&f)==healthy.hash&&Pi_OwnersEqual(&f,&healthy)&&Pi_Verify(&f)&&Pi_Optimize(&f,writable)&&Pi_CurrentLive(&f)&&Pi_Verify(&f));r.safety&=restored;
 done:r.safety&=(unsigned)Pi_Dispose(&f);
    printf("POOLED_INT64_REJECT id=52 profile=%u writable=%u healthy_verify=%u rejected_verify=%u optimize=%d owner=%u resultzero=%u restored_retry=%u safety=%u\n",
            profile,writable,initial,rejected,ok,owner,zero,restored,r.safety);
    return r;
}
SPiResult Pi_Faults(unsigned id) {
    SPiResult r={1u,1u},q;unsigned p,w;
    if(id==47u){
        r=pi_budget(id);
        for(p=0u;p<2u;++p){q=pi_owner_budget(p);
            Pi_Profile(id,p?"private_sccp_work_sweep":"private_pipeline_work_sweep",q);
            r.safety&=q.safety;r.feature&=q.feature;}
        return r;
    }
    if(id==48u)return pi_budget(id);
    if(id>=49u&&id<=51u)return pi_fault_case(id);
    if(id==52u){const char *names[]={"pool_index_oob","pooled_result_type_mismatch","pooled_instruction_type_mismatch"};
        for(p=0u;p<3u;++p)for(w=0u;w<2u;++w){q=pi_invalid(p,w);Pi_Profile(id,names[p],q);r.safety&=q.safety;r.feature&=q.feature;ZrTests_ExecOwner_Reset();}
    }else {
        for(p=0u;p<2u;++p){SPiFixture f;SPiSnapshot old;unsigned replaced=0u,owners=0u,i;q=(SPiResult){1u,1u};
            if(!Pi_Build(&f,53u,0u)||!Pi_Verify(&f)||!Pi_Snapshot(&f,&old)){q.safety=0u;goto lifetime_done;}
            q.safety=(unsigned)(Pi_Optimize(&f,p)&&Pi_CurrentLive(&f)&&Pi_Verify(&f)&&Pi_CheckPrefix(&f)&&Pi_NoOrphans(&f));
            q.feature=q.safety?(unsigned)Pi_CheckTargets(&f):0u;
            q.safety&=(unsigned)pi_old_releases(&old,&replaced);
            for(i=0u;i<old.ownerCount;++i)if(old.owners[i].record!=UINT32_MAX)++owners;
            q.feature&=(unsigned)(replaced==owners);
 lifetime_done:q.safety&=(unsigned)Pi_Dispose(&f);Pi_Profile(id,p?"rebuilt_success":"first_success",q);r.safety&=q.safety;r.feature&=q.feature;ZrTests_ExecOwner_Reset();
        }
    }return r;
}
