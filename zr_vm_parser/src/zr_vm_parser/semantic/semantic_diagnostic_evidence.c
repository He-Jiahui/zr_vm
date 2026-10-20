#include "semantic/semantic_diagnostic_evidence.h"

#include "parser/parser_ownership.h"
#include "parser/parser_ownership_strings.h"
#include "parser/parser_range_evidence.h"
#include "zr_vm_core/gc_domain.h"

#include <string.h>

typedef struct SZrDiagnosticEvidenceStage {
    SZrSemanticDiagnosticFact fact;
    SZrArray related;
    SZrArray fixes;
    SZrArray facts;
    SZrString **strings;
    TZrSize stringCapacity;
    TZrSize stringCount;
    SZrParserOwnershipPinBlock *pins;
} SZrDiagnosticEvidenceStage;

static TZrBool evidence_work(SZrParserComptimeBudgetTransaction *transaction, TZrSize work) {
    return ZrParser_ComptimeBudget_TryConsumeTransaction(
            transaction, ZR_PARSER_COMPTIME_BUDGET_FUEL, (TZrUInt64)work);
}

static TZrBool evidence_add_size(TZrSize left, TZrSize right, TZrSize *out) {
    if (right > ZR_MAX_SIZE - left) {
        return ZR_FALSE;
    }
    *out = left + right;
    return ZR_TRUE;
}

static TZrBool evidence_array_valid(const SZrArray *array, TZrSize elementSize, TZrBool required) {
    if (!array->isValid) {
        return !required && array->head == ZR_NULL && array->length == 0U &&
                array->capacity == 0U && array->elementSize == 0U;
    }
    return array->head != ZR_NULL && array->elementSize == elementSize &&
            array->capacity != 0U && array->length <= array->capacity &&
            array->capacity <= ZR_MAX_SIZE / elementSize;
}

