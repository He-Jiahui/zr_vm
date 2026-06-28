#include <stdio.h>
#include <string.h>

#include "metadata/zrp_metadata_dump.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/zrp_metadata.h"

#define DUMP_ASSERT_TRUE(condition, message)                                                                          \
    do {                                                                                                              \
        if (!(condition)) {                                                                                           \
            fprintf(stderr, "assertion failed: %s\n", message);                                                       \
            return 1;                                                                                                 \
        }                                                                                                             \
    } while (0)

#define DUMP_ASSERT_CONTAINS(text, fragment, message)                                                                 \
    do {                                                                                                              \
        if (strstr((text), (fragment)) == ZR_NULL) {                                                                  \
            fprintf(stderr, "assertion failed: %s (missing=%s)\n", message, fragment);                                \
            return 1;                                                                                                 \
        }                                                                                                             \
    } while (0)

static void set_counted_section(SZrZrpMetadataSection *section,
                                TZrUInt32 *nextOffset,
                                TZrUInt32 count,
                                TZrUInt32 elementSize) {
    section->offset = *nextOffset;
    section->count = count;
    section->elementSize = elementSize;
    section->byteLength = count * elementSize;
    *nextOffset += section->byteLength;
}

static int read_stream_text(FILE *file, char *buffer, size_t bufferLength) {
    size_t readLength;

    DUMP_ASSERT_TRUE(file != ZR_NULL, "file should not be null");
    DUMP_ASSERT_TRUE(buffer != ZR_NULL && bufferLength > 0u, "buffer should be writable");

    fflush(file);
    rewind(file);
    readLength = fread(buffer, 1u, bufferLength - 1u, file);
    buffer[readLength] = '\0';
    return 0;
}

