#include <stdlib.h>
#include <string.h>
#include "ssa_phi_identity_support.h"
#include "ssa_exec_ir_owned_module_shim.h"

static int call_state(SZrExecIrFunction *fn,unsigned id,unsigned effectIn,unsigned effectOut,unsigned version) {
    unsigned heap=ZR_EXEC_IR_MEMORY_MANAGED_HEAP,native=ZR_EXEC_IR_MEMORY_NATIVE_FFI;
    unsigned tokens[4]={ZR_EXEC_IR_MEMORY_TOKEN_MAKE(heap,1u),ZR_EXEC_IR_MEMORY_TOKEN_MAKE(native,1u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(heap,version),ZR_EXEC_IR_MEMORY_TOKEN_MAKE(native,version)};
    SZrExecIrRange range;SZrExecIrInstruction *in=fn->instructions+id-1u;
    if(!ZrCore_ExecIr_FunctionAppendMemoryTokens(fn,tokens,4u,&range))return 0;
    in->flags=ZR_EXEC_IR_FLAG_MAY_THROW|ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
    in->effectIn=effectIn;in->effectOut=effectOut;
    in->memoryIn=(SZrExecIrRange){.start=range.start,.count=2u};
    in->memoryOut=(SZrExecIrRange){.start=range.start+2u,.count=2u};return 1;
}
static int join_tokens(SZrExecIrFunction *fn,unsigned join,unsigned left,unsigned right,unsigned le,unsigned re,unsigned output) {
    unsigned i,regions[2]={ZR_EXEC_IR_MEMORY_NATIVE_FFI,ZR_EXEC_IR_MEMORY_MANAGED_HEAP};
    SZrExecIrPhiIncoming effect[2]={{left,le},{right,re}},memory[2];
    if(!ZrCore_ExecIr_FunctionSetEffectPhi(fn,join,output,effect,2u))return 0;
    for(i=0u;i<2u;++i){unsigned region=regions[i];
        memory[0]=(SZrExecIrPhiIncoming){left,ZR_EXEC_IR_MEMORY_TOKEN_MAKE(region,101u)};
        memory[1]=(SZrExecIrPhiIncoming){right,ZR_EXEC_IR_MEMORY_TOKEN_MAKE(region,102u)};
        if(!ZrCore_ExecIr_FunctionSetMemoryPhi(fn,join,(EZrExecIrMemoryClass)region,
            ZR_EXEC_IR_MEMORY_TOKEN_MAKE(region,103u),memory,2u))return 0;
    }
    return 1;
}
static int effect_diamond(SPixFixture *f,SZrExecIrFunction *fn,unsigned overlap) {
    unsigned c=Pix_Value(fn,1u,ZR_VALUE_TYPE_BOOL),a=Pix_Value(fn,1u,ZR_VALUE_TYPE_INT64);
    unsigned p=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64),block,start,out,id,t[2]={2u,3u},v[2]={a,a};
    unsigned tagged=ZR_EXEC_IR_MEMORY_MANAGED_HEAP,le=101u,re=102u;
    if(!c||!a||!p)return 0;
    if(overlap==1u){le=ZR_EXEC_IR_MEMORY_TOKEN_MAKE(tagged,101u);re=ZR_EXEC_IR_MEMORY_TOKEN_MAKE(tagged,102u);}
    for(block=1u;block<=4u;++block){start=fn->instructionCount;
        if(block==2u||block==3u){out=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);
            id=Pix_Emit(fn,ZR_EXEC_IR_OPCODE_CALL,0u,0u,out);
            if(!out||!id||!call_state(fn,id,overlap==1u?ZR_EXEC_IR_MEMORY_TOKEN_MAKE(tagged,1u):1u,
                block==2u?le:re,block==2u?101u:102u))return 0;}
        if(!Pix_Emit(fn,block==1u?ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:block==4u?ZR_EXEC_IR_OPCODE_RETURN:ZR_EXEC_IR_OPCODE_BRANCH,
            block==1u?c:block==4u?p:0u,0u,0u)||!Pix_Finish(fn,block,start))return 0;
        f->plan[0].dom[block-1u]=1u;
    }
    if(!Pix_Edges(fn,1u,t,2u))return 0;t[0]=4u;
    if(!Pix_Edges(fn,2u,t,1u)||!Pix_Edges(fn,3u,t,1u)||!Pix_Phi(fn,4u,p,v,2u)||
       !join_tokens(fn,4u,2u,3u,le,re,overlap==1u?ZR_EXEC_IR_MEMORY_TOKEN_MAKE(tagged,103u):103u))return 0;
    if(overlap==1u){ /* Real overlapping effect and managed-memory ranges. */
        /* Drop the now-unused tagged tail during construction: Core checks
         * every logical incoming row's declared domain. Baseline capture is
         * after this valid shape is finalized; optimization cannot shrink it. */
        fn->phiIncomingCount=fn->blocks[3].memoryPhiIncomings[tagged].start;
        fn->blocks[3].memoryPhiIncomings[tagged]=fn->blocks[3].effectPhiIncomings;
    }
    f->base.a=a;f->base.p=p;
    return Pix_AddOperandPatch(f->plan,fn->instructions[fn->instructionCount-1u].operands.start,a)&&Pix_RemovePhi(f->plan,p);
}
static int ordinary_effect_overlap(SPixFixture *f,SZrExecIrFunction *fn,unsigned legacy) {
    unsigned c=Pix_Value(fn,1u,ZR_VALUE_TYPE_BOOL),a=Pix_Value(fn,1u,ZR_VALUE_TYPE_INT64);
    unsigned p=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64),q[2]={a,a},block,start,t[2],v[2],id,out;
    unsigned merge=legacy?6u:4u,n=legacy?6u:4u;
    if(legacy){q[0]=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);q[1]=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);}
    if(!c||!a||!p||!q[0]||!q[1])return 0;
    for(block=1u;block<=n;++block){start=fn->instructionCount;
        if((!legacy&&block==1u)||(legacy&&(block==2u||block==3u))){
            out=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);id=Pix_Emit(fn,ZR_EXEC_IR_OPCODE_CALL,0u,0u,out);
            if(!out||!id||!call_state(fn,id,1u,legacy?q[block-2u]:a,legacy?99u+block:2u))return 0;
        }
        if(legacy&&(block==4u||block==5u)&&!Pix_Emit(fn,ZR_EXEC_IR_OPCODE_COPY,a,0u,q[block-4u]))return 0;
        if(!Pix_Emit(fn,block==1u?ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:block==merge?ZR_EXEC_IR_OPCODE_RETURN:ZR_EXEC_IR_OPCODE_BRANCH,
            block==1u?c:block==merge?p:0u,0u,0u)||!Pix_Finish(fn,block,start))return 0;
        f->plan[0].dom[block-1u]=legacy&&block==4u?2u:legacy&&block==5u?3u:1u;
    }
    t[0]=2u;t[1]=3u;if(!Pix_Edges(fn,1u,t,2u))return 0;
    if(legacy){t[0]=4u;if(!Pix_Edges(fn,2u,t,1u))return 0;t[0]=5u;if(!Pix_Edges(fn,3u,t,1u))return 0;
        t[0]=merge;if(!Pix_Edges(fn,4u,t,1u)||!Pix_Edges(fn,5u,t,1u))return 0;
    }else {t[0]=merge;if(!Pix_Edges(fn,2u,t,1u)||!Pix_Edges(fn,3u,t,1u))return 0;}
    v[0]=q[0];v[1]=q[1];if(!Pix_Phi(fn,merge,p,v,2u))return 0;
    if(legacy&&!join_tokens(fn,merge,4u,5u,q[0],q[1],10u))return 0;
    fn->blocks[merge-1u].effectPhiResult=legacy?10u:p;
    fn->blocks[merge-1u].effectPhiIncomings=fn->phiPool[0].incomings;
    /* Ordinary rows genuinely also carry terminal effect tokens. Neither old
     * COPY nor new PHI writers may normalize those bytes as ValueIds. */
    f->base.a=a;f->base.p=p;return 1;
}
static int throwing(SPixFixture *f,SZrExecIrFunction *fn) {
    unsigned c=Pix_Value(fn,1u,ZR_VALUE_TYPE_BOOL),a=Pix_Value(fn,1u,ZR_VALUE_TYPE_INT64);
    unsigned root=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64),p=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);
    unsigned block,start,t[3],v[3],invoke=0u,ret=0u,join=f->profile!=5u;
    if(!c||!a||!root||!p)return 0;fn->blocks[2].flags|=ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION;
    for(block=1u;block<=6u;++block){start=fn->instructionCount;
        if(block==1u){invoke=Pix_Emit(fn,ZR_EXEC_IR_OPCODE_INVOKE,0u,0u,root);
            if(!invoke||!call_state(fn,invoke,1u,2u,2u))return 0;}
        else if(block==2u){if(!Pix_Emit(fn,ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,c,0u,0u))return 0;}
        else if(block==3u&&!join){if(!Pix_Emit(fn,ZR_EXEC_IR_OPCODE_RETURN,a,0u,0u))return 0;}
        else if(block==6u){ret=Pix_Emit(fn,ZR_EXEC_IR_OPCODE_RETURN,p,0u,0u);if(!ret)return 0;}
        else if(!Pix_Emit(fn,ZR_EXEC_IR_OPCODE_BRANCH,0u,0u,0u))return 0;
        if(!Pix_Finish(fn,block,start))return 0;
        f->plan[0].dom[block-1u]=block==4u||block==5u||(!join&&block==6u)?2u:1u;
    }
    t[0]=2u;t[1]=3u;if(!Pix_Edges(fn,1u,t,2u))return 0;
    t[0]=4u;t[1]=5u;if(!Pix_Edges(fn,2u,t,2u))return 0;
    t[0]=6u;if(!Pix_Edges(fn,4u,t,1u)||!Pix_Edges(fn,5u,t,1u))return 0;
    if(join&&!Pix_Edges(fn,3u,t,1u))return 0;
    v[0]=v[1]=root;v[2]=f->profile==7u?a:root;
    if(!Pix_Phi(fn,6u,p,v,join?3u:2u))return 0;
    if(f->profile==6u)f->rejection=1u; /* Root unavailable after exceptional rejoin. */
    if(f->profile==5u&&!Pix_AddOperandPatch(f->plan,fn->instructions[ret-1u].operands.start,root))return 0;
    return f->profile!=5u||Pix_RemovePhi(f->plan,p);
}
static int reverse_producer(SPixFixture *f,SZrExecIrFunction *fn) {
    unsigned c=Pix_Value(fn,1u,ZR_VALUE_TYPE_BOOL),a=Pix_Value(fn,1u,ZR_VALUE_TYPE_INT64);
    unsigned b=Pix_Value(fn,1u,ZR_VALUE_TYPE_INT64),root=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);
    unsigned p=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64),block,start,t[2],v[2]={root,root};
    unsigned invalid=f->profile==8u,dom[5]={1u,3u,1u,3u,3u};
    if(!c||!a||!b||!root||!p)return 0;
    for(block=1u;block<=5u;++block){start=fn->instructionCount;
        if(block==3u&&!Pix_Emit(fn,ZR_EXEC_IR_OPCODE_ADD,a,b,root))return 0;
        if(!Pix_Emit(fn,block==2u?ZR_EXEC_IR_OPCODE_RETURN:
            ((invalid&&block==1u)||(!invalid&&block==3u))?ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:ZR_EXEC_IR_OPCODE_BRANCH,
            block==2u?p:((invalid&&block==1u)||(!invalid&&block==3u))?c:0u,0u,0u)||!Pix_Finish(fn,block,start))return 0;
        fn->blocks[block-1u].immediateDominator=3u;f->plan[0].dom[block-1u]=dom[block-1u];
    }
    t[0]=3u;t[1]=5u;if(!Pix_Edges(fn,1u,t,invalid?2u:1u))return 0;
    t[0]=4u;t[1]=5u;if(!Pix_Edges(fn,3u,t,invalid?1u:2u))return 0;
    t[0]=2u;if(!Pix_Edges(fn,4u,t,1u)||!Pix_Edges(fn,5u,t,1u)||!Pix_Phi(fn,2u,p,v,2u))return 0;
    f->rejection=invalid;
    return invalid||(Pix_AddOperandPatch(f->plan,fn->instructions[fn->blocks[1].terminatorInstructionId-1u].operands.start,root)&&Pix_RemovePhi(f->plan,p));
}
static int tombstone(SPixFixture *f,SZrExecIrFunction *fn) {
    unsigned ret,a,p,consume,out=0u,id,oldCount,profile=f->profile;
    SZrExecIrInstruction old;SZrExecIrRange memory;unsigned tokens[2],region=ZR_EXEC_IR_MEMORY_OWNERSHIP;
    if(!Pix_Diamond(f,fn,0u,0u))return 0;a=f->base.a;p=f->base.p;
    memset(f->plan,0,sizeof(f->plan));for(id=0u;id<4u;++id)f->plan[0].dom[id]=1u;
    consume=profile==1u||profile==3u?p:a;ret=fn->blocks[3].terminatorInstructionId;
    old=fn->instructions[ret-1u];oldCount=fn->instructionCount;
    if(profile<2u){out=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);if(!out)return 0;}
    if(profile==4u){fn->values[a-1u].ownership=ZR_EXEC_IR_OWNERSHIP_UNIQUE;
        fn->values[p-1u].ownership=ZR_EXEC_IR_OWNERSHIP_UNIQUE;fn->blocks[3].flags|=ZR_EXEC_IR_BLOCK_FLAG_CLEANUP;}
    id=Pix_Emit(fn,profile<2u?ZR_EXEC_IR_OPCODE_MOVE:profile==4u?ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED:ZR_EXEC_IR_OPCODE_DROP,
        consume,0u,out);if(!id)return 0;
    if(profile<2u){fn->instructions[id-1u].effectIn=1u;fn->instructions[id-1u].effectOut=2u;}
    if(profile>=2u){tokens[0]=ZR_EXEC_IR_MEMORY_TOKEN_MAKE(region,1u);tokens[1]=ZR_EXEC_IR_MEMORY_TOKEN_MAKE(region,2u);
        if(!ZrCore_ExecIr_FunctionAppendMemoryTokens(fn,tokens,2u,&memory))return 0;
        fn->instructions[id-1u].memoryIn=(SZrExecIrRange){.start=memory.start,.count=1u};
        fn->instructions[id-1u].memoryOut=(SZrExecIrRange){.start=memory.start+1u,.count=1u};
        fn->instructions[id-1u].effectIn=1u;fn->instructions[id-1u].effectOut=2u;
    }
    fn->instructions[ret-1u]=fn->instructions[id-1u];fn->instructions[id-1u]=old;
    if(out)fn->values[out-1u].definition=ret;
    fn->blocks[3].terminatorInstructionId=id;fn->blocks[3].instructions.count=fn->instructionCount-oldCount+1u;
    /* MOVE result is unused but MOVE remains observable under its schema. */
    return 1;
}
static int storage(SPixFixture *f,SZrExecIrFunction *fn) {
    unsigned profile=f->profile,ret,i,p,a,inputs[2];SPixPlan *plan=f->plan;
    if(!Pix_Diamond(f,fn,profile==0u?2u:profile==1u?1u:0u,0u))return 0;
    p=f->base.p;a=f->base.a;ret=fn->instructionCount;
    if(profile==0u){
        fn->sourceMaps=calloc(2u,sizeof(*fn->sourceMaps));if(!fn->sourceMaps)return 0;
        fn->sourceMapCount=fn->sourceMapCapacity=2u;
        for(i=0u;i<2u;++i){fn->sourceMaps[i].instructionId=ret-1u+i;
            fn->sourceMaps[i].sourceId=fn->instructions[ret-2u+i].sourceId;}
    }else if(profile==1u){unsigned q=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);
        inputs[0]=inputs[1]=a;if(!q||!Pix_Phi(fn,4u,q,inputs,2u))return 0;
        /* Keep the distinct PHI live via the condition-independent return;
         * identity q is genuinely dead, so this isolates logical compaction. */
        if(!Pix_RemovePhi(plan,q))return 0;
    }else {SZrExecIrInstruction old=fn->instructions[ret-1u];unsigned q=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64),copy;
        copy=Pix_Emit(fn,ZR_EXEC_IR_OPCODE_COPY,p,0u,q);if(!q||!copy)return 0;
        fn->instructions[ret-1u]=fn->instructions[copy-1u];fn->instructions[copy-1u]=old;
        fn->values[q-1u].definition=ret;fn->blocks[3].terminatorInstructionId=copy;++fn->blocks[3].instructions.count;
        /* COPY and RETURN genuinely own the same backing operand slot. The
         * safe all-owner transform and conservative skip are both explicit. */
        fn->instructions[ret-1u].operands=old.operands;
        if(profile==3u){fn->operands[old.operands.start]=q;f->rejection=1u;}
        else {
            f->optional=1u;
            if(!Pix_AddOperandPatch(plan,old.operands.start,a)||!Pix_Erase(plan,ret))return 0;
            /* q has no uses even if the shared p slot is preserved. */
            for(i=0u;i<plan->patches;++i)if(plan->operand[i].slot==old.operands.start)plan->operand[i].value=a;
        }
    }
    return 1;
}
int Pix_EdgeShape(SPixFixture *f,SZrExecIrFunction *fn) {
    unsigned profile=f->profile;
    if(f->id==6u)return profile>=8u?reverse_producer(f,fn):throwing(f,fn);
    if(f->id==10u)return tombstone(f,fn);
    if(f->id==13u)return storage(f,fn);
    if(profile<=1u)return effect_diamond(f,fn,profile);
    if(profile==2u||profile==3u)return ordinary_effect_overlap(f,fn,profile==3u);
    if(!Pix_Diamond(f,fn,profile==6u?3u:0u,0u))return 0;
    if(profile==4u){unsigned q=Pix_Value(fn,0u,ZR_VALUE_TYPE_INT64);SZrExecIrPhi phi=fn->phiPool[0];
        phi.result=q;
        if(!q||!ZrCore_ExecIr_FunctionAppendPhis(fn,&phi,1u,&fn->blocks[3].phis))return 0;
        /* Add the original PHI to the same active range; both own its rows. */
        fn->blocks[3].phis.start=0u;fn->blocks[3].phis.count=2u;
        if(!Pix_RemovePhi(f->plan,q))return 0;
    }else {
        fn->instructions[fn->instructionCount-1u].phiRange=fn->phiPool[0].incomings;
        /* Instruction phiRange is an additional use whose alias semantics
         * are not proved in this finite feature. Keep the complete identity
         * and both old/new incoming paths conservatively unchanged. */
        f->plan[0].patches=f->plan[0].incomingPatches=0u;
        f->plan[0].removed=f->plan[0].erased=0u;
    }
    return 1;
}
