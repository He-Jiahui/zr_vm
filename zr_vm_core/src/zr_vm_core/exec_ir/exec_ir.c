#include "zr_vm_core/exec_ir.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define ZR_EXEC_IR_INITIAL_CAPACITY ((TZrUInt32)4u)

static void zr_exec_ir_clear_diagnostic(SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static void zr_exec_ir_set_diagnostic(SZrExecIrDiagnostic *diagnostic,
                                      EZrExecutionDiagnosticCode code,
                                      const SZrExecIrFunction *function,
                                      TZrUInt32 instructionId,
                                      TZrUInt32 blockId,
                                      TZrUInt32 expected,
                                      TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->instructionId = instructionId;
    diagnostic->blockId = blockId;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

static TZrBool zr_exec_ir_size_is_valid(TZrSize count, size_t elementSize) {
    return (TZrBool)(count <= (TZrSize)UINT32_MAX &&
                     (elementSize == 0u || count <= SIZE_MAX / elementSize));
}

static TZrBool zr_exec_ir_reserve(void **storage,
                                  TZrUInt32 *capacity,
                                  TZrSize requested,
                                  size_t elementSize) {
    TZrSize newCapacity;
    void *newStorage;

    if (storage == ZR_NULL || capacity == ZR_NULL || elementSize == 0u ||
        !zr_exec_ir_size_is_valid(requested, elementSize)) {
        return ZR_FALSE;
    }
    if (requested <= (TZrSize)*capacity) {
        return ZR_TRUE;
    }
    newCapacity = *capacity == 0u ? ZR_EXEC_IR_INITIAL_CAPACITY : (TZrSize)*capacity;
    while (newCapacity < requested) {
        if (newCapacity > (TZrSize)UINT32_MAX / 2u) {
            newCapacity = requested;
            break;
        }
        newCapacity *= 2u;
    }
    if (!zr_exec_ir_size_is_valid(newCapacity, elementSize)) {
        return ZR_FALSE;
    }
    newStorage = realloc(*storage, (size_t)newCapacity * elementSize);
    if (newStorage == ZR_NULL) {
        return ZR_FALSE;
    }
    *storage = newStorage;
    *capacity = (TZrUInt32)newCapacity;
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_reserve_with_diagnostic(void **storage,
                                                  TZrUInt32 *capacity,
                                                  TZrSize requested,
                                                  size_t elementSize,
                                                  const SZrExecIrFunction *function,
                                                  SZrExecIrDiagnostic *diagnostic) {
    zr_exec_ir_clear_diagnostic(diagnostic);
    if (storage == ZR_NULL || capacity == ZR_NULL || elementSize == 0u ||
        requested > (TZrSize)UINT32_MAX ||
        (elementSize != 0u && requested > SIZE_MAX / elementSize)) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                  function,
                                  0u,
                                  0u,
                                  UINT32_MAX,
                                  requested > (TZrSize)UINT32_MAX
                                      ? UINT32_MAX
                                      : (TZrUInt32)requested);
        return ZR_FALSE;
    }
    if (!zr_exec_ir_reserve(storage, capacity, requested, elementSize)) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                  function,
                                  0u,
                                  0u,
                                  (TZrUInt32)requested,
                                  *capacity);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_append(void **storage,
                                 TZrUInt32 *count,
                                 TZrUInt32 *capacity,
                                 const void *items,
                                 TZrSize itemCount,
                                 size_t elementSize,
                                 SZrExecIrRange *outRange) {
    TZrUInt32 oldCount;

    if (storage == ZR_NULL || count == ZR_NULL || capacity == ZR_NULL ||
        (itemCount != 0u && items == ZR_NULL) ||
        itemCount > (TZrSize)UINT32_MAX - (TZrSize)*count) {
        return ZR_FALSE;
    }
    oldCount = *count;
    if (!zr_exec_ir_reserve(storage,
                            capacity,
                            (TZrSize)oldCount + itemCount,
                            elementSize)) {
        return ZR_FALSE;
    }
    if (itemCount != 0u) {
        memcpy((TZrByte *)*storage + (size_t)oldCount * elementSize,
               items,
               (size_t)itemCount * elementSize);
    }
    *count = oldCount + (TZrUInt32)itemCount;
    if (outRange != ZR_NULL) {
        outRange->start = oldCount;
        outRange->count = (TZrUInt32)itemCount;
    }
    return ZR_TRUE;
}

static void zr_exec_ir_free_function_arrays(SZrExecIrFunction *function) {
    if (function == ZR_NULL) {
        return;
    }
    free(function->values);
    free(function->instructions);
    free(function->blocks);
    free(function->operands);
    free(function->results);
    free(function->memoryTokenPool);
    free(function->phiPool);
    free(function->phiIncoming);
    free(function->predecessors);
    free(function->successors);
    if (function->frameLayout != ZR_NULL) {
        ZrCore_ExecIr_FrameLayoutFree(function->frameLayout);
        free(function->frameLayout);
    }
    if (function->gcMap != ZR_NULL) {
        ZrCore_ExecIr_GcMapFree(function->gcMap);
        free(function->gcMap);
    }
    free(function->gcRoots);
    free(function->deoptStates);
    free(function->deoptValues);
    free(function->sourceMaps);
}

void ZrCore_ExecIr_FrameLayoutInit(SZrExecIrFrameLayout *layout) {
    if (layout != ZR_NULL) {
        memset(layout, 0, sizeof(*layout));
    }
}

void ZrCore_ExecIr_FrameLayoutFree(SZrExecIrFrameLayout *layout) {
    if (layout != ZR_NULL) {
        free(layout->slots);
        memset(layout, 0, sizeof(*layout));
    }
}

void ZrCore_ExecIr_GcMapInit(SZrExecIrGcMap *map) {
    if (map != ZR_NULL) {
        memset(map, 0, sizeof(*map));
    }
}

void ZrCore_ExecIr_GcMapFree(SZrExecIrGcMap *map) {
    if (map != ZR_NULL) {
        free(map->entries);
        free(map->slotIndexPool);
        free(map->inlineRefOffsetPool);
        memset(map, 0, sizeof(*map));
    }
}

void ZrCore_ExecIr_FunctionInit(SZrExecIrFunction *function) {
    if (function != ZR_NULL) {
        memset(function, 0, sizeof(*function));
        function->entryBlockId = ZR_EXEC_IR_BLOCK_ID_INVALID;
    }
}

void ZrCore_ExecIr_ModuleInit(SZrExecIrModule *module) {
    if (module != ZR_NULL) {
        memset(module, 0, sizeof(*module));
        module->id = ZR_EXEC_IR_MODULE_ID_INVALID;
    }
}

void ZrCore_ExecIr_FreeFunction(SZrExecIrFunction *function) {
    if (function == ZR_NULL) {
        return;
    }
    zr_exec_ir_free_function_arrays(function);
    memset(function, 0, sizeof(*function));
}

void ZrCore_ExecIr_FreeModule(SZrExecIrModule *module) {
    TZrUInt32 index;

    if (module == ZR_NULL) {
        return;
    }
    for (index = 0u; index < module->functionCount; ++index) {
        ZrCore_ExecIr_FreeFunction(&module->functions[index]);
    }
    free(module->functions);
    free(module->constants);
    free(module->layouts);
    free(module->sourceMaps);
    memset(module, 0, sizeof(*module));
}

static TZrBool zr_exec_ir_clone_array(void **destination,
                                       TZrUInt32 *destinationCapacity,
                                       const void *source,
                                       TZrUInt32 count,
                                       size_t elementSize) {
    if (count == 0u) {
        return ZR_TRUE;
    }
    if (source == ZR_NULL ||
        !zr_exec_ir_reserve(destination, destinationCapacity, count, elementSize)) {
        return ZR_FALSE;
    }
    memcpy(*destination, source, (size_t)count * elementSize);
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_clone_function_into(const SZrExecIrFunction *source,
                                              SZrExecIrFunction *destination) {
    ZrCore_ExecIr_FunctionInit(destination);
    destination->id = source->id;
    destination->functionToken = source->functionToken;
    destination->signatureHash = source->signatureHash;
    destination->contract = source->contract;
    destination->entryBlockId = source->entryBlockId;

#define ZR_EXEC_IR_CLONE_FIELD(field, countField, elementType) \
    do { \
        if (!zr_exec_ir_clone_array((void **)&destination->field, \
                                     &destination->countField##Capacity, \
                                     source->field, \
                                     source->countField, \
                                     sizeof(elementType))) { \
            return ZR_FALSE; \
        } \
        destination->countField = source->countField; \
    } while (0)

    /* The macro above cannot form all capacity member names portably; keep
     * the explicit copies below so every allocation remains auditable. */
#undef ZR_EXEC_IR_CLONE_FIELD
    if (!zr_exec_ir_clone_array((void **)&destination->values, &destination->valueCapacity,
                                source->values, source->valueCount, sizeof(*source->values))) return ZR_FALSE;
    destination->valueCount = source->valueCount;
    if (!zr_exec_ir_clone_array((void **)&destination->instructions, &destination->instructionCapacity,
                                source->instructions, source->instructionCount, sizeof(*source->instructions))) return ZR_FALSE;
    destination->instructionCount = source->instructionCount;
    if (!zr_exec_ir_clone_array((void **)&destination->blocks, &destination->blockCapacity,
                                source->blocks, source->blockCount, sizeof(*source->blocks))) return ZR_FALSE;
    destination->blockCount = source->blockCount;
    if (!zr_exec_ir_clone_array((void **)&destination->operands, &destination->operandCapacity,
                                source->operands, source->operandCount, sizeof(*source->operands))) return ZR_FALSE;
    destination->operandCount = source->operandCount;
    if (!zr_exec_ir_clone_array((void **)&destination->results, &destination->resultCapacity,
                                source->results, source->resultCount, sizeof(*source->results))) return ZR_FALSE;
    destination->resultCount = source->resultCount;
    if (!zr_exec_ir_clone_array((void **)&destination->memoryTokenPool,
                                &destination->memoryTokenCapacity,
                                source->memoryTokenPool,
                                source->memoryTokenCount,
                                sizeof(*source->memoryTokenPool))) return ZR_FALSE;
    destination->memoryTokenCount = source->memoryTokenCount;
    if (!zr_exec_ir_clone_array((void **)&destination->phiPool,
                                &destination->phiCapacity,
                                source->phiPool,
                                source->phiCount,
                                sizeof(*source->phiPool))) return ZR_FALSE;
    destination->phiCount = source->phiCount;
    if (!zr_exec_ir_clone_array((void **)&destination->phiIncoming, &destination->phiIncomingCapacity,
                                source->phiIncoming, source->phiIncomingCount, sizeof(*source->phiIncoming))) return ZR_FALSE;
    destination->phiIncomingCount = source->phiIncomingCount;
    if (!zr_exec_ir_clone_array((void **)&destination->predecessors, &destination->predecessorCapacity,
                                source->predecessors, source->predecessorCount, sizeof(*source->predecessors))) return ZR_FALSE;
    destination->predecessorCount = source->predecessorCount;
    if (!zr_exec_ir_clone_array((void **)&destination->successors, &destination->successorCapacity,
                                source->successors, source->successorCount, sizeof(*source->successors))) return ZR_FALSE;
    destination->successorCount = source->successorCount;
    destination->gcMapCount = source->gcMapCount;
    destination->gcMapCapacity = source->gcMapCapacity;
    if (source->gcMap != ZR_NULL) {
        destination->gcMap = (SZrExecIrGcMap *)calloc(1u, sizeof(*destination->gcMap));
        if (destination->gcMap == ZR_NULL) return ZR_FALSE;
        *destination->gcMap = *source->gcMap;
        destination->gcMap->entries = ZR_NULL;
        destination->gcMap->slotIndexPool = ZR_NULL;
        destination->gcMap->inlineRefOffsetPool = ZR_NULL;
        destination->gcMap->entryCapacity = 0u;
        destination->gcMap->slotIndexCapacity = 0u;
        destination->gcMap->inlineRefOffsetCapacity = 0u;
        if (!zr_exec_ir_clone_array((void **)&destination->gcMap->entries,
                                     &destination->gcMap->entryCapacity,
                                     source->gcMap->entries,
                                     source->gcMap->entryCount,
                                     sizeof(*source->gcMap->entries)) ||
            !zr_exec_ir_clone_array((void **)&destination->gcMap->slotIndexPool,
                                     &destination->gcMap->slotIndexCapacity,
                                     source->gcMap->slotIndexPool,
                                     source->gcMap->slotIndexCount,
                                     sizeof(*source->gcMap->slotIndexPool)) ||
            !zr_exec_ir_clone_array((void **)&destination->gcMap->inlineRefOffsetPool,
                                     &destination->gcMap->inlineRefOffsetCapacity,
                                     source->gcMap->inlineRefOffsetPool,
                                     source->gcMap->inlineRefOffsetCount,
                                     sizeof(*source->gcMap->inlineRefOffsetPool))) return ZR_FALSE;
    }
    if (!zr_exec_ir_clone_array((void **)&destination->gcRoots, &destination->gcRootCapacity,
                                source->gcRoots, source->gcRootCount, sizeof(*source->gcRoots))) return ZR_FALSE;
    destination->gcRootCount = source->gcRootCount;
    if (!zr_exec_ir_clone_array((void **)&destination->deoptStates, &destination->deoptStateCapacity,
                                source->deoptStates, source->deoptStateCount, sizeof(*source->deoptStates))) return ZR_FALSE;
    destination->deoptStateCount = source->deoptStateCount;
    if (!zr_exec_ir_clone_array((void **)&destination->deoptValues, &destination->deoptValueCapacity,
                                source->deoptValues, source->deoptValueCount, sizeof(*source->deoptValues))) return ZR_FALSE;
    destination->deoptValueCount = source->deoptValueCount;
    if (!zr_exec_ir_clone_array((void **)&destination->sourceMaps, &destination->sourceMapCapacity,
                                source->sourceMaps, source->sourceMapCount, sizeof(*source->sourceMaps))) return ZR_FALSE;
    destination->sourceMapCount = source->sourceMapCount;
    if (source->frameLayout != ZR_NULL) {
        destination->frameLayout = (SZrExecIrFrameLayout *)calloc(1u, sizeof(*destination->frameLayout));
        if (destination->frameLayout == ZR_NULL) return ZR_FALSE;
        *destination->frameLayout = *source->frameLayout;
        destination->frameLayout->slots = ZR_NULL;
        destination->frameLayout->slotCapacity = 0u;
        if (!zr_exec_ir_clone_array((void **)&destination->frameLayout->slots,
                                     &destination->frameLayout->slotCapacity,
                                     source->frameLayout->slots,
                                     source->frameLayout->slotCount,
                                     sizeof(*source->frameLayout->slots))) return ZR_FALSE;
    }
    destination->sealed = source->sealed;
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_CloneFunction(const SZrExecIrFunction *source,
                                    SZrExecIrFunction *destination,
                                    SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrFunction temporary;

    zr_exec_ir_clear_diagnostic(diagnostic);
    if (source == ZR_NULL || destination == ZR_NULL || source == destination) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  source,
                                  0u,
                                  0u,
                                  0u,
                                  0u);
        return ZR_FALSE;
    }
    ZrCore_ExecIr_FunctionInit(&temporary);
    if (!zr_exec_ir_clone_function_into(source, &temporary)) {
        ZrCore_ExecIr_FreeFunction(&temporary);
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  source,
                                  0u,
                                  0u,
                                  0u,
                                  0u);
        return ZR_FALSE;
    }
    ZrCore_ExecIr_FreeFunction(destination);
    *destination = temporary;
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_ModuleAddFunction(SZrExecIrModule *module,
                                         TZrMetadataToken functionToken,
                                         TZrUInt64 signatureHash,
                                         TZrExecIrFunctionId *outId) {
    SZrExecIrFunction *function;
    TZrUInt32 id;

    if (module == ZR_NULL || functionToken == 0u || module->functionCount == UINT32_MAX ||
        !zr_exec_ir_reserve((void **)&module->functions,
                            &module->functionCapacity,
                            (TZrSize)module->functionCount + 1u,
                            sizeof(*module->functions))) {
        return ZR_FALSE;
    }
    id = module->functionCount + 1u;
    function = &module->functions[module->functionCount];
    ZrCore_ExecIr_FunctionInit(function);
    function->id = id;
    function->functionToken = functionToken;
    function->signatureHash = signatureHash;
    function->contract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    function->contract.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    function->contract.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    function->contract.generation = 1u;
    function->contract.targetToken = functionToken;
    function->contract.signatureHash = signatureHash;
    module->functionCount++;
    if (outId != ZR_NULL) {
        *outId = id;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_ModuleAppendConstant(SZrExecIrModule *module,
                                           const SZrExecIrConstant *constant,
                                           TZrSize count,
                                           SZrExecIrRange *outRange) {
    if (module == ZR_NULL || (count != 0u && constant == ZR_NULL)) {
        return ZR_FALSE;
    }
    return zr_exec_ir_append((void **)&module->constants,
                             &module->constantCount,
                             &module->constantCapacity,
                             constant,
                             count,
                             sizeof(*module->constants),
                             outRange);
}

TZrBool ZrCore_ExecIr_ModuleAppendLayout(SZrExecIrModule *module,
                                         const SZrExecIrLayout *layout,
                                         TZrSize count,
                                         SZrExecIrRange *outRange) {
    if (module == ZR_NULL || (count != 0u && layout == ZR_NULL)) {
        return ZR_FALSE;
    }
    return zr_exec_ir_append((void **)&module->layouts,
                             &module->layoutCount,
                             &module->layoutCapacity,
                             layout,
                             count,
                             sizeof(*module->layouts),
                             outRange);
}

SZrExecIrFunction *ZrCore_ExecIr_ModuleFunctionAt(SZrExecIrModule *module,
                                                   TZrExecIrFunctionId id) {
    if (module == ZR_NULL || id == ZR_EXEC_IR_FUNCTION_ID_INVALID || id > module->functionCount) {
        return ZR_NULL;
    }
    return &module->functions[id - 1u];
}

const SZrExecIrFunction *ZrCore_ExecIr_ModuleFunctionAtConst(const SZrExecIrModule *module,
                                                              TZrExecIrFunctionId id) {
    if (module == ZR_NULL || id == ZR_EXEC_IR_FUNCTION_ID_INVALID || id > module->functionCount) {
        return ZR_NULL;
    }
    return &module->functions[id - 1u];
}

TZrExecIrBlockId ZrCore_ExecIr_FunctionAddBlock(SZrExecIrFunction *function,
                                                 TZrUInt32 flags) {
    SZrExecIrBlock *block;
    TZrExecIrBlockId id;

    if (function == ZR_NULL || function->sealed || (flags & ~(ZR_EXEC_IR_BLOCK_FLAG_ENTRY |
                                           ZR_EXEC_IR_BLOCK_FLAG_COLD |
                                           ZR_EXEC_IR_BLOCK_FLAG_CLEANUP |
                                           ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION)) != 0u ||
        function->blockCount == UINT32_MAX ||
        ((flags & ZR_EXEC_IR_BLOCK_FLAG_ENTRY) != 0u &&
         function->entryBlockId != ZR_EXEC_IR_BLOCK_ID_INVALID) ||
        !zr_exec_ir_reserve((void **)&function->blocks,
                            &function->blockCapacity,
                            (TZrSize)function->blockCount + 1u,
                            sizeof(*function->blocks))) {
        return ZR_EXEC_IR_BLOCK_ID_INVALID;
    }
    id = function->blockCount + 1u;
    block = &function->blocks[function->blockCount];
    memset(block, 0, sizeof(*block));
    block->id = id;
    block->flags = flags;
    block->terminatorInstructionId = ZR_EXEC_IR_INSTRUCTION_ID_INVALID;
    function->blockCount++;
    if ((flags & ZR_EXEC_IR_BLOCK_FLAG_ENTRY) != 0u) {
        function->entryBlockId = id;
    }
    return id;
}

TZrExecIrValueId ZrCore_ExecIr_FunctionAddValue(SZrExecIrFunction *function,
                                                 TZrMetadataToken typeToken,
                                                 EZrExecIrOwnership ownership,
                                                 EZrExecIrNullability nullability) {
    SZrExecIrValue *value;
    TZrExecIrValueId id;

    if (function == ZR_NULL || function->sealed || ownership >= ZR_EXEC_IR_OWNERSHIP_COUNT ||
        nullability >= ZR_EXEC_IR_NULLABILITY_COUNT || function->valueCount == UINT32_MAX ||
        !zr_exec_ir_reserve((void **)&function->values,
                            &function->valueCapacity,
                            (TZrSize)function->valueCount + 1u,
                            sizeof(*function->values))) {
        return ZR_EXEC_IR_VALUE_ID_INVALID;
    }
    id = function->valueCount + 1u;
    value = &function->values[function->valueCount];
    memset(value, 0, sizeof(*value));
    value->id = id;
    value->typeToken = typeToken;
    value->ownership = ownership;
    value->nullability = nullability;
    function->valueCount++;
    return id;
}

TZrBool ZrCore_ExecIr_FunctionAppendOperands(SZrExecIrFunction *function,
                                              const TZrExecIrValueId *operands,
                                              TZrSize count,
                                              SZrExecIrRange *outRange) {
    if (function == ZR_NULL || function->sealed) {
        return ZR_FALSE;
    }
    return zr_exec_ir_append((void **)&function->operands,
                             &function->operandCount,
                             &function->operandCapacity,
                             operands,
                             count,
                             sizeof(*function->operands),
                             outRange);
}

TZrBool ZrCore_ExecIr_FunctionAppendResults(SZrExecIrFunction *function,
                                             const TZrExecIrValueId *results,
                                             TZrSize count,
                                             SZrExecIrRange *outRange) {
    if (function == ZR_NULL || function->sealed) {
        return ZR_FALSE;
    }
    return zr_exec_ir_append((void **)&function->results,
                             &function->resultCount,
                             &function->resultCapacity,
                             results,
                             count,
                             sizeof(*function->results),
                             outRange);
}

TZrBool ZrCore_ExecIr_FunctionAppendPhiIncoming(SZrExecIrFunction *function,
                                                const SZrExecIrPhiIncoming *incoming,
                                                TZrSize count,
                                                SZrExecIrRange *outRange) {
    if (function == ZR_NULL || function->sealed) {
        return ZR_FALSE;
    }
    return zr_exec_ir_append((void **)&function->phiIncoming,
                             &function->phiIncomingCount,
                             &function->phiIncomingCapacity,
                             incoming,
                             count,
                             sizeof(*function->phiIncoming),
                             outRange);
}

TZrBool ZrCore_ExecIr_FunctionAppendInstruction(SZrExecIrFunction *function,
                                                 const SZrExecIrInstruction *instruction,
                                                 TZrExecIrInstructionId *outId) {
    SZrExecIrInstruction *destination;
    TZrExecIrInstructionId id;
    TZrUInt32 resultIndex;

    if (function == ZR_NULL || function->sealed || instruction == ZR_NULL || function->instructionCount == UINT32_MAX ||
        instruction->resultRange.start > function->resultCount ||
        instruction->resultRange.count > function->resultCount - instruction->resultRange.start ||
        (instruction->resultRange.count != 0u &&
         (function->results == ZR_NULL || function->values == ZR_NULL)) ||
        !zr_exec_ir_reserve((void **)&function->instructions,
                            &function->instructionCapacity,
                            (TZrSize)function->instructionCount + 1u,
                            sizeof(*function->instructions))) {
        return ZR_FALSE;
    }
    for (resultIndex = instruction->resultRange.start;
         resultIndex < instruction->resultRange.start + instruction->resultRange.count;
         ++resultIndex) {
        TZrExecIrValueId valueId = function->results[resultIndex];
        if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID || valueId > function->valueCount ||
            function->values[valueId - 1u].definition != ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
            return ZR_FALSE;
        }
    }
    id = function->instructionCount + 1u;
    destination = &function->instructions[function->instructionCount];
    *destination = *instruction;
    function->instructionCount++;
    for (resultIndex = instruction->resultRange.start;
         resultIndex < instruction->resultRange.start + instruction->resultRange.count;
         ++resultIndex) {
        function->values[function->results[resultIndex] - 1u].definition = id;
    }
    if (outId != ZR_NULL) {
        *outId = id;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_FunctionReserveOperands(SZrExecIrFunction *function,
                                                TZrSize capacity) {
    return ZrCore_ExecIr_FunctionReserveOperandsEx(function, capacity, ZR_NULL);
}

TZrBool ZrCore_ExecIr_FunctionReserveOperandsEx(SZrExecIrFunction *function,
                                                TZrSize capacity,
                                                SZrExecIrDiagnostic *diagnostic) {
    if (function == ZR_NULL || function->sealed) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  function == ZR_NULL
                                      ? ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT
                                      : ZR_EXEC_IR_DIAGNOSTIC_SEALED,
                                  function,
                                  0u,
                                  0u,
                                  0u,
                                  0u);
        return ZR_FALSE;
    }
    return zr_exec_ir_reserve_with_diagnostic((void **)&function->operands,
                                              &function->operandCapacity,
                                              capacity,
                                              sizeof(*function->operands),
                                              function,
                                              diagnostic);
}

TZrBool ZrCore_ExecIr_FunctionReserveResults(SZrExecIrFunction *function,
                                               TZrSize capacity) {
    return ZrCore_ExecIr_FunctionReserveResultsEx(function, capacity, ZR_NULL);
}

TZrBool ZrCore_ExecIr_FunctionReserveResultsEx(SZrExecIrFunction *function,
                                               TZrSize capacity,
                                               SZrExecIrDiagnostic *diagnostic) {
    if (function == ZR_NULL || function->sealed) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  function == ZR_NULL
                                      ? ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT
                                      : ZR_EXEC_IR_DIAGNOSTIC_SEALED,
                                  function,
                                  0u,
                                  0u,
                                  0u,
                                  0u);
        return ZR_FALSE;
    }
    return zr_exec_ir_reserve_with_diagnostic((void **)&function->results,
                                              &function->resultCapacity,
                                              capacity,
                                              sizeof(*function->results),
                                              function,
                                              diagnostic);
}

