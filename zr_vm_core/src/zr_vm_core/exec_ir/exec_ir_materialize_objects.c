#include "exec_ir/exec_ir_runtime.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "gc/gc_domain_internal.h"
#include "gc/gc_internal.h"
#include "ownership/ownership_resource_internal.h"

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
        !zr_objects_array_disjoint(request, request->checkpoint.ownerStates,
                                     request->checkpoint.ownerStateCount,
                                     sizeof(*request->checkpoint.ownerStates)) ||
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
        TZrUInt32 owner = logical->ownerStates[index];
        /* Inactive cleanup obligations have no source payload. Do not even
         * inspect its tag: the caller may retain inaccessible/stale bytes. */
        if (owner == ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED ||
            owner == ZR_EXEC_IR_STATE_MAP_OWNER_MOVED || owner == ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED)
            continue;
        if (owner != ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED &&
            owner != ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN) return ZR_FALSE;
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
        TZrUInt32 owner = prepared->logical.ownerStates[index];
        if (owner != ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED &&
            owner != ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN) continue;
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
        /* Local Throw restored the entry RUNNING scope and released the
         * callback's marking lock. Cleanup may acquire a fresh level while
         * retaining our collection pause; the old token is already consumed. */
        prepared.mutationLocked = ZR_FALSE;
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

/**
 * @anchor zr_materialize_root_groups_anonymous_enum_contract
 *
 * ## Summary
 * 用匿名枚举中的值 7 为一次对象物化的临时 AOT 根帧与根映射提供固定容量，使所有分组在同一栈上准备记录中存活到统一清理。
 *
 * ## Value model
 * ZR_MATERIALIZE_ROOT_GROUPS 是帧分组上限，不是对象数、根槽总数或重试次数。七组依次为活动源单元、类型原型、旧输出、新对象、字段名称、保存的环境异常和既有 ignored 注册快照；空组可不实际压帧。
 *
 * ## Invariants and lifecycle
 * rootFrames 与 rootMaps 都按此值定长；register_roots 的七个分组与该容量必须同步维护。root_slot/push_roots 依赖外层正确计算容量和调用顺序，没有独立的 frameCount<7 检查。
 * 该值只参与编译期数组布局，不持有资源或建立GC根；运行期帧只由成功的 AotRootFramePush 增加 frameCount，cleanup 按该成功前缀逆序弹出。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:31、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:32 使用该值定义数组；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:500、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:505、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:509、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:513、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:517、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:521、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:525 对应七组提交；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:649 逆序清理。
 */

/**
 * @def ZR_MATERIALIZE_PAUSE_TIMEOUT_MILLISECONDS
 *
 * ## Summary
 * 为对象图附接阶段的 StopTheWorldBegin 提供毫秒等待上限；在保留可移动 raw 地址并执行写屏障前争取 collection pause。
 *
 * ## Value model
 * 展开为 TZrUInt32 类型的 1000u，唯一运行期消费处把它作为 timeoutMilliseconds；没有参数、调用副作用或返回状态。它限制该暂停请求的等待策略，不是整个物化/GC/线程回收的完成时限。
 *
 * ## Constraints and lifecycle
 * 宏本身不持锁、暂停线程或安排计时。StopTheWorldBegin 返回真后 prepared.worldPaused 才记账，失败经 ready 为假和外层诊断/清理结束；本接口没有重试该请求的循环。
 * 暂停策略及重入深度由 GC-domain 接口负责；已有外层 pause 必须保留其独立层级。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:570、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:571 是唯一值消费，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:572 记录成功取得的一层；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:653 仅结束本次记账的 pause；tests/core/test_ssa_runtime_objects.c:355 检查被注入拒绝，tests/core/test_ssa_runtime_objects.c:427 检查外层 pause 保留。
 */

/**
 * @struct SZrObjectPreparation
 *
 * ## Summary
 * 一次 MaterializeObjects 同步调用的栈上准备与清理记录：把私有逻辑快照、native 工作存储、GC 根、异常副本及暂停/锁令牌分开记账，直到最后发布或放弃候选图。
 *
 * ## Fields
 * request 借用外层请求，不接管请求、function/map、source单元、binding、state 或 target。logical 单独拥有 MaterializeState 成功交付的五类 native ID/owner/recipe 数组；它不是运行时对象保活容器。
 * objects、maps、bindings、names、rootSlots 和可选 ignored 是本次 calloc 的工作数组。objects 的元素是受GC管理的新对象；names/ignored 元素借用字符串/既有raw对象；free这些数组不等于free其GC对象。maps 在附接前拥有分离的哈希存储，转入object.nodeMap后原项经Construct置空以避免重复释放。
 * rootFrames/rootMaps 是七组固定记录；rootSlots 是动态描述数组。frameCount只累计成功压栈的帧，slotCount是已填槽的前缀长度，ignoredCount是先前registry快照项数。根图借用各数组或保存的异常单元，可能由moving GC改写其中的对象指针。
 * savedException及三个异常/线程标志保存环境状态；savedExecutionDepth、savedNativeDepth、savedNativeMode、savedNativeEnteredFromInactive保存入口scope数值，不保存mutator记录指针或旧epoch。
 * scopeEntered、worldPaused和mutationLocked分别记账本次额外RUNNING scope、collection pause层和确实取得的mutation锁令牌；FALSE锁令牌也可表示无需锁。ready只表示候选图已完成附接及ignore恢复，尚未代表target已发布；error供外层失败路径选择诊断。
 *
 * ## Invariants and lifecycle
 * 外层669全零初始化，并在取得每类资源后更新对应计数/标志；allocate_work须面对空的旧工作指针，不能在已有工作存储上重入覆盖。logical有效后先完成graph验证，再准备工作数组/根；根先于可允许GC的MutatorEnter建立。
 * 所有新对象先分配，再分离预留哈希存储，取得pause和必要mutation令牌后才接原型与字段，因此合法前向边、共享边和环能引用同一已分配对象。禁止将runtime callback或本记录跨同步TryRun保存。
 * cleanup逆序退根、结束本次pause并free六工作数组和logical；它不清空所有已free的指针，因此记录不是可重复调用cleanup或直接复用的通用对象。随后只有request与保存的scope字段继续用于restore_scopes；原始记录栈地址仍存在到公开入口返回。
 * 失败不发布候选对象图，但GC仍可能更新输入/旧输出指针，且runtime新分配对象可能留待GC回收；成功仅复制新指针到caller数组，旧GC对象不在这里手工free。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:669 建立记录，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:677 借用请求；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:684 准备logical；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:693、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:698 准备工作数组/根；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:593、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:594 转移native map；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:707 同步callback；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:725 发布；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:731、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:734 分别清理资源与恢复scope。
 */

