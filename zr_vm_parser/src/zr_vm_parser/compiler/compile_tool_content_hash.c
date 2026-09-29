#include "compile_tool_content_hash.h"

#include <stdint.h>
#include <string.h>

// SHA-256 core adapted to zr_vm types and warning policy from Brad Conte's
// public-domain crypto-algorithms implementation of FIPS 180-4 SHA-256.

/* 固定 SHA-256 压缩轮数；改变算法参数会改变 artifact 与缓存身份。 */
#define ZR_SHA256_ROUND_COUNT 64U

/* FIPS 180-4 的轮常量属于编译工具 artifact 与 cache key 的稳定 SHA-256 合同。 */
static const TZrUInt32 g_compileToolSha256RoundConstants[ZR_SHA256_ROUND_COUNT] = {
    UINT32_C(0x428a2f98), UINT32_C(0x71374491), UINT32_C(0xb5c0fbcf), UINT32_C(0xe9b5dba5),
    UINT32_C(0x3956c25b), UINT32_C(0x59f111f1), UINT32_C(0x923f82a4), UINT32_C(0xab1c5ed5),
    UINT32_C(0xd807aa98), UINT32_C(0x12835b01), UINT32_C(0x243185be), UINT32_C(0x550c7dc3),
    UINT32_C(0x72be5d74), UINT32_C(0x80deb1fe), UINT32_C(0x9bdc06a7), UINT32_C(0xc19bf174),
    UINT32_C(0xe49b69c1), UINT32_C(0xefbe4786), UINT32_C(0x0fc19dc6), UINT32_C(0x240ca1cc),
    UINT32_C(0x2de92c6f), UINT32_C(0x4a7484aa), UINT32_C(0x5cb0a9dc), UINT32_C(0x76f988da),
    UINT32_C(0x983e5152), UINT32_C(0xa831c66d), UINT32_C(0xb00327c8), UINT32_C(0xbf597fc7),
    UINT32_C(0xc6e00bf3), UINT32_C(0xd5a79147), UINT32_C(0x06ca6351), UINT32_C(0x14292967),
    UINT32_C(0x27b70a85), UINT32_C(0x2e1b2138), UINT32_C(0x4d2c6dfc), UINT32_C(0x53380d13),
    UINT32_C(0x650a7354), UINT32_C(0x766a0abb), UINT32_C(0x81c2c92e), UINT32_C(0x92722c85),
    UINT32_C(0xa2bfe8a1), UINT32_C(0xa81a664b), UINT32_C(0xc24b8b70), UINT32_C(0xc76c51a3),
    UINT32_C(0xd192e819), UINT32_C(0xd6990624), UINT32_C(0xf40e3585), UINT32_C(0x106aa070),
    UINT32_C(0x19a4c116), UINT32_C(0x1e376c08), UINT32_C(0x2748774c), UINT32_C(0x34b0bcb5),
    UINT32_C(0x391c0cb3), UINT32_C(0x4ed8aa4a), UINT32_C(0x5b9cca4f), UINT32_C(0x682e6ff3),
    UINT32_C(0x748f82ee), UINT32_C(0x78a5636f), UINT32_C(0x84c87814), UINT32_C(0x8cc70208),
    UINT32_C(0x90befffa), UINT32_C(0xa4506ceb), UINT32_C(0xbef9a3f7), UINT32_C(0xc67178f2)
};

/** @brief SHA-256 压缩阶段共享的轮内原语；调用方只传入 1..31 的固定旋转量。 */
static TZrUInt32 compile_tool_sha256_rotate_right(TZrUInt32 value, TZrUInt32 count) {
    return (value >> count) | (value << (UINT32_C(32) - count));
}

/** @brief 实现 SHA-256 轮函数的选择项，供压缩阶段组合状态字。 */
static TZrUInt32 compile_tool_sha256_choose(
        TZrUInt32 x,
        TZrUInt32 y,
        TZrUInt32 z) {
    return (x & y) ^ (~x & z);
}