static void build_summary_metadata(TZrByte *bytes,
                                   size_t byteLength,
                                   SZrZrpMetadataHeader *outHeader,
                                   TZrUInt32 typeDefCount,
                                   TZrUInt32 methodDefCount,
                                   TZrUInt32 stringPoolBytes,
                                   TZrUInt32 constantPoolBytes) {
    SZrZrpMetadataHeader header;
    TZrUInt32 nextOffset;

    memset(bytes, 0, byteLength);
    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.tokenRecords,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrMetadataTokenRecord));
    if (typeDefCount > 0u) {
        set_counted_section(&header.typeDefs,
                            &nextOffset,
                            typeDefCount,
                            (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    }
    if (methodDefCount > 0u) {
        set_counted_section(&header.methodDefs,
                            &nextOffset,
                            methodDefCount,
                            (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow));
    }
    if (stringPoolBytes > 0u) {
        set_counted_section(&header.stringPool, &nextOffset, stringPoolBytes, 1u);
    }
    if (constantPoolBytes > 0u) {
        set_counted_section(&header.constantPool, &nextOffset, constantPoolBytes, 1u);
    }

    if (outHeader != ZR_NULL) {
        *outHeader = header;
    }
    (void)ZrCore_ZrpMetadata_WriteHeader(bytes, (TZrSize)byteLength, &header);
}

static int test_summary_prints_metadata_section_bytes_and_counts(void) {
    SZrZrpMetadataHeader header;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + 1024u] = {0};
    char error[256] = {0};
    char outputText[4096];
    char expected[256];
    const char *testPath = "zr_cli_zrp_metadata_dump_test.zrp";
    FILE *output;
    FILE *input;

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.tokenRecords,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrMetadataTokenRecord));
    set_counted_section(&header.typeDefs,
                        &nextOffset,
                        2u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_counted_section(&header.methodDefs,
                        &nextOffset,
                        3u,
                        (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow));
    set_counted_section(&header.stringPool, &nextOffset, 12u, 1u);

    DUMP_ASSERT_TRUE(nextOffset <= (TZrUInt32)sizeof(bytes), "test metadata should fit in buffer");
    DUMP_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header), "header should write");

    output = tmpfile();
    DUMP_ASSERT_TRUE(output != ZR_NULL, "tmpfile should be available");
    DUMP_ASSERT_TRUE(ZrCli_ZrpMetadataDump_WriteSummary(output, bytes, sizeof(bytes), error, sizeof(error)),
                     "metadata summary should write");
    if (read_stream_text(output, outputText, sizeof(outputText)) != 0) {
        fclose(output);
        return 1;
    }
    fclose(output);

    DUMP_ASSERT_CONTAINS(outputText, "zrp.metadata.version=2", "version marker should be present");
    DUMP_ASSERT_CONTAINS(outputText, "zrp.metadata.headerBytes=208", "header size marker should be present");
    DUMP_ASSERT_CONTAINS(outputText, "zrp.metadata.sectionCount=12", "section count marker should be present");

    snprintf(expected,
             sizeof(expected),
             "zrp.metadata.section.typeDefs bytes=%u count=2 elementSize=%u offset=%u",
             (unsigned int)(2u * (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow)),
             (unsigned int)sizeof(SZrZrpMetadataTypeDefRow),
             (unsigned int)header.typeDefs.offset);
    DUMP_ASSERT_CONTAINS(outputText, expected, "typeDefs section should include bytes and count");

    snprintf(expected,
             sizeof(expected),
             "zrp.metadata.section.methodDefs bytes=%u count=3 elementSize=%u offset=%u",
             (unsigned int)(3u * (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow)),
             (unsigned int)sizeof(SZrZrpMetadataMethodDefRow),
             (unsigned int)header.methodDefs.offset);
    DUMP_ASSERT_CONTAINS(outputText, expected, "methodDefs section should include bytes and count");

    snprintf(expected,
             sizeof(expected),
             "zrp.metadata.section.stringPool bytes=12 count=12 elementSize=1 offset=%u",
             (unsigned int)header.stringPool.offset);
    DUMP_ASSERT_CONTAINS(outputText, expected, "stringPool section should include bytes and count");

    input = fopen(testPath, "wb");
    DUMP_ASSERT_TRUE(input != ZR_NULL, "test zrp file should open for write");
    DUMP_ASSERT_TRUE(fwrite(bytes, 1u, sizeof(bytes), input) == sizeof(bytes), "test zrp file should write");
    fclose(input);

    output = tmpfile();
    DUMP_ASSERT_TRUE(output != ZR_NULL, "tmpfile should be available for path summary");
    DUMP_ASSERT_TRUE(ZrCli_ZrpMetadataDump_RunPath(testPath, output, stderr) == 0,
                     "metadata dump path should run");
    if (read_stream_text(output, outputText, sizeof(outputText)) != 0) {
        fclose(output);
        remove(testPath);
        return 1;
    }
    fclose(output);
    remove(testPath);
    DUMP_ASSERT_CONTAINS(outputText, expected, "path summary should include metadata section bytes and count");

    return 0;
}

