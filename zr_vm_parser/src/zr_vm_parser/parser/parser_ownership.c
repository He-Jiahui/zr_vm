#include "parser/parser_ownership.h"
#include "parser/parser_ownership_internal.h"
#include "parser/parser_range_evidence.h"
#include "syntax/token_stream_internal.h"
#include "syntax/source_identity.h"

#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"

#include <stdint.h>
#include <string.h>

enum {
    ZR_PARSER_OWNED_STRING_INITIAL_CAPACITY = 16,
    ZR_PARSER_OWNED_STRING_HASH_INITIAL_CAPACITY = 32
};

typedef struct SZrParserOwnershipStringStage {
    SZrParserOwnedString *strings;
    TZrSize stringCapacity;
    TZrSize *slots;
    TZrSize slotCapacity;
    TZrSize slot;
    SZrGcRootHandle root;
    TZrBool rootCreated;
    TZrBool duplicate;
} SZrParserOwnershipStringStage;

static SZrParserComptimeBudget *ownership_budget(
        SZrAstOwnership *owner) {
    if (owner == ZR_NULL) {
        return ZR_NULL;
    }
    return owner->externalBudget != ZR_NULL
                   ? owner->externalBudget
                   : &owner->fallbackBudget;
}

SZrParserComptimeBudget *ZrParser_Ownership_Budget(
        SZrAstOwnership *owner) {
    return ownership_budget(owner);
}

void ZrParser_Ownership_DetachBudget(SZrAstOwnership *owner) {
    if (owner == ZR_NULL || owner->externalBudget == ZR_NULL) {
        return;
    }
    owner->fallbackBudget = *owner->externalBudget;
    owner->externalBudget = ZR_NULL;
}

static TZrSize ownership_string_hash(const SZrString *string) {
    uintptr_t address = (uintptr_t)string;
    return (TZrSize)((address >> 4u) ^ (address >> 13u));
}

static void ownership_unbounded_budget(SZrParserComptimeBudget *budget) {
    SZrParserComptimeBudgetLimits limits = {0};

    limits.fuel = ~(TZrUInt64)0;
    limits.callDepth = ~(TZrUInt64)0;
    limits.heapBytes = ~(TZrUInt64)0;
    limits.aggregateCount = ~(TZrUInt64)0;
    limits.generatedDeclarationCount = ~(TZrUInt64)0;
    limits.diagnosticCount = ~(TZrUInt64)0;
    limits.tokenCount = ~(TZrUInt64)0;
    limits.nodeCount = ~(TZrUInt64)0;
    limits.matchingCount = ~(TZrUInt64)0;
    limits.expansionCount = ~(TZrUInt64)0;
    ZrParser_ComptimeBudget_Init(budget, &limits);
}

static TZrBool ownership_budget_is_zero(
        const SZrParserComptimeBudget *budget) {
    const SZrParserComptimeBudgetLimits *limits;

    if (budget == ZR_NULL) {
        return ZR_TRUE;
    }
    limits = &budget->limits;
    return limits->fuel == 0u && limits->callDepth == 0u &&
           limits->heapBytes == 0u && limits->aggregateCount == 0u &&
           limits->generatedDeclarationCount == 0u &&
           limits->diagnosticCount == 0u && limits->tokenCount == 0u &&
           limits->nodeCount == 0u && limits->matchingCount == 0u &&
           limits->expansionCount == 0u;
}

static TZrBool ownership_string_vm_bytes(
        TZrSize length, TZrSize *outBytes) {
    TZrSize bytes;

    if (outBytes == ZR_NULL) {
        return ZR_FALSE;
    }
    bytes = sizeof(SZrString);
    if (length <= ZR_VM_SHORT_STRING_MAX) {
        if (ZR_VM_SHORT_STRING_MAX > ZR_MAX_SIZE - bytes) {
            return ZR_FALSE;
        }
        bytes += ZR_VM_SHORT_STRING_MAX;
    } else {
        if (length == ZR_MAX_SIZE || length + 1u > ZR_MAX_SIZE - bytes) {
            return ZR_FALSE;
        }
        bytes += length + 1u;
    }
    *outBytes = bytes;
    return ZR_TRUE;
}

