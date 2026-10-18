#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "ssa_phi_identity_support.h"
#include "ssa_exec_ir_owned_module_shim.h"

typedef struct SPixCacheCopy {
    TZrUInt64 revision,hash;unsigned mask,count;SZrExecIrSccpValue values[PIX_VALUES];
} SPixCacheCopy;
typedef struct SPixOwners {
    unsigned count,record[128];uintptr_t address[128];size_t bytes[128];
} SPixOwners;
static SZrOwnerHeapEvent trace[PIX_TRACE],prefix[PIX_TRACE];
static unsigned traceCount,prefixCount,remarkSite;
static const char sentinelName[]="phi_identity_sentinel";
static int events(unsigned start,SZrOwnerHeapEvent *out,unsigned *count) {
    unsigned i,end=ZrTests_ExecOwner_Stats().attempts;
    if(end<start||end-start>PIX_TRACE)return 0;*count=end-start;
    for(i=0u;i<*count;++i)if(!ZrTests_ExecOwner_TryEvent(start+i,out+i))return 0;return 1;
}
static int event_same(const SZrOwnerHeapEvent *a,const SZrOwnerHeapEvent *b) {
    return a->kind==b->kind&&a->count==b->count&&a->width==b->width&&a->bytes==b->bytes;
}
static int owner(SPixOwners *s,const void *p,unsigned count,unsigned capacity,size_t width) {
    SZrOwnerLiveSpan live;unsigned i=s->count;
    if(!p)return !count&&!capacity;
    if(i==128u||!capacity||count>capacity||
       !ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,p,capacity,width,&live))return 0;
    s->record[i]=live.record;s->address[i]=(uintptr_t)p;s->bytes[i]=(size_t)capacity*width;++s->count;return 1;
}
static int owners(const SPixFixture *f,SPixOwners *s) {
    const SZrExecIrModule *m=&f->base.module;unsigned i;memset(s,0,sizeof(*s));
#define O(p,n,c) do { if(!owner(s,(p),(n),(c),sizeof(*(p))))return 0; } while(0)
    O(m->functions,m->functionCount,m->functionCapacity);
    O(m->constants,m->constantCount,m->constantCapacity);O(m->layouts,m->layoutCount,m->layoutCapacity);
    O(m->sourceMaps,m->sourceMapCount,m->sourceMapCapacity);
    for(i=0u;i<m->functionCount;++i){const SZrExecIrFunction *fn=m->functions+i;
#define F(p,n,c) O(fn->p,fn->n,fn->c)
        F(values,valueCount,valueCapacity);F(instructions,instructionCount,instructionCapacity);F(blocks,blockCount,blockCapacity);
        F(operands,operandCount,operandCapacity);F(results,resultCount,resultCapacity);F(phiPool,phiCount,phiCapacity);
        F(phiIncoming,phiIncomingCount,phiIncomingCapacity);F(predecessors,predecessorCount,predecessorCapacity);
        F(successors,successorCount,successorCapacity);F(memoryTokenPool,memoryTokenCount,memoryTokenCapacity);
#undef F
        if(fn->frameLayout||fn->gcMap||fn->stateMap||fn->gcRoots||fn->deoptStates||fn->sourceMaps||fn->bindingRows)return 0;
    }
#undef O
    return 1;
}
static int owners_equal(const SPixFixture *f,const SPixOwners *old) {
    SPixOwners now;unsigned i,freed;
    if(!owners(f,&now)||now.count!=old->count)return 0;
    for(i=0u;i<now.count;++i)if(now.address[i]!=old->address[i]||now.bytes[i]!=old->bytes[i]||now.record[i]!=old->record[i]||
        !ZrTests_ExecOwner_TryRecordFrees(old->record[i],&freed)||freed)return 0;
    return 1;
}
static int seed_cache(SPixFixture *f,SPixCacheCopy *s) {
    TZrBool changed=ZR_FALSE;SZrExecIrAnalysisCache *c=&f->base.cache;
    if(!ZrParser_ExecIr_ComputeSccp(f->base.module.functions,&f->base.context,ZR_FALSE,&changed,&f->base.diagnostic)||
       !Pix_Live(f)||c->sccpValueCount>PIX_VALUES||changed)return 0;
    s->revision=c->revision;s->hash=c->irHash;s->mask=c->validMask;s->count=c->sccpValueCount;
    if(s->count)memcpy(s->values,c->sccpValues,s->count*sizeof(*s->values));return 1;
}
static int cache_equal(const SPixFixture *f,const SPixCacheCopy *s) {
    const SZrExecIrAnalysisCache *c=&f->base.cache;
    return Pix_Live(f)&&c->revision==s->revision&&c->irHash==s->hash&&c->validMask==s->mask&&c->sccpValueCount==s->count&&
        (!s->count||!memcmp(c->sccpValues,s->values,s->count*sizeof(*s->values)));
}
static int prepare(SPixFixture *f,unsigned entry,unsigned w,SPixCacheCopy *cache,SZrExecIrOptimizationRemark *sentinel) {
    SPixChecks valid;
    if(!Pix_Build(f,1u,0u,entry==2u?2u:1u))return 0;
    valid=Pix_Verify(f,w);if(!valid.core||!valid.parser)return 0;
    f->base.remarks.items=calloc(1u,sizeof(*f->base.remarks.items));if(!f->base.remarks.items)return 0;
    f->base.remarks.count=f->base.remarks.capacity=1u;
    memset(sentinel,0,sizeof(*sentinel));sentinel->pass=sentinelName;sentinel->sourceId=777u;
    sentinel->outcome=ZR_EXEC_IR_REMARK_MISSED;sentinel->reasonCode=123u;
    sentinel->beforeHash=111u;sentinel->afterHash=222u;sentinel->elapsedTicks=333u;sentinel->verifierTicks=444u;sentinel->verifierChecks=5u;
    f->base.remarks.items[0]=*sentinel;
    if(entry!=2u&&!seed_cache(f,cache))return 0;
    f->base.context.workUsed=4u;f->base.context.lastReasonCode=ZR_EXEC_IR_PASS_REASON_NO_CHANGE;
    f->base.context.lastSourceId=717u;f->base.context.passesRun=2u;
    return Pix_Live(f);
}
static int remark_prefix(const SPixFixture *f,const SZrExecIrOptimizationRemark *sentinel) {
    return Pix_Live(f)&&f->base.remarks.count==1u&&!memcmp(f->base.remarks.items,sentinel,sizeof(*sentinel));
}
static int zero_result(const SZrExecIrOptimizationResult *r) {
    SZrExecIrOptimizationResult zero;memset(&zero,0,sizeof(zero));return memcmp(r,&zero,sizeof(zero))==0;
}
/* Reproduce only the measured public prefix through its first real function.
 * This is a normal Core-owned IR clone, never a source-tree copy. */
