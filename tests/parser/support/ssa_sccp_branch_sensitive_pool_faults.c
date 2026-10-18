#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "ssa_sccp_branch_sensitive_pool_fixture.h"
#include "ssa_exec_ir_owned_module_shim.h"
static SZrOwnerHeapEvent g_trace[BE_TRACE_LIMIT],g_prefix[BE_TRACE_LIMIT];
static unsigned g_count,g_prefixCount,g_remarkSite;
static int be_zero(const SZrExecIrOptimizationResult *r) {
    return !r->functionsVisited&&!r->functionsChanged&&!r->passesRun&&!r->budgetExhausted&&!r->beforeHash&&!r->afterHash;
}
static int be_remarks(SBeFixture *f) {
    f->remarks.items=calloc(1u,sizeof(*f->remarks.items));if(!f->remarks.items)return 0;
    f->remarks.count=f->remarks.capacity=1u;f->remarks.items[0].sourceId=777u;f->remarks.items[0].reasonCode=123u;
    f->options.remarks=&f->remarks;return 1;
}
static int be_remark_preserved(const SBeFixture *f) {
    const SZrExecIrOptimizationRemark *r=f->remarks.items;
    return f->remarks.count==1u&&r&&r->sourceId==777u&&r->reasonCode==123u&&!r->pass&&
        r->outcome==ZR_EXEC_IR_REMARK_SUCCESS&&!r->beforeHash&&!r->afterHash&&!r->elapsedTicks&&!r->verifierTicks&&!r->verifierChecks;
}
static int be_trace(unsigned begin,SZrOwnerHeapEvent *events,unsigned *count) {
    unsigned i,end=ZrTests_ExecOwner_Stats().attempts;if(end<begin||end-begin>BE_TRACE_LIMIT)return 0;*count=end-begin;
    for(i=0;i<*count;++i)if(!ZrTests_ExecOwner_TryEvent(begin+i,events+i))return 0;return 1;
}
/* Match actual Verify+Clone, separately from Optimize, to classify a late
 * function failure. No guessed ordinal becomes an accepted OOM witness. */
