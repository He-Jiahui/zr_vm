#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "ssa_phi_identity_support.h"

static int budget_remarks(const SPixFixture *f,unsigned entry,unsigned exhausted) {
    const SZrExecIrRemarkSink *sink=&f->base.remarks;unsigned i,count;
    const SZrExecIrPassInfo *passes=ZrParser_ExecIr_GetScalarPasses(&count);
    if(!passes||count!=2u||!sink->count||sink->count>f->functions*2u)return 0;
    for(i=0u;i<sink->count;++i){const SZrExecIrOptimizationRemark *r=sink->items+i;
        if(r->pass!=passes[i%2u].name||r->verifierChecks!=2u)return 0;
        if(r->outcome==ZR_EXEC_IR_REMARK_BLOCKED){
            if(!exhausted||i+1u!=sink->count||r->reasonCode!=ZR_EXEC_IR_PASS_REASON_BUDGET)return 0;
        }else if(r->reasonCode==ZR_EXEC_IR_PASS_REASON_BUDGET)return 0;
    }
    if(exhausted&&sink->items[sink->count-1u].outcome!=ZR_EXEC_IR_REMARK_BLOCKED)return 0;
    if(entry==2u&&(f->base.result.passesRun!=sink->count||
        f->base.result.functionsVisited!=(sink->count+1u)/2u||
        f->base.result.functionsChanged>f->base.result.functionsVisited))return 0;
    return 1;
}

