#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "ssa_sccp_pooled_int64_fixture.h"
#include "ssa_exec_ir_owned_module_shim.h"
static SPiTotals g_totals;
void Pi_SetUp(void) { ZrTests_ExecOwner_Reset(); }
void Pi_TearDown(void) { ZrTests_ExecOwner_Disarm(); }
SPiTotals Pi_Totals(void) { return g_totals; }
void Pi_NoteFault(unsigned a, unsigned b) { g_totals.faultMeasured += a; g_totals.faultCompleted += b; }
void Pi_Init(SPiFixture *f) {
    memset(f, 0, sizeof(*f)); ZrCore_ExecIr_ModuleInit(&f->module);
    ZrParser_ExecIr_AnalysisCacheInit(&f->cache);
    ZrParser_ExecIr_RemarkSinkInit(&f->remarks);
    ZrParser_ExecIr_PassFailureInit(&f->failure);
    f->module.id = 1u; f->module.moduleHash = UINT64_C(0x53434350);
    f->options.disabledPassMask = ZR_EXEC_IR_OPTIMIZE_DISABLE_DCE;
    f->options.failure = &f->failure; f->source = 1000u;
}
int Pi_Dispose(SPiFixture *f) {
    SZrOwnerHeapStats st;
    ZrTests_ExecOwner_Disarm(); ZrCore_ExecIr_FreeModule(&f->module);
    ZrParser_ExecIr_AnalysisCacheFree(&f->cache);
    ZrParser_ExecIr_PassFailureFree(&f->failure);
    ZrParser_ExecIr_RemarkSinkFree(&f->remarks);
    st = ZrTests_ExecOwner_Stats();
    g_totals.attempts += st.attempts; g_totals.live += st.live; g_totals.errors += st.errors;
    /* Recovery is containment only. Observed nonzero ledger still rejects the case. */
    if (st.live != 0u) ZrTests_ExecOwner_Recover();
    return st.live == 0u && st.errors == 0u;
}
static unsigned pi_pool(SPiFixture *f, unsigned type, TZrUInt64 bits, unsigned flags) {
    SZrExecIrConstant c = {type, flags, bits}; SZrExecIrRange range;
    if (!ZrCore_ExecIr_ModuleAppendConstant(&f->module, &c, 1u, &range)) return UINT32_MAX;
    return range.start;
}
static SZrExecIrFunction *pi_function(SPiFixture *f) {
    TZrExecIrFunctionId id;
    if (!ZrCore_ExecIr_ModuleAddFunction(&f->module,
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 90u + f->module.functionCount),
            UINT64_C(0x494e543634), &id)) return NULL;
    return ZrCore_ExecIr_ModuleFunctionAt(&f->module, id);
}
static unsigned pi_emit(SPiFixture *f, SZrExecIrFunction *fn, unsigned opcode,
                        unsigned type, const unsigned *operands, unsigned count,
                        unsigned layout, unsigned flags, unsigned effect) {
    SZrExecIrInstruction in; TZrExecIrInstructionId id;
    unsigned v = 0u;
    memset(&in, 0, sizeof(in)); in.opcode = (TZrUInt16)opcode;
    in.typeToken = type; in.layoutId = layout; in.sourceId = f->source++;
    in.flags = (TZrUInt16)flags;
    if (effect) { in.effectIn = 1u; in.effectOut = 2u; }
    if (opcode != ZR_EXEC_IR_OPCODE_RETURN && opcode != ZR_EXEC_IR_OPCODE_BRANCH &&
        opcode != ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH) {
        v = ZrCore_ExecIr_FunctionAddValue(fn, type,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        if (!v || !ZrCore_ExecIr_FunctionAppendResults(fn, &v, 1u, &in.results)) return 0u;
    }
    if (count && !ZrCore_ExecIr_FunctionAppendOperands(fn, operands, count, &in.operands)) return 0u;
    if (!ZrCore_ExecIr_FunctionAppendInstruction(fn, &in, &id)) return 0u;
    return v ? v : id;
}
static unsigned pi_constant(SPiFixture *f, SZrExecIrFunction *fn, unsigned index, unsigned type) {
    return pi_emit(f, fn, ZR_EXEC_IR_OPCODE_CONSTANT, type, NULL, 0u, index, 0u, 0u);
}
static unsigned pi_op(SPiFixture *f, SZrExecIrFunction *fn, unsigned opcode,
                      unsigned type, unsigned a, unsigned b, unsigned flags) {
    unsigned operands[2] = {a,b};
    return pi_emit(f, fn, opcode, type, operands, opcode == ZR_EXEC_IR_OPCODE_NEG ||
            opcode == ZR_EXEC_IR_OPCODE_COPY ? 1u : 2u, 0u, flags,
            opcode == ZR_EXEC_IR_OPCODE_DIV);
}
static int pi_target(SPiFixture *f, SZrExecIrFunction *fn, unsigned v,
                      unsigned fold, TZrUInt64 bits) {
    SPiTarget *t;
    if (!v || f->targetCount == PI_MAX_TARGETS) return 0;
    t = &f->targets[f->targetCount++]; t->function = fn->id - 1u;
    t->original = fn->instructions[fn->values[v-1u].definition - 1u];
    t->source = t->original.sourceId; t->fold = fold; t->bits = bits; return 1;
}
static int pi_end(SPiFixture *f, SZrExecIrFunction *fn, unsigned value, unsigned block, unsigned start) {
    unsigned id = pi_emit(f, fn, ZR_EXEC_IR_OPCODE_RETURN, 0u, &value, 1u, 0u, 0u, 0u);
    if (!id) return 0;
    fn->blocks[block-1u].instructions = (SZrExecIrRange){.start=start,.count=fn->instructionCount-start};
    fn->blocks[block-1u].terminatorInstructionId = fn->instructionCount; return 1;
}
static int pi_chain(SPiFixture *f, SZrExecIrFunction *fn, unsigned source, unsigned two) {
    unsigned a = pi_op(f,fn,ZR_EXEC_IR_OPCODE_ADD,ZR_VALUE_TYPE_INT64,source,two,0u);
    unsigned n = pi_op(f,fn,ZR_EXEC_IR_OPCODE_NEG,ZR_VALUE_TYPE_INT64,a,0u,0u);
    unsigned z = pi_op(f,fn,ZR_EXEC_IR_OPCODE_ADD,ZR_VALUE_TYPE_INT64,n,two,0u);
    return a && n && z && pi_target(f,fn,a,0u,0u) && pi_target(f,fn,n,0u,0u) &&
           pi_target(f,fn,z,0u,0u) && pi_end(f,fn,z,fn->blockCount,
                   fn->blocks[fn->blockCount-1u].instructions.start);
}
static int pi_phi_chain(SPiFixture *f, unsigned origin, unsigned unknown) {
    SZrExecIrFunction *fn = pi_function(f); unsigned b,a,two,condition,p,term,other;
    unsigned edges[2]; SZrExecIrRange r; SZrExecIrPhi phi; SZrExecIrPhiIncoming incoming[2];
    if (!fn) return 0;
    for (b=1u;b<=4u;++b) if (ZrCore_ExecIr_FunctionAddBlock(fn,b==1u?ZR_EXEC_IR_BLOCK_FLAG_ENTRY:0u)!=b) return 0;
    a=pi_constant(f,fn,0u,origin); two=pi_constant(f,fn,1u,ZR_VALUE_TYPE_INT64);
    condition=ZrCore_ExecIr_FunctionAddExternalValue(fn,ZR_VALUE_TYPE_BOOL,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    if (!a || !two || !condition) return 0;
    term=pi_emit(f,fn,ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,0u,&condition,1u,0u,0u,0u);
    edges[0]=2u;edges[1]=3u;
    if (!term || !ZrCore_ExecIr_FunctionAppendSuccessors(fn,edges,2u,&r)) return 0;
    fn->instructions[term-1u].successorRange=r;fn->blocks[0].successors=r;
    fn->blocks[0].instructions=(SZrExecIrRange){.start=0u,.count=fn->instructionCount};fn->blocks[0].terminatorInstructionId=term;
    for (b=2u;b<=3u;++b) {
        unsigned start=fn->instructionCount;
        term=pi_emit(f,fn,ZR_EXEC_IR_OPCODE_BRANCH,0u,NULL,0u,0u,0u,0u);
        edges[0]=4u;
        if (!term || !ZrCore_ExecIr_FunctionAppendSuccessors(fn,edges,1u,&r)) return 0;
        fn->instructions[term-1u].successorRange=r;fn->blocks[b-1u].successors=r;
        edges[0]=1u;
        if (!ZrCore_ExecIr_FunctionAppendPredecessors(fn,edges,1u,&fn->blocks[b-1u].predecessors)) return 0;
        fn->blocks[b-1u].instructions=(SZrExecIrRange){.start=start,.count=1u};fn->blocks[b-1u].terminatorInstructionId=term;
    }
    edges[0]=2u;edges[1]=3u;
    if (!ZrCore_ExecIr_FunctionAppendPredecessors(fn,edges,2u,&fn->blocks[3].predecessors)) return 0;
    p=ZrCore_ExecIr_FunctionAddValue(fn,ZR_VALUE_TYPE_INT64,ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    other=unknown?ZrCore_ExecIr_FunctionAddExternalValue(fn,ZR_VALUE_TYPE_INT64,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN):a;
    if(!other)return 0;
    incoming[0]=(SZrExecIrPhiIncoming){2u,a};incoming[1]=(SZrExecIrPhiIncoming){3u,other};
    phi.result=p;
    if (!p || !ZrCore_ExecIr_FunctionAppendPhiIncoming(fn,incoming,2u,&phi.incomings) ||
        !ZrCore_ExecIr_FunctionAppendPhis(fn,&phi,1u,&fn->blocks[3].phis)) return 0;
    fn->blocks[3].instructions.start=fn->instructionCount;
    return pi_chain(f,fn,p,two);
}
/* Explicit real CFG metadata. No interpreter/evaluator is involved. */
static int pi_jump(SPiFixture *f,SZrExecIrFunction *fn,unsigned block,
                   unsigned start,unsigned condition,const unsigned *edges,unsigned count) {
    SZrExecIrRange range;unsigned term=pi_emit(f,fn,condition?
            ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:ZR_EXEC_IR_OPCODE_BRANCH,0u,
            condition?&condition:NULL,condition?1u:0u,0u,0u,0u);
    if(!term||!ZrCore_ExecIr_FunctionAppendSuccessors(fn,edges,count,&range))return 0;
    fn->instructions[term-1u].successorRange=range;fn->blocks[block-1u].successors=range;
    fn->blocks[block-1u].instructions=(SZrExecIrRange){.start=start,.count=fn->instructionCount-start};
    fn->blocks[block-1u].terminatorInstructionId=term;return 1;
}
static int pi_predecessors(SZrExecIrFunction *fn,unsigned block,
                           unsigned a,unsigned b,unsigned count) {
    unsigned ids[2]={a,b};
    return ZrCore_ExecIr_FunctionAppendPredecessors(fn,ids,count,&fn->blocks[block-1u].predecessors);
}
static unsigned pi_merge(SZrExecIrFunction *fn,unsigned block,unsigned type,
                          unsigned fromA,unsigned valueA,unsigned fromB,unsigned valueB) {
    SZrExecIrPhi phi;SZrExecIrPhiIncoming incoming[2]={{fromA,valueA},{fromB,valueB}};
    phi.result=ZrCore_ExecIr_FunctionAddValue(fn,type,ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    if(!phi.result||!ZrCore_ExecIr_FunctionAppendPhiIncoming(fn,incoming,2u,&phi.incomings)||
            !ZrCore_ExecIr_FunctionAppendPhis(fn,&phi,1u,&fn->blocks[block-1u].phis))return 0u;
    return phi.result;
}
static int pi_unknown_branch_chain(SPiFixture *f) {
    SZrExecIrFunction *fn=pi_function(f);unsigned i,a,two,fifty,yes,choose,unknown,condition,merged,start;
    unsigned edges[2];
    if(!fn||pi_pool(f,ZR_VALUE_TYPE_BOOL,1u,0u)!=2u||pi_pool(f,ZR_VALUE_TYPE_INT64,50u,0u)!=3u)return 0;
    for(i=1u;i<=7u;++i){
        if(ZrCore_ExecIr_FunctionAddBlock(fn,i==1u?ZR_EXEC_IR_BLOCK_FLAG_ENTRY:0u)!=i)return 0;
    }
    a=pi_constant(f,fn,0u,ZR_VALUE_TYPE_INT64);two=pi_constant(f,fn,1u,ZR_VALUE_TYPE_INT64);
    yes=pi_constant(f,fn,2u,ZR_VALUE_TYPE_BOOL);fifty=pi_constant(f,fn,3u,ZR_VALUE_TYPE_INT64);
    choose=ZrCore_ExecIr_FunctionAddExternalValue(fn,ZR_VALUE_TYPE_BOOL,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    unknown=ZrCore_ExecIr_FunctionAddExternalValue(fn,ZR_VALUE_TYPE_BOOL,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    if(!a||!two||!yes||!fifty||!choose||!unknown)return 0;
    edges[0]=2u;edges[1]=3u;
    if(!pi_jump(f,fn,1u,0u,choose,edges,2u))return 0;
    for(i=2u;i<=3u;++i){
        start=fn->instructionCount;edges[0]=4u;
        if(!pi_predecessors(fn,i,1u,0u,1u)||!pi_jump(f,fn,i,start,0u,edges,1u))return 0;
    }
    if(!pi_predecessors(fn,4u,2u,3u,2u))return 0;
    condition=pi_merge(fn,4u,ZR_VALUE_TYPE_BOOL,2u,yes,3u,unknown);
    start=fn->instructionCount;edges[0]=5u;edges[1]=6u;
    if(!condition||!pi_jump(f,fn,4u,start,condition,edges,2u))return 0;
    for(i=5u;i<=6u;++i){
        start=fn->instructionCount;edges[0]=7u;
        if(!pi_predecessors(fn,i,4u,0u,1u)||!pi_jump(f,fn,i,start,0u,edges,1u))return 0;
    }
    if(!pi_predecessors(fn,7u,5u,6u,2u))return 0;
    merged=pi_merge(fn,7u,ZR_VALUE_TYPE_INT64,5u,a,6u,fifty);
    fn->blocks[6].instructions.start=fn->instructionCount;
    /* UNKNOWN+true is not a proven branch condition. Both normal successors
     * feed distinct Int64 values; none of this pure consumer chain may fold. */
    return merged&&pi_chain(f,fn,merged,two);
}
int Pi_Build(SPiFixture *f, unsigned id, unsigned profile) {
    unsigned i,n=1u,type=ZR_VALUE_TYPE_INT64,op=ZR_EXEC_IR_OPCODE_ADD,flags=0u;
    TZrUInt64 left=40u,right=2u,expected=42u;
    Pi_Init(f);
    if (id==3u) {left=2u;right=40u;op=ZR_EXEC_IR_OPCODE_SUB;expected=UINT64_C(0)-38u;}
    if (id==4u) {left=UINT64_C(0)-7u;right=6u;op=ZR_EXEC_IR_OPCODE_MUL;expected=UINT64_C(0)-42u;}
    if (id==2u) op=ZR_EXEC_IR_OPCODE_ARITHMETIC;
    if (id==5u || id==6u) {left=id==5u?42u:UINT64_C(0)-42u;op=ZR_EXEC_IR_OPCODE_NEG;expected=UINT64_C(0)-left;}
    if (id==7u || id==8u || id==13u || id==25u || id==26u || id==38u) {
        op=ZR_EXEC_IR_OPCODE_DIV;left=id==8u?UINT64_C(0)-85u:84u;right=2u;flags=ZR_EXEC_IR_FLAG_MAY_THROW;
        if(id==25u){left=42u;right=0u;} if(id==26u){left=UINT64_C(1)<<63;right=UINT64_MAX;}
    }
    if(id==9u){left=UINT32_MAX;right=1u;expected=UINT64_C(1)<<32;}
    if(id==10u){left=(UINT64_C(1)<<63)+1u;right=1u;op=ZR_EXEC_IR_OPCODE_SUB;expected=UINT64_C(1)<<63;}
    if(id==11u){left=INT64_MAX-1u;right=1u;expected=INT64_MAX;}
    if(id==12u){left=UINT64_C(1)<<63;right=0u;op=ZR_EXEC_IR_OPCODE_MUL;expected=0u;}
    if(id==21u){left=INT64_MAX;right=1u;}
    if(id==22u){left=UINT64_C(1)<<63;right=1u;op=ZR_EXEC_IR_OPCODE_SUB;}
    if(id==23u){left=INT64_MAX;right=2u;op=ZR_EXEC_IR_OPCODE_MUL;}
    if(id==24u){left=UINT64_C(1)<<63;op=ZR_EXEC_IR_OPCODE_NEG;}
    if(id==27u || id==29u || (id==30u&&profile<2u)) type=(profile?ZR_VALUE_TYPE_UINT64:ZR_VALUE_TYPE_DOUBLE);
    if(id==28u) type=ZR_VALUE_TYPE_UINT64;
    if(id==31u) type=ZR_VALUE_TYPE_INT32;
    if(id==33u)flags=ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
    if(id==34u)flags=ZR_EXEC_IR_FLAG_MAY_GC;
    if(id==35u)flags=ZR_EXEC_IR_FLAG_MAY_SUSPEND;
    if(id==36u)flags=ZR_EXEC_IR_FLAG_DEBUG_POLL;
    if(id==37u)flags=ZR_EXEC_IR_FLAG_GUARD_EXIT;
    if(id==40u)flags=ZR_EXEC_IR_FLAG_MAY_THROW;
    if(id!=41u && id!=44u) {
        if(pi_pool(f,type,left,0u)!=0u || pi_pool(f,(id>=29u&&id<=31u)?ZR_VALUE_TYPE_INT64:type,right,0u)!=1u) return 0;
        if(id==7u || id==13u || id==25u || id==26u) if(pi_pool(f,ZR_VALUE_TYPE_INT64,id==13u?0u:1u,0u)==UINT32_MAX)return 0;
        if(id==14u && pi_pool(f,ZR_VALUE_TYPE_INT64,42u,0u)==UINT32_MAX)return 0;
        if(id==17u && pi_pool(f,ZR_VALUE_TYPE_INT64,42u,UINT32_C(0x80000000))==UINT32_MAX)return 0;
        if(id==20u) {
            if(pi_pool(f,ZR_VALUE_TYPE_BOOL,42u,0u)==UINT32_MAX || pi_pool(f,ZR_VALUE_TYPE_UINT64,42u,0u)==UINT32_MAX ||
               pi_pool(f,ZR_VALUE_TYPE_DOUBLE,42u,0u)==UINT32_MAX)return 0;
        }
    }
    if(id==18u) {if(pi_pool(f,ZR_VALUE_TYPE_INT64,3u,0u)==UINT32_MAX || pi_pool(f,ZR_VALUE_TYPE_INT64,5u,0u)==UINT32_MAX)return 0;}
    if(id==30u) {
        if(profile>=PI_PHI_PROFILES)return 0;
        if(profile==3u){if(!pi_unknown_branch_chain(f))return 0;}
        else if(!pi_phi_chain(f,type,profile==2u))return 0;
        goto done;
    }
    if(id==16u || id==18u || id==48u || id>=49u)n=2u;
    for(i=0u;i<n;++i) {
        SZrExecIrFunction *fn=pi_function(f);unsigned a,b,v;
        if(!fn || ZrCore_ExecIr_FunctionAddBlock(fn,ZR_EXEC_IR_BLOCK_FLAG_ENTRY)!=1u)return 0;
        a=pi_constant(f,fn,(id==41u||id==44u)?40u:0u,type);
        b=pi_constant(f,fn,(id==41u||id==44u)?2u:1u,(id>=29u&&id<=31u)?ZR_VALUE_TYPE_INT64:type);
        if(!a||!b)return 0;
        if(id==20u && (!pi_constant(f,fn,2u,ZR_VALUE_TYPE_BOOL)||!pi_constant(f,fn,3u,ZR_VALUE_TYPE_UINT64)||!pi_constant(f,fn,4u,ZR_VALUE_TYPE_DOUBLE)))return 0;
        if(id==29u || id==31u) {
            if(id==29u || !profile) a=pi_op(f,fn,ZR_EXEC_IR_OPCODE_COPY,ZR_VALUE_TYPE_INT64,a,0u,0u);
            if(!a || !pi_chain(f,fn,a,b))return 0;continue;
        }
        if(id==18u) {
            unsigned j,current=a;TZrUInt64 bits=40u;
            for(j=0u;j<14u;++j){current=pi_op(f,fn,ZR_EXEC_IR_OPCODE_ADD,ZR_VALUE_TYPE_INT64,current,b,0u);bits+=2u;
                if(!pi_target(f,fn,current,1u,bits))return 0;}
            if(!pi_end(f,fn,current,1u,0u))return 0;continue;
        }
        if(id==32u) {
            v=pi_op(f,fn,ZR_EXEC_IR_OPCODE_COMPARE,ZR_VALUE_TYPE_BOOL,a,b,0u);
            if(!v)return 0;fn->instructions[fn->values[v-1u].definition-1u].typeToken=ZR_EXEC_IR_COMPARE_KIND_EQUAL;
        } else v=pi_op(f,fn,op,type,a,b,flags);
        if(!v)return 0;
        if(id==39u) {
            const unsigned tokens[2]={1u,2u};SZrExecIrRange range;
            if(!ZrCore_ExecIr_FunctionAppendMemoryTokens(fn,tokens,2u,&range))return 0;
            fn->instructions[fn->values[v-1u].definition-1u].memoryIn=(SZrExecIrRange){.start=range.start,.count=1u};
            fn->instructions[fn->values[v-1u].definition-1u].memoryOut=(SZrExecIrRange){.start=range.start+1u,.count=1u};
        }
        if(id==7u || id==8u || id==13u || id==25u || id==26u) {
            unsigned consumer;
            if(!pi_target(f,fn,v,0u,0u))return 0;
            if(id==8u) {consumer=pi_op(f,fn,ZR_EXEC_IR_OPCODE_NEG,ZR_VALUE_TYPE_INT64,v,0u,0u);expected=42u;}
            else {unsigned c=pi_constant(f,fn,2u,ZR_VALUE_TYPE_INT64);
                consumer=pi_op(f,fn,ZR_EXEC_IR_OPCODE_ADD,ZR_VALUE_TYPE_INT64,v,c,0u);expected=id==13u?42u:43u;}
            if(!pi_target(f,fn,consumer,id==25u||id==26u?0u:1u,expected))return 0;
            if(id==13u || id==25u || id==26u) {consumer=pi_op(f,fn,ZR_EXEC_IR_OPCODE_NEG,ZR_VALUE_TYPE_INT64,consumer,0u,0u);
                if(!pi_target(f,fn,consumer,id==13u?1u:0u,UINT64_C(0)-42u))return 0;}
            v=consumer;
        } else {
            unsigned fold=(id<=20u || id==41u || id==44u || id>=47u) && id!=48u && id!=52u;
            if(!pi_target(f,fn,v,fold,expected))return 0;
            if(id==15u){v=pi_op(f,fn,op,type,a,b,0u);if(!pi_target(f,fn,v,1u,42u))return 0;}
        }
        if(!pi_end(f,fn,v,1u,0u))return 0;
    }
 done:
    if(f->module.constantCount>PI_MAX_CONSTANTS)return 0;
    f->initialCount=f->module.constantCount;
    for(i=0u;i<f->initialCount;++i)f->initial[i]=f->module.constants[i];
    return 1;
}
int Pi_PrepareRemarks(SPiFixture *f) {
    f->remarks.items=calloc(1u,sizeof(*f->remarks.items));if(!f->remarks.items)return 0;
    f->remarks.count=f->remarks.capacity=1u;
    f->remarks.items[0].sourceId=777u;f->remarks.items[0].reasonCode=123u;
    f->options.remarks=&f->remarks;return 1;
}
int Pi_Verify(SPiFixture *f) { return ZrCore_ExecIr_VerifyModule(&f->module,&f->diagnostic); }
int Pi_Optimize(SPiFixture *f,unsigned writable) {
    memset(&f->diagnostic,0,sizeof(f->diagnostic));memset(&f->result,0xA5,sizeof(f->result));
    return ZrParser_ExecIr_Optimize(&f->module,&f->options,&f->result,writable?&f->diagnostic:NULL);
}
static TZrUInt64 pi_mix(TZrUInt64 h,TZrUInt64 v) {return (h^v)*UINT64_C(1099511628211);}
static TZrUInt64 pi_contract(TZrUInt64 h,const SZrExecutionContract *c) {
    h=pi_mix(h,c->schemaVersion);h=pi_mix(h,c->abiVersion);h=pi_mix(h,c->logicalVersion);
    h=pi_mix(h,c->reserved0);h=pi_mix(h,c->generation);h=pi_mix(h,c->targetToken);
    h=pi_mix(h,c->reserved1);h=pi_mix(h,c->signatureHash);h=pi_mix(h,c->layoutHash);
    h=pi_mix(h,c->moduleHash);h=pi_mix(h,c->requiredCapabilities);return pi_mix(h,c->declaredEffects);
}
TZrUInt64 Pi_Hash(const SPiFixture *f) {
    unsigned i;TZrUInt64 h=UINT64_C(1469598103934665603);
    h=pi_mix(h,f->module.id);h=pi_mix(h,f->module.moduleToken);h=pi_mix(h,f->module.moduleHash);
    h=pi_contract(h,&f->module.contract);h=pi_mix(h,f->module.layoutCount);h=pi_mix(h,f->module.sourceMapCount);
    h=pi_mix(h,f->module.functionCount);h=pi_mix(h,f->module.constantCount);
    for(i=0u;i<f->module.constantCount;++i){h=pi_mix(h,f->module.constants[i].typeToken);h=pi_mix(h,f->module.constants[i].flags);h=pi_mix(h,f->module.constants[i].bits);}
    for(i=0u;i<f->module.functionCount;++i){unsigned j;const SZrExecIrFunction *fn=&f->module.functions[i];
        h=pi_mix(h,ZrParser_ExecIr_FunctionHash(fn));h=pi_contract(h,&fn->contract);h=pi_mix(h,fn->sealed);
        h=pi_mix(h,fn->bindingRowsSchemaVersion);
        for(j=0u;j<fn->blockCount;++j)h=pi_mix(h,fn->blocks[j].immediateDominator);
    }
    return h;
}
static int pi_owner(SPiSnapshot *s,const void *p,unsigned n,unsigned cap,size_t width) {
    SPiOwner *o; SZrOwnerLiveSpan span;SZrOwnerHeapStats st=ZrTests_ExecOwner_Stats();
    if(s->ownerCount==PI_MAX_OWNERS)return 0;
    o=&s->owners[s->ownerCount++];o->address=(uintptr_t)p;o->count=n;o->capacity=cap;o->width=width;o->record=UINT32_MAX;
    if(!p)return n==0u&&cap==0u;
    if(!cap || n>cap || !ZrTests_ExecOwner_TryLiveSpan(0u,st.records,p,cap,width,&span))return 0;
    o->record=span.record;return 1;
}
static int pi_inventory(const SPiFixture *f,SPiSnapshot *s) {
    unsigned i;memset(s,0,sizeof(*s));
#define PO(p,n,c) if(!pi_owner(s,(p),(n),(c),sizeof(*(p))))return 0
    PO(f->module.functions,f->module.functionCount,f->module.functionCapacity);
    PO(f->module.constants,f->module.constantCount,f->module.constantCapacity);
    PO(f->module.layouts,f->module.layoutCount,f->module.layoutCapacity);
    PO(f->module.sourceMaps,f->module.sourceMapCount,f->module.sourceMapCapacity);
    for(i=0u;i<f->module.functionCount;++i){const SZrExecIrFunction *fn=&f->module.functions[i];
#define FP(p,n,c) PO(fn->p,fn->n,fn->c)
        FP(values,valueCount,valueCapacity);FP(instructions,instructionCount,instructionCapacity);FP(blocks,blockCount,blockCapacity);
        FP(operands,operandCount,operandCapacity);FP(results,resultCount,resultCapacity);
        FP(memoryTokenPool,memoryTokenCount,memoryTokenCapacity);FP(phiPool,phiCount,phiCapacity);
        FP(phiIncoming,phiIncomingCount,phiIncomingCapacity);FP(predecessors,predecessorCount,predecessorCapacity);
        FP(successors,successorCount,successorCapacity);FP(gcMap,gcMapCount,gcMapCapacity);
        FP(gcRoots,gcRootCount,gcRootCapacity);FP(deoptStates,deoptStateCount,deoptStateCapacity);
        FP(deoptValues,deoptValueCount,deoptValueCapacity);FP(deoptAggregates,deoptAggregateCount,deoptAggregateCapacity);
        FP(deoptAggregateFields,deoptAggregateFieldCount,deoptAggregateFieldCapacity);FP(sourceMaps,sourceMapCount,sourceMapCapacity);
        FP(bindingRows,bindingRowCount,bindingRowCapacity);
        /* This fixture publishes no frameLayout/stateMap or GC-map inner owners. */
        if(fn->frameLayout || fn->stateMap || fn->gcMap)return 0;
#undef FP
    }
#undef PO
    s->constantCount=f->module.constantCount;s->functionCount=f->module.functionCount;return 1;
}
int Pi_CurrentLive(const SPiFixture *f) {SPiSnapshot s;return pi_inventory(f,&s);}
int Pi_Snapshot(const SPiFixture *f,SPiSnapshot *s) {
    if(!pi_inventory(f,s))return 0;s->hash=Pi_Hash(f);return 1;
}
int Pi_OwnersEqual(const SPiFixture *f,const SPiSnapshot *s) {
    SPiSnapshot now;unsigned i,freed;
    if(!pi_inventory(f,&now)||now.ownerCount!=s->ownerCount)return 0;
    for(i=0u;i<s->ownerCount;++i){const SPiOwner *a=&s->owners[i],*b=&now.owners[i];
        if(a->address!=b->address||a->count!=b->count||a->capacity!=b->capacity||a->record!=b->record)return 0;
        if(a->record!=UINT32_MAX && (!ZrTests_ExecOwner_TryRecordFrees(a->record,&freed)||freed))return 0;
    }return 1;
}
int Pi_CheckPrefix(const SPiFixture *f) {
    unsigned i;if(f->module.constantCount<f->initialCount)return 0;
    for(i=0u;i<f->initialCount;++i){const SZrExecIrConstant *a=&f->initial[i],*b=&f->module.constants[i];
        if(a->typeToken!=b->typeToken||a->flags!=b->flags||a->bits!=b->bits)return 0;}
    return 1;
}
static int pi_same_range(SZrExecIrRange a,SZrExecIrRange b){return a.start==b.start&&a.count==b.count;}
static int pi_same_instruction(const SZrExecIrInstruction *a,const SZrExecIrInstruction *b) {
    return a->opcode==b->opcode&&a->flags==b->flags&&a->typeToken==b->typeToken&&a->matchTypeToken==b->matchTypeToken&&
        a->layoutId==b->layoutId&&a->effectIn==b->effectIn&&a->effectOut==b->effectOut&&a->sourceId==b->sourceId&&
        a->deoptId==b->deoptId&&a->bindingRow==b->bindingRow&&pi_same_range(a->results,b->results)&&
        pi_same_range(a->operands,b->operands)&&pi_same_range(a->phiRange,b->phiRange)&&
        pi_same_range(a->successorRange,b->successorRange)&&pi_same_range(a->memoryIn,b->memoryIn)&&pi_same_range(a->memoryOut,b->memoryOut);
}
static int pi_check_targets(const SPiFixture *f,unsigned partial) {
    unsigned i,j,good=1u;
    for(i=0u;i<f->targetCount;++i){const SPiTarget *t=&f->targets[i];const SZrExecIrInstruction *in=NULL;
        const SZrExecIrFunction *fn=&f->module.functions[t->function];
        for(j=0u;j<fn->instructionCount;++j)if(fn->instructions[j].sourceId==t->source)in=&fn->instructions[j];
        if(!in){good=0u;continue;}
        if(!t->fold){if(!pi_same_instruction(in,&t->original))good=0u;continue;}
        if(partial&&pi_same_instruction(in,&t->original))continue;
        if(in->opcode!=ZR_EXEC_IR_OPCODE_CONSTANT||in->flags||in->operands.count||in->effectIn||in->effectOut||in->memoryIn.count||in->memoryOut.count){good=0u;continue;}
        if(!f->module.constantCount){if(in->layoutId!=t->bits)good=0u;}
        else if(in->layoutId>=f->module.constantCount)good=0u;
        else {const SZrExecIrConstant *c=&f->module.constants[in->layoutId];
            if(c->typeToken!=ZR_VALUE_TYPE_INT64||c->flags||c->bits!=t->bits)good=0u;}
    }return good;
}
int Pi_CheckTargets(const SPiFixture *f) {return pi_check_targets(f,0u);}
int Pi_CheckPartialTargets(const SPiFixture *f) {return pi_check_targets(f,1u);}
int Pi_NoOrphans(const SPiFixture *f) {
    unsigned p,i,j;
    for(p=f->initialCount;p<f->module.constantCount;++p){unsigned used=0u;
        for(i=0u;i<f->module.functionCount;++i)for(j=0u;j<f->module.functions[i].instructionCount;++j){
            const SZrExecIrInstruction *in=&f->module.functions[i].instructions[j];
            if(in->opcode==ZR_EXEC_IR_OPCODE_CONSTANT&&in->layoutId==p)used=1u;}
        if(!used)return 0;
    }return 1;
}
void Pi_Profile(unsigned id,const char *name,SPiResult r) {
    printf("POOLED_INT64_PROFILE id=%u name=%s safety=%u feature=%u result=%s\n",id,name,r.safety,r.feature,r.safety&&r.feature?"PASS":"FAIL");
}
static SPiResult pi_standard(unsigned id,unsigned profile,unsigned writable) {
    SPiFixture f;SPiResult r={1u,1u};unsigned before,after,attempt;TZrUInt64 hash;
    int pre=0,ok=0,post=0;
    if(!Pi_Build(&f,id,profile)){r.safety=0u;goto done;}
    pre=Pi_Verify(&f);if(!pre){r.safety=0u;goto done;}
    before=f.module.constantCount;attempt=ZrTests_ExecOwner_Stats().attempts;
    if(id==43u)f.options.disabledPassMask|=ZR_EXEC_IR_OPTIMIZE_DISABLE_SCCP;
    ok=Pi_Optimize(&f,writable);post=Pi_CurrentLive(&f)&&Pi_Verify(&f);after=f.module.constantCount;
    r.safety=(unsigned)(ok&&post&&Pi_CheckPrefix(&f)&&Pi_NoOrphans(&f));
    if(r.safety)r.feature=(unsigned)Pi_CheckTargets(&f);
    if(id==14u||id==15u||id==16u)r.feature&=(unsigned)(after==3u);
    if(id==17u)r.feature&=(unsigned)(after==4u);
    if(id==16u)r.safety&=(unsigned)(f.result.functionsVisited==2u);
    if((id>=21u&&id<=43u)&&id!=41u)r.safety&=(unsigned)(after==before);
    if(id==41u)r.safety&=(unsigned)(after==0u);
    if(id==18u){unsigned j,growth=0u;SZrOwnerHeapEvent ev;SZrOwnerLiveSpan pool;
        unsigned have=(unsigned)ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,f.module.constants,f.module.constantCapacity,sizeof(*f.module.constants),&pool);
        for(j=attempt;j<ZrTests_ExecOwner_Stats().attempts;++j)
            if(ZrTests_ExecOwner_TryEvent(j,&ev)&&have&&ev.record==pool.record&&ev.kind==3u&&ev.bytes>=8u*sizeof(SZrExecIrConstant)&&ev.bytes%sizeof(SZrExecIrConstant)==0u)++growth;
        /* Capacity/count proves actual pool growth; event count is reported, never guessed relocation. */
        r.feature&=(unsigned)(after>=18u&&f.module.constantCapacity>=32u&&growth>=2u);
        printf("POOLED_INT64_GROW id=18 events=%u before=%u after=%u capacity=%u\n",growth,before,after,f.module.constantCapacity);
    }
    if(id==19u&&r.safety){unsigned repeatBefore=f.module.constantCount;hash=Pi_Hash(&f);
        r.safety&=(unsigned)(Pi_Optimize(&f,writable)&&Pi_Verify(&f));
        r.feature&=(unsigned)(Pi_Hash(&f)==hash&&f.module.constantCount==repeatBefore&&Pi_CheckTargets(&f));}
    printf("POOLED_INT64_FOLD id=%u profile=%u writable=%u preverify=%d optimize=%d postverify=%d pool_before=%u pool_after=%u targets=%u matched=%u\n",
            id,profile,writable,pre,ok,post,before,after,f.targetCount,r.feature);
 done:r.safety&=(unsigned)Pi_Dispose(&f);return r;
}
static SPiResult pi_public(unsigned id) {
    SPiFixture f;SPiResult r={1u,1u};SZrExecIrPassBudget budget={0};
    SZrExecIrPassInfo pass;const SZrExecIrPassInfo *passes;unsigned count;
    SZrExecIrAnalysisCache before;SZrExecIrSccpValue old[32];unsigned i;
    if(!Pi_Build(&f,id==44u?44u:1u,0u)||!Pi_Verify(&f)){r.safety=0u;goto done;}
    passes=ZrParser_ExecIr_GetScalarPasses(&count);if(!passes||count<1u){r.safety=0u;goto done;}pass=passes[0];
    f.context.cache=&f.cache;f.context.constants=f.module.constants;f.context.constantCount=f.module.constantCount;
    f.context.budget=&budget;f.context.scratchArena=&f.initial;
    if(id==42u){TZrUInt64 hash=ZrParser_ExecIr_FunctionHash(f.module.functions);const void *view=f.context.constants;
        r.safety=(unsigned)(ZrParser_ExecIr_RunPassPipeline(f.module.functions,&pass,1u,&f.context,&f.diagnostic)&&Pi_Verify(&f));
        r.feature=(unsigned)(ZrParser_ExecIr_FunctionHash(f.module.functions)==hash&&Pi_CheckPrefix(&f)&&f.context.constants==view&&f.context.constantCount==2u&&f.context.scratchArena==&f.initial&&f.module.constantCount==2u);
    } else if(id==44u){
        r.safety=(unsigned)ZrParser_ExecIr_ComputeSccp(f.module.functions,&f.context,ZR_FALSE,NULL,&f.diagnostic);
        r.safety&=(unsigned)(f.cache.sccpValues!=NULL);
        before=f.cache;
        r.safety&=(unsigned)(ZrParser_ExecIr_RunPassPipeline(f.module.functions,&pass,1u,&f.context,&f.diagnostic)&&Pi_Verify(&f));
        r.feature=(unsigned)(!(f.cache.validMask&ZR_EXEC_IR_ANALYSIS_SCCP)&&!f.cache.sccpValues&&f.cache.revision>=before.revision&&Pi_CheckTargets(&f));
    } else {
        pass.requiresAnalysis|=ZR_EXEC_IR_ANALYSIS_SCCP;
        r.safety=(unsigned)(ZrParser_ExecIr_RunPassPipeline(f.module.functions,&pass,1u,&f.context,&f.diagnostic)&&Pi_Verify(&f));
        before=f.cache;if(!before.sccpValues||before.sccpValueCount>32u){r.safety=0u;goto done;}
        for(i=0u;i<before.sccpValueCount;++i)old[i]=before.sccpValues[i];
        if(id==45u){f.module.constants[0].bits=41u;
            r.safety&=(unsigned)(ZrParser_ExecIr_RunPassPipeline(f.module.functions,&pass,1u,&f.context,&f.diagnostic)&&Pi_Verify(&f));
            r.feature=(unsigned)(f.cache.revision>before.revision&&f.cache.irHash!=before.irHash);
            for(i=0u;i<f.cache.sccpValueCount;++i)if(f.cache.sccpValues[i].kind==ZR_EXEC_IR_SCCP_CONSTANT&&f.cache.sccpValues[i].bits==43u)break;
            r.feature&=(unsigned)(i<f.cache.sccpValueCount);
        } else {
            int ok;unsigned armed=(unsigned)ZrTests_ExecOwner_TryArm(1u);
            ok=ZrParser_ExecIr_RunPassPipeline(f.module.functions,&pass,1u,&f.context,&f.diagnostic);ZrTests_ExecOwner_Disarm();
            r.safety&=(unsigned)(armed&&!ok&&f.diagnostic.code==ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY&&Pi_Verify(&f));
            r.feature=(unsigned)(f.cache.revision==before.revision&&f.cache.irHash==before.irHash&&f.cache.validMask==before.validMask&&f.cache.sccpValueCount==before.sccpValueCount);
            for(i=0u;i<f.cache.sccpValueCount&&i<before.sccpValueCount;++i)if(old[i].kind!=f.cache.sccpValues[i].kind||old[i].bits!=f.cache.sccpValues[i].bits||old[i].typeToken!=f.cache.sccpValues[i].typeToken)r.feature=0u;
        }
    }
 done:r.safety&=(unsigned)Pi_Dispose(&f);return r;
}
SPiResult Pi_Matrix(unsigned id) {
    SPiResult all={1u,1u},r;unsigned p,w;
    const char *names[PI_PHI_PROFILES];
    if(id==42u || (id>=44u&&id<=46u)){r=pi_public(id);Pi_Profile(id,"public_context",r);return r;}
    if(id>=47u)return Pi_Faults(id);
    names[0]="typed";names[1]="typed";
    if(id==29u){names[0]="float_copy_chain";names[1]="uint64_copy_chain";}
    if(id==30u){names[0]="float_phi_chain";names[1]="uint64_phi_chain";
        names[2]="int64_external_unknown_phi";names[3]="unknown_bool_branch_int64_phi";}
    if(id==31u){names[0]="int32_copy_chain";names[1]="int32_direct_chain";}
    for(p=0u;p<(id==30u?PI_PHI_PROFILES:(id==29u||id==31u?2u:1u));++p) {
        for(w=0u;w<2u;++w) {
            r=pi_standard(id,p,w);Pi_Profile(id,names[p],r);all.safety&=r.safety;all.feature&=r.feature;
            ZrTests_ExecOwner_Reset();
        }
    }
    return all;
}