static void ownership_free(SZrAstOwnership *owner, void *memory, TZrSize bytes) {
    if (memory != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(owner->state->global, memory, bytes,
                                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
}

SZrAstOwnership *ZrParser_Ownership_Create(
        SZrState *state,
        const SZrTokenStream *stream,
        SZrParserComptimeBudget *budget) {
    SZrAstOwnership *owner;
    SZrParserComptimeBudget fallback;
    SZrParserComptimeBudget *chargeBudget;
    SZrParserComptimeBudgetTransaction transaction;
    TZrUInt64 ownerKey;
    TZrUInt64 sourceOrigin = 0U;
    if (state == ZR_NULL || state->global == ZR_NULL ||
        (stream != ZR_NULL &&
         (stream->global != state->global || !stream->sealed)) ||
        !ZrParser_SourceIdentity_Issue(&ownerKey) ||
        (stream == ZR_NULL && !ZrParser_SourceIdentity_Issue(&sourceOrigin))) {
        return ZR_NULL;
    }
    if (stream != ZR_NULL) {
        fallback = stream->fallbackBudget;
        /* Streams constructed against an external budget still carry a
         * usable snapshot in normal operation.  Keep a defensive copy path
         * for older stream builders that left fallbackBudget zeroed. */
        if (ownership_budget_is_zero(&fallback) && stream->budget != ZR_NULL) {
            fallback = *stream->budget;
        }
    } else {
        /* Ordinary source parsing is outside comptime and must not inherit
         * the macro heap cap merely because it uses this owner. */
        ownership_unbounded_budget(&fallback);
    }
    chargeBudget = budget != ZR_NULL ? budget : &fallback;
    ZrParser_ComptimeBudget_BeginTransaction(chargeBudget, &transaction);
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                &transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                sizeof(*owner))) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_NULL;
    }
    owner = ZrCore_Memory_RawMallocWithType(
            state->global, sizeof(*owner), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (owner == ZR_NULL) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_NULL;
    }
    if (!ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        ZrCore_Memory_RawFreeWithType(
                state->global, owner, sizeof(*owner),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_NULL;
    }
    memset(owner, 0, sizeof(*owner));
    owner->state = state;
    owner->refCount = 1u;
    owner->stream = stream;
    owner->ownerKey = ownerKey;
    owner->sourceOrigin = sourceOrigin;
    owner->externalBudget = budget;
    owner->fallbackBudget = fallback;
    if (stream != ZR_NULL) {
        ZrParser_TokenStream_Retain((SZrTokenStream *)stream);
    }
    return owner;
}

void ZrParser_Ownership_Retain(SZrAstOwnership *owner) {
    if (owner != ZR_NULL && owner->refCount != 0u) {
        ZR_ASSERT(owner->refCount < ZR_MAX_SIZE);
        if (owner->refCount < ZR_MAX_SIZE) {
            owner->refCount++;
        }
    }
}

TZrUInt64 ZrParser_Ownership_Key(const SZrAstOwnership *owner) {
    return owner != ZR_NULL ? owner->ownerKey : 0U;
}

TZrUInt64 ZrParser_Ownership_SourceOrigin(const SZrAstOwnership *owner) {
    return owner != ZR_NULL ? owner->sourceOrigin : 0U;
}

SZrParserRangeEvidence *ZrParser_Ownership_Evidence(const SZrAstOwnership *owner) {
    return owner != ZR_NULL ? owner->evidence : ZR_NULL;
}

