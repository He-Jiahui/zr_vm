#include <stdlib.h>
#include <string.h>
#include "ssa_phi_identity_support.h"

static int pool(const void *p,unsigned n,unsigned cap,size_t width) {
    SZrOwnerLiveSpan span;
    return p?cap&&n<=cap&&ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,p,cap,width,&span):!n&&!cap;
}
int Pix_Live(const SPixFixture *f) {
    const SZrExecIrModule *m=&f->base.module;unsigned i;
#define P(o,p,n,c) do { if(!pool((o)->p,(o)->n,(o)->c,sizeof(*(o)->p)))return 0; } while(0)
    P(m,functions,functionCount,functionCapacity);P(m,constants,constantCount,constantCapacity);
    P(m,layouts,layoutCount,layoutCapacity);P(m,sourceMaps,sourceMapCount,sourceMapCapacity);
    if(m->functionCount!=f->functions||!m->functionCount||m->functionCount>PIX_FUNCTIONS)return 0;
    for(i=0u;i<m->functionCount;++i){const SZrExecIrFunction *fn=m->functions+i;
        P(fn,values,valueCount,valueCapacity);P(fn,instructions,instructionCount,instructionCapacity);
        P(fn,blocks,blockCount,blockCapacity);P(fn,operands,operandCount,operandCapacity);
        P(fn,results,resultCount,resultCapacity);P(fn,memoryTokenPool,memoryTokenCount,memoryTokenCapacity);
        P(fn,phiPool,phiCount,phiCapacity);P(fn,phiIncoming,phiIncomingCount,phiIncomingCapacity);
        P(fn,predecessors,predecessorCount,predecessorCapacity);P(fn,successors,successorCount,successorCapacity);
        P(fn,gcRoots,gcRootCount,gcRootCapacity);P(fn,deoptStates,deoptStateCount,deoptStateCapacity);
        P(fn,deoptValues,deoptValueCount,deoptValueCapacity);P(fn,deoptAggregates,deoptAggregateCount,deoptAggregateCapacity);
        P(fn,deoptAggregateFields,deoptAggregateFieldCount,deoptAggregateFieldCapacity);
        P(fn,sourceMaps,sourceMapCount,sourceMapCapacity);P(fn,bindingRows,bindingRowCount,bindingRowCapacity);
        if(fn->frameLayout){const SZrExecIrFrameLayout *h=fn->frameLayout;
            if(!pool(h,1u,1u,sizeof(*h)))return 0;P(h,slots,slotCount,slotCapacity);}
        if(fn->gcMap){const SZrExecIrGcMap *h=fn->gcMap;
            if(fn->gcMapCount!=1u||fn->gcMapCapacity!=1u||!pool(h,1u,1u,sizeof(*h)))return 0;
            P(h,entries,entryCount,entryCapacity);P(h,slotIndexPool,slotIndexCount,slotIndexCapacity);
            P(h,inlineRefOffsetPool,inlineRefOffsetCount,inlineRefOffsetCapacity);
        }else if(fn->gcMapCount||fn->gcMapCapacity)return 0;
        if(fn->stateMap){const SZrExecIrStateMap *h=fn->stateMap;
            if(!pool(h,1u,1u,sizeof(*h)))return 0;
            P(h,entries,entryCount,entryCapacity);P(h,valuePool,valueCount,valueCapacity);
            P(h,rootPool,rootCount,rootCapacity);P(h,ownerStatePool,ownerStateCount,ownerStateCapacity);}
    }
    P((&f->base.remarks),items,count,capacity);
#undef P
    return pool(f->base.cache.sccpValues,f->base.cache.sccpValueCount,f->base.cache.sccpValueCount,sizeof(*f->base.cache.sccpValues));
}
SPixChecks Pix_Verify(SPixFixture *f,unsigned writable) {
    SPixChecks r={0u,0u};unsigned i;SZrExecIrDiagnostic *d=writable?&f->base.diagnostic:NULL;
    if(!Pix_Live(f))return r;
    r.core=1u;
    for(i=0u;i<f->functions;++i)r.core&=(unsigned)ZrCore_ExecIr_VerifyFunction(f->base.module.functions+i,ZR_EXEC_IR_VERIFY_ALL,d);
    r.core&=(unsigned)ZrCore_ExecIr_VerifyModule(&f->base.module,d);
    /* This Parser call delegates Core VerifyModule; it is not a second implementation. */
    r.parser=(unsigned)ZrParser_ExecIr_Verify(&f->base.module,d);return r;
}
static TZrUInt64 mix(TZrUInt64 h,TZrUInt64 n) { return (h^n)*UINT64_C(1099511628211); }
TZrUInt64 Pix_Hash(const SPixFixture *f) {
    unsigned i;TZrUInt64 h=UINT64_C(1469598103934665603);
    h=mix(h,f->base.module.moduleHash);h=mix(h,f->base.module.functionCount);
    for(i=0u;i<f->functions;++i)h=mix(h,ZrParser_ExecIr_FunctionHash(f->base.module.functions+i));
    return h;
}
static int bytes(unsigned char *out,unsigned *used,const void *p,size_t n) {
    if(n>PIX_META-*used)return 0;if(n)memcpy(out+*used,p,n);*used+=(unsigned)n;return 1;
}
#define CLEAR(o,p,c) do { (o).p=NULL;(o).c=0u; } while(0)
static SZrExecIrFunction header(const SZrExecIrFunction *fn) {
    SZrExecIrFunction h=*fn;
    CLEAR(h,values,valueCapacity);CLEAR(h,instructions,instructionCapacity);CLEAR(h,blocks,blockCapacity);
    CLEAR(h,operands,operandCapacity);CLEAR(h,results,resultCapacity);CLEAR(h,memoryTokenPool,memoryTokenCapacity);
    CLEAR(h,phiPool,phiCapacity);CLEAR(h,phiIncoming,phiIncomingCapacity);CLEAR(h,predecessors,predecessorCapacity);
    CLEAR(h,successors,successorCapacity);CLEAR(h,gcRoots,gcRootCapacity);CLEAR(h,deoptStates,deoptStateCapacity);
    CLEAR(h,deoptValues,deoptValueCapacity);CLEAR(h,deoptAggregates,deoptAggregateCapacity);
    CLEAR(h,deoptAggregateFields,deoptAggregateFieldCapacity);CLEAR(h,sourceMaps,sourceMapCapacity);
    CLEAR(h,bindingRows,bindingRowCapacity);CLEAR(h,gcMap,gcMapCapacity);h.stateMap=NULL;h.frameLayout=NULL;return h;
}
static int metadata(const SZrExecIrFunction *fn,SPixFunctionSnapshot *s) {
    unsigned present[3]={fn->frameLayout!=NULL,fn->gcMap!=NULL,fn->stateMap!=NULL};
#define B(p,n) do { if(!bytes(s->metadata,&s->metaSize,(p),(size_t)(n)*sizeof(*(p))))return 0; } while(0)
    B(present,3u);B(fn->gcRoots,fn->gcRootCount);B(fn->deoptStates,fn->deoptStateCount);
    B(fn->deoptValues,fn->deoptValueCount);B(fn->deoptAggregates,fn->deoptAggregateCount);
    B(fn->deoptAggregateFields,fn->deoptAggregateFieldCount);B(fn->bindingRows,fn->bindingRowCount);
    if(fn->frameLayout){SZrExecIrFrameLayout h=*fn->frameLayout;CLEAR(h,slots,slotCapacity);
        B((&h),1u);B(fn->frameLayout->slots,fn->frameLayout->slotCount);}
    if(fn->gcMap){const SZrExecIrGcMap *m=fn->gcMap;SZrExecIrGcMap h=*m;
        CLEAR(h,entries,entryCapacity);CLEAR(h,slotIndexPool,slotIndexCapacity);CLEAR(h,inlineRefOffsetPool,inlineRefOffsetCapacity);
        B((&h),1u);B(m->entries,m->entryCount);B(m->slotIndexPool,m->slotIndexCount);B(m->inlineRefOffsetPool,m->inlineRefOffsetCount);}
    if(fn->stateMap){const SZrExecIrStateMap *m=fn->stateMap;SZrExecIrStateMap h=*m;
        CLEAR(h,entries,entryCapacity);CLEAR(h,valuePool,valueCapacity);CLEAR(h,rootPool,rootCapacity);CLEAR(h,ownerStatePool,ownerStateCapacity);
        B((&h),1u);B(m->entries,m->entryCount);B(m->valuePool,m->valueCount);B(m->rootPool,m->rootCount);B(m->ownerStatePool,m->ownerStateCount);}
#undef B
    return 1;
}
int Pix_Capture(const SPixFixture *f,SPixSnapshot *s) {
    const SZrExecIrModule *m=&f->base.module;unsigned i;
    if(!Pix_Live(f))return 0;memset(s,0,sizeof(*s));s->header=*m;
    CLEAR(s->header,functions,functionCapacity);CLEAR(s->header,constants,constantCapacity);
    CLEAR(s->header,layouts,layoutCapacity);CLEAR(s->header,sourceMaps,sourceMapCapacity);
    if(!bytes(s->metadata,&s->metaSize,m->constants,(size_t)m->constantCount*sizeof(*m->constants))||
       !bytes(s->metadata,&s->metaSize,m->layouts,(size_t)m->layoutCount*sizeof(*m->layouts))||
       !bytes(s->metadata,&s->metaSize,m->sourceMaps,(size_t)m->sourceMapCount*sizeof(*m->sourceMaps)))return 0;
    for(i=0u;i<f->functions;++i){const SZrExecIrFunction *fn=m->functions+i;SPixFunctionSnapshot *o=s->fn+i;
        if(fn->valueCount>PIX_VALUES||fn->blockCount>PIX_BLOCKS||fn->instructionCount>PIX_INST||fn->phiCount>PIX_PHIS||
           fn->phiIncomingCount>PIX_POOL||fn->operandCount>PIX_POOL||fn->resultCount>PIX_POOL||fn->memoryTokenCount>PIX_POOL||
           fn->predecessorCount>PIX_POOL||fn->successorCount>PIX_POOL||fn->sourceMapCount>PIX_POOL)return 0;
        o->header=header(fn);
#define COPY(dst,p,n) do { if(fn->n)memcpy(o->dst,fn->p,(size_t)fn->n*sizeof(*fn->p)); } while(0)
        COPY(values,values,valueCount);COPY(blocks,blocks,blockCount);COPY(instructions,instructions,instructionCount);
        COPY(phis,phiPool,phiCount);COPY(incoming,phiIncoming,phiIncomingCount);COPY(operands,operands,operandCount);
        COPY(results,results,resultCount);COPY(memory,memoryTokenPool,memoryTokenCount);COPY(pred,predecessors,predecessorCount);
        COPY(succ,successors,successorCount);COPY(sources,sourceMaps,sourceMapCount);
#undef COPY
        if(!metadata(fn,o))return 0;
    }
    s->hash=Pix_Hash(f);return 1;
}
#undef CLEAR
static int same_fn(const SPixFunctionSnapshot *a,const SPixFunctionSnapshot *b) {
    if(memcmp(&a->header,&b->header,sizeof(a->header))||a->metaSize!=b->metaSize||memcmp(a->metadata,b->metadata,a->metaSize))return 0;
#define EQ(p,n) do { if(memcmp(a->p,b->p,(size_t)a->header.n*sizeof(*a->p)))return 0; } while(0)
    EQ(values,valueCount);EQ(instructions,instructionCount);EQ(blocks,blockCount);EQ(phis,phiCount);
    EQ(incoming,phiIncomingCount);EQ(operands,operandCount);EQ(results,resultCount);EQ(memory,memoryTokenCount);
    EQ(pred,predecessorCount);EQ(succ,successorCount);EQ(sources,sourceMapCount);
#undef EQ
    return 1;
}
static int same(const SPixSnapshot *a,const SPixSnapshot *b) {
    unsigned i;if(memcmp(&a->header,&b->header,sizeof(a->header))||a->metaSize!=b->metaSize||memcmp(a->metadata,b->metadata,a->metaSize))return 0;
    for(i=0u;i<a->header.functionCount;++i)if(!same_fn(a->fn+i,b->fn+i))return 0;return 1;
}
int Pix_Equal(const SPixFixture *f,const SPixSnapshot *s) { SPixSnapshot now;return Pix_Capture(f,&now)&&same(&now,s); }
static unsigned contains(const unsigned *values,unsigned count,unsigned value) {
    unsigned i;for(i=0u;i<count;++i)if(values[i]==value)return 1u;return 0u;
}
static void nop(SZrExecIrInstruction *in) {
    in->opcode=ZR_EXEC_IR_OPCODE_NOP;in->flags=0u;
    in->results=(SZrExecIrRange){0};in->operands=(SZrExecIrRange){0};in->phiRange=(SZrExecIrRange){0};
    in->successorRange=(SZrExecIrRange){0};in->memoryIn=(SZrExecIrRange){0};in->memoryOut=(SZrExecIrRange){0};
    in->effectIn=in->effectOut=in->matchTypeToken=in->layoutId=0u;
}
static void transform(SPixFunctionSnapshot *o,const SPixPlan *plan,unsigned sccp,unsigned dce) {
    unsigned i,n,old=o->header.phiCount,prefix[PIX_PHIS+1u];prefix[0]=0u;
    if(sccp){
        for(i=0u;i<plan->patches;++i)o->operands[plan->operand[i].slot]=plan->operand[i].value;
        for(i=0u;i<plan->incomingPatches;++i)o->incoming[plan->incoming[i].slot].value=plan->incoming[i].value;
    }
    for(i=0u;i<old;++i)prefix[i+1u]=prefix[i]+!(dce&&contains(plan->remove,plan->removed,o->phis[i].result));
    if(dce){
        for(i=0u;i<o->header.blockCount;++i){SZrExecIrRange r=o->blocks[i].phis;
            o->blocks[i].phis.start=prefix[r.start];o->blocks[i].phis.count=prefix[r.start+r.count]-prefix[r.start];}
        for(i=0u;i<old;++i)if(prefix[i+1u]!=prefix[i])o->phis[prefix[i]]=o->phis[i];
        o->header.phiCount=prefix[old];
        for(i=0u;i<plan->erased;++i)nop(o->instructions+plan->erase[i]-1u);
        for(i=0u,n=0u;i<o->header.sourceMapCount;++i)
            if(!contains(plan->erase,plan->erased,o->sources[i].instructionId))o->sources[n++]=o->sources[i];
        o->header.sourceMapCount=n;
    }
}
int Pix_Expected(const SPixFixture *f,const SPixSnapshot *s,unsigned entry) {
    SPixSnapshot now,expected=*s;unsigned i,b,sccp,dce;
    if(!Pix_Capture(f,&now))return 0;
    sccp=entry!=2u||!(f->base.options.disabledPassMask&ZR_EXEC_IR_OPTIMIZE_DISABLE_SCCP);
    dce=entry!=0u&&!(f->base.options.disabledPassMask&ZR_EXEC_IR_OPTIMIZE_DISABLE_DCE);
    for(i=0u;i<f->functions;++i){
        transform(expected.fn+i,f->plan+i,sccp,dce&&sccp);
        if(entry!=0u&&sccp)for(b=0u;b<expected.fn[i].header.blockCount;++b)
            expected.fn[i].blocks[b].immediateDominator=f->plan[i].dom[b];
    }
    if(same(&now,&expected))return 1;
    if(f->optional){ /* Only this explicitly flagged shared-slot fixture may skip. */
        expected=*s;
        if(dce){SPixPlan skip=f->plan[0];skip.patches=skip.incomingPatches=skip.removed=0u;
            transform(expected.fn,&skip,0u,1u);}
        if(entry!=0u&&sccp)for(b=0u;b<expected.fn[0].header.blockCount;++b)expected.fn[0].blocks[b].immediateDominator=f->plan[0].dom[b];
        return same(&now,&expected);
    }
    return 0;
}
/* A budget may stop after any completed legal store. Compare exact bytes to
 * the original plus a finite explicit slot/removal whitelist, never a solver. */
