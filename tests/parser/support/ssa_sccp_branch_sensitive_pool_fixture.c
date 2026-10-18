#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "ssa_sccp_branch_sensitive_pool_fixture.h"
#include "ssa_exec_ir_owned_module_shim.h"
static SBeTotals g_totals;
void Be_SetUp(void) { ZrTests_ExecOwner_Reset(); }
void Be_TearDown(void) { ZrTests_ExecOwner_Disarm(); }
SBeTotals Be_Totals(void) { return g_totals; }
void Be_NoteFault(unsigned a,unsigned b) { g_totals.faultMeasured+=a;g_totals.faultCompleted+=b; }
static TZrUInt64 be_mix(TZrUInt64 h,TZrUInt64 v) { return (h^v)*UINT64_C(1099511628211); }
TZrUInt64 Be_Hash(const SBeFixture *f) {
    unsigned i;TZrUInt64 h=UINT64_C(1469598103934665603);
    h=be_mix(h,f->module.functionCount);h=be_mix(h,f->module.constantCount);
    for(i=0;i<f->module.constantCount;++i){const SZrExecIrConstant *c=f->module.constants+i;
        h=be_mix(be_mix(be_mix(h,c->typeToken),c->flags),c->bits);}
    for(i=0;i<f->module.functionCount;++i)h=be_mix(h,ZrParser_ExecIr_FunctionHash(f->module.functions+i));
    return h;
}
/* The CFG witness excludes computed dominators and COPY operand aliases. It
 * includes ordered duplicate edge occurrences, PHI metadata and effect fields. */
