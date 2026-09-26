#include "metadata/zrp_metadata_dump.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/zrp_metadata.h"

/* 版本探测依赖当前格式约定的固定前缀；完整摘要仍交给核心元数据格式校验。 */
#define ZR_CLI_ZRP_METADATA_HEADER_PREFIX_SIZE 16u

/* 将核心节类别映射到 CLI 的稳定文本字段名，供两个摘要模式复用。 */
typedef struct SZrCliZrpMetadataDumpSectionInfo {
    const TZrChar *name;
    EZrZrpMetadataSectionKind kind;
} SZrCliZrpMetadataDumpSectionInfo;

/* 兼容性诊断只读取固定前缀，允许报告尚不能按当前版本解码的头。 */
typedef struct SZrCliZrpMetadataHeaderPrefix {
    TZrUInt32 magic;
    TZrUInt16 version;
    TZrUInt16 headerSize;
    TZrUInt32 flags;
    TZrUInt32 sectionCount;
} SZrCliZrpMetadataHeaderPrefix;

/* 摘要和差异共用此展示顺序；新增核心节时须同步名称与类别。 */
static const SZrCliZrpMetadataDumpSectionInfo g_zr_cli_zrp_metadata_dump_sections[] = {
        {"tokenRecords", ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS},
        {"typeDefs", ZR_ZRP_METADATA_SECTION_TYPE_DEFS},
        {"methodDefs", ZR_ZRP_METADATA_SECTION_METHOD_DEFS},
        {"fieldDefs", ZR_ZRP_METADATA_SECTION_FIELD_DEFS},
        {"genericParams", ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS},
        {"genericParamConstraints", ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS},
        {"typeSpecs", ZR_ZRP_METADATA_SECTION_TYPE_SPECS},
        {"methodSpecs", ZR_ZRP_METADATA_SECTION_METHOD_SPECS},
        {"moduleRefs", ZR_ZRP_METADATA_SECTION_MODULE_REFS},
        {"stringPool", ZR_ZRP_METADATA_SECTION_STRING_POOL},
        {"signatureBlobPool", ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL},
        {"constantPool", ZR_ZRP_METADATA_SECTION_CONSTANT_POOL},
        {"manifestExports", ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS},
};

static TZrUInt16 zrp_metadata_dump_read_u16(const TZrByte *buffer, TZrSize offset) {
    return (TZrUInt16)((TZrUInt16)buffer[offset] | ((TZrUInt16)buffer[offset + 1u] << 8u));
}

static TZrUInt32 zrp_metadata_dump_read_u32(const TZrByte *buffer, TZrSize offset) {
    return ((TZrUInt32)buffer[offset]) |
           ((TZrUInt32)buffer[offset + 1u] << 8u) |
           ((TZrUInt32)buffer[offset + 2u] << 16u) |
           ((TZrUInt32)buffer[offset + 3u] << 24u);
}

/* 写入辅助函数不接管缓冲区；公开写入入口先清空可选诊断，再在失败分支填入原因。 */
static void zrp_metadata_dump_write_error(TZrChar *buffer,
                                          TZrSize bufferSize,
                                          const TZrChar *format,
                                          ...) {
    va_list args;

    if (buffer == ZR_NULL || bufferSize == 0u || format == ZR_NULL) {
        return;
    }

    va_start(args, format);
    vsnprintf(buffer, bufferSize, format, args);
    va_end(args);
    buffer[bufferSize - 1u] = '\0';
}

