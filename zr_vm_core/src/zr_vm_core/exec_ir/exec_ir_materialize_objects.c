#include "zr_vm_core/exec_ir_runtime.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "../gc/gc_domain_internal.h"
#include "../gc/gc_internal.h"
#include "../ownership_resource_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* FRAME_BYTE_OFFSET describes VM stack storage, never native heap arrays. */
enum { ZR_MATERIALIZE_ROOT_GROUPS = 7 };
#define ZR_MATERIALIZE_PAUSE_TIMEOUT_MILLISECONDS ((TZrUInt32)1000u)

typedef struct SZrObjectPreparation {
    const SZrExecIrObjectMaterializationRequest *request;
    SZrExecIrMaterializedState logical;
    SZrObject **objects;
    SZrHashSet *maps;
    SZrString **names;
    SZrRawObject **ignored;
    TZrUInt32 ignoredCount;
    TZrUInt32 *bindings;
    SZrAotGcRootFrame rootFrames[ZR_MATERIALIZE_ROOT_GROUPS];
    SZrAotGcRootMap rootMaps[ZR_MATERIALIZE_ROOT_GROUPS];
    SZrAotGcRootSlot *rootSlots;
    TZrUInt32 frameCount;
    TZrUInt32 slotCount;
    SZrTypeValue savedException;
    EZrThreadStatus savedExceptionStatus;
    EZrThreadStatus savedThreadStatus;
    TZrBool hadException;
    TZrUInt32 savedExecutionDepth;
    TZrUInt32 savedNativeDepth;
    EZrGcNativeSafepointMode savedNativeMode;
    TZrBool savedNativeEnteredFromInactive;
    TZrBool scopeEntered;
    TZrBool worldPaused;
    TZrBool mutationLocked;
    TZrBool ready;
    EZrExecutionDiagnosticCode error;
} SZrObjectPreparation;

static SZrGcDomainMutatorRecord *zr_objects_mutator_locked(SZrState *state) {
    TZrSize index;
    SZrGcDomain *domain = state->gcDomain;
    for (index = 0u; index < domain->mutatorLength; ++index)
        if (domain->mutators[index].state == state) return &domain->mutators[index];
    return ZR_NULL;
}

static TZrBool zr_objects_save_scopes(SZrObjectPreparation *prepared) {
    SZrState *state = prepared->request->state;
    SZrGcDomainMutatorRecord *record;
    TZrBool valid = ZR_FALSE;
    ZrCore_GcDomain_Lock(state->gcDomain);
    record = zr_objects_mutator_locked(state);
    if (record != ZR_NULL && record->nativeMode == ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE &&
        record->executionDepth != UINT32_MAX &&
        (record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING ||
         record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE)) {
        prepared->savedExecutionDepth = record->executionDepth;
        prepared->savedNativeDepth = record->nativeDepth;
        prepared->savedNativeMode = record->nativeMode;
        prepared->savedNativeEnteredFromInactive = record->nativeEnteredFromInactive;
        valid = ZR_TRUE;
    }
    ZrCore_GcDomain_Unlock(state->gcDomain);
    return valid;
}

static void zr_objects_restore_scopes(SZrObjectPreparation *prepared) {
    SZrState *state = prepared->request->state;
    SZrGcDomainMutatorRecord *record;
    if (!prepared->scopeEntered) return;
    ZrCore_GcDomain_Lock(state->gcDomain);
    record = zr_objects_mutator_locked(state);
    if (record != ZR_NULL) {
        record->executionDepth = prepared->savedExecutionDepth;
        record->nativeDepth = prepared->savedNativeDepth;
        record->nativeMode = prepared->savedNativeMode;
        record->nativeEnteredFromInactive = prepared->savedNativeEnteredFromInactive;
        record->status = record->executionDepth != 0u || record->nativeDepth != 0u
                             ? ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING
                             : ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE;
        /* A saved record's epoch/status may predate a collector handshake. */
        record->observedEpoch = state->gcDomain->safepointEpoch;
        ZrCore_GcDomain_Broadcast(state->gcDomain);
    }
    ZrCore_GcDomain_Unlock(state->gcDomain);
    prepared->scopeEntered = ZR_FALSE;
}

static void zr_objects_diagnostic(const SZrExecIrObjectMaterializationRequest *request,
                                  SZrExecIrDiagnostic *diagnostic,
                                  EZrExecutionDiagnosticCode code) {
    const SZrExecIrFunction *function;
    const SZrExecIrStateMap *map;
    const SZrExecIrStateMapEntry *entry;
    TZrUInt32 index;
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    if (request == ZR_NULL) return;
    diagnostic->sourceId = request->checkpoint.sourceId;
    function = request->checkpoint.function;
    if (function == ZR_NULL) return;
    diagnostic->functionToken = function->functionToken;
    map = request->checkpoint.map != ZR_NULL ? request->checkpoint.map : function->stateMap;
    entry = ZrCore_ExecIr_StateMapFind(map, request->checkpoint.sourceId,
                                      request->checkpoint.resumeId, request->checkpoint.phase);
    if (entry == ZR_NULL) return;
    diagnostic->instructionId = entry->instructionId;
    if (function->blockCount > function->blockCapacity || function->blocks == ZR_NULL) return;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (entry->instructionId > block->instructionRange.start &&
            entry->instructionId - block->instructionRange.start <= block->instructionRange.count) {
            diagnostic->blockId = block->id;
            break;
        }
    }
}