/**
 * @fn static SZrGcDomainMutatorRecord *zr_objects_mutator_locked(SZrState *state)
 *
 * ## Summary
 * 在调用者已持有GC-domain协调锁时，线性找到属于当前state的mutator记录，供入口scope快照与退出恢复使用。
 *
 * ## Parameters
 * @param[in] state 非空有效state，其gcDomain和mutator数组须在当前协调锁保护下可读；按state指针身份匹配。
 *
 * ## Returns
 * @return 命中时返回domain数组中首个匹配记录的可写借用地址；未登记则返回ZR_NULL。
 *
 * ## Constraints and lifecycle
 * 本函数不自行加锁、不验证state/domain为空、不分配或延长domain/记录寿命。记录数组可在解锁后移动，返回指针不得保留跨越Unlock；修改记录仍须在同一协调锁下进行。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:64 与 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:84 是仅有直接调用，分别由 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:63、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:83 先加锁，并在 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:75、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:97 前用完借用。
 */

/**
 * @fn static TZrBool zr_objects_save_scopes(SZrObjectPreparation *prepared)
 *
 * ## Summary
 * 在创建额外RUNNING scope之前读取入口执行/native深度，拒绝不适合该物化路径的native模式或登记状态，便于最后精确撤销本次额外scope。
 *
 * ## Parameters
 * @param[in,out] prepared 带有效request/state/domain的非空准备记录；成功时写四个saved scope字段，其他资源仍由外层持有。
 *
 * ## Returns
 * @return 记录存在、nativeMode为GC_AWARE、executionDepth不为UINT32_MAX，且status为RUNNING或ATTACHED_INACTIVE时复制快照并返回ZR_TRUE；否则返回ZR_FALSE，不写这四个保存字段。
 *
 * ## Constraints and lifecycle
 * 查找与复制均在domain协调锁内，解锁后不保留记录指针；本函数不进入scope、不暂停世界，也不为state/domain延寿。成功快照依赖调用期间该state登记与元数据保持有效，不能当任意并发detach的凭证。
 * 只保存数值深度/模式，不保存旧observedEpoch或复制整个registry。nativeDepth没有在此递增。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:678 是唯一调用；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:701 随后才MutatorEnter，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:734 最后恢复。有限嵌套与拒绝场景 tests/core/test_ssa_runtime_objects.c:271、tests/core/test_ssa_runtime_objects.c:294、tests/core/test_ssa_runtime_objects.c:311。
 */

/**
 * @fn static void zr_objects_restore_scopes(SZrObjectPreparation *prepared)
 *
 * ## Summary
 * 在临时根和native工作存储退休之后撤销本次额外RUNNING scope，把原来的执行/native深度重新发布到domain。
 *
 * ## Parameters
 * @param[in,out] prepared 带有效request/state的非空记录；四个saved scope字段来自save_scopes，scopeEntered表示本次MutatorEnter确实成功。
 *
 * ## Returns
 * 无C返回值。scopeEntered为假时直接返回；为真时在协调锁内重新查找记录，若仍登记就恢复四字段、按保存深度计算RUNNING/ATTACHED_INACTIVE并发布当前safepointEpoch，最后清scopeEntered。未找到记录也清该标志，不产生诊断码。
 *
 * ## Constraints and lifecycle
 * 不能保留旧mutator记录指针；不恢复历史observedEpoch，以免把collector握手前的epoch重新发布。它不负责弹根、结束pause或释放mutation锁，必须在cleanup之后调用。
 * state/domain须稳定且仍符合入口的GC-aware协议；这是一次调用的收尾，不是通用线程登记修复。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:734 是唯一直接调用，位于 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:731 cleanup之后；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:94 采用当前epoch。有限scope和throw场景 tests/core/test_ssa_runtime_objects.c:271、tests/core/test_ssa_runtime_objects.c:294、tests/core/test_ssa_runtime_objects.c:392。
 */

/**
 * @fn static void zr_objects_diagnostic(const SZrExecIrObjectMaterializationRequest *request, SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code)
 *
 * ## Summary
 * 为本层拒绝或runtime失败补充诊断code与能够解析的function/checkpoint位置；按信息可用程度提前结束，不再执行完整校验。
 *
 * ## Parameters
 * @param[in] request 可为空；非空时本函数实际访问的checkpoint、function、选中的map及blocks描述须有效、稳定可读。
 * @param[out] diagnostic 可为空；非空时先全部清零再写code及已取得的位置，须有独立可写存储，不能覆盖request/function/map/blocks等会读取的输入。
 * @param[in] code 调用者选择的失败分类，直接写入diagnostic->code。
 *
 * ## Returns
 * 无C返回值。diagnostic为空时不读取request；否则code始终先写入。请求/function/entry不可取得时其余未填字段保持零；找到entry后写instructionId，并在合法的blocks前缀中找到首个包含该1起点instructionId的blockId。
 *
 * ## Constraints and lifecycle
 * 先memset输出再读取输入，因此本函数不提供重叠输入输出保障。它依赖StateMapFind的局部形状检查，但不是对任意非空地址或整个IR结构的认证；不分配、不保存输入、不改变target。
 * block范围比较使用先大于start再做减法的顺序，不把相邻声明或普通sourceId解释成已经找到block。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:674、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:679、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:730 是三处调用；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:117 查找entry，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:124 用减法定位block。tests/core/test_ssa_runtime_objects.c:174 的assert_failure_location检查有限字段，tests/core/test_ssa_runtime_objects.c:791 检查早拒绝INVALID_ARGUMENT。
 */

/**
 * @fn static TZrBool zr_objects_size(TZrUInt32 count, size_t size)
 *
 * ## Summary
 * 核对数组字节数是否同时适合主机 size_t 和本模块使用的 UInt32 根偏移；在请求跨度计算和临时根/工作数组分配前使用。
 *
 * ## Parameters
 * @param[in] count 待表示的元素数；零个元素仍须满足 size 非零。
 * @param[in] size 一个完整元素的字节数；零值直接无效。
 *
 * ## Returns
 * @return 仅在 size 非零，且 count 同时不超过 SIZE_MAX / size 和 UINT32_MAX / size 时返回 ZR_TRUE；否则返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 这是算术界限检查，不接收或验证地址、真实分配 extent、可读性、对齐或 count 与其他池的关系。零 count 配合法的非零 size 返回真。
 * 本函数同步读取数值，不分配、不保存参数；失败不改变请求或输出。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:162；请求计数检查 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:228、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:229、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:230、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:249；工作数组检查 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:438、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:439、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:440、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:441、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:442、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:443。
 */

/**
 * @fn static TZrBool zr_objects_hash_size(TZrSize count)
 *
 * ## Summary
 * 在配方字段进入共享哈希预留接口前，限制 pair 池块字节数及桶容量相关乘法的输入规模。
 *
 * ## Parameters
 * @param[in] count 一个 aggregate 配方的字段数，按全部字段计入，包括未初始化字段；零值允许。
 *
 * ## Returns
 * @return count 同时不超过两条源码界限时返回 ZR_TRUE：可容纳块头的 pair 数，以及 SIZE_MAX / (ZR_HASH_SET_MAX_LOAD_DENOMINATOR * sizeof(void *))；否则返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 只判断数值上界；不分配哈希桶、不预留 pair，也不保证随后分配成功或检查已有哈希表状态。此处的保守字段总数界限先于运行阶段计算实际已初始化字段数。
 * 参数只在同步调用期间按值使用；失败不改变 logical 或 target。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:407 是唯一直接调用；真正的桶和 pair 预留在 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:563、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:564；共享容量计算入口 zr_vm_core/include/zr_vm_core/hash_set.h:319。
 */

