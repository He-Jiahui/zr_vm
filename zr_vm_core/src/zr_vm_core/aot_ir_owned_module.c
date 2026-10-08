#include "zr_vm_core/aot_ir_owned_module.h"

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct SZrAotIrOwnedModule {
    SZrAotIrModule module;
};

/* MSVC C lacks max_align_t; MSVC and clang's Microsoft ABI define it as double. */
#if defined(_MSC_VER)
typedef double TOwnedModuleMaxAlign;
#else
typedef max_align_t TOwnedModuleMaxAlign;
#endif

/* Every arena object must be supported by ordinary CRT malloc alignment. */
#define OWNED_MODULE_ASSERT_ALIGNMENT(type) \
    _Static_assert(alignof(type) <= alignof(TOwnedModuleMaxAlign), \
                   #type " requires an over-aligned arena")
OWNED_MODULE_ASSERT_ALIGNMENT(SZrAotIrOwnedModule);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrAotIrModule);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrAotIrFunction);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrExecIrConstant);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrExecIrLayout);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrAotIrFrameSlot);
OWNED_MODULE_ASSERT_ALIGNMENT(TZrUInt32);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrAotIrBlock);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrAotIrInstruction);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrAotIrPhiIncoming);
OWNED_MODULE_ASSERT_ALIGNMENT(TZrExecIrMemoryTokenId);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrAotIrSourceMap);
OWNED_MODULE_ASSERT_ALIGNMENT(TZrExecIrValueId);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrExecIrDeoptState);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrExecIrDeoptAggregate);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrExecIrDeoptAggregateField);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrExecIrGcMap);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrExecIrGcMapEntry);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrExecIrStateMap);
OWNED_MODULE_ASSERT_ALIGNMENT(SZrExecIrStateMapEntry);
#undef OWNED_MODULE_ASSERT_ALIGNMENT

typedef struct SOwnedModuleCursor {
    unsigned char *arena; /* NULL measures; non-NULL copies and rebinds. */
    size_t capacity;
    size_t used;
} SOwnedModuleCursor;

/* Transactional geometry: failures leave both outputs unchanged. A zero-count
 * span consumes no alignment padding and reports the current cursor position. */
static TZrBool owned_module_checked_span(
        size_t *cursor, size_t count, size_t width, size_t alignment,
        size_t *offset) {
    size_t bytes, padding, start;
    if (cursor == ZR_NULL || offset == ZR_NULL || width == 0u ||
        alignment == 0u || (alignment & (alignment - 1u)) != 0u ||
        alignment > alignof(TOwnedModuleMaxAlign)) return ZR_FALSE;
    if (count == 0u) {
        *offset = *cursor;
        return ZR_TRUE;
    }
    if (count > SIZE_MAX / width) return ZR_FALSE;
    bytes = count * width;
    padding = (alignment - (*cursor & (alignment - 1u))) & (alignment - 1u);
    if (padding > SIZE_MAX - *cursor) return ZR_FALSE;
    start = *cursor + padding;
    if (bytes > SIZE_MAX - start) return ZR_FALSE;
    *offset = start;
    *cursor = start + bytes;
    return ZR_TRUE;
}

#if defined(ZR_TEST_SSA_AOT_OWNED_MODULE_FAULTS)
/* Private symbols exist only in the independently shimmed test object. */
TZrBool ZrTests_AotOwnedModule_CheckedSpan(
        size_t *cursor, size_t count, size_t width, size_t alignment,
        size_t *offset) {
    return owned_module_checked_span(cursor, count, width, alignment, offset);
}

size_t ZrTests_AotOwnedModule_AlignmentBound(void) {
    return alignof(TOwnedModuleMaxAlign);
}
#endif

/* out is a local void* slot, never an alias of a typed pointer field. The same
 * checked span helper drives measurement and placement, including byte extents. */
static TZrBool owned_module_copy_span(
        SOwnedModuleCursor *cursor, const void *source, size_t count,
        size_t width, size_t alignment, void **out) {
    size_t end = cursor->used, offset;
    if (!owned_module_checked_span(&end, count, width, alignment, &offset) ||
        end > cursor->capacity) return ZR_FALSE;
    *out = ZR_NULL;
    if (cursor->arena != ZR_NULL && count != 0u) {
        *out = cursor->arena + offset;
        if (source != ZR_NULL) memcpy(*out, source, end - offset);
    }
    cursor->used = end;
    return ZR_TRUE;
}

/* Lists are visited once by this shared measure/fill traversal. memcpy retains
 * inline facts and record padding; every reachable pointer is then rebound. */
#define OWNED_MODULE_COPY_POOL(cursor, source, destination, field, countField, type) \
    do { \
        void *copiedPool; \
        if (!owned_module_copy_span((cursor), (source)->field, \
                (size_t)(source)->countField, sizeof(type), alignof(type), \
                &copiedPool)) return ZR_FALSE; \
        if ((destination) != ZR_NULL) (destination)->field = (type *)copiedPool; \
    } while (0)