/* 两种摘要使用同一节映射，保持字段名与核心头中实际节统计一一对应。 */
static const SZrZrpMetadataSection *zrp_metadata_dump_get_section(const SZrZrpMetadataHeader *header,
                                                                  EZrZrpMetadataSectionKind kind) {
    if (header == ZR_NULL) {
        return ZR_NULL;
    }

    switch (kind) {
        case ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS:
            return &header->tokenRecords;
        case ZR_ZRP_METADATA_SECTION_TYPE_DEFS:
            return &header->typeDefs;
        case ZR_ZRP_METADATA_SECTION_METHOD_DEFS:
            return &header->methodDefs;
        case ZR_ZRP_METADATA_SECTION_FIELD_DEFS:
            return &header->fieldDefs;
        case ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS:
            return &header->genericParams;
        case ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS:
            return &header->genericParamConstraints;
        case ZR_ZRP_METADATA_SECTION_TYPE_SPECS:
            return &header->typeSpecs;
        case ZR_ZRP_METADATA_SECTION_METHOD_SPECS:
            return &header->methodSpecs;
        case ZR_ZRP_METADATA_SECTION_MODULE_REFS:
            return &header->moduleRefs;
        case ZR_ZRP_METADATA_SECTION_STRING_POOL:
            return &header->stringPool;
        case ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL:
            return &header->signatureBlobPool;
        case ZR_ZRP_METADATA_SECTION_CONSTANT_POOL:
            return &header->constantPool;
        case ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS:
            return &header->manifestExports;
        default:
            return ZR_NULL;
    }
}

/* 裁剪量只表达减少的部分；新增字节或记录在 diff 输出中记为零而非负数。 */
static TZrUInt32 zrp_metadata_dump_removed_u32(TZrUInt32 beforeValue, TZrUInt32 afterValue) {
    return beforeValue > afterValue ? beforeValue - afterValue : 0u;
}

/* 先确认最短前缀长度，再读取小端字段；不以当前版本的完整布局为前提。 */
static TZrBool zrp_metadata_dump_read_header_prefix(const TZrByte *buffer,
                                                    TZrSize bufferLength,
                                                    SZrCliZrpMetadataHeaderPrefix *outPrefix) {
    if (buffer == ZR_NULL || outPrefix == ZR_NULL || bufferLength < ZR_CLI_ZRP_METADATA_HEADER_PREFIX_SIZE) {
        return ZR_FALSE;
    }

    outPrefix->magic = zrp_metadata_dump_read_u32(buffer, 0u);
    outPrefix->version = zrp_metadata_dump_read_u16(buffer, 4u);
    outPrefix->headerSize = zrp_metadata_dump_read_u16(buffer, 6u);
    outPrefix->flags = zrp_metadata_dump_read_u32(buffer, 8u);
    outPrefix->sectionCount = zrp_metadata_dump_read_u32(buffer, 12u);
    return ZR_TRUE;
}

