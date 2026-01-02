//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_LOCATION_H
#define ZR_VM_PARSER_LOCATION_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_core/string.h"

// 文件位置
typedef struct SZrFilePosition {
    TZrSize offset;  // 字符偏移
    TInt32 line;     // 行号
    TInt32 column;   // 列号
} SZrFilePosition;

// 文件范围
typedef struct SZrFileRange {
    SZrFilePosition start;
    SZrFilePosition end;
    SZrString *source;  // 源文件名
} SZrFileRange;

// 创建文件位置
ZR_PARSER_API SZrFilePosition ZrFilePositionCreate(TZrSize offset, TInt32 line, TInt32 column);

// 创建文件范围
ZR_PARSER_API SZrFileRange ZrFileRangeCreate(SZrFilePosition start, SZrFilePosition end, SZrString *source);

// 合并两个文件范围
ZR_PARSER_API SZrFileRange ZrFileRangeMerge(SZrFileRange range1, SZrFileRange range2);

#endif //ZR_VM_PARSER_LOCATION_H

