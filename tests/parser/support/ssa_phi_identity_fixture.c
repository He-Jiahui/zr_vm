#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "ssa_phi_identity_fixture.h"
#include "exec_ir/exec_ir_mutation_internal.h"
#include "ssa_phi_identity_support.h"
#include "ssa_exec_ir_owned_module_shim.h"

static int pi_pool(const void *p,unsigned n,unsigned cap,size_t width) {
    SZrOwnerLiveSpan span;
    if(!p)return n==0u&&cap==0u;
    return cap!=0u&&n<=cap&&ZrTests_ExecOwner_TryLiveSpan(0u,
        ZrTests_ExecOwner_Stats().records,p,cap,width,&span);
}
int Pi_Live(const SPiFixture *f) {
    const SZrExecIrModule *m=&f->module;const SZrExecIrFunction *fn;
#define POOL(o,p,n,c) do { if(!pi_pool((o)->p,(o)->n,(o)->c,sizeof(*(o)->p)))return 0; } while(0)
    POOL(m,functions,functionCount,functionCapacity);
    POOL(m,constants,constantCount,constantCapacity);
    POOL(m,layouts,layoutCount,layoutCapacity);
    POOL(m,sourceMaps,sourceMapCount,sourceMapCapacity);
    if(m->functionCount!=1u)return 0;
    fn=m->functions;
    POOL(fn,values,valueCount,valueCapacity);
    POOL(fn,instructions,instructionCount,instructionCapacity);
    POOL(fn,blocks,blockCount,blockCapacity);
    POOL(fn,operands,operandCount,operandCapacity);
    POOL(fn,results,resultCount,resultCapacity);
    POOL(fn,memoryTokenPool,memoryTokenCount,memoryTokenCapacity);
    POOL(fn,phiPool,phiCount,phiCapacity);
    POOL(fn,phiIncoming,phiIncomingCount,phiIncomingCapacity);
    POOL(fn,predecessors,predecessorCount,predecessorCapacity);
    POOL(fn,successors,successorCount,successorCapacity);
    POOL(fn,gcRoots,gcRootCount,gcRootCapacity);
    POOL(fn,deoptStates,deoptStateCount,deoptStateCapacity);
    POOL(fn,deoptValues,deoptValueCount,deoptValueCapacity);
    POOL(fn,deoptAggregates,deoptAggregateCount,deoptAggregateCapacity);
    POOL(fn,deoptAggregateFields,deoptAggregateFieldCount,deoptAggregateFieldCapacity);
    POOL(fn,sourceMaps,sourceMapCount,sourceMapCapacity);
    POOL(fn,bindingRows,bindingRowCount,bindingRowCapacity);
    POOL((&f->remarks),items,count,capacity);
#undef POOL
    /* These fixtures deliberately have no side maps or token/result pools. */
    return !m->constants&&!m->layouts&&!m->sourceMaps&&
        !fn->stateMap&&!fn->frameLayout&&!fn->gcMap&&!fn->gcMapCount&&!fn->gcMapCapacity&&
        !fn->gcRoots&&!fn->deoptStates&&!fn->deoptValues&&!fn->deoptAggregates&&
        !fn->deoptAggregateFields&&!fn->sourceMaps&&!fn->bindingRows&&
        !fn->memoryTokenPool&&!fn->results;
}
static unsigned pi_value(SZrExecIrFunction *fn,unsigned type,unsigned external) {
    return external?ZrCore_ExecIr_FunctionAddExternalValue(fn,type,
        ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN):
        ZrCore_ExecIr_FunctionAddValue(fn,type,ZR_EXEC_IR_OWNERSHIP_UNKNOWN,ZR_EXEC_IR_NULLABILITY_UNKNOWN);
}
static int pi_instruction(SZrExecIrFunction *fn,unsigned block,unsigned opcode,unsigned operand) {
    SZrExecIrInstruction in={0};TZrExecIrInstructionId id=0u;
    in.opcode=(TZrUInt16)opcode;in.sourceId=100u+block;
    if(operand&&!ZrCore_ExecIr_FunctionAppendOperands(fn,&operand,1u,&in.operands))return 0;
    if(!ZrCore_ExecIr_FunctionAppendInstruction(fn,&in,&id)||id!=block)return 0;
    fn->blocks[block-1u].instructions=(SZrExecIrRange){.start=block-1u,.count=1u};
    fn->blocks[block-1u].terminatorInstructionId=id;
    return 1;
}
static int pi_build(SPiFixture *f,unsigned id) {
    TZrExecIrFunctionId fid=0u;SZrExecIrFunction *fn;unsigned c,b,i;
    const unsigned successors[PI_EDGES]={2u,3u,4u,4u};
    const unsigned predecessors[PI_EDGES]={1u,1u,2u,3u};
    SZrExecIrRange range;SZrExecIrPhi phi={0};SZrExecIrPhiIncoming incoming[2];
    memset(f,0,sizeof(*f));ZrCore_ExecIr_ModuleInit(&f->module);
    f->module.id=1u;f->module.moduleHash=UINT64_C(0x50484941);
    ZrParser_ExecIr_RemarkSinkInit(&f->remarks);f->options.remarks=&f->remarks;
    if(id==3u)f->options.disabledPassMask=ZR_EXEC_IR_OPTIMIZE_DISABLE_SCCP;
    if(!ZrCore_ExecIr_ModuleAddFunction(&f->module,
        ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF,91u),99u,&fid))return 0;
    fn=ZrCore_ExecIr_ModuleFunctionAt(&f->module,fid);if(!fn||fid!=1u)return 0;
    for(i=0u;i<PI_BLOCKS;++i)
        if(ZrCore_ExecIr_FunctionAddBlock(fn,i?0u:ZR_EXEC_IR_BLOCK_FLAG_ENTRY)!=i+1u)return 0;
    c=pi_value(fn,ZR_VALUE_TYPE_BOOL,1u);f->a=pi_value(fn,ZR_VALUE_TYPE_INT64,1u);
    b=id==2u?pi_value(fn,ZR_VALUE_TYPE_INT64,1u):f->a;
    f->p=pi_value(fn,ZR_VALUE_TYPE_INT64,0u);
    if(!c||!f->a||!b||!f->p||
        !pi_instruction(fn,1u,ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,c)||
        !pi_instruction(fn,2u,ZR_EXEC_IR_OPCODE_BRANCH,0u)||
        !pi_instruction(fn,3u,ZR_EXEC_IR_OPCODE_BRANCH,0u)||
        !pi_instruction(fn,4u,ZR_EXEC_IR_OPCODE_RETURN,f->p))return 0;
    if(!ZrCore_ExecIr_FunctionAppendSuccessors(fn,successors,PI_EDGES,&range)||range.start!=0u||
       !ZrCore_ExecIr_FunctionAppendPredecessors(fn,predecessors,PI_EDGES,&range)||range.start!=0u)return 0;
    fn->blocks[0].successors=(SZrExecIrRange){.start=0u,.count=2u};
    fn->blocks[1].successors=(SZrExecIrRange){.start=2u,.count=1u};
    fn->blocks[2].successors=(SZrExecIrRange){.start=3u,.count=1u};
    fn->blocks[1].predecessors=(SZrExecIrRange){.start=0u,.count=1u};
    fn->blocks[2].predecessors=(SZrExecIrRange){.start=1u,.count=1u};
    fn->blocks[3].predecessors=(SZrExecIrRange){.start=2u,.count=2u};
    for(i=0u;i<PI_BLOCKS;++i)fn->instructions[i].successorRange=fn->blocks[i].successors;
    incoming[0]=(SZrExecIrPhiIncoming){2u,f->a};incoming[1]=(SZrExecIrPhiIncoming){3u,b};
    phi.result=f->p;
    if(!ZrCore_ExecIr_FunctionAppendPhiIncoming(fn,incoming,2u,&phi.incomings)||
       !ZrCore_ExecIr_FunctionAppendPhis(fn,&phi,1u,&fn->blocks[3].phis))return 0;
    return 1;
}
static int pi_capture(const SPiFixture *f,SPiSnapshot *s) {
    const SZrExecIrFunction *fn=f->module.functions;
    if(fn->blockCount!=PI_BLOCKS||fn->instructionCount!=PI_INST||fn->valueCount>PI_VALUES||
       fn->phiCount!=1u||fn->phiIncomingCount!=PI_INCOMING||fn->operandCount!=2u||
       fn->predecessorCount!=PI_EDGES||fn->successorCount!=PI_EDGES)return 0;
    memset(s,0,sizeof(*s));s->module=f->module;s->function=*fn;
    memcpy(s->blocks,fn->blocks,sizeof(s->blocks));
    memcpy(s->instructions,fn->instructions,sizeof(s->instructions));
    memcpy(s->values,fn->values,fn->valueCount*sizeof(*fn->values));s->phi=fn->phiPool[0];
    memcpy(s->incoming,fn->phiIncoming,sizeof(s->incoming));
    memcpy(s->operands,fn->operands,sizeof(s->operands));
    memcpy(s->preds,fn->predecessors,sizeof(s->preds));memcpy(s->succs,fn->successors,sizeof(s->succs));
    return 1;
}
/* Allocation identity/capacity may change across the public clone transaction.
 * Normalize those fields only; every other module/function header byte stays. */
