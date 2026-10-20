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

typedef enum EZrFileRangeFlags {
    ZR_FILE_RANGE_FLAG_COMPOSITE = 1U,
    ZR_FILE_RANGE_FLAG_PARTIAL = 2U
} EZrFileRangeFlags;

/** @brief A physical source anchor, optionally backed by owner-held token evidence.
 * @note source is borrowed; offsets are half-open. Zero keys mean unknown origin.
 * Composite ranges retain their first native anchor, not a fabricated envelope. */
typedef struct SZrFileRange {
    SZrFilePosition start;
    SZrFilePosition end;
    SZrString *source;  // 源文件名
    TZrUInt64 originKey;
    TZrUInt64 ownerKey;
    TZrUInt32 evidenceIndex;
    TZrUInt32 flags;
} SZrFileRange;

/** @brief 直接包装调用方给出的字节偏移与行列，不做边界或基数校验。 */
ZR_PARSER_API SZrFilePosition ZrParser_FilePosition_Create(TZrSize offset, TZrInt32 line, TZrInt32 column);

/** @brief 组合两个位置与借用的源名；调用方负责保持三者指向同一源文本。 */
ZR_PARSER_API SZrFileRange ZrParser_FileRange_Create(SZrFilePosition start, SZrFilePosition end, SZrString *source);

/** @brief Conservatively combine anchors without access to token evidence.
 * @note Parser construction must use its checked, owner-aware merge instead. */
ZR_PARSER_API SZrFileRange ZrParser_FileRange_Merge(SZrFileRange range1, SZrFileRange range2);

/** @brief Drop token evidence after changing an anchor's physical endpoints. */
ZR_PARSER_API SZrFileRange ZrParser_FileRange_WithoutEvidence(SZrFileRange range);

#endif //ZR_VM_PARSER_LOCATION_H

