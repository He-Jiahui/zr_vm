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
    TZrInt32 line;     // 行号
    TZrInt32 column;   // 列号
} SZrFilePosition;

// 文件范围
typedef struct SZrFileRange {
    SZrFilePosition start;
    SZrFilePosition end;
    SZrString *source;  // 源文件名
} SZrFileRange;

// 创建文件位置
ZR_PARSER_API SZrFilePosition ZrParser_FilePosition_Create(TZrSize offset, TZrInt32 line, TZrInt32 column);

// 创建文件范围
ZR_PARSER_API SZrFileRange ZrParser_FileRange_Create(SZrFilePosition start, SZrFilePosition end, SZrString *source);

// 合并两个文件范围
ZR_PARSER_API SZrFileRange ZrParser_FileRange_Merge(SZrFileRange range1, SZrFileRange range2);

#endif //ZR_VM_PARSER_LOCATION_H

