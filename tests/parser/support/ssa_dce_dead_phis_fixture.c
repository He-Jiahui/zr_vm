#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "ssa_dce_dead_phis_fixture.h"
#include "ssa_exec_ir_owned_module_shim.h"

static SDpTotals g_totals;
void Dp_SetUp(void) { ZrTests_ExecOwner_Reset(); }
void Dp_TearDown(void) { ZrTests_ExecOwner_Disarm(); }
SDpTotals Dp_Totals(void) { return g_totals; }
void Dp_FaultCount(unsigned a,unsigned b) { g_totals.faultMeasured+=a;g_totals.faultCompleted+=b; }
static TZrUInt64 dp_mix(TZrUInt64 h,TZrUInt64 v) { return (h^v)*UINT64_C(1099511628211); }
TZrUInt64 Dp_Hash(const SDpFixture *f) {
    TZrUInt64 h=UINT64_C(1469598103934665603);unsigned i;
    h=dp_mix(h,f->module.functionCount);h=dp_mix(h,f->module.constantCount);
    for(i=0;i<f->module.functionCount;++i)h=dp_mix(h,ZrParser_ExecIr_FunctionHash(f->module.functions+i));
    return h;
}
static int dp_owner(SDpSnapshot *s,const void *p,unsigned n,unsigned cap,size_t width) {
    SZrOwnerLiveSpan span;SDpOwner *o;
    if(!p)return !n&&!cap;
    if(n>cap||!cap||s->count==DP_MAX_OWNERS||
       !ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,p,cap,width,&span))return 0;
    o=s->owners+s->count++;o->address=(uintptr_t)p;o->record=span.record;o->bytes=(size_t)cap*width;
    return 1;
}
static int dp_inventory(const SDpFixture *f,SDpSnapshot *s) {
    unsigned i;memset(s,0,sizeof(*s));
#define DO(p,n,c) do { if(!dp_owner(s,(p),(n),(c),sizeof(*(p))))return 0; } while(0)
    DO(f->module.functions,f->module.functionCount,f->module.functionCapacity);
    DO(f->module.constants,f->module.constantCount,f->module.constantCapacity);
    DO(f->module.layouts,f->module.layoutCount,f->module.layoutCapacity);
    DO(f->module.sourceMaps,f->module.sourceMapCount,f->module.sourceMapCapacity);
    for(i=0;i<f->module.functionCount;++i){const SZrExecIrFunction *fn=f->module.functions+i;
#define DF(p,n,c) DO(fn->p,fn->n,fn->c)
        DF(values,valueCount,valueCapacity);DF(instructions,instructionCount,instructionCapacity);
        DF(blocks,blockCount,blockCapacity);DF(operands,operandCount,operandCapacity);DF(results,resultCount,resultCapacity);
        DF(phiPool,phiCount,phiCapacity);DF(phiIncoming,phiIncomingCount,phiIncomingCapacity);
        DF(predecessors,predecessorCount,predecessorCapacity);DF(successors,successorCount,successorCapacity);
        DF(memoryTokenPool,memoryTokenCount,memoryTokenCapacity);DF(gcRoots,gcRootCount,gcRootCapacity);
        DF(deoptStates,deoptStateCount,deoptStateCapacity);DF(deoptValues,deoptValueCount,deoptValueCapacity);
        DF(deoptAggregates,deoptAggregateCount,deoptAggregateCapacity);DF(deoptAggregateFields,deoptAggregateFieldCount,deoptAggregateFieldCapacity);
        DF(sourceMaps,sourceMapCount,sourceMapCapacity);DF(bindingRows,bindingRowCount,bindingRowCapacity);
        if(fn->frameLayout||fn->gcMap)return 0;
        if(fn->stateMap){const SZrExecIrStateMap *m;
            DO(fn->stateMap,1u,1u);m=fn->stateMap;
            DO(m->entries,m->entryCount,m->entryCapacity);DO(m->valuePool,m->valueCount,m->valueCapacity);
            DO(m->rootPool,m->rootCount,m->rootCapacity);DO(m->ownerStatePool,m->ownerStateCount,m->ownerStateCapacity);}
#undef DF
    }
#undef DO
    return 1;
}
int Dp_Live(const SDpFixture *f) { SDpSnapshot s;return dp_inventory(f,&s); }
int Dp_Snapshot(const SDpFixture *f,SDpSnapshot *s) { if(!dp_inventory(f,s))return 0;s->hash=Dp_Hash(f);return 1; }
int Dp_SameOwners(const SDpFixture *f,const SDpSnapshot *s) {
    SDpSnapshot now;unsigned i,freed;
    if(!dp_inventory(f,&now)||now.count!=s->count)return 0;
    for(i=0;i<s->count;++i)if(now.owners[i].address!=s->owners[i].address||
        now.owners[i].bytes!=s->owners[i].bytes||now.owners[i].record!=s->owners[i].record||
        !ZrTests_ExecOwner_TryRecordFrees(s->owners[i].record,&freed)||freed)return 0;
    return 1;
}
static unsigned dp_value(SZrExecIrFunction *fn,unsigned type,unsigned external,unsigned ownership) {
    return external?ZrCore_ExecIr_FunctionAddExternalValue(fn,type,(EZrExecIrOwnership)ownership,ZR_EXEC_IR_NULLABILITY_UNKNOWN):
        ZrCore_ExecIr_FunctionAddValue(fn,type,(EZrExecIrOwnership)ownership,ZR_EXEC_IR_NULLABILITY_UNKNOWN);
}
static unsigned dp_emit(SDpFixture *f,SZrExecIrFunction *fn,unsigned op,unsigned a,unsigned b,unsigned out,unsigned flags) {
    SZrExecIrInstruction in={0};unsigned args[2]={a,b};TZrExecIrInstructionId id=0;
    in.opcode=(TZrUInt16)op;in.typeToken=out?fn->values[out-1u].typeToken:0u;
    in.flags=(TZrUInt16)flags;in.sourceId=f->source++;
    if(a&&!ZrCore_ExecIr_FunctionAppendOperands(fn,args,b?2u:1u,&in.operands))return 0;
    if(out&&!ZrCore_ExecIr_FunctionAppendResults(fn,&out,1u,&in.results))return 0;
    if(!ZrCore_ExecIr_FunctionAppendInstruction(fn,&in,&id))return 0;
    return id;
}
static unsigned dp_const(SDpFixture *f,SZrExecIrFunction *fn,unsigned bits) {
    unsigned v=dp_value(fn,ZR_VALUE_TYPE_INT64,0u,ZR_EXEC_IR_OWNERSHIP_UNKNOWN),id;
    if(!v)return 0;
    id=dp_emit(f,fn,ZR_EXEC_IR_OPCODE_CONSTANT,0u,0u,v,0u);
    if(!id)return 0;
    fn->instructions[id-1u].layoutId=bits;return v;
}
static unsigned dp_copy(SDpFixture *f,SZrExecIrFunction *fn,unsigned a) {
    unsigned v=dp_value(fn,fn->values[a-1u].typeToken,0u,fn->values[a-1u].ownership);
    return v&&dp_emit(f,fn,ZR_EXEC_IR_OPCODE_COPY,a,0u,v,0u)?v:0u;
}
static int dp_finish(SZrExecIrFunction *fn,unsigned block,unsigned start) {
    SZrExecIrBlock *b=fn->blocks+block-1u;
    b->instructions.start=start;b->instructions.count=fn->instructionCount-start;
    b->terminatorInstructionId=fn->instructionCount;return b->instructions.count!=0u;
}
static int dp_edge(SZrExecIrFunction *fn,unsigned block,unsigned a,unsigned b,unsigned n) {
    unsigned targets[2]={a,b},i,j,p[4];SZrExecIrBlock *from=fn->blocks+block-1u;
    if(!ZrCore_ExecIr_FunctionAppendSuccessors(fn,targets,n,&from->successors))return 0;
    fn->instructions[from->terminatorInstructionId-1u].successorRange=from->successors;
    for(i=0;i<n;++i){SZrExecIrBlock *to=fn->blocks+targets[i]-1u;unsigned count=to->predecessors.count;
        if(count>=4u)return 0;
        for(j=0;j<count;++j)p[j]=fn->predecessors[to->predecessors.start+j];
        p[count]=block;
        if(!ZrCore_ExecIr_FunctionAppendPredecessors(fn,p,count+1u,&to->predecessors))return 0;}
    return 1;
}
static int dp_phis(SZrExecIrFunction *fn,unsigned block,const unsigned *values,unsigned count,
                   unsigned pa,const unsigned *a,unsigned pb,const unsigned *b) {
    SZrExecIrPhi phis[3];unsigned i;
    if(count>3u)return 0;
    memset(phis,0,sizeof(phis));
    for(i=0;i<count;++i){SZrExecIrPhiIncoming in[2]={{pa,a[i]},{pb,b[i]}};phis[i].result=values[i];
        if(!ZrCore_ExecIr_FunctionAppendPhiIncoming(fn,in,2u,&phis[i].incomings))return 0;}
    return ZrCore_ExecIr_FunctionAppendPhis(fn,phis,count,&fn->blocks[block-1u].phis);
}
static void dp_erase_value(SDpExpected *e,const SZrExecIrFunction *fn,unsigned value) {
    unsigned definition=fn->values[value-1u].definition;
    if(definition&&definition<=DP_MAX_INSTRUCTIONS)e->erase[definition-1u]=1u;
}
static int dp_metadata(SDpFixture *f,SZrExecIrFunction *fn,unsigned root,unsigned mode) {
    unsigned ret=fn->instructionCount;
    if(mode==0u){fn->gcRoots=calloc(1u,sizeof(*fn->gcRoots));if(!fn->gcRoots)return 0;
        fn->gcRootCount=fn->gcRootCapacity=1u;fn->gcRoots[0]=root;}
    else if(mode<=2u){
        fn->deoptStates=calloc(1u,sizeof(*fn->deoptStates));if(!fn->deoptStates)return 0;
        fn->deoptStateCount=fn->deoptStateCapacity=1u;fn->deoptStates[0].id=1u;
        fn->deoptStates[0].sourceId=fn->instructions[ret-1u].sourceId;fn->deoptStates[0].resumeId=1u;
        fn->instructions[ret-1u].deoptId=1u;
        if(mode==1u){fn->deoptValues=calloc(1u,sizeof(*fn->deoptValues));if(!fn->deoptValues)return 0;
            fn->deoptValueCount=fn->deoptValueCapacity=1u;fn->deoptValues[0]=root;fn->deoptStates[0].valueRange.count=1u;}
        else {fn->deoptAggregates=calloc(1u,sizeof(*fn->deoptAggregates));
            fn->deoptAggregateFields=calloc(1u,sizeof(*fn->deoptAggregateFields));
            if(!fn->deoptAggregates||!fn->deoptAggregateFields)return 0;
            fn->deoptAggregateCount=fn->deoptAggregateCapacity=1u;
            fn->deoptAggregateFieldCount=fn->deoptAggregateFieldCapacity=1u;
            fn->deoptStates[0].aggregates.count=1u;fn->deoptAggregates[0].identityId=1u;
            fn->deoptAggregates[0].typeToken=101u;fn->deoptAggregates[0].layoutId=201u;fn->deoptAggregates[0].fields.count=1u;
            fn->deoptAggregateFields[0].kind=ZR_EXEC_IR_DEOPT_FIELD_VALUE;fn->deoptAggregateFields[0].valueId=root;}
    }else {
        /* An actual checked checkpoint makes this PHI live through metadata.
         * The return also uses it to supply an independently valid liveness witness. */
        fn->operands[fn->instructions[ret-1u].operands.start]=root;
        fn->instructions[ret-1u].flags|=ZR_EXEC_IR_FLAG_DEBUG_POLL;
        if(!ZrParser_ExecIr_BuildStateMaps(fn,&f->diagnostic))return 0;
    }
    return 1;
}
/* Distinct expected results/erased definitions are selected from the semantic
 * shape here, never recomputed by a liveness traversal in the oracle. */