TZrBool ZrParser_Ownership_PublishEvidence(
        SZrAstOwnership *owner, SZrParserRangeEvidence *evidence) {
    if (owner == ZR_NULL || evidence == ZR_NULL || owner->evidence != ZR_NULL) {
        return ZR_FALSE;
    }
    owner->evidence = evidence;
    return ZR_TRUE;
}

void ZrParser_Ownership_Release(SZrAstOwnership *owner) {
    TZrSize i;
    if (owner == ZR_NULL || owner->refCount == 0u ||
        --owner->refCount != 0u) {
        return;
    }
    ZrParser_Ownership_ReleaseStrings(owner);
    for (i = 0u; i < owner->stringCount; ++i) {
        ZrCore_GcRootHandle_Release(owner->state, &owner->strings[i].root);
    }
    ZrParser_RangeEvidence_Free(owner->evidence);
    ownership_free(owner, owner->strings,
                   owner->stringCapacity * sizeof(*owner->strings));
    ownership_free(owner, owner->slots,
                   owner->slotCapacity * sizeof(*owner->slots));
    ZrParser_TokenStream_Release((SZrTokenStream *)owner->stream);
    ownership_free(owner, owner, sizeof(*owner));
}

static void ownership_string_stage_reset(
        SZrParserOwnershipStringStage *stage) {
    if (stage != ZR_NULL) {
        memset(stage, 0, sizeof(*stage));
    }
}

static void ownership_string_stage_discard(
        SZrAstOwnership *owner,
        SZrParserOwnershipStringStage *stage) {
    if (owner == ZR_NULL || stage == ZR_NULL) {
        return;
    }
    if (stage->rootCreated) {
        ZrCore_GcRootHandle_Release(owner->state, &stage->root);
        stage->rootCreated = ZR_FALSE;
    }
    if (stage->strings != owner->strings) {
        ownership_free(owner, stage->strings,
                       stage->stringCapacity * sizeof(*stage->strings));
    }
    if (stage->slots != owner->slots) {
        ownership_free(owner, stage->slots,
                       stage->slotCapacity * sizeof(*stage->slots));
    }
    ownership_string_stage_reset(stage);
}

static void ownership_string_stage_apply(
        SZrAstOwnership *owner,
        SZrParserOwnershipStringStage *stage,
        SZrString *string) {
    if (owner == ZR_NULL || stage == ZR_NULL || stage->duplicate) {
        return;
    }
    if (stage->strings != owner->strings) {
        ownership_free(owner, owner->strings,
                       owner->stringCapacity * sizeof(*stage->strings));
    }
    if (stage->slots != owner->slots) {
        ownership_free(owner, owner->slots,
                       owner->slotCapacity * sizeof(*stage->slots));
    }
    owner->strings = stage->strings;
    owner->stringCapacity = stage->stringCapacity;
    owner->slots = stage->slots;
    owner->slotCapacity = stage->slotCapacity;
    owner->strings[owner->stringCount].string = string;
    owner->strings[owner->stringCount].root = stage->root;
    stage->rootCreated = ZR_FALSE;
    owner->slots[stage->slot] = ++owner->stringCount;
    ownership_string_stage_reset(stage);
}

/* Stage one owned string against a caller-owned transaction.  No owner field
 * is changed until the caller commits and applies the returned stage. */
