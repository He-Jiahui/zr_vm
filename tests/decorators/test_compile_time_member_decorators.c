#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"
#include "module_fixture_support.h"
#include "runtime_support.h"
#include "test_support.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

typedef ZrTestsFixtureSource SZrModuleFixtureSource;

#define MODULE_FIXTURE_SOURCE_TEXT(pathValue, sourceValue) ZR_TESTS_FIXTURE_SOURCE_TEXT(pathValue, sourceValue)
#define string_equals_cstring ZrTests_Fixture_StringEqualsCString
#define get_object_field_value ZrTests_Fixture_GetObjectFieldValue
#define get_array_length ZrTests_Fixture_GetArrayLength
#define get_array_entry_object ZrTests_Fixture_GetArrayEntryObject

static const SZrModuleFixtureSource *g_module_fixture_sources = ZR_NULL;
static TZrSize g_module_fixture_source_count = 0;

static TZrByte *build_module_binary_fixture(SZrState *state,
                                            const TZrChar *moduleSource,
                                            const TZrChar *binaryPath,
                                            TZrSize *outLength) {
    return ZrTests_Fixture_BuildBinaryFile(state, moduleSource, binaryPath, ZR_TRUE, outLength);
}

static TZrBool module_fixture_source_loader(SZrState *state, TZrNativeString sourcePath, TZrNativeString md5, SZrIo *io) {
    return ZrTests_Fixture_SourceLoaderFromArray(state,
                                                 sourcePath,
                                                 md5,
                                                 io,
                                                 g_module_fixture_sources,
                                                 g_module_fixture_source_count);
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

static void assert_parameter_reflection(SZrState *state,
                                        SZrObject *parameterReflection,
                                        const TZrChar *expectedTypeName,
                                        TZrInt64 expectedPosition,
                                        const TZrChar *expectedMetadataFlagName,
                                        const TZrChar *expectedDecoratorName) {
    const SZrTypeValue *kindValue;
    const SZrTypeValue *typeNameValue;
    const SZrTypeValue *positionValue;
    const SZrTypeValue *metadataValue;
    const SZrTypeValue *decoratorsValue;
    const SZrTypeValue *flagValue;
    const SZrTypeValue *decoratorNameValue;
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;
    SZrObject *decoratorEntry;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(parameterReflection);

    kindValue = get_object_field_value(state, parameterReflection, "kind");
    typeNameValue = get_object_field_value(state, parameterReflection, "typeName");
    positionValue = get_object_field_value(state, parameterReflection, "position");
    metadataValue = get_object_field_value(state, parameterReflection, "metadata");
    decoratorsValue = get_object_field_value(state, parameterReflection, "decorators");

    TEST_ASSERT_NOT_NULL(kindValue);
    TEST_ASSERT_NOT_NULL(typeNameValue);
    TEST_ASSERT_NOT_NULL(positionValue);
    TEST_ASSERT_NOT_NULL(metadataValue);
    TEST_ASSERT_NOT_NULL(decoratorsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, kindValue->type);
    TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, kindValue->value.object), "parameter"));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, typeNameValue->type);
    TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, typeNameValue->value.object), expectedTypeName));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(positionValue->type));
    TEST_ASSERT_EQUAL_INT64(expectedPosition, positionValue->value.nativeObject.nativeInt64);
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

static const TZrChar *kCompileTimeParameterDecoratorModuleSource =
        "%compileTime class MarkParameter {\n"
        "    @decorate(target: %type Parameter): DecoratorPatch {\n"
        "        return { metadata: { compileTimeParameter: true } };\n"
        "    }\n"
        "}\n"
        "\n"
        "pub load(#MarkParameter# value: int, other: int = 2): int {\n"
        "    return value + other;\n"
        "}\n";

static const TZrChar *kCompileTimeParameterDecoratorImportSource =
        "var decorated = %import(\"compile_time_parameter_decorators\");\n"
        "return %type(decorated.load).parameters[0];\n";

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

static void test_percent_type_source_parameter_reflection_exposes_compile_time_parameter_decorator_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Source Parameter Reflection Exposes Compile-Time Parameter Decorator Metadata";
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
        SZrObject *parameterReflection;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);

        fixtures[0].path = "compile_time_parameter_decorators";
        fixtures[0].source = kCompileTimeParameterDecoratorModuleSource;
        fixtures[0].bytes = ZR_NULL;
        fixtures[0].length = 0;
        fixtures[0].isBinary = ZR_FALSE;

        g_module_fixture_sources = fixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(fixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "type_source_compile_time_parameter_decorator_reflection_test.zr",
                                          61);
        TEST_ASSERT_NOT_NULL(sourceName);

        entryFunction = ZrParser_Source_Compile(state,
                                               kCompileTimeParameterDecoratorImportSource,
                                               strlen(kCompileTimeParameterDecoratorImportSource),
                                               sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        parameterReflection = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(parameterReflection);
        assert_parameter_reflection(state,
                                    parameterReflection,
                                    "int",
                                    0,
                                    "compileTimeParameter",
                                    "MarkParameter");

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

static void test_percent_type_binary_parameter_reflection_exposes_compile_time_parameter_decorator_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Percent Type Binary Parameter Reflection Exposes Compile-Time Parameter Decorator Metadata";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;
    TZrByte *binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;
    const TZrChar *binaryPath = "test_compile_time_parameter_decorators_fixture.zro";
    SZrModuleFixtureSource fixtures[1];

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = ZrTests_State_Create(ZR_NULL);
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObject *parameterReflection;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);

        binaryBytes = build_module_binary_fixture(state,
                                                 kCompileTimeParameterDecoratorModuleSource,
                                                 binaryPath,
                                                 &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        fixtures[0].path = "compile_time_parameter_decorators";
        fixtures[0].source = ZR_NULL;
        fixtures[0].bytes = binaryBytes;
        fixtures[0].length = binaryLength;
        fixtures[0].isBinary = ZR_TRUE;

        g_module_fixture_sources = fixtures;
        g_module_fixture_source_count = 1;
        state->global->sourceLoader = module_fixture_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "type_binary_compile_time_parameter_decorator_reflection_test.zr",
                                          61);
        TEST_ASSERT_NOT_NULL(sourceName);

        entryFunction = ZrParser_Source_Compile(state,
                                               kCompileTimeParameterDecoratorImportSource,
                                               strlen(kCompileTimeParameterDecoratorImportSource),
                                               sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        parameterReflection = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(parameterReflection);
        assert_parameter_reflection(state,
                                    parameterReflection,
                                    "int",
                                    0,
                                    "compileTimeParameter",
                                    "MarkParameter");

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
    RUN_TEST(test_percent_type_source_parameter_reflection_exposes_compile_time_parameter_decorator_metadata);
    RUN_TEST(test_percent_type_binary_parameter_reflection_exposes_compile_time_parameter_decorator_metadata);

    return UNITY_END();
}