static TZrBool owned_module_copy_gc_map(
        SOwnedModuleCursor *cursor, const SZrExecIrGcMap *source,
        SZrExecIrGcMap **out) {
    void *copiedHeader;
    SZrExecIrGcMap *map;
    if (!owned_module_copy_span(cursor, source, 1u, sizeof(*source),
            alignof(SZrExecIrGcMap), &copiedHeader)) return ZR_FALSE;
    map = (SZrExecIrGcMap *)copiedHeader;
    OWNED_MODULE_COPY_POOL(cursor, source, map, entries, entryCount, SZrExecIrGcMapEntry);
    OWNED_MODULE_COPY_POOL(cursor, source, map, slotIndexPool, slotIndexCount, TZrUInt32);
    OWNED_MODULE_COPY_POOL(cursor, source, map, inlineRefOffsetPool,
                           inlineRefOffsetCount, TZrUInt32);
    if (map != ZR_NULL) {
        map->entryCapacity = map->entryCount;
        map->slotIndexCapacity = map->slotIndexCount;
        map->inlineRefOffsetCapacity = map->inlineRefOffsetCount;
    }
    *out = map;
    return ZR_TRUE;
}

static TZrBool owned_module_copy_state_map(
        SOwnedModuleCursor *cursor, const SZrExecIrStateMap *source,
        SZrExecIrStateMap **out) {
    void *copiedHeader;
    SZrExecIrStateMap *map;
    if (!owned_module_copy_span(cursor, source, 1u, sizeof(*source),
            alignof(SZrExecIrStateMap), &copiedHeader)) return ZR_FALSE;
    map = (SZrExecIrStateMap *)copiedHeader;
    OWNED_MODULE_COPY_POOL(cursor, source, map, entries, entryCount, SZrExecIrStateMapEntry);
    OWNED_MODULE_COPY_POOL(cursor, source, map, valuePool, valueCount, TZrExecIrValueId);
    OWNED_MODULE_COPY_POOL(cursor, source, map, rootPool, rootCount, TZrExecIrValueId);
    OWNED_MODULE_COPY_POOL(cursor, source, map, ownerStatePool, ownerStateCount, TZrUInt32);
    if (map != ZR_NULL) {
        map->entryCapacity = map->entryCount;
        map->valueCapacity = map->valueCount;
        map->rootCapacity = map->rootCount;
        map->ownerStateCapacity = map->ownerStateCount;
    }
    *out = map;
    return ZR_TRUE;
}

static TZrBool owned_module_copy_function(
        SOwnedModuleCursor *cursor, const SZrAotIrFunction *source,
        SZrAotIrFunction *function) {
    SZrExecIrGcMap *gcMap = ZR_NULL;
    SZrExecIrStateMap *stateMap = ZR_NULL;
    OWNED_MODULE_COPY_POOL(cursor, source, function, frameSlots, frameSlotCount, SZrAotIrFrameSlot);
    OWNED_MODULE_COPY_POOL(cursor, source, function, valueSlotPool, valueSlotCount, TZrUInt32);
    OWNED_MODULE_COPY_POOL(cursor, source, function, blocks, blockCount, SZrAotIrBlock);
    OWNED_MODULE_COPY_POOL(cursor, source, function, instructions, instructionCount, SZrAotIrInstruction);
    OWNED_MODULE_COPY_POOL(cursor, source, function, operandPool, operandCount, TZrUInt32);
    OWNED_MODULE_COPY_POOL(cursor, source, function, resultPool, resultCount, TZrUInt32);
    OWNED_MODULE_COPY_POOL(cursor, source, function, phiIncomingPool, phiIncomingCount, SZrAotIrPhiIncoming);
    OWNED_MODULE_COPY_POOL(cursor, source, function, successorPool, successorCount, TZrUInt32);
    OWNED_MODULE_COPY_POOL(cursor, source, function, memoryTokenPool, memoryTokenCount, TZrExecIrMemoryTokenId);
    OWNED_MODULE_COPY_POOL(cursor, source, function, sourceMaps, sourceMapCount, SZrAotIrSourceMap);
    OWNED_MODULE_COPY_POOL(cursor, source, function, gcRootPool, gcRootCount, TZrExecIrValueId);
    OWNED_MODULE_COPY_POOL(cursor, source, function, deoptStates, deoptStateCount, SZrExecIrDeoptState);
    OWNED_MODULE_COPY_POOL(cursor, source, function, deoptValuePool, deoptValueCount, TZrExecIrValueId);
    OWNED_MODULE_COPY_POOL(cursor, source, function, deoptAggregates, deoptAggregateCount, SZrExecIrDeoptAggregate);
    OWNED_MODULE_COPY_POOL(cursor, source, function, deoptAggregateFields,
                           deoptAggregateFieldCount, SZrExecIrDeoptAggregateField);
    if (source->gcMap != ZR_NULL &&
        !owned_module_copy_gc_map(cursor, source->gcMap, &gcMap)) return ZR_FALSE;
    if (source->logicalStateMap != ZR_NULL &&
        !owned_module_copy_state_map(cursor, source->logicalStateMap, &stateMap)) return ZR_FALSE;
    if (function != ZR_NULL) {
        function->gcMap = gcMap;
        function->logicalStateMap = stateMap;
    }
    return ZR_TRUE;
}

