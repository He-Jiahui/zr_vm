//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/location.h"

SZrFilePosition ZrFilePositionCreate(TZrSize offset, TInt32 line, TInt32 column) {
    SZrFilePosition pos;
    pos.offset = offset;
    pos.line = line;
    pos.column = column;
    return pos;
}

SZrFileRange ZrFileRangeCreate(SZrFilePosition start, SZrFilePosition end, SZrString *source) {
    SZrFileRange range;
    range.start = start;
    range.end = end;
    range.source = source;
    return range;
}

SZrFileRange ZrFileRangeMerge(SZrFileRange range1, SZrFileRange range2) {
    SZrFileRange merged;
    // 选择更早的起始位置
    if (range1.start.offset < range2.start.offset) {
        merged.start = range1.start;
    } else {
        merged.start = range2.start;
    }
    // 选择更晚的结束位置
    if (range1.end.offset > range2.end.offset) {
        merged.end = range1.end;
    } else {
        merged.end = range2.end;
    }
    // 使用第一个范围的源文件名
    merged.source = range1.source;
    return merged;
}