static int test_diff_prints_metadata_section_deltas(void) {
    SZrZrpMetadataHeader beforeHeader;
    SZrZrpMetadataHeader afterHeader;
    TZrByte beforeBytes[ZR_ZRP_METADATA_HEADER_SIZE + 1024u];
    TZrByte afterBytes[ZR_ZRP_METADATA_HEADER_SIZE + 1024u];
    char error[256] = {0};
    char outputText[4096];
    char expected[256];
    const char *beforePath = "zr_cli_zrp_metadata_diff_before.zrp";
    const char *afterPath = "zr_cli_zrp_metadata_diff_after.zrp";
    FILE *output;
    FILE *input;

    build_summary_metadata(beforeBytes, sizeof(beforeBytes), &beforeHeader, 2u, 3u, 12u, 4u);
    build_summary_metadata(afterBytes, sizeof(afterBytes), &afterHeader, 1u, 1u, 5u, 9u);

    output = tmpfile();
    DUMP_ASSERT_TRUE(output != ZR_NULL, "tmpfile should be available");
    DUMP_ASSERT_TRUE(ZrCli_ZrpMetadataDump_WriteDiffSummary(output,
                                                            beforeBytes,
                                                            sizeof(beforeBytes),
                                                            afterBytes,
                                                            sizeof(afterBytes),
                                                            error,
                                                            sizeof(error)),
                     "metadata diff summary should write");
    if (read_stream_text(output, outputText, sizeof(outputText)) != 0) {
        fclose(output);
        return 1;
    }
    fclose(output);

    DUMP_ASSERT_CONTAINS(outputText,
                         "zrp.metadata.diff.versionBefore=2 versionAfter=2",
                         "version diff marker should be present");
    DUMP_ASSERT_CONTAINS(outputText,
                         "zrp.metadata.diff.headerBytesBefore=208 headerBytesAfter=208",
                         "header bytes diff marker should be present");
    DUMP_ASSERT_CONTAINS(outputText,
                         "zrp.metadata.diff.sectionCountBefore=12 sectionCountAfter=12",
                         "section count diff marker should be present");

    snprintf(expected,
             sizeof(expected),
             "zrp.metadata.diff.section.typeDefs bytesBefore=%u bytesAfter=%u bytesRemoved=%u countBefore=2 countAfter=1 countRemoved=1",
             (unsigned int)beforeHeader.typeDefs.byteLength,
             (unsigned int)afterHeader.typeDefs.byteLength,
             (unsigned int)(beforeHeader.typeDefs.byteLength - afterHeader.typeDefs.byteLength));
    DUMP_ASSERT_CONTAINS(outputText, expected, "typeDefs diff should include removed bytes and count");

    snprintf(expected,
             sizeof(expected),
             "zrp.metadata.diff.section.methodDefs bytesBefore=%u bytesAfter=%u bytesRemoved=%u countBefore=3 countAfter=1 countRemoved=2",
             (unsigned int)beforeHeader.methodDefs.byteLength,
             (unsigned int)afterHeader.methodDefs.byteLength,
             (unsigned int)(beforeHeader.methodDefs.byteLength - afterHeader.methodDefs.byteLength));
    DUMP_ASSERT_CONTAINS(outputText, expected, "methodDefs diff should include removed bytes and count");

    DUMP_ASSERT_CONTAINS(outputText,
                         "zrp.metadata.diff.section.stringPool bytesBefore=12 bytesAfter=5 bytesRemoved=7 countBefore=12 countAfter=5 countRemoved=7",
                         "stringPool diff should include byte pool shrink");
    DUMP_ASSERT_CONTAINS(outputText,
                         "zrp.metadata.diff.section.constantPool bytesBefore=4 bytesAfter=9 bytesRemoved=0 countBefore=4 countAfter=9 countRemoved=0",
                         "constantPool growth should not underflow removed counters");

    input = fopen(beforePath, "wb");
    DUMP_ASSERT_TRUE(input != ZR_NULL, "before zrp file should open for write");
    DUMP_ASSERT_TRUE(fwrite(beforeBytes, 1u, sizeof(beforeBytes), input) == sizeof(beforeBytes),
                     "before zrp file should write");
    fclose(input);
    input = fopen(afterPath, "wb");
    DUMP_ASSERT_TRUE(input != ZR_NULL, "after zrp file should open for write");
    DUMP_ASSERT_TRUE(fwrite(afterBytes, 1u, sizeof(afterBytes), input) == sizeof(afterBytes),
                     "after zrp file should write");
    fclose(input);

    output = tmpfile();
    DUMP_ASSERT_TRUE(output != ZR_NULL, "tmpfile should be available for path diff");
    DUMP_ASSERT_TRUE(ZrCli_ZrpMetadataDump_RunDiffPath(beforePath, afterPath, output, stderr) == 0,
                     "metadata diff path should run");
    if (read_stream_text(output, outputText, sizeof(outputText)) != 0) {
        fclose(output);
        remove(beforePath);
        remove(afterPath);
        return 1;
    }
    fclose(output);
    remove(beforePath);
    remove(afterPath);
    DUMP_ASSERT_CONTAINS(outputText,
                         "zrp.metadata.diff.section.stringPool bytesBefore=12 bytesAfter=5 bytesRemoved=7",
                         "path diff should include metadata section delta");

    return 0;
}

