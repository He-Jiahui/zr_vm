#include <stdio.h>
#include <string.h>
#include "unity.h"
#define ZR_TEST_RETURNED_EXEC_NO_REDIRECT
#include "support/ssa_source_returned_execir_owner_shim.h"
#define ZR_TEST_RETURNED_EXEC_FRAME_NO_REDIRECT
#include "support/ssa_source_returned_execir_frame_shim.h"
#include "zr_vm_core/metadata_token.h"
static const char g_nine[]="return 9;",g_eight[]="return 8;",g_bool[]="return true;",g_float[]="return 9.0;";
static const SZrSourceCompileOptions g_exec={ZR_SOURCE_RETENTION_REQUIRE_SCALAR_EXECIR};
static const SZrSourceCompileOptions g_aot={ZR_SOURCE_RETENTION_REQUIRE_SCALAR_AOT};
/* Fixed caller witness storage, not a provider capacity promise. Each actual
 * window/count must fit before it is read or accepted. No callback asserts. */
enum { SOURCE_TRACE=512, SOURCE_OUTPUT_RECORDS=256, SOURCE_FACT_WORDS=256 };
typedef struct SSourceFacts { TZrUInt64 words[SOURCE_FACT_WORDS]; unsigned count; } SSourceFacts;
typedef struct SSourceFinalizedObservation {
    SZrOwnerContents original,finalized,published;
    SSourceFacts facts;
    SZrOwnerHeapEvent actualTrace[SOURCE_TRACE],directTrace[SOURCE_TRACE];
    unsigned outputIds[SOURCE_OUTPUT_RECORDS],outputCount;
    uintptr_t addresses[ZR_RETURNED_EXEC_ADDRESSES],slotAddress;
    unsigned addressCount,producerFirst,first,last,attempts,directFirst,directLast,directAttempts;
    unsigned calls,errors,queries,refusals,inputPreserved,profileResult,originalResult,finalizedResult,publishedResult;
    unsigned actualArmResult,directArmResult,actualEventResult,directEventResult;
    unsigned failedIndex,directFailedIndex;
    SZrExecIrDiagnostic actual,direct;
    SZrSourceCompileDiagnostic expected;
    TZrBool result,directResult;
} SSourceFinalizedObservation;
static SSourceFinalizedObservation g_final;
static unsigned g_site,g_observerErrors,g_closed,g_closeErrors;
static SZrReturnedExecTeardown g_close;
static int words_same(const SZrOwnerContents *a,const SZrOwnerContents *b){
    if(a->count!=b->count)return 0;
    for(unsigned i=0u;i<a->count;++i)if(a->words[i]!=b->words[i])return 0;
    return 1;
}
static int put(SZrOwnerContents *o,TZrUInt64 value){
    if(o->count==ZR_OWNER_WORDS)return 0;
    o->words[o->count++]=value;return 1;
}
static int contract_words(const SZrExecutionContract *c,SZrOwnerContents *o){
#define W(field) if(!put(o,(TZrUInt64)c->field))return 0
    W(schemaVersion);W(abiVersion);W(logicalVersion);W(reserved0);W(generation);W(targetToken);W(reserved1);
    W(signatureHash);W(layoutHash);W(moduleHash);W(requiredCapabilities);W(declaredEffects);
#undef W
    return 1;
}
static int pool(const void *p,unsigned count,unsigned capacity,size_t width){
    return count<=capacity&&(count==0u||p!=ZR_NULL)&&(count==0u||width<=SIZE_MAX/count);
}
/* Independent finite-row writer, on the actual finalized Function supplied by
 * the producer. It never creates an alternate module/function for a Core call. */
