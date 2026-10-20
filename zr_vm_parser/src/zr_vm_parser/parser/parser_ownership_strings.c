#include "parser/parser_ownership_internal.h"

#include "zr_vm_core/gc.h"

#include <stdint.h>

struct SZrParserOwnershipPinBlock {
    SZrAstOwnership *owner;
    SZrParserOwnershipPinBlock *next;
    TZrSize count;
    TZrSize preparedCount;
    TZrBool published;
    SZrGcNativeCallPin pins[];
};

static TZrSize ownership_bulk_string_hash(const SZrString *string) {
    uintptr_t address = (uintptr_t)string;
    return (TZrSize)((address >> 4u) ^ (address >> 13u));
}

static TZrBool ownership_bulk_shape_is_valid(const SZrAstOwnership *owner) {
    return owner->stringCount <= owner->stringCapacity &&
           ((owner->stringCapacity == 0u) == (owner->strings == ZR_NULL)) &&
           ((owner->slotCapacity == 0u) == (owner->slots == ZR_NULL)) &&
           (owner->slotCapacity == 0u
                    ? owner->stringCount == 0u
                    : (owner->slotCapacity & (owner->slotCapacity - 1u)) == 0u);
}

static TZrBool ownership_bulk_contains(
        SZrAstOwnership *owner, SZrString *string,
        SZrParserComptimeBudgetTransaction *transaction, TZrBool *outFound) {
    SZrParserOwnershipPinBlock *block;
    TZrSize index;

    *outFound = ZR_FALSE;
    if (owner->slotCapacity != 0u) {
        TZrSize slot = ownership_bulk_string_hash(string) & (owner->slotCapacity - 1u);
        TZrSize probes;

        for (probes = 0u; probes < owner->slotCapacity; ++probes) {
            TZrSize entry;

            if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                        transaction, ZR_PARSER_COMPTIME_BUDGET_FUEL, 1u)) {
                return ZR_FALSE;
            }
            entry = owner->slots[slot];
            if (entry == 0u) {
                break;
            }
            if (entry > owner->stringCount) {
                return ZR_FALSE;
            }
            if (owner->strings[entry - 1u].string == string) {
                *outFound = ZR_TRUE;
                return ZR_TRUE;
            }
            slot = (slot + 1u) & (owner->slotCapacity - 1u);
        }
        if (probes == owner->slotCapacity) {
            return ZR_FALSE;
        }
    }
    for (block = owner->pinBlocks; block != ZR_NULL; block = block->next) {
        if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                    transaction, ZR_PARSER_COMPTIME_BUDGET_FUEL, 1u) ||
            block->owner != owner || !block->published || block->count == 0u ||
            block->preparedCount != block->count) {
            return ZR_FALSE;
        }
        for (index = 0u; index < block->preparedCount; ++index) {
            if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                        transaction, ZR_PARSER_COMPTIME_BUDGET_FUEL, 1u)) {
                return ZR_FALSE;
            }
            if (block->pins[index].object == (SZrRawObject *)string) {
                *outFound = ZR_TRUE;
                return ZR_TRUE;
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool ownership_bulk_is_new(
        SZrAstOwnership *owner, SZrString *const *strings, TZrSize index,
        SZrParserComptimeBudgetTransaction *transaction, TZrBool *outNew) {
    TZrBool found;
    TZrSize previous;
    SZrString *string = strings[index];

    *outNew = ZR_FALSE;
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_FUEL, 1u)) {
        return ZR_FALSE;
    }
    if (string == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!ZrCore_GcDomain_ObjectBelongsToState(owner->state, (SZrRawObject *)string) ||
        !ownership_bulk_contains(owner, string, transaction, &found)) {
        return ZR_FALSE;
    }
    if (found) {
        return ZR_TRUE;
    }
    for (previous = 0u; previous < index; ++previous) {
        if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                    transaction, ZR_PARSER_COMPTIME_BUDGET_FUEL, 1u)) {
            return ZR_FALSE;
        }
        if (strings[previous] == string) {
            return ZR_TRUE;
        }
    }
    *outNew = ZR_TRUE;
    return ZR_TRUE;
}

