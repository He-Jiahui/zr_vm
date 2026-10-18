#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "ssa_sccp_pooled_bool_fixture.h"
#include "ssa_exec_ir_owned_module_shim.h"

static int bp_budget_prepare(SBpFixture *f,unsigned reuse) {
    SZrExecIrConstant c={ZR_VALUE_TYPE_BOOL,0u,1u};SZrExecIrRange range;
    if (!Bp_Build(f,0u,40u,40u,1u)) return 0;
    if (reuse) {
        if (!ZrCore_ExecIr_ModuleAppendConstant(&f->module,&c,1u,&range)) return 0;
        f->appended=0u;f->expectedIndex=range.start;
    }
    return Bp_Seal(f)&&Bp_Verify(f);
}
SBpResult Bp_Budget(void) {
    SBpResult all={1u,1u}; unsigned reuse,entry,w;
    for (reuse=0u;reuse<2u;++reuse)
    for (entry=1u;entry<=2u;++entry) for (w=0u;w<2u;++w) {
        SBpFixture healthy; SZrExecIrPassBudget budget={0}; TZrUInt64 measured=0u,limit;
        unsigned safe=0u,fold=0u;
        if (bp_budget_prepare(&healthy,reuse)) {
            healthy.context.budget=&budget;
            safe=(unsigned)Bp_Run(&healthy,entry,w);
            if (Bp_Live(&healthy)) { safe&=(unsigned)Bp_Verify(&healthy); fold=(unsigned)Bp_Check(&healthy,1u); }
            measured=healthy.context.workUsed;
        }
        safe&=(unsigned)(measured>0u && measured<=4096u);
        safe&=(unsigned)Bp_Dispose(&healthy); all.safety&=safe; all.feature&=fold;
        printf("POOLED_BOOL_MEASURE reuse=%u entry=%u writable=%u work=%" PRIu64 " safety=%u feature=%u\n",reuse,entry,w,measured,safe,fold);
        ZrTests_ExecOwner_Reset();
        if (!safe) continue;
        for (limit=1u;limit<=measured;++limit) {
            SBpFixture f; unsigned ok=0u,valid=0u,partial=0u,exhausted=0u; TZrUInt64 used=0u;
            budget=(SZrExecIrPassBudget){.maxWork=limit,.maxInstructions=0u};
            if (bp_budget_prepare(&f,reuse)) {
                f.context.budget=&budget; ok=(unsigned)Bp_Run(&f,entry,w);
                used=f.context.workUsed; exhausted=f.context.budgetExhausted;
                if (Bp_Live(&f)) { valid=(unsigned)Bp_Verify(&f); partial=(unsigned)Bp_Partial(&f); }
            }
            safe=ok&&valid&&partial&&used<=limit&&Bp_NoOrphans(&f);
            safe&=(unsigned)(limit<measured?exhausted==1u:exhausted==0u);
            if (limit==measured) all.feature&=(unsigned)Bp_Check(&f,1u);
            safe&=(unsigned)Bp_Dispose(&f); all.safety&=safe;
            printf("POOLED_BOOL_BUDGET reuse=%u entry=%u writable=%u limit=%" PRIu64 " measured=%" PRIu64 " used=%" PRIu64 " exhausted=%u safety=%u\n",
                   reuse,entry,w,limit,measured,used,exhausted,safe);
            ZrTests_ExecOwner_Reset();
        }
    }
    return all;
}


