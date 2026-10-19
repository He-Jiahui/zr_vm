#include "parser/parser_range_evidence.h"
#include "parser/parser_ownership.h"

#include <stdint.h>
#include <string.h>

typedef struct SZrParserEvidenceAtom {
    SZrFileRange anchor;
    TZrUInt32 invalidCount;
    TZrUInt32 originChanges;
    TZrUInt32 startBacktracks;
    TZrUInt32 endBacktracks;
} SZrParserEvidenceAtom;

typedef struct SZrParserEvidenceInterval {
    TZrUInt32 first;
    TZrUInt32 last;
} SZrParserEvidenceInterval;

struct SZrParserRangeEvidence {
    SZrAstOwnership *owner;
    SZrState *state;
    SZrParserEvidenceAtom *atoms;
    SZrParserEvidenceInterval *intervals;
    TZrUInt32 atomCount;
    TZrUInt32 appended;
    TZrUInt32 intervalCount;
    TZrUInt32 intervalCapacity;
    TZrBool published;
};

static void evidence_clear_range(SZrFileRange *range) {
    if (range != ZR_NULL) {
        memset(range, 0, sizeof(*range));
    }
}

static TZrBool evidence_physical_valid(SZrFileRange range) {
    return range.originKey != 0U && range.start.offset <= range.end.offset &&
           range.start.line > 0 && range.end.line >= range.start.line &&
           range.start.column > 0 && range.end.column > 0 &&
           (range.start.line != range.end.line ||
            range.start.column <= range.end.column);
}

static TZrBool evidence_same_position(SZrFilePosition a, SZrFilePosition b) {
    return a.offset == b.offset && a.line == b.line && a.column == b.column;
}

static TZrBool evidence_same_range(SZrFileRange a, SZrFileRange b) {
    return evidence_same_position(a.start, b.start) &&
           evidence_same_position(a.end, b.end) && a.source == b.source &&
           a.originKey == b.originKey && a.ownerKey == b.ownerKey &&
           a.evidenceIndex == b.evidenceIndex && a.flags == b.flags;
}

