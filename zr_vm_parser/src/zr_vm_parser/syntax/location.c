//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/location.h"

SZrFilePosition ZrParser_FilePosition_Create(TZrSize offset, TZrInt32 line, TZrInt32 column) {
    SZrFilePosition pos;
    pos.offset = offset;
    pos.line = line;
    pos.column = column;
    return pos;
}

SZrFileRange ZrParser_FileRange_Create(SZrFilePosition start, SZrFilePosition end, SZrString *source) {
    SZrFileRange range = {0};
    range.start = start;
    range.end = end;
    range.source = source;
    return range;
}

SZrFileRange ZrParser_FileRange_Merge(SZrFileRange range1, SZrFileRange range2) {
    if (range1.originKey != 0U && range1.originKey == range2.originKey &&
        range1.ownerKey != 0U && range1.ownerKey == range2.ownerKey &&
        range1.flags == 0U && range2.flags == 0U) {
        if (range1.evidenceIndex == 0U && range2.evidenceIndex == 0U) {
            /* Ordinary text owners have one physical source and no ledger. */
            SZrFileRange merged = range1;
            merged.start = range1.start.offset < range2.start.offset ? range1.start : range2.start;
            merged.end = range1.end.offset > range2.end.offset ? range1.end : range2.end;
            return merged;
        }
        if (range1.evidenceIndex != 0U && range1.evidenceIndex == range2.evidenceIndex &&
            range1.start.offset == range2.start.offset && range1.end.offset == range2.end.offset &&
            range1.start.line == range2.start.line && range1.start.column == range2.start.column &&
            range1.end.line == range2.end.line && range1.end.column == range2.end.column &&
            range1.source == range2.source) {
            return range1;
        }
    }
    range1 = ZrParser_FileRange_WithoutEvidence(range1);
    range1.flags |= ZR_FILE_RANGE_FLAG_COMPOSITE;
    return range1;
}

SZrFileRange ZrParser_FileRange_WithoutEvidence(SZrFileRange range) {
    range.ownerKey = 0U;
    range.evidenceIndex = 0U;
    range.flags |= ZR_FILE_RANGE_FLAG_PARTIAL;
    return range;
}