static void ownership_bulk_free(
        SZrAstOwnership *owner, SZrParserOwnershipPinBlock *block) {
    TZrSize bytes = sizeof(*block) + block->count * sizeof(*block->pins);

    while (block->preparedCount != 0u) {
        ZrCore_Gc_NativeCallUnpin(owner->state->global,
                &block->pins[--block->preparedCount]);
    }
    ZrCore_Memory_RawFreeWithType(
            owner->state->global, block, bytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

TZrBool ZrParser_Ownership_PrepareStrings(
        SZrAstOwnership *owner, SZrString *const *strings, TZrSize count,
        SZrParserComptimeBudgetTransaction *transaction,
        SZrParserOwnershipPinBlock **outStage) {
    SZrParserOwnershipPinBlock *block;
    TZrSize index;
    TZrSize newCount = 0u;
    TZrSize bytes;

    if (outStage == ZR_NULL) {
        return ZR_FALSE;
    }
    *outStage = ZR_NULL;
    if (owner == ZR_NULL || owner->state == ZR_NULL ||
        owner->state->global == ZR_NULL || owner->refCount == 0u ||
        transaction == ZR_NULL || !transaction->active || transaction->failed ||
        transaction->budget != ZrParser_Ownership_Budget(owner) ||
        (count != 0u && strings == ZR_NULL) || count > ZR_MAX_SIZE / sizeof(*strings) ||
        !ownership_bulk_shape_is_valid(owner)) {
        return ZR_FALSE;
    }
    /* Count before allocating so each stage holds exactly the missing pins.
     * The caller keeps the owner and input pointer array unchanged until Apply. */
    for (index = 0u; index < count; ++index) {
        TZrBool isNew;

        if (!ownership_bulk_is_new(owner, strings, index, transaction, &isNew)) {
            return ZR_FALSE;
        }
        if (isNew) {
            newCount++;
        }
    }
    if (newCount == 0u) {
        return ZR_TRUE;
    }
    if (newCount > (ZR_MAX_SIZE - sizeof(*block)) / sizeof(*block->pins)) {
        return ZR_FALSE;
    }
    bytes = sizeof(*block) + newCount * sizeof(*block->pins);
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, bytes) ||
        !ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT, newCount)) {
        return ZR_FALSE;
    }
    block = ZrCore_Memory_RawMallocWithType(
            owner->state->global, bytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (block == ZR_NULL) {
        return ZR_FALSE;
    }
    block->owner = owner;
    block->next = ZR_NULL;
    block->count = newCount;
    block->preparedCount = 0u;
    block->published = ZR_FALSE;
    for (index = 0u; index < count; ++index) {
        TZrBool isNew;

        if (!ownership_bulk_is_new(owner, strings, index, transaction, &isNew)) {
            goto failed;
        }
        if (!isNew) {
            continue;
        }
        if (block->preparedCount == block->count ||
            !ZrParser_ComptimeBudget_TryConsumeTransaction(
                    transaction, ZR_PARSER_COMPTIME_BUDGET_FUEL, 1u) ||
            !ZrCore_Gc_NativeCallPinObject(owner->state, (SZrRawObject *)strings[index],
                    &block->pins[block->preparedCount])) {
            goto failed;
        }
        block->preparedCount++;
    }
    if (block->preparedCount != block->count) {
        goto failed;
    }
    *outStage = block;
    return ZR_TRUE;

failed:
    ownership_bulk_free(owner, block);
    return ZR_FALSE;
}

void ZrParser_Ownership_ApplyStrings(
        SZrAstOwnership *owner, SZrParserOwnershipPinBlock *stage) {
    if (owner == ZR_NULL || stage == ZR_NULL) {
        return;
    }
    ZR_ASSERT(stage->owner == owner && !stage->published &&
              stage->preparedCount == stage->count);
    if (stage->owner != owner || stage->published || stage->preparedCount != stage->count) {
        return;
    }
    stage->published = ZR_TRUE;
    stage->next = owner->pinBlocks;
    owner->pinBlocks = stage;
}

void ZrParser_Ownership_DiscardStrings(
        SZrAstOwnership *owner, SZrParserOwnershipPinBlock *stage) {
    if (owner == ZR_NULL || stage == ZR_NULL) {
        return;
    }
    ZR_ASSERT(stage->owner == owner && !stage->published);
    if (stage->owner == owner && !stage->published) {
        ownership_bulk_free(owner, stage);
    }
}

void ZrParser_Ownership_ReleaseStrings(SZrAstOwnership *owner) {
    if (owner == ZR_NULL) {
        return;
    }
    while (owner->pinBlocks != ZR_NULL) {
        SZrParserOwnershipPinBlock *block = owner->pinBlocks;

        owner->pinBlocks = block->next;
        ownership_bulk_free(owner, block);
    }
}