static TZrBool zr_objects_size(TZrUInt32 count, size_t size) {
    /* Root offsets are UInt32 even on a 64-bit host. */
    return (TZrBool)(size != 0u && count <= SIZE_MAX / size && count <= UINT32_MAX / size);
}

static TZrBool zr_objects_hash_size(TZrSize count) {
    return (TZrBool)(count <= (SIZE_MAX - sizeof(SZrHashPairPoolBlock)) / sizeof(SZrHashKeyValuePair) &&
                     count <= SIZE_MAX / (ZR_HASH_SET_MAX_LOAD_DENOMINATOR * sizeof(void *)));
}

static TZrBool zr_objects_overlap(const void *left, size_t leftSize,
                                  const void *right, size_t rightSize) {
    uintptr_t a = (uintptr_t)left, b = (uintptr_t)right;
    if (leftSize == 0u || rightSize == 0u) return ZR_FALSE;
    if (left == ZR_NULL || right == ZR_NULL ||
        a > UINTPTR_MAX - leftSize || b > UINTPTR_MAX - rightSize) return ZR_TRUE;
    return (TZrBool)(a < b + rightSize && b < a + leftSize);
}

static TZrBool zr_objects_target_disjoint(
        const SZrExecIrObjectMaterializationRequest *request, const void *input, size_t bytes) {
    return (TZrBool)(!zr_objects_overlap(request->target, sizeof(*request->target), input, bytes) &&
        !zr_objects_overlap(request->target->objects,
                            (size_t)request->target->capacity * sizeof(*request->target->objects),
                            input, bytes));
}

static TZrBool zr_objects_array_disjoint(const SZrExecIrObjectMaterializationRequest *request,
                                         const void *input, TZrUInt32 capacity, size_t elementSize) {
    if (input == ZR_NULL) return (TZrBool)(capacity == 0u);
    return (TZrBool)(zr_objects_size(capacity, elementSize) &&
                     zr_objects_target_disjoint(request, input, (size_t)capacity * elementSize));
}

static TZrBool zr_objects_map_disjoint(const SZrExecIrObjectMaterializationRequest *request,
                                       const SZrExecIrStateMap *map) {
    if (map == ZR_NULL) return ZR_TRUE;
    return (TZrBool)(zr_objects_target_disjoint(request, map, sizeof(*map)) &&
        zr_objects_array_disjoint(request, map->entries, map->entryCapacity, sizeof(*map->entries)) &&
        zr_objects_array_disjoint(request, map->valuePool, map->valueCapacity, sizeof(*map->valuePool)) &&
        zr_objects_array_disjoint(request, map->rootPool, map->rootCapacity, sizeof(*map->rootPool)) &&
        zr_objects_array_disjoint(request, map->ownerStatePool, map->ownerStateCapacity, sizeof(*map->ownerStatePool)));
}

static TZrBool zr_objects_ir_disjoint(const SZrExecIrObjectMaterializationRequest *request) {
    const SZrExecIrFunction *function = request->checkpoint.function;
    struct SZrMaterializeInputSpan { const void *data; TZrUInt32 capacity; size_t size; };
    const struct SZrMaterializeInputSpan spans[] = {
        {function->values, function->valueCapacity, sizeof(*function->values)},
        {function->instructions, function->instructionCapacity, sizeof(*function->instructions)},
        {function->blocks, function->blockCapacity, sizeof(*function->blocks)},
        {function->operandPool, function->operandCapacity, sizeof(*function->operandPool)},
        {function->resultPool, function->resultCapacity, sizeof(*function->resultPool)},
        {function->memoryTokenPool, function->memoryTokenCapacity, sizeof(*function->memoryTokenPool)},
        {function->phiPool, function->phiCapacity, sizeof(*function->phiPool)},
        {function->phiIncoming, function->phiIncomingCapacity, sizeof(*function->phiIncoming)},
        {function->predecessors, function->predecessorCapacity, sizeof(*function->predecessors)},
        {function->successors, function->successorCapacity, sizeof(*function->successors)},
        {function->gcRoots, function->gcRootCapacity, sizeof(*function->gcRoots)},
        {function->deoptStates, function->deoptStateCapacity, sizeof(*function->deoptStates)},
        {function->deoptValues, function->deoptValueCapacity, sizeof(*function->deoptValues)},
        {function->deoptAggregates, function->deoptAggregateCapacity, sizeof(*function->deoptAggregates)},
        {function->deoptAggregateFields, function->deoptAggregateFieldCapacity, sizeof(*function->deoptAggregateFields)},
        {function->sourceMaps, function->sourceMapCapacity, sizeof(*function->sourceMaps)}
    };
    size_t index;
    for (index = 0u; index < sizeof(spans) / sizeof(spans[0]); ++index)
        if (!zr_objects_array_disjoint(request, spans[index].data, spans[index].capacity, spans[index].size))
            return ZR_FALSE;
    if (function->frameLayout != ZR_NULL &&
        (!zr_objects_target_disjoint(request, function->frameLayout, sizeof(*function->frameLayout)) ||
         !zr_objects_array_disjoint(request, function->frameLayout->slots, function->frameLayout->slotCapacity,
                                    sizeof(*function->frameLayout->slots)))) return ZR_FALSE;
    if (function->gcMap != ZR_NULL &&
        (!zr_objects_target_disjoint(request, function->gcMap, sizeof(*function->gcMap)) ||
         !zr_objects_array_disjoint(request, function->gcMap->entries, function->gcMap->entryCapacity,
                                    sizeof(*function->gcMap->entries)) ||
         !zr_objects_array_disjoint(request, function->gcMap->slotIndexPool, function->gcMap->slotIndexCapacity,
                                    sizeof(*function->gcMap->slotIndexPool)) ||
         !zr_objects_array_disjoint(request, function->gcMap->inlineRefOffsetPool,
                                    function->gcMap->inlineRefOffsetCapacity,
                                    sizeof(*function->gcMap->inlineRefOffsetPool)))) return ZR_FALSE;
    return (TZrBool)(zr_objects_map_disjoint(request, request->checkpoint.map) &&
                     zr_objects_map_disjoint(request, function->stateMap));
}