/* diff 的两份输入使用同一核心校验，但保留 before/after 标签以定位失败侧。 */
static TZrBool zrp_metadata_dump_read_valid_header(const TZrByte *buffer,
                                                   TZrSize bufferLength,
                                                   const TZrChar *label,
                                                   SZrZrpMetadataHeader *outHeader,
                                                   TZrChar *errorBuffer,
                                                   TZrSize errorBufferSize) {
    if (buffer == ZR_NULL || outHeader == ZR_NULL) {
        zrp_metadata_dump_write_error(errorBuffer, errorBufferSize, "zrp metadata dump requires input buffer");
        return ZR_FALSE;
    }
    if (!ZrCore_ZrpMetadata_ReadHeader(buffer, bufferLength, outHeader) ||
        !ZrCore_ZrpMetadata_ValidateHeader(outHeader, bufferLength)) {
        if (label != ZR_NULL && label[0] != '\0') {
            zrp_metadata_dump_write_error(errorBuffer,
                                          errorBufferSize,
                                          "Invalid %s zrp metadata header",
                                          label);
        } else {
            zrp_metadata_dump_write_error(errorBuffer, errorBufferSize, "Invalid zrp metadata header");
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/* 单节文本行是 CLI 摘要的消费契约；取节失败与写流失败都向调用方传播。 */
static TZrBool zrp_metadata_dump_write_section_summary(FILE *output,
                                                       const SZrCliZrpMetadataDumpSectionInfo *info,
                                                       const SZrZrpMetadataHeader *header,
                                                       TZrChar *errorBuffer,
                                                       TZrSize errorBufferSize) {
    const SZrZrpMetadataSection *section;

    section = zrp_metadata_dump_get_section(header, info->kind);
    if (section == ZR_NULL) {
        zrp_metadata_dump_write_error(errorBuffer, errorBufferSize, "Invalid zrp metadata section kind");
        return ZR_FALSE;
    }

    if (fprintf(output,
                "zrp.metadata.section.%s bytes=%u count=%u elementSize=%u offset=%u\n",
                info->name,
                (unsigned int)section->byteLength,
                (unsigned int)section->count,
                (unsigned int)section->elementSize,
                (unsigned int)section->offset) < 0) {
        zrp_metadata_dump_write_error(errorBuffer, errorBufferSize, "Failed to write zrp metadata summary");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 差异行保留前后尺寸及偏移，供裁剪诊断区分大小变化与布局移动。 */
static TZrBool zrp_metadata_dump_write_section_diff_summary(FILE *output,
                                                            const SZrCliZrpMetadataDumpSectionInfo *info,
                                                            const SZrZrpMetadataHeader *beforeHeader,
                                                            const SZrZrpMetadataHeader *afterHeader,
                                                            TZrChar *errorBuffer,
                                                            TZrSize errorBufferSize) {
    const SZrZrpMetadataSection *beforeSection;
    const SZrZrpMetadataSection *afterSection;

    beforeSection = zrp_metadata_dump_get_section(beforeHeader, info->kind);
    afterSection = zrp_metadata_dump_get_section(afterHeader, info->kind);
    if (beforeSection == ZR_NULL || afterSection == ZR_NULL) {
        zrp_metadata_dump_write_error(errorBuffer, errorBufferSize, "Invalid zrp metadata section kind");
        return ZR_FALSE;
    }

    if (fprintf(output,
                "zrp.metadata.diff.section.%s "
                "bytesBefore=%u bytesAfter=%u bytesRemoved=%u "
                "countBefore=%u countAfter=%u countRemoved=%u "
                "elementSizeBefore=%u elementSizeAfter=%u offsetBefore=%u offsetAfter=%u\n",
                info->name,
                (unsigned int)beforeSection->byteLength,
                (unsigned int)afterSection->byteLength,
                (unsigned int)zrp_metadata_dump_removed_u32(beforeSection->byteLength, afterSection->byteLength),
                (unsigned int)beforeSection->count,
                (unsigned int)afterSection->count,
                (unsigned int)zrp_metadata_dump_removed_u32(beforeSection->count, afterSection->count),
                (unsigned int)beforeSection->elementSize,
                (unsigned int)afterSection->elementSize,
                (unsigned int)beforeSection->offset,
                (unsigned int)afterSection->offset) < 0) {
        zrp_metadata_dump_write_error(errorBuffer, errorBufferSize, "Failed to write zrp metadata diff summary");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrCli_ZrpMetadataDump_WriteSummary(FILE *output,
                                           const TZrByte *buffer,
                                           TZrSize bufferLength,
                                           TZrChar *errorBuffer,
                                           TZrSize errorBufferSize) {
    SZrZrpMetadataHeader header;
    TZrSize index;

    if (errorBuffer != ZR_NULL && errorBufferSize > 0u) {
        errorBuffer[0] = '\0';
    }
    if (output == ZR_NULL || buffer == ZR_NULL) {
        zrp_metadata_dump_write_error(errorBuffer, errorBufferSize, "zrp metadata dump requires output and input buffer");
        return ZR_FALSE;
    }
    if (!ZrCore_ZrpMetadata_ReadHeader(buffer, bufferLength, &header) ||
        !ZrCore_ZrpMetadata_ValidateHeader(&header, bufferLength)) {
        zrp_metadata_dump_write_error(errorBuffer, errorBufferSize, "Invalid zrp metadata header");
        return ZR_FALSE;
    }

    if (fprintf(output,
                "zrp.metadata.version=%u\n"
                "zrp.metadata.headerBytes=%u\n"
                "zrp.metadata.sectionCount=%u\n",
                (unsigned int)header.version,
                (unsigned int)header.headerSize,
                (unsigned int)header.sectionCount) < 0) {
        zrp_metadata_dump_write_error(errorBuffer, errorBufferSize, "Failed to write zrp metadata summary");
        return ZR_FALSE;
    }

    for (index = 0u; index < (TZrSize)(sizeof(g_zr_cli_zrp_metadata_dump_sections) /
                                       sizeof(g_zr_cli_zrp_metadata_dump_sections[0])); ++index) {
        if (!zrp_metadata_dump_write_section_summary(output,
                                                     &g_zr_cli_zrp_metadata_dump_sections[index],
                                                     &header,
                                                     errorBuffer,
                                                     errorBufferSize)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrCli_ZrpMetadataDump_WriteDiffSummary(FILE *output,
                                               const TZrByte *beforeBuffer,
                                               TZrSize beforeBufferLength,
                                               const TZrByte *afterBuffer,
                                               TZrSize afterBufferLength,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize) {
    SZrZrpMetadataHeader beforeHeader;
    SZrZrpMetadataHeader afterHeader;
    TZrSize index;

    if (errorBuffer != ZR_NULL && errorBufferSize > 0u) {
        errorBuffer[0] = '\0';
    }
    if (output == ZR_NULL || beforeBuffer == ZR_NULL || afterBuffer == ZR_NULL) {
        zrp_metadata_dump_write_error(errorBuffer, errorBufferSize, "zrp metadata diff requires output and input buffers");
        return ZR_FALSE;
    }
    /* 两份输入都通过当前格式校验后才写首行，避免把坏的 after 文件展示为有效差异。 */
    if (!zrp_metadata_dump_read_valid_header(beforeBuffer,
                                             beforeBufferLength,
                                             "before",
                                             &beforeHeader,
                                             errorBuffer,
                                             errorBufferSize)) {
        return ZR_FALSE;
    }
    if (!zrp_metadata_dump_read_valid_header(afterBuffer,
                                             afterBufferLength,
                                             "after",
                                             &afterHeader,
                                             errorBuffer,
                                             errorBufferSize)) {
        return ZR_FALSE;
    }

    if (fprintf(output,
                "zrp.metadata.diff.versionBefore=%u versionAfter=%u\n"
                "zrp.metadata.diff.headerBytesBefore=%u headerBytesAfter=%u\n"
                "zrp.metadata.diff.sectionCountBefore=%u sectionCountAfter=%u\n",
                (unsigned int)beforeHeader.version,
                (unsigned int)afterHeader.version,
                (unsigned int)beforeHeader.headerSize,
                (unsigned int)afterHeader.headerSize,
                (unsigned int)beforeHeader.sectionCount,
                (unsigned int)afterHeader.sectionCount) < 0) {
        zrp_metadata_dump_write_error(errorBuffer, errorBufferSize, "Failed to write zrp metadata diff summary");
        return ZR_FALSE;
    }

    for (index = 0u; index < (TZrSize)(sizeof(g_zr_cli_zrp_metadata_dump_sections) /
                                       sizeof(g_zr_cli_zrp_metadata_dump_sections[0])); ++index) {
        if (!zrp_metadata_dump_write_section_diff_summary(output,
                                                          &g_zr_cli_zrp_metadata_dump_sections[index],
                                                          &beforeHeader,
                                                          &afterHeader,
                                                          errorBuffer,
                                                          errorBufferSize)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrCli_ZrpMetadataDump_WriteVersionCheck(FILE *output,
                                                const TZrByte *buffer,
                                                TZrSize bufferLength,
                                                TZrChar *errorBuffer,
                                                TZrSize errorBufferSize) {
    SZrCliZrpMetadataHeaderPrefix prefix;
    SZrZrpMetadataHeader header;
    TZrBool isCurrentShape;

    if (errorBuffer != ZR_NULL && errorBufferSize > 0u) {
        errorBuffer[0] = '\0';
    }
    if (output == ZR_NULL || buffer == ZR_NULL) {
        zrp_metadata_dump_write_error(errorBuffer,
                                      errorBufferSize,
                                      "zrp metadata version check requires output and input buffer");
        return ZR_FALSE;
    }
    if (!zrp_metadata_dump_read_header_prefix(buffer, bufferLength, &prefix)) {
        zrp_metadata_dump_write_error(errorBuffer, errorBufferSize, "Invalid zrp metadata header");
        return ZR_FALSE;
    }

    /* 前缀可诊断未知版本；只有匹配当前形状时才进入完整解码与节范围校验。 */
    isCurrentShape = (TZrBool)(prefix.magic == ZR_ZRP_METADATA_MAGIC &&
                              prefix.version == ZR_ZRP_METADATA_VERSION &&
                              prefix.headerSize == ZR_ZRP_METADATA_HEADER_SIZE &&
                              prefix.sectionCount == ZR_ZRP_METADATA_SECTION_COUNT &&
                              ZrCore_ZrpMetadata_ReadHeader(buffer, bufferLength, &header) &&
                              ZrCore_ZrpMetadata_ValidateHeader(&header, bufferLength));

    if (fprintf(output,
                "zrp.metadata.versionCheck.status=%s\n"
                "zrp.metadata.versionCheck.magic=%u expectedMagic=%u\n"
                "zrp.metadata.versionCheck.version=%u expectedVersion=%u\n"
                "zrp.metadata.versionCheck.headerBytes=%u expectedHeaderBytes=%u\n"
                "zrp.metadata.versionCheck.sectionCount=%u expectedSectionCount=%u\n",
                isCurrentShape ? "ok" : "unsupported",
                (unsigned int)prefix.magic,
                (unsigned int)ZR_ZRP_METADATA_MAGIC,
                (unsigned int)prefix.version,
                (unsigned int)ZR_ZRP_METADATA_VERSION,
                (unsigned int)prefix.headerSize,
                (unsigned int)ZR_ZRP_METADATA_HEADER_SIZE,
                (unsigned int)prefix.sectionCount,
                (unsigned int)ZR_ZRP_METADATA_SECTION_COUNT) < 0) {
        zrp_metadata_dump_write_error(errorBuffer, errorBufferSize, "Failed to write zrp metadata version check");
        return ZR_FALSE;
    }

    /* 不支持的头也先输出 status 与实际字段，随后以非零状态告知自动化调用方。 */
    if (!isCurrentShape) {
        zrp_metadata_dump_write_error(errorBuffer,
                                      errorBufferSize,
                                      "Unsupported zrp metadata version or header shape");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 路径入口统一持有原始字节，成功后交给 Write*；失败不会转移文件或缓冲区所有权。 */
static int zrp_metadata_dump_read_file(const TZrChar *path,
                                       TZrByte **outBuffer,
                                       TZrSize *outBufferLength,
                                       FILE *errorOutput) {
    FILE *file;
    long fileLength;
    TZrByte *buffer;
    TZrSize readLength;

    if (path == ZR_NULL || path[0] == '\0') {
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "Missing zrp metadata file path\n");
        }
        return 1;
    }
    if (outBuffer == ZR_NULL || outBufferLength == ZR_NULL) {
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "Internal zrp metadata file read error\n");
        }
        return 1;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "Failed to open zrp metadata file: %s\n", path);
        }
        return 1;
    }
    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "Failed to seek zrp metadata file: %s\n", path);
        }
        return 1;
    }
    fileLength = ftell(file);
    if (fileLength <= 0L) {
        fclose(file);
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "Invalid zrp metadata file: %s\n", path);
        }
        return 1;
    }
    if (fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "Failed to rewind zrp metadata file: %s\n", path);
        }
        return 1;
    }

    buffer = (TZrByte *)malloc((size_t)fileLength);
    if (buffer == ZR_NULL) {
        fclose(file);
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "Failed to allocate zrp metadata buffer\n");
        }
        return 1;
    }

    readLength = (TZrSize)fread(buffer, 1u, (size_t)fileLength, file);
    fclose(file);
    if (readLength != (TZrSize)fileLength) {
        free(buffer);
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "Failed to read zrp metadata file: %s\n", path);
        }
        return 1;
    }

    *outBuffer = buffer;
    *outBufferLength = readLength;
    return 0;
}