/**
 * @fn static TZrBool zr_objects_overlap(const void *left, size_t leftSize, const void *right, size_t rightSize)
 *
 * ## Summary
 * 用整数地址和半开字节区间判断两段存储是否可能相交，为输出与输入的别名拒绝提供保守结果。
 *
 * ## Parameters
 * @param[in] left 左段起点；本函数只转换地址，不解引用。非空跨度遇 NULL 按可能相交处理。
 * @param[in] leftSize 左段字节数；零表示空段。
 * @param[in] right 右段起点；规则同 left。
 * @param[in] rightSize 右段字节数；零表示空段。
 *
 * ## Returns
 * @return 任一字节数为零先返回 ZR_FALSE；两个非空段中任一起点为 NULL，或任一整数终点加法越过 UINTPTR_MAX，返回 ZR_TRUE；否则仅在半开区间相交时返回 ZR_TRUE，首尾相接返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * ZR_TRUE 也可能表示空指针或终点无法安全表示，不能解释成已证明两份真实分配重叠。ZR_FALSE 只说明传入数值描述的区间不交，不证明地址可读、归属、对齐或实际 extent。
 * 无分配、解引用、写入或指针保留；不读取区间中的对象。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:153、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:154 检查 target 两种输出跨度；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:243 另查 target 记录与其对象指针数组互不覆盖；有限别名用例 tests/core/test_ssa_runtime_objects.c:652、tests/core/test_ssa_runtime_objects.c:690、tests/core/test_ssa_runtime_objects.c:791。
 */

/**
 * @fn static TZrBool zr_objects_target_disjoint(const SZrExecIrObjectMaterializationRequest *request, const void *input, size_t bytes)
 *
 * ## Summary
 * 核对单个输入跨度与输出 target 记录、整个 capacity 对象指针数组均不相交；由上层逐项枚举需要保护的输入。
 *
 * ## Parameters
 * @param[in] request 调用期间可读的请求，target 必须指向可读记录；target.capacity 的指针数组字节乘法须已由请求校验约束。
 * @param[in] input 待保护输入跨度的起点；只作地址比较，不解引用。
 * @param[in] bytes 输入跨度字节数；零为空段，非空 NULL 或终点溢出由 overlap 保守拒绝。
 *
 * ## Returns
 * @return 输入跨度与 target 记录和 target.objects 的整个 capacity 区间均不相交时返回 ZR_TRUE；任一比较为可能相交时返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 本函数直接解引用 request/target，不自行验证二者为空、count/capacity、数组乘法或分配来源。它也不比较两种输出跨度彼此是否重叠，该项由请求校验单独处理。
 * 只覆盖传入跨度；不会递归保护 input 指向的所有对象。请求和 target 记录由调用方保有，调用期间借用，不保存地址或改变内容。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:163、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:169、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:202、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:206、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:231、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:235、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:237、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:239、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:241、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:250、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:323；输出彼此检查 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:243。
 */

/**
 * @fn static TZrBool zr_objects_array_disjoint(const SZrExecIrObjectMaterializationRequest *request, const void *input, TZrUInt32 capacity, size_t elementSize)
 *
 * ## Summary
 * 将输入数组的容量转换为有界字节跨度，再检查其与输出的两段存储是否分离。
 *
 * ## Parameters
 * @param[in] request 供 target_disjoint 使用的有效请求；非空输入分支要求其 target 及容量已可安全读取。
 * @param[in] input 数组起点；NULL 时立即只按 capacity 是否为零作判断。
 * @param[in] capacity 需要保护的整个容量，不是已用元素数。
 * @param[in] elementSize 非空输入分支使用的单元素字节数，必须非零并满足双数值上限。
 *
 * ## Returns
 * @return input 为 NULL 时，capacity 为零返回 ZR_TRUE，否则返回 ZR_FALSE；该短路不读取 request，也不检查 elementSize。input 非空时，只有数组乘法界限和 target 不相交检查均成功才返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * 非空 input 配零容量且非零 elementSize 可通过，形成空跨度；不能误写为强制 NULL 与零容量双向等价。非空输入配零 elementSize 失败。
 * 不解引用数组元素、不核对实际分配大小或 count；只同步借用地址与容量，不分配、不写入、不保存参数。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:170、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:171、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:172、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:173 保护 map 四池；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:199、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:203、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:207、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:209、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:211 保护 IR/附属池；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:232 保护 owner witness；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:324 保护 descriptor 容量。
 */

/**
 * @fn static TZrBool zr_objects_map_disjoint(const SZrExecIrObjectMaterializationRequest *request, const SZrExecIrStateMap *map)
 *
 * ## Summary
 * 保护可选 state map 记录及其 entries、valuePool、rootPool、ownerStatePool 四个容量区间，使对象发布存储不会覆盖这些输入。
 *
 * ## Parameters
 * @param[in] request 带有效 target 的借用请求；供下层跨度检查使用。
 * @param[in] map 可为 NULL；非空时记录与各非空池描述必须可读，池由其所属 function 或外部所有者保有。
 *
 * ## Returns
 * @return map 为 NULL 直接返回 ZR_TRUE；非空时记录跨度和四池容量跨度均通过检查才返回 ZR_TRUE，首次失败短路返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 此处不是 StateMapStorageValid 或 ValidateStateMap：不验证各 count、entry 内容、generation、identity 或池元素。NULL map 只表示本步骤无该记录可检查，不保证后续 logical materialization 能找到可用 map。
 * 只读取 map 描述并比较地址，调用期间借用，不释放、复制或保留 map。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:214 和 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:215 分别处理 checkpoint.map 与 function.stateMap，同一指针可被检查两次；完整 logical 校验来自 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:684；有限 valuePool 别名断言 tests/core/test_ssa_runtime_objects.c:690。
 */

/**
 * @fn static TZrBool zr_objects_ir_disjoint(const SZrExecIrObjectMaterializationRequest *request)
 *
 * ## Summary
 * 枚举 function 的十六个基础容量池，并保护可选 frameLayout、gcMap 及两份 state map 中本函数列出的记录与池，避免输出覆盖恢复所需的 IR 元数据。
 *
 * ## Parameters
 * @param[in] request 已具备可读 checkpoint.function 和有效 target 的请求；function 及所有实际访问的非空附属记录必须在调用期间保持有效。
 *
 * ## Returns
 * @return 十六个基础数组、存在的 frameLayout/slots、gcMap 三池与两份 state map 均通过输出分离检查时返回 ZR_TRUE；首次失败返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 函数局部 spans 表按字段与其 capacity、sizeof(*field) 成对构造，保护整个容量而非 count。该表只借用地址，生命周期止于本次同步调用。
 * 这里只做显式枚举的几何检查，不验证 IR 的结构语义、真实 allocation extent 或所有递归可达对象；function 记录本身由 request_valid 另查，未列出的指向数据也没有自动保护。两个 map 可以相同或为空，是否能物化由后续 MaterializeState 决定。
 * 本函数不分配、不修改输入或输出、不保存借用。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:242 是唯一调用；局部描述类型 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:178 和 spans 表 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:179；function 记录另查 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:239；各 map 交给 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:214、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:215；有限 deoptAggregateFields/valuePool 别名用例 tests/core/test_ssa_runtime_objects.c:690。
 */