static int pi_headers(const SPiFixture *f,const SPiSnapshot *s) {
    SZrExecIrModule m=f->module;SZrExecIrFunction fn=*f->module.functions;
#define NORMAL(o,e,p,c) do { (o).p=(e).p;(o).c=(e).c; } while(0)
    NORMAL(m,s->module,functions,functionCapacity);
    NORMAL(m,s->module,constants,constantCapacity);NORMAL(m,s->module,layouts,layoutCapacity);
    NORMAL(m,s->module,sourceMaps,sourceMapCapacity);
    NORMAL(fn,s->function,values,valueCapacity);NORMAL(fn,s->function,instructions,instructionCapacity);
    NORMAL(fn,s->function,blocks,blockCapacity);NORMAL(fn,s->function,operands,operandCapacity);
    NORMAL(fn,s->function,results,resultCapacity);NORMAL(fn,s->function,memoryTokenPool,memoryTokenCapacity);
    NORMAL(fn,s->function,phiPool,phiCapacity);NORMAL(fn,s->function,phiIncoming,phiIncomingCapacity);
    NORMAL(fn,s->function,predecessors,predecessorCapacity);NORMAL(fn,s->function,successors,successorCapacity);
    NORMAL(fn,s->function,gcRoots,gcRootCapacity);NORMAL(fn,s->function,deoptStates,deoptStateCapacity);
    NORMAL(fn,s->function,deoptValues,deoptValueCapacity);NORMAL(fn,s->function,deoptAggregates,deoptAggregateCapacity);
    NORMAL(fn,s->function,deoptAggregateFields,deoptAggregateFieldCapacity);
    NORMAL(fn,s->function,sourceMaps,sourceMapCapacity);NORMAL(fn,s->function,bindingRows,bindingRowCapacity);
#undef NORMAL
    if(fn.phiCount>1u)return 0;
    fn.phiCount=s->function.phiCount;
    return memcmp(&m,&s->module,sizeof(m))==0&&memcmp(&fn,&s->function,sizeof(fn))==0;
}
static int pi_cfg(const SPiFixture *f,const SPiSnapshot *s,unsigned id) {
    const SZrExecIrFunction *fn=f->module.functions;unsigned i;
    if(fn->blockCount!=PI_BLOCKS||fn->predecessorCount!=PI_EDGES||fn->successorCount!=PI_EDGES)return 0;
    if(memcmp(fn->predecessors,s->preds,sizeof(s->preds))||memcmp(fn->successors,s->succs,sizeof(s->succs)))return 0;
    for(i=0u;i<PI_BLOCKS;++i){SZrExecIrBlock b=fn->blocks[i];
        /* SCCP requires DOM: entry dominates every block of this diamond.
         * DCE alone has no DOM requirement and preserves the initial zeros. */
        if(b.immediateDominator!=(id==3u?0u:1u))return 0;
        b.immediateDominator=s->blocks[i].immediateDominator;
        if(i==3u){
            if(b.phis.start!=0u||b.phis.count!=fn->phiCount)return 0;
            b.phis=s->blocks[i].phis;
        }
        if(memcmp(&b,s->blocks+i,sizeof(b)))return 0;
    }
    return 1;
}
static int pi_slots(const SPiFixture *f,const SPiSnapshot *s) {
    const SZrExecIrFunction *fn=f->module.functions;unsigned actual;
    if(fn->instructionCount!=PI_INST||fn->valueCount!=s->function.valueCount||fn->operandCount!=2u)return 0;
    if(memcmp(fn->instructions,s->instructions,sizeof(s->instructions))||
       memcmp(fn->values,s->values,fn->valueCount*sizeof(*fn->values))||fn->operands[0]!=s->operands[0])return 0;
    actual=fn->operands[1];
    return (actual==f->p||actual==f->a)&&(!fn->phiCount||memcmp(fn->phiPool,&s->phi,sizeof(s->phi))==0);
}
static int pi_remarks(const SPiFixture *f,unsigned id,TZrUInt64 before,TZrUInt64 after) {
    unsigned i,n=id==3u?1u:2u;const char *names[2]={"sccp","dce"};
    if(f->result.functionsVisited!=1u||f->result.passesRun!=n||f->result.budgetExhausted||
       f->remarks.count!=n||f->result.functionsChanged!=(TZrUInt32)(f->result.beforeHash!=f->result.afterHash)||
       f->result.functionsChanged!=(TZrUInt32)(before!=after))return 0;
    for(i=0u;i<n;++i){const SZrExecIrOptimizationRemark *r=f->remarks.items+i;
        if(!r->pass||strcmp(r->pass,n==1u?"dce":names[i])||r->verifierChecks!=2u||
           r->reasonCode==ZR_EXEC_IR_PASS_REASON_BUDGET||r->outcome==ZR_EXEC_IR_REMARK_BLOCKED||
           r->beforeHash!=before)return 0;
        before=r->afterHash;
    }
    return before==after;
}
SPiResult Pi_Minimal(unsigned id,const char *name) {
    SPiResult all={1u,1u};unsigned w;
    for(w=0u;w<2u;++w){
        SPiFixture f;SPiSnapshot before;SPiResult r={0u,0u};
        unsigned preCore=0u,preParser=0u,run=0u,postCore=0u,postParser=0u;
        unsigned cfg=0u,incoming=0u,slots=0u,maps=0u,owner=0u,remarks=0u,ret=0u,phis=0u,changed=0u;
        unsigned expectedReturn=0u,expectedPhi=id==1u?0u:1u,disposed;
        TZrUInt64 beforeHash=0u,afterHash=0u;
        Pi_SetUp();
        if(!pi_build(&f,id)||!Pi_Live(&f)||!pi_capture(&f,&before))goto done;
        expectedReturn=id==1u?f.a:f.p;
        preCore=(unsigned)(ZrCore_ExecIr_VerifyFunction(f.module.functions,ZR_EXEC_IR_VERIFY_ALL,&f.diagnostic)&&
            ZrCore_ExecIr_VerifyModule(&f.module,&f.diagnostic));
        /* Parser Verify is a Core module-verifier delegation, not an independent verifier. */
        preParser=(unsigned)ZrParser_ExecIr_Verify(&f.module,&f.diagnostic);
        if(!preCore||!preParser)goto done;
        beforeHash=ZrParser_ExecIr_FunctionHash(f.module.functions);
        memset(&f.diagnostic,0xA5,sizeof(f.diagnostic));
        run=(unsigned)ZrParser_ExecIr_Optimize(&f.module,&f.options,&f.result,w?&f.diagnostic:NULL);
        owner=(unsigned)Pi_Live(&f);
        if(!owner)goto done;
        postCore=(unsigned)(ZrCore_ExecIr_VerifyFunction(f.module.functions,ZR_EXEC_IR_VERIFY_ALL,&f.diagnostic)&&
            ZrCore_ExecIr_VerifyModule(&f.module,&f.diagnostic));
        postParser=(unsigned)ZrParser_ExecIr_Verify(&f.module,&f.diagnostic);
        maps=(unsigned)pi_headers(&f,&before);cfg=(unsigned)pi_cfg(&f,&before,id);
        incoming=(unsigned)(f.module.functions->phiIncomingCount==PI_INCOMING&&
            memcmp(f.module.functions->phiIncoming,before.incoming,sizeof(before.incoming))==0);
        slots=(unsigned)pi_slots(&f,&before);
        if(postCore&&postParser)afterHash=ZrParser_ExecIr_FunctionHash(f.module.functions);
        remarks=(unsigned)pi_remarks(&f,id,beforeHash,afterHash);
        if(slots){ret=f.module.functions->operands[1];phis=f.module.functions->phiCount;}
        changed=f.result.functionsChanged;
        r.safety=preCore&&preParser&&run&&postCore&&postParser&&owner&&maps&&cfg&&incoming&&slots&&remarks;
        r.feature=(ret==expectedReturn&&phis==expectedPhi&&changed==(id==1u?1u:0u));
    done:
        disposed=(unsigned)Pi_Dispose(&f);r.safety&=disposed;
        printf("PHI_IDENTITY_PROFILE id=%u name=%s profile=0 entry=2 writable=%u pre_core=%u pre_parser=%u run=%u post_core=%u post_parser=%u return_id=%u expected_return=%u logical_phi=%u expected_phi=%u cfg_exact=%u incoming_exact=%u slots_exact=%u maps_exact=%u changed=%u remarks=%u owner=%u disposed=%u safety=%u feature=%u\n",
            id,name,w,preCore,preParser,run,postCore,postParser,ret,expectedReturn,phis,expectedPhi,
            cfg,incoming,slots,maps,changed,remarks,owner,disposed,r.safety,r.feature);
        all.safety&=r.safety;all.feature&=r.feature;
    }
    return all;
}