/* TODO: 命令帮助示例使用 module.zrp，但普通项目 .zrp 由 project.c 按 JSON 解析，
 * 此处及另外两个路径入口只接受原始二进制元数据头。AOT 后端会在内存中生成或
 * 重写 metadata blob，尚需核实供 CLI 使用的独立磁盘文件来源，再决定命令应定位
 * 项目内元数据，还是将帮助示例改为明确的二进制输入。
 * 证据：command.c 示例、project.c 的 cJSON_Parse、backend_aot_c_zrp_metadata_*.c
 * 的 WriteHeader 调用，以及 WriteSummary/VersionCheck 的头校验。
 */
int ZrCli_ZrpMetadataDump_RunPath(const TZrChar *path, FILE *output, FILE *errorOutput) {
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0u;
    TZrChar error[ZR_CLI_ERROR_BUFFER_LENGTH];

    if (path == ZR_NULL || path[0] == '\0') {
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "Missing zrp metadata file path\n");
        }
        return 1;
    }
    if (output == ZR_NULL) {
        output = stdout;
    }
    if (errorOutput == ZR_NULL) {
        errorOutput = stderr;
    }

    if (zrp_metadata_dump_read_file(path, &buffer, &bufferLength, errorOutput) != 0) {
        return 1;
    }

    if (!ZrCli_ZrpMetadataDump_WriteSummary(output, buffer, bufferLength, error, sizeof(error))) {
        free(buffer);
        fprintf(errorOutput, "%s\n", error);
        return 1;
    }

    free(buffer);
    return 0;
}