/**
 * @struct SZrMaterializeInputSpan
 *
 * ## Summary
 * zr_objects_ir_disjoint 内部用于枚举输入数组跨度的函数局部描述类型；它只将借用起点、容量和元素字节数绑定在一起。
 *
 * ## Fields
 * data 是输入数组的借用起点；本地表仅把该地址交给跨度比较，不拥有或复制元素。
 * capacity 是整个数组容量，不是已用 count；size 是与该数组元素类型对应的 sizeof 字节数。
 *
 * ## Invariants and lifecycle
 * 十六个表项分别配对 function 的数组字段、对应 capacity 与 sizeof(*field)，避免用错误单元大小计算保护跨度。
 * 下层检查接受 NULL+capacity0；非空 data 分支要求 size 非零且乘积同时适合 size_t 与 UInt32。
 * 这些数值不证明真实分配 extent、对齐或可读性，也不完成 IR 内容验证。
 * 类型及其 spans 自动数组只在 zr_objects_ir_disjoint 一次同步调用中使用，data 仍由原 function/池所有者保有；
 * 函数不保存这份局部表，不释放输入，不把 capacity 解释成可遍历的有效元素数。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:179 构造十六项 spans；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:199 将每项提交给 zr_objects_array_disjoint；
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:242 由请求预检同步进入父函数。
 */

/**
 * @fn static TZrBool zr_objects_request_valid(const SZrExecIrObjectMaterializationRequest *request)
 *
 * ## Summary
 * 在构造私有 logical snapshot 前检查对象物化请求的必要指针/计数、列举的输入输出别名关系，以及既有 target 对象的基础运行时归属。
 *
 * ## Parameters
 * @param[in] request 可为 NULL；非空时记录及后续实际解引用的 state/global/collector、target、function、数组和既有对象都须是调用期间有效的存储。
 *
 * ## Returns
 * @return 必要字段缺失、checkpoint.target 非 NULL、计数或大小无效、任一枚举跨度可能与输出相交，或旧输出非空对象不满足所属域/对象标记要求时返回 ZR_FALSE；本函数所列检查全部通过返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * 按源码顺序先检查 state/global/gcDomain/collector、function、必须为空的逻辑 checkpoint.target、非空对象 target、count<=capacity、非零长度数组指针和三组数值大小；随后检查请求/witness/value/type/function/state/IR 输入与输出分离，并单独拒绝 target 记录与其 capacity 指针数组互相覆盖；再检查各 type.fields 跨度，最后检查 target.count 内非空旧对象。
 * 旧对象只检查 RawObject 未失效、OBJECT 类型、非 GC box 和同 state 域；不要求数组各项唯一。capacity0 时 objects 可为 NULL 或非空。此处不验证 checkpoint 身份/phase、完整 state map、绑定语义或 recipe，后续步骤仍可失败。
 * 这是对可读数据的协议检查，不能把任意非空地址变成安全指针；地址集合未覆盖 diagnostic、source GC payload 的所有 pointee、global/collector 及所有递归数据。调用方仍须满足合法 GC 使用时机与稳定元数据要求；若公开操作提供 diagnostic，它须有独立可写存储。只在同步调用期间借用，未保存/写入 request 或 target。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:673 是公开入口首步；通过后 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:677 将请求地址仅借给栈上 prepared，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:684 构造 logical snapshot，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:686 才检查 graph；对象请求公开协作定义 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_runtime.h:31；别名用例 tests/core/test_ssa_runtime_objects.c:652、tests/core/test_ssa_runtime_objects.c:690、tests/core/test_ssa_runtime_objects.c:791。
 */

/**
 * @fn static TZrBool zr_objects_value_valid(SZrState *state, const SZrTypeValue *value)
 *
 * ## Summary
 * 检查会被读取的活动源单元是否具有正常 value tag，并对 GC payload 核对非空、对象标签/原生标记一致、所属域和 resource 生命周期。
 *
 * ## Parameters
 * @param[in] state 从已核请求借用的有效运行时 state；用于 raw-object 与 GC 域检查，不在此接管。
 * @param[in] value 非空、可读的源单元；若标为垃圾收集对象，其非空 payload 必须可安全读取。
 *
 * ## Returns
 * @return 非正常 tag 返回 ZR_FALSE；GC 单元只有 payload 非空、未失效、raw 类型及 isNative 与单元一致、同域且非 DROPPING/DROPPED 时返回 ZR_TRUE；非 GC 单元只接受非 ZR_ITEM 且非 STRING，或原生 FUNCTION。
 *
 * ## Constraints and lifecycle
 * 不检查 ownershipKind/control/weakRef 或所有 raw resource 身份；这些由 plain_value 等独立步骤限制。不凭标志证明 payload 的分配来源/extent；没有 NULL value 防护，也不独立检查 state。
 * 只在调用期间读取借用，不保存地址、注册 GC 根、分配或改变单元。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:399 是唯一直接调用，位于 inactive owner 跳过之后 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:393；ownership/resource 限制另见 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:419；inactive poisoned slot 的有限用例 tests/core/test_ssa_runtime_objects.c:727，resource 源用例 tests/core/test_ssa_runtime_objects.c:638。
 */

/**
 * @fn static TZrBool zr_objects_plain_value(const SZrTypeValue *value)
 *
 * ## Summary
 * 拒绝需要 owner/weak handle 转移或 resource 语义的字段源，使当前恢复路径只重放普通非所有权值。
 *
 * ## Parameters
 * @param[in] value 非空、可读的字段源；若 isGarbageCollectable 非零，payload 及 resource 检查需要读取的对象/原型必须有效，不能传 NULL 或伪造地址。
 *
 * ## Returns
 * @return 仅在单元 ownershipKind 为 NONE、ownershipControl 和 ownershipWeakRef 均为空，且非 GC 单元或其 GC 对象无 control、不是 resource 对象、resourceLifecycleState 为 NONE 时返回 ZR_TRUE；否则返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 本函数不做完整 tag、域或对象有效性检查，也不因 ownershipKind==NONE 就允许 resource 对象。调用方须先满足 logical/源对象有效性的前提；它没有 GC payload 的空指针短路。
 * 只读取现存值，不 clone、move、drop、创建控制块或保存借用；失败保持源单元。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:419 唯一调用，所属 ExecIR 值的 UNIQUE/SHARED/BORROWED 限制在 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:415；resource 识别协作 zr_vm_core/src/zr_vm_core/ownership/ownership_resource.c:62；有限 owner/source 不变和无控制块 resource 拒绝用例 tests/core/test_ssa_runtime_objects.c:596、tests/core/test_ssa_runtime_objects.c:638。
 */