static int finalized_words(const SZrExecIrFunction *f,SZrOwnerContents *o){
    memset(o,0,sizeof(*o));
    if(!f||f->valueCount!=1u||f->instructionCount!=3u||f->blockCount!=1u||
       f->operandCount!=1u||f->resultCount!=1u||f->sourceMapCount!=3u||f->gcMap||f->gcMapCount||
       f->memoryTokenCount||f->phiCount||f->phiIncomingCount||f->predecessorCount||f->successorCount||
       f->gcRootCount||f->deoptStateCount||f->deoptValueCount||f->deoptAggregateCount||
       f->deoptAggregateFieldCount||f->bindingRowCount||!f->frameLayout)return 0;
#define P(field,count,capacity) if(!pool(f->field,f->count,f->capacity,sizeof(*f->field)))return 0
    P(values,valueCount,valueCapacity);P(instructions,instructionCount,instructionCapacity);
    P(blocks,blockCount,blockCapacity);P(operandPool,operandCount,operandCapacity);
    P(resultPool,resultCount,resultCapacity);P(sourceMaps,sourceMapCount,sourceMapCapacity);
#undef P
    const SZrExecIrFrameLayout *frame=f->frameLayout;
    if(frame->slotCount!=1u||!pool(frame->slots,frame->slotCount,frame->slotCapacity,sizeof(*frame->slots)))return 0;
    if(f->stateMap&&(f->stateMap->entryCount||f->stateMap->valueCount||f->stateMap->rootCount||
                    f->stateMap->ownerStateCount))return 0;
#define W(field) if(!put(o,(TZrUInt64)f->field))return 0
    W(id);W(functionToken);W(signatureHash);W(entryBlockId);W(bindingRowsSchemaVersion);W(sealed);W(gcMapCount);
    if(!contract_words(&f->contract,o))return 0;
    W(valueCount);W(instructionCount);W(blockCount);W(operandCount);W(resultCount);W(memoryTokenCount);
    W(phiCount);W(phiIncomingCount);W(predecessorCount);W(successorCount);W(gcRootCount);W(deoptStateCount);
    W(deoptValueCount);W(deoptAggregateCount);W(deoptAggregateFieldCount);W(sourceMapCount);W(bindingRowCount);
#undef W
#define W(field) if(!put(o,(TZrUInt64)r->field))return 0
#define R(field) W(field.offset);W(field.count)
    {const SZrExecIrValue *r=&f->values[0];W(id);W(definition);W(typeToken);W(ownership);W(nullability);W(flags);}
    for(unsigned i=0u;i<3u;++i){const SZrExecIrInstruction *r=&f->instructions[i];
        W(opcode);W(flags);R(results);R(operands);R(phiRange);R(successorRange);W(typeToken);W(matchTypeToken);
        W(layoutId);R(memoryIn);R(memoryOut);W(effectIn);W(effectOut);W(sourceId);W(deoptId);W(bindingRow);}
    {const SZrExecIrBlock *r=&f->blocks[0];W(id);W(flags);R(instructions);R(predecessors);R(successors);
        R(phis);W(effectPhiResult);R(effectPhiIncomings);
        for(unsigned i=0u;i<ZR_EXEC_IR_MEMORY_CLASS_COUNT;++i){
            if(!put(o,r->memoryPhiResults[i])||!put(o,r->memoryPhiIncomings[i].offset)||
               !put(o,r->memoryPhiIncomings[i].count))return 0;}
        W(immediateDominator);W(terminatorInstructionId);}
    if(!put(o,f->operandPool[0])||!put(o,f->resultPool[0]))return 0;
    for(unsigned i=0u;i<3u;++i){const SZrExecIrSourceMap *r=&f->sourceMaps[i];
        W(sourceId);W(instructionId);W(startOffset);W(endOffset);W(startLine);W(startColumn);W(endLine);W(endColumn);}
    if(!put(o,0u)||!put(o,f->stateMap!=ZR_NULL))return 0;
    if(f->stateMap){const SZrExecIrStateMap *r=f->stateMap;
        W(functionToken);W(signatureHash);W(generation);W(entryCount);W(valueCount);W(rootCount);W(ownerStateCount);}
    if(!put(o,1u))return 0;
    {const SZrExecIrFrameLayout *r=frame;W(storageSlotCount);W(logicalSlotCount);W(parameterPrefixCount);
        W(returnBufferOffset);W(frameByteSize);W(frameByteAlign);W(parameterCount);W(localCount);W(slotCount);W(layoutHash);}
    {const SZrExecIrFrameSlot *r=&frame->slots[0];W(slotId);W(byteOffset);W(byteSize);W(byteAlign);W(typeToken);W(kind);}
#undef R
#undef W
    return 1;
}
static int fact(SSourceFacts *o,TZrUInt64 value){
    if(o->count==SOURCE_FACT_WORDS)return 0;
    o->words[o->count++]=value;return 1;
}
static int range_facts(SSourceFacts *o,SZrFileRange r){
    return fact(o,r.start.offset)&&fact(o,r.end.offset)&&fact(o,(TZrUInt64)r.start.line)&&
           fact(o,(TZrUInt64)r.start.column)&&fact(o,(TZrUInt64)r.end.line)&&fact(o,(TZrUInt64)r.end.column);
}
static int source_facts(const SZrSemanticIrFunction *s,const SZrSemanticContext *context,const SZrAstNode *script,
        const SZrFunctionSourceCallableIdentity *identity,const SZrArray *constants,
        const SZrTypeValue *functionConstants,TZrUInt32 count,SSourceFacts *o){
    memset(o,0,sizeof(*o));
    if(!s||!context||!script||!identity||!constants||constants->length!=1u||count!=1u||!functionConstants||
       !constants->isValid||constants->elementSize!=sizeof(SZrTypeValue)||!constants->head||
       constants->length>constants->capacity||s->instructions.length!=3u||!s->instructions.head||
       s->instructions.elementSize!=sizeof(SZrSemanticIrInstruction)||s->instructions.length>s->instructions.capacity||
       s->sourceMap.length!=3u||!s->sourceMap.head||s->sourceMap.elementSize!=sizeof(SZrSemanticIrSourceMapEntry)||
        s->sourceMap.length>s->sourceMap.capacity||!s->state||s->state!=context->state||
        s->places.state!=s->state||s->cfg.state!=s->state||!context->symbols.isValid||
        context->symbols.elementSize!=sizeof(SZrSemanticSymbolRecord)||context->symbols.length>context->symbols.capacity||
        !context->symbols.head||!context->canonicalTypes.isValid||
        context->canonicalTypes.elementSize!=sizeof(SZrCanonicalTypeNode)||
        context->canonicalTypes.length>context->canonicalTypes.capacity||!context->canonicalTypes.head)return 0;
    const SZrTypeValue *compiler=(const SZrTypeValue *)constants->head;
    if(compiler[0].type!=ZR_VALUE_TYPE_INT64||functionConstants[0].type!=ZR_VALUE_TYPE_INT64)return 0;
#define W(field) if(!fact(o,(TZrUInt64)identity->field))return 0
    W(schemaVersion);W(symbolId);W(typeId);W(canonicalSignatureHash);W(returnPrimitive);W(hasExplicitNoArgsI64);
    W(parameterCount);W(receiverFlags);W(effectFlags);
    W(declarationRange.startLine);W(declarationRange.startColumn);W(declarationRange.endLine);W(declarationRange.endColumn);
#undef W
    if(!fact(o,s->symbolId)||!fact(o,s->callableTypeId)||!fact(o,(TZrUInt64)script->type)||!range_facts(o,script->location)||
       !fact(o,(TZrUInt64)compiler[0].type)||!fact(o,(TZrUInt64)compiler[0].value.nativeObject.nativeInt64)||
       !fact(o,(TZrUInt64)functionConstants[0].type)||!fact(o,(TZrUInt64)functionConstants[0].value.nativeObject.nativeInt64))return 0;
    const SZrSemanticIrInstruction *in=(const SZrSemanticIrInstruction *)s->instructions.head;
    const SZrSemanticIrSourceMapEntry *maps=(const SZrSemanticIrSourceMapEntry *)s->sourceMap.head;
    for(unsigned i=0u;i<3u;++i){
        if(!fact(o,in[i].id)||!fact(o,(TZrUInt64)in[i].opcode)||!fact(o,in[i].typeId)||!fact(o,in[i].resultValueId)||
           !fact(o,in[i].hasConstantPoolIndex)||!fact(o,in[i].constantPoolIndex)||!fact(o,in[i].placeId)||
           !fact(o,in[i].operandStart)||!fact(o,in[i].operandCount)||!range_facts(o,in[i].sourceRange)||
           !fact(o,maps[i].instructionId)||!range_facts(o,maps[i].sourceRange))return 0;
    }
    if(script->type!=ZR_AST_SCRIPT||!script->data.script.statements)return 0;
    const SZrAstNodeArray *statements=script->data.script.statements;
    if(statements->count!=1u||statements->count>statements->capacity||!statements->nodes||!statements->nodes[0])return 0;
    const SZrAstNode *returned=statements->nodes[0];
    if(returned->type!=ZR_AST_RETURN_STATEMENT||!returned->data.returnStatement.expr)return 0;
    const SZrAstNode *integer=returned->data.returnStatement.expr;
    if(integer->type!=ZR_AST_INTEGER_LITERAL||!fact(o,(TZrUInt64)returned->type)||!range_facts(o,returned->location)||
       !fact(o,(TZrUInt64)integer->type)||!range_facts(o,integer->location))return 0;
    const SZrSemanticSymbolRecord *symbols=(const SZrSemanticSymbolRecord *)context->symbols.head;unsigned found=0u;
    for(size_t i=0u;i<context->symbols.length;++i)if(symbols[i].id==s->symbolId){const SZrSemanticSymbolRecord *r=&symbols[i];
        ++found;if(r->kind!=ZR_SEMANTIC_SYMBOL_KIND_FUNCTION||r->astNode!=script||r->typeId!=s->callableTypeId||
                   r->overloadSetId||!fact(o,r->id)||!fact(o,(TZrUInt64)r->kind)||!fact(o,r->typeId)||!range_facts(o,r->location))return 0;}
    if(found!=1u)return 0;
    const SZrCanonicalTypeNode *nodes=(const SZrCanonicalTypeNode *)context->canonicalTypes.head;found=0u;TZrTypeId returnType=0u;
    for(size_t i=0u;i<context->canonicalTypes.length;++i)if(nodes[i].id==s->callableTypeId){const SZrCanonicalTypeNode *r=&nodes[i];
        ++found;if(r->kind!=ZR_CANONICAL_TYPE_FUNCTION||r->structuralHash!=identity->canonicalSignatureHash||
           r->data.function.parameterContracts.length||!fact(o,r->id)||!fact(o,(TZrUInt64)r->kind)||!fact(o,r->structuralHash)||
           !fact(o,r->data.function.returnTypeId)){
            return 0;
        }
        returnType=r->data.function.returnTypeId;
    }
    if(found!=1u)return 0;
    found=0u;
    for(size_t i=0u;i<context->canonicalTypes.length;++i)if(nodes[i].id==returnType){const SZrCanonicalTypeNode *r=&nodes[i];
        ++found;if(r->kind!=ZR_CANONICAL_TYPE_PRIMITIVE||r->data.primitive.valueType!=ZR_VALUE_TYPE_INT64||
            !fact(o,r->id)||!fact(o,(TZrUInt64)r->kind)||!fact(o,r->structuralHash)||!fact(o,(TZrUInt64)r->data.primitive.valueType))return 0;}
    if(found!=1u)return 0;
    return 1;
}
static int same_range(const SZrExecIrSourceMap *m,SZrFileRange r){
    return m->startOffset==r.start.offset&&m->endOffset==r.end.offset&&
           (TZrUInt64)m->startLine==(TZrUInt64)r.start.line&&(TZrUInt64)m->startColumn==(TZrUInt64)r.start.column&&
           (TZrUInt64)m->endLine==(TZrUInt64)r.end.line&&(TZrUInt64)m->endColumn==(TZrUInt64)r.end.column;
}
static int profile(const SZrSemanticIrFunction *s,const SZrFunctionSourceCallableIdentity *identity,
        const SZrArray *constants,const SZrTypeValue *functionConstants,const SZrExecIrModule *m,const SZrExecIrFunction *f){
    if(m->constantCount!=1u||m->layoutCount!=1u||m->sourceMapCount||m->functionCount!=1u||
       !pool(m->constants,1u,m->constantCapacity,sizeof(*m->constants))||
       !pool(m->layouts,1u,m->layoutCapacity,sizeof(*m->layouts))||
       !pool(m->functions,1u,m->functionCapacity,sizeof(*m->functions)))return 0;
    const SZrExecIrFunction *original=&m->functions[0];
    if(original==f||original->valueCount!=3u||original->instructionCount!=3u||original->blockCount!=1u||
       original->operandCount!=2u||original->resultCount!=2u||original->sourceMapCount!=3u||original->frameLayout||
       original->contract.layoutHash||original->id!=1u||f->id!=1u||original->entryBlockId!=1u||f->entryBlockId!=1u)return 0;
#define P(field,count,capacity) if(!pool(original->field,original->count,original->capacity,sizeof(*original->field)))return 0
    P(values,valueCount,valueCapacity);P(instructions,instructionCount,instructionCapacity);P(blocks,blockCount,blockCapacity);
    P(operandPool,operandCount,operandCapacity);P(resultPool,resultCount,resultCapacity);P(sourceMaps,sourceMapCount,sourceMapCapacity);
#undef P
    const SZrTypeValue *compiler=(const SZrTypeValue *)constants->head;
    if((TZrUInt64)compiler[0].value.nativeObject.nativeInt64!=m->constants[0].bits||
       compiler[0].value.nativeObject.nativeInt64!=functionConstants[0].value.nativeObject.nativeInt64||
       m->constants[0].flags||m->constants[0].typeToken!=m->layouts[0].typeToken||m->layouts[0].byteSize!=8u||
       m->layouts[0].byteAlign!=8u||!m->layouts[0].layoutHash||m->moduleHash==0u||
       m->contract.targetToken!=m->moduleToken||m->contract.moduleHash!=m->moduleHash||
       original->functionToken!=f->functionToken||original->signatureHash!=identity->canonicalSignatureHash||
       f->signatureHash!=identity->canonicalSignatureHash||f->contract.signatureHash!=f->signatureHash||
       f->contract.moduleHash!=m->moduleHash||f->contract.targetToken!=f->functionToken)return 0;
    const SZrSemanticIrInstruction *in=(const SZrSemanticIrInstruction *)s->instructions.head;
    if(in[0].opcode!=ZR_SEMANTIC_IR_CONSTANT||in[0].resultValueId!=1u||!in[0].hasConstantPoolIndex||
       in[0].constantPoolIndex!=0u||in[1].opcode!=ZR_SEMANTIC_IR_PLACE_BASE||in[1].placeId!=1u||
       in[2].opcode!=ZR_SEMANTIC_IR_RETURN)return 0;
    const SZrExecIrValue *v=&f->values[0];
    if(v->id!=1u||v->definition!=1u||v->typeToken!=m->constants[0].typeToken||
       v->ownership!=ZR_EXEC_IR_OWNERSHIP_UNKNOWN||v->nullability!=ZR_EXEC_IR_NULLABILITY_NONNULL||v->flags||
       original->values[1].id!=2u||original->values[1].definition!=2u||
       original->values[1].flags!=ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS||original->values[2].id!=3u||
       original->values[2].definition||original->values[2].flags!=ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY)return 0;
    for(unsigned i=0u;i<3u;++i){const SZrExecIrInstruction *a=&original->instructions[i],*b=&f->instructions[i];
        EZrExecIrOpcode originalOpcode=i==0u?ZR_EXEC_IR_OPCODE_CONSTANT:i==1u?ZR_EXEC_IR_OPCODE_PLACE_BASE:ZR_EXEC_IR_OPCODE_RETURN;
        EZrExecIrOpcode finalOpcode=i==1u?ZR_EXEC_IR_OPCODE_NOP:originalOpcode;
        if(a->opcode!=originalOpcode||b->opcode!=finalOpcode||a->sourceId!=i+1u||b->sourceId!=i+1u||
           a->typeToken!=v->typeToken||b->typeToken!=v->typeToken||
           a->operands.offset>original->operandCount||a->operands.count>original->operandCount-a->operands.offset||
           a->results.offset>original->resultCount||a->results.count>original->resultCount-a->results.offset||
           b->operands.offset>f->operandCount||b->operands.count>f->operandCount-b->operands.offset||
           b->results.offset>f->resultCount||b->results.count>f->resultCount-b->results.offset)return 0;
        if(i==0u){if(a->operands.count||b->operands.count||a->results.count!=1u||b->results.count!=1u||
           original->resultPool[a->results.offset]!=1u||f->resultPool[b->results.offset]!=1u)return 0;}
        else if(i==1u){if(a->operands.count!=1u||a->results.count!=1u||b->operands.count||b->results.count||
           original->operandPool[a->operands.offset]!=3u||original->resultPool[a->results.offset]!=2u)return 0;}
        else if(a->operands.count!=1u||b->operands.count!=1u||a->results.count||b->results.count||
           original->operandPool[a->operands.offset]!=1u||f->operandPool[b->operands.offset]!=1u)return 0;
        const SZrExecIrSourceMap *maps[2]={original->sourceMaps,f->sourceMaps};
        for(unsigned j=0u;j<2u;++j){unsigned matches=0u;
            for(unsigned k=0u;k<3u;++k)if(maps[j][k].instructionId==i+1u){++matches;
                if(maps[j][k].sourceId!=i+1u||!same_range(&maps[j][k],in[i].sourceRange))return 0;}
            if(matches!=1u)return 0;}
    }
    if(f->blocks[0].id!=1u||f->blocks[0].flags!=ZR_EXEC_IR_BLOCK_FLAG_ENTRY||
       f->blocks[0].instructions.offset||f->blocks[0].instructions.count!=3u||
       f->blocks[0].terminatorInstructionId!=3u||f->predecessorCount||f->successorCount)return 0;
    const SZrExecIrFrameLayout *frame=f->frameLayout;const SZrExecIrFrameSlot *slot=&frame->slots[0];
    return frame->slotCount==1u&&frame->storageSlotCount==1u&&frame->logicalSlotCount==1u&&
        frame->localCount==1u&&!frame->parameterCount&&!frame->parameterPrefixCount&&
        frame->frameByteSize==8u&&frame->frameByteAlign==8u&&frame->returnBufferOffset==8u&&
        frame->layoutHash!=0u&&frame->layoutHash==f->contract.layoutHash&&slot->slotId==1u&&
        slot->typeToken==v->typeToken&&slot->kind==ZR_EXEC_IR_FRAME_SLOT_VALUE&&!slot->byteOffset&&
        slot->byteSize==8u&&slot->byteAlign==8u;
}
static int frees_read(unsigned id,unsigned *frees){
    int result=ZrTests_ExecOwner_TryRecordFrees(id,frees);++g_final.queries;
    if(result!=1){++g_final.errors;++g_final.refusals;}return result;
}
static int records_have(unsigned first,unsigned last,unsigned expected){
    if(first>last||last>ZrTests_ExecOwner_Stats().records){++g_final.errors;return 0;}
    for(unsigned i=first;i<last;++i){unsigned frees=0u;
        if(frees_read(i,&frees)!=1||frees!=expected){++g_final.errors;return 0;}}
    return 1;
}
static int trace_read(unsigned first,unsigned last,SZrOwnerHeapEvent *trace,unsigned *failed){
    unsigned failures=0u;*failed=0u;
    if(first>last||last-first>SOURCE_TRACE){++g_final.errors;return 0;}
    for(unsigned i=first;i<last;++i){int result=ZrTests_ExecOwner_TryEvent(i,&trace[i-first]);
        ++g_final.queries;if(result!=1){++g_final.refusals;++g_final.errors;return 0;}
        if(trace[i-first].failed){*failed=i-first;++failures;}}
    if(failures!=(g_site!=0u)){++g_final.errors;return 0;}return 1;
}
static void address_add(const void *p){
    if(!p)return;
    if(g_final.addressCount==ZR_RETURNED_EXEC_ADDRESSES){++g_final.errors;return;}
    g_final.addresses[g_final.addressCount++]=(uintptr_t)p;
}
static void function_addresses(const SZrExecIrFunction *f){
    address_add(f);
#define A(field) address_add(f->field)
    A(values);A(instructions);A(blocks);A(operandPool);A(resultPool);A(memoryTokenPool);A(phiPool);A(phiIncoming);
    A(predecessors);A(successors);A(gcRoots);A(deoptStates);A(deoptValues);A(deoptAggregates);A(deoptAggregateFields);
    A(sourceMaps);A(bindingRows);A(gcMap);A(stateMap);A(frameLayout);
#undef A
    if(f->stateMap){address_add(f->stateMap->entries);address_add(f->stateMap->valuePool);
        address_add(f->stateMap->rootPool);address_add(f->stateMap->ownerStatePool);}
    if(f->frameLayout)address_add(f->frameLayout->slots);
}
static void independent_pointer(const void *p,size_t count,size_t width){
    if(!p||!count)return;
    for(unsigned i=0u;i<g_final.addressCount;++i)if((uintptr_t)p==g_final.addresses[i])++g_final.errors;
    SZrOwnerLiveSpan span={0};int result=ZrTests_ExecOwner_TryLiveSpan(g_final.first,g_final.last,p,count,width,&span);
    ++g_final.queries;if(result!=1){++g_final.refusals;++g_final.errors;return;}
    unsigned matches=0u;
    for(unsigned i=0u;i<g_final.outputCount;++i)if(g_final.outputIds[i]==span.record)++matches;
    if(matches!=1u)++g_final.errors;
}
static void independent_published(const SZrExecIrModule *m,SZrExecIrOwnedModule *owner){
    /* Both values are genuine objects returned by this helper, not arbitrary
     * addresses admitted by TryLiveSpan. Numeric spans only bind their records. */
    independent_pointer(owner,1u,1u);independent_pointer(m,1u,sizeof(*m));
#define P(obj,field,count) independent_pointer((obj)->field,(obj)->count,sizeof(*(obj)->field))
    P(m,constants,constantCount);P(m,layouts,layoutCount);P(m,functions,functionCount);
    if(m->functionCount!=1u){++g_final.errors;return;}const SZrExecIrFunction *f=&m->functions[0];
    P(f,values,valueCount);P(f,instructions,instructionCount);P(f,blocks,blockCount);
    P(f,operandPool,operandCount);P(f,resultPool,resultCount);P(f,sourceMaps,sourceMapCount);
    independent_pointer(f->stateMap,f->stateMap!=ZR_NULL,sizeof(*f->stateMap));
    independent_pointer(f->frameLayout,f->frameLayout!=ZR_NULL,sizeof(*f->frameLayout));
    if(f->frameLayout)P(f->frameLayout,slots,slotCount);
#undef P
}
static void expected_source_diagnostic(const SZrSemanticIrFunction *s,const SZrExecIrDiagnostic *d){
    SZrSourceCompileDiagnostic *e=&g_final.expected;memset(e,0,sizeof(*e));
    e->stage=ZR_SOURCE_STAGE_EXECIR_OWNER;e->result=ZR_SOURCE_RESULT_PRODUCER_FAILED;e->hasExecIr=ZR_TRUE;e->execIr=*d;
    if(d->code==ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY)e->result=ZR_SOURCE_RESULT_OUT_OF_MEMORY;
    else if(d->code==ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED)e->result=ZR_SOURCE_RESULT_UNSUPPORTED;
    if(d->sourceId==0u||d->sourceId>s->instructions.length)return;
    const SZrSemanticIrSourceMapEntry *maps=(const SZrSemanticIrSourceMapEntry *)s->sourceMap.head;
    for(size_t i=0u;i<s->sourceMap.length;++i)if(maps[i].instructionId==d->sourceId){
        e->hasRange=ZR_TRUE;e->startOffset=maps[i].sourceRange.start.offset;e->endOffset=maps[i].sourceRange.end.offset;
        e->startLine=maps[i].sourceRange.start.line;e->startColumn=maps[i].sourceRange.start.column;
        e->endLine=maps[i].sourceRange.end.line;e->endColumn=maps[i].sourceRange.end.column;break;}
}
TZrBool ZrTests_ReturnedExec_Finalized(
        const SZrSemanticIrFunction *semantic,const SZrSemanticContext *context,const SZrAstNode *script,
        const SZrFunctionSourceCallableIdentity *sourceIdentity,const SZrArray *compilerConstants,
        const SZrTypeValue *functionConstants,TZrUInt32 functionConstantCount,const SZrExecIrModule *original,
        const SZrExecIrFunction *finalized,SZrExecIrOwnedModule **destination,SZrExecIrDiagnostic *diagnostic){
    static SZrOwnerContents afterOriginal,afterFinalized;
    static SSourceFacts afterFacts;
    SZrOwnerHeapStats before,end;
    ++g_final.calls;
    g_final.originalResult=(unsigned)ZrTests_ExecOwner_TryCapture(original,&g_final.original);
    g_final.finalizedResult=(unsigned)finalized_words(finalized,&g_final.finalized);
    int factsResult=source_facts(semantic,context,script,sourceIdentity,compilerConstants,functionConstants,functionConstantCount,&g_final.facts);
    if(!g_final.originalResult||!g_final.finalizedResult||!factsResult||!destination||*destination||
       !original||original->constantCount!=1u||original->layoutCount!=1u||original->functionCount!=1u||
       original->sourceMapCount||g_final.calls!=1u)++g_final.errors;
    if(g_final.originalResult&&g_final.finalizedResult&&factsResult)
        g_final.profileResult=(unsigned)profile(semantic,sourceIdentity,compilerConstants,functionConstants,original,finalized);
    if(g_final.profileResult!=1u)++g_final.errors;
    g_final.slotAddress=(uintptr_t)destination;
    if(g_final.originalResult&&g_final.finalizedResult&&factsResult){
        address_add(original);address_add(original->constants);address_add(original->layouts);address_add(original->functions);
        function_addresses(&original->functions[0]);function_addresses(finalized);
        address_add(semantic);address_add(context);address_add(script);address_add(sourceIdentity);
        address_add(compilerConstants);address_add(compilerConstants->head);address_add(functionConstants);
    }
    /* The independent destination is always freed before returning to Unity. */
    if(g_site!=0u&&g_final.errors==0u){SZrExecIrOwnedModule *directOwner=ZR_NULL;
        before=ZrTests_ExecOwner_Stats();g_final.directFirst=before.records;
        g_final.directArmResult=(unsigned)ZrTests_ExecOwner_TryArm(g_site);
        if(g_final.directArmResult!=1u)++g_final.errors;
        else g_final.directResult=source_snapshot_finalized_primitive_module(semantic,context,script,sourceIdentity,
                compilerConstants,functionConstants,functionConstantCount,original,finalized,&directOwner,&g_final.direct);
        ZrTests_ExecOwner_Disarm();end=ZrTests_ExecOwner_Stats();g_final.directLast=end.records;
        g_final.directAttempts=end.attempts-before.attempts;
        g_final.directEventResult=(unsigned)trace_read(before.attempts,end.attempts,g_final.directTrace,&g_final.directFailedIndex);
        ZrCore_ExecIr_OwnedModuleFree(&directOwner);
        (void)records_have(g_final.directFirst,g_final.directLast,1u);
        if(ZrTests_ExecOwner_Stats().live!=before.live)++g_final.errors;
        int a=ZrTests_ExecOwner_TryCapture(original,&afterOriginal);
        int b=finalized_words(finalized,&afterFinalized);
        int c=source_facts(semantic,context,script,sourceIdentity,compilerConstants,functionConstants,functionConstantCount,&afterFacts);
        if(a!=1||b!=1||c!=1||!words_same(&g_final.original,&afterOriginal)||
           !words_same(&g_final.finalized,&afterFinalized)||g_final.facts.count!=afterFacts.count||
           memcmp(g_final.facts.words,afterFacts.words,afterFacts.count*sizeof(*afterFacts.words))!=0)++g_final.errors;
    }
    before=ZrTests_ExecOwner_Stats();g_final.first=before.records;
    if(g_site!=0u){g_final.actualArmResult=(unsigned)ZrTests_ExecOwner_TryArm(g_site);
        if(g_final.actualArmResult!=1u)++g_final.errors;}
    g_final.result=source_snapshot_finalized_primitive_module(semantic,context,script,sourceIdentity,
            compilerConstants,functionConstants,functionConstantCount,original,finalized,destination,diagnostic);
    ZrTests_ExecOwner_Disarm();end=ZrTests_ExecOwner_Stats();g_final.last=end.records;
    g_final.attempts=end.attempts-before.attempts;if(diagnostic)g_final.actual=*diagnostic;
    g_final.actualEventResult=(unsigned)trace_read(before.attempts,end.attempts,g_final.actualTrace,&g_final.failedIndex);
    if(!g_final.result){(void)records_have(g_final.first,g_final.last,1u);
        if(end.live!=before.live||(destination&&*destination!=ZR_NULL))++g_final.errors;
        if(factsResult&&diagnostic)expected_source_diagnostic(semantic,diagnostic);
    }else{
        for(unsigned i=g_final.first;i<g_final.last;++i){unsigned frees=0u;
            if(frees_read(i,&frees)!=1)continue;
            if(frees==0u){if(g_final.outputCount==SOURCE_OUTPUT_RECORDS)++g_final.errors;
                else g_final.outputIds[g_final.outputCount++]=i;}
            else if(frees!=1u)++g_final.errors;}
        if(end.live-before.live!=g_final.outputCount||g_final.outputCount==0u)++g_final.errors;
        const SZrExecIrModule *view=ZrCore_ExecIr_OwnedModuleView(*destination);
        g_final.publishedResult=(unsigned)ZrTests_ExecOwner_TryCapture(view,&g_final.published);
        if(g_final.publishedResult!=1u)++g_final.errors;
        else independent_published(view,*destination);
    }
    int a=ZrTests_ExecOwner_TryCapture(original,&afterOriginal);
    int b=finalized_words(finalized,&afterFinalized);
    int c=source_facts(semantic,context,script,sourceIdentity,compilerConstants,functionConstants,functionConstantCount,&afterFacts);
    g_final.inputPreserved=(unsigned)(a==1&&b==1&&c==1&&words_same(&g_final.original,&afterOriginal)&&
        words_same(&g_final.finalized,&afterFinalized)&&g_final.facts.count==afterFacts.count&&
        memcmp(g_final.facts.words,afterFacts.words,afterFacts.count*sizeof(*afterFacts.words))==0);
    if(g_final.inputPreserved!=1u)++g_final.errors;
    return g_final.result;
}
static void close_source(void){
    if(g_closed)return;
    ZrTests_ExecOwner_Disarm();g_site=0u;ZrTests_ReturnedExec_Close();
    g_close=ZrTests_ReturnedExec_Teardown();g_closeErrors=g_observerErrors+g_final.errors;g_closed=1u;
}
static int closed_valid(void){
    if(g_closed!=1u||g_closeErrors||g_close.beginResult!=1||g_close.vmResult!=1||g_close.observerErrors||
       g_close.vm.live||g_close.vm.errors||g_close.vm.diagnosticCount||g_close.vm.dropped||
       g_close.ledgers.closed!=1u||g_close.ledgers.completed!=1u||g_close.ledgers.healthyBeforeRecovery!=1u||
       g_close.ledgers.refusedQueries||g_close.ledgers.observationErrors||g_close.ledgers.ownerBefore.live||
       g_close.ledgers.ownerBefore.errors||g_close.ledgers.ownerAfter.live||g_close.ledgers.ownerAfter.errors||
       g_close.ledgers.ownerBefore.records!=g_close.ledgers.ownerBefore.frees||
       g_close.ledgers.ownerAfter.records!=g_close.ledgers.ownerAfter.frees||
       g_close.ledgers.ownerRecordCount>g_close.ledgers.storage.ownerCapacity||
       (g_close.ledgers.ownerRecordCount&&!g_close.ledgers.storage.owner))return 0;
    for(unsigned k=0u;k<2u;++k){
        if(g_close.ledgers.crtBeforeResult[k]!=1||g_close.ledgers.crtAfterResult[k]!=1||
           g_close.ledgers.crtBefore[k].live||g_close.ledgers.crtBefore[k].errors||
           g_close.ledgers.crtAfter[k].live||g_close.ledgers.crtAfter[k].errors||
           g_close.ledgers.crtBefore[k].successes!=g_close.ledgers.crtBefore[k].frees||
           g_close.ledgers.crtAfter[k].successes!=g_close.ledgers.crtAfter[k].frees||
           g_close.ledgers.crtRecordCount[k]>g_close.ledgers.storage.crtCapacity[k]||
           (g_close.ledgers.crtRecordCount[k]&&!g_close.ledgers.storage.crt[k]))return 0;
        for(unsigned i=0u;i<g_close.ledgers.crtRecordCount[k];++i){const SZrReturnedExecCleanupRecord *r=&g_close.ledgers.storage.crt[k][i];
            if(r->beforeResult!=1||r->afterResult!=1||r->beforeFrees!=1u||r->afterFrees!=1u)return 0;}
    }
    for(unsigned i=0u;i<g_close.ledgers.ownerRecordCount;++i){const SZrReturnedExecCleanupRecord *r=&g_close.ledgers.storage.owner[i];
        if(r->beforeResult!=1||r->afterResult!=1||r->beforeFrees!=1u||r->afterFrees!=1u)return 0;}
    return 1;
}
static void assert_closed(void){
    TEST_ASSERT_TRUE_MESSAGE(closed_valid(),"returned Source first teardown health/witness must remain valid before reset");
    TEST_ASSERT_EQUAL_UINT(0u,g_closeErrors);TEST_ASSERT_EQUAL_INT(1,g_close.beginResult);TEST_ASSERT_EQUAL_INT(1,g_close.vmResult);
    TEST_ASSERT_EQUAL_UINT(0u,g_close.observerErrors);TEST_ASSERT_EQUAL_UINT(0u,g_close.vm.live);TEST_ASSERT_EQUAL_UINT(0u,g_close.vm.errors);
    TEST_ASSERT_EQUAL_UINT(0u,g_close.vm.diagnosticCount);TEST_ASSERT_EQUAL_UINT(0u,g_close.vm.dropped);
    TEST_ASSERT_EQUAL_UINT(1u,g_close.ledgers.closed);TEST_ASSERT_EQUAL_UINT(1u,g_close.ledgers.completed);
    TEST_ASSERT_EQUAL_UINT(1u,g_close.ledgers.healthyBeforeRecovery);TEST_ASSERT_EQUAL_UINT(0u,g_close.ledgers.refusedQueries);
    TEST_ASSERT_EQUAL_UINT(0u,g_close.ledgers.observationErrors);
    for(unsigned k=0u;k<2u;++k){TEST_ASSERT_EQUAL_INT(1,g_close.ledgers.crtBeforeResult[k]);
        TEST_ASSERT_EQUAL_INT(1,g_close.ledgers.crtAfterResult[k]);
        for(unsigned i=0u;i<g_close.ledgers.crtRecordCount[k];++i){const SZrReturnedExecCleanupRecord *r=&g_close.ledgers.storage.crt[k][i];
            TEST_ASSERT_EQUAL_INT(1,r->beforeResult);TEST_ASSERT_EQUAL_INT(1,r->afterResult);
            TEST_ASSERT_EQUAL_UINT(1u,r->beforeFrees);TEST_ASSERT_EQUAL_UINT(1u,r->afterFrees);}}
    for(unsigned i=0u;i<g_close.ledgers.ownerRecordCount;++i){const SZrReturnedExecCleanupRecord *r=&g_close.ledgers.storage.owner[i];
        TEST_ASSERT_EQUAL_INT(1,r->beforeResult);TEST_ASSERT_EQUAL_INT(1,r->afterResult);
        TEST_ASSERT_EQUAL_UINT(1u,r->beforeFrees);TEST_ASSERT_EQUAL_UINT(1u,r->afterFrees);}
}
static void init_source(void){
    if(g_closed)assert_closed();
    ZrTests_ReturnedExec_Init();memset(&g_final,0,sizeof(g_final));memset(&g_close,0,sizeof(g_close));
    g_site=g_observerErrors=g_closeErrors=g_closed=0u;
}
void setUp(void){init_source();}
void tearDown(void){close_source();if(!Unity.CurrentTestFailed&&!Unity.CurrentTestIgnored)assert_closed();}
static SZrFunction *compile_source(unsigned slot,const char *source,TZrBool ordinary,
        const SZrSourceCompileOptions *options,SZrSourceCompileDiagnostic *diagnostic){
    g_observerErrors+=g_final.errors;memset(&g_final,0,sizeof(g_final));
    g_final.producerFirst=ZrTests_ExecOwner_Stats().records;
    return ZrTests_ReturnedExec_Compile(slot,source,ordinary,options,diagnostic);
}
static void diag_equal(const SZrSourceCompileDiagnostic *e,const SZrSourceCompileDiagnostic *a){
#define D(f) TEST_ASSERT_EQUAL_UINT64(e->f,a->f)
    D(stage);D(result);D(hasExecIr);D(hasAotIr);D(hasRange);
    ZrTests_ExecOwner_Diagnostic(&e->execIr,&a->execIr);
    D(aotIr.status);D(aotIr.functionId);D(aotIr.blockId);D(aotIr.instructionId);D(aotIr.index);D(aotIr.expected);D(aotIr.actual);
    D(startOffset);D(endOffset);
#undef D
#define C(f) TEST_ASSERT_EQUAL_INT32(e->f,a->f)
    C(startLine);C(startColumn);C(endLine);C(endColumn);
#undef C
}
static void zero_diag(const SZrSourceCompileDiagnostic *d){SZrSourceCompileDiagnostic z={0};diag_equal(&z,d);}
static void no_aot(void){for(unsigned i=0u;i<2u;++i){SZrReturnedHeapStats s=ZrTests_Returned_Stats(i);
    TEST_ASSERT_EQUAL_UINT(0u,s.attempts);TEST_ASSERT_EQUAL_UINT(0u,s.live);TEST_ASSERT_EQUAL_UINT(0u,s.errors);}}