TZrBool ZrCore_ExecIr_FunctionReserveBlocks(SZrExecIrFunction *function,
                                              TZrSize capacity) {
    return function != ZR_NULL && !function->sealed
               ? zr_exec_ir_reserve((void **)&function->blocks,
                                    &function->blockCapacity,
                                    capacity,
                                    sizeof(*function->blocks))
               : ZR_FALSE;
}

TZrBool ZrCore_ExecIr_FunctionReserveInstructions(SZrExecIrFunction *function,
                                                   TZrSize capacity) {
    return function != ZR_NULL && !function->sealed
               ? zr_exec_ir_reserve((void **)&function->instructions,
                                    &function->instructionCapacity,
                                    capacity,
                                    sizeof(*function->instructions))
               : ZR_FALSE;
}

TZrBool ZrCore_ExecIr_FunctionAppendMemoryTokens(
        SZrExecIrFunction *function,
        const TZrExecIrMemoryTokenId *tokens,
        TZrSize count,
        SZrExecIrRange *outRange) {
    if (function == ZR_NULL || function->sealed) {
        return ZR_FALSE;
    }
    return zr_exec_ir_append((void **)&function->memoryTokenPool,
                             &function->memoryTokenCount,
                             &function->memoryTokenCapacity,
                             tokens,
                             count,
                             sizeof(*function->memoryTokenPool),
                             outRange);
}