enum { BP_TRACE_LIMIT=4096 };
static SZrOwnerHeapEvent bp_trace[BP_TRACE_LIMIT];
static int bp_prefix_sink(SBpFixture *f) {
    SZrExecIrOptimizationRemark row;
    memset(&row,0,sizeof(row));row.pass="pooled_bool_existing";
    row.sourceId=601u;row.beforeHash=17u;row.afterHash=19u;
    f->remarks.items=calloc(1u,sizeof(*f->remarks.items));
    if (!f->remarks.items) return 0;
    f->remarks.capacity=f->remarks.count=1u;
    f->remarks.items[0]=row;f->existingRemark=row;
    return 1;
}
static int bp_sink_unchanged(const SBpFixture *f) {
    SZrOwnerLiveSpan span;
    if (f->remarks.count!=1u || !f->remarks.items ||
        !ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,
            f->remarks.items,f->remarks.capacity,sizeof(*f->remarks.items),&span)) return 0;
    return !memcmp(f->remarks.items,&f->existingRemark,sizeof(f->existingRemark));
}
static int bp_prepare(SBpFixture *f,unsigned two) {
    if (!Bp_Build(f,0u,40u,40u,1u)) return 0;
    if (two) {
        while (f->module.constantCount<f->module.constantCapacity) {
            SZrExecIrConstant c={ZR_VALUE_TYPE_INT64,0u,100u+f->module.constantCount};
            SZrExecIrRange range;
            if (!ZrCore_ExecIr_ModuleAppendConstant(&f->module,&c,1u,&range)) return 0;
        }
        if (!Bp_AddFunction(f)) return 0;
    }
    return Bp_Seal(f)&&Bp_Verify(f)&&bp_prefix_sink(f);
}
static unsigned bp_first_function_prefix(void) {
    SBpFixture f;SZrExecIrModule clone;SZrExecIrAnalysisCache cache;
    SZrExecIrPassContext context;const SZrExecIrPassInfo *passes;
    TZrUInt32 count;unsigned start=0u,prefix=0u,good=0u;
    ZrCore_ExecIr_ModuleInit(&clone);ZrParser_ExecIr_AnalysisCacheInit(&cache);
    memset(&context,0,sizeof(context));
    if (!bp_prepare(&f,1u)) goto done;
    passes=ZrParser_ExecIr_GetScalarPasses(&count);
    start=ZrTests_ExecOwner_Stats().attempts;
    if (!passes || count!=2u || !ZrParser_ExecIr_Verify(&f.module,&f.diagnostic) ||
        !ZrCore_ExecIr_CloneModule(&f.module,&clone,&f.diagnostic)) goto done;
    context.cache=&cache;context.remarks=&f.remarks;
    context.constants=clone.constants;context.constantCount=clone.constantCount;
    good=(unsigned)ZrParser_ExecIr_RunModulePassPipeline(&clone,clone.functions,
                         passes,count,&context,&f.diagnostic);
    if (good) prefix=ZrTests_ExecOwner_Stats().attempts-start;
 done:
    ZrCore_ExecIr_FreeModule(&clone);ZrParser_ExecIr_AnalysisCacheFree(&cache);
    good&=(unsigned)Bp_Dispose(&f);ZrTests_ExecOwner_Reset();
    return good?prefix:0u;
}
SBpResult Bp_Faults(void) {
    SBpResult all={1u,1u};unsigned two,w,prefix=bp_first_function_prefix();
    all.safety&=(unsigned)(prefix>0u && prefix<BP_TRACE_LIMIT);
    for (two=0u;two<2u;++two) for (w=0u;w<2u;++w) {
        SBpFixture healthy;SZrOwnerLiveSpan sink;unsigned measured=0u,begin=0u,site,completed=0u;
        unsigned safe=0u,fold=0u,remarkSite=0u,lateSeen=0u,remarkSeen=0u,j;
        unsigned growthSite=0u,growthRecord=UINT32_MAX,growthSeen=0u,beforeCapacity=0u;
        if (bp_prepare(&healthy,two) && ZrTests_ExecOwner_TryLiveSpan(0u,
            ZrTests_ExecOwner_Stats().records,healthy.remarks.items,1u,sizeof(*healthy.remarks.items),&sink)) {
            beforeCapacity=healthy.module.constantCapacity;
            begin=ZrTests_ExecOwner_Stats().attempts;
            safe=(unsigned)Bp_Run(&healthy,0u,w);
            measured=ZrTests_ExecOwner_Stats().attempts-begin;
            if (measured>=BP_TRACE_LIMIT) safe=0u;
            if (safe&&Bp_Live(&healthy)) {
                SZrOwnerLiveSpan publishedPool;
                if (ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,
                    healthy.module.constants,healthy.module.constantCapacity,sizeof(*healthy.module.constants),&publishedPool))
                    growthRecord=publishedPool.record;
                else safe=0u;
            }
            if (safe) for (j=0u;j<measured;++j) {
                if (!ZrTests_ExecOwner_TryEvent(begin+j,bp_trace+j)) {safe=0u;break;}
                if (!remarkSite && bp_trace[j].kind==3u && bp_trace[j].record==sink.record)
                    remarkSite=j+1u;
                if (!growthSite && bp_trace[j].kind==3u && bp_trace[j].record==growthRecord &&
                    bp_trace[j].bytes>beforeCapacity*sizeof(SZrExecIrConstant))
                    growthSite=j+1u;
            }
            if (Bp_Live(&healthy)) {safe&=(unsigned)Bp_Verify(&healthy);fold=(unsigned)Bp_Check(&healthy,1u);}
        }
        safe&=(unsigned)(measured>0u && measured<BP_TRACE_LIMIT && remarkSite>0u&&
            (!two||(growthSite>0u&&healthy.module.constantCapacity>beforeCapacity)));
        safe&=(unsigned)Bp_Dispose(&healthy);all.safety&=safe;all.feature&=fold;
        printf("POOLED_BOOL_TRACE two=%u writable=%u measured=%u first_function_prefix=%u remark_site=%u growth_site=%u growth_record=%u before_capacity=%u safety=%u feature=%u\n",
               two,w,measured,prefix,remarkSite,growthSite,growthRecord,beforeCapacity,safe,fold);
        ZrTests_ExecOwner_Reset();
        if (!safe) continue;
        for (site=1u;site<=measured;++site) {
            SBpFixture f;SZrExecIrFunction *owner=NULL;SZrExecIrConstant *pool=NULL;
            SZrOwnerLiveSpan originalFn,originalPool;TZrUInt64 hash=0u;
            unsigned ok=1u,valid=0u,original=0u,triggered=0u,retry=0u,trace=1u;
            unsigned late=0u,remark=0u,growth=0u,start=0u,releasedFn=1u,releasedPool=1u,failedToken=0u;
            if (bp_prepare(&f,two) && ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,
                    f.module.functions,f.module.functionCapacity,sizeof(*f.module.functions),&originalFn) &&
                ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,
                    f.module.constants,f.module.constantCapacity,sizeof(*f.module.constants),&originalPool)) {
                owner=f.module.functions;pool=f.module.constants;
                hash=ZrParser_ExecIr_FunctionHash(owner);start=ZrTests_ExecOwner_Stats().attempts;
                if (ZrTests_ExecOwner_TryArm(site)) ok=(unsigned)Bp_Run(&f,0u,w);
                ZrTests_ExecOwner_Disarm();
                failedToken=f.diagnostic.functionToken;
                for (j=0u;j<site;++j) {SZrOwnerHeapEvent e;
                    if (!ZrTests_ExecOwner_TryEvent(start+j,&e) || e.kind!=bp_trace[j].kind ||
                        e.count!=bp_trace[j].count || e.width!=bp_trace[j].width || e.bytes!=bp_trace[j].bytes ||
                        (j+1u<site && e.record!=bp_trace[j].record)) {trace=0u;break;}
                    if (j+1u==site) triggered=e.failed;
                }
                if (Bp_Live(&f)) {valid=(unsigned)Bp_Verify(&f);original=(unsigned)Bp_Check(&f,0u);}
                safe=(unsigned)(!ok&&triggered==1u&&trace&&valid&&original&&
                    owner==f.module.functions&&pool==f.module.constants&&
                    ZrParser_ExecIr_FunctionHash(f.module.functions)==hash&&
                    !f.result.functionsVisited&&!f.result.functionsChanged&&!f.result.passesRun&&
                    !f.result.beforeHash&&!f.result.afterHash&&!f.result.budgetExhausted&&bp_sink_unchanged(&f)&&
                    ZrTests_ExecOwner_TryRecordFrees(originalFn.record,&releasedFn)&&!releasedFn&&
                    ZrTests_ExecOwner_TryRecordFrees(originalPool.record,&releasedPool)&&!releasedPool);
                late=(unsigned)(two&&site>prefix&&
                    (!w||failedToken==ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF,202u)));
                remark=(unsigned)(site==remarkSite);
                growth=(unsigned)(growthSite&&site==growthSite);
                if (safe) {
                    retry=(unsigned)Bp_Run(&f,0u,w);
                    if (Bp_Live(&f)) retry&=(unsigned)(Bp_Verify(&f)&&Bp_Check(&f,1u)&&Bp_NoOrphans(&f));else retry=0u;
                }
            } else safe=0u;
            all.feature&=retry;safe&=(unsigned)Bp_Dispose(&f);all.safety&=safe;
            if (safe) {++completed;lateSeen|=late;remarkSeen|=remark;growthSeen|=growth;}
            printf("POOLED_BOOL_FAULT two=%u writable=%u site=%u measured=%u triggered=%u trace=%u rejected=%u original=%u verify=%u retry=%u late=%u remark=%u growth=%u safety=%u\n",
                two,w,site,measured,triggered,trace,!ok,original,valid,retry,late,remark,growth,safe);
            ZrTests_ExecOwner_Reset();
        }
        all.safety&=(unsigned)(remarkSeen&&(!two||(lateSeen&&growthSeen)));
        Bp_NoteFault(measured,completed);
    }
    return all;
}
SBpResult Bp_PublicBudgets(void) {
    SBpResult all={1u,1u};unsigned w;TZrUInt64 upper=0u;
    for (w=0u;w<2u;++w) {
        SBpFixture measured;SZrExecIrPassBudget budget={0};TZrUInt64 limit,work=0u;
        unsigned safe=0u,sawExhausted=0u,sawFull=0u;
        if (Bp_Build(&measured,0u,40u,40u,1u)&&Bp_Verify(&measured)) {
            measured.context.budget=&budget;safe=(unsigned)Bp_Run(&measured,2u,w);
            work=measured.context.workUsed;
        }
        safe&=(unsigned)Bp_Dispose(&measured);ZrTests_ExecOwner_Reset();
        safe&=(unsigned)(work>0u&&work<=4096u);all.safety&=safe;
        if (!safe) continue;
        upper=2u*work+64u; /* Explicit finite public bound, not public workUsed. */
        for (limit=1u;limit<=upper;++limit) {
            SBpFixture f;unsigned ok=0u,valid=0u,partial=0u,exhausted=0u;
            budget=(SZrExecIrPassBudget){.maxWork=limit,.maxInstructions=0u};
            if (Bp_Build(&f,0u,40u,40u,1u)&&Bp_AddFunction(&f)&&Bp_Verify(&f)) {
                f.options.budget=&budget;ok=(unsigned)Bp_Run(&f,0u,w);exhausted=f.result.budgetExhausted;
                if (Bp_Live(&f)) {valid=(unsigned)Bp_Verify(&f);
                    /* Different functions may be at different valid rewrite stages. */
                    partial=(unsigned)(Bp_Partial(&f)&&Bp_NoOrphans(&f));}
            }
            safe=ok&&valid&&partial;
            if (safe) {sawExhausted|=exhausted;sawFull|=(unsigned)Bp_Check(&f,1u);}
            safe&=(unsigned)Bp_Dispose(&f);all.safety&=safe;
            printf("POOLED_BOOL_PUBLIC_BUDGET writable=%u limit=%" PRIu64 " private_work=%" PRIu64 " public_upper=%" PRIu64 " run=%u exhausted=%u verify=%u noorphan=%u safety=%u\n",
                w,limit,work,upper,ok,exhausted,valid,partial,safe);
            ZrTests_ExecOwner_Reset();
        }
        all.feature&=(unsigned)(sawExhausted&&sawFull);
    }
    return all;
}
