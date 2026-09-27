#include "zr_vm_core/artifact_schema.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "artifact_schema_internal.h"

/* 借用调用方文本缓冲区；length 不含结尾 NUL，capacity 始终含其空间。 */
typedef struct SZrArtifactTextWriter {
    TZrChar *buffer;
    TZrSize capacity;
    TZrSize length;
} SZrArtifactTextWriter;

/* 每次追加后保留 NUL 终止；容量不足时不追加当前字节块。 */
static TZrBool artifact_text_append_bytes(SZrArtifactTextWriter *writer,
                                          const TZrChar *bytes,
                                          TZrSize length) {
    if (writer->length + length >= writer->capacity) {
        return ZR_FALSE;
    }
    if (length > 0u) {
        memcpy(writer->buffer + writer->length, bytes, length);
        writer->length += length;
    }
    writer->buffer[writer->length] = '\0';
    return ZR_TRUE;
}

/* 格式化结果只有完整落入剩余容量才计入 length；失败缓冲区可能已有前缀。 */
static TZrBool artifact_text_append_format(SZrArtifactTextWriter *writer,
                                           const TZrChar *format,
                                           ...) {
    va_list arguments;
    int written;

    if (writer->length >= writer->capacity) {
        return ZR_FALSE;
    }
    va_start(arguments, format);
    written = vsnprintf(writer->buffer + writer->length,
                        writer->capacity - writer->length,
                        format,
                        arguments);
    va_end(arguments);
    if (written < 0 || (TZrSize)written >= writer->capacity - writer->length) {
        return ZR_FALSE;
    }
    writer->length += (TZrSize)written;
    return ZR_TRUE;
}

static const TZrChar *artifact_kind_name(EZrArtifactKind kind) {
    switch (kind) {
        case ZR_ARTIFACT_KIND_ZRS:
            return "zrs";
        case ZR_ARTIFACT_KIND_ZRI:
            return "zri";
        case ZR_ARTIFACT_KIND_ZRO:
            return "zro";
        default:
            return "invalid";
    }
}