static int test_version_check_reports_current_header_shape(void) {
    SZrZrpMetadataHeader header;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + 1024u];
    char error[256] = {0};
    char outputText[1024];
    char expected[256];
    const char *testPath = "zr_cli_zrp_metadata_version_check.zrp";
    FILE *output;
    FILE *input;

    build_summary_metadata(bytes, sizeof(bytes), &header, 1u, 1u, 4u, 0u);

    output = tmpfile();
    DUMP_ASSERT_TRUE(output != ZR_NULL, "tmpfile should be available");
    DUMP_ASSERT_TRUE(ZrCli_ZrpMetadataDump_WriteVersionCheck(output, bytes, sizeof(bytes), error, sizeof(error)),
                     "version check should accept current metadata header");
    if (read_stream_text(output, outputText, sizeof(outputText)) != 0) {
        fclose(output);
        return 1;
    }
    fclose(output);

    DUMP_ASSERT_CONTAINS(outputText,
                         "zrp.metadata.versionCheck.status=ok",
                         "version check should report ok status");
    snprintf(expected,
             sizeof(expected),
             "zrp.metadata.versionCheck.magic=%u expectedMagic=%u",
             (unsigned int)header.magic,
             (unsigned int)ZR_ZRP_METADATA_MAGIC);
    DUMP_ASSERT_CONTAINS(outputText, expected, "version check should report magic");
    DUMP_ASSERT_CONTAINS(outputText,
                         "zrp.metadata.versionCheck.version=2 expectedVersion=2",
                         "version check should report metadata version");
    DUMP_ASSERT_CONTAINS(outputText,
                         "zrp.metadata.versionCheck.headerBytes=208 expectedHeaderBytes=208",
                         "version check should report header bytes");
    DUMP_ASSERT_CONTAINS(outputText,
                         "zrp.metadata.versionCheck.sectionCount=12 expectedSectionCount=12",
                         "version check should report section count");

    input = fopen(testPath, "wb");
    DUMP_ASSERT_TRUE(input != ZR_NULL, "test zrp file should open for write");
    DUMP_ASSERT_TRUE(fwrite(bytes, 1u, sizeof(bytes), input) == sizeof(bytes), "test zrp file should write");
    fclose(input);

    output = tmpfile();
    DUMP_ASSERT_TRUE(output != ZR_NULL, "tmpfile should be available for path version check");
    DUMP_ASSERT_TRUE(ZrCli_ZrpMetadataDump_RunVersionCheckPath(testPath, output, stderr) == 0,
                     "metadata version check path should run");
    if (read_stream_text(output, outputText, sizeof(outputText)) != 0) {
        fclose(output);
        remove(testPath);
        return 1;
    }
    fclose(output);
    remove(testPath);
    DUMP_ASSERT_CONTAINS(outputText,
                         "zrp.metadata.versionCheck.status=ok",
                         "path version check should report ok status");

    return 0;
}

