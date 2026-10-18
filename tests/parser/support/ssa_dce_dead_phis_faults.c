#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "ssa_dce_dead_phis_fixture.h"
#include "ssa_exec_ir_owned_module_shim.h"
enum { DP_TRACE_LIMIT=4096, DP_WORK_LIMIT=8192, DP_CACHE_LIMIT=128 };
static SZrOwnerHeapEvent trace[DP_TRACE_LIMIT],prefix[DP_TRACE_LIMIT];
static unsigned traceCount,prefixCount,remarkSite;
static int events(unsigned begin,SZrOwnerHeapEvent *out,unsigned *count) {
    unsigned i,end=ZrTests_ExecOwner_Stats().attempts;if(end<begin||end-begin>DP_TRACE_LIMIT)return 0;
    *count=end-begin;for(i=0;i<*count;++i)if(!ZrTests_ExecOwner_TryEvent(begin+i,out+i))return 0;return 1;
}
static SDpResult budget_case(unsigned id) {
    SDpResult all={1u,1u};unsigned entry,w;TZrUInt64 pipelineWork=0u;
    for(entry=0;entry<2u;++entry)for(w=0;w<2u;++w){SDpFixture f;SZrExecIrPassBudget budget={0};SDpResult r={1u,1u};TZrUInt64 measured=0u,limit;
        if(!Dp_Build(&f,1u,0u,1u)||!Dp_Live(&f)||!Dp_Verify(&f)){r.safety=0u;goto measured_done;}
        f.context.budget=&budget;
        if(!Dp_Run(&f,entry,w)||!Dp_Live(&f)||!Dp_Verify(&f)||!Dp_Stable(&f)){r.safety=0u;goto measured_done;}
        measured=f.context.workUsed;r.feature=(unsigned)Dp_Expected(&f);
        if(!measured||measured>DP_WORK_LIMIT)r.safety=0u;
     measured_done:r.safety&=(unsigned)Dp_Dispose(&f);ZrTests_ExecOwner_Reset();
        printf("DCE_PHI_MEASURE id=%u entry=%u writable=%u work=%" PRIu64 " safety=%u feature=%u\n",id,entry,w,measured,r.safety,r.feature);
        all.safety&=r.safety;all.feature&=r.feature;if(entry==1u)pipelineWork=measured;if(!r.safety)continue;
        for(limit=1u;limit<=measured;++limit){SDpSnapshot before;unsigned ok=0u,valid=0u,unchanged=0u,expected=0u,exhausted=0u,changed=0u;TZrUInt64 used=0u;r=(SDpResult){1u,1u};
            if(!Dp_Build(&f,1u,0u,1u)||!Dp_Live(&f)||!Dp_Verify(&f)||!Dp_Snapshot(&f,&before)){r.safety=0u;goto cutoff_done;}
            budget.maxWork=limit;f.context.budget=&budget;ok=(unsigned)Dp_Run(&f,entry,w);
            exhausted=f.context.budgetExhausted;used=f.context.workUsed;changed=f.changed;
            if(Dp_Live(&f)){valid=(unsigned)Dp_Verify(&f);unchanged=(unsigned)(Dp_Hash(&f)==before.hash);expected=(unsigned)Dp_Expected(&f);}
            r.safety&=(unsigned)(ok&&valid&&used<=limit&&Dp_Stable(&f));
            if(!entry&&exhausted)r.safety&=(unsigned)(unchanged&&!changed&&Dp_SameOwners(&f,&before));
            if(limit==measured){r.safety&=(unsigned)!exhausted;r.feature&=expected;}
         cutoff_done:r.safety&=(unsigned)Dp_Dispose(&f);
            printf("DCE_PHI_BUDGET id=%u entry=%u writable=%u limit=%" PRIu64 " measured=%" PRIu64 " used=%" PRIu64 " run=%u exhausted=%u verify=%u unchanged=%u changed=%u expected=%u safety=%u feature=%u\n",id,entry,w,limit,measured,used,ok,exhausted,valid,unchanged,changed,expected,r.safety,r.feature);
            all.safety&=r.safety;all.feature&=r.feature;ZrTests_ExecOwner_Reset();}
    }
    /* No public workUsed exists: this distinct finite sweep uses a stated
     * conservative upper bound, never claims an exact public measurement. */
    if(id==16u&&all.safety){TZrUInt64 upper=pipelineWork*2u+32u,limit;
        for(w=0;w<2u;++w)for(limit=1u;limit<=upper;++limit){SDpFixture f;SDpResult r={1u,1u};SZrExecIrPassBudget budget={0};unsigned ok=0u,valid=0u,exhausted=0u,expected=0u;
            if(!Dp_Build(&f,1u,0u,2u)||!Dp_Live(&f)||!Dp_Verify(&f)){r.safety=0u;goto public_done;}
            budget.maxWork=limit;f.options.budget=&budget;ok=(unsigned)Dp_Run(&f,2u,w);exhausted=f.result.budgetExhausted;
            if(Dp_Live(&f)){valid=(unsigned)Dp_Verify(&f);expected=(unsigned)Dp_Expected(&f);}
            r.safety&=(unsigned)(ok&&valid&&Dp_Stable(&f));if(limit==upper)r.feature&=expected;
         public_done:r.safety&=(unsigned)Dp_Dispose(&f);
            printf("DCE_PHI_PUBLIC_BUDGET id=16 writable=%u limit=%" PRIu64 " finite_upper=%" PRIu64 " run=%u exhausted=%u verify=%u expected=%u safety=%u feature=%u\n",w,limit,upper,ok,exhausted,valid,expected,r.safety,r.feature);
            all.safety&=r.safety;all.feature&=r.feature;ZrTests_ExecOwner_Reset();}}
    Dp_Profile(id,"all_observable_work_boundaries",id==15u?0u:1u,0u,all);return all;
}
typedef struct SDpCacheSnapshot { TZrUInt64 revision,hash;unsigned mask,count;SZrExecIrSccpValue values[DP_CACHE_LIMIT]; } SDpCacheSnapshot;
static int cache_live(const SZrExecIrAnalysisCache *c) {
    SZrOwnerLiveSpan span;return !c->sccpValues?!c->sccpValueCount:
        ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,c->sccpValues,c->sccpValueCount,sizeof(*c->sccpValues),&span);
}
static int seed(SDpFixture *f,SDpCacheSnapshot *old) {
    TZrBool changed=ZR_FALSE;
    if(!ZrParser_ExecIr_ComputeSccp(f->module.functions,&f->context,ZR_FALSE,&changed,&f->diagnostic)||!cache_live(&f->cache)||f->cache.sccpValueCount>DP_CACHE_LIMIT)return 0;
    old->revision=f->cache.revision;old->hash=f->cache.irHash;old->mask=f->cache.validMask;old->count=f->cache.sccpValueCount;
    if(old->count)memcpy(old->values,f->cache.sccpValues,old->count*sizeof(*old->values));f->context.workUsed=0u;return 1;
}
static int cache_equal(const SDpFixture *f,const SDpCacheSnapshot *old) {
    return cache_live(&f->cache)&&f->cache.revision==old->revision&&f->cache.irHash==old->hash&&f->cache.validMask==old->mask&&
        f->cache.sccpValueCount==old->count&&(!old->count||!memcmp(f->cache.sccpValues,old->values,old->count*sizeof(*old->values)));
}
static int prepare(SDpFixture *f,unsigned entry,SDpCacheSnapshot *cache) {
    if(!Dp_Build(f,1u,0u,entry==2u?2u:1u)||!Dp_Live(f)||!Dp_Verify(f)||!Dp_Remarks(f))return 0;
    return entry!=1u||seed(f,cache);
}
/* Measured first-function public Optimize prefix, not a guessed site ordinal. */
static int first_function_prefix(void) {
    SDpFixture f;SDpCacheSnapshot cs;SZrExecIrModule candidate;SZrExecIrPassContext ctx={0};SZrExecIrAnalysisCache cache;
    const SZrExecIrPassInfo *passes;unsigned count,begin=0u;int good=1;
    ZrCore_ExecIr_ModuleInit(&candidate);ZrParser_ExecIr_AnalysisCacheInit(&cache);
    if(!prepare(&f,2u,&cs)){good=0;goto done;}passes=ZrParser_ExecIr_GetScalarPasses(&count);if(!passes||count!=2u){good=0;goto done;}
    begin=ZrTests_ExecOwner_Stats().attempts;
    if(!ZrParser_ExecIr_Verify(&f.module,&f.diagnostic)||!ZrCore_ExecIr_CloneModule(&f.module,&candidate,&f.diagnostic)){good=0;goto done;}
    ctx.cache=&cache;ctx.remarks=&f.remarks;ctx.constants=candidate.constants;ctx.constantCount=candidate.constantCount;
    if(!ZrParser_ExecIr_RunModulePassPipeline(&candidate,candidate.functions,passes+1u,1u,&ctx,&f.diagnostic)||!events(begin,prefix,&prefixCount))good=0;
 done:ZrParser_ExecIr_AnalysisCacheFree(&cache);ZrCore_ExecIr_FreeModule(&candidate);good&=Dp_Dispose(&f);ZrTests_ExecOwner_Reset();return good;
}
static SDpResult measure(unsigned id,unsigned entry,unsigned w) {
    SDpFixture f;SDpResult r={1u,1u};SDpCacheSnapshot cache;SZrOwnerLiveSpan sink;unsigned begin=0u,i;
    traceCount=remarkSite=0u;
    if(!prepare(&f,entry,&cache)||!ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,f.remarks.items,1u,sizeof(*f.remarks.items),&sink)){r.safety=0u;goto done;}
    begin=ZrTests_ExecOwner_Stats().attempts;
    if(!Dp_Run(&f,entry,w)||!events(begin,trace,&traceCount)||!traceCount||!Dp_Live(&f)||!Dp_Verify(&f)||!Dp_Stable(&f)){r.safety=0u;goto done;}
    r.feature=(unsigned)Dp_Expected(&f);
    for(i=0;i<traceCount;++i)if(trace[i].kind==3u&&trace[i].record==sink.record&&!remarkSite)remarkSite=i+1u;
    if(entry==2u){if(prefixCount>=traceCount)r.safety=0u;
        for(i=0;i<prefixCount&&i<traceCount;++i)if(prefix[i].kind!=trace[i].kind||prefix[i].count!=trace[i].count||prefix[i].width!=trace[i].width||prefix[i].bytes!=trace[i].bytes)r.safety=0u;}
 done:r.safety&=(unsigned)Dp_Dispose(&f);ZrTests_ExecOwner_Reset();
    printf("DCE_PHI_TRACE id=%u entry=%u writable=%u measured=%u prefix=%u remark_site=%u safety=%u feature=%u\n",id,entry,w,traceCount,entry==2u?prefixCount:0u,remarkSite,r.safety,r.feature);return r;
}
static int result_zero(const SZrExecIrOptimizationResult *r) {
    return !r->functionsVisited&&!r->functionsChanged&&!r->passesRun&&!r->budgetExhausted&&!r->beforeHash&&!r->afterHash;
}
static SDpResult fault_sweep(unsigned id) {
    unsigned entry=id==17u?0u:id==18u?1u:2u,w,site;SDpResult all={1u,1u};
    if(entry==2u&&!first_function_prefix()){all.safety=0u;Dp_Profile(id,"prefix_precondition",entry,0u,all);return all;}
    for(w=0;w<2u;++w){SDpResult m=measure(id,entry,w);unsigned late=0u,remark=0u;all.safety&=m.safety;all.feature&=m.feature;if(!m.safety)continue;
        for(site=1u;site<=traceCount;++site){SDpFixture f;SDpSnapshot before;SDpCacheSnapshot cache;SDpResult r={1u,1u};SZrOwnerHeapEvent event={0};
            unsigned start=0u,triggered=0u,ok=1u,owned=0u,content=0u,valid=0u,retry=0u,expected=0u,code=0u,token=0u,traceok=1u,j;
            unsigned released=0u,cacheok=0u,remarks=0u,zero=0u,islate=0u,isremark=0u,contextok=0u;
            SZrExecIrPassContext oldContext;memset(&oldContext,0,sizeof(oldContext));
            if(!prepare(&f,entry,&cache)||!Dp_Snapshot(&f,&before)){r.safety=0u;goto done;}
            if(entry==1u){f.context.workUsed=4u;f.context.lastSourceId=717u;f.context.lastReasonCode=ZR_EXEC_IR_PASS_REASON_NO_CHANGE;f.context.passesRun=2u;}
            oldContext=f.context;
            start=ZrTests_ExecOwner_Stats().attempts;if(!ZrTests_ExecOwner_TryArm(site)){r.safety=0u;goto done;}
            ok=(unsigned)Dp_Run(&f,entry,w);ZrTests_ExecOwner_Disarm();
            if(ZrTests_ExecOwner_TryEvent(start+site-1u,&event))triggered=event.failed;
            for(j=0;j<site;++j){SZrOwnerHeapEvent actual;if(!ZrTests_ExecOwner_TryEvent(start+j,&actual)||actual.kind!=trace[j].kind||actual.count!=trace[j].count||actual.width!=trace[j].width||actual.bytes!=trace[j].bytes)traceok=0u;}
            if(w){code=(unsigned)f.diagnostic.code;token=f.diagnostic.functionToken;}
            /* Ledger proves live current storage before any hash/Verify. Old
             * addresses remain integers; a function rollback may replace heaps. */
            if(Dp_Live(&f)){owned=(unsigned)Dp_SameOwners(&f,&before);content=(unsigned)(Dp_Hash(&f)==before.hash);valid=(unsigned)Dp_Verify(&f);
                remarks=(unsigned)Dp_RemarkPrefix(&f);cacheok=(unsigned)(entry!=1u||cache_equal(&f,&cache));zero=(unsigned)(entry!=2u||result_zero(&f.result));}
            contextok=(unsigned)(entry!=1u||(f.context.workUsed==oldContext.workUsed&&f.context.budgetExhausted==oldContext.budgetExhausted&&
                f.context.lastSourceId==oldContext.lastSourceId&&f.context.lastReasonCode==oldContext.lastReasonCode&&f.context.passesRun==oldContext.passesRun));
            r.safety&=(unsigned)(triggered&&traceok&&!ok&&content&&valid&&remarks&&cacheok&&zero&&contextok);
            if(w)r.safety&=(unsigned)(code==ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
            if(entry!=1u)r.safety&=owned;
            islate=(unsigned)(entry==2u&&site>prefixCount&&w&&token==ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF,91u));
            isremark=(unsigned)(entry!=0u&&site==remarkSite&&event.kind==3u);late+=islate;remark+=isremark;
            if(r.safety){retry=(unsigned)(Dp_Run(&f,entry,w)&&Dp_Live(&f)&&Dp_Verify(&f)&&Dp_Stable(&f));expected=retry?(unsigned)Dp_Expected(&f):0u;r.safety&=retry;r.feature&=expected;}
         done:released=(unsigned)Dp_Dispose(&f);r.safety&=released;if(ZrTests_ExecOwner_Stats().failures!=1u)r.safety=0u;Dp_FaultCount(1u,r.safety?1u:0u);
            printf("DCE_PHI_FAULT id=%u entry=%u writable=%u site=%u measured=%u triggered=%u trace=%u run=%u owner=%u content=%u verify=%u cache=%u context=%u remarks=%u resultzero=%u retry=%u expected=%u released=%u late=%u remark=%u code=%u token=%u safety=%u feature=%u\n",id,entry,w,site,traceCount,triggered,traceok,ok,owned,content,valid,cacheok,contextok,remarks,zero,retry,expected,released,islate,isremark,code,token,r.safety,r.feature);
            all.safety&=r.safety;all.feature&=r.feature;ZrTests_ExecOwner_Reset();}
        if(entry!=0u&&!remark)all.safety=0u;if(entry==2u&&w&&!late)all.safety=0u;
        Dp_Profile(id,"every_actual_allocation",entry,w,all);
    }return all;
}
static SDpResult repeat_case(void) {
    SDpResult all={1u,1u};unsigned shape,entry,w;
    for(shape=0u;shape<2u;++shape)for(entry=0;entry<3u;++entry)for(w=0;w<2u;++w){
        SDpFixture f;SDpResult r={1u,1u};unsigned pass,observed[3]={0u,0u,0u};TZrUInt64 h=0u;
        if(!Dp_Build(&f,5u,shape?3u:0u,1u)||!Dp_Live(&f)||!Dp_Verify(&f)){r.safety=0u;goto done;}
        for(pass=0;pass<3u;++pass){SDpCacheSnapshot cache;TZrUInt64 before;TZrUInt64 revision=0u;
            if(entry==1u){if(!seed(&f,&cache)){r.safety=0u;break;}revision=f.cache.revision;}
            before=ZrParser_ExecIr_FunctionHash(f.module.functions);f.context.workUsed=0u;
            if(!Dp_Run(&f,entry,w)||!Dp_Live(&f)||!Dp_Verify(&f)||!Dp_Stable(&f)){r.safety=0u;break;}
            observed[pass]=(unsigned)Dp_ObserveChange(&f,entry,pass?0u:1u,before);
            if(entry==1u)observed[pass]&=(unsigned)(f.cache.revision>revision);
            r.feature&=(unsigned)(Dp_Expected(&f)&&observed[pass]);
            if(pass&&h!=Dp_Hash(&f))r.safety=0u;
            h=Dp_Hash(&f);
        }
     done:r.safety&=(unsigned)Dp_Dispose(&f);
        printf("DCE_PHI_FIXEDPOINT shape=%u entry=%u writable=%u first_ok=%u second_ok=%u third_ok=%u safety=%u feature=%u\n",shape,entry,w,observed[0],observed[1],observed[2],r.safety,r.feature);
        Dp_Profile(20u,shape?"three_fixedpoint_runs_phi_only":"three_fixedpoint_runs_mixed",entry,w,r);
        all.safety&=r.safety;all.feature&=r.feature;ZrTests_ExecOwner_Reset();
    }
    return all;
}
SDpResult Dp_Faults(unsigned id) {
    if(id==15u||id==16u)return budget_case(id);
    if(id>=17u&&id<=19u)return fault_sweep(id);
    return repeat_case();
}