/**
 * @fn static TZrBool zr_objects_prototype_valid(SZrState *state, const SZrObjectPrototype *prototype)
 *
 * ## Summary
 * 沿继承链检查用于恢复的原型及其祖先是否为同域、非 resource、无 managed fields 的 CLASS/STRUCT 原型，并用快慢指针拒绝继承链环。
 *
 * ## Parameters
 * @param[in] state 非空链检查使用的有效运行时 state，调用期间借用；prototype 为空时不读取 state。
 * @param[in] prototype 继承链起点；NULL 表示空链并返回真，非空链的各节点及 superPrototype 链接必须可安全读取。
 *
 * ## Returns
 * @return 每个访问的节点均满足 raw/object/prototype 标记、所属域、CLASS/STRUCT、非 RESOURCE、managedFieldCount 为零、descriptor count/capacity 与非空存储要求，且快慢指针未遇非空相等节点时返回 ZR_TRUE；任一失败返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 本函数的环检查只针对原型继承链，不是 aggregate 配方图；配方图允许环。快慢指针也会沿尚未逐项完成语义检查的节点读取链接，故不是任意坏地址防护。
 * 不比较 layoutGeneration、不校验 descriptor 内容或数组真实 extent，也不为原型加根/保活。起点 NULL 的局部真值不放宽 types_valid 对 binding.prototype 非空的要求。只同步借用、不保存、分配或改写链。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:316 唯一调用，初始非空要求 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:315，代际比较 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:317；resource 原型有限拒绝用例 tests/core/test_ssa_runtime_objects.c:631；CLASS 和 STRUCT 正常图分别 tests/core/test_ssa_runtime_objects.c:708、tests/core/test_ssa_runtime_objects.c:182。
 */

/**
 * @fn static TZrBool zr_objects_types_valid(const SZrExecIrObjectMaterializationRequest *request)
 *
 * ## Summary
 * 检查整个运行时 type-binding 表，包括未被当前 recipe 使用的项：非零身份、当前原型布局代际、祖先元数据与输出分离，以及每个字段绑定的合法性和唯一性。
 *
 * ## Parameters
 * @param[in] request request_valid 已通过的可读请求；type/field 数组与所有实际读取的原型、descriptor、名称对象须在本次操作期间保持有效。
 *
 * ## Returns
 * @return 全部绑定均有非零 (typeToken,layoutId)、有效非空原型及相等 layoutGeneration；各祖先记录/descriptor 容量与输出分离；身份对无重复；字段目标为非静态 FIELD 且名称为同域有效 STRING，且同一绑定中 fieldIndex、descriptorIndex、名称内容均无重复时返回 ZR_TRUE；首次不满足返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 字段唯一性按表中之前项比较，因此访问之前 descriptor 的前提来自先前项已通过。layoutGeneration 只与该 binding 的 prototype 比较，不给所有祖先建立 generation lease。typeCount0 在此局部返回真，但非空 aggregate 随后的 binding 查找仍可失败。
 * 别名保护只覆盖祖先记录和 descriptor 容量等明示跨度，不递归覆盖 descriptor.name 字符串对象或所有引用。类型/字段关系是调用期间借用，不保存指针、注册根或分配；后续 GC 根管理仍须保护 prototype/name。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:387 唯一调用；原型协作 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:316；祖先输出跨度 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:323、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:324；布局代际/重复字段/非FIELD/metadata别名有限用例 tests/core/test_ssa_runtime_objects.c:331、tests/core/test_ssa_runtime_objects.c:448、tests/core/test_ssa_runtime_objects.c:492、tests/core/test_ssa_runtime_objects.c:690。
 */

/**
 * @fn static TZrUInt32 zr_objects_binding_index(const SZrExecIrObjectMaterializationRequest *request, const SZrExecIrDeoptAggregate *aggregate)
 *
 * ## Summary
 * 按 aggregate 的 (typeToken, layoutId) 线性查找运行时类型绑定，供校验和工作表准备复用。
 *
 * ## Parameters
 * @param[in] request 可读请求与 typeCount 个可读绑定；地址只在调用期间借用。
 * @param[in] aggregate 非空可读配方，只读取其 typeToken、layoutId；不接管配方。
 *
 * ## Returns
 * @return 返回首个同时匹配两字段的零起点索引；没有匹配时返回 request->typeCount，零绑定表因此返回零哨兵。
 *
 * ## Constraints and lifecycle
 * 本函数不检查请求/配方为空，不验证 token 非零或绑定唯一，也不比较原型/代际。types_valid 在当前调用链先排除重复身份；若脱离此前提传入重复绑定，本函数仍只取首项。
 * 无分配和写入，不保留输入；调用方必须比较 count 哨兵后再用作下标。工作数组阶段依赖之前 graph_valid 已确认可匹配。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:405 的 graph 校验在 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:407 拒绝 count 哨兵；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:460 的工作表预存依赖该校验成功；缺失绑定有限用例 tests/core/test_ssa_runtime_objects.c:492。
 */

/**
 * @fn static const SZrMemberDescriptor *zr_objects_descriptor(const SZrExecIrRuntimeTypeBinding *binding, TZrUInt32 fieldIndex)
 *
 * ## Summary
 * 按 logical fieldIndex 查找运行时字段绑定，并返回对应原型 member descriptor 的借用地址。
 *
 * ## Parameters
 * @param[in] binding 已由 types_valid 约束的非空绑定，fields、prototype 和 descriptor 数组可读且索引关系有效。
 * @param[in] fieldIndex 要解析的逻辑字段标识；不要求连续，未找到则返回空。
 *
 * ## Returns
 * @return 返回首个匹配 fieldIndex 所指的 member descriptor；没有匹配返回 ZR_NULL，fieldCount0 也返回空。
 *
 * ## Constraints and lifecycle
 * 不自行检查 binding/prototype/数组为空、memberDescriptorIndex 越界、重复 fieldIndex 或 descriptor 类型；这些是当前调用链上游的前提。
 * 返回指针不转移所有权，受原型及 descriptor 存储的借用生命周期约束；所属存储释放、替换、重分配或布局失效后不可再用。本函数不分配、不修改或保存 binding。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:412 的 graph 校验将空返回视为失败；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:464 的名称表构造依赖此前成功；绑定索引与 descriptor 合法性由 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:333 检查；非FIELD有限拒绝用例 tests/core/test_ssa_runtime_objects.c:492。
 */

/**
 * @fn static TZrUInt32 zr_objects_identity_index(const SZrExecIrMaterializedState *logical, TZrUInt32 identity)
 *
 * ## Summary
 * 在 logical snapshot 的 aggregate 顺序中按 identityId 线性定位配方，供边校验与最终对象连边使用。
 *
 * ## Parameters
 * @param[in] logical 可读的有效 logical snapshot，其 aggregateCount 个配方必须保持有效；不接管存储。
 * @param[in] identity 需要解析的配方 identity；是否非零、是否唯一由上游验证，不在此判断。
 *
 * ## Returns
 * @return 命中时返回首个 identityId 相等的零起点索引；未命中返回 logical->aggregateCount，空表返回零哨兵。
 *
 * ## Constraints and lifecycle
 * 不执行 DFS、不递归遍历字段，也不拒绝前向边或环；不自行验证 identity 唯一。调用方在 graph 阶段拒绝 count 哨兵，运行阶段依赖该成功结果和未变的 logical。
 * 只同步读取、不分配、不改写、不保存指针；索引属于当前 logical 顺序，不能在替换/重排该 snapshot 后继续使用。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:421 的缺边检查及 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:618 的连接对象使用同一查询；上游 identity 唯一及边恰一命中在 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:132；允许环的正常图与257节点环用例 tests/core/test_ssa_runtime_objects.c:182、tests/core/test_ssa_runtime_objects.c:514。
 */