/* 节摘要仅供人阅读；语法树与语义 IR 的原文字节不作为读端的权威输入。 */
static TZrBool artifact_text_append_section_preview(SZrArtifactTextWriter *writer,
                                                    const SZrArtifactSectionView *section) {
    const TZrChar *name = ZrCore_Artifact_SectionName(section->kind);

    if (!artifact_text_append_format(writer,
                                     "section name=%s flags=%s count=%u element-size=%u bytes=%u\n",
                                     name,
                                     (section->flags & ZR_ARTIFACT_SECTION_FLAG_OPTIONAL) != 0u
                                             ? "optional"
                                             : "mandatory",
                                     section->elementCount,
                                     section->elementSize,
                                     section->byteLength)) {
        return ZR_FALSE;
    }
    if (section->kind == ZR_ARTIFACT_SECTION_SYNTAX_TREE ||
        section->kind == ZR_ARTIFACT_SECTION_SEMANTIC_IR) {
        if (!artifact_text_append_format(writer, "content-begin %s\n", name) ||
            !artifact_text_append_bytes(writer, (const TZrChar *)section->data, section->byteLength)) {
            return ZR_FALSE;
        }
        if (section->byteLength == 0u || section->data[section->byteLength - 1u] != (TZrByte)'\n') {
            if (!artifact_text_append_bytes(writer, "\n", 1u)) {
                return ZR_FALSE;
            }
        }
        if (!artifact_text_append_format(writer, "content-end %s\n", name)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/** @brief 将已编码的 artifact 投影为可读摘要及可精确往返的十六进制 payload。
 * @note 重新 Read 输入以验证二进制；输出缓冲区归调用方，失败时已写前缀可能保留，
 * 但 *outWrittenSize 为零，诊断可省略。 */
EZrArtifactStatus ZrCore_Artifact_WriteText(const SZrArtifactView *view,
                                            TZrChar *buffer,
                                            TZrSize bufferCapacity,
                                            TZrSize *outWrittenSize,
                                            SZrArtifactDiagnostic *diagnostic) {
    static const TZrChar hexDigits[] = "0123456789abcdef";
    SZrArtifactView validatedView;
    SZrArtifactTextWriter writer;
    TZrUInt32 index;
    TZrSize byteIndex;

    zr_artifact_diagnostic_clear(diagnostic);
    if (outWrittenSize != ZR_NULL) {
        *outWrittenSize = 0u;
    }
    if (view == ZR_NULL || view->buffer == ZR_NULL || buffer == ZR_NULL ||
        bufferCapacity == 0u || outWrittenSize == ZR_NULL ||
        !zr_artifact_kind_is_valid(view->kind)) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u);
    }
    {
        EZrArtifactStatus status = ZrCore_Artifact_Read(
                view->buffer, view->bufferLength, &validatedView, diagnostic);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
        view = &validatedView;
    }
    writer.buffer = buffer;
    writer.capacity = bufferCapacity;
    writer.length = 0u;
    /* TODO: 核定 buffer 与 view->buffer 是否允许重叠；此后仍从源读节和 payload，
     * 而这里已开始改写目标，当前仓内调用使用独立缓冲区。 */
    buffer[0] = '\0';

    if (!artifact_text_append_format(&writer,
                                     "zr-artifact schema=%u kind=%s bytes=%u\n",
                                     (TZrUInt32)ZR_ARTIFACT_SCHEMA_VERSION,
                                     artifact_kind_name(view->kind),
                                     (TZrUInt32)view->bufferLength) ||
        !artifact_text_append_format(&writer,
                                     "identity type-id=%u type-ref=%08x type-spec=%08x signature=%08x "
                                     "type-ref-hash=%016llx type-spec-hash=%016llx signature-hash=%016llx "
                                     "layout-version=%u layout-hash=%016llx contract-hash=%016llx "
                                     "module-hash=%016llx\n",
                                     view->identity.canonicalTypeId,
                                     view->identity.typeRefToken,
                                     view->identity.typeSpecToken,
                                     view->identity.signatureToken,
                                     (unsigned long long)view->identity.typeRefHash,
                                     (unsigned long long)view->identity.typeSpecHash,
                                     (unsigned long long)view->identity.signatureHash,
                                     view->identity.layoutVersion,
                                     (unsigned long long)view->identity.layoutHash,
                                     (unsigned long long)view->identity.callableContractHash,
                                     (unsigned long long)view->identity.moduleHash)) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL, 0u, 0u, 0u);
    }

    for (index = 0u; index < view->sectionCount; ++index) {
        SZrArtifactSectionView section;
        if (zr_artifact_decode_directory_entry(view, index, &section, diagnostic) != ZR_ARTIFACT_STATUS_OK ||
            !artifact_text_append_section_preview(&writer, &section)) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL,
                                    section.kind,
                                    index,
                                    0u);
        }
    }

    /* 预览原文可能含相同标记；读端选最后一个 payload-hex 作为二进制来源。 */
    if (!artifact_text_append_bytes(&writer, "payload-hex=", 12u)) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL, 0u, 0u, 0u);
    }
    for (byteIndex = 0u; byteIndex < view->bufferLength; ++byteIndex) {
        TZrChar encoded[2];
        encoded[0] = hexDigits[(view->buffer[byteIndex] >> 4u) & 0x0fu];
        encoded[1] = hexDigits[view->buffer[byteIndex] & 0x0fu];
        if (!artifact_text_append_bytes(&writer, encoded, 2u)) {
            return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL, 0u, 0u, 0u);
        }
    }
    if (!artifact_text_append_bytes(&writer, "\nend-artifact\n", 14u)) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL, 0u, 0u, 0u);
    }
    *outWrittenSize = writer.length;
    return ZR_ARTIFACT_STATUS_OK;
}