static void no_observer(void){SZrReturnedExecObservation o=ZrTests_ReturnedExec_Observation();
    TEST_ASSERT_EQUAL_UINT(0u,o.errors);TEST_ASSERT_EQUAL_UINT(0u,o.appendCalls);TEST_ASSERT_EQUAL_UINT(0u,o.snapshotCalls);
    TEST_ASSERT_EQUAL_UINT(0u,g_final.calls);TEST_ASSERT_EQUAL_UINT(0u,g_final.errors);}
static SZrFunction *none(unsigned slot,const char *source,TZrBool direct,const SZrSourceCompileOptions *options){
    SZrSourceCompileDiagnostic d;SZrFunction *f;memset(&d,0xA5,sizeof(d));
    f=compile_source(slot,source,direct,options,&d);
    TEST_ASSERT_NOT_NULL_MESSAGE(f,"ordinary real source wrapper prerequisite");if(!direct)zero_diag(&d);
    TEST_ASSERT_EQUAL_UINT32(1u,f->constantValueLength);TEST_ASSERT_NOT_NULL(f->constantValueList);
    if(source==g_nine||source==g_eight){TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64,f->constantValueList[0].type);
        TEST_ASSERT_EQUAL_INT64(source==g_nine?9:8,f->constantValueList[0].value.nativeObject.nativeInt64);}
    TEST_ASSERT_NULL(f->ownedExecIr);TEST_ASSERT_NULL(f->ownedAotIr);no_observer();return f;
}
static const SZrAotIrModule *aot(unsigned slot,const char *source,TZrUInt64 literal){
    SZrSourceCompileDiagnostic d;SZrAotIrDiagnostic ad={0};SZrAotIrCallableAbi abi={0};
    unsigned oldExecLive=ZrTests_ExecOwner_Stats().live;
    SZrFunction *f=compile_source(slot,source,ZR_FALSE,&g_aot,&d);const SZrAotIrModule *m;
    TEST_ASSERT_NOT_NULL_MESSAGE(f,"existing AOT source wrapper prerequisite");zero_diag(&d);
    TEST_ASSERT_NULL(f->ownedExecIr);m=ZrCore_Function_GetAotIr(f);TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK,ZrCore_AotIr_ValidateModule(m,&ad));
    TEST_ASSERT_EQUAL_UINT32(1u,m->functionCount);TEST_ASSERT_EQUAL_UINT32(1u,m->constantCount);
    TEST_ASSERT_EQUAL_UINT64(literal,m->constantPool[0].bits);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK,ZrCore_AotIr_RequireExecutableAbi(m,m->functions[0].id,&abi,&ad));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64,abi.kind);
    TEST_ASSERT_EQUAL_UINT64(f->moduleSignatureHash,m->moduleHash);
    TEST_ASSERT_EQUAL_UINT64(f->sourceCallableIdentity.canonicalSignatureHash,m->functions[0].signatureHash);
    ZrTests_ReturnedExec_AssertObservation();ZrTests_ReturnedExec_AssertProducerReleased();
    TEST_ASSERT_EQUAL_UINT(0u,ZrTests_ReturnedExec_Observation().snapshotCalls);
    TEST_ASSERT_EQUAL_UINT(0u,g_final.calls);TEST_ASSERT_EQUAL_UINT(0u,g_final.errors);
    TEST_ASSERT_EQUAL_UINT(oldExecLive,ZrTests_ExecOwner_Stats().live);
    TEST_ASSERT_EQUAL_UINT(0u,ZrTests_Returned_Stats(ZR_RETURNED_DESCRIPTOR).live);
    return m;
}
static void prerequisites(void){(void)none(0u,g_nine,ZR_TRUE,ZR_NULL);ZrTests_Returned_FreeFunction(0u);
    (void)aot(0u,g_nine,9u);ZrTests_Returned_FreeFunction(0u);
    TEST_ASSERT_EQUAL_UINT(0u,ZrTests_Returned_Stats(ZR_RETURNED_OWNER).live);}
