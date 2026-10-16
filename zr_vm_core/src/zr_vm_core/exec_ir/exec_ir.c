#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"

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

static TZrBool zr_exec_ir_reserve_capacity(TZrUInt32 capacity,
                                           TZrSize requested,
                                           size_t elementSize,
                                           TZrSize *outCapacity) {
    TZrSize newCapacity;
    if (elementSize == 0u || !zr_exec_ir_size_is_valid(requested, elementSize)) {
        return ZR_FALSE;
    }
    newCapacity = requested <= (TZrSize)capacity ? (TZrSize)capacity :
                  (capacity == 0u ? ZR_EXEC_IR_INITIAL_CAPACITY : (TZrSize)capacity);
    while (newCapacity < requested) {
        if (newCapacity > (TZrSize)UINT32_MAX / 2u) {
            newCapacity = requested;
            break;
        }
        newCapacity *= 2u;
    }
    if (!zr_exec_ir_size_is_valid(newCapacity, elementSize)) return ZR_FALSE;
    *outCapacity = newCapacity;
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_reserve(void **storage,
                                  TZrUInt32 *capacity,
                                  TZrSize requested,
                                  size_t elementSize) {
    TZrSize newCapacity;
    void *newStorage;

    if (storage == ZR_NULL || capacity == ZR_NULL ||
        !zr_exec_ir_reserve_capacity(*capacity, requested, elementSize, &newCapacity)) {
        return ZR_FALSE;
    }
    if (requested <= (TZrSize)*capacity) {
        return ZR_TRUE;
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
    TZrSize newCapacity;
    zr_exec_ir_clear_diagnostic(diagnostic);
    if (storage == ZR_NULL || capacity == ZR_NULL ||
        !zr_exec_ir_reserve_capacity(*capacity, requested, elementSize, &newCapacity)) {
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
    free(function->deoptAggregates);
    free(function->deoptAggregateFields);
    free(function->sourceMaps);
    free(function->bindingRows);
    if (function->stateMap != ZR_NULL) {
        ZrCore_ExecIr_StateMapFree(function->stateMap);
        free(function->stateMap);
    }
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
                                       size_t elementSize,
                                       const SZrExecIrFunction *function,
                                       SZrExecIrDiagnostic *diagnostic) {
    if (count == 0u) {
        return ZR_TRUE;
    }
    if (source == ZR_NULL) {
        zr_exec_ir_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  function, 0u, 0u, count, 0u);
        return ZR_FALSE;
    }
    if (!zr_exec_ir_reserve_with_diagnostic(destination, destinationCapacity,
                                            count, elementSize, function, diagnostic)) {
        return ZR_FALSE;
    }
    memcpy(*destination, source, (size_t)count * elementSize);
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_clone_array_shape_is_valid(
        const void *storage, TZrUInt32 count, TZrUInt32 capacity, size_t elementSize,
        const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic) {
    if (count > capacity || (count != 0u && storage == ZR_NULL)) {
        zr_exec_ir_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  function, 0u, 0u, count, capacity);
        return ZR_FALSE;
    }
    if (!zr_exec_ir_size_is_valid(count, elementSize)) {
        zr_exec_ir_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                  function, 0u, 0u, (TZrUInt32)(SIZE_MAX / elementSize), count);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_clone_function_shape_is_valid(
        const SZrExecIrFunction *source, SZrExecIrDiagnostic *diagnostic) {
    if (source == ZR_NULL ||
        source->gcMapCount > source->gcMapCapacity ||
        (source->gcMapCount != 0u && source->gcMap == ZR_NULL)) {
        zr_exec_ir_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  source, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
#define ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(object, field, countField, capacityField) \
    do { \
        if (!zr_exec_ir_clone_array_shape_is_valid((object)->field, (object)->countField, \
                (object)->capacityField, sizeof(*(object)->field), source, diagnostic)) \
            return ZR_FALSE; \
    } while (0)
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, values, valueCount, valueCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, instructions, instructionCount, instructionCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, blocks, blockCount, blockCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, operands, operandCount, operandCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, results, resultCount, resultCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, memoryTokenPool, memoryTokenCount, memoryTokenCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, phiPool, phiCount, phiCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, phiIncoming, phiIncomingCount, phiIncomingCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, predecessors, predecessorCount, predecessorCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, successors, successorCount, successorCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, gcRoots, gcRootCount, gcRootCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, deoptStates, deoptStateCount, deoptStateCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, deoptValues, deoptValueCount, deoptValueCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, deoptAggregates, deoptAggregateCount, deoptAggregateCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, deoptAggregateFields, deoptAggregateFieldCount, deoptAggregateFieldCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, sourceMaps, sourceMapCount, sourceMapCapacity);
    ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source, bindingRows, bindingRowCount, bindingRowCapacity);
    if (source->gcMap != ZR_NULL) {
        ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source->gcMap, entries, entryCount, entryCapacity);
        ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source->gcMap, slotIndexPool, slotIndexCount, slotIndexCapacity);
        ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source->gcMap, inlineRefOffsetPool, inlineRefOffsetCount, inlineRefOffsetCapacity);
    }
    if (source->frameLayout != ZR_NULL) {
        ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source->frameLayout, slots, slotCount, slotCapacity);
    }
    if (source->stateMap != ZR_NULL) {
        ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source->stateMap, entries, entryCount, entryCapacity);
        ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source->stateMap, valuePool, valueCount, valueCapacity);
        ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source->stateMap, rootPool, rootCount, rootCapacity);
        ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(source->stateMap, ownerStatePool, ownerStateCount, ownerStateCapacity);
        if (!ZrCore_ExecIr_StateMapStorageValid(source->stateMap)) {
            zr_exec_ir_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                      source, 0u, 0u, 0u, 0u);
            return ZR_FALSE;
        }
    }
#undef ZR_EXEC_IR_VALIDATE_CLONE_ARRAY
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_clone_function_into(const SZrExecIrFunction *source,
                                              SZrExecIrFunction *destination,
                                              SZrExecIrDiagnostic *diagnostic) {
    if (source == ZR_NULL || destination == ZR_NULL ||
        !zr_exec_ir_clone_function_shape_is_valid(source, diagnostic) ||
        !ZrCore_ExecIr_FunctionValidateBindingRows(source, ZR_NULL)) {
        if (diagnostic != ZR_NULL && diagnostic->code == ZR_EXECUTION_DIAGNOSTIC_NONE)
            zr_exec_ir_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                      source, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    ZrCore_ExecIr_FunctionInit(destination);
    destination->id = source->id;
    destination->functionToken = source->functionToken;
    destination->signatureHash = source->signatureHash;
    destination->contract = source->contract;
    destination->entryBlockId = source->entryBlockId;
    destination->bindingRowsSchemaVersion = source->bindingRowsSchemaVersion;

#define ZR_EXEC_IR_CLONE_FIELD(field, countField, capacityField) \
    do { \
        if (!zr_exec_ir_clone_array((void **)&destination->field, \
                                     &destination->capacityField, \
                                     source->field, \
                                     source->countField, \
                                     sizeof(*source->field), source, diagnostic)) { \
            return ZR_FALSE; \
        } \
        destination->countField = source->countField; \
    } while (0)

    ZR_EXEC_IR_CLONE_FIELD(values, valueCount, valueCapacity);
    ZR_EXEC_IR_CLONE_FIELD(instructions, instructionCount, instructionCapacity);
    ZR_EXEC_IR_CLONE_FIELD(blocks, blockCount, blockCapacity);
    ZR_EXEC_IR_CLONE_FIELD(operands, operandCount, operandCapacity);
    ZR_EXEC_IR_CLONE_FIELD(results, resultCount, resultCapacity);
    ZR_EXEC_IR_CLONE_FIELD(memoryTokenPool, memoryTokenCount, memoryTokenCapacity);
    ZR_EXEC_IR_CLONE_FIELD(phiPool, phiCount, phiCapacity);
    ZR_EXEC_IR_CLONE_FIELD(phiIncoming, phiIncomingCount, phiIncomingCapacity);
    ZR_EXEC_IR_CLONE_FIELD(predecessors, predecessorCount, predecessorCapacity);
    ZR_EXEC_IR_CLONE_FIELD(successors, successorCount, successorCapacity);
    destination->gcMapCount = source->gcMapCount;
    destination->gcMapCapacity = source->gcMapCapacity;
    if (source->gcMap != ZR_NULL) {
        destination->gcMap = (SZrExecIrGcMap *)calloc(1u, sizeof(*destination->gcMap));
        if (destination->gcMap == ZR_NULL) {
            zr_exec_ir_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                      source, 0u, 0u, 1u, 0u);
            return ZR_FALSE;
        }
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
                                     sizeof(*source->gcMap->entries), source, diagnostic) ||
            !zr_exec_ir_clone_array((void **)&destination->gcMap->slotIndexPool,
                                     &destination->gcMap->slotIndexCapacity,
                                     source->gcMap->slotIndexPool,
                                     source->gcMap->slotIndexCount,
                                     sizeof(*source->gcMap->slotIndexPool), source, diagnostic) ||
            !zr_exec_ir_clone_array((void **)&destination->gcMap->inlineRefOffsetPool,
                                     &destination->gcMap->inlineRefOffsetCapacity,
                                     source->gcMap->inlineRefOffsetPool,
                                     source->gcMap->inlineRefOffsetCount,
                                     sizeof(*source->gcMap->inlineRefOffsetPool), source, diagnostic)) return ZR_FALSE;
    }
    ZR_EXEC_IR_CLONE_FIELD(gcRoots, gcRootCount, gcRootCapacity);
    ZR_EXEC_IR_CLONE_FIELD(deoptStates, deoptStateCount, deoptStateCapacity);
    ZR_EXEC_IR_CLONE_FIELD(deoptValues, deoptValueCount, deoptValueCapacity);
    ZR_EXEC_IR_CLONE_FIELD(deoptAggregates, deoptAggregateCount, deoptAggregateCapacity);
    ZR_EXEC_IR_CLONE_FIELD(deoptAggregateFields, deoptAggregateFieldCount, deoptAggregateFieldCapacity);
    ZR_EXEC_IR_CLONE_FIELD(sourceMaps, sourceMapCount, sourceMapCapacity);
