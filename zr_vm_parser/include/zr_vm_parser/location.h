//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_LOCATION_H
#define ZR_VM_PARSER_LOCATION_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_core/string.h"

/** @brief 源文本中的位置；offset 按字节计，解析器产生的行列从 1 开始。
 * @note LSP 等调用方也会用 0 行列构造占位位置，不能仅凭此类型判断位置有效性。 */
typedef struct SZrFilePosition {
    TZrSize offset;  // 源文本字节偏移
    TZrInt32 line;     // 行号
    TZrInt32 column;   // 列号
} SZrFilePosition;

/** @brief 同一源文件中的范围；source 借用 VM 字符串，不转移所有权。
 * @note 正常解析位置按起点包含、终点不包含使用；占位范围可退化为空。 */
typedef struct SZrFileRange {
    SZrFilePosition start;
    SZrFilePosition end;
    SZrString *source;  // 源文件名
} SZrFileRange;

/** @brief 直接包装调用方给出的字节偏移与行列，不做边界或基数校验。 */
ZR_PARSER_API SZrFilePosition ZrParser_FilePosition_Create(TZrSize offset, TZrInt32 line, TZrInt32 column);

/** @brief 组合两个位置与借用的源名；调用方负责保持三者指向同一源文本。 */
ZR_PARSER_API SZrFileRange ZrParser_FileRange_Create(SZrFilePosition start, SZrFilePosition end, SZrString *source);

/** @brief 按字节偏移取最早起点和最晚终点，并沿用第一个范围的源名。
 * @pre 两个范围应属于同一源文件；函数不核查 source 是否一致。 */
ZR_PARSER_API SZrFileRange ZrParser_FileRange_Merge(SZrFileRange range1, SZrFileRange range2);

#endif //ZR_VM_PARSER_LOCATION_H