TZrBool ZrCore_ExecIr_FunctionAppendPhis(
        SZrExecIrFunction *function,
        const SZrExecIrPhi *phis,
        TZrSize count,
        SZrExecIrRange *outRange) {
    if (function == ZR_NULL || function->sealed) {
        return ZR_FALSE;
    }
    return zr_exec_ir_append((void **)&function->phiPool,
                             &function->phiCount,
                             &function->phiCapacity,
                             phis,
                             count,
                             sizeof(*function->phiPool),
                             outRange);
}

TZrBool ZrCore_ExecIr_FunctionAppendPredecessors(
        SZrExecIrFunction *function,
        const TZrExecIrBlockId *blocks,
        TZrSize count,
        SZrExecIrRange *outRange) {
    if (function == ZR_NULL || function->sealed) {
        return ZR_FALSE;
    }
    return zr_exec_ir_append((void **)&function->predecessors,
                             &function->predecessorCount,
                             &function->predecessorCapacity,
                             blocks,
                             count,
                             sizeof(*function->predecessors),
                             outRange);
}

TZrBool ZrCore_ExecIr_FunctionAppendSuccessors(
        SZrExecIrFunction *function,
        const TZrExecIrBlockId *blocks,
        TZrSize count,
        SZrExecIrRange *outRange) {
    if (function == ZR_NULL || function->sealed) {
        return ZR_FALSE;
    }
    return zr_exec_ir_append((void **)&function->successors,
                             &function->successorCount,
                             &function->successorCapacity,
                             blocks,
                             count,
                             sizeof(*function->successors),
                             outRange);
}