/** @brief 实现 SHA-256 轮函数的多数项，供压缩阶段组合状态字。 */
static TZrUInt32 compile_tool_sha256_majority(
        TZrUInt32 x,
        TZrUInt32 y,
        TZrUInt32 z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

/** @brief 计算 SHA-256 消息扩展的 sigma0，用于生成 64 轮输入词。 */
static TZrUInt32 compile_tool_sha256_schedule0(TZrUInt32 value) {
    return compile_tool_sha256_rotate_right(value, UINT32_C(7)) ^
           compile_tool_sha256_rotate_right(value, UINT32_C(18)) ^
           (value >> UINT32_C(3));
}

/** @brief 计算 SHA-256 消息扩展的 sigma1，用于生成 64 轮输入词。 */
static TZrUInt32 compile_tool_sha256_schedule1(TZrUInt32 value) {
    return compile_tool_sha256_rotate_right(value, UINT32_C(17)) ^
           compile_tool_sha256_rotate_right(value, UINT32_C(19)) ^
           (value >> UINT32_C(10));
}

/** @brief 计算 SHA-256 压缩轮的大写 sigma0。 */
static TZrUInt32 compile_tool_sha256_sum0(TZrUInt32 value) {
    return compile_tool_sha256_rotate_right(value, UINT32_C(2)) ^
           compile_tool_sha256_rotate_right(value, UINT32_C(13)) ^
           compile_tool_sha256_rotate_right(value, UINT32_C(22));
}

/** @brief 计算 SHA-256 压缩轮的大写 sigma1。 */
static TZrUInt32 compile_tool_sha256_sum1(TZrUInt32 value) {
    return compile_tool_sha256_rotate_right(value, UINT32_C(6)) ^
           compile_tool_sha256_rotate_right(value, UINT32_C(11)) ^
           compile_tool_sha256_rotate_right(value, UINT32_C(25));
}

/** @brief 消费一个完整 SHA-256 block 并推进 context；Update/Final 是其唯一状态机调用链。 */
static void compile_tool_sha256_transform(
        SZrParserSha256Context *context,
        const TZrByte block[ZR_PARSER_SHA256_BLOCK_BYTE_COUNT]) {
    TZrUInt32 words[ZR_SHA256_ROUND_COUNT];
    TZrUInt32 working[ZR_PARSER_SHA256_STATE_WORD_COUNT];

    for (TZrSize index = 0U; index < 16U; index++) {
        TZrSize offset = index * 4U;
        words[index] = ((TZrUInt32)block[offset] << UINT32_C(24)) |
                       ((TZrUInt32)block[offset + 1U] << UINT32_C(16)) |
                       ((TZrUInt32)block[offset + 2U] << UINT32_C(8)) |
                       (TZrUInt32)block[offset + 3U];
    }
    for (TZrSize index = 16U; index < ZR_SHA256_ROUND_COUNT; index++) {
        words[index] = compile_tool_sha256_schedule1(words[index - 2U]) +
                       words[index - 7U] +
                       compile_tool_sha256_schedule0(words[index - 15U]) +
                       words[index - 16U];
    }
    memcpy(working, context->state, sizeof(working));
    for (TZrSize index = 0U; index < ZR_SHA256_ROUND_COUNT; index++) {
        TZrUInt32 first = working[7] + compile_tool_sha256_sum1(working[4]) +
                           compile_tool_sha256_choose(
                                   working[4], working[5], working[6]) +
                           g_compileToolSha256RoundConstants[index] + words[index];
        TZrUInt32 second = compile_tool_sha256_sum0(working[0]) +
                            compile_tool_sha256_majority(
                                    working[0], working[1], working[2]);

        working[7] = working[6];
        working[6] = working[5];
        working[5] = working[4];
        working[4] = working[3] + first;
        working[3] = working[2];
        working[2] = working[1];
        working[1] = working[0];
        working[0] = first + second;
    }
    for (TZrSize index = 0U;
         index < ZR_PARSER_SHA256_STATE_WORD_COUNT;
         index++) {
        context->state[index] += working[index];
    }
}

/** @brief 初始化流式 SHA-256 context，供 artifact、IR cache 和 compile-time cache 共用。 */
void ZrParser_Sha256_Init(SZrParserSha256Context *context) {
    if (context == ZR_NULL) {
        return;
    }
    memset(context, 0, sizeof(*context));
    context->state[0] = UINT32_C(0x6a09e667);
    context->state[1] = UINT32_C(0xbb67ae85);
    context->state[2] = UINT32_C(0x3c6ef372);
    context->state[3] = UINT32_C(0xa54ff53a);
    context->state[4] = UINT32_C(0x510e527f);
    context->state[5] = UINT32_C(0x9b05688c);
    context->state[6] = UINT32_C(0x1f83d9ab);
    context->state[7] = UINT32_C(0x5be0cd19);
}

/**
 * @brief 向增量摘要追加字节，累计长度限制为 SHA-256 64 位 bit count 可表示的范围。
 * @pre context 已由 Init 初始化；bytes 可空仅当 byteCount 为零。
 * @return 长度溢出或输入无效时 false；失败不追加该段内容。
 */
TZrBool ZrParser_Sha256_Update(
        SZrParserSha256Context *context,
        const TZrByte *bytes,
        TZrSize byteCount) {
    if (context == ZR_NULL || (bytes == ZR_NULL && byteCount != 0U) ||
        (TZrUInt64)byteCount >
                UINT64_MAX / UINT64_C(8) - context->totalByteCount) {
        return ZR_FALSE;
    }
    context->totalByteCount += (TZrUInt64)byteCount;
    for (TZrSize index = 0U; index < byteCount; index++) {
        context->block[context->blockByteCount++] = bytes[index];
        if (context->blockByteCount == ZR_PARSER_SHA256_BLOCK_BYTE_COUNT) {
            compile_tool_sha256_transform(context, context->block);
            context->blockByteCount = 0U;
        }
    }
    return ZR_TRUE;
}

/**
 * @brief 完成流式摘要并输出固定 32 字节结果；调用者负责确保 context 和 digest 有效。
 * @note Final 消费当前上下文；同一 context 不支持再次 Final 或继续 Update。
 */
void ZrParser_Sha256_Final(
        SZrParserSha256Context *context,
        TZrByte digest[ZR_PARSER_SHA256_DIGEST_BYTE_COUNT]) {
    TZrSize index = context->blockByteCount;

    context->block[index++] = UINT8_C(0x80);
    if (index > 56U) {
        while (index < ZR_PARSER_SHA256_BLOCK_BYTE_COUNT) {
            context->block[index++] = 0U;
        }
        compile_tool_sha256_transform(context, context->block);
        index = 0U;
    }
    while (index < 56U) {
        context->block[index++] = 0U;
    }
    {
        TZrUInt64 bitCount = context->totalByteCount * UINT64_C(8);

        for (TZrSize byteIndex = 0U; byteIndex < 8U; byteIndex++) {
            context->block[63U - byteIndex] =
                    (TZrByte)(bitCount >> (byteIndex * 8U));
        }
    }
    compile_tool_sha256_transform(context, context->block);
    for (TZrSize wordIndex = 0U;
         wordIndex < ZR_PARSER_SHA256_STATE_WORD_COUNT;
         wordIndex++) {
        for (TZrSize byteIndex = 0U; byteIndex < 4U; byteIndex++) {
            digest[wordIndex * 4U + byteIndex] = (TZrByte)(
                    context->state[wordIndex] >> (24U - byteIndex * 8U));
        }
    }
}

/**
 * @brief 将摘要格式化为 sha256: 前缀加无 padding base64url，匹配 compile-tool manifest 格式。
 * @pre digest 指向完整摘要；输出至少有 ZR_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH 字节。
 */
TZrBool ZrParser_Sha256_FormatDigest(
        const TZrByte digest[ZR_PARSER_SHA256_DIGEST_BYTE_COUNT],
        TZrChar *outHash,
        TZrSize outHashSize) {
    static const TZrChar alphabet[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    static const TZrChar prefix[] = "sha256:";
    TZrSize outputIndex = sizeof(prefix) - 1U;

    if (outHash == ZR_NULL ||
        outHashSize < ZR_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH) {
        return ZR_FALSE;
    }
    memcpy(outHash, prefix, sizeof(prefix) - 1U);
    for (TZrSize inputIndex = 0U;
         inputIndex + 3U <= ZR_PARSER_SHA256_DIGEST_BYTE_COUNT;
         inputIndex += 3U) {
        TZrUInt32 value = ((TZrUInt32)digest[inputIndex] << 16U) |
                           ((TZrUInt32)digest[inputIndex + 1U] << 8U) |
                           (TZrUInt32)digest[inputIndex + 2U];
        outHash[outputIndex++] = alphabet[(value >> 18U) & 63U];
        outHash[outputIndex++] = alphabet[(value >> 12U) & 63U];
        outHash[outputIndex++] = alphabet[(value >> 6U) & 63U];
        outHash[outputIndex++] = alphabet[value & 63U];
    }
    outHash[outputIndex++] = alphabet[(digest[30] >> 2U) & 63U];
    outHash[outputIndex++] = alphabet[
            ((TZrUInt32)(digest[30] & 3U) << 4U) |
            ((TZrUInt32)digest[31] >> 4U)];
    outHash[outputIndex++] = alphabet[(TZrUInt32)(digest[31] & 15U) << 2U];
    outHash[outputIndex] = '\0';
    return ZR_TRUE;
}

/**
 * @brief 对完整 artifact 内容生成可存入锁文件的 SHA-256 文本身份。
 * @pre bytes 可空仅当 byteCount 为零；outHash 必须提供规定的完整输出容量。
 * @return 无效输入、长度不可编码或输出容量不足时 false。
 */
TZrBool ZrParser_CompileToolContentHash_Bytes(
        const TZrByte *bytes,
        TZrSize byteCount,
        TZrChar *outHash,
        TZrSize outHashSize) {
    SZrParserSha256Context context;
    TZrByte digest[ZR_PARSER_SHA256_DIGEST_BYTE_COUNT];

    if ((bytes == ZR_NULL && byteCount != 0U) ||
        (TZrUInt64)byteCount > UINT64_MAX / UINT64_C(8)) {
        return ZR_FALSE;
    }
    ZrParser_Sha256_Init(&context);
    if (!ZrParser_Sha256_Update(&context, bytes, byteCount)) {
        return ZR_FALSE;
    }
    ZrParser_Sha256_Final(&context, digest);
    return ZrParser_Sha256_FormatDigest(
            digest, outHash, outHashSize);
}