static int first_function_prefix(unsigned w) {
    SPixFixture f;SPixCacheCopy cacheCopy;SZrExecIrOptimizationRemark sentinel;
    SZrExecIrModule candidate;SZrExecIrAnalysisCache cache;SZrExecIrPassContext ctx={0};
    const SZrExecIrPassInfo *passes;unsigned count=0u,start=0u;int good=0;
    ZrCore_ExecIr_ModuleInit(&candidate);ZrParser_ExecIr_AnalysisCacheInit(&cache);Pi_SetUp();prefixCount=0u;
    if(!prepare(&f,2u,w,&cacheCopy,&sentinel))goto done;
    passes=ZrParser_ExecIr_GetScalarPasses(&count);if(!passes||count!=2u)goto done;
    start=ZrTests_ExecOwner_Stats().attempts;
    if(!ZrParser_ExecIr_Verify(&f.base.module,w?&f.base.diagnostic:NULL)||
       !ZrCore_ExecIr_CloneModule(&f.base.module,&candidate,w?&f.base.diagnostic:NULL))goto done;
    {SPixFixture candidateView=f;candidateView.base.module=candidate;
        if(!Pix_Live(&candidateView))goto done;}
    ctx.cache=&cache;ctx.remarks=&f.base.remarks;ctx.constants=candidate.constants;ctx.constantCount=candidate.constantCount;
    if(!ZrParser_ExecIr_RunModulePassPipeline(&candidate,candidate.functions,passes,count,&ctx,w?&f.base.diagnostic:NULL))goto done;
    good=events(start,prefix,&prefixCount)&&prefixCount>0u;
done:
    ZrParser_ExecIr_AnalysisCacheFree(&cache);ZrCore_ExecIr_FreeModule(&candidate);
    good&=Pi_Dispose(&f.base);
    printf("PHI_IDENTITY_PREFIX id=20 writable=%u measured=%u safety=%u\n",w,prefixCount,(unsigned)good);
    return good;
}
static SPiResult measure(unsigned id,unsigned entry,unsigned w) {
    SPixFixture f;SPixSnapshot before;SPixCacheCopy cache;SZrExecIrOptimizationRemark sentinel;
    SPixChecks post={0};SPiResult r={0u,0u};SZrOwnerLiveSpan sink;unsigned start=0u,i,run=0u;
    Pi_SetUp();traceCount=remarkSite=0u;
    if(!prepare(&f,entry,w,&cache,&sentinel)||!Pix_Capture(&f,&before)||
       !ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,f.base.remarks.items,1u,sizeof(*f.base.remarks.items),&sink))goto done;
    start=ZrTests_ExecOwner_Stats().attempts;run=(unsigned)Pix_Run(&f,entry,w);
    if(!events(start,trace,&traceCount)||!traceCount)goto done;
    post=Pix_Verify(&f,w);
    r.safety=run&&post.core&&post.parser&&Pix_Partial(&f,&before,entry);
    r.feature=r.safety?(unsigned)Pix_Expected(&f,&before,entry):0u;
    for(i=0u;i<traceCount;++i)if(trace[i].kind==3u&&trace[i].record==sink.record&&!remarkSite)remarkSite=i+1u;
    if(entry!=0u&&!remarkSite)r.safety=0u;
    if(entry==2u){if(prefixCount>=traceCount)r.safety=0u;
        for(i=0u;i<prefixCount&&i<traceCount;++i)if(!event_same(prefix+i,trace+i))r.safety=0u;}