TZrBool ZrCore_ExecIr_FunctionReserveMemoryTokens(SZrExecIrFunction *function,
                                                   TZrSize capacity) {
    return function != ZR_NULL && !function->sealed
               ? zr_exec_ir_reserve((void **)&function->memoryTokenPool,
                                    &function->memoryTokenCapacity,
                                    capacity,
                                    sizeof(*function->memoryTokenPool))
               : ZR_FALSE;
}

TZrBool ZrCore_ExecIr_FunctionReservePhis(SZrExecIrFunction *function,
                                          TZrSize capacity) {
    return function != ZR_NULL && !function->sealed
               ? zr_exec_ir_reserve((void **)&function->phiPool,
                                    &function->phiCapacity,
                                    capacity,
                                    sizeof(*function->phiPool))
               : ZR_FALSE;
}

TZrBool ZrCore_ExecIr_FunctionReservePredecessors(SZrExecIrFunction *function,
                                                   TZrSize capacity) {
    return function != ZR_NULL && !function->sealed
               ? zr_exec_ir_reserve((void **)&function->predecessors,
                                    &function->predecessorCapacity,
                                    capacity,
                                    sizeof(*function->predecessors))
               : ZR_FALSE;
}

TZrBool ZrCore_ExecIr_FunctionReserveSuccessors(SZrExecIrFunction *function,
                                                 TZrSize capacity) {
    return function != ZR_NULL && !function->sealed
               ? zr_exec_ir_reserve((void **)&function->successors,
                                    &function->successorCapacity,
                                    capacity,
                                    sizeof(*function->successors))
               : ZR_FALSE;
}