static TZrUInt64 be_cfg(const SBeFixture *f) {
    unsigned n,i,j;TZrUInt64 h=UINT64_C(1469598103934665603);
    for(n=0;n<f->module.functionCount;++n){const SZrExecIrFunction *fn=f->module.functions+n;
        h=be_mix(h,fn->blockCount);h=be_mix(h,fn->instructionCount);
        for(i=0;i<fn->blockCount;++i){const SZrExecIrBlock *b=fn->blocks+i;
            h=be_mix(be_mix(be_mix(h,b->id),b->instructions.start),b->instructions.count);
            h=be_mix(h,b->terminatorInstructionId);
            h=be_mix(h,b->predecessors.count);h=be_mix(h,b->successors.count);
            for(j=0;j<b->predecessors.count;++j)h=be_mix(h,fn->predecessors[b->predecessors.start+j]);
            for(j=0;j<b->successors.count;++j)h=be_mix(h,fn->successors[b->successors.start+j]);
            h=be_mix(h,b->phis.count);
            for(j=0;j<b->phis.count;++j){unsigned k;const SZrExecIrPhi *p=fn->phiPool+b->phis.start+j;
                h=be_mix(h,p->result);h=be_mix(h,p->incomings.count);
                for(k=0;k<p->incomings.count;++k){const SZrExecIrPhiIncoming *v=fn->phiIncoming+p->incomings.start+k;
                    h=be_mix(be_mix(h,v->predecessor),v->value);}}
        }
        for(i=0;i<fn->instructionCount;++i){const SZrExecIrInstruction *in=fn->instructions+i;
            h=be_mix(be_mix(h,in->effectIn),in->effectOut);
            const SZrExecIrInstruction *witness=in;unsigned q;
            for(q=0u;q<f->compareCount;++q)
                if(f->compares[q].function==n&&f->compares[q].instruction==i)
                    witness=&f->compares[q].original;
            if(witness->opcode==ZR_EXEC_IR_OPCODE_COMPARE||in->opcode==ZR_EXEC_IR_OPCODE_DIV){
                h=be_mix(be_mix(be_mix(h,witness->opcode),witness->flags),witness->typeToken);
                h=be_mix(h,witness->operands.count);
                if(in->opcode==ZR_EXEC_IR_OPCODE_DIV)
                    for(j=0;j<in->operands.count;++j)h=be_mix(h,fn->operands[in->operands.start+j]);}
        }
    }return h;
}
static int be_owner(SBeSnapshot *s,const void *p,unsigned n,unsigned cap,size_t width) {
    SBeOwner *o;SZrOwnerLiveSpan span;unsigned records=ZrTests_ExecOwner_Stats().records;
    if(s->count==BE_OWNER_LIMIT)return 0;o=s->owners+s->count++;
    o->address=(uintptr_t)p;o->count=n;o->capacity=cap;o->record=UINT32_MAX;
    if(!p)return !n&&!cap;
    if(!cap||n>cap||!ZrTests_ExecOwner_TryLiveSpan(0u,records,p,cap,width,&span))return 0;
    o->record=span.record;return 1;
}
static int be_inventory(const SBeFixture *f,SBeSnapshot *s) {
    unsigned i;memset(s,0,sizeof(*s));
#define BO(p,n,c) if(!be_owner(s,(p),(n),(c),sizeof(*(p))))return 0
    BO(f->module.functions,f->module.functionCount,f->module.functionCapacity);
    BO(f->module.constants,f->module.constantCount,f->module.constantCapacity);
    BO(f->module.layouts,f->module.layoutCount,f->module.layoutCapacity);
    BO(f->module.sourceMaps,f->module.sourceMapCount,f->module.sourceMapCapacity);
    for(i=0;i<f->module.functionCount;++i){const SZrExecIrFunction *fn=f->module.functions+i;
#define BF(p,n,c) BO(fn->p,fn->n,fn->c)
        BF(values,valueCount,valueCapacity);BF(instructions,instructionCount,instructionCapacity);
        BF(blocks,blockCount,blockCapacity);BF(operands,operandCount,operandCapacity);BF(results,resultCount,resultCapacity);
        BF(memoryTokenPool,memoryTokenCount,memoryTokenCapacity);BF(phiPool,phiCount,phiCapacity);
        BF(phiIncoming,phiIncomingCount,phiIncomingCapacity);BF(predecessors,predecessorCount,predecessorCapacity);
        BF(successors,successorCount,successorCapacity);BF(gcMap,gcMapCount,gcMapCapacity);BF(gcRoots,gcRootCount,gcRootCapacity);
        BF(deoptStates,deoptStateCount,deoptStateCapacity);BF(deoptValues,deoptValueCount,deoptValueCapacity);
        BF(deoptAggregates,deoptAggregateCount,deoptAggregateCapacity);BF(deoptAggregateFields,deoptAggregateFieldCount,deoptAggregateFieldCapacity);
        BF(sourceMaps,sourceMapCount,sourceMapCapacity);BF(bindingRows,bindingRowCount,bindingRowCapacity);
        if(fn->frameLayout||fn->stateMap||fn->gcMap)return 0;
#undef BF
    }
#undef BO
    return 1;
}
int Be_Snapshot(const SBeFixture *f,SBeSnapshot *s) { if(!be_inventory(f,s))return 0;s->hash=Be_Hash(f);return 1; }
int Be_OwnersEqual(const SBeFixture *f,const SBeSnapshot *s) {
    SBeSnapshot now;unsigned i,freed;
    if(!be_inventory(f,&now)||s->count!=now.count)return 0;
    for(i=0;i<s->count;++i){const SBeOwner *a=s->owners+i,*b=now.owners+i;
        if(a->address!=b->address||a->count!=b->count||a->capacity!=b->capacity||a->record!=b->record)return 0;
        if(a->record!=UINT32_MAX&&(!ZrTests_ExecOwner_TryRecordFrees(a->record,&freed)||freed))return 0;}
    return 1;
}
static unsigned be_pool(SBeFixture *f,unsigned type,TZrUInt64 bits) {
    SZrExecIrConstant c={type,0u,bits};SZrExecIrRange r;
    return ZrCore_ExecIr_ModuleAppendConstant(&f->module,&c,1u,&r)?r.start:UINT32_MAX;
}
static unsigned be_emit(SBeFixture *f,SZrExecIrFunction *fn,unsigned op,unsigned type,
        const unsigned *args,unsigned count,unsigned layout,unsigned flags) {
    SZrExecIrInstruction in;TZrExecIrInstructionId id;unsigned value=0;
    memset(&in,0,sizeof(in));in.opcode=(TZrUInt16)op;in.typeToken=type;in.layoutId=layout;
    in.flags=(TZrUInt16)flags;in.sourceId=f->source++;
    if(flags&ZR_EXEC_IR_FLAG_MAY_THROW){in.effectIn=1u;in.effectOut=2u;}
    if(op!=ZR_EXEC_IR_OPCODE_RETURN&&op!=ZR_EXEC_IR_OPCODE_BRANCH&&op!=ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH){
        value=ZrCore_ExecIr_FunctionAddValue(fn,type,ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        if(!value||!ZrCore_ExecIr_FunctionAppendResults(fn,&value,1u,&in.results))return 0;}
    if(count&&!ZrCore_ExecIr_FunctionAppendOperands(fn,args,count,&in.operands))return 0;
    if(!ZrCore_ExecIr_FunctionAppendInstruction(fn,&in,&id))return 0;return value?value:id;
}
static unsigned be_const(SBeFixture *f,SZrExecIrFunction *fn,unsigned type,TZrUInt64 bits,unsigned empty) {
    unsigned p=empty?(unsigned)bits:be_pool(f,type,bits);
    return p==UINT32_MAX?0u:be_emit(f,fn,ZR_EXEC_IR_OPCODE_CONSTANT,type,NULL,0u,p,0u);
}
static unsigned be_ext(SZrExecIrFunction *fn,unsigned type) {
    return ZrCore_ExecIr_FunctionAddExternalValue(fn,type,ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN);
}
static int be_preds(SZrExecIrFunction *fn,unsigned block,unsigned a,unsigned b,unsigned count) {
    unsigned p[2]={a,b};return ZrCore_ExecIr_FunctionAppendPredecessors(fn,p,count,&fn->blocks[block-1u].predecessors);
}
static unsigned be_phi(SZrExecIrFunction *fn,unsigned block,unsigned type,unsigned pa,unsigned a,unsigned pb,unsigned b) {
    SZrExecIrPhi p;SZrExecIrPhiIncoming v[2]={{pa,a},{pb,b}};
    p.result=ZrCore_ExecIr_FunctionAddValue(fn,type,ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    if(!p.result||!ZrCore_ExecIr_FunctionAppendPhiIncoming(fn,v,2u,&p.incomings)||
            !ZrCore_ExecIr_FunctionAppendPhis(fn,&p,1u,&fn->blocks[block-1u].phis))return 0;
    return p.result;
}
static int be_jump(SBeFixture *f,SZrExecIrFunction *fn,unsigned block,unsigned start,unsigned cond,unsigned a,unsigned b,unsigned count) {
    unsigned edges[2]={a,b};SZrExecIrRange r;
    unsigned id=be_emit(f,fn,cond?ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:ZR_EXEC_IR_OPCODE_BRANCH,0u,cond?&cond:NULL,cond?1u:0u,0u,0u);
    if(!id||!ZrCore_ExecIr_FunctionAppendSuccessors(fn,edges,count,&r))return 0;
    fn->instructions[id-1u].successorRange=r;fn->blocks[block-1u].successors=r;
    fn->blocks[block-1u].instructions=(SZrExecIrRange){.start=start,.count=fn->instructionCount-start};
    fn->blocks[block-1u].terminatorInstructionId=id;return 1;
}
static int be_finish(SBeFixture *f,SZrExecIrFunction *fn,unsigned block,unsigned a,unsigned two,unsigned fold,TZrUInt64 bits,unsigned flags) {
    unsigned start=fn->instructionCount,args[2]={a,two},v;
    SBeTarget *t;if(f->targetCount==BE_TARGET_LIMIT)return 0;
    v=be_emit(f,fn,ZR_EXEC_IR_OPCODE_ADD,ZR_VALUE_TYPE_INT64,args,2u,0u,flags);
    if(!v)return 0;t=f->targets+f->targetCount++;t->function=fn->id-1u;t->fold=fold;t->bits=bits;
    t->original=fn->instructions[fn->values[v-1u].definition-1u];t->source=t->original.sourceId;
    if(!be_emit(f,fn,ZR_EXEC_IR_OPCODE_RETURN,0u,&v,1u,0u,0u))return 0;
    fn->blocks[block-1u].instructions=(SZrExecIrRange){.start=start,.count=fn->instructionCount-start};
    fn->blocks[block-1u].terminatorInstructionId=fn->instructionCount;return 1;
}
static int be_function(SBeFixture *f,unsigned id,unsigned profile,unsigned empty) {
    TZrExecIrFunctionId fid;SZrExecIrFunction *fn;unsigned blocks=4u,b,start,c,a,z,two,truth,falsity,ext,p;
    unsigned selected=id==2u?0u:1u,fold=1u,nested=id==11u||id==13u||id==15u||(id==10u&&profile<2u),loop=id==16u;
    unsigned duplicate=id==10u&&profile>=2u,origin=ZR_VALUE_TYPE_INT64;
    if(id>=3u&&id<=8u)selected=profile==0u;
    if(id==9u||id==11u||id==16u)selected=profile==0u;
    if(id==10u)selected=profile==0u||profile==2u;
    if(id==12u||id==13u||id==14u||id==15u||(loop&&profile==1u)||(duplicate&&profile==4u))fold=0u;
    if(nested)blocks=7u;if(loop)blocks=5u;if(duplicate)blocks=2u;
    if(!ZrCore_ExecIr_ModuleAddFunction(&f->module,ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF,101u+f->module.functionCount),UINT64_C(0x424f4f4c),&fid))return 0;
    fn=ZrCore_ExecIr_ModuleFunctionAt(&f->module,fid);if(!fn)return 0;
    for(b=1;b<=blocks;++b)if(ZrCore_ExecIr_FunctionAddBlock(fn,b==1u?ZR_EXEC_IR_BLOCK_FLAG_ENTRY:0u)!=b)return 0;
    a=be_const(f,fn,ZR_VALUE_TYPE_INT64,40u,empty);z=be_const(f,fn,ZR_VALUE_TYPE_INT64,50u,empty);
    two=be_const(f,fn,ZR_VALUE_TYPE_INT64,2u,empty);truth=be_const(f,fn,ZR_VALUE_TYPE_BOOL,1u,empty);
    falsity=be_const(f,fn,ZR_VALUE_TYPE_BOOL,0u,empty);ext=be_ext(fn,ZR_VALUE_TYPE_BOOL);
    if(!a||!z||!two||!truth||!falsity||!ext)return 0;c=selected?truth:falsity;
    if(id>=3u&&id<=8u){unsigned args[2],kind=id-3u;
        TZrUInt64 lo=UINT64_C(0x8000000000000000),hi=UINT64_C(0x7fffffffffffffff),left=lo,right=hi;
        if(kind==0u){left=hi;right=selected?hi:lo;}
        else if(kind==1u){left=selected?lo:hi;right=selected?hi:lo;}
        else if(kind==2u){left=selected?lo:hi;right=lo;}
        else if(kind==3u){left=selected?hi:lo;right=selected?lo:hi;}
        else if(kind==4u){left=selected?hi:lo;right=hi;}
        else {right=selected?hi:lo;}
        args[0]=be_const(f,fn,ZR_VALUE_TYPE_INT64,left,empty);args[1]=be_const(f,fn,ZR_VALUE_TYPE_INT64,right,empty);
        c=be_emit(f,fn,ZR_EXEC_IR_OPCODE_COMPARE,ZR_VALUE_TYPE_BOOL,args,2u,0u,0u);
        if(!c)return 0;fn->instructions[fn->values[c-1u].definition-1u].typeToken=kind;
        if(f->compareCount==BE_TARGET_LIMIT)return 0;
        {SBeCompare *w=f->compares+f->compareCount++;
            w->function=fn->id-1u;w->instruction=fn->values[c-1u].definition-1u;
            w->expected=selected;w->original=fn->instructions[w->instruction];}
    }
    if(id==9u)c=be_emit(f,fn,ZR_EXEC_IR_OPCODE_COPY,ZR_VALUE_TYPE_BOOL,&c,1u,0u,0u);
    if(id==14u){unsigned raw,x,args[2];origin=profile==0u?ZR_VALUE_TYPE_DOUBLE:profile==1u?ZR_VALUE_TYPE_UINT64:ZR_VALUE_TYPE_INT32;
        raw=be_const(f,fn,origin,40u,empty);x=be_emit(f,fn,ZR_EXEC_IR_OPCODE_COPY,ZR_VALUE_TYPE_INT64,&raw,1u,0u,0u);
        args[0]=x;args[1]=a;c=be_emit(f,fn,ZR_EXEC_IR_OPCODE_COMPARE,ZR_VALUE_TYPE_BOOL,args,2u,0u,0u);
        if(!raw||!x||!c)return 0;fn->instructions[fn->values[c-1u].definition-1u].typeToken=0u;}
    if(id==12u||id==13u||id==15u||(id==10u&&!duplicate))c=ext;
    if(duplicate){if(profile==4u)c=ext;
        if(!be_jump(f,fn,1u,0u,c,2u,2u,2u)||!be_preds(fn,2u,1u,1u,2u))return 0;
        p=be_phi(fn,2u,ZR_VALUE_TYPE_INT64,1u,a,1u,z);
        return p&&be_finish(f,fn,2u,p,two,fold,selected?42u:52u,0u);}
    if(loop){unsigned h;
        if(!be_jump(f,fn,1u,0u,0u,2u,0u,1u)||!be_preds(fn,2u,1u,3u,2u))return 0;
        h=be_phi(fn,2u,ZR_VALUE_TYPE_BOOL,1u,truth,3u,profile==0u?truth:ext);
        start=fn->instructionCount;if(!h||!be_jump(f,fn,2u,start,h,3u,4u,2u)||!be_preds(fn,3u,2u,0u,1u))return 0;
        start=fn->instructionCount;if(!be_jump(f,fn,3u,start,ext,2u,5u,2u)||!be_preds(fn,4u,2u,0u,1u))return 0;
        start=fn->instructionCount;if(!be_jump(f,fn,4u,start,0u,5u,0u,1u)||!be_preds(fn,5u,3u,4u,2u))return 0;
        p=be_phi(fn,5u,ZR_VALUE_TYPE_INT64,3u,a,4u,z);
        return p&&be_finish(f,fn,5u,p,two,fold,42u,0u);}
    if(!c||!be_jump(f,fn,1u,0u,c,2u,3u,2u))return 0;
    for(b=2u;b<=3u;++b){start=fn->instructionCount;
        if(id==18u&&profile<2u&&b==3u){unsigned args[2];TZrUInt64 left=profile?UINT64_C(0x8000000000000000):40u;
            args[0]=be_const(f,fn,ZR_VALUE_TYPE_INT64,left,empty);
            args[1]=be_const(f,fn,ZR_VALUE_TYPE_INT64,profile?UINT64_MAX:0u,empty);
            if(!args[0]||!args[1]||!be_emit(f,fn,ZR_EXEC_IR_OPCODE_DIV,ZR_VALUE_TYPE_INT64,args,2u,0u,ZR_EXEC_IR_FLAG_MAY_THROW))return 0;}
        if(!be_preds(fn,b,1u,0u,1u)||!be_jump(f,fn,b,start,0u,4u,0u,1u))return 0;}
    if(!be_preds(fn,4u,2u,3u,2u))return 0;
    if(nested||(id==10u&&!duplicate)){
        unsigned other=id==13u?ext:(id==10u?c:(id==15u?falsity:falsity));
        if(id==10u){unsigned same=selected?truth:falsity;p=be_phi(fn,4u,ZR_VALUE_TYPE_BOOL,2u,same,3u,same);}
        else p=be_phi(fn,4u,ZR_VALUE_TYPE_BOOL,2u,truth,3u,other);
        if(!p)return 0;
        start=fn->instructionCount;if(!be_jump(f,fn,4u,start,p,5u,6u,2u))return 0;
        for(b=5u;b<=6u;++b){start=fn->instructionCount;
            if(!be_preds(fn,b,4u,0u,1u)||!be_jump(f,fn,b,start,0u,7u,0u,1u))return 0;}
        if(!be_preds(fn,7u,5u,6u,2u))return 0;
        p=be_phi(fn,7u,ZR_VALUE_TYPE_INT64,5u,a,6u,z);
        return p&&be_finish(f,fn,7u,p,two,fold,selected?42u:52u,0u);}
    p=be_phi(fn,4u,ZR_VALUE_TYPE_INT64,2u,a,3u,z);
    if(id==18u&&profile==2u)fold=0u;
    return p&&be_finish(f,fn,4u,p,two,fold,selected?42u:52u,id==18u&&profile==2u?ZR_EXEC_IR_FLAG_MAY_THROW:0u);
}
int Be_Build(SBeFixture *f,unsigned id,unsigned profile) {
    unsigned i;memset(f,0,sizeof(*f));ZrCore_ExecIr_ModuleInit(&f->module);
    ZrParser_ExecIr_AnalysisCacheInit(&f->cache);ZrParser_ExecIr_RemarkSinkInit(&f->remarks);ZrParser_ExecIr_PassFailureInit(&f->failure);
    f->module.id=1u;f->module.moduleHash=UINT64_C(0x424f4f4c);f->source=1000u;
    f->options.disabledPassMask=ZR_EXEC_IR_OPTIMIZE_DISABLE_DCE;f->options.failure=&f->failure;
    if(!be_function(f,id,profile,id==17u&&profile==1u))return 0;
    if(id==22u&&!be_function(f,1u,0u,0u))return 0;
    if(f->module.constantCount>16u)return 0;f->initialCount=f->module.constantCount;
    for(i=0;i<f->initialCount;++i)f->initial[i]=f->module.constants[i];f->cfgHash=be_cfg(f);return 1;
}
int Be_Verify(SBeFixture *f) { return ZrCore_ExecIr_VerifyModule(&f->module,&f->diagnostic); }
int Be_Optimize(SBeFixture *f,unsigned writable) {
    unsigned char old[sizeof(f->diagnostic)];int ok;
    memset(&f->diagnostic,0xA5,sizeof(f->diagnostic));memset(&f->result,0xA5,sizeof(f->result));
    memcpy(old,&f->diagnostic,sizeof(old));
    ok=ZrParser_ExecIr_Optimize(&f->module,&f->options,&f->result,writable?&f->diagnostic:NULL);
    return ok&&(writable||memcmp(old,&f->diagnostic,sizeof(old))==0);
}
int Be_Private(SBeFixture *f,unsigned entry,unsigned writable) {
    TZrBool changed=ZR_FALSE;int ok;unsigned char old[sizeof(f->diagnostic)];
    f->context.cache=&f->cache;f->context.scratchArena=&f->initial;
    memset(&f->diagnostic,0xA5,sizeof(f->diagnostic));memcpy(old,&f->diagnostic,sizeof(old));
    if(entry==0u)ok=ZrParser_ExecIr_RunModuleSccpPass(&f->module,f->module.functions,&f->context,&changed,writable?&f->diagnostic:NULL);
    else {
        unsigned count=0u;const SZrExecIrPassInfo *passes=ZrParser_ExecIr_GetScalarPasses(&count);
        if(entry!=1u||!passes||!count||passes[0].run!=ZrParser_ExecIr_RunSccpPass)return 0;
        ok=ZrParser_ExecIr_RunModulePassPipeline(&f->module,f->module.functions,passes,1u,
                &f->context,writable?&f->diagnostic:NULL);
    }
    return ok&&(writable||memcmp(old,&f->diagnostic,sizeof(old))==0)&&
        f->context.constants==f->module.constants&&f->context.constantCount==f->module.constantCount&&
        f->context.scratchArena==&f->initial;
}
static int be_instruction_equal(const SZrExecIrInstruction *a,const SZrExecIrInstruction *b) {
    return memcmp(a,b,sizeof(*a))==0;
}
int Be_Check(SBeFixture *f,unsigned requireFold) {
    unsigned i,j,k;SBeSnapshot live;
    if(!be_inventory(f,&live)||!Be_Verify(f)||be_cfg(f)!=f->cfgHash||f->module.constantCount<f->initialCount)return 0;
    for(i=0;i<f->initialCount;++i){const SZrExecIrConstant *a=f->initial+i,*b=f->module.constants+i;
        if(a->typeToken!=b->typeToken||a->flags!=b->flags||a->bits!=b->bits)return 0;}
    for(i=f->initialCount;i<f->module.constantCount;++i){unsigned used=0u;
        for(j=0;j<f->module.functionCount;++j)for(k=0;k<f->module.functions[j].instructionCount;++k){const SZrExecIrInstruction *in=f->module.functions[j].instructions+k;
            if(in->opcode==ZR_EXEC_IR_OPCODE_CONSTANT&&in->layoutId==i)used=1u;}
        if(!used)return 0;}
    for(i=0u;i<f->compareCount;++i){const SBeCompare *w=f->compares+i;
        const SZrExecIrFunction *fn=f->module.functions+w->function;
        const SZrExecIrInstruction *in=fn->instructions+w->instruction;
        const SZrExecIrConstant *c;
        if(!requireFold&&be_instruction_equal(in,&w->original))continue;
        if(in->opcode!=ZR_EXEC_IR_OPCODE_CONSTANT||in->typeToken!=ZR_VALUE_TYPE_BOOL||
           in->flags||in->bindingRow||in->deoptId||in->matchTypeToken||
           in->effectIn||in->effectOut||in->operands.count||in->phiRange.count||
           in->successorRange.count||in->memoryIn.count||in->memoryOut.count||
           in->sourceId!=w->original.sourceId||in->results.start!=w->original.results.start||
           in->results.count!=w->original.results.count||in->results.count!=1u||
           fn->values[fn->results[in->results.start]-1u].typeToken!=ZR_VALUE_TYPE_BOOL||
           fn->values[fn->results[in->results.start]-1u].definition!=w->instruction+1u||
           in->layoutId>=f->module.constantCount)return 0;
        c=f->module.constants+in->layoutId;
        if(c->typeToken!=ZR_VALUE_TYPE_BOOL||c->flags||c->bits!=w->expected)return 0;
    }
    for(i=0;i<f->targetCount;++i){const SBeTarget *t=f->targets+i;const SZrExecIrFunction *fn=f->module.functions+t->function;const SZrExecIrInstruction *in=NULL;
        for(j=0;j<fn->instructionCount;++j)if(fn->instructions[j].sourceId==t->source)in=fn->instructions+j;
        if(!in)return 0;
        if(!t->fold){if(!be_instruction_equal(in,&t->original))return 0;continue;}
        if(!requireFold&&be_instruction_equal(in,&t->original))continue;
        if(in->opcode!=ZR_EXEC_IR_OPCODE_CONSTANT||in->flags||in->operands.count||in->effectIn||in->effectOut||in->memoryIn.count||in->memoryOut.count||
                in->results.start!=t->original.results.start||in->results.count!=1u||in->typeToken!=ZR_VALUE_TYPE_INT64)return 0;
        if(!f->module.constantCount){if(in->layoutId!=t->bits)return 0;}
        else {const SZrExecIrConstant *c;if(in->layoutId>=f->module.constantCount)return 0;c=f->module.constants+in->layoutId;
            if(c->typeToken!=ZR_VALUE_TYPE_INT64||c->flags||c->bits!=t->bits)return 0;}
    }return 1;
}
int Be_Dispose(SBeFixture *f) {
    SZrOwnerHeapStats st;ZrTests_ExecOwner_Disarm();ZrCore_ExecIr_FreeModule(&f->module);
    ZrParser_ExecIr_AnalysisCacheFree(&f->cache);ZrParser_ExecIr_RemarkSinkFree(&f->remarks);ZrParser_ExecIr_PassFailureFree(&f->failure);
    st=ZrTests_ExecOwner_Stats();g_totals.attempts+=st.attempts;g_totals.live+=st.live;g_totals.errors+=st.errors;
    if(st.live)ZrTests_ExecOwner_Recover();return !st.live&&!st.errors;
}
void Be_Profile(unsigned id,const char *name,SBeResult r) {
    printf("BOOL_EDGE_PROFILE id=%u name=%s safety=%u feature=%u result=%s\n",id,name,r.safety,r.feature,r.safety&&r.feature?"PASS":"FAIL");
}
static SBeResult be_standard(unsigned id,unsigned p,unsigned writable,const char *name) {
    SBeFixture f;SBeResult r={1u,1u};unsigned pre=0u,post=0u,ok=0u,before=0u,after=0u,matched=0u;TZrUInt64 hash=0;
    if(!Be_Build(&f,id,p)){r.safety=0u;goto done;}pre=(unsigned)Be_Verify(&f);if(!pre){r.safety=0u;goto done;}
    before=f.module.constantCount;
    if(id==17u&&p==0u){const SZrExecIrPassInfo *passes;unsigned count;
        passes=ZrParser_ExecIr_GetScalarPasses(&count);f.context.cache=&f.cache;f.context.constants=f.module.constants;f.context.constantCount=f.module.constantCount;
        f.targets[0].fold=0u;ok=(unsigned)(passes&&count&&ZrParser_ExecIr_RunPassPipeline(f.module.functions,passes,1u,&f.context,writable?&f.diagnostic:NULL));
    }else ok=(unsigned)Be_Optimize(&f,writable);
    post=(unsigned)Be_Check(&f,0u);after=f.module.constantCount;r.safety=(unsigned)(ok&&post);
    matched=r.safety?(unsigned)Be_Check(&f,1u):0u;r.feature=matched;
    if(id==19u&&r.safety){unsigned repeatBefore=f.module.constantCount;hash=Be_Hash(&f);
        r.safety&=(unsigned)(Be_Optimize(&f,writable)&&Be_Check(&f,0u));
        r.feature&=(unsigned)(Be_Check(&f,1u)&&hash==Be_Hash(&f)&&repeatBefore==f.module.constantCount);}
 done:r.safety&=(unsigned)Be_Dispose(&f);
    printf("BOOL_EDGE_FOLD id=%u profile=%s writable=%u preverify=%u optimize=%u postverify=%u pool_before=%u pool_after=%u matched=%u safety=%u\n",
        id,name,writable,pre,ok,post,before,after,matched,r.safety);
    Be_Profile(id,name,r);ZrTests_ExecOwner_Reset();return r;
}
SBeResult Be_Matrix(unsigned id) {
    SBeResult r={1u,1u},q;unsigned p,n,w;const char *name;
    if(id==20u)return Be_Budget(id);if(id>=21u)return Be_Faults(id);
    n=(id>=3u&&id<=9u)||id==11u||id==16u||id==17u?2u:id==10u?5u:id==14u||id==18u?3u:1u;
    for(p=0;p<n;++p) {
        for(w=0;w<(id<=2u?2u:1u);++w){char label[64];
        name=id==10u?(p<2u?(p?"identical_false":"identical_true"):p==2u?"duplicate_true":p==3u?"duplicate_false":"duplicate_unknown"):
            id==11u?(p?"nested_dependent_false":"nested_dependent_true"):
            id==16u?(p?"loop_external_backedge":"loop_stable_true"):
            id==17u?(p?"legacy_empty":"legacy_no_owner"):
            id==14u?(p==0u?"float_copy":p==1u?"uint64_copy":"int32_copy"):
            id==18u?(p==0u?"dead_div_zero":p==1u?"dead_div_min":"observable_add"):
            id==2u?"false":id==12u?"external_bool":id==13u?"true_unknown_phi":
            id==15u?"different_bool_phi":id==19u?"repeat":p?"false":"true";
        snprintf(label,sizeof(label),"%s_w%u",name,id<=2u?w:1u);
        q=be_standard(id,p,id<=2u?w:1u,label);r.safety&=q.safety;r.feature&=q.feature;
        }
    }
    return r;
}