done:
    r.safety&=(unsigned)Pi_Dispose(&f.base);
    printf("PHI_IDENTITY_TRACE id=%u entry=%u writable=%u measured=%u prefix=%u remark_site=%u bound=%u run=%u post=%u safety=%u feature=%u\n",
        id,entry,w,traceCount,entry==2u?prefixCount:0u,remarkSite,PIX_TRACE,run,post.core&&post.parser,r.safety,r.feature);
    return r;
}
SPiResult Pix_Transactions(unsigned id) {
    SPiResult all={1u,1u};unsigned entry=id-18u,w,site;
    for(w=0u;w<2u;++w){unsigned late=0u,remark=0u;
        SPiResult measured;
        if(entry==2u&&!first_function_prefix(w)){all.safety=0u;continue;}
        measured=measure(id,entry,w);all.safety&=measured.safety;all.feature&=measured.feature;
        if(!measured.safety)continue;
        for(site=1u;site<=traceCount;++site){
            SPixFixture f;SPixSnapshot before;SPixCacheCopy cache;SPixOwners oldOwners;
            SZrExecIrOptimizationRemark sentinel;SZrExecIrPassContext oldContext;
            SZrOwnerHeapEvent event={0};SPixChecks post={0};SPiResult r={0u,0u};
            unsigned start=0u,run=1u,hit=0u,traceOk=1u,unchanged=0u,ownerOk=0u,cacheOk=0u,contextOk=0u;
            unsigned prefixOk=0u,resultOk=0u,retry=0u,expected=0u,code=0u,token=0u,j,isLate=0u,isRemark=0u,released;
            Pi_SetUp();
            if(!prepare(&f,entry,w,&cache,&sentinel)||!Pix_Capture(&f,&before)||!owners(&f,&oldOwners))goto done;
            oldContext=f.base.context;start=ZrTests_ExecOwner_Stats().attempts;
            if(!ZrTests_ExecOwner_TryArm(site))goto done;
            run=(unsigned)Pix_Run(&f,entry,w);ZrTests_ExecOwner_Disarm();
            if(ZrTests_ExecOwner_TryEvent(start+site-1u,&event))hit=event.failed;
            for(j=0u;j<site;++j){SZrOwnerHeapEvent actual;
                if(!ZrTests_ExecOwner_TryEvent(start+j,&actual)||!event_same(&actual,trace+j))traceOk=0u;}
            if(w){code=f.base.diagnostic.code;token=f.base.diagnostic.functionToken;}
            post=Pix_Verify(&f,w);
            if(post.core&&post.parser){
                unchanged=(unsigned)Pix_Equal(&f,&before);
                ownerOk=entry==1u?1u:(unsigned)owners_equal(&f,&oldOwners);
                /* Direct SCCP may already have replaced/published cacheValues.
                 * Only manager FALSE promises the seeded cache contents back. */
                cacheOk=entry==1u?(unsigned)cache_equal(&f,&cache):(unsigned)Pix_Live(&f);
                contextOk=entry==1u?(unsigned)(memcmp(&f.base.context,&oldContext,sizeof(oldContext))==0):1u;
                prefixOk=(unsigned)remark_prefix(&f,&sentinel);
                resultOk=entry==2u?(unsigned)zero_result(&f.base.result):1u;
            }
            r.safety=hit&&traceOk&&!run&&post.core&&post.parser&&unchanged&&ownerOk&&cacheOk&&contextOk&&prefixOk&&resultOk;
            if(w)r.safety&=(code==ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
            isLate=entry==2u&&site>prefixCount&&w&&token==ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF,92u);
            isRemark=entry!=0u&&site==remarkSite&&event.kind==3u;late+=isLate;remark+=isRemark;
            if(post.core&&post.parser){
                retry=(unsigned)Pix_Run(&f,entry,w);post=Pix_Verify(&f,w);
                retry&=post.core&&post.parser&&Pix_Partial(&f,&before,entry);
                expected=retry?(unsigned)Pix_Expected(&f,&before,entry):0u;
            }
            r.safety&=retry;r.feature=expected;
done:
            released=(unsigned)Pi_DisposeFault(&f.base,1u);r.safety&=released;
            Pi_NoteFault(1u,r.safety?1u:0u);
            printf("PHI_IDENTITY_FAULT id=%u entry=%u writable=%u site=%u measured=%u triggered=%u trace=%u run=%u owner=%u unchanged=%u cache=%u context=%u prefix=%u resultzero=%u verify=%u retry=%u expected=%u released=%u late=%u remark=%u code=%u token=%u safety=%u feature=%u\n",
                id,entry,w,site,traceCount,hit,traceOk,run,ownerOk,unchanged,cacheOk,contextOk,prefixOk,resultOk,
                post.core&&post.parser,retry,expected,released,isLate,isRemark,code,token,r.safety,r.feature);
            all.safety&=r.safety;all.feature&=r.feature;
        }
        if(entry!=0u&&!remark)all.safety=0u;
        if(entry==2u&&w&&!late)all.safety=0u;
        printf("PHI_IDENTITY_FAULT_SUMMARY id=%u entry=%u writable=%u measured=%u remark_hits=%u late_hits=%u safety=%u feature=%u\n",
            id,entry,w,traceCount,remark,late,all.safety,all.feature);
    }
    return all;
}