static void identity(SZrFunction *f,const SZrExecIrModule *m,TZrUInt64 literal){
    const SZrMetadataTokenRecord *module=ZR_NULL,*entry=ZR_NULL;
    TEST_ASSERT_EQUAL_UINT32(1u,m->functionCount);TEST_ASSERT_EQUAL_UINT32(1u,m->constantCount);
    TEST_ASSERT_EQUAL_UINT32(1u,m->layoutCount);TEST_ASSERT_NOT_NULL(m->functions);TEST_ASSERT_NOT_NULL(m->constants);TEST_ASSERT_NOT_NULL(m->layouts);
    TEST_ASSERT_EQUAL_UINT64(literal,m->constants[0].bits);TEST_ASSERT_NOT_EQUAL_UINT32(0u,m->constants[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(m->constants[0].typeToken,m->layouts[0].typeToken);
    TEST_ASSERT_EQUAL_UINT64(f->moduleSignatureHash,m->moduleHash);
    TEST_ASSERT_EQUAL_UINT64(f->sourceCallableIdentity.canonicalSignatureHash,m->functions[0].signatureHash);
    for(TZrUInt32 i=0u;i<f->metadataTokenRecordLength;++i){const SZrMetadataTokenRecord *r=&f->metadataTokenRecords[i];
        if(ZR_METADATA_TOKEN_TABLE(r->token)==ZR_METADATA_TABLE_MODULE){TEST_ASSERT_NULL(module);module=r;}
        if(ZR_METADATA_TOKEN_TABLE(r->token)==ZR_METADATA_TABLE_MEMBER_DEF&&r->reserved0==ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY){TEST_ASSERT_NULL(entry);entry=r;}}
    TEST_ASSERT_NOT_NULL(module);TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(module->token,m->moduleToken);TEST_ASSERT_EQUAL_UINT32(module->token,m->contract.targetToken);
    TEST_ASSERT_EQUAL_UINT32(entry->token,m->functions[0].functionToken);
    TEST_ASSERT_EQUAL_UINT64(m->moduleHash,m->contract.moduleHash);
    TEST_ASSERT_EQUAL_UINT64(m->functions[0].signatureHash,m->functions[0].contract.signatureHash);
}
static void assert_output_ids(const SSourceFinalizedObservation *o,unsigned expected){
    TEST_ASSERT_GREATER_THAN_UINT(0u,o->outputCount);
    for(unsigned i=0u;i<o->outputCount;++i){unsigned frees=0u;int result=ZrTests_ExecOwner_TryRecordFrees(o->outputIds[i],&frees);
        TEST_ASSERT_EQUAL_INT(1,result);TEST_ASSERT_EQUAL_UINT(expected,frees);}
}
static void assert_producer_released(void){
    unsigned end=ZrTests_ExecOwner_Stats().records;
    for(unsigned i=g_final.producerFirst;i<end;++i){unsigned expected=1u,frees=0u;
        for(unsigned j=0u;j<g_final.outputCount;++j)if(g_final.outputIds[j]==i)expected=0u;
        int result=ZrTests_ExecOwner_TryRecordFrees(i,&frees);
        TEST_ASSERT_EQUAL_INT(1,result);TEST_ASSERT_EQUAL_UINT(expected,frees);}
}
static void assert_view(const SSourceFinalizedObservation *o,const SZrExecIrModule *m){
    static SZrOwnerContents view;
    int result=ZrTests_ExecOwner_TryCapture(m,&view);TEST_ASSERT_EQUAL_INT(1,result);
    TEST_ASSERT_TRUE(words_same(&o->published,&view));
    /* Exact module prefix: nineteen header/contract/count fields, one three-word
     * constant and one five-word layout. No module source maps in this profile. */
    const unsigned prefix=19u+3u+5u;
    TEST_ASSERT_EQUAL_UINT(prefix+o->finalized.count,view.count);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(prefix,o->original.count);
    for(unsigned i=0u;i<prefix;++i)TEST_ASSERT_EQUAL_UINT64(o->original.words[i],view.words[i]);
    for(unsigned i=0u;i<o->finalized.count;++i)TEST_ASSERT_EQUAL_UINT64(o->finalized.words[i],view.words[prefix+i]);
}
static SZrFunction *exec(unsigned slot,const char *source,TZrUInt64 literal,TZrBool diagnostic){
    SZrSourceCompileDiagnostic d;SZrFunction *f;SZrReturnedExecObservation o;
    SZrReturnedHeapStats beforeAot[2]={ZrTests_Returned_Stats(0u),ZrTests_Returned_Stats(1u)};
    memset(&d,0xA5,sizeof(d));f=compile_source(slot,source,ZR_FALSE,&g_exec,diagnostic?&d:ZR_NULL);
    ZrTests_ReturnedExec_AssertObservation();o=ZrTests_ReturnedExec_Observation();
    TEST_ASSERT_EQUAL_UINT32(1u,o.constantCount);TEST_ASSERT_EQUAL_UINT32(1u,o.layoutCount);TEST_ASSERT_EQUAL_UINT32(1u,o.functionCount);
    TEST_ASSERT_EQUAL_UINT64(literal,o.literalBits);TEST_ASSERT_NOT_EQUAL_UINT32(0u,o.typeToken);
    TEST_ASSERT_EQUAL_UINT32(o.typeToken,o.layoutTypeToken);TEST_ASSERT_TRUE(o.identityValid);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_TABLE_MODULE,ZR_METADATA_TOKEN_TABLE(o.moduleToken));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_TABLE_MEMBER_DEF,ZR_METADATA_TOKEN_TABLE(o.entryToken));
    TEST_ASSERT_NOT_EQUAL_UINT64(0u,o.moduleHash);TEST_ASSERT_NOT_EQUAL_UINT64(0u,o.signatureHash);
    /* Genuine producer prerequisites precede the only admitted feature failure. */
    assert_producer_released();TEST_ASSERT_EQUAL_UINT(0u,g_final.errors);TEST_ASSERT_EQUAL_UINT(0u,o.snapshotCalls);
    for(unsigned i=0u;i<2u;++i){SZrReturnedHeapStats after=ZrTests_Returned_Stats(i);
        TEST_ASSERT_EQUAL_UINT(beforeAot[i].attempts,after.attempts);TEST_ASSERT_EQUAL_UINT(beforeAot[i].live,after.live);
        TEST_ASSERT_EQUAL_UINT(beforeAot[i].frees,after.frees);TEST_ASSERT_EQUAL_UINT(0u,after.errors);}
    fprintf(stderr,"RETURNED_EXEC_FEATURE returned=%u stage=%u result=%u append=%u finalizer=%u\n",
            (unsigned)(f!=ZR_NULL),diagnostic?(unsigned)d.stage:0u,diagnostic?(unsigned)d.result:0u,o.appendCalls,g_final.calls);
    if(f==ZR_NULL&&diagnostic){SZrSourceCompileDiagnostic expected={0};
        expected.stage=ZR_SOURCE_STAGE_EXECIR_OWNER;expected.result=ZR_SOURCE_RESULT_UNSUPPORTED;expected.hasExecIr=ZR_TRUE;
        expected.execIr.code=ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;diag_equal(&expected,&d);}
    if(f==ZR_NULL){close_source();assert_closed();TEST_FAIL_MESSAGE("RETURNED_EXEC_MISSING_FINALIZED_OWNER");return ZR_NULL;}
    TEST_ASSERT_EQUAL_UINT64(g_final.slotAddress,(uintptr_t)&f->ownedExecIr);
    if(diagnostic)zero_diag(&d);TEST_ASSERT_EQUAL_UINT(1u,g_final.calls);TEST_ASSERT_TRUE(g_final.result);
    TEST_ASSERT_EQUAL_UINT(1u,g_final.originalResult);TEST_ASSERT_EQUAL_UINT(1u,g_final.finalizedResult);
    TEST_ASSERT_EQUAL_UINT(1u,g_final.profileResult);TEST_ASSERT_EQUAL_UINT(1u,g_final.inputPreserved);
    TEST_ASSERT_EQUAL_UINT(1u,g_final.publishedResult);TEST_ASSERT_EQUAL_UINT(1u,g_final.actualEventResult);
    TEST_ASSERT_NULL(f->ownedAotIr);TEST_ASSERT_NOT_NULL(f->ownedExecIr);
    assert_output_ids(&g_final,0u);assert_view(&g_final,ZrCore_Function_GetExecIr(f));identity(f,ZrCore_Function_GetExecIr(f),literal);
    return f;
}
static void test_none_nine_returns_without_owners(void){(void)none(0u,g_nine,ZR_TRUE,ZR_NULL);no_aot();TEST_ASSERT_EQUAL_UINT(0u,ZrTests_ExecOwner_Stats().attempts);}
static void test_zero_eight_and_null_options_nine_without_owners(void){SZrSourceCompileOptions z={0};
    (void)none(0u,g_eight,ZR_FALSE,&z);(void)none(1u,g_nine,ZR_FALSE,ZR_NULL);no_aot();}