int Pix_Run(SPixFixture *f,unsigned entry,unsigned writable) {
    SPiFixture *b=&f->base;SZrExecIrDiagnostic *d=writable?&b->diagnostic:NULL;
    const SZrExecIrPassInfo *passes;unsigned count=0u;
    memset(&b->diagnostic,0xA5,sizeof(b->diagnostic));b->changed=ZR_FALSE;
    if(entry==0u)return ZrParser_ExecIr_RunSccpPass(b->module.functions,&b->context,&b->changed,d);
    if(entry==1u){
        passes=ZrParser_ExecIr_GetScalarPasses(&count);
        if(!passes||count!=2u)return 0;
        return ZrParser_ExecIr_RunModulePassPipeline(&b->module,b->module.functions,passes,count,&b->context,d);
    }
    return ZrParser_ExecIr_Optimize(&b->module,&b->options,&b->result,d);
}
int Pix_Observe(const SPixFixture *f,unsigned entry,TZrUInt64 before,unsigned start,unsigned expected) {
    const SPiFixture *b=&f->base;unsigned i,n,passCount;const SZrExecIrPassInfo *passes;
    if(entry==0u)return (unsigned)b->changed==expected;
    passes=ZrParser_ExecIr_GetScalarPasses(&passCount);if(!passes||passCount!=2u)return 0;
    n=entry==1u?2u:((b->options.disabledPassMask&ZR_EXEC_IR_OPTIMIZE_DISABLE_SCCP)?0u:1u)+
        ((b->options.disabledPassMask&ZR_EXEC_IR_OPTIMIZE_DISABLE_DCE)?0u:1u);
    if(b->remarks.count!=start+n*f->functions)return 0;
    if(entry==1u&&(b->context.passesRun!=b->remarks.count||b->context.budgetExhausted))return 0;
    if(entry==2u&&(b->result.functionsVisited!=f->functions||b->result.functionsChanged!=expected||
        b->result.passesRun!=n*f->functions||b->result.budgetExhausted||
        (b->result.beforeHash!=b->result.afterHash)!=(expected!=0u)))return 0;
    for(i=start;i<b->remarks.count;++i){const SZrExecIrOptimizationRemark *r=b->remarks.items+i;
        unsigned offset=(i-start)%n,fi=(i-start)/n;
        unsigned pass=n==1u?(b->options.disabledPassMask&ZR_EXEC_IR_OPTIMIZE_DISABLE_SCCP?1u:0u):offset;
        if(r->pass!=passes[pass].name||r->verifierChecks!=(b->module.functions[fi].stateMap?3u:2u)||
           r->outcome==ZR_EXEC_IR_REMARK_BLOCKED||r->reasonCode==ZR_EXEC_IR_PASS_REASON_BUDGET)return 0;
        if((r->beforeHash!=r->afterHash)!=(r->outcome==ZR_EXEC_IR_REMARK_SUCCESS))return 0;
        if(offset&&r->beforeHash!=b->remarks.items[i-1u].afterHash)return 0;
        if(offset+1u==n&&r->afterHash!=ZrParser_ExecIr_FunctionHash(b->module.functions+fi))return 0;
    }
    if(entry==1u&&n&&b->remarks.items[start].beforeHash!=before)return 0;
    return 1;
}
void Pix_Row(unsigned id,unsigned profile,unsigned entry,unsigned writable,unsigned repeat,
             SPixChecks pre,unsigned run,SPixChecks post,unsigned exact,SPiResult r) {
    printf("PHI_IDENTITY_PROFILE id=%u name=matrix_%u profile=%u entry=%u writable=%u repeat=%u pre_core=%u pre_parser=%u run=%u post_core=%u post_parser=%u exact=%u safety=%u feature=%u\n",
        id,id,profile,entry,writable,repeat,pre.core,pre.parser,run,post.core,post.parser,exact,r.safety,r.feature);
}
static void pix_print_plan(const SPixFixture *f,const SPixSnapshot *s,unsigned entry,unsigned w) {
    unsigned fi,j,sccp=entry!=2u||!(f->base.options.disabledPassMask&ZR_EXEC_IR_OPTIMIZE_DISABLE_SCCP);
    unsigned dce=entry!=0u&&!(f->base.options.disabledPassMask&ZR_EXEC_IR_OPTIMIZE_DISABLE_DCE);
    for(fi=0u;fi<f->functions;++fi){const SPixPlan *p=f->plan+fi;
        printf("PHI_IDENTITY_EXPECT id=%u profile=%u entry=%u writable=%u function=%u sccp=%u dce=%u optional_skip=%u phi_count=%u operand_slots=",
            f->id,f->profile,entry,w,fi+1u,sccp,dce,f->optional,
            s->fn[fi].header.phiCount-(sccp&&dce?p->removed:0u));
        if(sccp)for(j=0u;j<p->patches;++j)printf("%s%u:%u",j?",":"",p->operand[j].slot,p->operand[j].value);
        printf(" incoming_slots=");
        if(sccp)for(j=0u;j<p->incomingPatches;++j)printf("%s%u:%u",j?",":"",p->incoming[j].slot,p->incoming[j].value);
        printf(" erased_inst_ids=");
        if(sccp&&dce)for(j=0u;j<p->erased;++j)printf("%s%u",j?",":"",p->erase[j]);
        printf(" removed_phi_results=");
        if(sccp&&dce)for(j=0u;j<p->removed;++j)printf("%s%u",j?",":"",p->remove[j]);
        printf("\n");
    }
}
SPiResult Pix_Semantic(unsigned id,const char *name) {
    SPiResult all={1u,1u};unsigned profile,entry,w,repeat;
    (void)name;
    for(profile=0u;profile<Pix_Profiles(id);++profile)
    for(entry=(id==7u||id==14u)?2u:0u;entry<3u;++entry)
    for(w=0u;w<2u;++w){
        SPixFixture f;SPixSnapshot original;SPiResult r={0u,0u};SPixChecks pre={0},post={0};
        unsigned built=0u,run=0u,exact=0u,expected=0u,observe=0u,rejected=0u,completed=0u;
        Pi_SetUp();
        built=(unsigned)Pix_Build(&f,id,profile,id==7u?2u:1u);
        if(!built||!Pix_Live(&f)||!Pix_Capture(&f,&original)){r.safety=0u;goto done;}
        pre=Pix_Verify(&f,w);
        if(f.rejection){
            rejected=!pre.core&&!pre.parser&&Pix_Equal(&f,&original);
            r.safety=rejected;r.feature=rejected;
            printf("PHI_IDENTITY_REJECTION id=%u profile=%u entry=%u writable=%u core_rejected=%u parser_rejected=%u unchanged=%u safety=%u feature=%u\n",
                id,profile,entry,w,!pre.core,!pre.parser,Pix_Equal(&f,&original),r.safety,r.feature);
            goto done;
        }
        if(!pre.core||!pre.parser){r.safety=0u;goto done;}
        pix_print_plan(&f,&original,entry,w);
        r=(SPiResult){1u,1u};
        for(repeat=0u;repeat<Pix_Repeats(id);++repeat){
            unsigned fi,start=f.base.remarks.count,changes=0u;TZrUInt64 hashes[PIX_FUNCTIONS]={0},beforeHash;
            pre=Pix_Verify(&f,w);if(!pre.core||!pre.parser){r.safety=0u;break;}
            for(fi=0u;fi<f.functions;++fi)hashes[fi]=ZrParser_ExecIr_FunctionHash(f.base.module.functions+fi);
            beforeHash=hashes[0];run=(unsigned)Pix_Run(&f,entry,w);++completed;
            post=Pix_Verify(&f,w);
            exact=post.core&&post.parser?(unsigned)Pix_Partial(&f,&original,entry):0u;
            expected=exact?(unsigned)Pix_Expected(&f,&original,entry):0u;
            if(post.core&&post.parser)for(fi=0u;fi<f.functions;++fi)
                changes+=(unsigned)(hashes[fi]!=ZrParser_ExecIr_FunctionHash(f.base.module.functions+fi));
            observe=exact?(unsigned)Pix_Observe(&f,entry,beforeHash,start,changes):0u;
            r.safety&=(run&&post.core&&post.parser&&exact&&observe);
            r.feature&=expected;
            printf("PHI_IDENTITY_FIXEDPOINT id=%u profile=%u entry=%u writable=%u repeat=%u changes=%u observed=%u expected=%u safety=%u feature=%u\n",
                id,profile,entry,w,repeat,changes,observe,expected,r.safety,r.feature);
            if(!r.safety)break;
        }
    done:
        r.safety&=(unsigned)Pi_Dispose(&f.base);
        Pix_Row(id,profile,entry,w,completed,pre,run,post,exact,r);
        all.safety&=r.safety;all.feature&=r.feature;
    }
    return all;
}
SPiResult Pi_Semantic(unsigned id,const char *name) {
    if(id<=3u)return Pi_Minimal(id,name);
    if(id==16u||id==17u)return Pix_Budget(id);
    if(id>=18u&&id<=20u)return Pix_Transactions(id);
    return Pix_Semantic(id,name);
}
