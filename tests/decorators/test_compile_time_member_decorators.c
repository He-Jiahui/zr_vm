#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/writer.h"
#include "test_support.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

typedef struct {
    const TZrChar *path;
    const TZrChar *source;
    const TZrByte *bytes;
    TZrSize length;
    TZrBool isBinary;
} SZrModuleFixtureSource;

#define MODULE_FIXTURE_SOURCE_TEXT(pathValue, sourceValue) \
    {                                                      \
            (pathValue),                                   \
            (sourceValue),                                 \
            ZR_NULL,                                       \
            0,                                             \
            ZR_FALSE,                                      \
    }

typedef struct {
    const TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} SZrModuleFixtureReader;

static const SZrModuleFixtureSource *g_module_fixture_sources = ZR_NULL;
static TZrSize g_module_fixture_source_count = 0;

static TZrBool string_equals_cstring(SZrString *value, const TZrChar *expected) {
    const TZrChar *nativeString;

    if (value == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeString = ZrCore_String_GetNativeString(value);
    return nativeString != ZR_NULL && strcmp(nativeString, expected) == 0;
}

static const SZrTypeValue *get_object_field_value(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrString *fieldNameString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldNameString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldNameString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static TZrSize get_array_length(SZrObject *array) {
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return 0;
    }

    return array->nodeMap.elementCount;
}

static SZrObject *get_array_entry_object(SZrState *state, SZrObject *array, TZrSize index) {
    SZrTypeValue key;
    const SZrTypeValue *entryValue;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    entryValue = ZrCore_Object_GetValue(state, array, &key);
    if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, entryValue->value.object);
}

static TZrBytePtr module_fixture_reader_read(SZrState *state, TZrPtr customData, TZrSize *size) {
    SZrModuleFixtureReader *reader = (SZrModuleFixtureReader *)customData;

    ZR_UNUSED_PARAMETER(state);

    if (reader == ZR_NULL || size == ZR_NULL || reader->consumed) {
        if (size != ZR_NULL) {
            *size = 0;
        }
        return ZR_NULL;
    }

    reader->consumed = ZR_TRUE;
    *size = reader->length;
    return (TZrBytePtr)reader->bytes;
}

static void module_fixture_reader_close(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);

    if (customData != ZR_NULL) {
        free(customData);
    }
}

static TZrByte *read_test_file_bytes(const TZrChar *path, TZrSize *outLength) {
    FILE *file;
    long fileSize;
    TZrByte *buffer;

    if (path == ZR_NULL || outLength == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
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

    buffer = (TZrByte *)malloc((size_t)fileSize);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    fclose(file);
    *outLength = (TZrSize)fileSize;
    return buffer;
}

static TZrByte *build_module_binary_fixture(SZrState *state,
                                            const TZrChar *moduleSource,
                                            const TZrChar *binaryPath,
                                            TZrSize *outLength) {
    SZrString *sourceName;
    SZrFunction *function;
    TZrBool oldEmitCompileTimeRuntimeSupport = ZR_FALSE;

    if (state == ZR_NULL || moduleSource == ZR_NULL || binaryPath == ZR_NULL || outLength == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)binaryPath, strlen(binaryPath));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global != ZR_NULL) {
        oldEmitCompileTimeRuntimeSupport = state->global->emitCompileTimeRuntimeSupport;
        state->global->emitCompileTimeRuntimeSupport = ZR_TRUE;
    }
    function = ZrParser_Source_Compile(state, moduleSource, strlen(moduleSource), sourceName);
    if (state->global != ZR_NULL) {
        state->global->emitCompileTimeRuntimeSupport = oldEmitCompileTimeRuntimeSupport;
    }
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    if (!ZrParser_Writer_WriteBinaryFile(state, function, binaryPath)) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    return read_test_file_bytes(binaryPath, outLength);
}