TZrBool ZrCore_ExecIr_FunctionSeal(SZrExecIrFunction *function,
                                    SZrExecIrDiagnostic *diagnostic) {
    if (function == ZR_NULL) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  ZR_NULL,
                                  0u,
                                  0u,
                                  0u,
                                  0u);
        return ZR_FALSE;
    }
    if (!ZrCore_ExecIr_VerifyFunction(function,
                                      ZR_EXEC_IR_VERIFY_STRUCTURE,
                                      diagnostic)) {
        return ZR_FALSE;
    }
    function->sealed = ZR_TRUE;
    return ZR_TRUE;
}

SZrExecIrBlock *ZrCore_ExecIr_FunctionBlockAt(SZrExecIrFunction *function,
                                                TZrExecIrBlockId id) {
    if (function == ZR_NULL || id == ZR_EXEC_IR_BLOCK_ID_INVALID || id > function->blockCount) {
        return ZR_NULL;
    }
    return &function->blocks[id - 1u];
}

const SZrExecIrBlock *ZrCore_ExecIr_FunctionBlockAtConst(const SZrExecIrFunction *function,
                                                          TZrExecIrBlockId id) {
    if (function == ZR_NULL || id == ZR_EXEC_IR_BLOCK_ID_INVALID || id > function->blockCount) {
        return ZR_NULL;
    }
    return &function->blocks[id - 1u];
}