void ZrParser_RangeEvidence_Free(SZrParserRangeEvidence *evidence) {
    SZrGlobalState *global;
    if (evidence == ZR_NULL) {
        return;
    }
    global = evidence->state->global;
    if (evidence->atoms != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global, evidence->atoms,
                (TZrSize)evidence->atomCount * sizeof(*evidence->atoms),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (evidence->intervals != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global, evidence->intervals,
                (TZrSize)evidence->intervalCapacity * sizeof(*evidence->intervals),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    ZrCore_Memory_RawFreeWithType(global, evidence, sizeof(*evidence),
                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

TZrBool ZrParser_RangeEvidence_Begin(
        SZrAstOwnership *owner, SZrState *state, TZrSize atomCount,
        SZrParserRangeEvidence **outStage) {
    SZrParserRangeEvidence *stage;
    if (outStage == ZR_NULL) {
        return ZR_FALSE;
    }
    *outStage = ZR_NULL;
    if (owner == ZR_NULL || state == ZR_NULL || state->global == ZR_NULL ||
        ZrParser_Ownership_Evidence(owner) != ZR_NULL ||
        ZrParser_Ownership_SourceOrigin(owner) != 0U || atomCount == 0U ||
        atomCount > UINT32_MAX ||
        atomCount > ZR_MAX_SIZE / sizeof(SZrParserEvidenceAtom) ||
        atomCount > ZR_MAX_SIZE / sizeof(SZrParserEvidenceInterval) ||
        !ZrParser_ComptimeBudget_TryConsume(ZrParser_Ownership_Budget(owner),
                ZR_PARSER_COMPTIME_BUDGET_FUEL, (TZrUInt64)atomCount)) {
        return ZR_FALSE;
    }
    stage = ZrParser_Ownership_Allocate(owner, sizeof(*stage),
                                       ZR_MEMORY_NATIVE_TYPE_ARRAY, ZR_FALSE);
    if (stage == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(stage, 0, sizeof(*stage));
    stage->owner = owner;
    stage->state = state;
    stage->atomCount = (TZrUInt32)atomCount;
    stage->intervalCapacity = (TZrUInt32)atomCount;
    stage->atoms = ZrParser_Ownership_Allocate(owner,
            atomCount * sizeof(*stage->atoms), ZR_MEMORY_NATIVE_TYPE_ARRAY, ZR_FALSE);
    stage->intervals = ZrParser_Ownership_Allocate(owner,
            atomCount * sizeof(*stage->intervals), ZR_MEMORY_NATIVE_TYPE_ARRAY, ZR_FALSE);
    if (stage->atoms == ZR_NULL || stage->intervals == ZR_NULL) {
        ZrParser_RangeEvidence_Free(stage);
        return ZR_FALSE;
    }
    memset(stage->atoms, 0, atomCount * sizeof(*stage->atoms));
    *outStage = stage;
    return ZR_TRUE;
}

TZrBool ZrParser_RangeEvidence_Append(
        SZrParserRangeEvidence *stage, SZrTokenSpan span,
        TZrUInt64 originKey, SZrString *name, TZrBool absent) {
    SZrParserEvidenceAtom *atom;
    SZrParserEvidenceAtom *previous;
    TZrUInt32 ordinal;

    if (stage == ZR_NULL || stage->published || stage->appended >= stage->atomCount ||
        (!absent && (!ZrParser_TokenSpan_IsValid(span) || originKey == 0U || name == ZR_NULL)) ||
        (absent && (stage->atomCount != 1U || originKey != 0U || name != ZR_NULL))) {
        return ZR_FALSE;
    }
    ordinal = stage->appended + 1U;
    atom = &stage->atoms[stage->appended];
    atom->anchor.start = ZrParser_FilePosition_Create(span.startOffset, span.startLine, span.startColumn);
    atom->anchor.end = ZrParser_FilePosition_Create(span.endOffset, span.endLine, span.endColumn);
    atom->anchor.source = name;
    atom->anchor.originKey = originKey;
    atom->anchor.ownerKey = ZrParser_Ownership_Key(stage->owner);
    atom->anchor.evidenceIndex = ordinal;
    atom->anchor.flags = absent ? ZR_FILE_RANGE_FLAG_PARTIAL : 0U;
    if (!absent && !evidence_physical_valid(atom->anchor)) {
        return ZR_FALSE;
    }
    previous = stage->appended != 0U ? &stage->atoms[stage->appended - 1U] : ZR_NULL;
    atom->invalidCount = (previous != ZR_NULL ? previous->invalidCount : 0U) + (absent ? 1U : 0U);
    atom->originChanges = previous != ZR_NULL ? previous->originChanges +
            (previous->anchor.originKey != originKey ? 1U : 0U) : 0U;
    atom->startBacktracks = previous != ZR_NULL ? previous->startBacktracks +
            (previous->anchor.start.offset > atom->anchor.start.offset ||
             previous->anchor.start.line > atom->anchor.start.line ||
             (previous->anchor.start.line == atom->anchor.start.line &&
              previous->anchor.start.column > atom->anchor.start.column) ? 1U : 0U) : 0U;
    atom->endBacktracks = previous != ZR_NULL ? previous->endBacktracks +
            (previous->anchor.end.offset > atom->anchor.end.offset ||
             previous->anchor.end.line > atom->anchor.end.line ||
             (previous->anchor.end.line == atom->anchor.end.line &&
              previous->anchor.end.column > atom->anchor.end.column) ? 1U : 0U) : 0U;
    stage->intervals[stage->appended].first = ordinal;
    stage->intervals[stage->appended].last = ordinal;
    stage->appended++;
    return ZR_TRUE;
}

TZrBool ZrParser_RangeEvidence_Commit(SZrParserRangeEvidence *stage) {
    if (stage == ZR_NULL || stage->published || stage->appended != stage->atomCount ||
        !ZrParser_Ownership_PublishEvidence(stage->owner, stage)) {
        return ZR_FALSE;
    }
    stage->intervalCount = stage->atomCount;
    stage->published = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrParser_RangeEvidence_Location(
        SZrAstOwnership *owner, TZrUInt32 index, SZrFileRange *outRange) {
    SZrParserRangeEvidence *evidence = ZrParser_Ownership_Evidence(owner);
    SZrParserEvidenceInterval interval;
    const SZrParserEvidenceAtom *first;
    const SZrParserEvidenceAtom *last;
    TZrUInt32 invalidBefore;

    evidence_clear_range(outRange);
    if (outRange == ZR_NULL || evidence == ZR_NULL || !evidence->published ||
        index == 0U || index > evidence->intervalCount) {
        return ZR_FALSE;
    }
    interval = evidence->intervals[index - 1U];
    if (interval.first == 0U || interval.first > interval.last || interval.last > evidence->atomCount) {
        return ZR_FALSE;
    }
    first = &evidence->atoms[interval.first - 1U];
    last = &evidence->atoms[interval.last - 1U];
    invalidBefore = interval.first > 1U ? evidence->atoms[interval.first - 2U].invalidCount : 0U;
    if (last->invalidCount != invalidBefore) {
        return ZR_FALSE;
    }
    *outRange = first->anchor;
    outRange->evidenceIndex = index;
    if (first->originChanges == last->originChanges &&
        first->startBacktracks == last->startBacktracks &&
        first->endBacktracks == last->endBacktracks) {
        outRange->end = last->anchor.end;
    } else {
        outRange->flags |= ZR_FILE_RANGE_FLAG_COMPOSITE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_RangeEvidence_Validate(SZrAstOwnership *owner, SZrFileRange range) {
    SZrFileRange expected;
    if (owner == ZR_NULL || range.ownerKey == 0U ||
        range.ownerKey != ZrParser_Ownership_Key(owner)) {
        return ZR_FALSE;
    }
    if (range.evidenceIndex == 0U) {
        return ZrParser_Ownership_SourceOrigin(owner) != 0U &&
               range.originKey == ZrParser_Ownership_SourceOrigin(owner) &&
               range.flags == 0U && evidence_physical_valid(range);
    }
    return ZrParser_RangeEvidence_Location(owner, range.evidenceIndex, &expected) &&
           evidence_same_range(range, expected);
}

TZrBool ZrParser_RangeEvidence_Describe(
        SZrAstOwnership *owner, SZrFileRange range,
        SZrParserRangeEvidenceDescription *outDescription) {
    SZrParserRangeEvidence *evidence;

    if (outDescription == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outDescription, 0, sizeof(*outDescription));
    if (range.ownerKey == 0U && range.evidenceIndex == 0U) {
        return ZR_TRUE;
    }
    if (!ZrParser_RangeEvidence_Validate(owner, range)) {
        return ZR_FALSE;
    }
    if (range.evidenceIndex == 0U) {
        outDescription->kind = ZR_PARSER_RANGE_EVIDENCE_SOURCE;
        return ZR_TRUE;
    }
    evidence = ZrParser_Ownership_Evidence(owner);
    outDescription->kind = ZR_PARSER_RANGE_EVIDENCE_INTERVAL;
    outDescription->first = evidence->intervals[range.evidenceIndex - 1U].first;
    outDescription->last = evidence->intervals[range.evidenceIndex - 1U].last;
    return ZR_TRUE;
}

TZrBool ZrParser_RangeEvidence_AnchorAt(
        SZrAstOwnership *owner, SZrFileRange range,
        TZrSize relativeOrdinal, SZrFileRange *outRange) {
    SZrParserRangeEvidenceDescription description;
    SZrParserRangeEvidence *evidence;

    evidence_clear_range(outRange);
    if (outRange == ZR_NULL ||
        !ZrParser_RangeEvidence_Describe(owner, range, &description)) {
        return ZR_FALSE;
    }
    if (description.kind == ZR_PARSER_RANGE_EVIDENCE_SOURCE) {
        if (relativeOrdinal != 0U) {
            return ZR_FALSE;
        }
        *outRange = range;
        return ZR_TRUE;
    }
    if (description.kind != ZR_PARSER_RANGE_EVIDENCE_INTERVAL ||
        relativeOrdinal > (TZrSize)(description.last - description.first)) {
        return ZR_FALSE;
    }
    evidence = ZrParser_Ownership_Evidence(owner);
    *outRange = evidence->atoms[(TZrSize)description.first - 1U + relativeOrdinal].anchor;
    return ZR_TRUE;
}

TZrBool ZrParser_RangeEvidence_TokenLocation(
        SZrAstOwnership *owner, TZrUInt32 index, TZrUInt32 first,
        TZrUInt32 last, SZrFileRange *outRange) {
    SZrParserRangeEvidence *evidence = ZrParser_Ownership_Evidence(owner);
    evidence_clear_range(outRange);
    if (evidence == ZR_NULL || !evidence->published || index == 0U ||
        index > evidence->intervalCount || first == 0U || first > last ||
        evidence->intervals[index - 1U].first != first ||
        evidence->intervals[index - 1U].last != last) {
        return ZR_FALSE;
    }
    return ZrParser_RangeEvidence_Location(owner, index, outRange);
}

static TZrBool evidence_reserve_interval(SZrParserRangeEvidence *evidence) {
    SZrParserEvidenceInterval *intervals;
    TZrSize capacity;
    if (evidence->intervalCount < evidence->intervalCapacity) {
        return ZR_TRUE;
    }
    if (evidence->intervalCount == UINT32_MAX) {
        return ZR_FALSE;
    }
    capacity = evidence->intervalCapacity <= UINT32_MAX / 2U
                       ? (TZrSize)evidence->intervalCapacity * 2U : UINT32_MAX;
    if (capacity > ZR_MAX_SIZE / sizeof(*intervals) ||
        !ZrParser_ComptimeBudget_TryConsume(ZrParser_Ownership_Budget(evidence->owner),
                ZR_PARSER_COMPTIME_BUDGET_FUEL, evidence->intervalCount)) {
        return ZR_FALSE;
    }
    intervals = ZrParser_Ownership_Allocate(evidence->owner, capacity * sizeof(*intervals),
                                           ZR_MEMORY_NATIVE_TYPE_ARRAY, ZR_FALSE);
    if (intervals == ZR_NULL) {
        return ZR_FALSE;
    }
    memcpy(intervals, evidence->intervals,
           (TZrSize)evidence->intervalCount * sizeof(*intervals));
    ZrCore_Memory_RawFreeWithType(evidence->state->global, evidence->intervals,
            (TZrSize)evidence->intervalCapacity * sizeof(*intervals), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    evidence->intervals = intervals;
    evidence->intervalCapacity = (TZrUInt32)capacity;
    return ZR_TRUE;
}

TZrBool ZrParser_RangeEvidence_Interval(
        SZrAstOwnership *owner, TZrUInt32 first, TZrUInt32 last, TZrUInt32 *outIndex) {
    SZrParserRangeEvidence *evidence = ZrParser_Ownership_Evidence(owner);
    if (outIndex == ZR_NULL) {
        return ZR_FALSE;
    }
    *outIndex = 0U;
    if (evidence == ZR_NULL || !evidence->published || first == 0U || first > last ||
        last > evidence->atomCount ||
        !ZrParser_ComptimeBudget_TryConsume(ZrParser_Ownership_Budget(owner),
                ZR_PARSER_COMPTIME_BUDGET_FUEL, 1U)) {
        return ZR_FALSE;
    }
    if (first == last) {
        *outIndex = first;
        return ZR_TRUE;
    }
    if (!evidence_reserve_interval(evidence)) {
        return ZR_FALSE;
    }
    evidence->intervals[evidence->intervalCount].first = first;
    evidence->intervals[evidence->intervalCount].last = last;
    *outIndex = ++evidence->intervalCount;
    return ZR_TRUE;
}

TZrBool ZrParser_RangeEvidence_Merge(
        SZrAstOwnership *owner, SZrFileRange a, SZrFileRange b, SZrFileRange *outRange) {
    SZrParserRangeEvidence *evidence = ZrParser_Ownership_Evidence(owner);
    SZrParserEvidenceInterval left, right;
    TZrUInt32 first, last, index;
    evidence_clear_range(outRange);
    if (outRange == ZR_NULL || !ZrParser_RangeEvidence_Validate(owner, a) ||
        !ZrParser_RangeEvidence_Validate(owner, b) ||
        !ZrParser_ComptimeBudget_TryConsume(ZrParser_Ownership_Budget(owner),
                ZR_PARSER_COMPTIME_BUDGET_FUEL, 1U)) {
        return ZR_FALSE;
    }
    if (a.evidenceIndex == 0U || b.evidenceIndex == 0U) {
        if (a.evidenceIndex != 0U || b.evidenceIndex != 0U || a.originKey != b.originKey) {
            return ZR_FALSE;
        }
        *outRange = a;
        if (b.start.offset < a.start.offset) {
            outRange->start = b.start;
        }
        if (b.end.offset > a.end.offset) {
            outRange->end = b.end;
        }
        return ZR_TRUE;
    }
    left = evidence->intervals[a.evidenceIndex - 1U];
    right = evidence->intervals[b.evidenceIndex - 1U];
    first = left.first < right.first ? left.first : right.first;
    last = left.last > right.last ? left.last : right.last;
    if (first == left.first && last == left.last) {
        *outRange = a;
        return ZR_TRUE;
    }
    if (first == right.first && last == right.last) {
        *outRange = b;
        return ZR_TRUE;
    }
    return ZrParser_RangeEvidence_Interval(owner, first, last, &index) &&
           ZrParser_RangeEvidence_Location(owner, index, outRange);
}