static TZrBool ownership_hold_string_inner(
        SZrAstOwnership *owner,
        SZrString *string,
        SZrParserComptimeBudgetTransaction *transaction,
        SZrParserOwnershipStringStage *stage) {
    TZrSize stringCapacity;
    TZrSize slotCapacity;
    TZrSize slot;
    TZrSize index;

    if (stage == ZR_NULL) {
        return ZR_FALSE;
    }
    ownership_string_stage_reset(stage);
    if (string == ZR_NULL) {
        stage->duplicate = ZR_TRUE;
        return ZR_TRUE;
    }
    if (owner == ZR_NULL || owner->state == ZR_NULL ||
        owner->state->global == ZR_NULL || transaction == ZR_NULL ||
        owner->stringCount == ZR_MAX_SIZE) {
        return ZR_FALSE;
    }
    if (owner->slotCapacity != 0u) {
        slot = ownership_string_hash(string) & (owner->slotCapacity - 1u);
        while (owner->slots[slot] != 0u) {
            if (owner->strings[owner->slots[slot] - 1u].string == string) {
                stage->duplicate = ZR_TRUE;
                return ZR_TRUE;
            }
            slot = (slot + 1u) & (owner->slotCapacity - 1u);
        }
    }
    stringCapacity = owner->stringCapacity;
    slotCapacity = owner->slotCapacity;
    if (owner->stringCount == stringCapacity) {
        if (stringCapacity > ZR_MAX_SIZE / 2u) {
            return ZR_FALSE;
        }
        stringCapacity = stringCapacity == 0u
                                 ? ZR_PARSER_OWNED_STRING_INITIAL_CAPACITY
                                 : stringCapacity * 2u;
    }
    if (slotCapacity == 0u || owner->stringCount + 1u > slotCapacity / 2u) {
        if (slotCapacity > ZR_MAX_SIZE / 2u) {
            return ZR_FALSE;
        }
        slotCapacity = slotCapacity == 0u
                               ? ZR_PARSER_OWNED_STRING_HASH_INITIAL_CAPACITY
                               : slotCapacity * 2u;
    }
    if (stringCapacity > ZR_MAX_SIZE / sizeof(*stage->strings) ||
        slotCapacity > ZR_MAX_SIZE / sizeof(*stage->slots)) {
        return ZR_FALSE;
    }
    stage->strings = owner->strings;
    stage->stringCapacity = stringCapacity;
    stage->slots = owner->slots;
    stage->slotCapacity = slotCapacity;
    if (stringCapacity != owner->stringCapacity) {
        if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                    transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                    stringCapacity * sizeof(*stage->strings))) {
            return ZR_FALSE;
        }
        stage->strings = ZrCore_Memory_RawMallocWithType(
                owner->state->global, stringCapacity * sizeof(*stage->strings),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (stage->strings == ZR_NULL) {
            return ZR_FALSE;
        }
        if (owner->stringCount != 0u) {
            memcpy(stage->strings, owner->strings,
                   owner->stringCount * sizeof(*stage->strings));
        }
    }
    if (slotCapacity != owner->slotCapacity) {
        if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                    transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                    slotCapacity * sizeof(*stage->slots))) {
            return ZR_FALSE;
        }
        stage->slots = ZrCore_Memory_RawMallocWithType(
                owner->state->global, slotCapacity * sizeof(*stage->slots),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (stage->slots == ZR_NULL) {
            return ZR_FALSE;
        }
        memset(stage->slots, 0, slotCapacity * sizeof(*stage->slots));
        for (index = 0u; index < owner->stringCount; ++index) {
            slot = ownership_string_hash(stage->strings[index].string) &
                   (slotCapacity - 1u);
            while (stage->slots[slot] != 0u) {
                slot = (slot + 1u) & (slotCapacity - 1u);
            }
            stage->slots[slot] = index + 1u;
        }
    }
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT, 1u) ||
        !ZrCore_GcRootHandle_Create(
                owner->state, (SZrRawObject *)string, &stage->root)) {
        return ZR_FALSE;
    }
    stage->rootCreated = ZR_TRUE;
    stage->slot = ownership_string_hash(string) & (slotCapacity - 1u);
    while (stage->slots[stage->slot] != 0u) {
        stage->slot = (stage->slot + 1u) & (slotCapacity - 1u);
    }
    return ZR_TRUE;
}

