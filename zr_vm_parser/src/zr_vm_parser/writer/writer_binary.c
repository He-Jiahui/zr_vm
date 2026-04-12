#include "writer_binary_internal.h"

#include <string.h>

#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_version_info.h"
#include "zr_vm_core/string.h"

static void writer_binary_write_native_string_with_length(FILE *file, const TZrChar *text) {
    TZrSize length = 0;

    if (file == ZR_NULL) {
        return;
    }

    if (text != ZR_NULL) {
        length = strlen(text);
    }

    fwrite(&length, sizeof(TZrSize), 1, file);
    if (length > 0) {
        fwrite(text, sizeof(TZrChar), length, file);
    }
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteBinaryFileWithOptions(SZrState *state,
                                                                 SZrFunction *function,
                                                                 const TZrChar *filename,
                                                                 const SZrBinaryWriterOptions *options) {
    const TZrChar *moduleName = "simple";
    const TZrChar *moduleHash = ZR_NULL;
    FILE *file;
    TZrUInt32 versionMajor = ZR_VM_MAJOR_VERSION;
    TZrUInt32 versionMinor = ZR_VM_MINOR_VERSION;
    TZrUInt32 versionPatch = ZR_VM_PATCH_VERSION;
    TZrUInt64 format = ((TZrUInt64)versionMajor << ZR_IO_VERSION_FORMAT_SHIFT_BITS) | versionMinor;
    TZrUInt8 nativeIntSize = ZR_IO_NATIVE_INT_SIZE;
    TZrUInt8 sizeTypeSize = ZR_IO_SIZE_T_SIZE;
    TZrUInt8 instructionSize = ZR_IO_INSTRUCTION_SIZE;
    TZrUInt8 endian = ZR_IO_IS_LITTLE_ENDIAN ? ZR_TRUE : ZR_FALSE;
    TZrUInt8 debug;
    TZrUInt8 opt[ZR_IO_SOURCE_HEADER_OPT_BYTES] = {ZR_FALSE, ZR_FALSE, ZR_FALSE};
    TZrUInt64 modulesLength = 1;
    TZrUInt64 importsLength = 0;
    TZrUInt64 declaresLength = 0;

    if (state == ZR_NULL || function == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }

    if (options != ZR_NULL && options->moduleName != ZR_NULL && options->moduleName[0] != '\0') {
        moduleName = options->moduleName;
    }
    if (options != ZR_NULL && options->moduleHash != ZR_NULL && options->moduleHash[0] != '\0') {
        moduleHash = options->moduleHash;
    }

    file = fopen(filename, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    fwrite(ZR_IO_SOURCE_SIGNATURE, sizeof(TZrUInt8), ZR_IO_SOURCE_SIGNATURE_LENGTH, file);
    fwrite(&versionMajor, sizeof(TZrUInt32), 1, file);
    fwrite(&versionMinor, sizeof(TZrUInt32), 1, file);
    fwrite(&versionPatch, sizeof(TZrUInt32), 1, file);
    fwrite(&format, sizeof(TZrUInt64), 1, file);
    fwrite(&nativeIntSize, sizeof(TZrUInt8), 1, file);
    fwrite(&sizeTypeSize, sizeof(TZrUInt8), 1, file);
    fwrite(&instructionSize, sizeof(TZrUInt8), 1, file);
    fwrite(&endian, sizeof(TZrUInt8), 1, file);

    debug = ZrParser_Writer_FunctionTreeHasDebugInfo(function) ? ZR_TRUE : ZR_FALSE;
    fwrite(&debug, sizeof(TZrUInt8), 1, file);
    fwrite(opt, sizeof(TZrUInt8), ZR_IO_SOURCE_HEADER_OPT_BYTES, file);
    fwrite(&modulesLength, sizeof(TZrUInt64), 1, file);

    writer_binary_write_native_string_with_length(file, moduleName);
    writer_binary_write_native_string_with_length(file, moduleHash);
    fwrite(&importsLength, sizeof(TZrUInt64), 1, file);
    fwrite(&declaresLength, sizeof(TZrUInt64), 1, file);

    if (!ZrParser_Writer_WriteIoFunction(state, file, function, "__entry", options)) {
        fclose(file);
        return ZR_FALSE;
    }

    fclose(file);
    return ZR_TRUE;
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteBinaryFile(SZrState *state, SZrFunction *function, const TZrChar *filename) {
    return ZrParser_Writer_WriteBinaryFileWithOptions(state, function, filename, ZR_NULL);
}