#undef ZR_EXEC_IR_CLONE_FIELD
    if (source->bindingRowCapacity != 0u &&
        !zr_exec_ir_reserve_with_diagnostic((void **)&destination->bindingRows,
                            &destination->bindingRowCapacity,
                            source->bindingRowCapacity,
                            sizeof(*source->bindingRows), source, diagnostic)) return ZR_FALSE;
    if (source->bindingRowCount != 0u)
        memcpy(destination->bindingRows, source->bindingRows,
               (size_t)source->bindingRowCount * sizeof(*source->bindingRows));
    destination->bindingRowCount = source->bindingRowCount;
    if (source->stateMap != ZR_NULL) {
        destination->stateMap = (SZrExecIrStateMap *)calloc(1u, sizeof(*destination->stateMap));
        if (destination->stateMap == ZR_NULL) {
            zr_exec_ir_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                      source, 0u, 0u, 1u, 0u);
            return ZR_FALSE;
        }
        ZrCore_ExecIr_StateMapInit(destination->stateMap);
        if (!ZrCore_ExecIr_StateMapClone(source->stateMap, destination->stateMap)) {
            zr_exec_ir_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                      source, 0u, 0u, 0u, 0u);
            return ZR_FALSE;
        }
    }
    if (source->frameLayout != ZR_NULL) {
        destination->frameLayout = (SZrExecIrFrameLayout *)calloc(1u, sizeof(*destination->frameLayout));
        if (destination->frameLayout == ZR_NULL) {
            zr_exec_ir_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                      source, 0u, 0u, 1u, 0u);
            return ZR_FALSE;
        }
        *destination->frameLayout = *source->frameLayout;
        destination->frameLayout->slots = ZR_NULL;
        destination->frameLayout->slotCapacity = 0u;
        if (!zr_exec_ir_clone_array((void **)&destination->frameLayout->slots,
                                     &destination->frameLayout->slotCapacity,
                                     source->frameLayout->slots,
                                     source->frameLayout->slotCount,
                                     sizeof(*source->frameLayout->slots), source, diagnostic)) return ZR_FALSE;
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
    if (!zr_exec_ir_clone_function_into(source, &temporary, diagnostic)) {
        ZrCore_ExecIr_FreeFunction(&temporary);
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

    if (function == ZR_NULL || function->sealed ||
        (TZrUInt32)ownership >= ZR_EXEC_IR_OWNERSHIP_COUNT ||
        (TZrUInt32)nullability >= ZR_EXEC_IR_NULLABILITY_COUNT ||
        function->valueCount == UINT32_MAX ||
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

TZrExecIrValueId ZrCore_ExecIr_FunctionAddExternalValue(
        SZrExecIrFunction *function,
        TZrMetadataToken typeToken,
        EZrExecIrOwnership ownership,
        EZrExecIrNullability nullability) {
    TZrExecIrValueId id = ZrCore_ExecIr_FunctionAddValue(
            function, typeToken, ownership, nullability);
    if (id != ZR_EXEC_IR_VALUE_ID_INVALID) {
        function->values[id - 1u].flags =
                ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    }
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

TZrBool ZrCore_ExecIr_FunctionSetEffectPhi(
        SZrExecIrFunction *function,
        TZrExecIrBlockId blockId,
        TZrExecIrEffectTokenId result,
        const SZrExecIrPhiIncoming *incoming,
        TZrSize count) {
    SZrExecIrRange range;

    if (function == ZR_NULL || function->sealed || blockId == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        blockId > function->blockCount || (result == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) !=
                                             (count == 0u) ||
        (count != 0u && incoming == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (function->blocks[blockId - 1u].effectPhiResult !=
        ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) {
        return ZR_FALSE;
    }
    if (result == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) {
        return ZR_TRUE;
    }
    if (!ZrCore_ExecIr_FunctionAppendPhiIncoming(function, incoming, count, &range)) {
        return ZR_FALSE;
    }
    function->blocks[blockId - 1u].effectPhiResult = result;
    function->blocks[blockId - 1u].effectPhiIncomings = range;
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_FunctionSetMemoryPhi(
        SZrExecIrFunction *function,
        TZrExecIrBlockId blockId,
        EZrExecIrMemoryClass region,
        TZrExecIrMemoryTokenId result,
        const SZrExecIrPhiIncoming *incoming,
        TZrSize count) {
    SZrExecIrRange range;

    if (function == ZR_NULL || function->sealed || blockId == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        blockId > function->blockCount || (TZrUInt32)region >= ZR_EXEC_IR_MEMORY_CLASS_COUNT ||
        result == ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID ||
        !ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(result) ||
        ZR_EXEC_IR_MEMORY_TOKEN_REGION(result) != region || count == 0u || incoming == ZR_NULL) {
        return ZR_FALSE;
    }
    if (function->blocks[blockId - 1u].memoryPhiResults[region] !=
        ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) {
        return ZR_FALSE;
    }
    if (!ZrCore_ExecIr_FunctionAppendPhiIncoming(function, incoming, count, &range)) {
        return ZR_FALSE;
    }
    function->blocks[blockId - 1u].memoryPhiResults[region] = result;
    function->blocks[blockId - 1u].memoryPhiIncomings[region] = range;
    return ZR_TRUE;
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
            function->values[valueId - 1u].definition != ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
            (function->values[valueId - 1u].flags &
             ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u) {
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
    if (function == ZR_NULL || function->sealed ||
        id == ZR_EXEC_IR_BLOCK_ID_INVALID || id > function->blockCount) {
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
        .minimumOperands = (ZR_EXEC_IR_OPCODE_##opcode_name == \
                                    ZR_EXEC_IR_OPCODE_RETURN \
                                ? 0u \
                                : ((operand_arity) == ZR_EXEC_IR_VARIADIC \
                                       ? 0u \
                                       : (operand_arity))), \
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
    if (source == ZR_NULL || destination == ZR_NULL || source == destination) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                  ZR_NULL,
                                  0u,
                                  0u,
                                  0u,
                                  0u);
        return ZR_FALSE;
    }
    if (!zr_exec_ir_clone_array_shape_is_valid(source->constants, source->constantCount,
                                               source->constantCapacity, sizeof(*source->constants),
                                               ZR_NULL, diagnostic) ||
        !zr_exec_ir_clone_array_shape_is_valid(source->layouts, source->layoutCount,
                                               source->layoutCapacity, sizeof(*source->layouts),
                                               ZR_NULL, diagnostic) ||
        !zr_exec_ir_clone_array_shape_is_valid(source->sourceMaps, source->sourceMapCount,
                                               source->sourceMapCapacity, sizeof(*source->sourceMaps),
                                               ZR_NULL, diagnostic) ||
        !zr_exec_ir_clone_array_shape_is_valid(source->functions, source->functionCount,
                                               source->functionCapacity, sizeof(*source->functions),
                                               ZR_NULL, diagnostic)) {
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
                                sizeof(*source->constants), ZR_NULL, diagnostic) ||
        !zr_exec_ir_clone_array((void **)&temporary.layouts,
                                &temporary.layoutCapacity,
                                source->layouts,
                                source->layoutCount,
                                sizeof(*source->layouts), ZR_NULL, diagnostic) ||
        !zr_exec_ir_clone_array((void **)&temporary.sourceMaps,
                                &temporary.sourceMapCapacity,
                                source->sourceMaps,
                                source->sourceMapCount,
                                sizeof(*source->sourceMaps), ZR_NULL, diagnostic)) {
        ZrCore_ExecIr_FreeModule(&temporary);
        return ZR_FALSE;
    }
    temporary.constantCount = source->constantCount;
    temporary.layoutCount = source->layoutCount;
    temporary.sourceMapCount = source->sourceMapCount;
    if (source->functionCount != 0u &&
        !zr_exec_ir_reserve_with_diagnostic((void **)&temporary.functions,
                            &temporary.functionCapacity,
                            source->functionCount,
                            sizeof(*source->functions), ZR_NULL, diagnostic)) {
        ZrCore_ExecIr_FreeModule(&temporary);
        return ZR_FALSE;
    }
    for (index = 0u; index < source->functionCount; ++index) {
        /* Include the in-progress function in rollback: cloning can fail after
         * one of its owned side arrays has already been allocated. */
        ZrCore_ExecIr_FunctionInit(&temporary.functions[index]);
        temporary.functionCount++;
        if (!zr_exec_ir_clone_function_into(&source->functions[index],
                                            &temporary.functions[index], diagnostic)) {
            ZrCore_ExecIr_FreeModule(&temporary);
            return ZR_FALSE;
        }
    }
    ZrCore_ExecIr_FreeModule(destination);
    *destination = temporary;
    return ZR_TRUE;
}

/**
 * @def ZR_EXEC_IR_INITIAL_CAPACITY
 *
 * ## Summary
 * 定义空池首次真正增长时使用的4元素起始容量，供reserve_capacity在请求量为正且旧容量为0时采用。
 *
 * ## Value and domain
 * 常量表达式((TZrUInt32)4u)；表示元素个数，不是字节数、固定最终容量或每次必分配量。
 *
 * ## Constraints and lifecycle
 * requested=capacity=0时reserve_capacity直接得到0，不使用此起点；正请求超过4时仍继续倍增，接近UInt32边界时改用requested。最终容量还需通过元素大小乘法检查，分配可另失败。宏无动态状态、所有权或线程同步；初始4的选取不是逻辑count发布。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:8
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:39
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:44
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:47
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:49
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:56
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 */

/**
 * @fn static void zr_exec_ir_clear_diagnostic(SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 为带诊断的扩容和公开克隆入口建立全零诊断基线，使后续只写部分字段的助手不会继承旧错误上下文。
 *
 * ## Parameters
 * @param[out] diagnostic 可为空；非空时指向独立可写的完整诊断记录。
 *
 * ## Returns
 * 无C返回值。非空记录的全部字节被清零；空指针无操作。
 *
 * ## Constraints and lifecycle
 * 此助手只写诊断，不验证IR、不分配或保存借用。诊断槽不得覆盖本次操作随后仍要读取的源/目标记录、池或控制字段。并非所有拒绝入口都会先调用它：ReserveOperandsEx/ResultsEx的NULL或sealed分支、Seal的NULL分支直接用部分字段setter；不能因此承诺所有公开接口的每个失败都清空诊断。共享同一诊断槽的写入须由调用方串行化。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:10
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:12
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:16
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:91
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:475
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:807
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:835
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:985
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1074
 */

/**
 * @fn static void zr_exec_ir_set_diagnostic(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code, const SZrExecIrFunction *function, TZrUInt32 instructionId, TZrUInt32 blockId, TZrUInt32 expected, TZrUInt32 actual)
 *
 * ## Summary
 * 在失败分支填写错误代码、函数身份、指令/块定位及两个数量上下文；供上层区分参数、容量、分配与封存拒绝。
 *
 * ## Parameters
 * @param[out] diagnostic 可为空；非空时为与输入记录/池独立的可写诊断槽，仅列明字段被覆盖。
 * @param[in] code 调用方选定的错误码；本助手不判断其与失败原因是否匹配。
 * @param[in] function 可为空；非空时只读取functionToken，调用期间该字段须稳定可读。
 * @param[in] instructionId 要写入的指令定位；0表示调用方未提供特定指令。
 * @param[in] blockId 要写入的块定位；0表示调用方未提供特定块。
 * @param[in] expected 写入expectedVersion的上下文；本文件也用它表达容量或数量，名称不限定为版本。
 * @param[in] actual 写入actualVersion的上下文；按调用分支解释。
 *
 * ## Returns
 * 无C返回值；空diagnostic无操作，否则更新code/functionToken/instructionId/blockId/expectedVersion/actualVersion。
 *
 * ## Constraints and lifecycle
 * 不memset，也不写sourceId及其它未列字段；需全新诊断的入口必须先clear。function为空时写functionToken=0。不得让诊断写入覆盖仍待读取的function或池；不检查别名或地址来源，无资源转移、借用留存或同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:16
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:23
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:26
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:31
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:91
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:94
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:264
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:475
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:477
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:808
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:836
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:986
 */

/**
 * @fn static TZrBool zr_exec_ir_size_is_valid(TZrSize count, size_t elementSize)
 *
 * ## Summary
 * 在计算分配字节数前同时约束逻辑元素数为UInt32范围，并用除法检查size_t乘法是否可表示。
 *
 * ## Parameters
 * @param[in] count 拟处理的元素数，尚未窄化为TZrUInt32。
 * @param[in] elementSize 单元素字节数；为0时仅检查UInt32元素数限制。
 *
 * ## Returns
 * @return TRUE仅表示count<=UINT32_MAX且非零elementSize下count*elementSize不会溢出SIZE_MAX；否则FALSE。
 *
 * ## Constraints and lifecycle
 * 纯数量判断，不读取存储、不检查容量、不保证分配可成功或池可读。允许elementSize=0是此helper自身规则；reserve_capacity另行拒绝0。调用方在按sizeof(T)校验clone池时传非零元素大小。无所有权、借用或同步副作用。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:34
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:35
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:36
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:44
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:56
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:284
 */

/**
 * @fn static TZrBool zr_exec_ir_reserve_capacity(TZrUInt32 capacity, TZrSize requested, size_t elementSize, TZrSize *outCapacity)
 *
 * ## Summary
 * 在不分配内存的阶段为池计算增长容量：不足时从初始4或旧容量倍增，接近UInt32上界时改用请求量。
 *
 * ## Parameters
 * @param[in] capacity 当前UInt32容量值；本函数不确认它对应实际分配。
 * @param[in] requested 至少需要容纳的元素数，受UInt32和size_t字节上限约束。
 * @param[in] elementSize 非零元素大小；0直接拒绝。
 * @param[out] outCapacity 成功时写入计算容量的非空可写槽；函数不检查其空值，须由内部调用方提供。
 *
 * ## Returns
 * @return TRUE时发布可表示且不小于请求量的计算容量；FALSE时不写outCapacity，包括元素大小为0、请求或最终增长容量字节数不可表示。
 *
 * ## Constraints and lifecycle
 * requested已满足时仍检查最终旧capacity的字节可表示性；requested=capacity=0成功给0，不自动分配初始4。实际倍增容量可能大于requested，且其字节检查失败时不会退化为一个较小但恰好可分配的容量。只计算整数，不分配、不修改池或逻辑count，不证明已有存储有效；无共享状态。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:39
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:8
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:44
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:47
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:50
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:54
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:56
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:57
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:69
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:93
 */

/**
 * @fn static TZrBool zr_exec_ir_reserve(void **storage, TZrUInt32 *capacity, TZrSize requested, size_t elementSize)
 *
 * ## Summary
 * 为一个由调用方拥有的连续池确保至少requested容量；只有realloc成功才发布新指针和容量，保持逻辑count由上层管理。
 *
 * ## Parameters
 * @param[in,out] storage 指向池指针的可写槽；槽为空则FALSE。旧非空池须兼容C realloc，且不能与capacity槽或调用中仍需读取的外部存储重叠。
 * @param[in,out] capacity 指向当前UInt32容量的可写槽；槽为空则FALSE，值须与实际池容量相符。
 * @param[in] requested 所需元素容量，不是要发布的逻辑count。
 * @param[in] elementSize 非零元素字节数。
 *
 * ## Returns
 * @return TRUE表示容量已足够或realloc成功并发布指针/容量；FALSE表示参数槽、容量计算或分配失败，此helper不改旧storage与capacity。
 *
 * ## Constraints and lifecycle
 * 容量已足够即返回，不检查*storage是否非空或实际可读，也不初始化新区域。增长成功可使所有旧池/元素借用失效，即使上层之后另一步失败也不可继续用旧地址。失败realloc保留旧分配；只在有效独立元数据槽和兼容分配前提下成立。无收缩、count更新、generation/stateMap维护或内部锁；拥有池的修改/释放须独占。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:61
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:68
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:72
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:79
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:80
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:133
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:767
 */

/**
 * @fn static TZrBool zr_exec_ir_reserve_with_diagnostic(void **storage, TZrUInt32 *capacity, TZrSize requested, size_t elementSize, const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 给底层reserve增加清空诊断和容量/分配错误分类，供显式ReserveEx与克隆候选的池分配共用。
 *
 * ## Parameters
 * @param[in,out] storage 可空的池指针槽；空槽被归入CAPACITY_OVERFLOW；非空槽的旧池须满足底层reserve所有权前提。
 * @param[in,out] capacity 可空容量槽；空槽被拒绝，非空槽须稳定可读写且与storage独立。
 * @param[in] requested 拟保证的元素容量。
 * @param[in] elementSize 非零元素大小。
 * @param[in] function 可为空；错误时读取其functionToken用于定位，不转移所有权。
 * @param[out] diagnostic 可为空；非空为与function、storage/capacity槽及池独立的可写诊断记录，入口整体清零。
 *
 * ## Returns
 * @return TRUE时底层容量满足且诊断维持全零；FALSE时参数槽/计算拒绝报告CAPACITY_OVERFLOW，底层reserve失败报告OUT_OF_MEMORY。旧池指针/容量在FALSE时保持。
 *
 * ## Constraints and lifecycle
 * 先clear再校验/读取输入，诊断不能别名这些输入。CAPACITY_OVERFLOW还包括NULL元数据槽和elementSize=0，不只算术溢出；expected/actual按本分支数量规则写入，并非精确可分配上限。底层reserve会再次算容量，足够时不证明池形状；成功可realloc而使旧借用失效。没有逻辑count发布或失败统一资源回收职责，调用方保持池拥有权并串行修改。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:84
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:91
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:92
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:94
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:99
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:105
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:106
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:111
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:268
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:428
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:819
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:847
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1126
 */

/**
 * @fn static TZrBool zr_exec_ir_append(void **storage, TZrUInt32 *count, TZrUInt32 *capacity, const void *items, TZrSize itemCount, size_t elementSize, SZrExecIrRange *outRange)
 *
 * ## Summary
 * 把一段元素复制到连续池的逻辑尾部；先验证UInt32合计和容量，复制完后统一发布count及可选的零基范围。
 *
 * ## Parameters
 * @param[in,out] storage 独立可写的池指针槽；非空旧池须可realloc并能容纳当前count。
 * @param[in,out] count 独立可写的逻辑元素数槽，入口须不大于真实capacity；本helper不验证count<=capacity。
 * @param[in,out] capacity 独立可写的实际分配容量槽。
 * @param[in] items itemCount非零时为足量可读源元素；0时可为空。不转移源存储所有权。
 * @param[in] itemCount 要复制的元素数；与旧count的和须可表示为UInt32。
 * @param[in] elementSize 与池和items一致的非零单元素大小。
 * @param[out] outRange 可为空；成功时写start=旧count、count=itemCount的零基范围。须稳定可写，且不覆盖池内容或storage/count/capacity元数据槽。
 *
 * ## Returns
 * @return TRUE表示尾部复制及count/outRange发布完成；FALSE来自空元数据槽、非零输入为空、UInt32总数或底层reserve失败，count/outRange保持且旧池指针/容量也未发布变化。
 *
 * ## Constraints and lifecycle
 * itemCount=0的追加仍调用reserve并发布空range；NULL items只在itemCount=0安全。使用memcpy而非memmove，items不得与最终写区重叠，也不得借用可能因同一reserve增长而失效的目标池；调用方须提供稳定独立输入。outRange可位于另一个不会被此池增长移动的描述符（如blocks中的范围字段），不能一概禁止与整个function有关联。复制不做元素语义/ID/范围验证，count增长不自动维护generation、sealed或stateMap；公共包装负责sealed门禁。单次正常失败在reserve之前或其FALSE处，成功后无可恢复失败分支；不涵盖非法地址造成的未定义行为。拥有池修改须独占。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:118
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:127
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:129
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:132
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:133
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:139
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:140
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:144
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:145
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:536
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:657
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:883
 */

/**
 * @fn static void zr_exec_ir_free_function_arrays(SZrExecIrFunction *function)
 *
 * ## Summary
 * 释放函数记录所拥有的全部直接池，以及frameLayout、单个gcMap和单个stateMap记录及其嵌套池，供公开FreeFunction统一清空记录前调用。
 *
 * ## Parameters
 * @param[in,out] function 可为空；非空须是已初始化、各非空拥有指针可独立按对应Free/C free释放的函数记录，也可为克隆中途构造的可释放候选。
 *
 * ## Returns
 * 无C返回值；空参数无操作，非空参数释放列出的拥有存储，但本helper不清零function字段。
 *
 * ## Constraints and lifecycle
 * 不释放function记录自身，不验证count/capacity、分配来源、池间别名或借用身份；不得把借来的池或与其他拥有者共享的池交给它。gcMap/gcMaps和stateMap/stateMaps是同一指针的别名，不按gcMapCount遍历多个map记录。返回后记录仍含悬空指针，不能直接再次调用；唯一当前调用方立即memset清空。所有旧池/嵌套记录/元素借用失效，function记录地址本身仍存在。调用期间不得并发读取、修改或释放这些存储。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:152
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:156
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:166
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:170
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:174
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:180
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:181
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:233
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:234
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:480
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:481
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:506
 */

/**
 * @fn void ZrCore_ExecIr_FrameLayoutInit(SZrExecIrFrameLayout *layout)
 *
 * ## Summary
 * 在首次构造或已释放后清零一个帧布局记录，建立空容器状态；不会申请内存，也不会替调用方释放旧池。
 *
 * ## Parameters
 * @param[out] layout 可空；非空须是完整可写的帧布局记录，调用前不再拥有需要释放的池。
 *
 * ## Returns
 * 无C返回值；NULL无操作。非空时整个布局记录归零，包括slots指针及其count/capacity；不分配或释放任何池。
 *
 * ## Constraints and lifecycle
 * Init只覆盖记录，不扫描或free已有指针；仍拥有资源时重复Init会丢失所有权，必须先释放旧拥有池。记录空间由调用方持有。现有布局标量也归零，不据此建立有效槽布局。 须独占写记录，不提供锁、线程间发布或同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:187
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:549
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:189
 */

/**
 * @fn void ZrCore_ExecIr_FrameLayoutFree(SZrExecIrFrameLayout *layout)
 *
 * ## Summary
 * 释放帧布局自身拥有的slots池并清零布局记录，供函数销毁、候选布局回滚与独立布局回收使用。
 *
 * ## Parameters
 * @param[in,out] layout 可空；非空记录及其slots必须可读写，非空slots须为兼容C free的独立有效分配。
 *
 * ## Returns
 * 无返回值；NULL无操作，否则释放slots后整个记录归零；不释放layout记录所在空间。
 *
 * ## Constraints and lifecycle
 * 不验证slotCount/slotCapacity、槽范围或分配来源，不能用它安全销毁任意伪造描述符。旧slots及元素借用失效，layout记录地址继续有效且为空；持有该记录的外层Function负责另行free记录。不得与使用者并发释放。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:193
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:550
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:195
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:167
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c:71
 * - tests/parser/test_ssa_primitive_source_frame_storage.c:31
 */

/**
 * @fn void ZrCore_ExecIr_GcMapInit(SZrExecIrGcMap *map)
 *
 * ## Summary
 * 在首次构造或已释放后清零一个GC映射记录，建立空容器状态；不会申请内存，也不会替调用方释放旧池。
 *
 * ## Parameters
 * @param[out] map 可空；非空须是完整可写的GC映射记录，调用前不再拥有需要释放的池。
 *
 * ## Returns
 * 无C返回值；NULL无操作。非空时整个GC映射记录归零，包括三池指针及其计数容量；不申请或释放池。
 *
 * ## Constraints and lifecycle
 * Init只覆盖记录，不扫描或free已有指针；仍拥有资源时重复Init会丢失所有权，必须先释放旧拥有池。记录空间由调用方持有。空记录没有可供运行时查找的GC映射，后续构造负责填写entries与索引范围。 须独占写记录，不提供锁、线程间发布或同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:200
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:551
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:202
 */

/**
 * @fn void ZrCore_ExecIr_GcMapFree(SZrExecIrGcMap *map)
 *
 * ## Summary
 * 释放单个GC映射记录拥有的entries、slotIndexPool和inlineRefOffsetPool三池并清零记录。
 *
 * ## Parameters
 * @param[in,out] map 可空；非空记录可读写，三个非空池都须为可各自free且互不共用所有权的有效分配。
 *
 * ## Returns
 * 无返回值；NULL无操作；返回后记录为空，但记录本身未被free。
 *
 * ## Constraints and lifecycle
 * 不按Function.gcMapCount遍历多个记录，也不释放gcRoots或运行时对象。三个旧池及元素借用失效；map记录自身继续存在。调用方须在所有读者结束后独占回收，不提供存储合法性或别名检查。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:206
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:552
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:208
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:210
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:171
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c:599
 */

/**
 * @fn void ZrCore_ExecIr_FunctionInit(SZrExecIrFunction *function)
 *
 * ## Summary
 * 在首次构造或已释放后清零一个函数记录，建立空容器状态；不会申请内存，也不会替调用方释放旧池。
 *
 * ## Parameters
 * @param[out] function 可空；非空须是完整可写的函数记录，调用前不再拥有需要释放的池。
 *
 * ## Returns
 * 无C返回值；NULL无操作。非空时先清零函数记录，再将entryBlockId设为INVALID；不分配或释放池。
 *
 * ## Constraints and lifecycle
 * Init只覆盖记录，不扫描或free已有指针；仍拥有资源时重复Init会丢失所有权，必须先释放旧拥有池。记录空间由调用方持有。contract、函数身份、各池计数容量和sealed都从零开始；ModuleAddFunction等后续构造负责赋予身份/版本。 须独占写记录，不提供锁、线程间发布或同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:215
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:548
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:217
 * - tests/parser/test_ssa_core_model.c:54
 */

/**
 * @fn void ZrCore_ExecIr_ModuleInit(SZrExecIrModule *module)
 *
 * ## Summary
 * 在首次构造或已释放后清零一个模块记录，建立空容器状态；不会申请内存，也不会替调用方释放旧池。
 *
 * ## Parameters
 * @param[out] module 可空；非空须是完整可写的模块记录，调用前不再拥有需要释放的池。
 *
 * ## Returns
 * 无C返回值；NULL无操作。非空时先清零模块记录，再将id设为MODULE_ID_INVALID；不分配或释放池。
 *
 * ## Constraints and lifecycle
 * Init只覆盖记录，不扫描或free已有指针；仍拥有资源时重复Init会丢失所有权，必须先释放旧拥有池。记录空间由调用方持有。moduleToken/moduleHash/contract和各池计数容量为空值，后续构造负责填充模块身份与内容。 须独占写记录，不提供锁、线程间发布或同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:222
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:547
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:224
 * - tests/parser/test_ssa_core_model.c:29
 * - tests/parser/test_ssa_core_model.c:30
 */

/**
 * @fn void ZrCore_ExecIr_FreeFunction(SZrExecIrFunction *function)
 *
 * ## Summary
 * 递归回收函数拥有的直接池、帧布局、单个GC映射和可选逻辑stateMap，再清零函数记录。
 *
 * ## Parameters
 * @param[in,out] function 可为空；非空记录可读写，各非空直接池、嵌套记录及其池须为兼容对应Free/C free的独立有效拥有分配；也可为本克隆过程建立的部分候选。
 *
 * ## Returns
 * 无返回值；NULL无操作，正常返回后function记录清零，不free该记录自身。
 *
 * ## Constraints and lifecycle
 * 先释放各直接池；frameLayout、gcMap、stateMap分别先调用对应Free再free嵌套记录。兼容复数拼写gcMaps/stateMaps仍与单数指针是同一个union成员，不是额外所有权。所有旧池、嵌套记录及其元素借用失效；外层函数记录地址仍有效且为空。不得对只读浅视图、与其他所有者共享池的浅副本或尚被读者使用的记录调用。无同步。 此实现按非空指针释放直接池而非按逻辑count遍历，不检查count/capacity形状；克隆失败留下的合法可释放部分候选也由此回收，不能把它当任意指针/别名认证。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:229
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:554
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:152
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:166
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:181
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:233
 * - tests/parser/test_ssa_core_model.c:85
 */

/**
 * @fn void ZrCore_ExecIr_FreeModule(SZrExecIrModule *module)
 *
 * ## Summary
 * 逐个销毁模块functions逻辑范围内的函数，再释放模块四个池并清零模块记录。
 *
 * ## Parameters
 * @param[in,out] module 可空；非空须有可读的functionCount个函数，以及独立可free的functions/constants/layouts/sourceMaps所有权。
 *
 * ## Returns
 * 无返回值；NULL无操作；正常返回后模块为空，module记录本身继续有效。
 *
 * ## Constraints and lifecycle
 * 按functionCount而非functionCapacity调用FreeFunction，故计数须真实且逻辑范围内的每项均已初始化。常量和布局是标量描述符，不把bits解释为需要free的运行时对象。释放后所有模块池、函数元素和嵌套资源借用失效。浅复制或共享拥有池不能由两个模块分别释放；无并发保护。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:553
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:243
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:246
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:250
 * - tests/parser/test_ssa_core_model.c:42
 */

/**
 * @fn static TZrBool zr_exec_ir_clone_array(void **destination, TZrUInt32 *destinationCapacity, const void *source, TZrUInt32 count, size_t elementSize, const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 把一个逻辑池的count个元素复制到候选目标池；按count请求容量，具体增长容量由通用reserve决定，逻辑count由调用方另行发布。
 *
 * ## Parameters
 * @param[in,out] destination 非零count时为候选池指针槽；须与源实际读区、容量槽和诊断独立，旧非空池兼容realloc。
 * @param[in,out] destinationCapacity 非零count时为候选池容量槽；须准确描述可重分配旧池。
 * @param[in] source count非零时为稳定且足量可读的独立源池；NULL被拒绝。count=0时不读取。
 * @param[in] count 要复制的UInt32逻辑元素数，而非源spare capacity。
 * @param[in] elementSize 非零count时须是正确非零元素大小；字节乘法由reserve检查。
 * @param[in] function 可为空；失败诊断读取其functionToken。
 * @param[out] diagnostic 可为空的独立诊断槽；非零有效源进入reserve时先清零，空源拒绝仅部分更新，count=0不写。
 *
 * ## Returns
 * @return count=0直接TRUE且不触碰目标/容量/源/诊断；非零时TRUE表示复制完成，FALSE表示源NULL或容量/分配失败，目标逻辑count未由本helper修改。
 *
 * ## Constraints and lifecycle
 * 空复制不把目标池清空、不校验destination/capacity槽或elementSize。非零复制的源/目标须满足memcpy不重叠及reserve不使源失效前提；不是通用自复制操作。新候选从空池开始，容量可能为4或更高倍增值，不保证等于count，也不保留源spare capacity。此helper不释放上层已经准备的其他池；失败由CloneFunction/CloneModule负责销毁完整候选。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:253
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:260
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:263
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:268
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:272
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:368
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:404
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:460
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1104
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1119
 */

/**
 * @fn static TZrBool zr_exec_ir_clone_array_shape_is_valid( const void *storage, TZrUInt32 count, TZrUInt32 capacity, size_t elementSize, const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在复制之前校验一个源逻辑池的count/capacity关系、非空逻辑池指针及复制字节可表示性，防止缺池或尺寸溢出进入分配/复制阶段。
 *
 * ## Parameters
 * @param[in] storage 只检查NULL关系，不读取池元素；count=0时允许为空或非空。
 * @param[in] count 逻辑元素数，必须不大于capacity。
 * @param[in] capacity 记录的容量，只参与count关系检查；不是此helper认证的真实分配长度。
 * @param[in] elementSize 当前调用均由sizeof(*pool)传入正确非零元素大小；0时size_is_valid略过字节检查，不能据成功认证任意真实元素尺寸。
 * @param[in] function 可为空；拒绝时读取functionToken用于诊断。
 * @param[out] diagnostic 可为空的独立可写诊断槽；失败仅setter更新字段，成功不清理旧内容。
 *
 * ## Returns
 * @return TRUE仅表示上述逻辑形状/字节检查通过；FALSE时count>capacity或非空逻辑池缺指针报INVALID_ARGUMENT，复制字节不可表示报CAPACITY_OVERFLOW。
 *
 * ## Constraints and lifecycle
 * 不分配或读取storage元素，不证明地址可读、容量字节实际存在、来源可释放、各池不重叠，也不验证spare capacity字节可表示；仅复制count需要的字节。当前sizeof参数非零，count是UInt32；在32位size_t上仍可能触发乘法界限。诊断不得覆盖输入function或元数据；无借用留存或同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:276
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:279
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:280
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:284
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:285
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:286
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:303
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1085
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1094
 */

/**
 * @fn static TZrBool zr_exec_ir_clone_function_shape_is_valid( const SZrExecIrFunction *source, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在任何函数候选池分配前检查源函数、各逻辑池及嵌套GC/帧/state-map存储形状，为逐池克隆提供有限的前置校验。
 *
 * ## Parameters
 * @param[in] source 可为空；非空记录及被读取的gcMap/frameLayout/stateMap元数据须稳定可读，实际逻辑池可读性仍是调用方责任。
 * @param[out] diagnostic 可为空；非空须独立可写，拒绝时部分写入，不在成功路径主动清空。
 *
 * ## Returns
 * @return TRUE表示本函数列出的形状检查及存在stateMap时的StateMapStorageValid通过；FALSE报告INVALID_ARGUMENT或CAPACITY_OVERFLOW。源和拥有池不被修改。
 *
 * ## Constraints and lifecycle
 * gcMapCount只检查<=gcMapCapacity及非零count必须有单个gcMap指针，不将其认证为多map数组。宏逐一校验17个函数池、GC三池、帧slots及stateMap四个逻辑池；StateMapStorageValid进一步检查其拥有容量区间等。count=0/capacity>0/NULL的普通池在此层可通过，不据此证明任意池完整性。没有完整CFG/SSA/效果、绑定表或恢复语义验证；绑定表随后由clone_function_into单独验证。诊断不得别名输入；不分配、不持有借用、无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:292
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:294
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:301
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:307
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:323
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:324
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:329
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:332
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:337
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:351
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:352
 */

/**
 * @def ZR_EXEC_IR_VALIDATE_CLONE_ARRAY(object, field, countField, capacityField)
 *
 * ## Summary
 * 在函数克隆形状前检中，把指定对象的一组池指针/count/capacity成员送入统一检查；任何FALSE直接退出外层形状函数。
 *
 * ## Parameters
 * @param[in] object 带所列成员的对象表达式；须稳定可读且无副作用，可能被重复求值。
 * @param[in] field 池指针成员名token；sizeof(*field)提供非零元素类型大小。
 * @param[in] countField 逻辑count成员名token。
 * @param[in] capacityField 记录capacity成员名token。
 *
 * ## Returns
 * 无独立C返回值；检查通过后继续外层函数，失败用return ZR_FALSE退出外层函数。
 *
 * ## Constraints and lifecycle
 * 仅供clone_function_shape_is_valid的词法上下文使用，隐式引用source和diagnostic局部名字。do/while(0)构成单语句，但不是可当布尔表达式调用的函数；参数表达式不应有副作用。底层只检逻辑形状与复制字节范围，不证明池可读、分配来源或任意语义。定义在源函数中但预处理作用域仍是TU文本，C343明确#undef终止，不外泄到后续函数；文本include实例重复编译同一物理定义，不新增实体信用。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:276
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:301
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:303
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:307
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:323
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:325
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:330
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:333
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:343
 */

/**
 * @fn static TZrBool zr_exec_ir_clone_function_into(const SZrExecIrFunction *source, SZrExecIrFunction *destination, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 向由上层管理的空候选函数复制源拥有存储；先完成形状与绑定表前检，再分阶段建立可由FreeFunction回收的候选，供公开克隆入口在整体成功后发布。
 *
 * ## Parameters
 * @param[in] source 可为空并被拒绝；非空时须在调用期间稳定可读，实际被读逻辑池须足量可读；前检只检查有限形状与绑定行。
 * @param[in,out] destination 可为空并被拒绝；非空时须与源记录/池独立，且为空或已Free候选。前检通过后FunctionInit覆盖它，禁止带未释放旧拥有资源调用。
 * @param[in,out] diagnostic 可为空；否则为与源/候选/池独立的已初始化诊断槽，前检/参数拒绝可能读取其code以保留已报错误。当前两个上层公开入口均先整体clear，内部reserve可再clear。
 *
 * ## Returns
 * @return TRUE表示所有池、嵌套记录和sealed元数据复制完成，候选归上层；FALSE可发生在前检或任一分配阶段，前检前目标未动，开始构造后可保留部分资源，调用方必须FreeFunction候选。
 *
 * ## Constraints and lifecycle
 * 本helper本身不事务替换已发布目标、不自动销毁部分候选。逻辑池复制count，实际容量按增长策略计算；bindingRows额外以源capacity请求，至少预留该容量但可向上取整，并只复制count行。gcMap和frameLayout先复制记录再摘除源指针、各自克隆嵌套池；stateMap另calloc并委托StateMapClone，其FALSE按此路径报告OUT_OF_MEMORY。复制contract、身份、source-map、deopt配方等元数据和sealed，不递增generation或修复语义陈旧stateMap。source!=destination及拥有池独立、diagnostic独立、可读与可释放前提由上层保证；成功也不证明完整VerifyFunction成立。源不得并发修改/释放，候选写入与最终接管须独占。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:347
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:350
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:351
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:352
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:358
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:366
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:375
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:388
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:397
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:398
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:420
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:427
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:432
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:436
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:444
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:450
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:457
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:466
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:487
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:488
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1136
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1137
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1138
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1140
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:353
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:354
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:475
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1074
 */

/**
 * @def ZR_EXEC_IR_CLONE_FIELD(field, countField, capacityField)
 *
 * ## Summary
 * 在空候选函数中按源逻辑count复制一个直接池，成功后才写该池的逻辑count；失败立即把部分候选交回外层调用者。
 *
 * ## Parameters
 * @param[in] field source和destination共用的池指针成员名token。
 * @param[in] countField 源逻辑count及成功后目标count成员名token。
 * @param[in] capacityField 目标容量成员名token，由clone_array/reserve修改。
 *
 * ## Returns
 * 无独立C返回值；单池复制成功后继续，失败return ZR_FALSE退出clone_function_into，不自行释放之前已复制的池。
 *
 * ## Constraints and lifecycle
 * 依赖隐式source、destination、diagnostic局部变量和已Init的目标；只能在返回TZrBool且上层负责Free候选的构造上下文使用。参数是成员名而非可带副作用的运行期表达式。复制的是count个元素，增长得到的capacity不要求等于count或源capacity；本宏不复制spare元素。普通池16次展开由同一物理宏承担，不分别计为新增函数；C426#undef后不再有效。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:253
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:260
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:268
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:366
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:368
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:375
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:378
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:387
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:420
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:425
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:426
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:488
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1140
 */

/**
 * @fn TZrBool ZrCore_ExecIr_CloneFunction(const SZrExecIrFunction *source, SZrExecIrFunction *destination, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在临时函数中复制源函数的拥有存储；全部复制成功后才释放并替换目标，使调用方能够保留原目标应对复制失败。
 *
 * ## Parameters
 * @param[in] source 非空、调用期间稳定可读的源函数；逻辑数组形状、绑定表和stateMap存储须满足实现的前检，实际被读池须足量可读。
 * @param[in,out] destination 非空、与source不同且拥有存储不与source共享的目标记录；须已Init或持有可由FreeFunction释放的旧状态。
 * @param[out] diagnostic 可空；非空须是独立于源、目标及其所有池的可写诊断槽，入口先整体清零。
 *
 * ## Returns
 * @return TRUE表示临时副本已接管为destination；FALSE包括NULL/同址参数、源形状/绑定表无效或复制分配失败，目标旧记录和拥有池保持。
 *
 * ## Constraints and lifecycle
 * 不是完整VerifyFunction：复制形状合法不等于SSA、效果、CFG或恢复语义有效。复制各逻辑池，bindingRows另以源capacity请求至少该容量，实际容量仍可按增长策略向上取整；帧布局slots、GC三池和stateMap也各有独立副本。失败回收已准备的临时资源；成功释放旧目标拥有的池/嵌套记录，使这些旧存储及元素借用失效；destination记录地址继续有效并承载新副本，源不变；contract、sealed等元数据复制，不自动递增generation或修复旧stateMap。源可由多个只读者读取但期间不得修改/释放，目标写与释放须独占。入口清诊断要求明确非别名。 普通池只复制逻辑count并按通用增长策略确定目标capacity，不保证capacity=count或与源spare capacity相等。 内部clone_function_into的FALSE可留下部分候选；公开包装立即FreeFunction候选，旧destination直到所有候选准备成功才Free并赋值。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:470
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:558
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:351
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:378
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:427
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:437
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:475
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:488
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:491
 * - tests/parser/test_ssa_core_model.c:272
 * - tests/parser/test_ssa_core_model.c:274
 */

/**
 * @fn TZrBool ZrCore_ExecIr_ModuleAddFunction(SZrExecIrModule *module, TZrMetadataToken functionToken, TZrUInt64 signatureHash, TZrExecIrFunctionId *outId)
 *
 * ## Summary
 * 在模块functions池末尾追加一个空函数，赋予count+1的1基ID及初始执行契约，供解码器和Parser构造函数容器。
 *
 * ## Parameters
 * @param[in,out] module 非空、计数容量与拥有functions池一致的模块；须独占写。
 * @param[in] functionToken 非零元数据身份；入口仅要求非零，不在此证明跨模块唯一或真实定义存在。
 * @param[in] signatureHash 保存到函数和执行契约的签名摘要，入口不要求非零或重新计算。
 * @param[out] outId 可空；非空须是不会随functions扩容失效的独立可写ID槽，仅成功时写入。
 *
 * ## Returns
 * @return TRUE为新函数初始化并发布，outId若非空得到1基ID；FALSE为参数、count上限或扩容失败，逻辑functionCount和outId不发布。
 *
 * ## Constraints and lifecycle
 * 可能realloc整个functions池，成功后旧函数元素指针失效；其他嵌套池不因这一追加被克隆。新contract保存当前schema/ABI/logical版本、generation=1、目标token与签名；其他字段初始0，不自动继承完整module.contract，也不验证旧图。需在取得新借用后继续构造。无内部同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:496
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:573
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:503
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:510
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:516
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:522
 * - tests/parser/test_ssa_core_model.c:31
 * - tests/parser/test_ssa_core_model.c:36
 */

/**
 * @fn TZrBool ZrCore_ExecIr_ModuleAppendConstant(SZrExecIrModule *module, const SZrExecIrConstant *constant, TZrSize count, SZrExecIrRange *outRange)
 *
 * ## Summary
 * 将常量描述符数组按值追加到模块对应池，返回该池内的0基range；构造过程不校验描述符的语义。
 *
 * ## Parameters
 * @param[in,out] module 非空、目标池count/capacity与可realloc所有权一致的模块。
 * @param[in] constant count非零时为足量可读的常量描述符数组；必须在扩容期间保持有效，不能借用将被移动的同一目标池。
 * @param[in] count 追加元素数，可为0；逻辑总数须能用TZrUInt32表示，分配字节须可表示。
 * @param[out] outRange 可空；非空为稳定可写range槽，与目标池及其指针/count/capacity字段独立；仅成功写入。
 *
 * ## Returns
 * @return TRUE时复制元素、更新逻辑count，并在outRange给出旧count作为start和追加count；FALSE为参数、尺寸/容量或分配失败，旧逻辑内容及outRange保持。
 *
 * ## Constraints and lifecycle
 * 内部先reserve再memcpy，不是memmove；输入与写区不能重叠，输入也不能在reserve后悬空。count=0允许输入NULL。成功扩容可使该池的旧元素借用失效；指令的range索引仍须由调用方关联。不会验证typeToken/layoutHash或运行时值，不修改generation或修复已存在stateMap；独占模块写。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:529
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:577
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:127
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:133
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:140
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:145
 */

/**
 * @fn TZrBool ZrCore_ExecIr_ModuleAppendLayout(SZrExecIrModule *module, const SZrExecIrLayout *layout, TZrSize count, SZrExecIrRange *outRange)
 *
 * ## Summary
 * 将布局描述符数组按值追加到模块对应池，返回该池内的0基range；构造过程不校验描述符的语义。
 *
 * ## Parameters
 * @param[in,out] module 非空、目标池count/capacity与可realloc所有权一致的模块。
 * @param[in] layout count非零时为足量可读的布局描述符数组；必须在扩容期间保持有效，不能借用将被移动的同一目标池。
 * @param[in] count 追加元素数，可为0；逻辑总数须能用TZrUInt32表示，分配字节须可表示。
 * @param[out] outRange 可空；非空为稳定可写range槽，与目标池及其指针/count/capacity字段独立；仅成功写入。
 *
 * ## Returns
 * @return TRUE时复制元素、更新逻辑count，并在outRange给出旧count作为start和追加count；FALSE为参数、尺寸/容量或分配失败，旧逻辑内容及outRange保持。
 *
 * ## Constraints and lifecycle
 * 内部先reserve再memcpy，不是memmove；输入与写区不能重叠，输入也不能在reserve后悬空。count=0允许输入NULL。成功扩容可使该池的旧元素借用失效；指令的range索引仍须由调用方关联。不会验证typeToken/layoutHash或运行时值，不修改generation或修复已存在stateMap；独占模块写。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:545
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:582
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:127
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:133
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:140
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:145
 */

/**
 * @fn SZrExecIrFunction *ZrCore_ExecIr_ModuleFunctionAt(SZrExecIrModule *module, TZrExecIrFunctionId id)
 *
 * ## Summary
 * 将模块内1基函数ID转为可写元素借用；供构造器或只读消费者按逻辑位置访问。
 *
 * ## Parameters
 * @param[in] module 可空；非空时functions逻辑范围须真实可读写；本访问器不改变模块。 本调用只读取容器元数据；返回后的可写元素借用由调用方另行协调写入。
 * @param[in] id 1..functionCount的1基ID；0或越界返回NULL。
 *
 * ## Returns
 * @return 返回functions[id-1]借用；NULL表示module为空、ID为0或超过逻辑count。
 *
 * ## Constraints and lifecycle
 * 访问器只检查NULL与ID边界，不验证functions指针、count<=capacity、元素自身ID或完整函数合同。即便元素sealed，此模块级可写访问器仍不拒绝；sealed不是内存写保护。functions扩容、模块Clone成功替换或FreeModule使旧元素地址失效；返回值不得free，调用方负责读写期及并发协调。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:561
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:587
 * - tests/parser/test_ssa_core_model.c:36
 */

/**
 * @fn const SZrExecIrFunction *ZrCore_ExecIr_ModuleFunctionAtConst(const SZrExecIrModule *module, TZrExecIrFunctionId id)
 *
 * ## Summary
 * 将模块内1基函数ID转为只读元素借用；供构造器或只读消费者按逻辑位置访问。
 *
 * ## Parameters
 * @param[in] module 可空；非空时functions逻辑范围须真实可读。
 * @param[in] id 1..functionCount的1基ID；0或越界返回NULL。
 *
 * ## Returns
 * @return 返回functions[id-1]借用；NULL表示module为空、ID为0或超过逻辑count。
 *
 * ## Constraints and lifecycle
 * 访问器只检查NULL与ID边界，不验证functions指针、count<=capacity、元素自身ID或完整函数合同。const访问不提供同步。functions扩容、模块Clone成功替换或FreeModule使旧元素地址失效；返回值不得free，调用方负责读写期及并发协调。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:569
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:589
 * - tests/parser/test_ssa_core_model.c:36
 * - tests/parser/test_ssa_builder_cfg.c:324
 */

/**
 * @fn TZrExecIrBlockId ZrCore_ExecIr_FunctionAddBlock(SZrExecIrFunction *function, TZrUInt32 flags)
 *
 * ## Summary
 * 为未sealed函数追加清零的基本块，按count+1分配1基ID；ENTRY标志同时设置函数入口ID。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数，blocks拥有池及计数容量一致。
 * @param[in] flags 仅由ENTRY/COLD/CLEANUP/EXCEPTION位组合；不能含未知位，ENTRY在已有入口时被拒绝。
 *
 * ## Returns
 * @return 返回新块的非零ID；NULL/封存、标志非法、重复ENTRY、数量上限或扩容失败返回INVALID(0)，不追加逻辑块。
 *
 * ## Constraints and lifecycle
 * reserve成功后初始化并发布；可移动blocks池，旧块借用须重新获取。ENTRY宏数值1是首块常见ID，但此入口允许先加普通块再以ENTRY选择后续块；不保证入口始终等于1。不会构造指令范围、边、支配信息或终结指令，也不更新generation/stateMap；这些由后续构造与验证完成。独占函数写。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:577
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:592
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:582
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:595
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:603
 * - tests/parser/test_ssa_core_model.c:38
 * - tests/parser/test_ssa_core_model.c:39
 */

/**
 * @fn TZrExecIrValueId ZrCore_ExecIr_FunctionAddValue(SZrExecIrFunction *function, TZrMetadataToken typeToken, EZrExecIrOwnership ownership, EZrExecIrNullability nullability)
 *
 * ## Summary
 * 为未sealed函数追加尚未绑定定义指令的逻辑值，保存类型、所有权和空性分类。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；values池计数容量和可realloc所有权须一致。
 * @param[in] typeToken 按值保存的元数据token；本入口不查询类型或要求非零。
 * @param[in] ownership UNKNOWN..GC范围内的枚举值；COUNT及越界值被拒绝。
 * @param[in] nullability UNKNOWN/NONNULL/NULLABLE之一；COUNT及越界值被拒绝。
 *
 * ## Returns
 * @return 返回新值count+1的1基ID；参数/枚举/封存/数量上限或扩容失败返回INVALID(0)，不追加逻辑值。
 *
 * ## Constraints and lifecycle
 * 可能移动values池，使旧值元素借用失效；只添加标量元数据，不创建运行时值、根或引用计数。新值definition=0、flags=0，须由后续指令/PHI或显式入口语义完成定义，不能据追加成功声称SSA已合法。不自动递增generation或重建stateMap；独占函数写。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:608
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:594
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:615
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:625
 * - tests/parser/test_ssa_core_model.c:88
 */

/**
 * @fn TZrExecIrValueId ZrCore_ExecIr_FunctionAddExternalValue( SZrExecIrFunction *function, TZrMetadataToken typeToken, EZrExecIrOwnership ownership, EZrExecIrNullability nullability)
 *
 * ## Summary
 * 追加入口外部提供的SSA值，并设置EXTERNAL_ENTRY标志、保持definition=0。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；values池计数容量和可realloc所有权须一致。
 * @param[in] typeToken 按值保存的元数据token；本入口不查询类型或要求非零。
 * @param[in] ownership UNKNOWN..GC范围内的枚举值；COUNT及越界值被拒绝。
 * @param[in] nullability UNKNOWN/NONNULL/NULLABLE之一；COUNT及越界值被拒绝。
 *
 * ## Returns
 * @return 返回新值count+1的1基ID；参数/枚举/封存/数量上限或扩容失败返回INVALID(0)，不追加逻辑值。
 *
 * ## Constraints and lifecycle
 * 可能移动values池，使旧值元素借用失效；只添加标量元数据，不创建运行时值、根或引用计数。external标志表示值在函数入口已有定义，不能再作为AppendInstruction的结果；其有效使用仍须全SSA/入口环境验证。不自动递增generation或重建stateMap；独占函数写。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:636
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:599
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:641
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:644
 * - tests/parser/test_ssa_core_model.c:88
 * - tests/parser/test_ssa_value_validation.c:81
 * - tests/parser/test_ssa_value_validation.c:140
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionAppendOperands(SZrExecIrFunction *function, const TZrExecIrValueId *operands, TZrSize count, SZrExecIrRange *outRange)
 *
 * ## Summary
 * 向未sealed函数的operandPool追加ValueId操作数数组，为调用方返回该池中的0基range。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；operandPool/operandCount及对应capacity与真实可realloc存储一致。
 * @param[in] operands count非零时须为足量可读的ValueId操作数；不允许来自将被移动的同一目标池，且不能与memcpy写区重叠。
 * @param[in] count 追加元素数；0允许输入NULL；与当前count相加不得超过UINT32_MAX，字节数须可表示。
 * @param[out] outRange 可空；非空为稳定可写range槽，不能别名被扩容池或其指针/count/capacity；可以位于其他不移动描述符字段。
 *
 * ## Returns
 * @return TRUE时发布operandCount和outRange(start=原count,count=追加数)；FALSE为参数、封存、尺寸/容量或分配失败，旧逻辑内容/count及outRange保持。
 *
 * ## Constraints and lifecycle
 * 先reserve再memcpy，成功扩容可能使operandPool旧元素借用失效；不承诺保留capacity值或指针地址。本入口不验证ValueId存在、定义关系或使用顺序。 追加成功不证明整图可执行，不更新execution contract的generation或已有stateMap；函数写必须独占。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:650
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:604
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:127
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:133
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:140
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:145
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionAppendResults(SZrExecIrFunction *function, const TZrExecIrValueId *results, TZrSize count, SZrExecIrRange *outRange)
 *
 * ## Summary
 * 向未sealed函数的resultPool追加ValueId结果数组，为调用方返回该池中的0基range。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；resultPool/resultCount及对应capacity与真实可realloc存储一致。
 * @param[in] results count非零时须为足量可读的ValueId结果；不允许来自将被移动的同一目标池，且不能与memcpy写区重叠。
 * @param[in] count 追加元素数；0允许输入NULL；与当前count相加不得超过UINT32_MAX，字节数须可表示。
 * @param[out] outRange 可空；非空为稳定可写range槽，不能别名被扩容池或其指针/count/capacity；可以位于其他不移动描述符字段。
 *
 * ## Returns
 * @return TRUE时发布resultCount和outRange(start=原count,count=追加数)；FALSE为参数、封存、尺寸/容量或分配失败，旧逻辑内容/count及outRange保持。
 *
 * ## Constraints and lifecycle
 * 先reserve再memcpy，成功扩容可能使resultPool旧元素借用失效；不承诺保留capacity值或指针地址。本入口不验证ValueId存在、定义关系或使用顺序。 追加成功不证明整图可执行，不更新execution contract的generation或已有stateMap；函数写必须独占。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:666
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:609
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:127
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:133
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:140
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:145
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionAppendPhiIncoming(SZrExecIrFunction *function, const SZrExecIrPhiIncoming *incoming, TZrSize count, SZrExecIrRange *outRange)
 *
 * ## Summary
 * 向未sealed函数的phiIncoming追加PHI输入记录数组，为调用方返回该池中的0基range。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；phiIncoming/phiIncomingCount及对应capacity与真实可realloc存储一致。
 * @param[in] incoming count非零时须为足量可读的PHI输入记录；不允许来自将被移动的同一目标池，且不能与memcpy写区重叠。
 * @param[in] count 追加元素数；0允许输入NULL；与当前count相加不得超过UINT32_MAX，字节数须可表示。
 * @param[out] outRange 可空；非空为稳定可写range槽，不能别名被扩容池或其指针/count/capacity；可以位于其他不移动描述符字段。
 *
 * ## Returns
 * @return TRUE时发布phiIncomingCount和outRange(start=原count,count=追加数)；FALSE为参数、封存、尺寸/容量或分配失败，旧逻辑内容/count及outRange保持。
 *
 * ## Constraints and lifecycle
 * 先reserve再memcpy，成功扩容可能使phiIncoming旧元素借用失效；不承诺保留capacity值或指针地址。incoming.value解释取决于消费范围：普通PHI为ValueId，effectPhi为EffectTokenId，memoryPhi为MemoryTokenId，不能统一当值SSA。 追加成功不证明整图可执行，不更新execution contract的generation或已有stateMap；函数写必须独占。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:682
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:634
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:127
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:133
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:140
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:145
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionSetEffectPhi( SZrExecIrFunction *function, TZrExecIrBlockId blockId, TZrExecIrEffectTokenId result, const SZrExecIrPhiIncoming *incoming, TZrSize count)
 *
 * ## Summary
 * 在未sealed函数的指定块上一次性建立效果token合流；输入记录追加到共用phiIncoming池后才发布块的结果token及range。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数，blocks和phiIncoming存储须有效。
 * @param[in] blockId 1..blockCount的1基块ID。
 * @param[in] result 效果结果token；count>0时必须非零，0只允许与count=0配对为空操作。
 * @param[in] incoming count非零时为稳定可读输入；predecessor为块ID，value在此解释为效果token而非ValueId。
 * @param[in] count 输入边实例数；本入口不证明与块前驱顺序/重数匹配。
 *
 * ## Returns
 * @return TRUE为合流发布，或result=0且count=0的空操作；FALSE为参数、重复已有结果、池追加/分配失败，块合流字段不发布。
 *
 * ## Constraints and lifecycle
 * 同一块已有非零effectPhiResult即拒绝重设；incoming的可读性、扩容期稳定及memcpy不重叠由调用方保证。成功可能移动phiIncoming池；只复制记录，不完整验证token链或CFG。效果token是独立域，不能和普通值PHI混用；无代际更新或同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:698
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:639
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:706
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:712
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:716
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:719
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:722
 * - tests/parser/test_ssa_effects_verifier.c:85
 * - tests/parser/test_ssa_effects_verifier.c:309
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionSetMemoryPhi( SZrExecIrFunction *function, TZrExecIrBlockId blockId, EZrExecIrMemoryClass region, TZrExecIrMemoryTokenId result, const SZrExecIrPhiIncoming *incoming, TZrSize count)
 *
 * ## Summary
 * 为指定块和内存region建立一次内存token合流，向phiIncoming共用池追加输入并保存该region的结果token和range。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；blocks和phiIncoming拥有存储须有效。
 * @param[in] blockId 有效1基块ID。
 * @param[in] region STACK_FRAME..IO范围内的内存区域枚举，COUNT及越界值拒绝。
 * @param[in] result 非零tagged token，编码region必须等于传入region；入口不额外保证version非零。
 * @param[in] incoming count>0的稳定可读输入记录；value在此是MemoryTokenId。
 * @param[in] count 正的输入记录数，后续验证还须检查前驱顺序、token区域和版本。
 *
 * ## Returns
 * @return TRUE为追加及块region字段发布成功；FALSE为参数、同region已有合流或追加失败，原合流字段保持。
 *
 * ## Constraints and lifecycle
 * 不接受空合流，不覆盖已有非零memoryPhiResults[region]；发布前不修改目标块字段。成功追加可能使phiIncoming旧指针失效，输入不得借用该移动池或与目标memcpy区域重叠。结果version=0虽可经此入口存下，完整VerifyEffects会拒绝；不能把构造成功当完整token有效性。独占写，无generation/stateMap自动维护。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:727
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:645
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:736
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:743
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:747
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:750
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:154
 * - tests/parser/test_ssa_effects_verifier.c:209
 * - tests/parser/test_ssa_effects_verifier.c:312
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionAppendInstruction(SZrExecIrFunction *function, const SZrExecIrInstruction *instruction, TZrExecIrInstructionId *outId)
 *
 * ## Summary
 * 在未sealed函数末尾复制一条指令，并把其结果ValueId的definition发布为新1基指令ID；只执行追加所需的局部结果检查。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；instructions/results/values及count/capacity须与真实存储一致。
 * @param[in] instruction 非空、调用全程稳定的源描述符；不能借用本次reserve可能移动的instructions池；resultRange须指向现有resultPool。
 * @param[out] outId 可空；非空为独立稳定可写指令ID槽，仅在逻辑追加成功后写入。
 *
 * ## Returns
 * @return TRUE为指令、instructionCount、各结果definition及可选outId已发布；FALSE为参数/封存/range/分配或结果ID/已有定义/external结果非法，逻辑追加未发布。
 *
 * ## Constraints and lifecycle
 * 实现先reserve instructions，之后才逐结果拒绝无效ID、已有定义或EXTERNAL_ENTRY；因此FALSE仍可能改变instructionCapacity/池地址并使旧指令借用失效，不能承诺整记录事务。成功也不证明opcode、flags、operands、CFG、重复结果或全SSA有效，需后续验证。side-pool ranges按值复制，不复制其他池；不自动更新contract.generation或stateMap。独占写，输出不能破坏输入/拥有字段。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:755
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:652
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:762
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:767
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:773
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:777
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:786
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:791
 * - tests/parser/test_ssa_core_model.c:523
 * - tests/parser/test_ssa_value_validation.c:140
 * - tests/parser/test_ssa_value_validation.c:155
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionReserveOperands(SZrExecIrFunction *function, TZrSize capacity)
 *
 * ## Summary
 * 确保未sealed函数operandPool至少可容纳capacity个元素；供后续构造预留存储，不增加逻辑count或初始化元素。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；operandPool须为与operandCapacity/逻辑count一致的可realloc拥有存储。
 * @param[in] capacity 所需最小元素容量，不是追加数量；须能表示为TZrUInt32且元素字节数可表示。
 *
 * ## Returns
 * @return TRUE表示当前容量已够或扩容成功；FALSE表示NULL/封存、容量/字节数不可表示或realloc失败。失败不发布新池/容量，逻辑count始终不变。
 *
 * ## Constraints and lifecycle
 * 只增长不缩小；零请求允许空池。成功realloc可能移动operandPool并使旧池元素借用失效，需要重新取指针。调用方仍须保证真实存储及count/capacity一致；已足容量的快速返回不认证任意指针。本入口不输出诊断；Operands/Results薄封装分别调用Ex(NULL diagnostic)。 不更新generation、ranges或stateMap；写和释放须独占，无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:799
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:657
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:39
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:61
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionReserveOperandsEx(SZrExecIrFunction *function, TZrSize capacity, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 确保未sealed函数operandPool至少可容纳capacity个元素；供后续构造预留存储，不增加逻辑count或初始化元素。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；operandPool须为与operandCapacity/逻辑count一致的可realloc拥有存储。
 * @param[in] capacity 所需最小元素容量，不是追加数量；须能表示为TZrUInt32且元素字节数可表示。
 * @param[out] diagnostic 可空且不得别名function或其任一输入池；有效未sealed函数进入reserve助手时先清零，参数/封存早退仅写诊断助手实际字段。
 *
 * ## Returns
 * @return TRUE表示当前容量已够或扩容成功；FALSE表示NULL/封存、容量/字节数不可表示或realloc失败。失败不发布新池/容量，逻辑count始终不变。
 *
 * ## Constraints and lifecycle
 * 只增长不缩小；零请求允许空池。成功realloc可能移动operandPool并使旧池元素借用失效，需要重新取指针。调用方仍须保证真实存储及count/capacity一致；已足容量的快速返回不认证任意指针。有效函数路径清除诊断后报告容量溢出或OOM；NULL/封存早退的setter不整体memset，不能要求所有未写字段归零。 不更新generation、ranges或stateMap；写和释放须独占，无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:804
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:659
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:799
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:39
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:61
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:807
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:91
 * - tests/parser/test_ssa_core_model.c:537
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionReserveResults(SZrExecIrFunction *function, TZrSize capacity)
 *
 * ## Summary
 * 确保未sealed函数resultPool至少可容纳capacity个元素；供后续构造预留存储，不增加逻辑count或初始化元素。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；resultPool须为与resultCapacity/逻辑count一致的可realloc拥有存储。
 * @param[in] capacity 所需最小元素容量，不是追加数量；须能表示为TZrUInt32且元素字节数可表示。
 *
 * ## Returns
 * @return TRUE表示当前容量已够或扩容成功；FALSE表示NULL/封存、容量/字节数不可表示或realloc失败。失败不发布新池/容量，逻辑count始终不变。
 *
 * ## Constraints and lifecycle
 * 只增长不缩小；零请求允许空池。成功realloc可能移动resultPool并使旧池元素借用失效，需要重新取指针。调用方仍须保证真实存储及count/capacity一致；已足容量的快速返回不认证任意指针。本入口不输出诊断；Operands/Results薄封装分别调用Ex(NULL diagnostic)。 不更新generation、ranges或stateMap；写和释放须独占，无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:827
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:663
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:39
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:61
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionReserveResultsEx(SZrExecIrFunction *function, TZrSize capacity, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 确保未sealed函数resultPool至少可容纳capacity个元素；供后续构造预留存储，不增加逻辑count或初始化元素。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；resultPool须为与resultCapacity/逻辑count一致的可realloc拥有存储。
 * @param[in] capacity 所需最小元素容量，不是追加数量；须能表示为TZrUInt32且元素字节数可表示。
 * @param[out] diagnostic 可空且不得别名function或其任一输入池；有效未sealed函数进入reserve助手时先清零，参数/封存早退仅写诊断助手实际字段。
 *
 * ## Returns
 * @return TRUE表示当前容量已够或扩容成功；FALSE表示NULL/封存、容量/字节数不可表示或realloc失败。失败不发布新池/容量，逻辑count始终不变。
 *
 * ## Constraints and lifecycle
 * 只增长不缩小；零请求允许空池。成功realloc可能移动resultPool并使旧池元素借用失效，需要重新取指针。调用方仍须保证真实存储及count/capacity一致；已足容量的快速返回不认证任意指针。有效函数路径清除诊断后报告容量溢出或OOM；NULL/封存早退的setter不整体memset，不能要求所有未写字段归零。 不更新generation、ranges或stateMap；写和释放须独占，无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:832
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:665
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:827
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:39
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:61
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:835
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:91
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionReserveBlocks(SZrExecIrFunction *function, TZrSize capacity)
 *
 * ## Summary
 * 确保未sealed函数blocks至少可容纳capacity个元素；供后续构造预留存储，不增加逻辑count或初始化元素。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；blocks须为与blockCapacity/逻辑count一致的可realloc拥有存储。
 * @param[in] capacity 所需最小元素容量，不是追加数量；须能表示为TZrUInt32且元素字节数可表示。
 *
 * ## Returns
 * @return TRUE表示当前容量已够或扩容成功；FALSE表示NULL/封存、容量/字节数不可表示或realloc失败。失败不发布新池/容量，逻辑count始终不变。
 *
 * ## Constraints and lifecycle
 * 只增长不缩小；零请求允许空池。成功realloc可能移动blocks并使旧池元素借用失效，需要重新取指针。调用方仍须保证真实存储及count/capacity一致；已足容量的快速返回不认证任意指针。本入口不输出诊断。 不更新generation、ranges或stateMap；写和释放须独占，无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:855
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:669
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:39
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:61
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionReserveInstructions(SZrExecIrFunction *function, TZrSize capacity)
 *
 * ## Summary
 * 确保未sealed函数instructions至少可容纳capacity个元素；供后续构造预留存储，不增加逻辑count或初始化元素。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；instructions须为与instructionCapacity/逻辑count一致的可realloc拥有存储。
 * @param[in] capacity 所需最小元素容量，不是追加数量；须能表示为TZrUInt32且元素字节数可表示。
 *
 * ## Returns
 * @return TRUE表示当前容量已够或扩容成功；FALSE表示NULL/封存、容量/字节数不可表示或realloc失败。失败不发布新池/容量，逻辑count始终不变。
 *
 * ## Constraints and lifecycle
 * 只增长不缩小；零请求允许空池。成功realloc可能移动instructions并使旧池元素借用失效，需要重新取指针。调用方仍须保证真实存储及count/capacity一致；已足容量的快速返回不认证任意指针。本入口不输出诊断。 不更新generation、ranges或stateMap；写和释放须独占，无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:865
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:671
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:39
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:61
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionAppendMemoryTokens( SZrExecIrFunction *function, const TZrExecIrMemoryTokenId *tokens, TZrSize count, SZrExecIrRange *outRange)
 *
 * ## Summary
 * 向未sealed函数的memoryTokenPool追加内存token ID数组，为调用方返回该池中的0基range。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；memoryTokenPool/memoryTokenCount及对应capacity与真实可realloc存储一致。
 * @param[in] tokens count非零时须为足量可读的内存token ID；不允许来自将被移动的同一目标池，且不能与memcpy写区重叠。
 * @param[in] count 追加元素数；0允许输入NULL；与当前count相加不得超过UINT32_MAX，字节数须可表示。
 * @param[out] outRange 可空；非空为稳定可写range槽，不能别名被扩容池或其指针/count/capacity；可以位于其他不移动描述符字段。
 *
 * ## Returns
 * @return TRUE时发布memoryTokenCount和outRange(start=原count,count=追加数)；FALSE为参数、封存、尺寸/容量或分配失败，旧逻辑内容/count及outRange保持。
 *
 * ## Constraints and lifecycle
 * 先reserve再memcpy，成功扩容可能使memoryTokenPool旧元素借用失效；不承诺保留capacity值或指针地址。token编码和区域/版本规则由效果验证器检查，本入口只复制ID。 追加成功不证明整图可执行，不更新execution contract的generation或已有stateMap；函数写必须独占。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:875
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:614
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:127
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:133
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:140
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:145
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionAppendPhis( SZrExecIrFunction *function, const SZrExecIrPhi *phis, TZrSize count, SZrExecIrRange *outRange)
 *
 * ## Summary
 * 向未sealed函数的phiPool追加PHI描述符数组，为调用方返回该池中的0基range。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；phiPool/phiCount及对应capacity与真实可realloc存储一致。
 * @param[in] phis count非零时须为足量可读的PHI描述符；不允许来自将被移动的同一目标池，且不能与memcpy写区重叠。
 * @param[in] count 追加元素数；0允许输入NULL；与当前count相加不得超过UINT32_MAX，字节数须可表示。
 * @param[out] outRange 可空；非空为稳定可写range槽，不能别名被扩容池或其指针/count/capacity；可以位于其他不移动描述符字段。
 *
 * ## Returns
 * @return TRUE时发布phiCount和outRange(start=原count,count=追加数)；FALSE为参数、封存、尺寸/容量或分配失败，旧逻辑内容/count及outRange保持。
 *
 * ## Constraints and lifecycle
 * 先reserve再memcpy，成功扩容可能使phiPool旧元素借用失效；不承诺保留capacity值或指针地址。phis.incomings仍引用函数phiIncoming池；复制描述符不复制其指向的记录。 追加成功不证明整图可执行，不更新execution contract的generation或已有stateMap；函数写必须独占。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:892
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:619
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:127
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:133
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:140
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:145
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionAppendPredecessors( SZrExecIrFunction *function, const TZrExecIrBlockId *blocks, TZrSize count, SZrExecIrRange *outRange)
 *
 * ## Summary
 * 向未sealed函数的predecessors追加前驱BlockId边实例数组，为调用方返回该池中的0基range。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；predecessors/predecessorCount及对应capacity与真实可realloc存储一致。
 * @param[in] blocks count非零时须为足量可读的前驱BlockId边实例；不允许来自将被移动的同一目标池，且不能与memcpy写区重叠。
 * @param[in] count 追加元素数；0允许输入NULL；与当前count相加不得超过UINT32_MAX，字节数须可表示。
 * @param[out] outRange 可空；非空为稳定可写range槽，不能别名被扩容池或其指针/count/capacity；可以位于其他不移动描述符字段。
 *
 * ## Returns
 * @return TRUE时发布predecessorCount和outRange(start=原count,count=追加数)；FALSE为参数、封存、尺寸/容量或分配失败，旧逻辑内容/count及outRange保持。
 *
 * ## Constraints and lifecycle
 * 先reserve再memcpy，成功扩容可能使predecessors旧元素借用失效；不承诺保留capacity值或指针地址。边数组保留顺序和重复出现，平行边不是自动去重；互逆关系及块范围由上层构造。 追加成功不证明整图可执行，不更新execution contract的generation或已有stateMap；函数写必须独占。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:909
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:624
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:127
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:133
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:140
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:145
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionAppendSuccessors( SZrExecIrFunction *function, const TZrExecIrBlockId *blocks, TZrSize count, SZrExecIrRange *outRange)
 *
 * ## Summary
 * 向未sealed函数的successors追加后继BlockId边实例数组，为调用方返回该池中的0基range。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；successors/successorCount及对应capacity与真实可realloc存储一致。
 * @param[in] blocks count非零时须为足量可读的后继BlockId边实例；不允许来自将被移动的同一目标池，且不能与memcpy写区重叠。
 * @param[in] count 追加元素数；0允许输入NULL；与当前count相加不得超过UINT32_MAX，字节数须可表示。
 * @param[out] outRange 可空；非空为稳定可写range槽，不能别名被扩容池或其指针/count/capacity；可以位于其他不移动描述符字段。
 *
 * ## Returns
 * @return TRUE时发布successorCount和outRange(start=原count,count=追加数)；FALSE为参数、封存、尺寸/容量或分配失败，旧逻辑内容/count及outRange保持。
 *
 * ## Constraints and lifecycle
 * 先reserve再memcpy，成功扩容可能使successors旧元素借用失效；不承诺保留capacity值或指针地址。边数组保留顺序和重复出现，平行边不是自动去重；互逆关系及块范围由上层构造。 追加成功不证明整图可执行，不更新execution contract的generation或已有stateMap；函数写必须独占。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:926
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:629
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:127
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:133
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:140
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:145
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionReserveMemoryTokens(SZrExecIrFunction *function, TZrSize capacity)
 *
 * ## Summary
 * 确保未sealed函数memoryTokenPool至少可容纳capacity个元素；供后续构造预留存储，不增加逻辑count或初始化元素。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；memoryTokenPool须为与memoryTokenCapacity/逻辑count一致的可realloc拥有存储。
 * @param[in] capacity 所需最小元素容量，不是追加数量；须能表示为TZrUInt32且元素字节数可表示。
 *
 * ## Returns
 * @return TRUE表示当前容量已够或扩容成功；FALSE表示NULL/封存、容量/字节数不可表示或realloc失败。失败不发布新池/容量，逻辑count始终不变。
 *
 * ## Constraints and lifecycle
 * 只增长不缩小；零请求允许空池。成功realloc可能移动memoryTokenPool并使旧池元素借用失效，需要重新取指针。调用方仍须保证真实存储及count/capacity一致；已足容量的快速返回不认证任意指针。本入口不输出诊断。 不更新generation、ranges或stateMap；写和释放须独占，无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:943
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:673
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:39
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:61
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionReservePhis(SZrExecIrFunction *function, TZrSize capacity)
 *
 * ## Summary
 * 确保未sealed函数phiPool至少可容纳capacity个元素；供后续构造预留存储，不增加逻辑count或初始化元素。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；phiPool须为与phiCapacity/逻辑count一致的可realloc拥有存储。
 * @param[in] capacity 所需最小元素容量，不是追加数量；须能表示为TZrUInt32且元素字节数可表示。
 *
 * ## Returns
 * @return TRUE表示当前容量已够或扩容成功；FALSE表示NULL/封存、容量/字节数不可表示或realloc失败。失败不发布新池/容量，逻辑count始终不变。
 *
 * ## Constraints and lifecycle
 * 只增长不缩小；零请求允许空池。成功realloc可能移动phiPool并使旧池元素借用失效，需要重新取指针。调用方仍须保证真实存储及count/capacity一致；已足容量的快速返回不认证任意指针。本入口不输出诊断。 不更新generation、ranges或stateMap；写和释放须独占，无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:953
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:675
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:39
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:61
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionReservePredecessors(SZrExecIrFunction *function, TZrSize capacity)
 *
 * ## Summary
 * 确保未sealed函数predecessors至少可容纳capacity个元素；供后续构造预留存储，不增加逻辑count或初始化元素。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；predecessors须为与predecessorCapacity/逻辑count一致的可realloc拥有存储。
 * @param[in] capacity 所需最小元素容量，不是追加数量；须能表示为TZrUInt32且元素字节数可表示。
 *
 * ## Returns
 * @return TRUE表示当前容量已够或扩容成功；FALSE表示NULL/封存、容量/字节数不可表示或realloc失败。失败不发布新池/容量，逻辑count始终不变。
 *
 * ## Constraints and lifecycle
 * 只增长不缩小；零请求允许空池。成功realloc可能移动predecessors并使旧池元素借用失效，需要重新取指针。调用方仍须保证真实存储及count/capacity一致；已足容量的快速返回不认证任意指针。本入口不输出诊断。 不更新generation、ranges或stateMap；写和释放须独占，无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:963
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:677
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:39
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:61
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionReserveSuccessors(SZrExecIrFunction *function, TZrSize capacity)
 *
 * ## Summary
 * 确保未sealed函数successors至少可容纳capacity个元素；供后续构造预留存储，不增加逻辑count或初始化元素。
 *
 * ## Parameters
 * @param[in,out] function 非空未sealed函数；successors须为与successorCapacity/逻辑count一致的可realloc拥有存储。
 * @param[in] capacity 所需最小元素容量，不是追加数量；须能表示为TZrUInt32且元素字节数可表示。
 *
 * ## Returns
 * @return TRUE表示当前容量已够或扩容成功；FALSE表示NULL/封存、容量/字节数不可表示或realloc失败。失败不发布新池/容量，逻辑count始终不变。
 *
 * ## Constraints and lifecycle
 * 只增长不缩小；零请求允许空池。成功realloc可能移动successors并使旧池元素借用失效，需要重新取指针。调用方仍须保证真实存储及count/capacity一致；已足容量的快速返回不认证任意指针。本入口不输出诊断。 不更新generation、ranges或stateMap；写和释放须独占，无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:973
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:679
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:39
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:61
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionSeal(SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 以STRUCTURE级VerifyFunction检查函数，成功后置sealed，作为builder可写接口的发布边界。
 *
 * ## Parameters
 * @param[in,out] function 可空；NULL返回FALSE；非空须满足验证器可读存储前提，成功只改变sealed。
 * @param[out] diagnostic 可空且独立于function及所有池；非空函数路径由VerifyFunction先清零，NULL早退setter并非整体清零。
 *
 * ## Returns
 * @return TRUE表示结构验证及已有stateMap验证通过且sealed=TRUE；FALSE不置新sealed，也不会把先前TRUE自动改回FALSE。
 *
 * ## Constraints and lifecycle
 * 不单独启用SSA/EFFECT标志，不能称为完整VerifyModule；已有stateMap仍经VerifyFunction无条件校验。不会冻结C公开内存、转移池所有权或递增generation；直接字段写和ModuleFunctionAt仍可能修改数据，调用方须自行遵守发布后协议。成功后FunctionBlockAt拒绝可写借用而Const访问器可读；此前取得的指针不因置位本身被free，但不能据其继续破坏发布约束。无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:983
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:681
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:985
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:995
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1000
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:909
 * - tests/parser/test_ssa_core_model.c:76
 * - tests/parser/test_ssa_core_model.c:78
 * - tests/parser/test_ssa_core_model.c:80
 */

/**
 * @fn SZrExecIrBlock *ZrCore_ExecIr_FunctionBlockAt(SZrExecIrFunction *function, TZrExecIrBlockId id)
 *
 * ## Summary
 * 按1基块ID返回函数blocks中的可写借用，供只读状态图消费者或未发布builder访问。
 *
 * ## Parameters
 * @param[in] function 可空；非空时blocks逻辑范围须真实可读写；此访问器本身不修改记录。 本调用只读取容器元数据；返回后的可写元素借用由调用方另行协调写入。
 * @param[in] id 1..blockCount的1基块ID，0或越界返回NULL。
 *
 * ## Returns
 * @return NULL表示空函数、sealed或ID越界；否则返回blocks[id-1]。
 *
 * ## Constraints and lifecycle
 * 只做入口/ID/sealed检查，不验证池地址、count/capacity、元素id或CFG。结果不得free；blocks扩容、成功Clone替换或FreeFunction使旧借用失效。须在函数构造期独占写，封存后不能利用旧可写指针绕过发布协议。无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1004
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:684
 * - tests/parser/test_ssa_core_model.c:78
 */

/**
 * @fn const SZrExecIrBlock *ZrCore_ExecIr_FunctionBlockAtConst(const SZrExecIrFunction *function, TZrExecIrBlockId id)
 *
 * ## Summary
 * 按1基块ID返回函数blocks中的只读借用，供只读状态图消费者或未发布builder访问。
 *
 * ## Parameters
 * @param[in] function 可空；非空时blocks逻辑范围须真实可读。
 * @param[in] id 1..blockCount的1基块ID，0或越界返回NULL。
 *
 * ## Returns
 * @return NULL表示空函数或ID越界；有效时返回blocks[id-1]，sealed不影响Const访问。
 *
 * ## Constraints and lifecycle
 * 只做入口/ID检查，不验证池地址、count/capacity、元素id或CFG。结果不得free；blocks扩容、成功Clone替换或FreeFunction使旧借用失效。只读返回不能延长函数生命周期，读取期间不得并发修改/释放。无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1013
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:686
 * - tests/parser/test_ssa_core_model.c:80
 */

/**
 * @var static const SZrExecIrOpcodeInfo g_zr_exec_ir_opcode_info[ZR_EXEC_IR_OPCODE_COUNT]
 *
 * ## Summary
 * 持有按Core opcode枚举索引的进程期只读schema元数据，供OpcodeInfo及其验证/投影消费者共享读取。
 *
 * ## Fields and invariants
 * 数组长度ZR_EXEC_IR_OPCODE_COUNT；INVALID槽显式零arity/内存/效果且名称为INVALID，其余槽由同一个exec_ir_opcode.def注册块指定初始化。每项保存opcode、resultArity、operandArity、minimumOperands、memoryReads/Writes、flags、effects和静态name。RETURN的minimumOperands固定0；其他VARIADIC也为0；其余等于operandArity。
 *
 * ## Constraints and lifecycle
 * 静态const存储无需Init/Free，返回的表项/名称借用在进程内持续有效，允许多个只读访问者。数据不可写入或释放。OpcodeInfo主动拒绝INVALID/COUNT/越界值，因此不能靠INVALID槽存在推导公开查询会返回它。schema可知不等于后端支持或指令合法；实际arity、memory与语义检查在消费者完成。Xmacro和include/undef块另列其预处理职责，不按生成表项重复授物理单元信用。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1021
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1022
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1031
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1033
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1037
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1045
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1052
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1054
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1056
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1057
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1060
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1063
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1064
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:215
 */

/**
 * @def ZR_EXEC_IR_OP(opcode_name, result_arity, operand_arity, memory_reads, memory_writes, schema_flags)
 *
 * ## Summary
 * 把opcode schema的一行展开为按opcode枚举索引的只读信息表项，将arity、内存读写类别和schema标志转换成通用效果掩码与静态名称。
 *
 * ## Parameters
 * @param[in] opcode_name 用于拼接ZR_EXEC_IR_OPCODE_*和字符串化的schema opcode名称token。
 * @param[in] result_arity schema结果arity表达式，写入resultArity。
 * @param[in] operand_arity 操作数arity表达式；RETURN或VARIADIC的minimumOperands为0，其他固定arity原样用作最小值。
 * @param[in] memory_reads 内存读取类别掩码，原样保留，非零时派生READ_MEMORY效果位。
 * @param[in] memory_writes 内存写入类别掩码，原样保留，非零时派生WRITE_MEMORY效果位。
 * @param[in] schema_flags schema标志表达式；原样写flags，指定MAY_ALLOCATE/THROW/SUSPEND/DROP时派生对应效果。
 *
 * ## Returns
 * 无运行期C返回值；预处理展开生成带尾逗号的数组指定初始化项。
 *
 * ## Constraints and lifecycle
 * 只适用于g_zr_exec_ir_opcode_info初始化上下文与固定schema行，不是可执行运行期调用。部分表达式被重复出现，输入须是常量且无副作用；token拼接与字符串化要求名称token。此表表达Core通用schema，不代表Oracle、ExecBC、AOT或某条优化路径支持每个opcode，也不替代具体IR验证。与公开H中的同名Xmacro是不同物理定义/展开语境；C1052包含.def后C1053立刻#undef。生成字符串与const表均为静态存储，无调用方释放。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1021
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1033
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1037
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1043
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1045
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1050
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1051
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1052
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1053
 * - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def:11
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:198
 */

/**
 * @anchor zr_exec_ir_opcode_schema_table_registration
 *
 * ## Summary
 * 在opcode const表初始化内部文本包含唯一schema行集，并在展开后解除局部ZR_EXEC_IR_OP，闭合该表的生成入口。
 *
 * ## Constraints and lifecycle
 * 该块仅发生在预处理/编译阶段，不是运行期callback注册或初始化调用。.def每一行绑定C1033这份宏生成指定初始化项；同一.def在公开H等消费者使用不同宏构建别的实体。undef防止此映射影响后续代码；不分配动态资源、不承担池生命周期。生成条目归table而非另计35个函数，块只记生成入口合同。修改schema须同时核对枚举、info表及各后端能力/校验，不在本注释提案更改schema或行为。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1033
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1052
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1053
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1054
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1056
 * - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def:11
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:198
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:200
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:201
 */

/**
 * @fn const SZrExecIrOpcodeInfo *ZrCore_ExecIr_OpcodeInfo(EZrExecIrOpcode opcode)
 *
 * ## Summary
 * 从与opcode.def共同生成的静态表查询有效opcode的结构、内存域、schema flags和派生效果元数据。
 *
 * ## Parameters
 * @param[in] opcode 枚举操作码；INVALID、COUNT及域外值返回NULL。
 *
 * ## Returns
 * @return 返回进程静态只读元数据借用；无效opcode返回NULL，不分配、不修改诊断。
 *
 * ## Constraints and lifecycle
 * 表与字符串由实现静态持有，调用方不得修改或free。resultArity/minimumResults以及operandArity/maximumOperands各是union别名；VARIADIC仅为arity哨兵，RETURN最少操作数按当前实现为0。memoryReads/Writes是MemoryClass位集，flags是schema域，effects是另一个效果域，不能当instruction.flags互换。静态只读查询可并行，但不能据元数据已知推断任意后端都支持该opcode。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1056
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:688
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1021
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1037
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1045
 * - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def:16
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:551
 */

/**
 * @fn const TZrChar *ZrCore_ExecIr_OpcodeName(EZrExecIrOpcode opcode)
 *
 * ## Summary
 * 将opcode映射为元数据表中的稳定静态名称，用于诊断展示。
 *
 * ## Parameters
 * @param[in] opcode 待显示的枚举值，可为无效或域外值。
 *
 * ## Returns
 * @return 有效opcode返回其静态name；其他值返回静态字符串"INVALID"。结果不需要且不能free。
 *
 * ## Constraints and lifecycle
 * 只是OpcodeInfo薄查询，不验证函数、不分配、不更新诊断；名称的稳定性对应当前编译schema，不能作为跨版本数值ABI替代。返回只读进程生命周期字符串，可供并行只读使用。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1063
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:689
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1065
 * - tests/parser/ssa_source_execbc_vm_diagnostics.inc:387
 */

/**
 * @fn TZrBool ZrCore_ExecIr_CloneModule(const SZrExecIrModule *source, SZrExecIrModule *destination, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 以临时模块深复制模块标量池和每个函数，成功后一次替换目标模块；适用于拥有模块复制和pass-manager候选快照。
 *
 * ## Parameters
 * @param[in] source 非空、稳定可读的源模块；四池计数容量形状以及每个函数的可复制前提须成立。
 * @param[in,out] destination 非空且与source不同；须为空或可由FreeModule销毁，拥有存储与source不共享。
 * @param[out] diagnostic 可空的独立可写诊断；不得覆盖任一输入记录/池或目标拥有存储，入口先清零。
 *
 * ## Returns
 * @return TRUE为完整副本已发布；FALSE为参数、源形状或任意复制阶段失败，旧destination保持。
 *
 * ## Constraints and lifecycle
 * 临时functions项在克隆前Init并计入临时functionCount，使途中失败能统一FreeModule。复制仅保证实现所检查的形状/绑定及拥有池，不替代完整模块验证；源contract/generation按值保留。成功时旧目标拥有的池、嵌套函数/记录及元素借用失效；destination记录自身继续有效承载新副本，新副本由目标独立拥有；失败仅诊断和临时分配活动可见。源不得在读取期间变化，目标须独占写/释放，无锁。 普通池只复制逻辑count并按通用增长策略确定目标capacity，不保证capacity=count或与源spare capacity相等。 各候选函数在调用内部clone前先Init并计入temporary.functionCount，即使内部在前检或部分构造处失败，FreeModule也遍历到该可释放槽。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1068
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:555
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1074
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1085
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1125
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1136
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1140
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1144
 * - tests/parser/test_ssa_core_model.c:589
 * - tests/parser/test_ssa_core_model.c:614
 * - tests/parser/test_ssa_core_model.c:629
 * - tests/parser/test_ssa_core_model.c:825
 */