static void test_aot_nine_returns_only_aot(void){(void)aot(0u,g_nine,9u);}
static void test_aot_eight_returns_only_aot(void){(void)aot(0u,g_eight,8u);}
static void test_execir_nine_survives_actual_producer_cleanup(void){prerequisites();(void)exec(0u,g_nine,9u,ZR_TRUE);}
static void test_execir_eight_preserves_actual_source_metadata(void){prerequisites();(void)exec(0u,g_eight,8u,ZR_TRUE);}
static void test_execir_null_diagnostic_preserves_content(void){prerequisites();(void)exec(0u,g_nine,9u,ZR_FALSE);}
static void invalid_mode(unsigned value){SZrSourceCompileOptions o={(EZrSourceRetention)value};SZrSourceCompileDiagnostic d,e={0};
    SZrState *s=ZrTests_Returned_State();SZrString *n=ZrTests_Returned_Name();unsigned vm=ZrTests_Returned_VmAttempts();
    memset(&d,0xA5,sizeof(d));TEST_ASSERT_NULL(ZrParser_Source_CompileWithOptions(s,g_nine,sizeof(g_nine)-1u,n,&o,&d));
    e.stage=ZR_SOURCE_STAGE_OPTIONS;e.result=ZR_SOURCE_RESULT_INVALID_ARGUMENT;diag_equal(&e,&d);
    TEST_ASSERT_EQUAL_UINT(vm,ZrTests_Returned_VmAttempts());TEST_ASSERT_EQUAL_UINT(0u,ZrTests_ExecOwner_Stats().attempts);no_aot();no_observer();}