/**
 * @fn static TZrBool zr_objects_graph_valid(SZrObjectPreparation *prepared)
 *
 * ## Summary
 * 在私有 logical snapshot 成功产生后、工作数组和 GC 对象分配前，核对输出容量、运行时类型绑定、活动源单元及 aggregate 字段重放条件。
 *
 * ## Parameters
 * @param[in] prepared 非空可读的栈上准备记录；request 必须通过 request_valid，logical 必须是该请求经 MaterializeState 成功产生并保持未变的 snapshot；本函数虽接收非 const 指针但不写成员。
 *
 * ## Returns
 * @return 输出容量足够、全体绑定有效、需读取的 live 源满足 owner/tag/nonnull 条件，且每个 aggregate 有绑定、字段计数算术有界、字段有 descriptor、VALUE 源无受限所有权/resource、AGGREGATE 边命中当前 logical 时返回 ZR_TRUE；首次失败返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 依赖上游已验证 logical 的各池形状、ownerStates 与 values 的逐项对应、配方字段范围/kind、非零 valueId 与 function.valueCount 界限、identity 唯一及 live 关系；不能接受任意伪造 logical。VALUE 分支在检查 request.valueCount 前已索引 function.values[valueId-1]，因此上游 valueId 有效性是必要前提。
 * live 检查对 UNINITIALIZED/MOVED/DROPPED 先跳过，连 tag 都不读；其余只允许 INITIALIZED/UNKNOWN。但 recipe VALUE 分支仍读取相应源单元，不能把前述跳过扩大为任意 inactive 字段引用都不会读 payload；调用方必须保证配方实际引用的源具有可读有效 payload。
 * 这是有限扫描与 identity 查找，没有 DFS、拓扑排序或环拒绝；合法前向边、别名、环依赖先分配全部对象后连边得以重建。仅 type graph 的原型继承链另行拒环。
 * 本函数不分配、注册根、写入 target/prepared 或设置 diagnostic。失败由公开入口设置 MATERIALIZATION_FAILED 并走 cleanup；已有私有 logical 分配仍由外层释放，不能称整个操作此前零分配。请求地址只被同一次同步调用中的栈上 prepared 借用。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:684 成功构造 logical 后由 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:686 唯一调用；失败诊断/释放 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:730、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:731，logical 释放 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:663；上游字段/identity 规则 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:93、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:132，live 关系 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:336，配方准备 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:361；有限请求 count/capacity 早拒绝（不覆盖本函数容量分支）、缺绑定/owner/resource/inactive/环用例 tests/core/test_ssa_runtime_objects.c:504、tests/core/test_ssa_runtime_objects.c:492、tests/core/test_ssa_runtime_objects.c:596、tests/core/test_ssa_runtime_objects.c:638、tests/core/test_ssa_runtime_objects.c:727、tests/core/test_ssa_runtime_objects.c:514。
 */

/**
 * @fn static TZrBool zr_objects_allocate_work(SZrObjectPreparation *prepared)
 *
 * ## Summary
 * 在非空logical图通过验证之后，为对象指针、分离hash集合、binding索引、名称借用、root slots及旧ignore注册快照分配native工作存储。
 *
 * ## Parameters
 * @param[in,out] prepared 非空、零起点工作记录；request和logical有效、graph_valid已成功、aggregateCount非零，六个工作指针不得持有未释放旧分配。
 *
 * ## Returns
 * @return ignored计数/根槽数和各数组字节数均在表示界限内，所有必需calloc成功、快照及索引/名称表完成时返回ZR_TRUE；任一界限或分配失败返回ZR_FALSE。失败可已写ignoredCount及多个工作指针，调用者必须统一cleanup。
 *
 * ## Constraints and lifecycle
 * 根槽上界用UInt64累加活动候选值上限、typeCount、旧target.count、新aggregateCount、fieldCount、一个异常槽及ignoredCount，再经UInt32/字节界限检查。它不是最终实际GC根数。
 * 依次执行五次必选calloc和ignoredCount非零时的第六次，最后才统一判空，因此一次失败不阻止其他数组被分配。names在零字段时仍分配一个元素；数组只由本次记录独立拥有，本函数不回滚或free部分结果。
 * 复制ignored指针和descriptor.name只建立借用，尚未注册根；collector/IR/binding元数据须依调用协议稳定。binding与descriptor查找依赖先前graph成功，函数不再验证完整图。没有在此创建GC对象。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:693 是唯一调用，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:657 至 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:663 的相应free由 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:731 统一进入；槽数构造 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:432、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:437，六个calloc位于 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:444、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:445、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:446、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:447、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:449、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:451；tests/core/test_ssa_runtime_objects.c:569 的ordinal只替换本C直接calloc，不覆盖linked analysis/snapshot分配。
 */

/**
 * @fn static void zr_objects_root_slot(SZrObjectPreparation *prepared, TZrUInt32 offset)
 *
 * ## Summary
 * 把一个可被moving GC改写的对象指针位置追加到本次rootSlots，供当前分组稍后一次压根帧。
 *
 * ## Parameters
 * @param[in,out] prepared 非空准备记录；rootSlots已由allocate_work零初始化，slotCount所指位置在预留数组内，调用者负责保证余量。
 * @param[in] offset 相对当前分组base的字节偏移，须已在上游表示界限内并确实定位一个合适的raw对象指针单元。
 *
 * ## Returns
 * 无C返回值；写locationKind为LOCAL_ADDRESS、写frameByteOffset，并把slotCount增加一。不检查容量或返回失败状态。
 *
 * ## Constraints and lifecycle
 * LOCAL_ADDRESS使这里的frameByteOffset描述native/栈上存储，而非VM栈的FRAME_BYTE_OFFSET寻址；本函数不计算base、不登记根帧，也不验证目标地址/对齐。
 * 调用者必须保持rootSlots及分组base直到相应帧弹出；此helper不能在任意未分配记录上使用。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:497、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:503、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:508、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:512、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:516、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:520、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:524 是七种组内调用；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:477 接着按分组压帧；tests/core/test_ssa_runtime_objects.c:727 仅有限验证inactive源不被作为活动payload根处理。
 */

