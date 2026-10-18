#include <stdlib.h>
#include <string.h>
#include "ssa_phi_identity_support.h"
#include "ssa_exec_ir_owned_module_shim.h"

void Pix_Init(SPixFixture *f,unsigned id,unsigned profile) {
    memset(f,0,sizeof(*f));f->id=id;f->profile=profile;
    ZrCore_ExecIr_ModuleInit(&f->base.module);f->base.module.id=1u;
    f->base.module.moduleHash=UINT64_C(0x50484941);
    ZrParser_ExecIr_AnalysisCacheInit(&f->base.cache);
    ZrParser_ExecIr_RemarkSinkInit(&f->base.remarks);
    f->base.options.remarks=&f->base.remarks;
    f->base.context.cache=&f->base.cache;f->base.context.remarks=&f->base.remarks;
}
SZrExecIrFunction *Pix_AddFunction(SPixFixture *f,unsigned blocks) {
    TZrExecIrFunctionId id=0u;SZrExecIrFunction *fn;unsigned i,n=f->base.module.functionCount;
    if(n>=PIX_FUNCTIONS||blocks>PIX_BLOCKS||!blocks||
       !ZrCore_ExecIr_ModuleAddFunction(&f->base.module,
        ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF,91u+n),99u,&id))return NULL;
    fn=ZrCore_ExecIr_ModuleFunctionAt(&f->base.module,id);if(!fn)return NULL;
    for(i=0u;i<blocks;++i)
        if(ZrCore_ExecIr_FunctionAddBlock(fn,i?0u:ZR_EXEC_IR_BLOCK_FLAG_ENTRY)!=i+1u)return NULL;
    f->functions=f->base.module.functionCount;return fn;
}
unsigned Pix_Value(SZrExecIrFunction *fn,unsigned external,unsigned type) {
    return external?ZrCore_ExecIr_FunctionAddExternalValue(fn,type,ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN):
        ZrCore_ExecIr_FunctionAddValue(fn,type,ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN);
}
unsigned Pix_Emit(SZrExecIrFunction *fn,unsigned opcode,unsigned a,unsigned b,unsigned result) {
    SZrExecIrInstruction in={0};unsigned args[2]={a,b};TZrExecIrInstructionId id=0u;
    in.opcode=(TZrUInt16)opcode;in.sourceId=101u+fn->instructionCount;
    in.typeToken=result?fn->values[result-1u].typeToken:0u;
    if(a&&!ZrCore_ExecIr_FunctionAppendOperands(fn,args,b?2u:1u,&in.operands))return 0u;
    if(result&&!ZrCore_ExecIr_FunctionAppendResults(fn,&result,1u,&in.results))return 0u;
    return ZrCore_ExecIr_FunctionAppendInstruction(fn,&in,&id)?id:0u;
}
int Pix_Finish(SZrExecIrFunction *fn,unsigned block,unsigned start) {
    SZrExecIrBlock *b=fn->blocks+block-1u;
    b->instructions=(SZrExecIrRange){.start=start,.count=fn->instructionCount-start};
    b->terminatorInstructionId=fn->instructionCount;return b->instructions.count!=0u;
}
int Pix_Edges(SZrExecIrFunction *fn,unsigned block,const unsigned *targets,unsigned count) {
    unsigned i,j,p[PIX_BLOCKS];SZrExecIrBlock *from=fn->blocks+block-1u;
    if(!ZrCore_ExecIr_FunctionAppendSuccessors(fn,targets,count,&from->successors))return 0;
    fn->instructions[from->terminatorInstructionId-1u].successorRange=from->successors;
    for(i=0u;i<count;++i){SZrExecIrBlock *to=fn->blocks+targets[i]-1u;unsigned n=to->predecessors.count;
        if(n>=PIX_BLOCKS)return 0;
        for(j=0u;j<n;++j)p[j]=fn->predecessors[to->predecessors.start+j];p[n]=block;
        if(!ZrCore_ExecIr_FunctionAppendPredecessors(fn,p,n+1u,&to->predecessors))return 0;
    }
    return 1;
}
int Pix_Phi(SZrExecIrFunction *fn,unsigned block,unsigned result,const unsigned *values,unsigned count) {
    SZrExecIrPhi phi={0};SZrExecIrPhiIncoming in[PIX_BLOCKS];SZrExecIrRange added;
    SZrExecIrBlock *b=fn->blocks+block-1u;unsigned i;
    if(count>PIX_BLOCKS||count!=b->predecessors.count)return 0;phi.result=result;
    for(i=0u;i<count;++i){in[i].predecessor=fn->predecessors[b->predecessors.start+i];in[i].value=values[i];}
    if(!ZrCore_ExecIr_FunctionAppendPhiIncoming(fn,in,count,&phi.incomings)||
       !ZrCore_ExecIr_FunctionAppendPhis(fn,&phi,1u,&added))return 0;
    if(!b->phis.count)b->phis=added;
    else {if(b->phis.start+b->phis.count!=added.start)return 0;++b->phis.count;}
    return 1;
}
int Pix_AddOperandPatch(SPixPlan *p,unsigned slot,unsigned value) {
    if(p->patches==PIX_PATCHES)return 0;p->operand[p->patches++]=(SPixPatch){slot,value};return 1;
}
int Pix_AddIncomingPatch(SPixPlan *p,unsigned slot,unsigned value) {
    if(p->incomingPatches==PIX_PATCHES)return 0;p->incoming[p->incomingPatches++]=(SPixPatch){slot,value};return 1;
}
int Pix_RemovePhi(SPixPlan *p,unsigned result) {
    if(p->removed==PIX_PHIS)return 0;p->remove[p->removed++]=result;return 1;
}
int Pix_Erase(SPixPlan *p,unsigned instruction) {
    if(p->erased==PIX_INST)return 0;p->erase[p->erased++]=instruction;return 1;
}
/* Modes: external same, distinct, tail COPY, predecessor COPY, arithmetic root.
 * Reverse layout puts the merge physically before its predecessor definitions. */
