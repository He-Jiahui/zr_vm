#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "ssa_sccp_pooled_bool_fixture.h"
#include "ssa_exec_ir_owned_module_shim.h"

static SBpTotals g_totals;
void Bp_SetUp(void) { ZrTests_ExecOwner_Reset(); }
void Bp_TearDown(void) { ZrTests_ExecOwner_Disarm(); }
SBpTotals Bp_Totals(void) { return g_totals; }
void Bp_NoteFault(unsigned measured, unsigned completed) {
    g_totals.faultMeasured += measured; g_totals.faultCompleted += completed;
}
static int bp_span(const void *pointer, unsigned count, unsigned capacity, size_t width) {
    SZrOwnerLiveSpan span;
    if (!pointer) return !count && !capacity;
    return capacity && count <= capacity &&
        ZrTests_ExecOwner_TryLiveSpan(0u, ZrTests_ExecOwner_Stats().records,
                                    pointer, capacity, width, &span);
}
int Bp_Live(const SBpFixture *f) {
    const SZrExecIrFunction *fn; unsigned i;
#define BP(p,n,c) if (!bp_span((p),(n),(c),sizeof(*(p)))) return 0
    BP(f->module.functions,f->module.functionCount,f->module.functionCapacity);
    BP(f->module.constants,f->module.constantCount,f->module.constantCapacity);
    if (f->module.functionCount>2u) return 0;
    for (i=0u;i<f->module.functionCount;++i) {
    fn = f->module.functions+i;
    BP(fn->blocks,fn->blockCount,fn->blockCapacity);
    BP(fn->values,fn->valueCount,fn->valueCapacity);
    BP(fn->instructions,fn->instructionCount,fn->instructionCapacity);
    BP(fn->operands,fn->operandCount,fn->operandCapacity);
    BP(fn->results,fn->resultCount,fn->resultCapacity);
    BP(fn->sourceMaps,fn->sourceMapCount,fn->sourceMapCapacity);
    BP(fn->phiIncoming,fn->phiIncomingCount,fn->phiIncomingCapacity);
    BP(fn->successors,fn->successorCount,fn->successorCapacity);
    BP(fn->memoryTokenPool,fn->memoryTokenCount,fn->memoryTokenCapacity);
    BP(fn->deoptStates,fn->deoptStateCount,fn->deoptStateCapacity);
    BP(fn->deoptValues,fn->deoptValueCount,fn->deoptValueCapacity);
    if (fn->phiPool || fn->stateMap || fn->gcMap ||
            fn->bindingRows || fn->frameLayout)
        return 0;
    }
#undef BP
    return 1;
}
static unsigned bp_emit(SBpFixture *f, SZrExecIrFunction *fn, unsigned opcode,
                         unsigned type, const unsigned *args, unsigned argc,
                         unsigned layout) {
    SZrExecIrInstruction in;
    TZrExecIrInstructionId id;
    unsigned value = 0u;
    memset(&in,0,sizeof(in)); in.opcode=(TZrUInt16)opcode;
    in.typeToken=type; in.layoutId=layout; in.sourceId=700u+fn->instructionCount;
    if (opcode != ZR_EXEC_IR_OPCODE_RETURN) {
        value=ZrCore_ExecIr_FunctionAddValue(fn,type,ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                                           ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        if (!value || !ZrCore_ExecIr_FunctionAppendResults(fn,&value,1u,&in.results)) return 0;
    }
    if (argc && !ZrCore_ExecIr_FunctionAppendOperands(fn,args,argc,&in.operands)) return 0;
    if (!ZrCore_ExecIr_FunctionAppendInstruction(fn,&in,&id)) return 0;
    (void)f;
    return value ? value : id;
}
int Bp_Build(SBpFixture *f, unsigned predicate, TZrUInt64 left,
             TZrUInt64 right, unsigned expected) {
    SZrExecIrConstant constants[2]={{ZR_VALUE_TYPE_INT64,0u,left},
                                  {ZR_VALUE_TYPE_INT64,0u,right}};
    SZrExecIrFunction *fn;
    SZrExecIrRange range;
    TZrExecIrFunctionId fid;
    unsigned args[2],value;
    memset(f,0,sizeof(*f)); ZrCore_ExecIr_ModuleInit(&f->module);
    ZrParser_ExecIr_AnalysisCacheInit(&f->cache);
    ZrParser_ExecIr_RemarkSinkInit(&f->remarks);
    f->module.id=1u; f->module.moduleHash=UINT64_C(0x424f4f4c);
    f->options.remarks=&f->remarks; f->context.cache=&f->cache;
    f->context.remarks=&f->remarks; f->expected=expected;
    f->appended=1u; f->expectedIndex=UINT32_MAX;
    if (!ZrCore_ExecIr_ModuleAppendConstant(&f->module,constants,2u,&range) ||
        !ZrCore_ExecIr_ModuleAddFunction(&f->module,
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF,201u),
            UINT64_C(0x424f4f4c),&fid)) return 0;
    fn=ZrCore_ExecIr_ModuleFunctionAt(&f->module,fid);
    if (!fn || ZrCore_ExecIr_FunctionAddBlock(fn,ZR_EXEC_IR_BLOCK_FLAG_ENTRY)!=1u) return 0;
    args[0]=bp_emit(f,fn,ZR_EXEC_IR_OPCODE_CONSTANT,ZR_VALUE_TYPE_INT64,NULL,0u,range.start);
    args[1]=bp_emit(f,fn,ZR_EXEC_IR_OPCODE_CONSTANT,ZR_VALUE_TYPE_INT64,NULL,0u,range.start+1u);
    if (!args[0] || !args[1]) return 0;
    value=bp_emit(f,fn,ZR_EXEC_IR_OPCODE_COMPARE,ZR_VALUE_TYPE_BOOL,args,2u,0u);
    if (!value) return 0;
    f->resultId=value; fn->instructions[2].typeToken=predicate;
    f->original=fn->instructions[2];
    if (!bp_emit(f,fn,ZR_EXEC_IR_OPCODE_RETURN,0u,&value,1u,0u)) return 0;
    fn->blocks[0].instructions=(SZrExecIrRange){.start=0u,.count=fn->instructionCount};
    fn->blocks[0].terminatorInstructionId=fn->instructionCount;
    f->initialCount=f->module.constantCount;
    memcpy(f->prefix,f->module.constants,f->initialCount*sizeof(*f->prefix));
    return Bp_Live(f);
}
int Bp_Verify(SBpFixture *f) {
    return Bp_Live(f) && ZrCore_ExecIr_VerifyModule(&f->module,&f->diagnostic) &&
        ZrParser_ExecIr_Verify(&f->module,&f->diagnostic);
}
int Bp_Run(SBpFixture *f, unsigned entry, unsigned writable) {
    SZrExecIrDiagnostic *d=writable?&f->diagnostic:NULL;
    TZrUInt32 count;
    const SZrExecIrPassInfo *passes;
    if (writable) memset(d,0xA5,sizeof(*d));
    if (!entry) return ZrParser_ExecIr_Optimize(&f->module,&f->options,&f->result,d);
    if (entry==1u) return ZrParser_ExecIr_RunModuleSccpPass(&f->module,
            f->module.functions,&f->context,&f->changed,d);
    passes=ZrParser_ExecIr_GetScalarPasses(&count);
    return passes && count==2u && ZrParser_ExecIr_RunModulePassPipeline(&f->module,
            f->module.functions,passes,1u,&f->context,d);
}
static int bp_side_metadata(const SBpFixture *f,const SZrExecIrFunction *fn) {
    switch (f->boundary) {
        case 7u: return fn->deoptStateCount==1u&&fn->deoptValueCount==1u&&
            fn->deoptStates[0].id==1u&&fn->deoptStates[0].sourceId==702u&&
            fn->deoptStates[0].valueRange.start==0u&&fn->deoptStates[0].valueRange.count==1u&&
            fn->deoptValues[0]==1u;
        case 8u: return fn->bindingRowsSchemaVersion==ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY&&
            !fn->bindingRows&&!fn->bindingRowCount&&!fn->bindingRowCapacity;
        case 9u: return fn->phiIncomingCount==1u&&
            fn->phiIncoming[0].predecessor==1u&&fn->phiIncoming[0].value==1u;
        case 10u: return fn->successorCount==1u&&fn->successors[0]==1u&&
            !fn->blocks[0].successors.count;
        case 11u: return fn->memoryTokenCount==1u&&fn->memoryTokenPool[0]==1u;
        case 12u: return fn->memoryTokenCount==1u&&fn->memoryTokenPool[0]==2u;
        default: return 1;
    }
}
static int bp_function_stage(const SBpFixture *f,unsigned j,unsigned folded) {
    const SZrExecIrFunction *fn=f->module.functions+j;
    const SZrExecIrInstruction *in;
    const SZrExecIrConstant *c;
    if (fn->instructionCount!=4u||fn->blockCount!=1u||!f->resultId||
        fn->valueCount<f->resultId||fn->blocks[0].instructions.start||
        fn->blocks[0].instructions.count!=4u||fn->blocks[0].terminatorInstructionId!=4u||
        fn->values[f->resultId-1u].definition!=3u||
        fn->values[f->resultId-1u].typeToken!=ZR_VALUE_TYPE_BOOL||
        fn->instructions[3].opcode!=ZR_EXEC_IR_OPCODE_RETURN||
        fn->instructions[3].operands.count!=1u||
        fn->operands[fn->instructions[3].operands.start]!=f->resultId||
        !bp_side_metadata(f,fn)) return 0;
    in=fn->instructions+2u;
    if (!folded) return !memcmp(in,&f->original,sizeof(*in));
    if (in->opcode!=ZR_EXEC_IR_OPCODE_CONSTANT||in->typeToken!=ZR_VALUE_TYPE_BOOL||
        in->flags||in->bindingRow||in->deoptId||in->matchTypeToken||
        in->effectIn||in->effectOut||in->operands.count||in->phiRange.count||
        in->successorRange.count||in->memoryIn.count||in->memoryOut.count||
        in->sourceId!=f->original.sourceId||
        in->results.start!=f->original.results.start||in->results.count!=1u||
        fn->results[in->results.start]!=f->resultId||
        fn->values[f->resultId-1u].typeToken!=ZR_VALUE_TYPE_BOOL||
        in->layoutId>=f->module.constantCount) return 0;
    c=f->module.constants+in->layoutId;
    return c->typeToken==ZR_VALUE_TYPE_BOOL&&!c->flags&&c->bits==f->expected&&
        (f->expectedIndex==UINT32_MAX||in->layoutId==f->expectedIndex);
}
static int bp_prefix(const SBpFixture *f) {
    unsigned i;
    if (!Bp_Live(f)||!f->module.functionCount||f->module.constantCount<f->initialCount) return 0;
    for (i=0u;i<f->initialCount;++i)
        if (memcmp(f->prefix+i,f->module.constants+i,sizeof(*f->prefix))) return 0;
    return 1;
}
int Bp_Check(const SBpFixture *f,unsigned folded) {
    unsigned j;
    if (!bp_prefix(f)||f->module.constantCount!=f->initialCount+(folded?f->appended:0u)) return 0;
    for (j=0u;j<f->module.functionCount;++j)
        if (!bp_function_stage(f,j,folded)) return 0;
    return 1;
}
int Bp_Partial(const SBpFixture *f) {
    unsigned j;
    if (!bp_prefix(f)||f->module.constantCount>f->initialCount+f->appended) return 0;
    for (j=0u;j<f->module.functionCount;++j)
        if (!bp_function_stage(f,j,0u)&&!bp_function_stage(f,j,1u)) return 0;
    return Bp_NoOrphans(f);
}
int Bp_Dispose(SBpFixture *f) {
    SZrOwnerHeapStats s;
    unsigned readable=(unsigned)Bp_Live(f);
    ZrTests_ExecOwner_Disarm();
    if (readable) {
        ZrCore_ExecIr_FreeModule(&f->module);
        ZrParser_ExecIr_AnalysisCacheFree(&f->cache);
        ZrParser_ExecIr_RemarkSinkFree(&f->remarks);
    }
    s=ZrTests_ExecOwner_Stats(); g_totals.attempts+=s.attempts;
    g_totals.live+=s.live; g_totals.errors+=s.errors;
    if (s.live) ZrTests_ExecOwner_Recover();
    return readable && !s.live && !s.errors;
}
SBpResult Bp_Semantic(unsigned id) {
    /* Literal expected rows, not the optimizer's evaluator. */
    static const TZrUInt64 operands[6][2][2]={
        {{40u,40u},{40u,41u}},
        {{UINT64_C(0x8000000000000000),UINT64_C(0x7fffffffffffffff)},
         {UINT64_C(0x7fffffffffffffff),UINT64_C(0x8000000000000000)}},
        {{UINT64_C(0x8000000000000000),UINT64_C(0x8000000000000000)},
         {1u,UINT64_MAX}},
        {{UINT64_C(0x7fffffffffffffff),UINT64_C(0x8000000000000000)},
         {UINT64_C(0x8000000000000000),UINT64_C(0x7fffffffffffffff)}},
        {{0u,0u},{UINT64_MAX,0u}},
        {{UINT64_MAX,0u},{UINT64_C(0x7fffffffffffffff),UINT64_C(0x7fffffffffffffff)}}
    };
    SBpResult all={1u,1u}; unsigned shape,w,predicate=id>=2u&&id<=7u?id-2u:0u;
    unsigned shapes=id>=2u&&id<=7u?2u:1u;
    for (shape=0u;shape<shapes;++shape) for (w=0u;w<2u;++w) {
        SBpFixture f; SBpResult row={1u,1u};
        unsigned pre=0u,run=0u,post=0u,matched=0u,expected=shape?0u:1u;
        if (!Bp_Build(&f,predicate,operands[predicate][shape][0],
                       operands[predicate][shape][1],expected)) { row.safety=0u; goto done; }
        if (id==8u) f.options.disabledPassMask=ZR_EXEC_IR_OPTIMIZE_DISABLE_SCCP;
        pre=(unsigned)Bp_Verify(&f);
        if (!pre) { row.safety=0u; goto done; }
        run=(unsigned)Bp_Run(&f,0u,w);
        if (Bp_Live(&f)) {
            post=(unsigned)Bp_Verify(&f); matched=(unsigned)Bp_Check(&f,id!=8u);
        }
        row.safety&=run&&post; row.feature&=matched;
    done:
        row.safety&=(unsigned)Bp_Dispose(&f);
        printf("POOLED_BOOL_IR id=%u predicate=%u shape=%u writable=%u preverify=%u run=%u postverify=%u matched=%u safety=%u\n",
               id,predicate,shape,w,pre,run,post,matched,row.safety);
        printf("POOLED_BOOL_PROFILE id=%u name=predicate_%u_shape_%u_diag_%u safety=%u feature=%u result=%s\n",
               id,predicate,shape,w,row.safety,row.feature,row.safety&&row.feature?"PASS":"FAIL");
        all.safety&=row.safety; all.feature&=row.feature;
        ZrTests_ExecOwner_Reset();
    }
    return all;
}