static TZrBool module_fixture_source_loader(SZrState *state, TZrNativeString sourcePath, TZrNativeString md5, SZrIo *io) {
    TZrSize index;

    ZR_UNUSED_PARAMETER(md5);

    if (state == ZR_NULL || sourcePath == ZR_NULL || io == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < g_module_fixture_source_count; index++) {
        const SZrModuleFixtureSource *fixture = &g_module_fixture_sources[index];
        if (fixture->path != ZR_NULL && strcmp(fixture->path, sourcePath) == 0) {
            SZrModuleFixtureReader *reader =
                    (SZrModuleFixtureReader *)malloc(sizeof(SZrModuleFixtureReader));
            if (reader == ZR_NULL) {
                return ZR_FALSE;
            }

            if (fixture->bytes != ZR_NULL && fixture->length > 0) {
                reader->bytes = fixture->bytes;
                reader->length = fixture->length;
            } else {
                reader->bytes = (const TZrByte *)fixture->source;
                reader->length = fixture->source != ZR_NULL ? strlen(fixture->source) : 0;
            }
            reader->consumed = ZR_FALSE;

            ZrCore_Io_Init(state, io, module_fixture_reader_read, module_fixture_reader_close, reader);
            io->isBinary = fixture->isBinary;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void assert_member_reflection(SZrState *state,
                                     SZrObject *moduleReflection,
                                     const TZrChar *memberFieldName,
                                     const TZrChar *expectedKind,
                                     const TZrChar *expectedMetadataFlagName,
                                     const TZrChar *expectedDecoratorName) {
    const SZrTypeValue *memberValue;
    const SZrTypeValue *kindValue;
    const SZrTypeValue *metadataValue;
    const SZrTypeValue *decoratorsValue;
    const SZrTypeValue *flagValue;
    const SZrTypeValue *decoratorNameValue;
    SZrObject *memberReflection;
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;
    SZrObject *decoratorEntry;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(moduleReflection);

    memberValue = get_object_field_value(state, moduleReflection, memberFieldName);
    TEST_ASSERT_NOT_NULL(memberValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, memberValue->type);

    memberReflection = ZR_CAST_OBJECT(state, memberValue->value.object);
    TEST_ASSERT_NOT_NULL(memberReflection);

    kindValue = get_object_field_value(state, memberReflection, "kind");
    metadataValue = get_object_field_value(state, memberReflection, "metadata");
    decoratorsValue = get_object_field_value(state, memberReflection, "decorators");
    TEST_ASSERT_NOT_NULL(kindValue);
    TEST_ASSERT_NOT_NULL(metadataValue);
    TEST_ASSERT_NOT_NULL(decoratorsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, kindValue->type);
    TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, kindValue->value.object), expectedKind));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, metadataValue->type);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, decoratorsValue->type);

    metadataObject = ZR_CAST_OBJECT(state, metadataValue->value.object);
    decoratorsArray = ZR_CAST_OBJECT(state, decoratorsValue->value.object);
    TEST_ASSERT_NOT_NULL(metadataObject);
    TEST_ASSERT_NOT_NULL(decoratorsArray);

    flagValue = get_object_field_value(state, metadataObject, expectedMetadataFlagName);
    TEST_ASSERT_NOT_NULL(flagValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, flagValue->type);
    TEST_ASSERT_TRUE(flagValue->value.nativeObject.nativeBool);

    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)get_array_length(decoratorsArray));
    decoratorEntry = get_array_entry_object(state, decoratorsArray, 0);
    TEST_ASSERT_NOT_NULL(decoratorEntry);
    decoratorNameValue = get_object_field_value(state, decoratorEntry, "name");
    TEST_ASSERT_NOT_NULL(decoratorNameValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, decoratorNameValue->type);
    TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, decoratorNameValue->value.object),
                                           expectedDecoratorName));
}

static const TZrChar *kCompileTimeMemberDecoratorModuleSource =
        "%compileTime class MarkField {\n"
        "    @decorate(target: %type Field): DecoratorPatch {\n"
        "        return { metadata: { compileTimeField: true } };\n"
        "    }\n"
        "}\n"
        "\n"
        "%compileTime class MarkMethod {\n"
        "    @decorate(target: %type Method): DecoratorPatch {\n"
        "        return { metadata: { compileTimeMethod: true } };\n"
        "    }\n"
        "}\n"
        "\n"
        "%compileTime class MarkProperty {\n"
        "    @decorate(target: %type Property): DecoratorPatch {\n"
        "        return { metadata: { compileTimeProperty: true } };\n"
        "    }\n"
        "}\n"
        "\n"
        "pub class User {\n"
        "    #MarkField#\n"
        "    pub var id: int = 1;\n"
        "\n"
        "    pri var _value: int = 2;\n"
        "\n"
        "    #MarkMethod#\n"
        "    pub load(v: int): int {\n"
        "        return v;\n"
        "    }\n"
        "\n"
        "    #MarkProperty#\n"
        "    pub get value: int {\n"
        "        return this._value;\n"
        "    }\n"
        "}\n";