/**
 * @fn static TZrBool zr_objects_push_roots(SZrObjectPreparation *prepared, void *base, TZrUInt32 start)
 *
 * ## Summary
 * 把start到当前slotCount的根槽作为一个组压到state的AOT根链，仅在成功后增加frameCount，方便外层释放已成功的前缀。
 *
 * ## Parameters
 * @param[in,out] prepared 非空准备记录；rootSlots[start..slotCount)已填有效描述，start<=slotCount，非空组的frameCount小于固定七组容量。
 * @param[in,out] base 当前组的根存储基址；非空组须能按描述读取/改写对象指针并存活到pop，空组不读取该base。
 * @param[in] start 本组在rootSlots中的起始索引，通常为上一组提交后的slotCount。
 *
 * ## Returns
 * @return 组为空时直接返回ZR_TRUE而不加帧；非空组在AotRootFramePush成功后增加frameCount并返回ZR_TRUE。Push失败返回ZR_FALSE，frameCount不增，先前成功帧仍由外层cleanup弹出。
 *
 * ## Constraints and lifecycle
 * 本函数先写本帧rootMap的count/roots，再尝试Push；失败不回退这些局部描述或slotCount，不能当完整候选事务。Push借用本记录的frame/map/slots与base，不接管其所有权，也没有把全部对象长期保活到公开调用结束之后。
 * 本层不检查start范围、固定帧容量、真实base extent或root描述的语义；依赖register_roots与allocate_work的受限调用链。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:500、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:505、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:509、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:513、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:517、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:521、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:525 对应七组；实际根链操作 zr_vm_core/src/zr_vm_core/gc/gc.c:257；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:649 逆序退成功帧。tests/core/test_ssa_runtime_objects.c:182、tests/core/test_ssa_runtime_objects.c:199 仅检查有限场景最终根深度。
 */

/**
 * @fn static TZrBool zr_objects_register_roots(SZrObjectPreparation *prepared)
 *
 * ## Summary
 * 在外层MutatorEnter可能允许GC之前，登记活动源、类型原型、旧输出、新对象、字段名称、保存异常与ignored快照的七组临时根。
 *
 * ## Parameters
 * @param[in,out] prepared graph与allocate_work已成功的非空记录；savedException及环境标志已复制，frameCount/slotCount起始为零，各base在调用和后续使用期间稳定可写。
 *
 * ## Returns
 * @return 七组都成功提交（空组允许跳过）返回ZR_TRUE；首次非空组Push失败返回ZR_FALSE。失败时先前成功根帧和已填槽仍在记录中，外层必须cleanup，不能直接free存储。
 *
 * ## Constraints and lifecycle
 * 源值只对owner为INITIALIZED或UNKNOWN且单元标GC时追加payload根；inactive owner在读取源tag之前跳过。其余组按明确计数填入，名称数组中未用元素来自calloc的NULL，保存异常仅GC单元需要槽。
 * 依赖logical的valueId/owner配对和先前graph的源可读性；不是完整owner解析器，也不接管这些GC对象。本函数不分配、不进入mutator scope、不持有collection pause；它建立的借用直到cleanup退根才结束。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:698 是唯一调用，环境复制位于 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:694，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:701 在全部根建立后才进入额外RUNNING；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:649 失败/成功都退根。有限GC/环境异常/inactive场景 tests/core/test_ssa_runtime_objects.c:188、tests/core/test_ssa_runtime_objects.c:255、tests/core/test_ssa_runtime_objects.c:727。
 */

/**
 * @fn static TZrBool zr_objects_restore_ignored(SZrObjectPreparation *prepared)
 *
 * ## Summary
 * 重新登记旧ignored快照中当前已不在registry的非空对象，抵消附接写屏障传播escape时移除既有注册的影响。
 *
 * ## Parameters
 * @param[in] prepared 非空记录；ignored可为空，否则ignoredCount个快照指针在本次GC根/暂停协议下保持有效，request->state/global可使用。记录本身不在此重置。
 *
 * ## Returns
 * @return ignored为空直接返回ZR_TRUE；扫描中所有缺失成员IgnoreObject成功返回ZR_TRUE，首次重新登记失败返回ZR_FALSE。失败前已恢复的成员不会回滚。
 *
 * ## Constraints and lifecycle
 * 借用快照元素，不拥有/释放raw对象；通过IgnoreObject改变registry及对象的ignore状态。本函数不要求或恢复registry原顺序、原始字节、引用计数或全部标志，也不删除后来新增成员。
 * 调用链依赖Unignore不收缩capacity来复用旧注册容量；helper仍保留FALSE返回，不能据此把底层IgnoreObject的所有失败/分配可能性抹掉。它没有自行取得整个快照的并发事务锁，须遵循物化的GC-domain协议。
 * 正常callback在ready赋值和finish各调用一次，外层cleanup还会尽力再调用并忽略结果，因此cleanup不报告独立的ignore恢复错误。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:625、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:627、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:638 是三个直接调用；registry插入 zr_vm_core/src/zr_vm_core/gc/gc_roots.c:58，移除不收缩capacity见 zr_vm_core/src/zr_vm_core/gc/gc_object.c:777；tests/core/test_ssa_runtime_objects.c:665 只检查计数及三个样本成员在成功/失败后的资格。
 */

/**
 * @fn static void zr_objects_prepare_runtime(SZrState *state, TZrPtr arguments)
 *
 * ## Summary
 * 作为同步TryRun回调，先为全部recipe identity创建真实GC对象并预留分离hash存储，再在collection pause与必要mutation锁保护下附接原型和字段，保存共享边/前向边/环的对象身份。
 *
 * ## Parameters
 * @param[in,out] state 必须与prepared.request->state相同，处于外层已进入的GC-aware RUNNING协议；回调会创建GC对象、执行写屏障并可能抛出异常。
 * @param[in,out] arguments 必须指向本次非空SZrObjectPreparation，可信logical/graph与native工作数组、临时根已建立；仅在TryRun同步调用期间借用，不可异步保存。
 *
 * ## Returns
 * 无C返回值。普通早退通过ready仍为假及error提示失败阶段；完整附接且ignore恢复成功才将ready设真。OOM等也可通过Throw退出到TryRun，由外层status归类；ready为真还不表示target已经发布。
 *
 * ## Constraints and lifecycle
 * 先以NULL prototype逐个Object_New，避免让可移动prototype跨分配持有；全部对象在连边前均存在。随后逐图Init/预留hash桶和pair pool，集合位于稳定native数组内；这些步骤可分配并触发GC，已有根负责更新指针。
 * 进入附接阶段时先取得有界collection pause，再取得MutationBegin令牌；FALSE令牌不统一表示失败，也可表示无需锁。随后在pause内预留remembered registry容量，才读取并连接可移动raw地址。
 * 每个分离map整体赋给object.nodeMap后立即Construct原项，转移其native存储所有权。字段UNINITIALIZED不生成pair；其余使用已预留pair与名称，VALUE读取源单元，AGGREGATE按identity指向已分配对象；规范化NONE单元走现有pair写入，避免一般copy把STRUCT另行克隆。该路径不重放constructor/getter/setter或owner/resource转移。
 * 附接阶段不应发起新的GC对象分配或用户回调；真实写屏障仍执行，测试替身可在边界故意Throw。遇pair/跨域写检查失败保留部分候选，交给外层cleanup。成功/普通早退均不结束本次pause或mutation令牌，以保护发布和对象清理；Throw由TryRun恢复其入口scope；TryRun非FINE返回时外层清旧mutation令牌，cleanup必要时另取新层，不再配对已展开的旧层。此清标条件不覆盖FINE返回。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:707 唯一以函数指针交给TryRun，实际同步dispatch为 zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:80；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:549 先分配，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:587 与 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:597 后连边；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:708、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:712 处理非FINE返回后的令牌；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:731 统一cleanup。有限环/故障/用户回调/并发throw场景 tests/core/test_ssa_runtime_objects.c:182、tests/core/test_ssa_runtime_objects.c:199、tests/core/test_ssa_runtime_objects.c:232、tests/core/test_ssa_runtime_objects.c:244、tests/core/test_ssa_runtime_objects.c:375、tests/core/test_ssa_runtime_objects.c:392、tests/core/test_ssa_runtime_objects.c:514、tests/core/test_ssa_runtime_objects.c:609。
 */