int Bp_Seal(SBpFixture *f) {
    if (!Bp_Live(f) || f->module.constantCount>128u) return 0;
    f->initialCount=f->module.constantCount;
    memcpy(f->prefix,f->module.constants,f->initialCount*sizeof(*f->prefix));
    f->original=f->module.functions[0].instructions[2];
    return 1;
}
int Bp_AddFunction(SBpFixture *f) {
    TZrExecIrFunctionId id; SZrExecIrFunction *fn;
    if (!ZrCore_ExecIr_ModuleAddFunction(&f->module,
        ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF,202u),UINT64_C(0x424f4f4c),&id)) return 0;
    fn=ZrCore_ExecIr_ModuleFunctionAt(&f->module,id);
    if (!fn || !ZrCore_ExecIr_CloneFunction(f->module.functions,fn,&f->diagnostic)) return 0;
    fn->id=id; fn->functionToken=ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF,202u);
    return Bp_Live(f);
}
int Bp_NoOrphans(const SBpFixture *f) {
    unsigned i,j,k;
    if (!Bp_Live(f)) return 0;
    for (i=f->initialCount;i<f->module.constantCount;++i) {
        unsigned used=0u;
        for (j=0u;j<f->module.functionCount;++j) for (k=0u;k<f->module.functions[j].instructionCount;++k) {
            const SZrExecIrInstruction *in=f->module.functions[j].instructions+k;
            if (in->opcode==ZR_EXEC_IR_OPCODE_CONSTANT && in->layoutId==i) used=1u;
        }
        if (!used) return 0;
    }
    return 1;
}
static int bp_append(SBpFixture *f,unsigned type,unsigned flags,TZrUInt64 bits) {
    SZrExecIrConstant c={type,flags,bits}; SZrExecIrRange range;
    return ZrCore_ExecIr_ModuleAppendConstant(&f->module,&c,1u,&range);
}
static int bp_remark(const SBpFixture *f,unsigned count,EZrExecIrRemarkOutcome outcome,unsigned reason) {
    SZrOwnerLiveSpan span;
    if (f->remarks.count!=count || !count || !f->remarks.items ||
        !ZrTests_ExecOwner_TryLiveSpan(0u,ZrTests_ExecOwner_Stats().records,
             f->remarks.items,f->remarks.capacity,sizeof(*f->remarks.items),&span)) return 0;
    return f->remarks.items[count-1u].outcome==outcome &&
        f->remarks.items[count-1u].reasonCode==reason;
}
SBpResult Bp_Advanced(unsigned id) {
    SBpResult all={1u,1u}; unsigned p,w,n=id==11u?2u:id==12u?4u:id==15u?12u:id==16u?3u:1u;
    for (p=0u;p<n;++p) for (w=0u;w<2u;++w) {
        SBpFixture f; SBpResult r={1u,1u}; unsigned pre=0u,ok=0u,post=0u,matched=0u;
        unsigned fold=1u,entry=id==14u?1u:id==17u?2u:0u,repeat=0u;
        TZrUInt64 hash=0u; unsigned beforeCount=0u,beforeCapacity=0u;
        if (!Bp_Build(&f,0u,40u,id==11u&&p==0u?41u:40u,id==11u?p:1u)) {r.safety=0u;goto done;}
        if (id==11u) { /* Existing BOOL0/1 and duplicate exact tuple: first wins. */
            if (!bp_append(&f,ZR_VALUE_TYPE_BOOL,0u,p) || !bp_append(&f,ZR_VALUE_TYPE_BOOL,0u,p)) {r.safety=0u;goto done;}
            f.appended=0u; f.expectedIndex=2u;
        }
        if (id==12u) {unsigned types[4]={ZR_VALUE_TYPE_INT64,ZR_VALUE_TYPE_UINT64,ZR_VALUE_TYPE_DOUBLE,ZR_VALUE_TYPE_BOOL};
            if (!bp_append(&f,types[p],p==3u?1u:0u,1u)) {r.safety=0u;goto done;}}
        if (id==13u) { /* Force a growth boundary; no assertion about realloc moving. */
            while (f.module.constantCount<f.module.constantCapacity)
                if (!bp_append(&f,ZR_VALUE_TYPE_INT64,0u,100u+f.module.constantCount)) {r.safety=0u;goto done;}
            if (!Bp_AddFunction(&f)) {r.safety=0u;goto done;}}
        if (id==15u) {static const unsigned flags[6]={ZR_EXEC_IR_FLAG_MAY_ALLOCATE,ZR_EXEC_IR_FLAG_MAY_GC,
            ZR_EXEC_IR_FLAG_MAY_SUSPEND,ZR_EXEC_IR_FLAG_DEBUG_POLL,ZR_EXEC_IR_FLAG_GUARD_EXIT,ZR_EXEC_IR_FLAG_MAY_THROW};
            SZrExecIrFunction *fn=f.module.functions;SZrExecIrInstruction *in=fn->instructions+2u;
            if (p<6u) in->flags=(TZrUInt16)flags[p];
            else {f.boundary=p+1u;
                if (p==6u) {
                    fn->deoptStates=calloc(1u,sizeof(*fn->deoptStates));
                    fn->deoptValues=calloc(1u,sizeof(*fn->deoptValues));
                    if (!fn->deoptStates||!fn->deoptValues) {r.safety=0u;goto done;}
                    fn->deoptStateCount=fn->deoptStateCapacity=1u;
                    fn->deoptValueCount=fn->deoptValueCapacity=1u;
                    fn->deoptStates[0].id=1u;fn->deoptStates[0].sourceId=in->sourceId;
                    fn->deoptStates[0].valueRange.count=1u;fn->deoptValues[0]=1u;in->deoptId=1u;
                } else if (p==7u) {
                    /* Real LEGACY schema intentionally has no resolved row pool.
                     * Typed non-call binding rows are rejected by Core and are
                     * not represented as a legal COMPARE boundary. */
                    in->bindingRow=1u;
                } else if (p==8u) {
                    fn->phiIncoming=calloc(1u,sizeof(*fn->phiIncoming));
                    if (!fn->phiIncoming) {r.safety=0u;goto done;}
                    fn->phiIncomingCount=fn->phiIncomingCapacity=1u;
                    fn->phiIncoming[0].predecessor=1u;fn->phiIncoming[0].value=1u;
                    in->phiRange.count=1u;
                } else if (p==9u) {
                    fn->successors=calloc(1u,sizeof(*fn->successors));
                    if (!fn->successors) {r.safety=0u;goto done;}
                    fn->successorCount=fn->successorCapacity=1u;fn->successors[0]=1u;
                    in->successorRange.count=1u;
                } else {
                    fn->memoryTokenPool=calloc(1u,sizeof(*fn->memoryTokenPool));
                    if (!fn->memoryTokenPool) {r.safety=0u;goto done;}
                    fn->memoryTokenCount=fn->memoryTokenCapacity=1u;
                    fn->memoryTokenPool[0]=p==10u?1u:2u;
                    if (p==10u) in->memoryIn.count=1u;else in->memoryOut.count=1u;
                }
            }
            fold=0u;}
        if (id==16u) {SZrExecIrFunction *fn=f.module.functions;
            unsigned type=p==0u?ZR_VALUE_TYPE_UINT64:p==1u?ZR_VALUE_TYPE_DOUBLE:ZR_VALUE_TYPE_INT32;
            fn->values[0].typeToken=type; fn->instructions[0].typeToken=type;
            f.module.constants[0].typeToken=type; fold=0u;}
        if (id==18u) { /* External operand, while the old producer becomes dead. */
            SZrExecIrFunction *fn=f.module.functions; unsigned ext=ZrCore_ExecIr_FunctionAddExternalValue(fn,ZR_VALUE_TYPE_INT64,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN);
            if (!ext) {r.safety=0u;goto done;}
            fn->operands[fn->instructions[2].operands.start]=ext; fold=0u;}
        if (id==19u) f.options.disabledPassMask=ZR_EXEC_IR_OPTIMIZE_DISABLE_DCE;
        if (id==20u) {f.options.disabledPassMask=ZR_EXEC_IR_OPTIMIZE_DISABLE_KNOWN_MASK;fold=0u;}
        if (id==21u) {SZrExecIrFunction *fn=f.module.functions;
            fn->sourceMaps=calloc(1u,sizeof(*fn->sourceMaps));
            if (!fn->sourceMaps) {r.safety=0u;goto done;}
            fn->sourceMapCount=fn->sourceMapCapacity=1u;
            fn->sourceMaps[0].sourceId=fn->instructions[2].sourceId;
            fn->sourceMaps[0].instructionId=3u;
        }
        if (!Bp_Seal(&f)) {r.safety=0u;goto done;}
        pre=(unsigned)Bp_Verify(&f); if (!pre) {r.safety=0u;goto done;}
        beforeCount=f.module.constantCount;beforeCapacity=f.module.constantCapacity;
        ok=(unsigned)Bp_Run(&f,entry,w);
        if (Bp_Live(&f)) {post=(unsigned)Bp_Verify(&f);matched=(unsigned)(Bp_Check(&f,fold)&&Bp_NoOrphans(&f));}
        r.safety&=ok&&post; r.feature&=matched;
        if (r.safety && matched) {
            hash=ZrParser_ExecIr_FunctionHash(f.module.functions);
            if (id==14u) r.feature&=(unsigned)(f.changed && !(f.cache.validMask&ZR_EXEC_IR_ANALYSIS_SCCP));
            if (id==13u) r.feature&=(unsigned)(f.module.constantCapacity>beforeCapacity);
            if (entry) r.feature&=(unsigned)(f.context.constants==f.module.constants&&
                f.context.constantCount==f.module.constantCount);
            if (id==17u) r.feature&=(unsigned)(bp_remark(&f,1u,ZR_EXEC_IR_REMARK_SUCCESS,
                ZR_EXEC_IR_PASS_REASON_NONE)&&f.remarks.items[0].beforeHash!=f.remarks.items[0].afterHash);
            if (id==19u) r.feature&=(unsigned)(f.module.functions[0].instructions[0].opcode==ZR_EXEC_IR_OPCODE_CONSTANT &&
                f.module.functions[0].instructions[1].opcode==ZR_EXEC_IR_OPCODE_CONSTANT);
            if (id==21u) r.feature&=(unsigned)(f.module.functions[0].sourceMapCount==1u&&
                f.module.functions[0].sourceMaps[0].instructionId==3u);
            repeat=(unsigned)Bp_Run(&f,entry,w);
            if (Bp_Live(&f)) repeat&=(unsigned)(Bp_Verify(&f)&&Bp_Check(&f,fold)&&
                ZrParser_ExecIr_FunctionHash(f.module.functions)==hash);
            else repeat=0u;
            r.safety&=repeat;
            if (!entry) r.feature&=(unsigned)(f.result.functionsChanged==0u);
            if (id==14u) r.feature&=(unsigned)!f.changed;
            if (id==17u) r.feature&=(unsigned)bp_remark(&f,2u,ZR_EXEC_IR_REMARK_MISSED,
                ZR_EXEC_IR_PASS_REASON_NO_CHANGE);
        }
    done:
        r.safety&=(unsigned)Bp_Dispose(&f);
        printf("POOLED_BOOL_ADVANCED id=%u profile=%u writable=%u preverify=%u run=%u postverify=%u matched=%u repeat=%u before=%u safety=%u feature=%u\n",
            id,p,w,pre,ok,post,matched,repeat,beforeCount,r.safety,r.feature);
        printf("POOLED_BOOL_PROFILE id=%u name=advanced_%u_diag_%u safety=%u feature=%u result=%s\n",
            id,p,w,r.safety,r.feature,r.safety&&r.feature?"PASS":"FAIL");
        all.safety&=r.safety;all.feature&=r.feature;ZrTests_ExecOwner_Reset();
    }
    return all;
}