static void test_unknown_two_refused_before_preparation(void){invalid_mode(2u);}
static void test_mixed_aot_execir_refused_before_preparation(void){invalid_mode(5u);}
static void test_other_unknown_mode_refused_before_preparation(void){invalid_mode(255u);}
static void test_execir_null_state_refused_without_producer(void){SZrSourceCompileDiagnostic d,e={0};memset(&d,0xA5,sizeof(d));
    TEST_ASSERT_NULL(ZrParser_Source_CompileWithOptions(ZR_NULL,g_nine,sizeof(g_nine)-1u,ZR_NULL,&g_exec,&d));
    e.stage=ZR_SOURCE_STAGE_OPTIONS;e.result=ZR_SOURCE_RESULT_INVALID_ARGUMENT;diag_equal(&e,&d);no_observer();no_aot();
    TEST_ASSERT_EQUAL_UINT(0u,ZrTests_ExecOwner_Stats().attempts);}
static void test_execir_null_source_and_zero_length_refused(void){SZrSourceCompileDiagnostic d,e={0};
    SZrState *s=ZrTests_Returned_State();SZrString *n=ZrTests_Returned_Name();unsigned vm=ZrTests_Returned_VmAttempts();
    e.stage=ZR_SOURCE_STAGE_OPTIONS;e.result=ZR_SOURCE_RESULT_INVALID_ARGUMENT;
    memset(&d,0xA5,sizeof(d));TEST_ASSERT_NULL(ZrParser_Source_CompileWithOptions(s,ZR_NULL,9u,n,&g_exec,&d));diag_equal(&e,&d);
    memset(&d,0xA5,sizeof(d));TEST_ASSERT_NULL(ZrParser_Source_CompileWithOptions(s,g_nine,0u,n,&g_exec,&d));diag_equal(&e,&d);
    TEST_ASSERT_EQUAL_UINT(vm,ZrTests_Returned_VmAttempts());no_observer();no_aot();TEST_ASSERT_EQUAL_UINT(0u,ZrTests_ExecOwner_Stats().attempts);}
static void unsupported(const char *source,EZrValueType type){SZrSourceCompileDiagnostic d,e={0};SZrFunction *baseline=none(0u,source,ZR_TRUE,ZR_NULL);
    TEST_ASSERT_EQUAL_UINT32(1u,baseline->constantValueLength);TEST_ASSERT_EQUAL_INT(type,baseline->constantValueList[0].type);ZrTests_Returned_FreeFunction(0u);
    memset(&d,0xA5,sizeof(d));TEST_ASSERT_NULL(compile_source(0u,source,ZR_FALSE,&g_exec,&d));
    e.stage=ZR_SOURCE_STAGE_CONSTANT;e.result=ZR_SOURCE_RESULT_UNSUPPORTED;e.hasExecIr=ZR_TRUE;
    e.execIr.code=ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;e.execIr.expectedHash=1u;diag_equal(&e,&d);
    no_observer();no_aot();TEST_ASSERT_EQUAL_UINT(0u,ZrTests_ExecOwner_Stats().live);}
static void test_execir_bool_reports_actual_constant_refusal(void){unsupported(g_bool,ZR_VALUE_TYPE_BOOL);}
static void test_execir_float_reports_actual_constant_refusal(void){unsupported(g_float,ZR_VALUE_TYPE_DOUBLE);}
static void test_two_returned_execir_owners_are_independent(void){SZrFunction *a,*b;static SSourceFinalizedObservation oa,ob;
    prerequisites();a=exec(0u,g_nine,9u,ZR_TRUE);oa=g_final;b=exec(1u,g_eight,8u,ZR_TRUE);ob=g_final;
    TEST_ASSERT_TRUE(a->ownedExecIr!=b->ownedExecIr);ZrTests_ExecOwner_Independent(ZrCore_Function_GetExecIr(a),ZrCore_Function_GetExecIr(b));
    ZrTests_Returned_FreeFunction(0u);assert_output_ids(&oa,1u);
    assert_output_ids(&ob,0u);assert_view(&ob,ZrCore_Function_GetExecIr(b));}
static void test_returned_aot_and_execir_owners_coexist(void){SZrFunction *f;static SSourceFinalizedObservation o;unsigned arena;
    prerequisites();(void)aot(0u,g_nine,9u);arena=ZrTests_Returned_OwnerRecord(0u);f=exec(1u,g_eight,8u,ZR_TRUE);o=g_final;
    ZrTests_Returned_FreeFunction(0u);TEST_ASSERT_EQUAL_UINT(1u,ZrTests_Returned_RecordFrees(ZR_RETURNED_OWNER,arena));
    assert_output_ids(&o,0u);assert_view(&o,ZrCore_Function_GetExecIr(f));}
static void test_aot_replacement_preserves_returned_execir(void){SZrFunction *f;static SSourceFinalizedObservation o;SZrAotIrDiagnostic d;
    const SZrAotIrModule *m;unsigned arena;prerequisites();f=exec(0u,g_nine,9u,ZR_TRUE);o=g_final;
    m=aot(1u,g_nine,9u);TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK,ZrCore_Function_SnapshotAotIr(f,m,&d));
    arena=ZrTests_Returned_OwnerRecord(0u);ZrTests_Returned_FreeFunction(1u);m=aot(1u,g_eight,8u);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK,ZrCore_Function_SnapshotAotIr(f,m,&d));
    TEST_ASSERT_EQUAL_UINT(1u,ZrTests_Returned_RecordFrees(ZR_RETURNED_OWNER,arena));
    assert_output_ids(&o,0u);assert_view(&o,ZrCore_Function_GetExecIr(f));
    TEST_ASSERT_EQUAL_UINT64(8u,ZrCore_Function_GetAotIr(f)->constantPool[0].bits);}
static void test_function_free_then_global_shutdown_releases_execir(void){static SSourceFinalizedObservation a,b;prerequisites();
    (void)exec(0u,g_nine,9u,ZR_TRUE);a=g_final;(void)exec(1u,g_eight,8u,ZR_TRUE);b=g_final;
    ZrTests_Returned_FreeFunction(0u);assert_output_ids(&a,1u);assert_output_ids(&b,0u);
    ZrTests_Returned_Shutdown();assert_output_ids(&a,1u);assert_output_ids(&b,1u);
    ZrTests_ExecOwner_AssertBalanced();ZrTests_Returned_AssertBalanced();}
static void test_sixteen_alternating_compile_free_lifecycles(void){prerequisites();for(unsigned i=0u;i<16u;++i){
    (void)exec(0u,(i&1u)?g_eight:g_nine,(i&1u)?8u:9u,ZR_TRUE);
    ZrTests_Returned_FreeFunction(0u);assert_output_ids(&g_final,1u);TEST_ASSERT_EQUAL_UINT(0u,ZrTests_ExecOwner_Stats().live);}}