TZrBool ZrParser_Ownership_HoldString(SZrAstOwnership *owner, SZrString *string) {
    SZrParserComptimeBudgetTransaction transaction;
    SZrParserOwnershipStringStage stage;

    if (string == ZR_NULL) {
        return ZR_TRUE;
    }
    if (owner == ZR_NULL || ownership_budget(owner) == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrParser_ComptimeBudget_BeginTransaction(
            ownership_budget(owner), &transaction);
    if (!ownership_hold_string_inner(owner, string, &transaction, &stage)) {
        ownership_string_stage_discard(owner, &stage);
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_FALSE;
    }
    if (!ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        ownership_string_stage_discard(owner, &stage);
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_FALSE;
    }
    if (!stage.duplicate) {
        ZrCore_GarbageCollector_PinObject(
                owner->state, (SZrRawObject *)string,
                ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
        ownership_string_stage_apply(owner, &stage, string);
    }
    return ZR_TRUE;
}

void *ZrParser_Ownership_Allocate(
        SZrAstOwnership *owner, TZrSize bytes, EZrMemoryNativeType type,
        TZrBool astNode) {
    SZrParserComptimeBudgetTransaction transaction;
    void *memory;
    SZrParserComptimeBudget *budget;
    if (owner == ZR_NULL || bytes == 0u ||
        (budget = ownership_budget(owner)) == ZR_NULL) {
        return ZR_NULL;
    }
    ZrParser_ComptimeBudget_BeginTransaction(budget, &transaction);
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                &transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, bytes) ||
        (astNode && !ZrParser_ComptimeBudget_TryConsumeTransaction(
                            &transaction, ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT, 1u))) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_NULL;
    }
    memory = ZrCore_Memory_RawMallocWithType(owner->state->global, bytes, type);
    if (memory == ZR_NULL ||
        !ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        if (memory != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(owner->state->global, memory, bytes, type);
        }
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_NULL;
    }
    return memory;
}

TZrBool ZrParser_Lexer_HoldString(SZrLexState *lexer, SZrString *string) {
    if (lexer == ZR_NULL ||
        !ZrParser_Ownership_HoldString(lexer->ownership, string)) {
        if (lexer != ZR_NULL) {
            lexer->currentTokenHadError = ZR_TRUE;
            lexer->currentTokenErrorMessage = "Parser string ownership budget exceeded";
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

SZrString *ZrParser_Lexer_CreateString(
        SZrLexState *lexer, const TZrChar *bytes, TZrSize length) {
    SZrString *string;
    SZrParserComptimeBudget *budget;
    SZrParserComptimeBudgetTransaction transaction;
    SZrParserOwnershipStringStage stage;
    TZrSize charge;
    ownership_string_stage_reset(&stage);
    if (lexer == ZR_NULL || lexer->ownership == ZR_NULL || bytes == ZR_NULL ||
        !ownership_string_vm_bytes(length, &charge) ||
        (budget = ownership_budget(lexer->ownership)) == ZR_NULL) {
        if (lexer != ZR_NULL) {
            lexer->currentTokenHadError = ZR_TRUE;
            lexer->currentTokenErrorMessage = "Parser string allocation budget exceeded";
        }
        return ZR_NULL;
    }
    ZrParser_ComptimeBudget_BeginTransaction(budget, &transaction);
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                &transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, charge)) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        lexer->currentTokenHadError = ZR_TRUE;
        lexer->currentTokenErrorMessage = "Parser string allocation budget exceeded";
        return ZR_NULL;
    }
    string = ZrCore_String_Create(lexer->state, (TZrNativeString)bytes, length);
    if (string == ZR_NULL ||
        !ownership_hold_string_inner(
                lexer->ownership, string, &transaction, &stage) ||
        !ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        ownership_string_stage_discard(lexer->ownership, &stage);
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        lexer->currentTokenHadError = ZR_TRUE;
        lexer->currentTokenErrorMessage = "Parser string allocation budget exceeded";
        return ZR_NULL;
    }
    if (!stage.duplicate) {
        ZrCore_GarbageCollector_PinObject(
                lexer->state, (SZrRawObject *)string,
                ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
        ownership_string_stage_apply(lexer->ownership, &stage, string);
    }
    return string;
}