int Pix_Diamond(SPixFixture *f,SZrExecIrFunction *fn,unsigned mode,unsigned reverse) {
    SPixPlan *plan=f->plan+fn->id-1u;
    unsigned c=Pix_Value(fn,1u,ZR_VALUE_TYPE_BOOL),a=Pix_Value(fn,1u,ZR_VALUE_TYPE_INT64);
    unsigned b=mode==1u||mode==4u?Pix_Value(fn,1u,ZR_VALUE_TYPE_INT64):a;
    unsigned root=a,p=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64),q=0u,side[2]={a,b},copies[2]={0u,0u};
    unsigned merge=reverse?2u:4u,left=reverse?3u:2u,right=reverse?4u:3u,block,start,ret=0u;
    unsigned target[2]={left,right},inputs[2];
    if(!c||!a||!b||!p)return 0;
    if(mode==2u){q=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);if(!q)return 0;}
    if(mode==3u){side[0]=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);side[1]=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);if(!side[0]||!side[1])return 0;}
    for(block=1u;block<=4u;++block){start=fn->instructionCount;
        if(block==1u){
            if(mode==4u){root=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);
                if(!root||!Pix_Emit(fn,ZR_EXEC_IR_OPCODE_ADD,a,b,root))return 0;side[0]=side[1]=root;}
            if(!Pix_Emit(fn,ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,c,0u,0u))return 0;
        }else if(block==merge){
            if(mode==2u&&!Pix_Emit(fn,ZR_EXEC_IR_OPCODE_COPY,p,0u,q))return 0;
            ret=Pix_Emit(fn,ZR_EXEC_IR_OPCODE_RETURN,q?q:p,0u,0u);if(!ret)return 0;
        }else {
            unsigned index=block==left?0u:1u;
            if(mode==3u){copies[index]=Pix_Emit(fn,ZR_EXEC_IR_OPCODE_COPY,a,0u,side[index]);if(!copies[index])return 0;}
            if(!Pix_Emit(fn,ZR_EXEC_IR_OPCODE_BRANCH,0u,0u,0u))return 0;
        }
        if(!Pix_Finish(fn,block,start))return 0;plan->dom[block-1u]=1u;
    }
    if(!Pix_Edges(fn,1u,target,2u)||!Pix_Edges(fn,left,&merge,1u)||!Pix_Edges(fn,right,&merge,1u))return 0;
    inputs[0]=side[0];inputs[1]=side[1];
    if(!Pix_Phi(fn,merge,p,inputs,2u))return 0;
    if(mode!=1u){
        if(!Pix_AddOperandPatch(plan,fn->instructions[ret-1u].operands.start,root)||!Pix_RemovePhi(plan,p))return 0;
        if(mode==2u){unsigned copy=ret-1u;
            if(!Pix_AddOperandPatch(plan,fn->instructions[copy-1u].operands.start,root)||!Pix_Erase(plan,copy))return 0;}
        if(mode==3u){unsigned slot=fn->phiPool[0].incomings.start;
            if(!Pix_AddIncomingPatch(plan,slot,a)||!Pix_AddIncomingPatch(plan,slot+1u,a)||
               !Pix_Erase(plan,copies[0])||!Pix_Erase(plan,copies[1]))return 0;}
    }
    f->base.a=root;f->base.p=p;return 1;
}
static int duplicate(SPixFixture *f,SZrExecIrFunction *fn) {
    unsigned c=Pix_Value(fn,1u,ZR_VALUE_TYPE_BOOL),a=Pix_Value(fn,1u,ZR_VALUE_TYPE_INT64);
    unsigned b=f->profile?Pix_Value(fn,1u,ZR_VALUE_TYPE_INT64):a,p=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);
    unsigned targets[2]={2u,2u},in[2]={a,b},start;SPixPlan *plan=f->plan;
    if(!c||!a||!b||!p||!Pix_Emit(fn,ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,c,0u,0u)||!Pix_Finish(fn,1u,0u))return 0;
    start=fn->instructionCount;
    if(!Pix_Emit(fn,ZR_EXEC_IR_OPCODE_RETURN,p,0u,0u)||!Pix_Finish(fn,2u,start)||
       !Pix_Edges(fn,1u,targets,2u)||!Pix_Phi(fn,2u,p,in,2u))return 0;
    plan->dom[0]=plan->dom[1]=1u;
    if(!f->profile&&!Pix_AddOperandPatch(plan,fn->instructions[1].operands.start,a))return 0;
    return f->profile||Pix_RemovePhi(plan,p);
}
static int second_join(SPixFixture *f,SZrExecIrFunction *fn,unsigned nonidentity) {
    unsigned c=Pix_Value(fn,1u,ZR_VALUE_TYPE_BOOL),a=Pix_Value(fn,1u,ZR_VALUE_TYPE_INT64);
    unsigned b=nonidentity?Pix_Value(fn,1u,ZR_VALUE_TYPE_INT64):a,p=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);
    unsigned q=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64),t[2],v[2],start,block,ret;SPixPlan *plan=f->plan;
    if(!c||!a||!b||!p||!q)return 0;
    for(block=1u;block<=7u;++block){start=fn->instructionCount;
        if(!Pix_Emit(fn,block==1u||block==4u?ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:block==7u?ZR_EXEC_IR_OPCODE_RETURN:ZR_EXEC_IR_OPCODE_BRANCH,
            block==1u||block==4u?c:block==7u?q:0u,0u,0u)||!Pix_Finish(fn,block,start))return 0;
        plan->dom[block-1u]=block<=4u?1u:4u;
    }
    t[0]=2u;t[1]=3u;if(!Pix_Edges(fn,1u,t,2u))return 0;t[0]=4u;
    if(!Pix_Edges(fn,2u,t,1u)||!Pix_Edges(fn,3u,t,1u))return 0;
    t[0]=5u;t[1]=6u;if(!Pix_Edges(fn,4u,t,2u))return 0;t[0]=7u;
    if(!Pix_Edges(fn,5u,t,1u)||!Pix_Edges(fn,6u,t,1u))return 0;
    v[0]=a;v[1]=b;if(!Pix_Phi(fn,4u,p,v,2u))return 0;
    v[0]=v[1]=p;if(!Pix_Phi(fn,7u,q,v,2u))return 0;
    ret=fn->blocks[6].terminatorInstructionId;
    if(!Pix_AddOperandPatch(plan,fn->instructions[ret-1u].operands.start,nonidentity?p:a)||!Pix_RemovePhi(plan,q))return 0;
    if(!nonidentity){unsigned slot=fn->phiPool[1].incomings.start;
        if(!Pix_RemovePhi(plan,p)||!Pix_AddIncomingPatch(plan,slot,a)||!Pix_AddIncomingPatch(plan,slot+1u,a))return 0;}
    return 1;
}
static int cycle(SPixFixture *f,SZrExecIrFunction *fn) {
    unsigned c=Pix_Value(fn,1u,ZR_VALUE_TYPE_BOOL),a=Pix_Value(fn,1u,ZR_VALUE_TYPE_INT64);
    unsigned p=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64),q=0u,start,block,t[2],v[2];
    if(f->profile==1u)q=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);
    if(!c||!a||!p||(f->profile==1u&&!q))return 0;
    if(f->profile==3u){unsigned x=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);
        f->rejection=1u;
        return x&&Pix_Emit(fn,ZR_EXEC_IR_OPCODE_COPY,x,0u,p)&&Pix_Emit(fn,ZR_EXEC_IR_OPCODE_COPY,p,0u,x)&&
            Pix_Emit(fn,ZR_EXEC_IR_OPCODE_RETURN,p,0u,0u)&&Pix_Finish(fn,1u,0u);}
    for(block=1u;block<=4u;++block){start=fn->instructionCount;
        if(block==3u&&f->profile==2u){q=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);
            if(!q||!Pix_Emit(fn,ZR_EXEC_IR_OPCODE_COPY,p,0u,q))return 0;}
        if(!Pix_Emit(fn,block==2u?ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:block==4u?ZR_EXEC_IR_OPCODE_RETURN:ZR_EXEC_IR_OPCODE_BRANCH,
           block==2u?c:block==4u?p:0u,0u,0u)||!Pix_Finish(fn,block,start))return 0;
        f->plan[0].dom[block-1u]=block<=2u?1u:2u;
    }
    t[0]=2u;if(!Pix_Edges(fn,1u,t,1u)||!Pix_Edges(fn,3u,t,1u))return 0;
    t[0]=3u;t[1]=4u;if(!Pix_Edges(fn,2u,t,2u))return 0;
    v[0]=a;v[1]=q?q:p;if(!Pix_Phi(fn,2u,p,v,2u))return 0;
    if(f->profile==1u){v[0]=a;v[1]=p;if(!Pix_Phi(fn,2u,q,v,2u))return 0;}
    /* The mixed visiting dependency is conservatively rejected as a whole;
     * neither its PHI nor dependent COPY invents an acyclic canonical root. */
    return 1;
}
static void mismatch(SZrExecIrValue *source,SZrExecIrValue *dest,unsigned kind) {
    static const unsigned owners[]={ZR_EXEC_IR_OWNERSHIP_UNIQUE,ZR_EXEC_IR_OWNERSHIP_SHARED,
        ZR_EXEC_IR_OWNERSHIP_BORROWED,ZR_EXEC_IR_OWNERSHIP_GC};
    if(kind==0u)dest->typeToken=ZR_VALUE_TYPE_BOOL;
    else if(kind==1u)source->nullability=ZR_EXEC_IR_NULLABILITY_NULLABLE;
    else if(kind==2u)dest->nullability=ZR_EXEC_IR_NULLABILITY_NULLABLE;
    else if(kind<=6u)dest->ownership=(EZrExecIrOwnership)owners[kind-3u];
    else dest->flags|=ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS|(kind==8u?ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE:0u);
}
static int descriptors(SPixFixture *f,SZrExecIrFunction *fn) {
    unsigned location=f->profile/9u,kind=f->profile%9u,mode=location==0u?3u:location==1u?2u:0u;
    SPixPlan *plan=f->plan;unsigned p,a=f->base.a,q=0u,i;
    if(!Pix_Diamond(f,fn,mode,0u))return 0;p=f->base.p;a=f->base.a;
    memset(plan,0,sizeof(*plan));for(i=0u;i<4u;++i)plan->dom[i]=1u;
    if(location==0u){
        for(i=1u;i<=2u;++i){unsigned inst=fn->blocks[i].instructions.start;
            unsigned v=fn->results[fn->instructions[inst].results.start];
            mismatch(fn->values+a-1u,fn->values+v-1u,kind);
        }
    }else if(location==1u){unsigned copy=fn->blocks[3].instructions.start;
        q=fn->results[fn->instructions[copy].results.start];
        mismatch(fn->values+p-1u,fn->values+q-1u,kind);
        if(kind!=1u){ /* p itself remains eligible; q must never be bypassed. */
            if(!Pix_AddOperandPatch(plan,fn->instructions[copy].operands.start,a)||!Pix_RemovePhi(plan,p))return 0;}
    }else if(location==2u)mismatch(fn->values+p-1u,fn->values+a-1u,kind);
    else mismatch(fn->values+a-1u,fn->values+p-1u,kind);
    for(i=0u;i<fn->instructionCount;++i)if(fn->instructions[i].results.count){
        unsigned result=fn->results[fn->instructions[i].results.start];
        fn->instructions[i].typeToken=fn->values[result-1u].typeToken;
    }
    return 1;
}
int Pix_Build(SPixFixture *f,unsigned id,unsigned profile,unsigned functions) {
    unsigned i,blocks=4u;SZrExecIrFunction *fn;
    Pix_Init(f,id,profile);
    if(id==4u)blocks=2u;
    if((id==5u&&profile==4u)||(id==6u&&profile==2u))blocks=7u;
    if(id==6u&&profile>=5u)blocks=6u;
    if(id==6u&&profile>=8u)blocks=5u;
    if(id==12u&&profile==3u)blocks=6u;
    if(id==9u&&profile==3u)blocks=1u;
    if(id==21u&&profile==0u)blocks=1u;
    if(!functions||functions>PIX_FUNCTIONS)return 0;
    for(i=0u;i<functions;++i){fn=Pix_AddFunction(f,blocks);if(!fn)return 0;
        if(id==4u){if(!duplicate(f,fn))return 0;}
        else if(id==5u&&profile==4u){if(!second_join(f,fn,0u))return 0;}
        else if(id==5u){if(!Pix_Diamond(f,fn,profile==0u?2u:profile==1u?3u:profile==2u?2u:3u,profile>=2u))return 0;}
        else if(id==6u&&profile==2u){if(!second_join(f,fn,1u))return 0;}
        else if(id==6u&&profile<5u){if(!Pix_Diamond(f,fn,profile==1u?4u:0u,profile==4u))return 0;
            if(profile>=3u){unsigned b;for(b=0u;b<fn->blockCount;++b)fn->blocks[b].immediateDominator=profile==3u?0u:2u;}}
        else if(id==8u){if(!descriptors(f,fn))return 0;}
        else if(id==9u){if(!cycle(f,fn))return 0;}
        else if(id==10u||id==12u||id==13u||(id==6u&&profile>=5u)){if(!Pix_EdgeShape(f,fn))return 0;}
        else if(id==21u&&profile==0u){unsigned v=Pix_Value(fn,1u,ZR_VALUE_TYPE_INT64);
            if(!v||!Pix_Emit(fn,ZR_EXEC_IR_OPCODE_RETURN,v,0u,0u)||!Pix_Finish(fn,1u,0u))return 0;f->plan[i].dom[0]=1u;}
        else {unsigned mode=id==7u&&profile&&i==1u?1u:id==21u?1u:id==15u&&profile==1u?1u:id==15u&&profile==2u?2u:0u;
            if(!Pix_Diamond(f,fn,mode,0u))return 0;}
        if(id==11u&&!Pix_Metadata(f,fn,profile))return 0;
    }
    if(id==14u)f->base.options.disabledPassMask=ZR_EXEC_IR_OPTIMIZE_DISABLE_DCE|
        (profile?ZR_EXEC_IR_OPTIMIZE_DISABLE_SCCP:0u);
    f->base.context.constants=f->base.module.constants;f->base.context.constantCount=f->base.module.constantCount;
    return 1;
}
unsigned Pix_Profiles(unsigned id) {
    static const unsigned counts[22]={0u,1u,1u,1u,2u,5u,10u,2u,36u,4u,5u,11u,7u,4u,2u,3u,1u,1u,1u,1u,1u,2u};
    return id<=21u?counts[id]:0u;
}
unsigned Pix_Repeats(unsigned id) { return id==8u||id==15u?3u:1u; }