static void geometry(const SZrOwnerHeapEvent *a,const SZrOwnerHeapEvent *b){
    TEST_ASSERT_EQUAL_UINT(a->kind,b->kind);TEST_ASSERT_EQUAL_UINT64(a->count,b->count);
    TEST_ASSERT_EQUAL_UINT64(a->width,b->width);TEST_ASSERT_EQUAL_UINT64(a->bytes,b->bytes);
}
static void fault_sweep(const char *source,TZrUInt64 literal){
    unsigned count,completed=0u;static SZrOwnerHeapEvent baseline[SOURCE_TRACE];
    prerequisites();(void)exec(0u,source,literal,ZR_TRUE);count=g_final.attempts;
    TEST_ASSERT_GREATER_THAN_UINT(0u,count);TEST_ASSERT_LESS_OR_EQUAL_UINT(SOURCE_TRACE,count);
    memcpy(baseline,g_final.actualTrace,count*sizeof(*baseline));close_source();assert_closed();
    for(unsigned index=1u;index<=count;++index){SZrSourceCompileDiagnostic d;SZrFunction *f;
        init_source();g_site=index;memset(&d,0xA5,sizeof(d));
        f=compile_source(0u,source,ZR_FALSE,&g_exec,&d);
        unsigned returned=(f!=ZR_NULL);
        /* All genuine compiler/helper cleanup precedes the saved-health checks.
         * Each fault uses its own Global, and its retry uses another fresh one. */
        close_source();assert_closed();assert_producer_released();
        TEST_ASSERT_EQUAL_UINT(0u,returned);TEST_ASSERT_EQUAL_UINT(1u,g_final.calls);TEST_ASSERT_EQUAL_UINT(0u,g_final.errors);
        TEST_ASSERT_FALSE(g_final.directResult);TEST_ASSERT_FALSE(g_final.result);
        TEST_ASSERT_EQUAL_UINT(1u,g_final.directArmResult);TEST_ASSERT_EQUAL_UINT(1u,g_final.actualArmResult);
        TEST_ASSERT_EQUAL_UINT(1u,g_final.directEventResult);TEST_ASSERT_EQUAL_UINT(1u,g_final.actualEventResult);
        TEST_ASSERT_EQUAL_UINT(index-1u,g_final.failedIndex);TEST_ASSERT_EQUAL_UINT(index-1u,g_final.directFailedIndex);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT(index,g_final.attempts);TEST_ASSERT_LESS_OR_EQUAL_UINT(count,g_final.attempts);
        TEST_ASSERT_EQUAL_UINT(g_final.directAttempts,g_final.attempts);TEST_ASSERT_EQUAL_UINT(1u,g_final.inputPreserved);
        for(unsigned j=0u;j<g_final.attempts;++j){geometry(&g_final.directTrace[j],&g_final.actualTrace[j]);
            if(j<index)geometry(&baseline[j],&g_final.actualTrace[j]);
            TEST_ASSERT_EQUAL_UINT(j==index-1u,g_final.actualTrace[j].failed);}
        TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,g_final.direct.code);
        diag_equal(&g_final.expected,&d);ZrTests_ExecOwner_Diagnostic(&g_final.direct,&g_final.actual);
        init_source();(void)exec(0u,source,literal,ZR_TRUE);
        TEST_ASSERT_EQUAL_UINT(count,g_final.attempts);
        for(unsigned j=0u;j<count;++j)geometry(&baseline[j],&g_final.actualTrace[j]);
        close_source();assert_closed();++completed;
    }
    TEST_ASSERT_EQUAL_UINT(count,completed);
    fprintf(stderr,"RETURNED_EXEC_FAULT_SWEEP literal=%llu measured=%u completed=%u\n",(unsigned long long)literal,count,completed);
    init_source();
}
static void test_each_source_snapshot_allocation_fault_cleanup_retry(void){fault_sweep(g_nine,9u);fault_sweep(g_eight,8u);}
/* Frame-only observations use the same owner ledger as every real Core free.
 * Fixed copies below are made from the live producer candidate, never rebuilt
 * as a replacement input. Callbacks report errors without Unity longjmps. */
typedef struct SSourceFrameInput {
    SZrExecIrFunction function;SZrExecIrValue value;SZrExecIrInstruction instructions[3];
    SZrExecIrBlock block;TZrExecIrValueId operand,result;SZrExecIrSourceMap sources[3];
    SZrExecIrStateMap state;SZrExecIrLayout layout;
} SSourceFrameInput;
typedef struct SSourceFrameObservation {
    unsigned enabled,active,site,calls,errors,arms,hits,first,last,preserved,published;
    unsigned valuesCalls,headerCalls,valuesFrees,headerFrees,nullFrees,packedCalls,packedSlots,packedMaps,packedCleared;
    unsigned headerRecord,slotRecord,publishedLive;uintptr_t valuesAddress,headerAddress;
    SZrOwnerHeapEvent valuesEvent,headerEvent;TZrMetadataToken token;TZrBool result;
    SZrExecIrDiagnostic diagnostic;
} SSourceFrameObservation;
static SSourceFrameObservation g_frame;
static int frame_input(const SZrExecIrFunction *f,const SZrExecIrLayout *layouts,unsigned count,SSourceFrameInput *out){
    memset(out,0,sizeof(*out));
    if(!f||count!=1u||!layouts||f->frameLayout||f->contract.layoutHash||f->valueCount!=1u||
       f->instructionCount!=3u||f->blockCount!=1u||f->operandCount!=1u||f->resultCount!=1u||
       f->sourceMapCount!=3u||f->gcMap||f->gcMapCount||f->memoryTokenCount||f->phiCount||
       f->phiIncomingCount||f->predecessorCount||f->successorCount||f->gcRootCount||f->deoptStateCount||
       f->deoptValueCount||f->deoptAggregateCount||f->deoptAggregateFieldCount||f->bindingRowCount)return 0;
#define P(field,count,capacity) if(!pool(f->field,f->count,f->capacity,sizeof(*f->field)))return 0
    P(values,valueCount,valueCapacity);P(instructions,instructionCount,instructionCapacity);
    P(blocks,blockCount,blockCapacity);P(operandPool,operandCount,operandCapacity);
    P(resultPool,resultCount,resultCapacity);P(sourceMaps,sourceMapCount,sourceMapCapacity);
#undef P
    if(f->stateMap&&(f->stateMap->entryCount||f->stateMap->valueCount||f->stateMap->rootCount||f->stateMap->ownerStateCount))return 0;
    memcpy(&out->function,f,sizeof(*f));memcpy(&out->value,f->values,sizeof(out->value));
    memcpy(out->instructions,f->instructions,sizeof(out->instructions));memcpy(&out->block,f->blocks,sizeof(out->block));
    memcpy(&out->operand,f->operandPool,sizeof(out->operand));memcpy(&out->result,f->resultPool,sizeof(out->result));
    memcpy(out->sources,f->sourceMaps,sizeof(out->sources));memcpy(&out->layout,layouts,sizeof(out->layout));
    if(f->stateMap)memcpy(&out->state,f->stateMap,sizeof(out->state));return 1;
}
static unsigned frame_freed(unsigned record,unsigned expected){unsigned n=~0u;
    return ZrTests_ExecOwner_TryRecordFrees(record,&n)&&n==expected;}
static unsigned frame_span(const void *p,size_t count,size_t width){SZrOwnerLiveSpan s={0};
    if(!ZrTests_ExecOwner_TryLiveSpan(g_frame.first,ZrTests_ExecOwner_Stats().records,p,count,width,&s)){
        ++g_frame.errors;return ~0u;}return s.record;}