static const TZrChar *kCompileTimeMemberDecoratorImportSource =
        "var decorated = %import(\"compile_time_member_decorators\");\n"
        "return {\n"
        "    field: %type(decorated.User).members.id[0],\n"
        "    method: %type(decorated.User).members.load[0],\n"
        "    property: %type(decorated.User).members.value[0]\n"
        "};\n";

void setUp(void) {
}

void tearDown(void) {
}

static void test_percent_type_source_member_reflection_exposes_compile_time_member_decorator_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Source Member Reflection Exposes Compile-Time Member Decorator Metadata";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;
    SZrModuleFixtureSource fixtures[1];

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = ZrTests_State_Create(ZR_NULL);
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *resultObject;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);

        fixtures[0].path = "compile_time_member_decorators";
        fixtures[0].source = kCompileTimeMemberDecoratorModuleSource;
        fixtures[0].bytes = ZR_NULL;
        fixtures[0].length = 0;
        fixtures[0].isBinary = ZR_FALSE;

        g_module_fixture_sources = fixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(fixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "type_source_compile_time_member_decorator_reflection_test.zr",
                                          58);
        TEST_ASSERT_NOT_NULL(sourceName);

        entryFunction = ZrParser_Source_Compile(state,
                                               kCompileTimeMemberDecoratorImportSource,
                                               strlen(kCompileTimeMemberDecoratorImportSource),
                                               sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        resultObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(resultObject);

        assert_member_reflection(state, resultObject, "field", "field", "compileTimeField", "MarkField");
        assert_member_reflection(state, resultObject, "method", "method", "compileTimeMethod", "MarkMethod");
        assert_member_reflection(state, resultObject, "property", "property", "compileTimeProperty", "MarkProperty");

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        ZrTests_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_percent_type_binary_member_reflection_exposes_compile_time_member_decorator_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Binary Member Reflection Exposes Compile-Time Member Decorator Metadata";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;
    TZrByte *binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;
    const TZrChar *binaryPath = "test_compile_time_member_decorators_fixture.zro";
    SZrModuleFixtureSource fixtures[1];

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = ZrTests_State_Create(ZR_NULL);
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *resultObject;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);

        binaryBytes = build_module_binary_fixture(state,
                                                 kCompileTimeMemberDecoratorModuleSource,
                                                 binaryPath,
                                                 &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        fixtures[0].path = "compile_time_member_decorators";
        fixtures[0].source = ZR_NULL;
        fixtures[0].bytes = binaryBytes;
        fixtures[0].length = binaryLength;
        fixtures[0].isBinary = ZR_TRUE;

        g_module_fixture_sources = fixtures;
        g_module_fixture_source_count = 1;
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "type_binary_compile_time_member_decorator_reflection_test.zr",
                                          58);
        TEST_ASSERT_NOT_NULL(sourceName);

        entryFunction = ZrParser_Source_Compile(state,
                                               kCompileTimeMemberDecoratorImportSource,
                                               strlen(kCompileTimeMemberDecoratorImportSource),
                                               sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        resultObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(resultObject);

        assert_member_reflection(state, resultObject, "field", "field", "compileTimeField", "MarkField");
        assert_member_reflection(state, resultObject, "method", "method", "compileTimeMethod", "MarkMethod");
        assert_member_reflection(state, resultObject, "property", "property", "compileTimeProperty", "MarkProperty");

        ZrCore_Function_Free(state, entryFunction);
        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        free(binaryBytes);
        remove(binaryPath);
        ZrTests_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

int main(void) {
    UNITY_BEGIN();

    ZR_TEST_MODULE_DIVIDER();
    printf("Compile-Time Member Decorator Tests\n");
    ZR_TEST_MODULE_DIVIDER();

    RUN_TEST(test_percent_type_source_member_reflection_exposes_compile_time_member_decorator_metadata);
    RUN_TEST(test_percent_type_binary_member_reflection_exposes_compile_time_member_decorator_metadata);

    return UNITY_END();
}