/**
 * @fn static void zr_objects_cleanup(SZrObjectPreparation *prepared)
 *
 * ## Summary
 * 统一退休本次部分或完整候选的hash/native工作存储与logical快照，并在退出额外RUNNING scope之前按顺序结束临时GC保护。
 *
 * ## Parameters
 * @param[in,out] prepared 由本次外层零初始化后逐步填写的非空记录，request/state始终有效；flags和frameCount须准确反映本次已取得的资源，不能传入任意旧指针或重复清理。
 *
 * ## Returns
 * 无C返回值。必要时在仍持collection pause下重新取得mutation令牌；尽力恢复ignored成员，析构分离map；ready为假时析构已分配新对象的内部存储并清缓存。随后配对MutationEnd、逆序弹已成功根帧、结束本次pause，free六工作数组并MaterializedStateFree(logical)。
 *
 * ## Constraints and lifecycle
 * TryRun返回非FINE时，外层将异常展开已消耗的旧callback mutation令牌标假，cleanup可在仍持pause时按需取得新层，再配对当前令牌；FINE返回不进入外层清标分支，不能把任意Throw都视为旧令牌已标假。MutationBegin的FALSE也可能表示不需要加锁。
 * map项在转移到object.nodeMap后已Construct为空，因此两路析构不重复free同一hash存储。ready为真时对象内部存储留给成功图；ready为假时Deconstruct不等于手工释放GC对象本体，新raw对象仍由GC管理。
 * 先结束mutation层，再退根和结束本次pause，native数组在根借用退休之后才free；本次额外RUNNING直到后续restore_scopes才撤销。Ignore恢复与RootFramePop的返回值被忽略，接口不报告这些独立失败。
 * 清理只把部分flags/counts归零，没有把六个已free指针一概置NULL，故仅可调用一次，不能随后解引用工作数组或当作可重复Free接口。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:731 唯一调用，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:734 在其后恢复scope；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:712 在非FINE返回后使旧令牌失效；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:594 避免转移map重复释放。有限旧target/throw/native分配失败断言 tests/core/test_ssa_runtime_objects.c:199、tests/core/test_ssa_runtime_objects.c:215、tests/core/test_ssa_runtime_objects.c:392、tests/core/test_ssa_runtime_objects.c:569；实际AOT退根语义 zr_vm_core/src/zr_vm_core/gc/gc.c:282。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_MaterializeObjects(const SZrExecIrObjectMaterializationRequest *request, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 把指定checkpoint的逻辑aggregate recipe恢复为真实GC对象图；候选logical、native工作表和GC对象都准备完成后才向caller的target数组发布，保持共享边与合法环的身份。
 *
 * ## Parameters
 * @param[in] request 可为空，空请求返回假；非空记录本身同步只读借用，但其state、values的可移动payload、types的prototype字段与target均须按协议可写。checkpoint.target必须为空，logical候选由内部另建；function/map/recipe/bindings及实际读取存储保持有效稳定，valueId按values的1起点索引，target的数组/容量由caller提供且可写。
 * @param[out] diagnostic 可为空；非空必须是独立可写存储，不覆盖request、function/map/IR、source/binding、target或临时根会访问的内容。下层先清零再读输入，有限alias检查不会替caller认证此约束。
 *
 * ## Returns
 * @return 成功发布指定顺序的新对象指针及aggregateCount时返回ZR_TRUE；空图成功仅清target.count为零，旧数组slot不在该分支清空。参数/入口scope/logical/graph/native分配/根注册/进入scope/runtime准备任一失败返回ZR_FALSE并在可用诊断中归类。失败不把候选图发布到target；moving GC仍可更新输入、prototype及旧输出指针，不能解释为所有字节或整个runtime回滚。
 *
 * ## Constraints and lifecycle
 * 必须在合法GC safepoint使用；拒绝非GC_AWARE或不允许的登记status。共享域有其他活动线程时，caller须先根住输入、再进入GC-aware mutator scope，并保持到返回后的输出根发布；inactive入口只适用于单mutator。state/domain及登记关系在同步调用期间保持有效，不提供任意并发detach或metadata改写同步。
 * 先预检有限跨度和旧target对象，再保存scope并用独立checkpoint.target构造logical；选用map必须通过对当前function的完整state-map校验，binding布局另由graph检查。logical成功后graph只读检查，非空图才分配工作数组、保存环境异常、注册根、进入额外RUNNING并同步TryRun。callback先分配全部对象、后受pause/锁保护附接，不重放constructor/accessor或owner/resource转移。
 * TryRun返回后还原保存的异常/线程状态；仅在返回status非FINE时把旧callback mutation令牌置假，再由cleanup按需取得新层。FINE返回不进入该分支，不能把此条件等同于任意Throw。只有status为FINE且ready时复制候选指针和count；target数组地址/capacity的所有权仍归caller，本函数不free旧GC对象或caller数组。
 * 成功对象及各借用GC指针仍受GC移动/根生命周期约束；caller须在下个safepoint前根住输出。无论成功或失败，finish统一退休已取得的临时资源后才恢复入口scope；早于资源取得的直接返回各自不留下本次候选资源。重试必须重新满足当前状态和借用约束，不保证原始raw地址不变。
 *
 * ## References
 * 普通私有声明 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_runtime.h:55；仓内原符号直接调用目前仅 tests/core/test_ssa_runtime_objects.c:183 等14处，fault别名由 tests/core/ssa_runtime_objects_faults.c:123、tests/core/ssa_runtime_objects_faults.c:131 编译同一物理定义，另有 tests/core/ssa_runtime_objects_concurrency.c:271 调用其别名；无生产直接调用搜索结果不是死码证明。共享域caller根/作用域示例 tests/core/ssa_runtime_objects_concurrency.c:257、tests/core/ssa_runtime_objects_concurrency.c:259、tests/core/ssa_runtime_objects_concurrency.c:277。有限断言 tests/core/test_ssa_runtime_objects.c:182、tests/core/test_ssa_runtime_objects.c:504、tests/core/test_ssa_runtime_objects.c:569、tests/core/test_ssa_runtime_objects.c:727；测试目标 tests/cmake/ssa-tests.cmake:304，CTest tests/cmake/ssa-tests.cmake:324。
 */