int ZrCli_ZrpMetadataDump_RunDiffPath(const TZrChar *beforePath,
                                      const TZrChar *afterPath,
                                      FILE *output,
                                      FILE *errorOutput) {
    TZrByte *beforeBuffer = ZR_NULL;
    TZrByte *afterBuffer = ZR_NULL;
    TZrSize beforeBufferLength = 0u;
    TZrSize afterBufferLength = 0u;
    TZrChar error[ZR_CLI_ERROR_BUFFER_LENGTH];

    if (beforePath == ZR_NULL || beforePath[0] == '\0' || afterPath == ZR_NULL || afterPath[0] == '\0') {
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "Missing zrp metadata diff file path\n");
        }
        return 1;
    }
    if (output == ZR_NULL) {
        output = stdout;
    }
    if (errorOutput == ZR_NULL) {
        errorOutput = stderr;
    }

    if (zrp_metadata_dump_read_file(beforePath, &beforeBuffer, &beforeBufferLength, errorOutput) != 0) {
        return 1;
    }
    if (zrp_metadata_dump_read_file(afterPath, &afterBuffer, &afterBufferLength, errorOutput) != 0) {
        free(beforeBuffer);
        return 1;
    }

    if (!ZrCli_ZrpMetadataDump_WriteDiffSummary(output,
                                                beforeBuffer,
                                                beforeBufferLength,
                                                afterBuffer,
                                                afterBufferLength,
                                                error,
                                                sizeof(error))) {
        free(beforeBuffer);
        free(afterBuffer);
        fprintf(errorOutput, "%s\n", error);
        return 1;
    }

    free(beforeBuffer);
    free(afterBuffer);
    return 0;
}

int ZrCli_ZrpMetadataDump_RunVersionCheckPath(const TZrChar *path, FILE *output, FILE *errorOutput) {
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0u;
    TZrChar error[ZR_CLI_ERROR_BUFFER_LENGTH];

    if (path == ZR_NULL || path[0] == '\0') {
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "Missing zrp metadata file path\n");
        }
        return 1;
    }
    if (output == ZR_NULL) {
        output = stdout;
    }
    if (errorOutput == ZR_NULL) {
        errorOutput = stderr;
    }

    if (zrp_metadata_dump_read_file(path, &buffer, &bufferLength, errorOutput) != 0) {
        return 1;
    }

    if (!ZrCli_ZrpMetadataDump_WriteVersionCheck(output, buffer, bufferLength, error, sizeof(error))) {
        free(buffer);
        fprintf(errorOutput, "%s\n", error);
        return 1;
    }

    free(buffer);
    return 0;
}
