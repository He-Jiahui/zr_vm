#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/global.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

static TZrPtr fixture_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        free(pointer);
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    return realloc(pointer, newSize);
}

static char *read_text_file(const char *path, TZrSize *outLength) {
    FILE *file = fopen(path, "rb");
    char *buffer = ZR_NULL;
    long fileSize = 0;

    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }
    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (char *) malloc((size_t) fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fread(buffer, 1, (size_t) fileSize, file) != (size_t) fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);

    *outLength = (TZrSize) fileSize;
    return buffer;
}

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: zr_vm_project_fixture_builder <input.zr> <output.zro> [output.zri]\n");
        return 1;
    }

    TZrSize sourceLength = 0;
    char *source = read_text_file(argv[1], &sourceLength);
    if (source == ZR_NULL) {
        fprintf(stderr, "failed to read source fixture: %s\n", argv[1]);
        return 1;
    }

    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(fixture_allocator, ZR_NULL, 0x5A525F4649585455ULL, &callbacks);
    if (global == ZR_NULL) {
        free(source);
        fprintf(stderr, "failed to create global state\n");
        return 1;
    }

    SZrState *state = global->mainThreadState;
    SZrString *sourceName = ZrCore_String_Create(state, argv[1], strlen(argv[1]));
    SZrFunction *function = ZrParser_Source_Compile(state, source, sourceLength, sourceName);
    free(source);

    if (function == ZR_NULL) {
        fprintf(stderr, "failed to compile source fixture: %s\n", argv[1]);
        ZrCore_GlobalState_Free(global);
        return 1;
    }

    if (!ZrParser_Writer_WriteBinaryFile(state, function, argv[2])) {
        fprintf(stderr, "failed to write binary fixture: %s\n", argv[2]);
        ZrCore_GlobalState_Free(global);
        return 1;
    }

    if (argc == 4 && !ZrParser_Writer_WriteIntermediateFile(state, function, argv[3])) {
        fprintf(stderr, "failed to write intermediate fixture: %s\n", argv[3]);
        ZrCore_GlobalState_Free(global);
        return 1;
    }

    printf("generated %s\n", argv[2]);
    ZrCore_GlobalState_Free(global);
    return 0;
}