static int dp_diamond(SDpFixture *f,SZrExecIrFunction *fn,SDpExpected *e,unsigned id,unsigned profile) {
    unsigned cond,answer,x[3],y[3],p[3],c[3],d[3],i,start,live;
    unsigned n=(id==5u||id==14u)?3u:1u,type=ZR_VALUE_TYPE_INT64;
    if(id==10u&&(profile==0u||profile==3u))type=ZR_VALUE_TYPE_OBJECT;
    cond=dp_value(fn,ZR_VALUE_TYPE_BOOL,1u,ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
    answer=dp_const(f,fn,7u);if(!cond||!answer)return 0;
    if(type==ZR_VALUE_TYPE_OBJECT){x[0]=dp_value(fn,type,1u,ZR_EXEC_IR_OWNERSHIP_GC);y[0]=dp_value(fn,type,1u,ZR_EXEC_IR_OWNERSHIP_GC);}
    else x[0]=y[0]=0u;
    if(!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,cond,0u,0u,0u)||!dp_finish(fn,1u,0u))return 0;
    for(i=0;i<2u;++i){unsigned *side=i?y:x,*copy=i?d:c;start=fn->instructionCount;
        if(!side[0])side[0]=dp_const(f,fn,40u+i*10u);
        if(!side[0])return 0;
        copy[0]=dp_copy(f,fn,side[0]);if(!copy[0])return 0;
        if(!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_BRANCH,0u,0u,0u,0u)||!dp_finish(fn,i+2u,start))return 0;
    }
    start=fn->instructionCount;
    for(i=0;i<n;++i){p[i]=dp_value(fn,type,0u,type==ZR_VALUE_TYPE_OBJECT?ZR_EXEC_IR_OWNERSHIP_GC:ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
        if(!p[i])return 0;c[i]=c[0];d[i]=d[0];}
    live=(id==7u||id==9u||id==10u||id==21u);
    e->root=id==10u?p[0]:0u;
    if(!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_RETURN,live&&id!=10u?p[0]:answer,0u,0u,0u)||!dp_finish(fn,4u,start)||
       !dp_edge(fn,1u,2u,3u,2u)||!dp_edge(fn,2u,4u,0u,1u)||!dp_edge(fn,3u,4u,0u,1u)||
       !dp_phis(fn,4u,p,n,2u,c,3u,d))return 0;
    if(live){e->surviving[e->survivingCount++]=p[0];
        if(id!=10u)dp_erase_value(e,fn,answer);}
    else if(n==3u){e->surviving[e->survivingCount++]=p[1];
        fn->operands[fn->instructions[fn->instructionCount-1u].operands.start]=p[1];dp_erase_value(e,fn,answer);}
    else {dp_erase_value(e,fn,c[0]);dp_erase_value(e,fn,d[0]);
        if(type!=ZR_VALUE_TYPE_OBJECT){dp_erase_value(e,fn,x[0]);dp_erase_value(e,fn,y[0]);}}
    if(id==10u){if(!dp_metadata(f,fn,p[0],profile))return 0;if(profile==3u)dp_erase_value(e,fn,answer);}
    if(id==13u){unsigned ids[2]={fn->values[c[0]-1u].definition,fn->instructionCount};
        fn->sourceMaps=calloc(2u,sizeof(*fn->sourceMaps));if(!fn->sourceMaps)return 0;
        fn->sourceMapCount=fn->sourceMapCapacity=2u;
        for(i=0;i<2u;++i){fn->sourceMaps[i].instructionId=ids[i];fn->sourceMaps[i].sourceId=fn->instructions[ids[i]-1u].sourceId;}}
    if(id==14u){if(!dp_metadata(f,fn,p[1],2u))return 0;
        fn->instructions[fn->instructionCount-1u].flags|=ZR_EXEC_IR_FLAG_DEBUG_POLL;
        if(!ZrParser_ExecIr_BuildStateMaps(fn,&f->diagnostic))return 0;}
    return 1;
}
static int dp_loop(SDpFixture *f,SZrExecIrFunction *fn,SDpExpected *e,unsigned id) {
    unsigned cond=dp_value(fn,ZR_VALUE_TYPE_BOOL,1u,0u),a,b,answer,p[2],aa[2],bb[2],n=id==3u?1u:2u,start,i;
    a=dp_const(f,fn,40u);b=dp_const(f,fn,50u);answer=dp_const(f,fn,7u);
    if(!cond||!a||!b||!answer||!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_BRANCH,0u,0u,0u,0u)||!dp_finish(fn,1u,0u))return 0;
    for(i=0;i<n;++i){p[i]=dp_value(fn,ZR_VALUE_TYPE_INT64,0u,0u);if(!p[i])return 0;}
    aa[0]=a;aa[1]=b;bb[0]=n==1u?p[0]:p[1];bb[1]=p[0];
    start=fn->instructionCount;
    if(!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,cond,0u,0u,0u)||!dp_finish(fn,2u,start))return 0;
    start=fn->instructionCount;
    if(!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_BRANCH,0u,0u,0u,0u)||!dp_finish(fn,3u,start))return 0;
    start=fn->instructionCount;
    if(!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_RETURN,id==8u?p[0]:answer,0u,0u,0u)||!dp_finish(fn,4u,start)||
       !dp_edge(fn,1u,2u,0u,1u)||!dp_edge(fn,2u,3u,4u,2u)||!dp_edge(fn,3u,2u,0u,1u)||
       !dp_phis(fn,2u,p,n,1u,aa,3u,bb))return 0;
    if(id==8u){for(i=0;i<n;++i)e->surviving[e->survivingCount++]=p[i];dp_erase_value(e,fn,answer);}
    else {dp_erase_value(e,fn,a);dp_erase_value(e,fn,b);}
    return 1;
}
static int dp_chain(SDpFixture *f,SZrExecIrFunction *fn,SDpExpected *e) {
    unsigned cond=dp_value(fn,ZR_VALUE_TYPE_BOOL,1u,0u),answer=dp_const(f,fn,7u),a,b,p,q,c,d,start;
    if(!cond||!answer||!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,cond,0u,0u,0u)||!dp_finish(fn,1u,0u))return 0;
    start=fn->instructionCount;a=dp_const(f,fn,40u);
    if(!a||!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_BRANCH,0u,0u,0u,0u)||!dp_finish(fn,2u,start))return 0;
    start=fn->instructionCount;b=dp_const(f,fn,50u);
    if(!b||!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_BRANCH,0u,0u,0u,0u)||!dp_finish(fn,3u,start))return 0;
    p=dp_value(fn,ZR_VALUE_TYPE_INT64,0u,0u);q=dp_value(fn,ZR_VALUE_TYPE_INT64,0u,0u);
    start=fn->instructionCount;
    if(!p||!q||!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,cond,0u,0u,0u)||!dp_finish(fn,4u,start))return 0;
    start=fn->instructionCount;c=dp_copy(f,fn,p);
    if(!c||!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_BRANCH,0u,0u,0u,0u)||!dp_finish(fn,5u,start))return 0;
    start=fn->instructionCount;d=dp_copy(f,fn,p);
    if(!d||!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_BRANCH,0u,0u,0u,0u)||!dp_finish(fn,6u,start))return 0;
    start=fn->instructionCount;
    if(!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_RETURN,answer,0u,0u,0u)||!dp_finish(fn,7u,start)||
       !dp_edge(fn,1u,2u,3u,2u)||!dp_edge(fn,2u,4u,0u,1u)||!dp_edge(fn,3u,4u,0u,1u)||
       !dp_edge(fn,4u,5u,6u,2u)||!dp_edge(fn,5u,7u,0u,1u)||!dp_edge(fn,6u,7u,0u,1u)||
       !dp_phis(fn,4u,&p,1u,2u,&a,3u,&b)||!dp_phis(fn,7u,&q,1u,5u,&c,6u,&d))return 0;
    dp_erase_value(e,fn,a);dp_erase_value(e,fn,b);dp_erase_value(e,fn,c);dp_erase_value(e,fn,d);return 1;
}
static int dp_mixed(SDpFixture *f,SZrExecIrFunction *fn,SDpExpected *e,unsigned profile) {
    SZrExecIrPhi old[2],dead[2],hole;SZrExecIrRange appended;unsigned p,q;
    if(!dp_chain(f,fn,e))return 0;
    old[0]=fn->phiPool[0];old[1]=fn->phiPool[1];p=old[0].result;q=old[1].result;
    dead[0]=old[0];dead[1]=old[1];
    dead[0].result=dp_value(fn,ZR_VALUE_TYPE_INT64,0u,0u);dead[1].result=dp_value(fn,ZR_VALUE_TYPE_INT64,0u,0u);
    if(!dead[0].result||!dead[1].result||!ZrCore_ExecIr_FunctionAppendPhis(fn,dead,2u,&appended))return 0;
    fn->phiPool[0]=dead[0];fn->phiPool[1]=old[0];fn->phiPool[2]=old[1];fn->phiPool[3]=dead[1];
    fn->blocks[3].phis=(SZrExecIrRange){.start=0u,.count=2u};fn->blocks[6].phis=(SZrExecIrRange){.start=2u,.count=2u};
    e->surviving[0]=p;e->surviving[1]=q;e->survivingCount=2u;memset(e->erase,0,sizeof(e->erase));
    {unsigned answer=fn->operands[fn->instructions[fn->instructionCount-1u].operands.start];
        dp_erase_value(e,fn,answer);fn->operands[fn->instructions[fn->instructionCount-1u].operands.start]=q;}
    if(profile==1u){fn->phiPool[0]=old[1];fn->phiPool[1]=dead[1];fn->phiPool[2]=dead[0];fn->phiPool[3]=old[0];
        fn->blocks[6].phis.start=0u;fn->blocks[3].phis.start=2u;e->surviving[0]=q;e->surviving[1]=p;}
    if(profile==2u){hole=dead[0];hole.result=dp_value(fn,ZR_VALUE_TYPE_INT64,0u,0u);
        if(!hole.result||!ZrCore_ExecIr_FunctionAppendPhis(fn,&hole,1u,&appended))return 0;
        memmove(fn->phiPool+1u,fn->phiPool,4u*sizeof(*fn->phiPool));fn->phiPool[0]=hole;
        fn->blocks[3].phis.start=1u;fn->blocks[6].phis.start=3u;}
    return 1;
}
static int dp_parallel(SDpFixture *f,SZrExecIrFunction *fn,SDpExpected *e) {
    unsigned cond=dp_value(fn,ZR_VALUE_TYPE_BOOL,1u,0u),a=dp_const(f,fn,40u),b=dp_const(f,fn,50u);
    unsigned p=dp_value(fn,ZR_VALUE_TYPE_INT64,0u,0u),start;
    if(!cond||!a||!b||!p||!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,cond,0u,0u,0u)||!dp_finish(fn,1u,0u))return 0;
    start=fn->instructionCount;
    if(!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_RETURN,p,0u,0u,0u)||!dp_finish(fn,2u,start)||
       !dp_edge(fn,1u,2u,2u,2u)||!dp_phis(fn,2u,&p,1u,1u,&a,1u,&b))return 0;
    e->surviving[e->survivingCount++]=p;return 1;
}
/* Only the dead PHI disappears: all inputs are external and both real
 * instructions are retained terminators; duplicate edge occurrences stay. */
