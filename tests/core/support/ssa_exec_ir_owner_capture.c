#include "ssa_exec_ir_owner_capture.h"
#include <stdint.h>
#include <string.h>
typedef struct SZrOwnerVisit {
    int checked;
    unsigned words;
    int (*emit)(void *,TZrUInt64);
    void *context;
} SZrOwnerVisit;
static int storage_valid(const void *storage,TZrUInt32 count,TZrUInt32 capacity,size_t width) {
    return count <= capacity && (count == 0u || storage != ZR_NULL) &&
           (count == 0u || width <= SIZE_MAX / count);
}
#define ROOM() do { if (v->checked && v->words >= ZR_OWNER_WORDS) return 0; } while (0)
#define EMIT(value) do { ROOM(); if (!v->emit(v->context, (TZrUInt64)(value))) return 0; \
    if (v->checked) ++v->words; } while (0)
#define W(f) EMIT(o->f)
#define SHAPE(o,field,count,capacity) do { if (v->checked && \
    !storage_valid((o)->field,(o)->count,(o)->capacity,sizeof(*(o)->field))) return 0; } while (0)
#define R(f) W(f.offset);W(f.count)
static int capture_contract(const SZrExecutionContract *o,SZrOwnerVisit *v) {
    ROOM();
    W(schemaVersion);W(abiVersion);W(logicalVersion);W(reserved0);W(generation);W(targetToken);W(reserved1);
    W(signatureHash);W(layoutHash);W(moduleHash);W(requiredCapabilities);W(declaredEffects);
    return 1;
}
static int capture_function(const SZrExecIrFunction *f,SZrOwnerVisit *v) {
    ROOM();
    if (v->checked && (f->gcMapCount > f->gcMapCapacity ||
        (f->gcMapCount != 0u && f->gcMap == ZR_NULL))) return 0;
    SHAPE(f,values,valueCount,valueCapacity);
    SHAPE(f,instructions,instructionCount,instructionCapacity);
    SHAPE(f,blocks,blockCount,blockCapacity);
    SHAPE(f,operandPool,operandCount,operandCapacity);
    SHAPE(f,resultPool,resultCount,resultCapacity);
    SHAPE(f,memoryTokenPool,memoryTokenCount,memoryTokenCapacity);
    SHAPE(f,phiPool,phiCount,phiCapacity);
    SHAPE(f,phiIncoming,phiIncomingCount,phiIncomingCapacity);
    SHAPE(f,predecessors,predecessorCount,predecessorCapacity);
    SHAPE(f,successors,successorCount,successorCapacity);
    SHAPE(f,gcRoots,gcRootCount,gcRootCapacity);
    SHAPE(f,deoptStates,deoptStateCount,deoptStateCapacity);
    SHAPE(f,deoptValues,deoptValueCount,deoptValueCapacity);
    SHAPE(f,deoptAggregates,deoptAggregateCount,deoptAggregateCapacity);
    SHAPE(f,deoptAggregateFields,deoptAggregateFieldCount,deoptAggregateFieldCapacity);
    SHAPE(f,sourceMaps,sourceMapCount,sourceMapCapacity);
    SHAPE(f,bindingRows,bindingRowCount,bindingRowCapacity);
    { const SZrExecIrFunction *o=f;
    W(id);W(functionToken);W(signatureHash);W(entryBlockId);W(bindingRowsSchemaVersion);W(sealed);W(gcMapCount);
    if (!capture_contract(&f->contract,v)) return 0;
#define COUNT(field) W(field)
    COUNT(valueCount);COUNT(instructionCount);COUNT(blockCount);COUNT(operandCount);COUNT(resultCount);
    COUNT(memoryTokenCount);COUNT(phiCount);COUNT(phiIncomingCount);COUNT(predecessorCount);COUNT(successorCount);
    COUNT(gcRootCount);COUNT(deoptStateCount);COUNT(deoptValueCount);COUNT(deoptAggregateCount);COUNT(deoptAggregateFieldCount);
    COUNT(sourceMapCount);COUNT(bindingRowCount);
#undef COUNT
    }
    for(TZrUInt32 i=0u;i<f->valueCount;++i){ROOM();const SZrExecIrValue *o=&f->values[i];W(id);W(definition);W(typeToken);W(ownership);W(nullability);W(flags);}
    for(TZrUInt32 i=0u;i<f->instructionCount;++i){ROOM();const SZrExecIrInstruction *o=&f->instructions[i];
        W(opcode);W(flags);R(results);R(operands);R(phiRange);R(successorRange);W(typeToken);W(matchTypeToken);W(layoutId);
        R(memoryIn);R(memoryOut);W(effectIn);W(effectOut);W(sourceId);W(deoptId);W(bindingRow);}
    for(TZrUInt32 i=0u;i<f->blockCount;++i){ROOM();const SZrExecIrBlock *o=&f->blocks[i];
        W(id);W(flags);R(instructions);R(predecessors);R(successors);R(phis);W(effectPhiResult);R(effectPhiIncomings);
        for(unsigned j=0u;j<ZR_EXEC_IR_MEMORY_CLASS_COUNT;++j){EMIT(o->memoryPhiResults[j]);EMIT(o->memoryPhiIncomings[j].offset);EMIT(o->memoryPhiIncomings[j].count);}
        W(immediateDominator);W(terminatorInstructionId);}
#define NUM(field,count) for(TZrUInt32 i=0u;i<f->count;++i){EMIT(f->field[i]);}
    NUM(operandPool,operandCount);NUM(resultPool,resultCount);NUM(memoryTokenPool,memoryTokenCount);
    NUM(predecessors,predecessorCount);NUM(successors,successorCount);NUM(gcRoots,gcRootCount);NUM(deoptValues,deoptValueCount);
#undef NUM
    for(TZrUInt32 i=0u;i<f->phiCount;++i){ROOM();const SZrExecIrPhi *o=&f->phiPool[i];W(result);R(incomings);}
    for(TZrUInt32 i=0u;i<f->phiIncomingCount;++i){ROOM();const SZrExecIrPhiIncoming *o=&f->phiIncoming[i];W(predecessor);W(value);}
    for(TZrUInt32 i=0u;i<f->deoptStateCount;++i){ROOM();const SZrExecIrDeoptState *o=&f->deoptStates[i];W(id);W(sourceId);W(resumeId);R(valueRange);W(cleanupState);R(aggregates);}
    for(TZrUInt32 i=0u;i<f->deoptAggregateCount;++i){ROOM();const SZrExecIrDeoptAggregate *o=&f->deoptAggregates[i];W(identityId);W(typeToken);W(layoutId);R(fields);}
    for(TZrUInt32 i=0u;i<f->deoptAggregateFieldCount;++i){ROOM();const SZrExecIrDeoptAggregateField *o=&f->deoptAggregateFields[i];W(fieldIndex);W(kind);W(valueId);W(aggregateId);}
    for(TZrUInt32 i=0u;i<f->sourceMapCount;++i){ROOM();const SZrExecIrSourceMap *o=&f->sourceMaps[i];W(sourceId);W(instructionId);W(startOffset);W(endOffset);W(startLine);W(startColumn);W(endLine);W(endColumn);}
    for(TZrUInt32 i=0u;i<f->bindingRowCount;++i){ROOM();const SZrExecIrBindingRow *o=&f->bindingRows[i];
        W(rowIndex);W(instructionId);W(segmentIndex);W(sourceId);
        W(contract.bindingKind);W(contract.targetMetadataToken);W(contract.signatureToken);W(contract.ownerTypeToken);
        W(contract.signatureHash);W(contract.moduleSignatureHash);W(contract.layoutVersion);W(contract.dispatchSlot);
        W(contract.layoutHash);W(contract.operation);W(contract.reserved0);W(contract.reserved1);
        W(location.kind);W(location.targetIndex);W(location.ownerDepth);W(location.flags);}
    EMIT(f->gcMap != ZR_NULL);
    if(f->gcMap != ZR_NULL){
        ROOM();
        SHAPE(f->gcMap,entries,entryCount,entryCapacity);
        SHAPE(f->gcMap,slotIndexPool,slotIndexCount,slotIndexCapacity);
        SHAPE(f->gcMap,inlineRefOffsetPool,inlineRefOffsetCount,inlineRefOffsetCapacity);
        {const SZrExecIrGcMap *o=f->gcMap;W(entryCount);W(slotIndexCount);W(inlineRefOffsetCount);W(safepointId);W(sourceId);R(rootRange);}
        for(TZrUInt32 i=0u;i<f->gcMap->entryCount;++i){ROOM();const SZrExecIrGcMapEntry *o=&f->gcMap->entries[i];W(site);R(liveRefSlots);R(inlineRefOffsets);}
        for(TZrUInt32 i=0u;i<f->gcMap->slotIndexCount;++i)EMIT(f->gcMap->slotIndexPool[i]);
        for(TZrUInt32 i=0u;i<f->gcMap->inlineRefOffsetCount;++i)EMIT(f->gcMap->inlineRefOffsetPool[i]);}
    EMIT(f->stateMap != ZR_NULL);
    if(f->stateMap != ZR_NULL){
        ROOM();
        SHAPE(f->stateMap,entries,entryCount,entryCapacity);
        SHAPE(f->stateMap,valuePool,valueCount,valueCapacity);
        SHAPE(f->stateMap,rootPool,rootCount,rootCapacity);
        SHAPE(f->stateMap,ownerStatePool,ownerStateCount,ownerStateCapacity);
        {const SZrExecIrStateMap *o=f->stateMap;W(functionToken);W(signatureHash);W(generation);W(entryCount);W(valueCount);W(rootCount);W(ownerStateCount);}
        for(TZrUInt32 i=0u;i<f->stateMap->entryCount;++i){ROOM();const SZrExecIrStateMapEntry *o=&f->stateMap->entries[i];
            W(sourceId);W(instructionId);W(deoptId);W(resumeId);W(cleanupState);W(boundaryFlags);W(phase);
            R(liveValues);R(rootValues);R(ownerStates);W(effectIn);W(effectOut);W(handlerBlockId);W(exceptionState);}
        for(TZrUInt32 i=0u;i<f->stateMap->valueCount;++i)EMIT(f->stateMap->valuePool[i]);
        for(TZrUInt32 i=0u;i<f->stateMap->rootCount;++i)EMIT(f->stateMap->rootPool[i]);
        for(TZrUInt32 i=0u;i<f->stateMap->ownerStateCount;++i)EMIT(f->stateMap->ownerStatePool[i]);}
    EMIT(f->frameLayout != ZR_NULL);
    if(f->frameLayout != ZR_NULL){
        ROOM();
        SHAPE(f->frameLayout,slots,slotCount,slotCapacity);
        {const SZrExecIrFrameLayout *o=f->frameLayout;
        W(storageSlotCount);W(logicalSlotCount);W(parameterPrefixCount);W(returnBufferOffset);W(frameByteSize);
        W(frameByteAlign);W(parameterCount);W(localCount);W(slotCount);W(layoutHash);}
        for(TZrUInt32 i=0u;i<f->frameLayout->slotCount;++i){ROOM();const SZrExecIrFrameSlot *o=&f->frameLayout->slots[i];W(slotId);W(byteOffset);W(byteSize);W(byteAlign);W(typeToken);W(kind);}}
    return 1;
}
int ZrTests_ExecOwner_VisitContents(const SZrExecIrModule *o,int checkedGeometry,
                                  int (*emit)(void *,TZrUInt64),void *context) {
    SZrOwnerVisit visit = {checkedGeometry,0u,emit,context};
    SZrOwnerVisit *v = &visit;
    if (v->checked && (o == ZR_NULL || emit == ZR_NULL)) return 0;
    SHAPE(o,constants,constantCount,constantCapacity);
    SHAPE(o,layouts,layoutCount,layoutCapacity);
    SHAPE(o,sourceMaps,sourceMapCount,sourceMapCapacity);
    SHAPE(o,functions,functionCount,functionCapacity);
    W(id);W(moduleToken);W(moduleHash);if (!capture_contract(&o->contract,v)) return 0;
    W(constantCount);W(layoutCount);W(sourceMapCount);W(functionCount);
    for(TZrUInt32 i=0u;i<o->constantCount;++i){EMIT(o->constants[i].typeToken);EMIT(o->constants[i].flags);EMIT(o->constants[i].bits);}
    for(TZrUInt32 i=0u;i<o->layoutCount;++i){ROOM();const SZrExecIrLayout *r=&o->layouts[i];EMIT(r->id);EMIT(r->typeToken);EMIT(r->byteSize);EMIT(r->byteAlign);EMIT(r->layoutHash);}
    for(TZrUInt32 i=0u;i<o->sourceMapCount;++i){ROOM();const SZrExecIrSourceMap *r=&o->sourceMaps[i];EMIT(r->sourceId);EMIT(r->instructionId);EMIT(r->startOffset);EMIT(r->endOffset);EMIT(r->startLine);EMIT(r->startColumn);EMIT(r->endLine);EMIT(r->endColumn);}
    for(TZrUInt32 i=0u;i<o->functionCount;++i){ROOM();if (!capture_function(&o->functions[i],v)) return 0;}
    return 1;
}
#undef W
#undef R
#undef EMIT
#undef ROOM
#undef SHAPE
static int candidate_word(void *context,TZrUInt64 value) {
    SZrOwnerContents *candidate = context;
    if (candidate->count >= ZR_OWNER_WORDS) return 0;
    candidate->words[candidate->count++] = value;
    return 1;
}
int ZrTests_ExecOwner_TryCapture(const SZrExecIrModule *module,SZrOwnerContents *contents) {
    SZrOwnerContents candidate;
    if (module == ZR_NULL || contents == ZR_NULL) return 0;
    memset(&candidate,0,sizeof candidate);
    if (!ZrTests_ExecOwner_VisitContents(module,1,candidate_word,&candidate)) return 0;
    memcpy(contents,&candidate,sizeof candidate);
    return 1;
}