/* 从显式长度的文本中取最后一次匹配，避开 syntax/SemIR 预览中的同名标记。 */
static TZrSize artifact_text_find(const TZrChar *text,
                                  TZrSize textLength,
                                  const TZrChar *needle,
                                  TZrSize needleLength) {
    TZrSize index;
    TZrSize result = ZR_MAX_SIZE;
    if (needleLength == 0u || textLength < needleLength) {
        return ZR_MAX_SIZE;
    }
    for (index = 0u; index <= textLength - needleLength; ++index) {
        if (memcmp(text + index, needle, needleLength) == 0) {
            result = index;
        }
    }
    return result;
}

static TZrInt32 artifact_text_hex_value(TZrChar value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

/** @brief 从最后一次出现的 payload-hex 恢复二进制并交给 schema Read 再次验证。
 * @note 可读 header 与节预览不用于重建；输入、输出均由调用方持有且按显式长度访问。
 * @return 失败时 *outWrittenSize 为零，buffer 可能保留已解出的前缀。 */
EZrArtifactStatus ZrCore_Artifact_ReadText(const TZrChar *text,
                                           TZrSize textLength,
                                           TZrByte *buffer,
                                           TZrSize bufferCapacity,
                                           TZrSize *outWrittenSize,
                                           SZrArtifactDiagnostic *diagnostic) {
    static const TZrChar payloadPrefix[] = "payload-hex=";
    SZrArtifactView view;
    TZrSize payloadOffset;
    TZrSize payloadEnd;
    TZrSize hexLength;
    TZrSize outputLength;
    TZrSize index;
    EZrArtifactStatus status;

    zr_artifact_diagnostic_clear(diagnostic);
    if (outWrittenSize != ZR_NULL) {
        *outWrittenSize = 0u;
    }
    if (text == ZR_NULL || buffer == ZR_NULL || outWrittenSize == ZR_NULL) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u);
    }
    payloadOffset = artifact_text_find(text,
                                       textLength,
                                       payloadPrefix,
                                       sizeof(payloadPrefix) - 1u);
    if (payloadOffset == ZR_MAX_SIZE) {
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_TEXT, 0u, 0u, 0u);
    }
    payloadOffset += sizeof(payloadPrefix) - 1u;
    payloadEnd = payloadOffset;
    while (payloadEnd < textLength && text[payloadEnd] != '\n' && text[payloadEnd] != '\r') {
        ++payloadEnd;
    }
    hexLength = payloadEnd - payloadOffset;
    if ((hexLength & 1u) != 0u) {
        return zr_artifact_fail(diagnostic,
                                ZR_ARTIFACT_STATUS_INVALID_TEXT,
                                0u,
                                0u,
                                (TZrUInt32)payloadOffset);
    }
    outputLength = hexLength / 2u;
    if (outputLength > bufferCapacity || outputLength > ZR_ARTIFACT_MAX_BYTE_LENGTH) {
        return zr_artifact_fail(diagnostic,
                                outputLength > ZR_ARTIFACT_MAX_BYTE_LENGTH
                                        ? ZR_ARTIFACT_STATUS_COUNT_LIMIT
                                        : ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL,
                                0u,
                                0u,
                                (TZrUInt32)payloadOffset);
    }
    /* TODO: 核定 text 与 buffer 是否允许重叠；逐字节写入会影响后续十六进制读取，
     * 仓内往返测试使用独立输入和输出缓冲区。 */
    for (index = 0u; index < outputLength; ++index) {
        TZrInt32 high = artifact_text_hex_value(text[payloadOffset + index * 2u]);
        TZrInt32 low = artifact_text_hex_value(text[payloadOffset + index * 2u + 1u]);
        if (high < 0 || low < 0) {
            return zr_artifact_fail(diagnostic,
                                    ZR_ARTIFACT_STATUS_INVALID_TEXT,
                                    0u,
                                    0u,
                                    (TZrUInt32)(payloadOffset + index * 2u));
        }
        buffer[index] = (TZrByte)((high << 4) | low);
    }
    /* 十六进制只证明字符可解码；二进制结构及身份仍以共用 Read 为准。 */
    status = ZrCore_Artifact_Read(buffer, outputLength, &view, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }
    *outWrittenSize = outputLength;
    return ZR_ARTIFACT_STATUS_OK;
}
