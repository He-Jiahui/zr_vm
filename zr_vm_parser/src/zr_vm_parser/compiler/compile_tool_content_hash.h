#ifndef ZR_VM_PARSER_COMPILE_TOOL_CONTENT_HASH_H
#define ZR_VM_PARSER_COMPILE_TOOL_CONTENT_HASH_H

#include "zr_vm_parser/compile_tool.h"

#define ZR_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH \
    ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH
#define ZR_PARSER_SHA256_BLOCK_BYTE_COUNT 64U
#define ZR_PARSER_SHA256_DIGEST_BYTE_COUNT 32U
#define ZR_PARSER_SHA256_STATE_WORD_COUNT 8U

typedef struct SZrParserSha256Context {
    TZrByte block[ZR_PARSER_SHA256_BLOCK_BYTE_COUNT];
    TZrSize blockByteCount;
    TZrUInt64 totalByteCount;
    TZrUInt32 state[ZR_PARSER_SHA256_STATE_WORD_COUNT];
} SZrParserSha256Context;

void ZrParser_Sha256_Init(SZrParserSha256Context *context);
TZrBool ZrParser_Sha256_Update(
        SZrParserSha256Context *context,
        const TZrByte *bytes,
        TZrSize byteCount);
void ZrParser_Sha256_Final(
        SZrParserSha256Context *context,
        TZrByte digest[ZR_PARSER_SHA256_DIGEST_BYTE_COUNT]);
TZrBool ZrParser_Sha256_FormatDigest(
        const TZrByte digest[ZR_PARSER_SHA256_DIGEST_BYTE_COUNT],
        TZrChar *outHash,
        TZrSize outHashSize);

#endif // ZR_VM_PARSER_COMPILE_TOOL_CONTENT_HASH_H