static int test_version_check_reports_unsupported_header_shape(void) {
    SZrZrpMetadataHeader header;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + 1024u];
    char error[256] = {0};
    char outputText[1024];
    FILE *output;

    build_summary_metadata(bytes, sizeof(bytes), &header, 1u, 1u, 4u, 0u);
    bytes[4] = 3u;
    bytes[5] = 0u;

    output = tmpfile();
    DUMP_ASSERT_TRUE(output != ZR_NULL, "tmpfile should be available");
    DUMP_ASSERT_TRUE(!ZrCli_ZrpMetadataDump_WriteVersionCheck(output, bytes, sizeof(bytes), error, sizeof(error)),
                     "unsupported metadata version should fail");
    if (read_stream_text(output, outputText, sizeof(outputText)) != 0) {
        fclose(output);
        return 1;
    }
    fclose(output);

    DUMP_ASSERT_CONTAINS(outputText,
                         "zrp.metadata.versionCheck.status=unsupported",
                         "version check should report unsupported status");
    DUMP_ASSERT_CONTAINS(outputText,
                         "zrp.metadata.versionCheck.version=3 expectedVersion=2",
                         "version check should report actual unsupported version");
    DUMP_ASSERT_CONTAINS(error,
                         "Unsupported zrp metadata version or header shape",
                         "unsupported version should explain failure");
    return 0;
}

static int test_summary_rejects_invalid_metadata_header(void) {
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE - 1u] = {0};
    char error[256] = {0};
    FILE *output = tmpfile();

    DUMP_ASSERT_TRUE(output != ZR_NULL, "tmpfile should be available");
    DUMP_ASSERT_TRUE(!ZrCli_ZrpMetadataDump_WriteSummary(output, bytes, sizeof(bytes), error, sizeof(error)),
                     "short metadata buffer should fail");
    fclose(output);
    DUMP_ASSERT_CONTAINS(error, "Invalid zrp metadata header", "error should explain invalid header");
    return 0;
}

static int test_version_check_rejects_short_header(void) {
    TZrByte bytes[8] = {0};
    char error[256] = {0};
    FILE *output = tmpfile();

    DUMP_ASSERT_TRUE(output != ZR_NULL, "tmpfile should be available");
    DUMP_ASSERT_TRUE(!ZrCli_ZrpMetadataDump_WriteVersionCheck(output, bytes, sizeof(bytes), error, sizeof(error)),
                     "short metadata header should fail version check");
    fclose(output);
    DUMP_ASSERT_CONTAINS(error, "Invalid zrp metadata header", "error should explain invalid header");
    return 0;
}

static int test_diff_rejects_invalid_metadata_header(void) {
    SZrZrpMetadataHeader beforeHeader;
    TZrByte beforeBytes[ZR_ZRP_METADATA_HEADER_SIZE + 1024u];
    TZrByte afterBytes[ZR_ZRP_METADATA_HEADER_SIZE - 1u] = {0};
    char error[256] = {0};
    FILE *output = tmpfile();

    build_summary_metadata(beforeBytes, sizeof(beforeBytes), &beforeHeader, 1u, 1u, 4u, 0u);

    DUMP_ASSERT_TRUE(output != ZR_NULL, "tmpfile should be available");
    DUMP_ASSERT_TRUE(!ZrCli_ZrpMetadataDump_WriteDiffSummary(output,
                                                             beforeBytes,
                                                             sizeof(beforeBytes),
                                                             afterBytes,
                                                             sizeof(afterBytes),
                                                             error,
                                                             sizeof(error)),
                     "short after metadata buffer should fail");
    fclose(output);
    DUMP_ASSERT_CONTAINS(error, "Invalid after zrp metadata header", "error should explain invalid after header");
    return 0;
}

int main(void) {
    if (test_summary_prints_metadata_section_bytes_and_counts() != 0) {
        return 1;
    }
    if (test_diff_prints_metadata_section_deltas() != 0) {
        return 1;
    }
    if (test_version_check_reports_current_header_shape() != 0) {
        return 1;
    }
    if (test_version_check_reports_unsupported_header_shape() != 0) {
        return 1;
    }
    if (test_summary_rejects_invalid_metadata_header() != 0) {
        return 1;
    }
    if (test_version_check_rejects_short_header() != 0) {
        return 1;
    }
    if (test_diff_rejects_invalid_metadata_header() != 0) {
        return 1;
    }

    printf("cli zrp metadata dump tests passed\n");
    return 0;
}