static SPiResult measure(unsigned id,unsigned entry,unsigned writable,TZrUInt64 *work) {
    SPixFixture f;SPixSnapshot before;SPixChecks pre={0},post={0};SPiResult r={0u,0u};
    SZrExecIrPassBudget budget={0};unsigned run=0u,exact=0u;
    *work=0u;Pi_SetUp();
    if(!Pix_Build(&f,1u,0u,1u)||!Pix_Live(&f)||!Pix_Capture(&f,&before))goto done;
    pre=Pix_Verify(&f,writable);if(!pre.core||!pre.parser)goto done;
    /* NULL budget would disable work accounting, so unlimited is a real object. */
    f.base.context.budget=&budget;run=(unsigned)Pix_Run(&f,entry,writable);
    post=Pix_Verify(&f,writable);
    if(post.core&&post.parser){exact=(unsigned)Pix_Partial(&f,&before,entry);r.feature=(unsigned)Pix_Expected(&f,&before,entry);}
    *work=f.base.context.workUsed;
    r.safety=run&&post.core&&post.parser&&exact&&*work>0u&&*work<=PIX_WORK&&!f.base.context.budgetExhausted;
done:
    r.safety&=(unsigned)Pi_Dispose(&f.base);
    printf("PHI_IDENTITY_MEASURE id=%u entry=%u writable=%u work=%" PRIu64 " bound=%u run=%u pre=%u post=%u exact=%u safety=%u feature=%u\n",
        id,entry,writable,*work,PIX_WORK,run,pre.core&&pre.parser,post.core&&post.parser,exact,r.safety,r.feature);
    return r;
}
static SPiResult cutoff(unsigned id,unsigned entry,unsigned w,TZrUInt64 limit,TZrUInt64 instructions,TZrUInt64 full,unsigned publicRun) {
    SPixFixture f;SPixSnapshot before;SPixChecks pre={0},post={0};SPiResult r={0u,1u};
    SZrExecIrPassBudget budget={0};unsigned run=0u,exact=0u,expected=0u,exhausted=0u,reason=0u,changed=0u;
    TZrUInt64 used=0u;unsigned healthy=limit==full||limit==full+1u||instructions>=4u;
    Pi_SetUp();
    if(!Pix_Build(&f,1u,0u,publicRun?2u:1u)||!Pix_Capture(&f,&before))goto done;
    pre=Pix_Verify(&f,w);if(!pre.core||!pre.parser)goto done;
    budget.maxWork=limit;budget.maxInstructions=instructions;
    if(publicRun)f.base.options.budget=&budget;else f.base.context.budget=&budget;
    run=(unsigned)Pix_Run(&f,entry,w);post=Pix_Verify(&f,w);
    exhausted=publicRun?(unsigned)f.base.result.budgetExhausted:(unsigned)f.base.context.budgetExhausted;
    used=f.base.context.workUsed;reason=f.base.context.lastReasonCode;
    changed=publicRun?f.base.result.functionsChanged:(unsigned)f.base.changed;
    if(post.core&&post.parser){
        exact=(unsigned)Pix_Partial(&f,&before,entry);expected=(unsigned)Pix_Expected(&f,&before,entry);
        if(entry==1u)changed=(unsigned)(Pix_Hash(&f)!=before.hash);
        if(publicRun&&f.base.remarks.count)reason=f.base.remarks.items[f.base.remarks.count-1u].reasonCode;
    }
    r.safety=run&&post.core&&post.parser&&exact&&(publicRun||!limit||used<=limit);
    if(entry==0u&&post.core&&post.parser)
        r.safety&=((unsigned)f.base.changed==(unsigned)(Pix_Hash(&f)!=before.hash));
    if(!publicRun&&exhausted)r.safety&=(reason==ZR_EXEC_IR_PASS_REASON_BUDGET);
    if(entry!=0u&&post.core&&post.parser)r.safety&=(unsigned)budget_remarks(&f,entry,exhausted);
    /* A cutoff may publish completed rewrites while blocking DCE. Only known
     * full boundaries require the final logical-PHI lifecycle oracle. */
    if(healthy)r.feature=expected&&!exhausted;
    if(instructions&&instructions<4u)r.feature=exhausted;
done:
    r.safety&=(unsigned)Pi_Dispose(&f.base);
    printf("%s id=%u entry=%u writable=%u limit=%" PRIu64 " max_instructions=%" PRIu64 " %s=%" PRIu64 " %s=%" PRIu64 " run=%u exhausted=%u reason=%u changed=%u pre=%u post=%u exact=%u expected=%u safety=%u feature=%u\n",
        publicRun?"PHI_IDENTITY_PUBLIC_BUDGET":"PHI_IDENTITY_BUDGET",id,entry,w,limit,instructions,
        publicRun?"finite_upper":"measured",full,publicRun?"work_used_unavailable":"used",publicRun?UINT64_C(1):used,run,exhausted,reason,changed,
        pre.core&&pre.parser,post.core&&post.parser,exact,expected,r.safety,r.feature);
    return r;
}
SPiResult Pix_Budget(unsigned id) {
    SPiResult all={1u,1u};unsigned entry,w;TZrUInt64 work,limit,inst;
    for(entry=id==17u?1u:0u;entry<2u;++entry)for(w=0u;w<2u;++w){
        SPiResult measured=measure(id,entry,w,&work);all.safety&=measured.safety;all.feature&=measured.feature;
        if(!measured.safety)continue;
        if(id==16u){
            for(limit=1u;limit<=work+1u;++limit){SPiResult r=cutoff(id,entry,w,limit,0u,work,0u);
                all.safety&=r.safety;all.feature&=r.feature;}
            for(inst=3u;inst<=5u;++inst){SPiResult r=cutoff(id,entry,w,0u,inst,work,0u);
                all.safety&=r.safety;all.feature&=r.feature;}
        }else {
            /* Identical two-function fixture: each scalar pipeline has the
             * measured work. Public manager bookkeeping does not consume pass
             * budget. The +64 margin is explicit, not a public measured W. */
            TZrUInt64 upper=work*2u+64u;
            printf("PHI_IDENTITY_PUBLIC_BOUND id=17 writable=%u private_pipeline_work=%" PRIu64 " functions=2 margin=64 finite_upper=%" PRIu64 "\n",w,work,upper);
            for(limit=1u;limit<=upper;++limit){SPiResult r=cutoff(id,2u,w,limit,0u,upper,1u);
                all.safety&=r.safety;all.feature&=r.feature;}
            for(inst=3u;inst<=5u;++inst){SPiResult r=cutoff(id,2u,w,0u,inst,upper,1u);
                all.safety&=r.safety;all.feature&=r.feature;}
        }
    }
    return all;
}
