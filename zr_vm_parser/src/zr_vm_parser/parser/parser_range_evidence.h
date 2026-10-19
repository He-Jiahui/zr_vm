#ifndef ZR_VM_PARSER_RANGE_EVIDENCE_H
#define ZR_VM_PARSER_RANGE_EVIDENCE_H

#include "zr_vm_parser/lexer.h"

typedef struct SZrParserRangeEvidence SZrParserRangeEvidence;

typedef enum EZrParserRangeEvidenceKind {
    ZR_PARSER_RANGE_EVIDENCE_NONE,
    ZR_PARSER_RANGE_EVIDENCE_SOURCE,
    ZR_PARSER_RANGE_EVIDENCE_INTERVAL
} EZrParserRangeEvidenceKind;

typedef struct SZrParserRangeEvidenceDescription {
    EZrParserRangeEvidenceKind kind;
    TZrUInt32 first;
    TZrUInt32 last;
} SZrParserRangeEvidenceDescription;

TZrBool ZrParser_RangeEvidence_Begin(
        SZrAstOwnership *owner, SZrState *state, TZrSize atomCount,
        SZrParserRangeEvidence **outStage);
TZrBool ZrParser_RangeEvidence_Append(
        SZrParserRangeEvidence *stage, SZrTokenSpan span,
        TZrUInt64 originKey, SZrString *name, TZrBool absent);
TZrBool ZrParser_RangeEvidence_Commit(SZrParserRangeEvidence *stage);
void ZrParser_RangeEvidence_Free(SZrParserRangeEvidence *evidence);

/* Reads do not allocate or consume budget. Ordinals and indexes are one-based. */
TZrBool ZrParser_RangeEvidence_Location(
        SZrAstOwnership *owner, TZrUInt32 index, SZrFileRange *outRange);
TZrBool ZrParser_RangeEvidence_TokenLocation(
        SZrAstOwnership *owner, TZrUInt32 index, TZrUInt32 first,
        TZrUInt32 last, SZrFileRange *outRange);
TZrBool ZrParser_RangeEvidence_Validate(
        SZrAstOwnership *owner, SZrFileRange range);
/* NONE denotes an unproven legacy range or explicit partial projection.
 * Complete evidence is validated against its AST owner before description.
 * Interval identity is canonical ordinals, not an allocated handle index. */
TZrBool ZrParser_RangeEvidence_Describe(
        SZrAstOwnership *owner, SZrFileRange range,
        SZrParserRangeEvidenceDescription *outDescription);
/* relativeOrdinal is zero-based within the described interval. */
TZrBool ZrParser_RangeEvidence_AnchorAt(
        SZrAstOwnership *owner, SZrFileRange range,
        TZrSize relativeOrdinal, SZrFileRange *outRange);
TZrBool ZrParser_RangeEvidence_Interval(
        SZrAstOwnership *owner, TZrUInt32 first, TZrUInt32 last,
        TZrUInt32 *outIndex);
TZrBool ZrParser_RangeEvidence_Merge(
        SZrAstOwnership *owner, SZrFileRange a, SZrFileRange b,
        SZrFileRange *outRange);

#endif