static const SZrExecIrOpcodeInfo g_zr_exec_ir_opcode_info[ZR_EXEC_IR_OPCODE_COUNT] = {
    [ZR_EXEC_IR_OPCODE_INVALID] = {
        .opcode = ZR_EXEC_IR_OPCODE_INVALID,
        .resultArity = 0u,
        .operandArity = 0u,
        .minimumOperands = 0u,
        .memoryReads = 0u,
        .memoryWrites = 0u,
        .flags = 0u,
        .effects = 0u,
        .name = "INVALID"
    },
#define ZR_EXEC_IR_OP(opcode_name, result_arity, operand_arity, memory_reads, memory_writes, schema_flags) \
    [ZR_EXEC_IR_OPCODE_##opcode_name] = { \
        .opcode = ZR_EXEC_IR_OPCODE_##opcode_name, .resultArity = (result_arity), \
        .operandArity = (operand_arity), \
        .minimumOperands = ((operand_arity) == ZR_EXEC_IR_VARIADIC ? 0u : (operand_arity)), \
        .memoryReads = (memory_reads), .memoryWrites = (memory_writes), \
        .flags = (schema_flags), \
        .effects = (((memory_reads) != 0u) ? ZR_EXEC_IR_EFFECT_READ_MEMORY : 0u) | \
                   (((memory_writes) != 0u) ? ZR_EXEC_IR_EFFECT_WRITE_MEMORY : 0u) | \
                   (((schema_flags) & ZR_EXEC_IR_SCHEMA_FLAG_MAY_ALLOCATE) != 0u ? ZR_EXEC_IR_EFFECT_ALLOCATE : 0u) | \
                   (((schema_flags) & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) != 0u ? ZR_EXEC_IR_EFFECT_THROW : 0u) | \
                   (((schema_flags) & ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND) != 0u ? ZR_EXEC_IR_EFFECT_SUSPEND : 0u) | \
                   (((schema_flags) & ZR_EXEC_IR_SCHEMA_FLAG_MAY_DROP) != 0u ? ZR_EXEC_IR_EFFECT_DROP : 0u), \
        .name = #opcode_name },
#include "zr_vm_core/exec_ir_opcode.def"
#undef ZR_EXEC_IR_OP
};

const SZrExecIrOpcodeInfo *ZrCore_ExecIr_OpcodeInfo(EZrExecIrOpcode opcode) {
    if (opcode <= ZR_EXEC_IR_OPCODE_INVALID || opcode >= ZR_EXEC_IR_OPCODE_COUNT) {
        return ZR_NULL;
    }
    return &g_zr_exec_ir_opcode_info[opcode];
}

const TZrChar *ZrCore_ExecIr_OpcodeName(EZrExecIrOpcode opcode) {
    const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(opcode);
    return info != ZR_NULL ? info->name : "INVALID";
}

TZrBool ZrCore_ExecIr_CloneModule(const SZrExecIrModule *source,
                                  SZrExecIrModule *destination,
                                  SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrModule temporary;
    TZrUInt32 index;

    zr_exec_ir_clear_diagnostic(diagnostic);
    if (source == ZR_NULL || destination == ZR_NULL || source == destination ||
        source->functionCount > source->functionCapacity ||
        source->constantCount > source->constantCapacity ||
        source->layoutCount > source->layoutCapacity ||
        source->sourceMapCount > source->sourceMapCapacity ||
        (source->functionCount != 0u && source->functions == ZR_NULL)) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  ZR_NULL,
                                  0u,
                                  0u,
                                  0u,
                                  0u);
        return ZR_FALSE;
    }
    ZrCore_ExecIr_ModuleInit(&temporary);
    temporary.id = source->id;
    temporary.moduleToken = source->moduleToken;
    temporary.moduleHash = source->moduleHash;
    temporary.contract = source->contract;
    if (!zr_exec_ir_clone_array((void **)&temporary.constants,
                                &temporary.constantCapacity,
                                source->constants,
                                source->constantCount,
                                sizeof(*source->constants)) ||
        !zr_exec_ir_clone_array((void **)&temporary.layouts,
                                &temporary.layoutCapacity,
                                source->layouts,
                                source->layoutCount,
                                sizeof(*source->layouts)) ||
        !zr_exec_ir_clone_array((void **)&temporary.sourceMaps,
                                &temporary.sourceMapCapacity,
                                source->sourceMaps,
                                source->sourceMapCount,
                                sizeof(*source->sourceMaps))) {
        ZrCore_ExecIr_FreeModule(&temporary);
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                  ZR_NULL,
                                  0u,
                                  0u,
                                  0u,
                                  0u);
        return ZR_FALSE;
    }
    temporary.constantCount = source->constantCount;
    temporary.layoutCount = source->layoutCount;
    temporary.sourceMapCount = source->sourceMapCount;
    if (source->functionCount != 0u &&
        !zr_exec_ir_reserve((void **)&temporary.functions,
                            &temporary.functionCapacity,
                            source->functionCount,
                            sizeof(*source->functions))) {
        ZrCore_ExecIr_FreeModule(&temporary);
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                  ZR_NULL,
                                  0u,
                                  0u,
                                  source->functionCount,
                                  source->functionCapacity);
        return ZR_FALSE;
    }
    for (index = 0u; index < source->functionCount; ++index) {
        if (!zr_exec_ir_clone_function_into(&source->functions[index],
                                            &temporary.functions[index])) {
            ZrCore_ExecIr_FreeModule(&temporary);
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                      &source->functions[index],
                                      0u,
                                      0u,
                                      0u,
                                      0u);
            return ZR_FALSE;
        }
        temporary.functionCount++;
    }
    ZrCore_ExecIr_FreeModule(destination);
    *destination = temporary;
    return ZR_TRUE;
}