static int dp_phi_only(SDpFixture *f,SZrExecIrFunction *fn,SDpExpected *e) {
    unsigned cond=dp_value(fn,ZR_VALUE_TYPE_BOOL,1u,0u),a=dp_value(fn,ZR_VALUE_TYPE_INT64,1u,0u);
    unsigned b=dp_value(fn,ZR_VALUE_TYPE_INT64,1u,0u),p[2],aa[2],bb[2],start;
    p[0]=dp_value(fn,ZR_VALUE_TYPE_INT64,0u,0u);p[1]=dp_value(fn,ZR_VALUE_TYPE_INT64,0u,0u);
    if(!cond||!a||!b||!p[0]||!p[1])return 0;
    aa[0]=aa[1]=a;bb[0]=bb[1]=b;
    if(!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,cond,0u,0u,0u)||!dp_finish(fn,1u,0u))return 0;
    start=fn->instructionCount;
    if(!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_RETURN,p[0],0u,0u,0u)||!dp_finish(fn,2u,start)||
       !dp_edge(fn,1u,2u,2u,2u)||!dp_phis(fn,2u,p,2u,1u,aa,1u,bb))return 0;
    e->surviving[e->survivingCount++]=p[0];return 1;
}
static int dp_tokens(SDpFixture *f,SZrExecIrFunction *fn,SDpExpected *e) {
    unsigned i,heap=ZR_EXEC_IR_MEMORY_MANAGED_HEAP,ffi=ZR_EXEC_IR_MEMORY_NATIVE_FFI;
    SZrExecIrPhiIncoming effects[2]={{2u,101u},{3u,102u}},memory[2],native[2];
    if(!dp_diamond(f,fn,e,1u,0u))return 0;
    /* CALL is metadata only. Its actual schema reads/writes both regions. */
    for(i=0;i<2u;++i){unsigned at=fn->blocks[i+1u].instructions.start;SZrExecIrInstruction *in=fn->instructions+at;
        unsigned tokens[4]={ZR_EXEC_IR_MEMORY_TOKEN_MAKE(heap,1u),ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ffi,1u),
            ZR_EXEC_IR_MEMORY_TOKEN_MAKE(heap,101u+i),ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ffi,101u+i)};
        SZrExecIrRange range;in->opcode=ZR_EXEC_IR_OPCODE_CALL;in->flags=ZR_EXEC_IR_FLAG_MAY_THROW|ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
        in->effectIn=1u;in->effectOut=101u+i;in->layoutId=0u;e->erase[at]=0u;
        if(!ZrCore_ExecIr_FunctionAppendMemoryTokens(fn,tokens,4u,&range))return 0;
        in->memoryIn=(SZrExecIrRange){.start=range.start,.count=2u};in->memoryOut=(SZrExecIrRange){.start=range.start+2u,.count=2u};
        memory[i].predecessor=i+2u;memory[i].value=tokens[2];native[i].predecessor=i+2u;native[i].value=tokens[3];}
    if(!ZrCore_ExecIr_FunctionSetEffectPhi(fn,4u,103u,effects,2u)||
       !ZrCore_ExecIr_FunctionSetMemoryPhi(fn,4u,(EZrExecIrMemoryClass)ffi,ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ffi,103u),native,2u)||
       !ZrCore_ExecIr_FunctionSetMemoryPhi(fn,4u,(EZrExecIrMemoryClass)heap,ZR_EXEC_IR_MEMORY_TOKEN_MAKE(heap,103u),memory,2u))return 0;
    fn->instructions[fn->instructionCount-1u].phiRange=fn->phiPool[0].incomings;
    return 1;
}
static int dp_observable(SDpFixture *f,SZrExecIrFunction *fn,SDpExpected *e,unsigned profile) {
    unsigned i,id,flags[6]={ZR_EXEC_IR_FLAG_MAY_THROW,ZR_EXEC_IR_FLAG_MAY_ALLOCATE,ZR_EXEC_IR_FLAG_MAY_GC,
        ZR_EXEC_IR_FLAG_MAY_SUSPEND,ZR_EXEC_IR_FLAG_DEBUG_POLL,ZR_EXEC_IR_FLAG_GUARD_EXIT};
    if(!dp_diamond(f,fn,e,1u,0u))return 0;
    if(profile<6u){
        for(i=0;i<2u;++i){unsigned start=fn->blocks[i+1u].instructions.start;
            id=start+1u;fn->instructions[id-1u].flags=(TZrUInt16)flags[profile];e->erase[id-1u]=0u;
        }
    }else {unsigned ownerValue=dp_value(fn,ZR_VALUE_TYPE_OBJECT,1u,ZR_EXEC_IR_OWNERSHIP_UNIQUE),result=0u,oldReturn=fn->instructionCount;
        SZrExecIrInstruction ret=fn->instructions[oldReturn-1u],boundary;
        if(!ownerValue)return 0;
        if(profile==6u){result=dp_value(fn,ZR_VALUE_TYPE_OBJECT,0u,ZR_EXEC_IR_OWNERSHIP_UNIQUE);if(!result)return 0;}
        id=dp_emit(f,fn,profile==6u?ZR_EXEC_IR_OPCODE_MOVE:ZR_EXEC_IR_OPCODE_DROP,ownerValue,0u,result,0u);
        if(!id)return 0;
        if(profile==7u){SZrExecIrRange range;unsigned tokens[2]={ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_OWNERSHIP,1u),
                ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_OWNERSHIP,2u)};
            if(!ZrCore_ExecIr_FunctionAppendMemoryTokens(fn,tokens,2u,&range))return 0;
            fn->instructions[id-1u].memoryIn=(SZrExecIrRange){.start=range.start,.count=1u};
            fn->instructions[id-1u].memoryOut=(SZrExecIrRange){.start=range.start+1u,.count=1u};
            fn->instructions[id-1u].effectIn=1u;fn->instructions[id-1u].effectOut=2u;}
        boundary=fn->instructions[id-1u];fn->instructions[oldReturn-1u]=boundary;fn->instructions[id-1u]=ret;
        if(result)fn->values[result-1u].definition=oldReturn;
        ++fn->blocks[3].instructions.count;fn->blocks[3].terminatorInstructionId=id;
    }
    return 1;
}
static int dp_capture(SZrExecIrFunction *fn,SDpExpected *e) {
    unsigned i,j;if(fn->instructionCount>DP_MAX_INSTRUCTIONS||fn->blockCount>DP_MAX_BLOCKS||
        fn->phiCount>DP_MAX_PHIS||fn->phiIncomingCount>DP_MAX_INCOMING||fn->predecessorCount>DP_MAX_INCOMING||fn->successorCount>DP_MAX_INCOMING)return 0;
    e->instructionCount=fn->instructionCount;e->valueCount=fn->valueCount;e->blockCount=fn->blockCount;
    e->phiCount=fn->phiCount;e->incomingCount=fn->phiIncomingCount;
    memcpy(e->instructions,fn->instructions,fn->instructionCount*sizeof(*fn->instructions));
    memcpy(e->blocks,fn->blocks,fn->blockCount*sizeof(*fn->blocks));
    if(fn->phiCount)memcpy(e->phis,fn->phiPool,fn->phiCount*sizeof(*fn->phiPool));
    if(fn->phiIncomingCount)memcpy(e->incoming,fn->phiIncoming,fn->phiIncomingCount*sizeof(*fn->phiIncoming));
    e->predCount=fn->predecessorCount;e->succCount=fn->successorCount;
    if(e->predCount)memcpy(e->preds,fn->predecessors,e->predCount*sizeof(*fn->predecessors));
    if(e->succCount)memcpy(e->succs,fn->successors,e->succCount*sizeof(*fn->successors));
    for(i=0;i<fn->blockCount;++i)for(j=fn->blocks[i].phis.start;j<fn->blocks[i].phis.start+fn->blocks[i].phis.count;++j)e->phiBlock[j]=i;
    return 1;
}
int Dp_Build(SDpFixture *f,unsigned id,unsigned profile,unsigned functions) {
    unsigned i,b,shape=id;memset(f,0,sizeof(*f));f->profile=profile;f->source=101u;
    ZrCore_ExecIr_ModuleInit(&f->module);f->module.id=1u;f->module.moduleHash=UINT64_C(0x444345);
    ZrParser_ExecIr_AnalysisCacheInit(&f->cache);ZrParser_ExecIr_RemarkSinkInit(&f->remarks);
    f->context.cache=&f->cache;f->context.remarks=&f->remarks;
    f->options.disabledPassMask=ZR_EXEC_IR_OPTIMIZE_DISABLE_SCCP;f->options.remarks=&f->remarks;
    if(functions>DP_MAX_FUNCTIONS||!functions)return 0;
    if(id>=15u&&id<=20u)shape=1u;
    for(i=0;i<functions;++i){TZrExecIrFunctionId fid;SZrExecIrFunction *fn;SDpExpected *e=f->expected+i;
        if(!ZrCore_ExecIr_ModuleAddFunction(&f->module,ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF,90u+i),99u,&fid))return 0;
        fn=ZrCore_ExecIr_ModuleFunctionAt(&f->module,fid);if(!fn)return 0;
        for(b=0;b<(shape==21u&&profile==0u?1u:(shape==5u&&profile==3u)?2u:(shape==2u||shape==5u)?7u:shape==9u?2u:4u);++b)
            if(ZrCore_ExecIr_FunctionAddBlock(fn,b?0u:ZR_EXEC_IR_BLOCK_FLAG_ENTRY)!=b+1u)return 0;
        if(shape==21u&&profile==0u){unsigned v=dp_value(fn,ZR_VALUE_TYPE_INT64,1u,0u);
            if(!v||!dp_emit(f,fn,ZR_EXEC_IR_OPCODE_RETURN,v,0u,0u,0u)||!dp_finish(fn,1u,0u))return 0;}
        else if(shape==2u){if(!dp_chain(f,fn,e))return 0;}
        else if(shape==5u){if(profile==3u){if(!dp_phi_only(f,fn,e))return 0;}
            else if(!dp_mixed(f,fn,e,profile))return 0;}
        else if(shape==3u||shape==4u||shape==8u){if(!dp_loop(f,fn,e,shape))return 0;}
        else if(shape==9u){if(!dp_parallel(f,fn,e))return 0;}
        else if(shape==6u){if(!dp_tokens(f,fn,e))return 0;
            if(profile==1u){unsigned k,region=ZR_EXEC_IR_MEMORY_MANAGED_HEAP;SZrExecIrBlock *join=fn->blocks+3u;
                for(k=0;k<2u;++k){SZrExecIrInstruction *in=fn->instructions+fn->blocks[k+1u].instructions.start;
                    in->effectIn=ZR_EXEC_IR_MEMORY_TOKEN_MAKE(region,1u);in->effectOut=ZR_EXEC_IR_MEMORY_TOKEN_MAKE(region,101u+k);
                    fn->phiIncoming[join->effectPhiIncomings.start+k].value=in->effectOut;}
                join->effectPhiResult=ZR_EXEC_IR_MEMORY_TOKEN_MAKE(region,103u);
                fn->phiIncomingCount=join->memoryPhiIncomings[region].start;
                join->memoryPhiIncomings[region]=join->effectPhiIncomings;}}
        else if(shape==12u){if(!dp_observable(f,fn,e,profile))return 0;}
        else if(!dp_diamond(f,fn,e,shape,profile))return 0;
        if(shape==11u){/* A throwing producer remains while its unused PHI disappears. */
            unsigned k;
            for(k=0;k<2u;++k){unsigned at=fn->blocks[k+1u].instructions.start;
                SZrExecIrInstruction *in=fn->instructions+at+1u;unsigned v=fn->results[in->results.start];
                unsigned operands[2]={fn->operands[in->operands.start],fn->operands[in->operands.start]};
                if(!ZrCore_ExecIr_FunctionAppendOperands(fn,operands,2u,&in->operands))return 0;
                in->opcode=ZR_EXEC_IR_OPCODE_DIV;in->flags=ZR_EXEC_IR_FLAG_MAY_THROW;in->effectIn=1u;in->effectOut=101u+k;
                e->erase[at]=0u;e->erase[at+1u]=0u;(void)v;}
            {SZrExecIrPhiIncoming incoming[2]={{2u,101u},{3u,102u}};
                if(!ZrCore_ExecIr_FunctionSetEffectPhi(fn,4u,103u,incoming,2u))return 0;}}
        if(!dp_capture(fn,e))return 0;
    }
    return 1;
}
int Dp_Verify(SDpFixture *f) {
    return ZrCore_ExecIr_VerifyModule(&f->module,&f->diagnostic)&&ZrParser_ExecIr_Verify(&f->module,&f->diagnostic);
}
int Dp_Run(SDpFixture *f,unsigned entry,unsigned writable) {
    TZrBool changed=ZR_FALSE;SZrExecIrDiagnostic *d=writable?&f->diagnostic:NULL;
    SZrExecIrPassInfo pass={0};memset(&f->diagnostic,0xA5,sizeof(f->diagnostic));
    f->changed=0u;
    if(entry==0u){int ok=ZrParser_ExecIr_RunDcePass(f->module.functions,&f->context,&changed,d);f->changed=changed;return ok;}
    if(entry==1u){pass.name="dce";pass.run=ZrParser_ExecIr_RunDcePass;
        pass.invalidatesAnalysis=ZR_EXEC_IR_ANALYSIS_ALL;
        return ZrParser_ExecIr_RunModulePassPipeline(&f->module,f->module.functions,&pass,1u,&f->context,d);}
    return ZrParser_ExecIr_Optimize(&f->module,&f->options,&f->result,d);
}
/* Observe the actual direct bool, normal ALL-invalidating manager policy,
 * and public Optimize result. No synthesized changed output or test hook. */