static TZrBool zr_objects_request_valid(const SZrExecIrObjectMaterializationRequest *request) {
    TZrUInt32 index;
    if (request == ZR_NULL || request->state == ZR_NULL ||
        request->state->global == ZR_NULL || request->state->gcDomain == ZR_NULL ||
        request->state->global->garbageCollector == ZR_NULL ||
        request->checkpoint.function == ZR_NULL || request->checkpoint.target != ZR_NULL ||
        request->target == ZR_NULL || request->target->count > request->target->capacity ||
        (request->target->capacity != 0u && request->target->objects == ZR_NULL) ||
        (request->valueCount != 0u && request->values == ZR_NULL) ||
        (request->typeCount != 0u && request->types == ZR_NULL) ||
        !zr_objects_size(request->target->capacity, sizeof(*request->target->objects)) ||
        !zr_objects_size(request->valueCount, sizeof(*request->values)) ||
        !zr_objects_size(request->typeCount, sizeof(*request->types))) return ZR_FALSE;
    if (!zr_objects_target_disjoint(request, request, sizeof(*request)) ||
        !zr_objects_target_disjoint(request, request->values,
                                     (size_t)request->valueCount * sizeof(*request->values)) ||
        !zr_objects_target_disjoint(request, request->types,
                                     (size_t)request->typeCount * sizeof(*request->types)) ||
        !zr_objects_target_disjoint(request, request->checkpoint.function,
                                     sizeof(*request->checkpoint.function)) ||
        !zr_objects_target_disjoint(request, request->state, sizeof(*request->state)) ||
        !zr_objects_ir_disjoint(request) ||
        zr_objects_overlap(request->target, sizeof(*request->target), request->target->objects,
                           (size_t)request->target->capacity * sizeof(*request->target->objects)))
        return ZR_FALSE;
    for (index = 0u; index < request->typeCount; ++index) {
        const SZrExecIrRuntimeTypeBinding *type = &request->types[index];
        if ((type->fieldCount != 0u && type->fields == ZR_NULL) ||
            !zr_objects_size(type->fieldCount, sizeof(*type->fields)) ||
            !zr_objects_target_disjoint(request, type->fields,
                                         (size_t)type->fieldCount * sizeof(*type->fields))) return ZR_FALSE;
    }
    for (index = 0u; index < request->target->count; ++index) {
        SZrObject *object = request->target->objects[index];
        if (object != ZR_NULL &&
            (ZrCore_RawObject_IsUnreferenced(request->state, &object->super) ||
             object->super.type != ZR_RAW_OBJECT_TYPE_OBJECT || object->super.isGcBox ||
             !ZrCore_GcDomain_ObjectBelongsToState(request->state, &object->super))) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_objects_value_valid(SZrState *state, const SZrTypeValue *value) {
    if (!ZR_VALUE_IS_TYPE_NORMAL(value->type)) return ZR_FALSE;
    if (value->isGarbageCollectable) {
        const SZrRawObject *raw = value->value.object;
        return (TZrBool)(raw != ZR_NULL && !ZrCore_RawObject_IsUnreferenced(state, (SZrRawObject *)raw) &&
            (EZrValueType)raw->type == value->type &&
            raw->isNative == value->isNative && ZrCore_GcDomain_ObjectBelongsToState(state, raw) &&
            raw->resourceLifecycleState != ZR_RESOURCE_LIFECYCLE_DROPPING &&
            raw->resourceLifecycleState != ZR_RESOURCE_LIFECYCLE_DROPPED);
    }
    return (TZrBool)((!ZR_VALUE_IS_TYPE_ZR_ITEM(value->type) && value->type != ZR_VALUE_TYPE_STRING) ||
                     (value->type == ZR_VALUE_TYPE_FUNCTION && value->isNative));
}

static TZrBool zr_objects_plain_value(const SZrTypeValue *value) {
    return (TZrBool)(value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE &&
                     value->ownershipControl == ZR_NULL && value->ownershipWeakRef == ZR_NULL &&
                     (!value->isGarbageCollectable ||
                      (value->value.object->ownershipControl == ZR_NULL &&
                       !ZrCore_OwnershipResource_IsObject(value->value.object) &&
                       value->value.object->resourceLifecycleState == ZR_RESOURCE_LIFECYCLE_NONE)));
}

static TZrBool zr_objects_prototype_valid(SZrState *state, const SZrObjectPrototype *prototype) {
    const SZrObjectPrototype *slow = prototype, *fast = prototype;
    while (prototype != ZR_NULL) {
        if (ZrCore_RawObject_IsUnreferenced(state, (SZrRawObject *)prototype) ||
            prototype->super.super.type != ZR_RAW_OBJECT_TYPE_OBJECT ||
            prototype->super.super.isGcBox ||
            prototype->super.internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE ||
            !ZrCore_GcDomain_ObjectBelongsToState(state, &prototype->super.super) ||
            (prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS &&
             prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) ||
            (prototype->modifierFlags & ZR_TYPE_MODIFIER_FLAG_RESOURCE) != 0u ||
            prototype->managedFieldCount != 0u ||
            prototype->memberDescriptorCount > prototype->memberDescriptorCapacity ||
            (prototype->memberDescriptorCount != 0u && prototype->memberDescriptors == ZR_NULL))
            return ZR_FALSE;
        prototype = prototype->superPrototype;
        slow = slow != ZR_NULL ? slow->superPrototype : ZR_NULL;
        fast = fast != ZR_NULL ? fast->superPrototype : ZR_NULL;
        fast = fast != ZR_NULL ? fast->superPrototype : ZR_NULL;
        if (slow != ZR_NULL && slow == fast) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_objects_types_valid(const SZrExecIrObjectMaterializationRequest *request) {
    TZrUInt32 index, prior, fieldIndex, priorField;
    for (index = 0u; index < request->typeCount; ++index) {
        const SZrExecIrRuntimeTypeBinding *binding = &request->types[index];
        SZrObjectPrototype *prototype = binding->prototype;
        if (binding->typeToken == 0u || binding->layoutId == 0u || prototype == ZR_NULL ||
            !zr_objects_prototype_valid(request->state, prototype) ||
            binding->layoutGeneration != prototype->layoutGeneration) return ZR_FALSE;
        {
            const SZrObjectPrototype *ancestor = prototype;
            while (ancestor != ZR_NULL) {
                size_t prototypeSize = ancestor->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT
                                           ? sizeof(SZrStructPrototype) : sizeof(*ancestor);
                if (!zr_objects_target_disjoint(request, ancestor, prototypeSize) ||
                    !zr_objects_array_disjoint(request, ancestor->memberDescriptors,
                         ancestor->memberDescriptorCapacity, sizeof(*ancestor->memberDescriptors))) return ZR_FALSE;
                ancestor = ancestor->superPrototype;
            }
        }
        for (prior = 0u; prior < index; ++prior) {
            if (binding->typeToken == request->types[prior].typeToken &&
                binding->layoutId == request->types[prior].layoutId) return ZR_FALSE;
        }
        for (fieldIndex = 0u; fieldIndex < binding->fieldCount; ++fieldIndex) {
            const SZrExecIrRuntimeFieldBinding *field = &binding->fields[fieldIndex];
            const SZrMemberDescriptor *descriptor;
            if (field->memberDescriptorIndex >= prototype->memberDescriptorCount) return ZR_FALSE;
            descriptor = &prototype->memberDescriptors[field->memberDescriptorIndex];
            if (descriptor->kind != ZR_MEMBER_DESCRIPTOR_KIND_FIELD || descriptor->isStatic ||
                descriptor->name == ZR_NULL ||
                ZrCore_RawObject_IsUnreferenced(request->state, &descriptor->name->super) ||
                descriptor->name->super.type != ZR_RAW_OBJECT_TYPE_STRING ||
                !ZrCore_GcDomain_ObjectBelongsToState(request->state, &descriptor->name->super))
                return ZR_FALSE;
            for (priorField = 0u; priorField < fieldIndex; ++priorField) {
                const SZrExecIrRuntimeFieldBinding *other = &binding->fields[priorField];
                if (other->fieldIndex == field->fieldIndex ||
                    other->memberDescriptorIndex == field->memberDescriptorIndex ||
                    ZrCore_String_Equal(descriptor->name,
                        prototype->memberDescriptors[other->memberDescriptorIndex].name)) return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static TZrUInt32 zr_objects_binding_index(const SZrExecIrObjectMaterializationRequest *request,
                                          const SZrExecIrDeoptAggregate *aggregate) {
    TZrUInt32 index;
    for (index = 0u; index < request->typeCount; ++index) {
        if (request->types[index].typeToken == aggregate->typeToken &&
            request->types[index].layoutId == aggregate->layoutId) return index;
    }
    return request->typeCount;
}

static const SZrMemberDescriptor *zr_objects_descriptor(
        const SZrExecIrRuntimeTypeBinding *binding, TZrUInt32 fieldIndex) {
    TZrUInt32 index;
    for (index = 0u; index < binding->fieldCount; ++index) {
        if (binding->fields[index].fieldIndex == fieldIndex)
            return &binding->prototype->memberDescriptors[binding->fields[index].memberDescriptorIndex];
    }
    return ZR_NULL;
}

static TZrUInt32 zr_objects_identity_index(const SZrExecIrMaterializedState *logical, TZrUInt32 identity) {
    TZrUInt32 index;
    for (index = 0u; index < logical->aggregateCount; ++index)
        if (logical->aggregates[index].identityId == identity) return index;
    return logical->aggregateCount;
}

static TZrBool zr_objects_graph_valid(SZrObjectPreparation *prepared) {
    const SZrExecIrObjectMaterializationRequest *request = prepared->request;
    const SZrExecIrMaterializedState *logical = &prepared->logical;
    TZrUInt32 index, fieldIndex;
    if (request->target->capacity < logical->aggregateCount || !zr_objects_types_valid(request)) return ZR_FALSE;
    for (index = 0u; index < logical->valueCount; ++index) {
        TZrExecIrValueId id = logical->values[index];
        if (id == 0u || id > request->valueCount ||
            !zr_objects_value_valid(request->state, &request->values[id - 1u]) ||
            (request->checkpoint.function->values[id - 1u].nullability == ZR_EXEC_IR_NULLABILITY_NONNULL &&
             request->values[id - 1u].type == ZR_VALUE_TYPE_NULL)) return ZR_FALSE;
    }
    for (index = 0u; index < logical->aggregateCount; ++index) {
        const SZrExecIrDeoptAggregate *aggregate = &logical->aggregates[index];
        TZrUInt32 bindingIndex = zr_objects_binding_index(request, aggregate);
        /* Bound the unchecked arithmetic in the shared hash reservation API. */
        if (bindingIndex == request->typeCount || !zr_objects_hash_size(aggregate->fields.count))
            return ZR_FALSE;
        for (fieldIndex = 0u; fieldIndex < aggregate->fields.count; ++fieldIndex) {
            const SZrExecIrDeoptAggregateField *field =
                &logical->aggregateFields[aggregate->fields.start + fieldIndex];
            if (zr_objects_descriptor(&request->types[bindingIndex], field->fieldIndex) == ZR_NULL)
                return ZR_FALSE;
            if (field->kind == ZR_EXEC_IR_DEOPT_FIELD_VALUE) {
                EZrExecIrOwnership owner = request->checkpoint.function->values[field->valueId - 1u].ownership;
                if (field->valueId > request->valueCount ||
                    owner == ZR_EXEC_IR_OWNERSHIP_UNIQUE || owner == ZR_EXEC_IR_OWNERSHIP_SHARED ||
                    owner == ZR_EXEC_IR_OWNERSHIP_BORROWED ||
                    !zr_objects_plain_value(&request->values[field->valueId - 1u])) return ZR_FALSE;
            } else if (field->kind == ZR_EXEC_IR_DEOPT_FIELD_AGGREGATE &&
                       zr_objects_identity_index(logical, field->aggregateId) == logical->aggregateCount)
                return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_objects_allocate_work(SZrObjectPreparation *prepared) {
    const SZrExecIrObjectMaterializationRequest *request = prepared->request;
    const SZrExecIrMaterializedState *logical = &prepared->logical;
    const SZrGarbageCollector *collector = request->state->global->garbageCollector;
    TZrUInt64 slots = (TZrUInt64)logical->valueCount + request->typeCount + request->target->count +
                     logical->aggregateCount + logical->aggregateFieldCount + 1u;
    TZrUInt32 index, fieldIndex;
    if (collector->ignoredObjectCount > UINT32_MAX) return ZR_FALSE;
    prepared->ignoredCount = (TZrUInt32)collector->ignoredObjectCount;
    slots += prepared->ignoredCount;
    if (slots > UINT32_MAX || !zr_objects_size((TZrUInt32)slots, sizeof(*prepared->rootSlots)) ||
        !zr_objects_size(prepared->ignoredCount, sizeof(*prepared->ignored)) ||
        !zr_objects_size(logical->aggregateCount, sizeof(*prepared->objects)) ||
        !zr_objects_size(logical->aggregateCount, sizeof(*prepared->maps)) ||
        !zr_objects_size(logical->aggregateCount, sizeof(*prepared->bindings)) ||
        !zr_objects_size(logical->aggregateFieldCount, sizeof(*prepared->names))) return ZR_FALSE;
    prepared->objects = calloc(logical->aggregateCount, sizeof(*prepared->objects));
    prepared->maps = calloc(logical->aggregateCount, sizeof(*prepared->maps));
    prepared->bindings = calloc(logical->aggregateCount, sizeof(*prepared->bindings));
    prepared->names = calloc(logical->aggregateFieldCount != 0u ? logical->aggregateFieldCount : 1u,
                              sizeof(*prepared->names));
    prepared->rootSlots = calloc((size_t)slots, sizeof(*prepared->rootSlots));
    if (prepared->ignoredCount != 0u)
        prepared->ignored = calloc(prepared->ignoredCount, sizeof(*prepared->ignored));
    if (prepared->objects == ZR_NULL || prepared->maps == ZR_NULL || prepared->bindings == ZR_NULL ||
        prepared->names == ZR_NULL || prepared->rootSlots == ZR_NULL ||
        (prepared->ignoredCount != 0u && prepared->ignored == ZR_NULL)) return ZR_FALSE;
    if (prepared->ignoredCount != 0u)
        memcpy(prepared->ignored, collector->ignoredObjects,
               (size_t)prepared->ignoredCount * sizeof(*prepared->ignored));
    for (index = 0u; index < logical->aggregateCount; ++index) {
        const SZrExecIrDeoptAggregate *aggregate = &logical->aggregates[index];
        prepared->bindings[index] = zr_objects_binding_index(request, aggregate);
        ZrCore_HashSet_Construct(&prepared->maps[index]);
        for (fieldIndex = 0u; fieldIndex < aggregate->fields.count; ++fieldIndex) {
            TZrUInt32 offset = aggregate->fields.start + fieldIndex;
            prepared->names[offset] = zr_objects_descriptor(&request->types[prepared->bindings[index]],
                                                logical->aggregateFields[offset].fieldIndex)->name;
        }
    }
    return ZR_TRUE;
}

static void zr_objects_root_slot(SZrObjectPreparation *prepared, TZrUInt32 offset) {
    SZrAotGcRootSlot *slot = &prepared->rootSlots[prepared->slotCount++];
    slot->locationKind = ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS;
    slot->frameByteOffset = offset;
}

static TZrBool zr_objects_push_roots(SZrObjectPreparation *prepared, void *base, TZrUInt32 start) {
    TZrUInt32 frame = prepared->frameCount;
    if (prepared->slotCount == start) return ZR_TRUE;
    prepared->rootMaps[frame].rootCount = prepared->slotCount - start;
    prepared->rootMaps[frame].roots = &prepared->rootSlots[start];
    if (!ZrCore_Gc_AotRootFramePush(prepared->request->state, &prepared->rootFrames[frame],
                                    (TZrStackValuePointer)base, &prepared->rootMaps[frame])) return ZR_FALSE;
    ++prepared->frameCount;
    return ZR_TRUE;
}

static TZrBool zr_objects_register_roots(SZrObjectPreparation *prepared) {
    const SZrExecIrObjectMaterializationRequest *request = prepared->request;
    TZrUInt32 index, start = 0u;
    for (index = 0u; index < prepared->logical.valueCount; ++index) {
        TZrUInt32 valueIndex = prepared->logical.values[index] - 1u;
        if (request->values[valueIndex].isGarbageCollectable)
            zr_objects_root_slot(prepared, (TZrUInt32)((size_t)valueIndex * sizeof(*request->values) +
                                                      offsetof(SZrTypeValue, value)));
    }
    if (!zr_objects_push_roots(prepared, request->values, start)) return ZR_FALSE;
    start = prepared->slotCount;
    for (index = 0u; index < request->typeCount; ++index)
        zr_objects_root_slot(prepared, (TZrUInt32)((size_t)index * sizeof(*request->types) +
                                          offsetof(SZrExecIrRuntimeTypeBinding, prototype)));
    if (!zr_objects_push_roots(prepared, request->types, start)) return ZR_FALSE;
    start = prepared->slotCount;
    for (index = 0u; index < request->target->count; ++index)
        zr_objects_root_slot(prepared, (TZrUInt32)((size_t)index * sizeof(*request->target->objects)));
    if (!zr_objects_push_roots(prepared, request->target->objects, start)) return ZR_FALSE;
    start = prepared->slotCount;
    for (index = 0u; index < prepared->logical.aggregateCount; ++index)
        zr_objects_root_slot(prepared, (TZrUInt32)((size_t)index * sizeof(*prepared->objects)));
    if (!zr_objects_push_roots(prepared, prepared->objects, start)) return ZR_FALSE;
    start = prepared->slotCount;
    for (index = 0u; index < prepared->logical.aggregateFieldCount; ++index)
        zr_objects_root_slot(prepared, (TZrUInt32)((size_t)index * sizeof(*prepared->names)));
    if (!zr_objects_push_roots(prepared, prepared->names, start)) return ZR_FALSE;
    start = prepared->slotCount;
    if (prepared->savedException.isGarbageCollectable)
        zr_objects_root_slot(prepared, (TZrUInt32)offsetof(SZrTypeValue, value));
    if (!zr_objects_push_roots(prepared, &prepared->savedException, start)) return ZR_FALSE;
    start = prepared->slotCount;
    for (index = 0u; index < prepared->ignoredCount; ++index)
        zr_objects_root_slot(prepared, (TZrUInt32)((size_t)index * sizeof(*prepared->ignored)));
    return zr_objects_push_roots(prepared, prepared->ignored, start);
}

static TZrBool zr_objects_restore_ignored(SZrObjectPreparation *prepared) {
    TZrUInt32 index;
    if (prepared->ignored == ZR_NULL) return ZR_TRUE;
    /* Barriers also propagate closure escape and may unignore captures. Keep
     * the entire prior registry, not just the direct field values. Unignore
     * never shrinks capacity, so restoring these entries cannot allocate. */
    for (index = 0u; index < prepared->ignoredCount; ++index) {
        SZrRawObject *object = prepared->ignored[index];
        if (object != ZR_NULL &&
            !ZrCore_GarbageCollector_IsObjectIgnoredFast(prepared->request->state->global, object) &&
            !ZrCore_GarbageCollector_IgnoreObject(prepared->request->state, object)) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static void zr_objects_prepare_runtime(SZrState *state, TZrPtr arguments) {
    SZrObjectPreparation *prepared = arguments;
    const SZrExecIrObjectMaterializationRequest *request = prepared->request;
    const SZrExecIrMaterializedState *logical = &prepared->logical;
    TZrUInt32 index, fieldIndex;
    /* Do not pass a movable prototype through Object_New's allocating call. */
    for (index = 0u; index < logical->aggregateCount; ++index) {
        prepared->objects[index] = ZrCore_Object_New(state, ZR_NULL);
        if (prepared->objects[index] == ZR_NULL) return;
    }
    for (index = 0u; index < logical->aggregateCount; ++index) {
        TZrUInt32 initialized = 0u;
        const SZrExecIrDeoptAggregate *aggregate = &logical->aggregates[index];
        for (fieldIndex = 0u; fieldIndex < aggregate->fields.count; ++fieldIndex)
            if (logical->aggregateFields[aggregate->fields.start + fieldIndex].kind !=
                ZR_EXEC_IR_DEOPT_FIELD_UNINITIALIZED) ++initialized;
        /* Detached maps live in native storage; GC cannot invalidate the set
         * pointer retained by the allocating reservation implementation. */
        ZrCore_HashSet_Init(state, &prepared->maps[index], ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
        if (!prepared->maps[index].isValid ||
            !ZrCore_HashSet_EnsureCapacityForElementCount(state, &prepared->maps[index], initialized) ||
            !ZrCore_HashSet_EnsurePairPoolForElementCount(state, &prepared->maps[index], initialized)) return;
    }
    prepared->error = ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED;
    /* MutationBegin protects concurrent marking, but its nested callers may
     * park for another collector. Own the collection pause before retaining
     * any movable raw address across prototype/field write barriers. */
    if (!ZrCore_GcDomain_StopTheWorldBegin(
                state, ZR_MATERIALIZE_PAUSE_TIMEOUT_MILLISECONDS, ZR_NULL)) return;
    prepared->worldPaused = ZR_TRUE;
    prepared->mutationLocked = ZrCore_GcDomain_MutationBegin(state);
    /* Reserve only after other mutators and concurrent marking can no longer
     * consume the spare slots. This helper allocates native memory without GC;
     * no movable raw addresses have been retained or connected yet. */
    prepared->error = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
    {
        TZrSize remembered = state->global->garbageCollector->rememberedObjectCount;
        TZrSize limit = SIZE_MAX / (2u * sizeof(SZrRawObject *));
        if (remembered > limit || logical->aggregateCount > limit - remembered ||
            !garbage_collector_ensure_remembered_registry_capacity(
                state->global, remembered + logical->aggregateCount)) return;
    }
    prepared->error = ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED;
    /* No GC allocation or user callback is permitted from this point. */
    for (index = 0u; index < logical->aggregateCount; ++index) {
        SZrObject *object = prepared->objects[index];
        SZrObjectPrototype *prototype = request->types[prepared->bindings[index]].prototype;
        object->prototype = prototype;
        object->internalType = prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT
                                   ? ZR_OBJECT_INTERNAL_TYPE_STRUCT : ZR_OBJECT_INTERNAL_TYPE_OBJECT;
        object->nodeMap = prepared->maps[index];
        ZrCore_HashSet_Construct(&prepared->maps[index]);
        ZrCore_RawObject_Barrier(state, &object->super, &prototype->super.super);
    }
    for (index = 0u; index < logical->aggregateCount; ++index) {
        SZrObject *object = prepared->objects[index];
        const SZrExecIrDeoptAggregate *aggregate = &logical->aggregates[index];
        for (fieldIndex = 0u; fieldIndex < aggregate->fields.count; ++fieldIndex) {
            TZrUInt32 offset = aggregate->fields.start + fieldIndex;
            const SZrExecIrDeoptAggregateField *field = &logical->aggregateFields[offset];
            SZrHashKeyValuePair *pair;
            SZrTypeValue value;
            TZrSize bucket;
            if (field->kind == ZR_EXEC_IR_DEOPT_FIELD_UNINITIALIZED) continue;
            pair = ZrCore_HashSet_TakeReservedPair(&object->nodeMap);
            if (pair == ZR_NULL) goto finish;
            ZrCore_Value_InitAsRawObject(state, &pair->key, &prepared->names[offset]->super);
            ZrCore_Value_ResetAsNull(&pair->value);
            bucket = ZR_HASH_MOD(prepared->names[offset]->super.hash, object->nodeMap.capacity);
            pair->next = object->nodeMap.buckets[bucket];
            object->nodeMap.buckets[bucket] = pair;
            ++object->nodeMap.elementCount;
            ZrCore_Gc_WriteBarrier(state, &object->super, &pair->key);
            if (field->kind == ZR_EXEC_IR_DEOPT_FIELD_VALUE) value = request->values[field->valueId - 1u];
            else ZrCore_Value_InitAsRawObject(state, &value,
                &prepared->objects[zr_objects_identity_index(logical, field->aggregateId)]->super);
            if (!ZrCore_GcDomain_ValidateValueWrite(state, &object->super, &value)) goto finish;
            /* A regular SetValue/Value_Copy clones STRUCT payloads. Existing
             * normalized NONE cells preserve the already allocated identity. */
            ZrCore_Object_SetExistingPairValueUnchecked(state, object, pair, &value);
        }
    }
    prepared->ready = zr_objects_restore_ignored(prepared);
finish:
    (void)zr_objects_restore_ignored(prepared);
    /* Retain the pause and marking lock through publication and heap cleanup. */
}

static void zr_objects_cleanup(SZrObjectPreparation *prepared) {
    TZrUInt32 index;
    SZrState *state = prepared->request->state;
    /* A caught throw released our marking lock before rejoining the mutator
     * protocol. Our collection pause still prevents this entry from parking. */
    if (prepared->worldPaused && !prepared->mutationLocked)
        prepared->mutationLocked = ZrCore_GcDomain_MutationBegin(state);
    (void)zr_objects_restore_ignored(prepared);
    for (index = 0u; index < prepared->logical.aggregateCount; ++index) {
        if (prepared->maps != ZR_NULL) ZrCore_HashSet_Deconstruct(state, &prepared->maps[index]);
        if (!prepared->ready && prepared->objects != ZR_NULL && prepared->objects[index] != ZR_NULL) {
            ZrCore_Object_Deconstruct(state, prepared->objects[index]);
            prepared->objects[index]->cachedStringLookupPair = ZR_NULL;
            prepared->objects[index]->cachedStringLookupPair2 = ZR_NULL;
        }
    }
    ZrCore_GcDomain_MutationEnd(state, prepared->mutationLocked);
    prepared->mutationLocked = ZR_FALSE;
    while (prepared->frameCount != 0u) {
        --prepared->frameCount;
        (void)ZrCore_Gc_AotRootFramePop(state, &prepared->rootFrames[prepared->frameCount]);
    }
    if (prepared->worldPaused) {
        ZrCore_GcDomain_StopTheWorldEnd(state);
        prepared->worldPaused = ZR_FALSE;
    }
    free(prepared->objects);
    free(prepared->maps);
    free(prepared->bindings);
    free(prepared->names);
    free(prepared->ignored);
    free(prepared->rootSlots);
    ZrCore_ExecIr_MaterializedStateFree(&prepared->logical);
}

TZrBool ZrCore_ExecIr_MaterializeObjects(
        const SZrExecIrObjectMaterializationRequest *request,
        SZrExecIrDiagnostic *diagnostic) {
    SZrObjectPreparation prepared = {0};
    SZrExecIrResumeRequest checkpoint;
    EZrThreadStatus status;
    TZrBool success = ZR_FALSE;
    if (!zr_objects_request_valid(request)) {
        zr_objects_diagnostic(request, diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
        return ZR_FALSE;
    }
    prepared.request = request;
    if (!zr_objects_save_scopes(&prepared)) {
        zr_objects_diagnostic(request, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED);
        return ZR_FALSE;
    }
    checkpoint = request->checkpoint;
    checkpoint.target = &prepared.logical;
    if (!ZrCore_ExecIr_MaterializeState(&checkpoint, diagnostic)) return ZR_FALSE;
    prepared.error = ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED;
    if (!zr_objects_graph_valid(&prepared)) goto finish;
    if (prepared.logical.aggregateCount == 0u) {
        request->target->count = 0u;
        success = ZR_TRUE;
        goto finish;
    }
    prepared.error = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
    if (!zr_objects_allocate_work(&prepared)) goto finish;
    prepared.savedException = request->state->currentException;
    prepared.savedExceptionStatus = request->state->currentExceptionStatus;
    prepared.savedThreadStatus = request->state->threadStatus;
    prepared.hadException = request->state->hasCurrentException;
    if (!zr_objects_register_roots(&prepared)) goto finish;
    /* Also protect callers that entered from an inactive state. The roots
     * must precede this entry because its handshake can permit moving GC. */
    if (!ZrCore_GcDomain_MutatorEnter(request->state)) {
        prepared.error = ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED;
        goto finish;
    }
    prepared.scopeEntered = ZR_TRUE;
    request->state->threadStatus = ZR_THREAD_STATUS_FINE;
    status = ZrCore_Exception_TryRun(request->state, zr_objects_prepare_runtime, &prepared);
    if (status != ZR_THREAD_STATUS_FINE) {
        TZrBool entered;
        /* Throw unwinds ALL caller execution/native scopes, not only ours.
         * Rejoin the safepoint protocol before touching movable heap state.
         * Never wait for a collector while retaining our mutation lock. */
        ZrCore_GcDomain_MutationEnd(request->state, prepared.mutationLocked);
        prepared.mutationLocked = ZR_FALSE;
        entered = ZrCore_GcDomain_MutatorEnter(request->state);
        ZR_ASSERT(entered); /* The same registered state survived the local catch. */
        if (!entered) status = ZR_THREAD_STATUS_RUNTIME_ERROR;
    }
    request->state->currentException = prepared.savedException;
    request->state->currentExceptionStatus = prepared.savedExceptionStatus;
    request->state->hasCurrentException = prepared.hadException;
    request->state->threadStatus = prepared.savedThreadStatus;
    if (status != ZR_THREAD_STATUS_FINE) {
        prepared.error = status == ZR_THREAD_STATUS_MEMORY_ERROR ? ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY
                                                               : ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED;
        goto finish;
    }
    if (!prepared.ready) goto finish;
    /* Caller-owned output is published once, after every fallible operation. */
    memcpy(request->target->objects, prepared.objects,
           (size_t)prepared.logical.aggregateCount * sizeof(*prepared.objects));
    request->target->count = prepared.logical.aggregateCount;
    success = ZR_TRUE;
finish:
    if (!success) zr_objects_diagnostic(request, diagnostic, prepared.error);
    zr_objects_cleanup(&prepared);
    /* Keep the temporary RUNNING scope until roots and native storage have
     * been retired; an originally inactive caller becomes inactive only now. */
    zr_objects_restore_scopes(&prepared);
    return success;
}
