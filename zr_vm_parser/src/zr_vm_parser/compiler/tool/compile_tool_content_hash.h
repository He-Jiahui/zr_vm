#ifndef ZR_VM_PARSER_COMPILE_TOOL_CONTENT_HASH_H
#define ZR_VM_PARSER_COMPILE_TOOL_CONTENT_HASH_H

#include "zr_vm_parser/compile_tool.h"

/* 复用公共 API 长度合同；容量必须容纳前缀、完整 base64url 文本和 NUL。 */
#define ZR_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH \
    ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH
#define ZR_PARSER_SHA256_BLOCK_BYTE_COUNT 64U
#define ZR_PARSER_SHA256_DIGEST_BYTE_COUNT 32U
/* 压缩状态固定由 SHA-256 规范定义的八个 32 位 chaining words 组成。 */
#define ZR_PARSER_SHA256_STATE_WORD_COUNT 8U

/**
 * @brief 增量 SHA-256 状态；blockByteCount 指向未满 block 的有效前缀，totalByteCount 累计已接收字节。
 * @note Init 后依次 Update，再 Final；摘要之外的成员仅由本模块管理。
 * @note block/blockByteCount 共同描述待处理前缀；totalByteCount 覆盖已接收输入；state 保存八个 chaining words。
 */
typedef struct SZrParserSha256Context {
    TZrByte block[ZR_PARSER_SHA256_BLOCK_BYTE_COUNT];
    TZrSize blockByteCount;
    TZrUInt64 totalByteCount;
    TZrUInt32 state[ZR_PARSER_SHA256_STATE_WORD_COUNT];
} SZrParserSha256Context;

/** @brief 初始化摘要流；空指针被忽略。 */
void ZrParser_Sha256_Init(SZrParserSha256Context *context);
/** @brief 累加一段输入；byteCount 为零时允许 bytes 为空。 */
TZrBool ZrParser_Sha256_Update(
        SZrParserSha256Context *context,
        const TZrByte *bytes,
        TZrSize byteCount);
/** @brief 完成摘要流；要求有效 context 和 32 字节输出缓冲。 */
void ZrParser_Sha256_Final(
        SZrParserSha256Context *context,
        TZrByte digest[ZR_PARSER_SHA256_DIGEST_BYTE_COUNT]);
/** @brief 把 32 字节摘要转换为 compile-tool 内容 hash 文本。 */
TZrBool ZrParser_Sha256_FormatDigest(
        const TZrByte digest[ZR_PARSER_SHA256_DIGEST_BYTE_COUNT],
        TZrChar *outHash,
        TZrSize outHashSize);

#endif // ZR_VM_PARSER_COMPILE_TOOL_CONTENT_HASH_H