static TZrBool owned_module_layout(
        const SZrAotIrModule *source, unsigned char *arena, size_t capacity,
        size_t *totalBytes) {
    SOwnedModuleCursor cursor = {arena, capacity, 0u};
    void *copiedHeader, *copiedFunctions;
    SZrAotIrOwnedModule *owner;
    SZrAotIrModule *module;
    SZrAotIrFunction *functions;
    if (!owned_module_copy_span(&cursor, ZR_NULL, 1u, sizeof(SZrAotIrOwnedModule),
            alignof(SZrAotIrOwnedModule), &copiedHeader)) return ZR_FALSE;
    owner = (SZrAotIrOwnedModule *)copiedHeader;
    module = owner == ZR_NULL ? ZR_NULL : &owner->module;
    if (module != ZR_NULL) memcpy(module, source, sizeof(*module));
    if (!owned_module_copy_span(&cursor, source->functions, (size_t)source->functionCount,
            sizeof(SZrAotIrFunction), alignof(SZrAotIrFunction),
            &copiedFunctions)) return ZR_FALSE;
    functions = (SZrAotIrFunction *)copiedFunctions;
    if (module != ZR_NULL) module->functions = functions;
    OWNED_MODULE_COPY_POOL(&cursor, source, module, constantPool, constantCount, SZrExecIrConstant);
    OWNED_MODULE_COPY_POOL(&cursor, source, module, layoutPool, layoutCount, SZrExecIrLayout);
    for (size_t i = 0u; i < (size_t)source->functionCount; ++i) {
        if (!owned_module_copy_function(&cursor, &source->functions[i],
                functions == ZR_NULL ? ZR_NULL : &functions[i])) return ZR_FALSE;
    }
    *totalBytes = cursor.used;
    return ZR_TRUE;
}
#undef OWNED_MODULE_COPY_POOL

static EZrAotIrStatus owned_module_fail(
        EZrAotIrStatus status, SZrAotIrDiagnostic *diagnostic, size_t expected) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
        diagnostic->expected = (TZrUInt64)expected;
    }
    return status;
}

EZrAotIrStatus ZrCore_AotIr_CloneOwnedModule(
        const SZrAotIrModule *source, SZrAotIrOwnedModule **destination,
        SZrAotIrDiagnostic *diagnostic) {
    EZrAotIrStatus status;
    SZrAotIrOwnedModule *candidate, *old;
    size_t totalBytes, filledBytes;
    if (source == ZR_NULL || destination == ZR_NULL)
        return owned_module_fail(ZR_AOT_IR_INVALID_ARGUMENT, diagnostic, 0u);
    status = ZrCore_AotIr_ValidateModule(source, diagnostic);
    if (status != ZR_AOT_IR_OK) return status;
    if (!owned_module_layout(source, ZR_NULL, SIZE_MAX, &totalBytes))
        return owned_module_fail(ZR_AOT_IR_INVALID_RANGE, diagnostic, 0u);
    candidate = (SZrAotIrOwnedModule *)malloc(totalBytes);
    if (candidate == ZR_NULL)
        return owned_module_fail(ZR_AOT_IR_OUT_OF_MEMORY, diagnostic, totalBytes);
    memset(candidate, 0, totalBytes);
    if (!owned_module_layout(source, (unsigned char *)candidate, totalBytes, &filledBytes) ||
        filledBytes != totalBytes) {
        free(candidate);
        return owned_module_fail(ZR_AOT_IR_INVALID_RANGE, diagnostic, totalBytes);
    }
    status = ZrCore_AotIr_ValidateModule(&candidate->module, diagnostic);
    if (status != ZR_AOT_IR_OK) {
        free(candidate);
        return status;
    }
    /* All source reads and validation precede publication, even for self-source. */
    old = *destination;
    *destination = candidate;
    if (old != ZR_NULL) free(old);
    return ZR_AOT_IR_OK;
}

const SZrAotIrModule *ZrCore_AotIr_OwnedModuleView(const SZrAotIrOwnedModule *owner) {
    return owner == ZR_NULL ? ZR_NULL : &owner->module;
}

void ZrCore_AotIr_OwnedModuleFree(SZrAotIrOwnedModule **owner) {
    if (owner == ZR_NULL || *owner == ZR_NULL) return;
    free(*owner);
    *owner = ZR_NULL;
}