int Dp_ObserveChange(const SDpFixture *f,unsigned entry,unsigned expected,TZrUInt64 before) {
    TZrUInt64 after=ZrParser_ExecIr_FunctionHash(f->module.functions);
    const SZrExecIrOptimizationRemark *remark;SZrOwnerLiveSpan span;
    if((before!=after)!=(expected!=0u))return 0;
    if(entry==0u)return f->changed==expected;
    if(f->remarks.count==0u||f->remarks.count>f->remarks.capacity||!f->remarks.items||
       !ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,f->remarks.items,
            f->remarks.capacity,sizeof(*f->remarks.items),&span))return 0;
    remark=f->remarks.items+f->remarks.count-1u;
    if(remark->outcome!=(expected?ZR_EXEC_IR_REMARK_SUCCESS:ZR_EXEC_IR_REMARK_MISSED)||
       remark->beforeHash!=before||remark->afterHash!=after||
       (!expected&&remark->reasonCode!=ZR_EXEC_IR_PASS_REASON_NO_CHANGE))return 0;
    if(entry==1u)return f->cache.validMask==0u&&!f->cache.sccpValues&&!f->cache.sccpValueCount&&f->cache.revision>0u;
    return f->result.functionsVisited==1u&&f->result.functionsChanged==expected&&
        !f->result.budgetExhausted&&((f->result.beforeHash!=f->result.afterHash)==(expected!=0u));
}
int Dp_Stable(const SDpFixture *f) {
    unsigned i,j;for(i=0;i<f->module.functionCount;++i){const SZrExecIrFunction *fn=f->module.functions+i;const SDpExpected *e=f->expected+i;
        if(fn->instructionCount!=e->instructionCount||fn->valueCount!=e->valueCount||fn->blockCount!=e->blockCount||
           fn->phiIncomingCount!=e->incomingCount||fn->predecessorCount!=e->predCount||fn->successorCount!=e->succCount)return 0;
        if(e->incomingCount&&memcmp(fn->phiIncoming,e->incoming,e->incomingCount*sizeof(*fn->phiIncoming)))return 0;
        if(e->predCount&&memcmp(fn->predecessors,e->preds,e->predCount*sizeof(*fn->predecessors)))return 0;
        if(e->succCount&&memcmp(fn->successors,e->succs,e->succCount*sizeof(*fn->successors)))return 0;
        for(j=0;j<fn->valueCount;++j)if(fn->values[j].id!=j+1u)return 0;
        for(j=0;j<fn->blockCount;++j){const SZrExecIrBlock *a=fn->blocks+j,*b=e->blocks+j;unsigned r;
            if(a->id!=b->id||a->instructions.start!=b->instructions.start||a->instructions.count!=b->instructions.count||
               a->terminatorInstructionId!=b->terminatorInstructionId||a->effectPhiResult!=b->effectPhiResult||
               a->effectPhiIncomings.start!=b->effectPhiIncomings.start||a->effectPhiIncomings.count!=b->effectPhiIncomings.count)return 0;
            for(r=0;r<ZR_EXEC_IR_MEMORY_CLASS_COUNT;++r)if(a->memoryPhiResults[r]!=b->memoryPhiResults[r]||
                a->memoryPhiIncomings[r].start!=b->memoryPhiIncomings[r].start||a->memoryPhiIncomings[r].count!=b->memoryPhiIncomings[r].count)return 0;}
        for(j=0;j<fn->instructionCount;++j)if(!e->erase[j]&&memcmp(fn->instructions+j,e->instructions+j,sizeof(*fn->instructions)))return 0;
    }return 1;
}
int Dp_Expected(const SDpFixture *f) {
    unsigned i,j,k;for(i=0;i<f->module.functionCount;++i){const SZrExecIrFunction *fn=f->module.functions+i;const SDpExpected *e=f->expected+i;
        if(fn->phiCount!=e->survivingCount)return 0;
        for(j=0;j<e->survivingCount;++j){const SZrExecIrPhi *p=fn->phiPool+j;
            if(p->result!=e->surviving[j])return 0;
            for(k=0;k<e->phiCount&&e->phis[k].result!=p->result;++k){}
            if(k==e->phiCount||p->incomings.start!=e->phis[k].incomings.start||p->incomings.count!=e->phis[k].incomings.count)return 0;}
        for(j=0;j<fn->blockCount;++j){unsigned count=0u,prefix=0u;
            for(k=0;k<e->survivingCount;++k){unsigned at;
                for(at=0;at<e->phiCount&&e->phis[at].result!=e->surviving[k];++at){}
                if(at==e->phiCount)return 0;
                if(at<e->blocks[j].phis.start)++prefix;
                else if(at-e->blocks[j].phis.start<e->blocks[j].phis.count)++count;}
            if(fn->blocks[j].phis.count!=count||fn->blocks[j].phis.start!=prefix)return 0;}
        for(j=0;j<fn->instructionCount;++j)if(e->erase[j]&&fn->instructions[j].opcode!=ZR_EXEC_IR_OPCODE_NOP)return 0;
        if(e->root&&f->profile==3u){unsigned seen=0u,rooted=0u;
            if(!fn->stateMap)return 0;
            for(j=0;j<fn->stateMap->valueCount;++j){if(fn->stateMap->valuePool[j]==e->root)seen=1u;}
            for(j=0;j<fn->stateMap->rootCount;++j){if(fn->stateMap->rootPool[j]==e->root)rooted=1u;}
            if(!seen||!rooted||fn->stateMap->ownerStateCount!=fn->stateMap->valueCount)return 0;}
        if(fn->sourceMapCount&&fn->sourceMapCount!=1u)return 0;
    }return 1;
}
int Dp_Remarks(SDpFixture *f) {
    f->remarks.items=calloc(1u,sizeof(*f->remarks.items));if(!f->remarks.items)return 0;
    f->remarks.count=f->remarks.capacity=1u;f->remarks.items[0].sourceId=777u;f->remarks.items[0].reasonCode=123u;return 1;
}
int Dp_RemarkPrefix(const SDpFixture *f) {
    SZrOwnerLiveSpan span;
    return f->remarks.count==1u&&f->remarks.capacity>=1u&&f->remarks.items&&
        ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,f->remarks.items,
            f->remarks.capacity,sizeof(*f->remarks.items),&span)&&
        f->remarks.items[0].sourceId==777u&&f->remarks.items[0].reasonCode==123u;
}
int Dp_Dispose(SDpFixture *f) {
    SZrOwnerHeapStats st;ZrTests_ExecOwner_Disarm();
    ZrParser_ExecIr_AnalysisCacheFree(&f->cache);ZrParser_ExecIr_RemarkSinkFree(&f->remarks);ZrCore_ExecIr_FreeModule(&f->module);
    st=ZrTests_ExecOwner_Stats();g_totals.attempts+=st.attempts;g_totals.live+=st.live;g_totals.errors+=st.errors;
    if(st.live)ZrTests_ExecOwner_Recover();
    return !st.live&&!st.errors;
}
void Dp_Profile(unsigned id,const char *name,unsigned entry,unsigned writable,SDpResult r) {
    printf("DCE_PHI_PROFILE id=%u name=%s entry=%u writable=%u safety=%u feature=%u\n",id,name,entry,writable,r.safety,r.feature);
}
SDpResult Dp_Semantic(unsigned id) {
    SDpResult all={1u,1u};unsigned profile,w,entry,n=id==5u?4u:id==6u?2u:id==10u?4u:id==12u?8u:id==21u?2u:1u;
    for(profile=0;profile<n;++profile){
        for(entry=0;entry<2u;++entry){
            for(w=0;w<2u;++w){
        SDpFixture f;SDpResult r={1u,1u};unsigned pre=0u,post=0u,ok=0u,stable=0u,expected=0u;
        unsigned observe=(id==21u||(id==5u&&profile==3u)),firstExpected=id==21u&&profile==0u?0u:1u,firstOk=0u,repeatOk=0u;TZrUInt64 beforeHash=0u;
        if(!Dp_Build(&f,id,profile,1u)||!Dp_Live(&f)||!(pre=(unsigned)Dp_Verify(&f))){r.safety=0u;goto done;}
        if(observe)beforeHash=ZrParser_ExecIr_FunctionHash(f.module.functions);
        ok=(unsigned)Dp_Run(&f,entry,w);
        if(Dp_Live(&f)){post=(unsigned)Dp_Verify(&f);stable=(unsigned)Dp_Stable(&f);expected=(unsigned)Dp_Expected(&f);}
        r.safety&=(unsigned)(ok&&post&&stable);r.feature&=expected;
        if(observe&&r.safety){TZrUInt64 hash=Dp_Hash(&f);
            firstOk=(unsigned)Dp_ObserveChange(&f,entry,firstExpected,beforeHash);r.feature&=firstOk;
            beforeHash=ZrParser_ExecIr_FunctionHash(f.module.functions);
            r.safety&=(unsigned)(Dp_Run(&f,entry,w)&&Dp_Live(&f)&&Dp_Verify(&f)&&hash==Dp_Hash(&f)&&Dp_Stable(&f));
            if(r.safety){repeatOk=(unsigned)Dp_ObserveChange(&f,entry,0u,beforeHash);r.feature&=repeatOk;}}
     done:r.safety&=(unsigned)Dp_Dispose(&f);
        if(observe)printf("DCE_PHI_CHANGE id=%u profile=%u entry=%u writable=%u first_expected=%u first_ok=%u repeat_ok=%u\n",id,profile,entry,w,firstExpected,firstOk,repeatOk);
        printf("DCE_PHI_IR id=%u profile=%u entry=%u writable=%u preverify=%u run=%u postverify=%u stable=%u expected=%u\n",id,profile,entry,w,pre,ok,post,stable,expected);
        {char name[32];(void)snprintf(name,sizeof(name),"shape_%u",profile);Dp_Profile(id,name,entry,w,r);}
        all.safety&=r.safety;all.feature&=r.feature;ZrTests_ExecOwner_Reset();
            }
        }
    }
    return all;
}
