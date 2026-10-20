#ifndef ZR_VM_PARSER_TOKEN_SPAN_H
#define ZR_VM_PARSER_TOKEN_SPAN_H

#include "zr_vm_parser/conf.h"

typedef TZrUInt32 TZrTokenSourceId;

enum {
    ZR_TOKEN_SOURCE_INVALID = 0
};

typedef struct SZrTokenText {
    TZrTokenSourceId sourceId;
    TZrSize offset;
    TZrSize length;
} SZrTokenText;

typedef struct SZrTokenSpan {
    TZrTokenSourceId sourceId;
    TZrSize startOffset;
    TZrSize endOffset;
    TZrInt32 startLine;
    TZrInt32 startColumn;
    TZrInt32 endLine;
    TZrInt32 endColumn;
} SZrTokenSpan;

ZR_PARSER_API SZrTokenSpan ZrParser_TokenSpan_None(void);
ZR_PARSER_API TZrBool ZrParser_TokenSpan_IsValid(SZrTokenSpan span);
ZR_PARSER_API TZrBool ZrParser_TokenSpan_Join(
        SZrTokenSpan left, SZrTokenSpan right, SZrTokenSpan *outSpan);
/* Byte subspans are valid only inside a single-line token. */
ZR_PARSER_API TZrBool ZrParser_TokenSpan_Subspan(
        SZrTokenSpan span, TZrSize startOffset, TZrSize endOffset,
        SZrTokenSpan *outSpan);

#endif