static int be_prefix(unsigned id) {
    SBeFixture f;SZrExecIrModule clone;unsigned start;int good=1;
    ZrCore_ExecIr_ModuleInit(&clone);
    if(!Be_Build(&f,id,0u)||!be_remarks(&f)||!Be_Verify(&f)){good=0;goto done;}
    start=ZrTests_ExecOwner_Stats().attempts;
    good=ZrCore_ExecIr_VerifyModule(&f.module,&f.diagnostic)&&ZrCore_ExecIr_CloneModule(&f.module,&clone,&f.diagnostic);
    if(good)good=be_trace(start,g_prefix,&g_prefixCount)&&g_prefixCount>0u;
 done:ZrCore_ExecIr_FreeModule(&clone);good&=Be_Dispose(&f);ZrTests_ExecOwner_Reset();return good;
}
static SBeResult be_measure(unsigned id,unsigned writable) {
    SBeFixture f;SBeResult r={1u,1u};SZrOwnerLiveSpan sink;unsigned begin=0u,i;
    g_count=g_remarkSite=0u;
    if(!Be_Build(&f,id,0u)||!be_remarks(&f)||!Be_Verify(&f)||
       !ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,f.remarks.items,1u,sizeof(*f.remarks.items),&sink)){r.safety=0u;goto done;}
    begin=ZrTests_ExecOwner_Stats().attempts;
    if(!Be_Optimize(&f,writable)||!be_trace(begin,g_trace,&g_count)||!g_count){r.safety=0u;goto done;}
    if(g_prefixCount>g_count){r.safety=0u;goto done;}
    for(i=0;i<g_prefixCount;++i)if(g_prefix[i].kind!=g_trace[i].kind||g_prefix[i].count!=g_trace[i].count||
            g_prefix[i].width!=g_trace[i].width||g_prefix[i].bytes!=g_trace[i].bytes)r.safety=0u;
    for(i=0;i<g_count;++i)if(g_trace[i].kind==3u&&g_trace[i].record==sink.record&&
        g_trace[i].bytes==2u*sizeof(*f.remarks.items)&&!g_remarkSite)g_remarkSite=i+1u;
    r.safety&=(unsigned)Be_Check(&f,0u);r.feature=r.safety?(unsigned)Be_Check(&f,1u):0u;
 done:r.safety&=(unsigned)Be_Dispose(&f);
    printf("BOOL_EDGE_TRACE id=%u writable=%u measured=%u prefix=%u remark_site=%u healthy_fold=%u safety=%u\n",
        id,writable,g_count,g_prefixCount,g_remarkSite,r.feature,r.safety);
    ZrTests_ExecOwner_Reset();return r;
}
static SBeResult be_site(unsigned id,unsigned writable,unsigned site,unsigned *late,unsigned *remark) {
    SBeFixture f;SBeSnapshot old;SBeResult r={1u,1u};SZrOwnerHeapEvent e={0};
    unsigned start=0u,j,triggered=0u,trace=1u,owner=0u,content=0u,zero=0u,remarks=0u,retry=0u,retryValid=0u;
    unsigned code=0u,token=0u,ok=1u,released;SZrOwnerHeapStats st;
    *late=*remark=0u;
    if(!Be_Build(&f,id,0u)||!be_remarks(&f)||!Be_Verify(&f)||!Be_Snapshot(&f,&old)){r.safety=0u;goto done;}
    start=ZrTests_ExecOwner_Stats().attempts;
    if(!ZrTests_ExecOwner_TryArm(site)){r.safety=0u;goto done;}
    ok=(unsigned)Be_Optimize(&f,writable);ZrTests_ExecOwner_Disarm();
    if(writable){code=(unsigned)f.diagnostic.code;token=f.diagnostic.functionToken;}
    for(j=0;j<site;++j){memset(&e,0,sizeof(e));if(!ZrTests_ExecOwner_TryEvent(start+j,&e)||e.kind!=g_trace[j].kind||
        e.count!=g_trace[j].count||e.width!=g_trace[j].width||e.bytes!=g_trace[j].bytes)trace=0u;
        if(j+1u==site)triggered=e.failed;}
    owner=(unsigned)Be_OwnersEqual(&f,&old);
    /* Inventory checks ownership before hashing/Verify; no released old owner
     * or unexpected replacement is dereferenced to claim rollback. */
    if(owner){content=(unsigned)(Be_Hash(&f)==old.hash);zero=(unsigned)be_zero(&f.result);remarks=(unsigned)be_remark_preserved(&f);}
    r.safety&=(unsigned)(!ok&&triggered&&trace&&owner&&content&&zero&&remarks&&
        (!writable||code==ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY));
    if(r.safety)r.safety&=(unsigned)Be_Verify(&f);
    if(writable&&site>g_prefixCount&&token==ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF,102u))*late=1u;
    if(writable&&site==g_remarkSite&&token==0u)*remark=1u;
    if(r.safety){retryValid=(unsigned)(Be_Optimize(&f,writable)&&Be_Check(&f,0u));
        retry=retryValid?(unsigned)Be_Check(&f,1u):0u;r.safety&=retryValid;r.feature&=retry;}
 done:released=(unsigned)Be_Dispose(&f);st=ZrTests_ExecOwner_Stats();
    r.safety&=(unsigned)(released&&st.failures==1u);Be_NoteFault(1u,r.safety?1u:0u);
    printf("BOOL_EDGE_FAULT id=%u writable=%u site=%u measured=%u triggered=%u trace=%u optimize=%u owner=%u content=%u resultzero=%u remarks=%u retry_valid=%u retry_fold=%u late=%u remark=%u code=%u token=%u released=%u live=%u errors=%u safety=%u\n",
        id,writable,site,g_count,triggered,trace,ok,owner,content,zero,remarks,retryValid,retry,*late,*remark,code,token,released,st.live,st.errors,r.safety);
    ZrTests_ExecOwner_Reset();return r;
}
SBeResult Be_Faults(unsigned id) {
    SBeResult r={1u,1u},q;unsigned w,site,late,remark,foundLate,foundRemark;
    for(w=0u;w<2u;++w){foundLate=foundRemark=0u;
        if(!be_prefix(id)){r.safety=0u;continue;}
        q=be_measure(id,w);r.safety&=q.safety;r.feature&=q.feature;if(!q.safety)continue;
        for(site=1u;site<=g_count;++site){q=be_site(id,w,site,&late,&remark);
            r.safety&=q.safety;r.feature&=q.feature;foundLate|=late;foundRemark|=remark;}
        if(id==22u&&w)r.safety&=(unsigned)(foundLate&&foundRemark&&g_remarkSite>0u);
        Be_Profile(id,w?"actual_sites_w1":"actual_sites_w0",r);
    }return r;
}
SBeResult Be_Budget(unsigned id) {
    SBeResult r={1u,1u},q;SBeFixture f;SZrExecIrPassBudget budget={0u,0u};
    unsigned entry,w,ok,valid,matched,appended;TZrUInt64 measured,cut;
    for(entry=0u;entry<2u;++entry) {
        for(w=0u;w<2u;++w){measured=0u;
        if(!Be_Build(&f,1u,0u)||!Be_Verify(&f)){r.safety=0u;goto measured_done;}
        f.context.budget=&budget;ok=(unsigned)Be_Private(&f,entry,w);measured=f.context.workUsed;
        r.safety&=(unsigned)(ok&&measured>0u&&measured<=4096u&&!f.context.budgetExhausted&&Be_Check(&f,0u));
        r.feature&=(unsigned)Be_Check(&f,1u);
 measured_done:r.safety&=(unsigned)Be_Dispose(&f);ZrTests_ExecOwner_Reset();
        printf("BOOL_EDGE_MEASURE id=%u entry=%u writable=%u work=%" PRIu64 " safety=%u\n",id,entry,w,measured,r.safety);
        if(!r.safety)continue;
        for(cut=1u;cut<=measured;++cut){unsigned exhausted=0u;TZrUInt64 used=0u;
            q=(SBeResult){1u,1u};budget.maxWork=cut;budget.maxInstructions=0u;ok=valid=matched=appended=0u;
            if(!Be_Build(&f,1u,0u)||!Be_Verify(&f)){q.safety=0u;goto cutoff_done;}
            f.context.budget=&budget;ok=(unsigned)Be_Private(&f,entry,w);used=f.context.workUsed;exhausted=f.context.budgetExhausted;
            valid=(unsigned)(ok&&Be_Check(&f,0u));matched=valid?(unsigned)Be_Check(&f,1u):0u;
            appended=f.module.constantCount-f.initialCount;q.safety=(unsigned)(valid&&used<=cut&&
                (cut==measured?!exhausted:exhausted));if(cut==measured)q.feature=matched;
 cutoff_done:q.safety&=(unsigned)Be_Dispose(&f);r.safety&=q.safety;r.feature&=q.feature;
            printf("BOOL_EDGE_BUDGET id=%u entry=%u writable=%u limit=%" PRIu64 " measured=%" PRIu64 " work=%" PRIu64 " exhausted=%u optimize=%u verify=%u appended=%u matched=%u safety=%u\n",
                id,entry,w,cut,measured,used,exhausted,ok,valid,appended,matched,q.safety);
            ZrTests_ExecOwner_Reset();}
        budget.maxWork=0u;
        /* Public workUsed is not observable. This is one bounded public
         * partial-success witness, not an exhaustive public work sweep. */
        if(entry==0u){
        q=(SBeResult){1u,1u};if(!Be_Build(&f,1u,0u)||!Be_Verify(&f)){q.safety=0u;goto public_done;}
        budget.maxWork=1u;f.options.budget=&budget;ok=(unsigned)Be_Optimize(&f,w);
        q.safety=(unsigned)(ok&&f.result.budgetExhausted&&Be_Check(&f,0u));
 public_done:q.safety&=(unsigned)Be_Dispose(&f);r.safety&=q.safety;ZrTests_ExecOwner_Reset();budget.maxWork=0u;
        }
        Be_Profile(id,entry?(w?"measured_pipeline_sweep_w1":"measured_pipeline_sweep_w0"):
                (w?"measured_owner_sweep_w1":"measured_owner_sweep_w0"),r);
        }
    }
    return r;
}