static void *frame_allocate(unsigned site,size_t count,size_t width){
    unsigned armed=0u;SZrOwnerHeapStats before=ZrTests_ExecOwner_Stats();SZrOwnerHeapEvent event={0};void *p;
    if(g_frame.active){
        if(site==1u){if(++g_frame.valuesCalls!=1u||count!=1u||width!=sizeof(SZrExecIrPackedValue))++g_frame.errors;}
        else if(++g_frame.headerCalls!=1u||count!=1u||width!=sizeof(SZrExecIrFrameLayout))++g_frame.errors;
        if(g_frame.site==site){armed=(unsigned)ZrTests_ExecOwner_TryArm(1u);g_frame.arms+=armed;if(!armed)++g_frame.errors;}
    }
    p=site==1u?ZrTests_ExecOwner_Calloc(count,width):ZrTests_ExecOwner_Malloc(width);
    if(armed)ZrTests_ExecOwner_Disarm();
    if(g_frame.active){
        if(ZrTests_ExecOwner_Stats().attempts!=before.attempts+1u||!ZrTests_ExecOwner_TryEvent(before.attempts,&event)||
           event.kind!=(site==1u?2u:1u)||event.count!=count||event.width!=width||event.bytes!=count*width)++g_frame.errors;
        if(event.failed){++g_frame.hits;if(!armed||p)++g_frame.errors;}
        if(site==1u){g_frame.valuesEvent=event;g_frame.valuesAddress=(uintptr_t)p;}
        else{g_frame.headerEvent=event;g_frame.headerAddress=(uintptr_t)p;if(p)g_frame.headerRecord=event.record;}
    }
    return p;
}
void *ZrTests_ReturnedExec_FrameMalloc(size_t bytes){return frame_allocate(2u,1u,bytes);}
void *ZrTests_ReturnedExec_FrameCalloc(size_t count,size_t width){return frame_allocate(1u,count,width);}
void ZrTests_ReturnedExec_FrameFree(void *p){
    if(g_frame.active){if(!p)++g_frame.nullFrees;
        else if((uintptr_t)p==g_frame.valuesAddress)++g_frame.valuesFrees;
        else if((uintptr_t)p==g_frame.headerAddress)++g_frame.headerFrees;else ++g_frame.errors;}
    ZrTests_ExecOwner_Free(p);
}
void ZrTests_ReturnedExec_PackedFree(SZrExecIrPackedFrameLayout *p){
    unsigned ids[4],n=0u;SZrExecIrPackedFrameLayout zero={0};
    if(g_frame.active){++g_frame.packedCalls;if(!p){++g_frame.errors;return;}
        if(p->frame.slots){++g_frame.packedSlots;ids[n++]=frame_span(p->frame.slots,p->frame.slotCount,sizeof(*p->frame.slots));}
        if(p->logicalValueIds&&p->logicalToPhysical&&p->slotClasses)++g_frame.packedMaps;else ++g_frame.errors;
        if(p->logicalValueIds)ids[n++]=frame_span(p->logicalValueIds,1u,sizeof(*p->logicalValueIds));
        if(p->logicalToPhysical)ids[n++]=frame_span(p->logicalToPhysical,1u,sizeof(*p->logicalToPhysical));
        if(p->slotClasses)ids[n++]=frame_span(p->slotClasses,1u,sizeof(*p->slotClasses));
    }
    ZrParser_ExecIr_PackedFrameLayoutFree(p);
    if(g_frame.active){for(unsigned i=0u;i<n;++i)if(!frame_freed(ids[i],1u))++g_frame.errors;
        /* Production clears the entire real, still-live stack object. */
        memset(&zero,0,sizeof(zero));g_frame.packedCleared+=(unsigned)(memcmp(p,&zero,sizeof(zero))==0);}
}
TZrBool ZrTests_ReturnedExec_AttachFrame(SZrExecIrFunction *function,const SZrSemanticContext *context,
        const SZrExecIrLayout *layouts,TZrUInt32 count,TZrUInt32 limit,SZrExecIrDiagnostic *diagnostic){
    SSourceFrameInput before,after;unsigned captured;
    if(!g_frame.enabled)return ZrParser_ExecIr_AttachPrimitiveSourceFrame(function,context,layouts,count,limit,diagnostic);
    ++g_frame.calls;if(g_frame.active||g_site||!diagnostic||!function)++g_frame.errors;
    captured=(unsigned)frame_input(function,layouts,count,&before);if(!captured)++g_frame.errors;
    g_frame.token=function?function->functionToken:0u;g_frame.first=ZrTests_ExecOwner_Stats().records;g_frame.active=1u;
    g_frame.result=ZrParser_ExecIr_AttachPrimitiveSourceFrame(function,context,layouts,count,limit,diagnostic);
    g_frame.last=ZrTests_ExecOwner_Stats().records;
    if(diagnostic)memcpy(&g_frame.diagnostic,diagnostic,sizeof(*diagnostic));
    if(!g_frame.result){g_frame.preserved=captured&&frame_input(function,layouts,count,&after)&&memcmp(&before,&after,sizeof(before))==0;
        if(function&&(function->frameLayout||function->contract.layoutHash))++g_frame.errors;
    }else if(function&&function->frameLayout&&function->contract.layoutHash){
        g_frame.published=1u;g_frame.slotRecord=frame_span(function->frameLayout->slots,1u,sizeof(*function->frameLayout->slots));
        g_frame.publishedLive=frame_freed(g_frame.headerRecord,0u)&&frame_freed(g_frame.slotRecord,0u)&&
                (uintptr_t)function->frameLayout==g_frame.headerAddress;
    }else ++g_frame.errors;
    g_frame.active=0u;g_observerErrors+=g_frame.errors;return g_frame.result;
}
static unsigned frame_window_released(unsigned first,unsigned last){
    for(unsigned i=first;i<last;++i)if(!frame_freed(i,1u))return 0u;return 1u;
}
static void frame_counts(unsigned site){
    TEST_ASSERT_EQUAL_UINT(1u,g_frame.calls);TEST_ASSERT_EQUAL_UINT(0u,g_frame.errors);
    TEST_ASSERT_EQUAL_UINT(1u,g_frame.valuesCalls);TEST_ASSERT_EQUAL_UINT(site!=1u,g_frame.headerCalls);
    TEST_ASSERT_EQUAL_UINT(site!=1u,g_frame.valuesFrees);TEST_ASSERT_EQUAL_UINT(0u,g_frame.headerFrees);
    TEST_ASSERT_EQUAL_UINT(site!=1u,g_frame.nullFrees);TEST_ASSERT_EQUAL_UINT(site!=1u,g_frame.packedCalls);
    TEST_ASSERT_EQUAL_UINT(site==2u,g_frame.packedSlots);TEST_ASSERT_EQUAL_UINT(site!=1u,g_frame.packedMaps);
    TEST_ASSERT_EQUAL_UINT(site!=1u,g_frame.packedCleared);TEST_ASSERT_EQUAL_UINT(site!=0u,g_frame.arms);
    TEST_ASSERT_EQUAL_UINT(site!=0u,g_frame.hits);TEST_ASSERT_EQUAL_UINT(site==1u,g_frame.valuesEvent.failed);
    if(site!=1u)TEST_ASSERT_EQUAL_UINT(site==2u,g_frame.headerEvent.failed);
}
static void frame_oom(unsigned site){
    SZrSourceCompileDiagnostic d,expected={0};SZrFunction *f;SZrReturnedExecObservation observed;
    unsigned returned,released,live,errors,failures;memset(&g_frame,0,sizeof(g_frame));g_frame.enabled=1u;g_frame.site=site;
    memset(&d,0xA5,sizeof(d));f=compile_source(0u,g_nine,ZR_FALSE,&g_exec,&d);returned=(f!=ZR_NULL);
    observed=ZrTests_ReturnedExec_Observation();released=frame_window_released(g_final.producerFirst,ZrTests_ExecOwner_Stats().records);
    live=ZrTests_ExecOwner_Stats().live;errors=ZrTests_ExecOwner_Stats().errors;failures=ZrTests_ExecOwner_Stats().failures;
    /* Save genuine cleanup before shutdown and before any ledger reset. */
    close_source();assert_closed();TEST_ASSERT_EQUAL_UINT(0u,returned);TEST_ASSERT_EQUAL_UINT(1u,released);
    TEST_ASSERT_EQUAL_UINT(0u,live);TEST_ASSERT_EQUAL_UINT(0u,errors);TEST_ASSERT_EQUAL_UINT(0u,observed.errors);
    TEST_ASSERT_EQUAL_UINT(1u,failures);TEST_ASSERT_TRUE(observed.appendResult);TEST_ASSERT_TRUE(observed.identityValid);
    TEST_ASSERT_EQUAL_UINT32(1u,observed.layoutCount);TEST_ASSERT_EQUAL_UINT64(9u,observed.literalBits);
    TEST_ASSERT_EQUAL_UINT(1u,observed.appendCalls);TEST_ASSERT_EQUAL_UINT(0u,observed.snapshotCalls);
    TEST_ASSERT_EQUAL_UINT(0u,g_final.calls);TEST_ASSERT_EQUAL_UINT(0u,g_final.errors);frame_counts(site);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u,observed.entryToken);TEST_ASSERT_EQUAL_UINT32(observed.entryToken,g_frame.token);
    TEST_ASSERT_FALSE(g_frame.result);TEST_ASSERT_EQUAL_UINT(1u,g_frame.preserved);TEST_ASSERT_EQUAL_UINT(0u,g_frame.published);
    expected.stage=ZR_SOURCE_STAGE_FRAME;expected.result=ZR_SOURCE_RESULT_OUT_OF_MEMORY;expected.hasExecIr=ZR_TRUE;
    expected.execIr.code=ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;expected.execIr.functionToken=g_frame.token;
    diag_equal(&expected,&d);ZrTests_ExecOwner_Diagnostic(&expected.execIr,&g_frame.diagnostic);no_aot();
    /* init_source first checks the saved healthy close, then resets the ledger.
     * Its next compile creates a different fresh Global, with no armed fault. */
    g_frame.enabled=0u;init_source();memset(&g_frame,0,sizeof(g_frame));g_frame.enabled=1u;
    (void)exec(0u,g_nine,9u,ZR_TRUE);
    released=frame_window_released(g_frame.first,g_frame.last);
    unsigned persistentReleased=frame_freed(g_frame.headerRecord,1u)&&frame_freed(g_frame.slotRecord,1u);
    failures=ZrTests_ExecOwner_Stats().failures;
    close_source();assert_closed();frame_counts(0u);TEST_ASSERT_EQUAL_UINT(1u,g_final.calls);
    TEST_ASSERT_TRUE(g_frame.result);TEST_ASSERT_EQUAL_UINT(1u,g_frame.published);TEST_ASSERT_EQUAL_UINT(1u,g_frame.publishedLive);
    TEST_ASSERT_EQUAL_UINT(1u,released);TEST_ASSERT_EQUAL_UINT(1u,persistentReleased);
    TEST_ASSERT_EQUAL_UINT(0u,failures);
    TEST_ASSERT_EQUAL_UINT32(ZrTests_ReturnedExec_Observation().entryToken,g_frame.token);
    SZrExecIrDiagnostic zero={0};ZrTests_ExecOwner_Diagnostic(&zero,&g_frame.diagnostic);
    fprintf(stderr,"RETURNED_EXEC_FRAME_OOM site=%s failed=1 retry=1\n",site==1u?"values":"header");g_frame.enabled=0u;
}
static void test_source_frame_values_oom_cleanup_retry(void){frame_oom(1u);}
static void test_source_frame_header_oom_cleanup_retry(void){frame_oom(2u);}
int main(void){UNITY_BEGIN();
    RUN_TEST(test_none_nine_returns_without_owners);
    RUN_TEST(test_zero_eight_and_null_options_nine_without_owners);
    RUN_TEST(test_aot_nine_returns_only_aot);
    RUN_TEST(test_aot_eight_returns_only_aot);
    RUN_TEST(test_execir_nine_survives_actual_producer_cleanup);
    RUN_TEST(test_execir_eight_preserves_actual_source_metadata);
    RUN_TEST(test_execir_null_diagnostic_preserves_content);
    RUN_TEST(test_unknown_two_refused_before_preparation);
    RUN_TEST(test_mixed_aot_execir_refused_before_preparation);
    RUN_TEST(test_other_unknown_mode_refused_before_preparation);
    RUN_TEST(test_execir_null_state_refused_without_producer);
    RUN_TEST(test_execir_null_source_and_zero_length_refused);
    RUN_TEST(test_execir_bool_reports_actual_constant_refusal);
    RUN_TEST(test_execir_float_reports_actual_constant_refusal);
    RUN_TEST(test_two_returned_execir_owners_are_independent);
    RUN_TEST(test_returned_aot_and_execir_owners_coexist);
    RUN_TEST(test_aot_replacement_preserves_returned_execir);
    RUN_TEST(test_function_free_then_global_shutdown_releases_execir);
    RUN_TEST(test_sixteen_alternating_compile_free_lifecycles);
    RUN_TEST(test_each_source_snapshot_allocation_fault_cleanup_retry);
    RUN_TEST(test_source_frame_values_oom_cleanup_retry);
    RUN_TEST(test_source_frame_header_oom_cleanup_retry);
    close_source();int support=closed_valid();
    printf("RETURNED_EXEC_FINAL closed=%u support=%u vm_live=%u vm_errors=%u owner_live=%u owner_errors=%u observer_errors=%u\n",
           g_closed,(unsigned)support,g_close.vm.live,g_close.vm.errors,g_close.ledgers.ownerBefore.live,
           g_close.ledgers.ownerBefore.errors,g_closeErrors+g_close.observerErrors);
    int result=UNITY_END();return support?result:1;}
