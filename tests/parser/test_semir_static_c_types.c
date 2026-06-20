#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/writer.h"

typedef struct SZrBinaryFixtureReader {
    const TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} SZrBinaryFixtureReader;

static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr)pointer >= (TZrPtr)0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    if ((TZrPtr)pointer >= (TZrPtr)0x1000) {
        return realloc(pointer, newSize);
    }

    return malloc(newSize);
}

static SZrState *create_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 0, &callbacks);
    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    return global->mainThreadState;
}

static void destroy_test_state(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrCore_GlobalState_Free(state->global);
    }
}

static TZrByte *read_binary_file_owned(const char *path, TZrSize *outLength) {
    FILE *file;
    long fileSize;
    TZrByte *buffer;

    if (path == ZR_NULL || outLength == ZR_NULL) {
        return ZR_NULL;
    }

    *outLength = 0;
    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (TZrByte *)malloc((size_t)fileSize);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fread(buffer, 1u, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    fclose(file);
    *outLength = (TZrSize)fileSize;
    return buffer;
}

static TZrBytePtr binary_fixture_reader_read(struct SZrState *state, TZrPtr customData, ZR_OUT TZrSize *size) {
    SZrBinaryFixtureReader *reader = (SZrBinaryFixtureReader *)customData;

    ZR_UNUSED_PARAMETER(state);
    if (reader == ZR_NULL || size == ZR_NULL || reader->consumed) {
        return ZR_NULL;
    }

    reader->consumed = ZR_TRUE;
    *size = reader->length;
    return (TZrBytePtr)reader->bytes;
}

static void binary_fixture_reader_close(struct SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static SZrFunction *compile_static_c_type_fixture(SZrState *state) {
    const char *source =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub @constructor(x: int) {\n"
            "        this.x = x;\n"
            "    }\n"
            "}\n"
            "var amount: int = 7;\n"
            "var flag: bool = true;\n"
            "var label: string = \"ok\";\n"
            "var point: Point = $Point(amount);\n"
            "return amount;";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, "semir_static_c_types.zr", 23);
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static const SZrFunctionTypedTypeRef *find_static_type(const SZrFunction *function, TZrUInt32 staticCType) {
    if (function == ZR_NULL || function->semIrTypeTable == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0u; index < function->semIrTypeTableLength; index++) {
        const SZrFunctionTypedTypeRef *typeRef = &function->semIrTypeTable[index];
        if (typeRef->staticCType == staticCType) {
            return typeRef;
        }
    }

    return ZR_NULL;
}

static void assert_semir_static_c_types(const SZrFunction *function) {
    const SZrFunctionTypedTypeRef *i64Type;
    const SZrFunctionTypedTypeRef *boolType;
    const SZrFunctionTypedTypeRef *refType;
    const SZrFunctionTypedTypeRef *structType;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->semIrTypeTable);
    TEST_ASSERT_GREATER_THAN_UINT32(1u, function->semIrTypeTableLength);

    i64Type = find_static_type(function, ZR_STATIC_C_TYPE_I64);
    boolType = find_static_type(function, ZR_STATIC_C_TYPE_BOOL);
    refType = find_static_type(function, ZR_STATIC_C_TYPE_GC_REF);
    structType = find_static_type(function, ZR_STATIC_C_TYPE_STRUCT);

    TEST_ASSERT_NOT_NULL(i64Type);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_INT64, i64Type->baseType);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, i64Type->staticCTypeId);

    TEST_ASSERT_NOT_NULL(boolType);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_BOOL, boolType->baseType);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, boolType->staticCTypeId);

    TEST_ASSERT_NOT_NULL(refType);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_STRING, refType->baseType);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, refType->staticCTypeId);

    TEST_ASSERT_NOT_NULL(structType);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_OBJECT, structType->baseType);
    TEST_ASSERT_NOT_EQUAL(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, structType->staticCTypeId);
}

static SZrFunction *roundtrip_function(SZrState *state, SZrFunction *function, const char *binaryPath) {
    TZrSize binaryLength = 0;
    TZrByte *binaryBytes;
    SZrBinaryFixtureReader reader;
    SZrIo *io;
    SZrIoSource *sourceObject;
    SZrFunction *runtimeFunction;

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
    binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
    TEST_ASSERT_NOT_NULL(binaryBytes);
    TEST_ASSERT_TRUE(binaryLength > 0);

    reader.bytes = binaryBytes;
    reader.length = binaryLength;
    reader.consumed = ZR_FALSE;

    io = ZrCore_Io_New(state->global);
    TEST_ASSERT_NOT_NULL(io);
    ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    io->isBinary = ZR_TRUE;

    sourceObject = ZrCore_Io_ReadSourceNew(io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);

    ZrCore_Io_Free(state->global, io);
    free(binaryBytes);
    remove(binaryPath);
    return runtimeFunction;
}

static void test_semir_type_table_records_static_c_type_annotations(void) {
    SZrState *state = create_test_state();
    SZrFunction *function;
    SZrFunction *runtimeFunction;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_static_c_type_fixture(state);
    TEST_ASSERT_NOT_NULL(function);

    assert_semir_static_c_types(function);

    runtimeFunction = roundtrip_function(state, function, "semir_static_c_types_test.zro");
    assert_semir_static_c_types(runtimeFunction);

    ZrCore_Function_Free(state, runtimeFunction);
    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_semir_type_table_records_static_c_type_annotations);
    return UNITY_END();
}