int Pix_Partial(const SPixFixture *f,const SPixSnapshot *s,unsigned entry) {
    SPixSnapshot now,allowed=*s;unsigned fi,i,j,b;
    if(!Pix_Capture(f,&now))return 0;
    for(fi=0u;fi<f->functions;++fi){SPixFunctionSnapshot *a=allowed.fn+fi,*n=now.fn+fi;const SPixPlan *p=f->plan+fi;
        for(i=0u;i<p->patches;++i){unsigned slot=p->operand[i].slot;if(n->operands[slot]==p->operand[i].value)a->operands[slot]=p->operand[i].value;}
        for(i=0u;i<p->incomingPatches;++i){unsigned slot=p->incoming[i].slot;if(n->incoming[slot].value==p->incoming[i].value)a->incoming[slot].value=p->incoming[i].value;}
        if(entry!=0u){SPixPlan completed=*p;completed.removed=completed.erased=0u;
            for(i=0u;i<p->removed;++i){unsigned found=0u;
                for(j=0u;j<n->header.phiCount;++j)if(n->phis[j].result==p->remove[i])found=1u;
                if(!found)completed.remove[completed.removed++]=p->remove[i];}
            for(i=0u;i<p->erased;++i)if(n->instructions[p->erase[i]-1u].opcode==ZR_EXEC_IR_OPCODE_NOP)
                completed.erase[completed.erased++]=p->erase[i];
            transform(a,&completed,0u,1u);
            {unsigned originalDom=1u,computedDom=1u;
                unsigned mayCompute=entry==1u||!(f->base.options.disabledPassMask&ZR_EXEC_IR_OPTIMIZE_DISABLE_SCCP);
                for(b=0u;b<a->header.blockCount;++b){
                    originalDom&=(n->blocks[b].immediateDominator==a->blocks[b].immediateDominator);
                    computedDom&=(n->blocks[b].immediateDominator==p->dom[b]);
                }
                if(!originalDom&&(!mayCompute||!computedDom))return 0;
                if(mayCompute&&computedDom)for(b=0u;b<a->header.blockCount;++b)a->blocks[b].immediateDominator=p->dom[b];
            }
        }
    }
    return same(&now,&allowed);
}