static TZrBool evidence_diagnostic_valid(SZrParserComptimeBudgetTransaction *transaction,
        const SZrStructuredDiagnostic *diagnostic) {
    TZrSize index;

    if (!evidence_work(transaction, 1U) || diagnostic->code == ZR_NULL ||
            diagnostic->message == ZR_NULL ||
            (TZrUInt32)diagnostic->severity > ZR_STRUCTURED_DIAGNOSTIC_HINT ||
            (TZrUInt32)diagnostic->noFixReason > ZR_DIAGNOSTIC_NO_FIX_REASON_UNSAFE_EDIT ||
            !evidence_array_valid(&diagnostic->relatedInformation,
                    sizeof(SZrStructuredDiagnosticRelatedInformation), ZR_FALSE) ||
            !evidence_array_valid(&diagnostic->fixes, sizeof(SZrStructuredDiagnosticFix), ZR_FALSE) ||
            (diagnostic->fixes.length == 0U
                    ? diagnostic->noFixReason == ZR_DIAGNOSTIC_NO_FIX_REASON_UNSPECIFIED
                    : diagnostic->noFixReason != ZR_DIAGNOSTIC_NO_FIX_REASON_UNSPECIFIED)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < diagnostic->relatedInformation.length; ++index) {
        const SZrStructuredDiagnosticRelatedInformation *related =
                (const SZrStructuredDiagnosticRelatedInformation *)(diagnostic->relatedInformation.head +
                        index * sizeof(*related));
        if (!evidence_work(transaction, 1U) || related->message == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    for (index = 0U; index < diagnostic->fixes.length; ++index) {
        const SZrStructuredDiagnosticFix *fix = (const SZrStructuredDiagnosticFix *)(
                diagnostic->fixes.head + index * sizeof(*fix));
        if (!evidence_work(transaction, 1U) || fix->title == ZR_NULL || fix->editText == ZR_NULL ||
                (TZrUInt32)fix->applicability > ZR_DIAGNOSTIC_FIX_MAYBE_INCORRECT) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool evidence_same_physical(SZrParserComptimeBudgetTransaction *transaction,
        SZrFileRange left, SZrFileRange right, TZrBool *outEqual) {
    *outEqual = ZR_FALSE;
    if (!evidence_work(transaction, 1U)) {
        return ZR_FALSE;
    }
    *outEqual = left.originKey != 0U && left.originKey == right.originKey &&
            left.start.offset == right.start.offset && left.end.offset == right.end.offset &&
            left.start.line == right.start.line && left.end.line == right.end.line &&
            left.start.column == right.start.column && left.end.column == right.end.column;
    return ZR_TRUE;
}

static TZrBool evidence_same_string(SZrParserComptimeBudgetTransaction *transaction,
        SZrString *left, SZrString *right, TZrBool *outEqual) {
    *outEqual = ZR_FALSE;
    if (!evidence_work(transaction, 1U)) {
        return ZR_FALSE;
    }
    if (left == right) {
        *outEqual = ZR_TRUE;
        return ZR_TRUE;
    }
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!evidence_work(transaction, ZrCore_String_GetByteLength(left)) ||
            !evidence_work(transaction, ZrCore_String_GetByteLength(right))) {
        return ZR_FALSE;
    }
    *outEqual = ZrCore_String_Equal(left, right);
    return ZR_TRUE;
}

static TZrBool evidence_duplicate(SZrSemanticContext *context, SZrAstOwnership *owner,
        const SZrSemanticDiagnosticFact *fact, SZrParserRangeEvidenceDescription description,
        SZrParserComptimeBudgetTransaction *transaction, TZrBool *outDuplicate) {
    TZrSize index;

    *outDuplicate = ZR_FALSE;
    for (index = 0U; index < context->diagnosticFacts.length; ++index) {
        const SZrSemanticDiagnosticFact *existing = (const SZrSemanticDiagnosticFact *)(
                context->diagnosticFacts.head + index * sizeof(*existing));
        SZrParserRangeEvidenceDescription existingDescription;
        TZrBool equal;

        if (!evidence_work(transaction, 1U)) {
            return ZR_FALSE;
        }
        if (existing->diagnostic.location.ownerKey != fact->diagnostic.location.ownerKey) {
            continue;
        }
        if (!ZrParser_RangeEvidence_Describe(owner, existing->diagnostic.location, &existingDescription) ||
                existingDescription.kind == ZR_PARSER_RANGE_EVIDENCE_NONE) {
            return ZR_FALSE;
        }
        if (existingDescription.kind != description.kind ||
                existing->diagnostic.location.originKey != fact->diagnostic.location.originKey) {
            continue;
        }
        if (description.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL) {
            if (!evidence_work(transaction, 1U)) {
                return ZR_FALSE;
            }
            if (existingDescription.first != description.first || existingDescription.last != description.last) {
                continue;
            }
        } else {
            if (!evidence_same_physical(transaction, existing->diagnostic.location,
                    fact->diagnostic.location, &equal)) {
                return ZR_FALSE;
            }
            if (!equal) {
                continue;
            }
        }
        if (existing->diagnostic.code == ZR_NULL || existing->diagnostic.message == ZR_NULL ||
                !evidence_same_string(transaction, existing->diagnostic.code, fact->diagnostic.code, &equal)) {
            return ZR_FALSE;
        }
        if (!equal) {
            continue;
        }
        if (!evidence_same_string(transaction, existing->diagnostic.message, fact->diagnostic.message, &equal)) {
            return ZR_FALSE;
        }
        if (equal) {
            *outDuplicate = ZR_TRUE;
            return ZR_TRUE;
        }
    }
    return ZR_TRUE;
}

static TZrBool evidence_atom(SZrParserComptimeBudgetTransaction *transaction,
        SZrAstOwnership *owner, SZrFileRange location, TZrSize ordinal, SZrFileRange *out) {
    return evidence_work(transaction, 1U) &&
            ZrParser_RangeEvidence_AnchorAt(owner, location, ordinal, out);
}

static TZrBool evidence_missing_atom(SZrParserComptimeBudgetTransaction *transaction,
        SZrAstOwnership *owner, const SZrStructuredDiagnostic *diagnostic,
        TZrSize ordinal, SZrFileRange *outAtom, TZrBool *outMissing) {
    TZrSize index;
    TZrBool equal;

    *outMissing = ZR_FALSE;
    if (!evidence_atom(transaction, owner, diagnostic->location, ordinal, outAtom) ||
            !evidence_same_physical(transaction, *outAtom, diagnostic->location, &equal)) {
        return ZR_FALSE;
    }
    if (equal) {
        return ZR_TRUE;
    }
    for (index = 0U; index < diagnostic->relatedInformation.length; ++index) {
        const SZrStructuredDiagnosticRelatedInformation *related =
                (const SZrStructuredDiagnosticRelatedInformation *)(diagnostic->relatedInformation.head +
                        index * sizeof(*related));
        if (!evidence_work(transaction, 1U) ||
                !evidence_same_physical(transaction, *outAtom, related->location, &equal)) {
            return ZR_FALSE;
        }
        if (equal) {
            return ZR_TRUE;
        }
    }
    for (index = 0U; index < ordinal; ++index) {
        SZrFileRange previous;
        if (!evidence_atom(transaction, owner, diagnostic->location, index, &previous) ||
                !evidence_same_physical(transaction, *outAtom, previous, &equal)) {
            return ZR_FALSE;
        }
        if (equal) {
            return ZR_TRUE;
        }
    }
    *outMissing = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool evidence_related_count(SZrParserComptimeBudgetTransaction *transaction,
        SZrAstOwnership *owner, const SZrStructuredDiagnostic *diagnostic,
        SZrParserRangeEvidenceDescription description, TZrSize *outCount) {
    TZrSize count = diagnostic->relatedInformation.length;
    TZrSize ordinal;
    TZrSize atoms;

    if ((diagnostic->location.flags & ZR_FILE_RANGE_FLAG_COMPOSITE) == 0U) {
        *outCount = count;
        return ZR_TRUE;
    }
    if (description.kind != ZR_PARSER_RANGE_EVIDENCE_INTERVAL ||
            description.first == 0U || description.last < description.first) {
        return ZR_FALSE;
    }
    atoms = (TZrSize)description.last - description.first + 1U;
    for (ordinal = 0U; ordinal < atoms; ++ordinal) {
        SZrFileRange atom;
        TZrBool missing;
        if (!evidence_missing_atom(transaction, owner, diagnostic, ordinal, &atom, &missing) ||
                (missing && !evidence_add_size(count, 1U, &count))) {
            return ZR_FALSE;
        }
    }
    *outCount = count;
    return ZR_TRUE;
}

static TZrBool evidence_array_stage(SZrState *state,
        SZrParserComptimeBudgetTransaction *transaction, TZrSize count,
        TZrSize elementSize, SZrArray *out) {
    TZrSize bytes;
    TZrBytePtr head;

    if (count == 0U) {
        return ZR_TRUE;
    }
    if (count > ZR_MAX_SIZE / elementSize) {
        return ZR_FALSE;
    }
    bytes = count * elementSize;
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
            transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, (TZrUInt64)bytes)) {
        return ZR_FALSE;
    }
    head = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(state->global, bytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (head == ZR_NULL) {
        return ZR_FALSE;
    }
    out->head = head;
    out->elementSize = elementSize;
    out->length = count;
    out->capacity = count;
    out->isValid = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool evidence_fill_related(SZrParserComptimeBudgetTransaction *transaction,
        SZrAstOwnership *owner, const SZrStructuredDiagnostic *diagnostic,
        SZrParserRangeEvidenceDescription description, SZrArray *out) {
    TZrSize index = diagnostic->relatedInformation.length;
    TZrSize ordinal;
    TZrSize atoms;

    if (!evidence_work(transaction, index)) {
        return ZR_FALSE;
    }
    if (index != 0U) {
        memcpy(out->head, diagnostic->relatedInformation.head,
                index * sizeof(SZrStructuredDiagnosticRelatedInformation));
    }
    if ((diagnostic->location.flags & ZR_FILE_RANGE_FLAG_COMPOSITE) == 0U) {
        return index == out->length;
    }
    atoms = (TZrSize)description.last - description.first + 1U;
    for (ordinal = 0U; ordinal < atoms; ++ordinal) {
        SZrStructuredDiagnosticRelatedInformation related;
        TZrBool missing;
        memset(&related, 0, sizeof(related));
        if (!evidence_missing_atom(transaction, owner, diagnostic, ordinal, &related.location, &missing)) {
            return ZR_FALSE;
        }
        if (missing) {
            if (index >= out->length || !evidence_work(transaction, 1U)) {
                return ZR_FALSE;
            }
            related.message = diagnostic->message;
            memcpy(out->head + index++ * sizeof(related), &related, sizeof(related));
        }
    }
    return index == out->length;
}

static TZrBool evidence_string_add(SZrState *state, SZrParserComptimeBudgetTransaction *transaction,
        SZrDiagnosticEvidenceStage *stage, SZrString *string) {
    if (!evidence_work(transaction, 1U)) {
        return ZR_FALSE;
    }
    if (string != ZR_NULL) {
        if (!ZrCore_GcDomain_ObjectBelongsToState(state, (SZrRawObject *)string) ||
                stage->stringCount >= stage->stringCapacity) {
            return ZR_FALSE;
        }
        stage->strings[stage->stringCount++] = string;
    }
    return ZR_TRUE;
}

static TZrBool evidence_strings(SZrState *state,
        SZrParserComptimeBudgetTransaction *transaction, SZrDiagnosticEvidenceStage *stage) {
    SZrStructuredDiagnostic *diagnostic = &stage->fact.diagnostic;
    SZrArray pointers;
    TZrSize capacity = 5U;
    TZrSize index;

    ZrCore_Array_Construct(&pointers);
    if (stage->related.length > (ZR_MAX_SIZE - capacity) / 2U) {
        return ZR_FALSE;
    }
    capacity += stage->related.length * 2U;
    if (stage->fixes.length > (ZR_MAX_SIZE - capacity) / 3U) {
        return ZR_FALSE;
    }
    capacity += stage->fixes.length * 3U;
    if (!evidence_array_stage(state, transaction, capacity, sizeof(SZrString *), &pointers)) {
        return ZR_FALSE;
    }
    stage->strings = (SZrString **)pointers.head;
    stage->stringCapacity = capacity;
    if (!evidence_string_add(state, transaction, stage, diagnostic->location.source) ||
            !evidence_string_add(state, transaction, stage, diagnostic->code) ||
            !evidence_string_add(state, transaction, stage, diagnostic->message) ||
            !evidence_string_add(state, transaction, stage, diagnostic->cause) ||
            !evidence_string_add(state, transaction, stage, diagnostic->suggestion)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < stage->related.length; ++index) {
        const SZrStructuredDiagnosticRelatedInformation *related =
                (const SZrStructuredDiagnosticRelatedInformation *)(stage->related.head + index * sizeof(*related));
        if (!evidence_string_add(state, transaction, stage, related->location.source) ||
                !evidence_string_add(state, transaction, stage, related->message)) {
            return ZR_FALSE;
        }
    }
    for (index = 0U; index < stage->fixes.length; ++index) {
        const SZrStructuredDiagnosticFix *fix =
                (const SZrStructuredDiagnosticFix *)(stage->fixes.head + index * sizeof(*fix));
        if (!evidence_string_add(state, transaction, stage, fix->title) ||
                !evidence_string_add(state, transaction, stage, fix->editRange.source) ||
                !evidence_string_add(state, transaction, stage, fix->editText)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static void evidence_stage_discard(SZrState *state, SZrAstOwnership *owner,
        SZrDiagnosticEvidenceStage *stage) {
    if (stage->pins != ZR_NULL) {
        ZrParser_Ownership_DiscardStrings(owner, stage->pins);
    }
    if (stage->strings != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global, stage->strings,
                stage->stringCapacity * sizeof(*stage->strings), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    ZrCore_Array_Free(state, &stage->facts);
    ZrCore_Array_Free(state, &stage->fixes);
    ZrCore_Array_Free(state, &stage->related);
}

TZrBool ZrParser_SemanticDiagnosticEvidence_Append(
        SZrSemanticContext *context, const SZrSemanticDiagnosticFact *fact) {
    SZrAstOwnership *owner;
    SZrParserComptimeBudget *budget;
    SZrParserComptimeBudgetTransaction transaction;
    SZrParserRangeEvidenceDescription description;
    SZrDiagnosticEvidenceStage stage;
    TZrSize relatedCount;
    TZrSize factCount;
    TZrBool duplicate;

    if (context == ZR_NULL || context->state == ZR_NULL || context->state->global == ZR_NULL ||
            fact == ZR_NULL || fact->node == ZR_NULL || (owner = fact->node->ownership) == ZR_NULL ||
            !evidence_array_valid(&context->diagnosticFacts, sizeof(SZrSemanticDiagnosticFact), ZR_TRUE) ||
            (budget = ZrParser_Ownership_Budget(owner)) == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&stage, 0, sizeof(stage));
    ZrCore_Array_Construct(&stage.related);
    ZrCore_Array_Construct(&stage.fixes);
    ZrCore_Array_Construct(&stage.facts);
    ZrParser_ComptimeBudget_BeginTransaction(budget, &transaction);
    if (!evidence_diagnostic_valid(&transaction, &fact->diagnostic) ||
            !evidence_work(&transaction, 1U) ||
            !ZrParser_RangeEvidence_Describe(owner, fact->diagnostic.location, &description) ||
            description.kind == ZR_PARSER_RANGE_EVIDENCE_NONE ||
            !evidence_duplicate(context, owner, fact, description, &transaction, &duplicate)) {
        goto fail;
    }
    if (duplicate) {
        if (!ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
            goto fail;
        }
        return ZR_TRUE;
    }
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
            &transaction, ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT, 1U) ||
            !evidence_related_count(&transaction, owner, &fact->diagnostic, description, &relatedCount) ||
            !evidence_array_stage(context->state, &transaction, relatedCount,
                    sizeof(SZrStructuredDiagnosticRelatedInformation), &stage.related) ||
            !evidence_array_stage(context->state, &transaction, fact->diagnostic.fixes.length,
                    sizeof(SZrStructuredDiagnosticFix), &stage.fixes) ||
            !evidence_fill_related(&transaction, owner, &fact->diagnostic, description, &stage.related) ||
            !evidence_work(&transaction, stage.fixes.length)) {
        goto fail;
    }
    if (stage.fixes.length != 0U) {
        memcpy(stage.fixes.head, fact->diagnostic.fixes.head,
                stage.fixes.length * sizeof(SZrStructuredDiagnosticFix));
    }
    stage.fact = *fact;
    stage.fact.diagnostic.relatedInformation = stage.related;
    stage.fact.diagnostic.fixes = stage.fixes;
    if (!evidence_add_size(context->diagnosticFacts.length, 1U, &factCount)) {
        goto fail;
    }
    if (factCount > context->diagnosticFacts.capacity) {
        if (!evidence_array_stage(context->state, &transaction, factCount,
                sizeof(SZrSemanticDiagnosticFact), &stage.facts) ||
                !evidence_work(&transaction, context->diagnosticFacts.length)) {
            goto fail;
        }
        if (context->diagnosticFacts.length != 0U) {
            memcpy(stage.facts.head, context->diagnosticFacts.head,
                    context->diagnosticFacts.length * sizeof(SZrSemanticDiagnosticFact));
        }
    }
    if (!evidence_strings(context->state, &transaction, &stage) ||
            !ZrParser_Ownership_PrepareStrings(owner, stage.strings, stage.stringCount, &transaction, &stage.pins) ||
            !ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        goto fail;
    }

    /* All remaining operations are allocation-free and cannot reject. */
    if (stage.pins != ZR_NULL) {
        ZrParser_Ownership_ApplyStrings(owner, stage.pins);
        stage.pins = ZR_NULL;
    }
    if (stage.facts.isValid) {
        SZrArray previous = context->diagnosticFacts;
        memcpy(stage.facts.head + previous.length * sizeof(stage.fact), &stage.fact, sizeof(stage.fact));
        context->diagnosticFacts = stage.facts;
        ZrCore_Array_Construct(&stage.facts);
        ZrCore_Array_Free(context->state, &previous);
    } else {
        memcpy(context->diagnosticFacts.head + context->diagnosticFacts.length * sizeof(stage.fact),
                &stage.fact, sizeof(stage.fact));
        context->diagnosticFacts.length = factCount;
    }
    ZrCore_Array_Construct(&stage.related);
    ZrCore_Array_Construct(&stage.fixes);
    evidence_stage_discard(context->state, owner, &stage);
    return ZR_TRUE;

fail:
    evidence_stage_discard(context->state, owner, &stage);
    ZrParser_ComptimeBudget_AbortTransaction(&transaction);
    return ZR_FALSE;
}
