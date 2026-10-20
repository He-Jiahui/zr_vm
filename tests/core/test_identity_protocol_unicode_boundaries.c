#include <string.h>

#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/utf8.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void test_module_restores_protocol_bits_from_seven_through_max(void) {
    SZrFunction entryFunction;
    SZrCompiledPrototypeInfo prototypeInfo;
    TZrByte prototypeData[sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo)] = {0};
    const TZrUInt32 prototypeCount = 1u;
    SZrTypeValue constant;
    SZrObjectModule *module;
    SZrString *typeName;
    SZrString *moduleName;
    SZrString *fullPath;
    const SZrTypeValue *exported;
    SZrObjectPrototype *prototype;
    TZrUInt64 protocolMask = 0u;

    memset(&entryFunction, 0, sizeof(entryFunction));
    typeName = ZrCore_String_Create(g_state,
                                    "ProtocolBoundaryTarget",
                                    sizeof("ProtocolBoundaryTarget") - 1u);
    TEST_ASSERT_NOT_NULL(typeName);
    ZrCore_Value_InitAsRawObject(g_state, &constant, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
    constant.type = ZR_VALUE_TYPE_STRING;

    for (EZrProtocolId protocolId = (EZrProtocolId)(ZR_PROTOCOL_ID_NONE + 1);
         protocolId <= ZR_PROTOCOL_ID_MAX;
         protocolId = (EZrProtocolId)(protocolId + 1)) {
        protocolMask |= ZR_PROTOCOL_BIT(protocolId);
    }

    memset(&prototypeInfo, 0, sizeof(prototypeInfo));
    prototypeInfo.nameStringIndex = 0u;
    prototypeInfo.type = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    prototypeInfo.accessModifier = ZR_ACCESS_CONSTANT_PUBLIC;
    prototypeInfo.protocolMask = protocolMask;
    memcpy(prototypeData, &prototypeCount, sizeof(prototypeCount));
    memcpy(prototypeData + sizeof(prototypeCount), &prototypeInfo, sizeof(prototypeInfo));

    entryFunction.prototypeCount = prototypeCount;
    entryFunction.prototypeData = prototypeData;
    entryFunction.prototypeDataLength = sizeof(prototypeData);
    entryFunction.constantValueList = &constant;
    entryFunction.constantValueLength = 1u;

    module = ZrCore_Module_Create(g_state);
    TEST_ASSERT_NOT_NULL(module);
    moduleName = ZrCore_String_Create(g_state, "protocol_boundaries", sizeof("protocol_boundaries") - 1u);
    fullPath = ZrCore_String_Create(g_state,
                                   "protocol_boundaries.zr",
                                   sizeof("protocol_boundaries.zr") - 1u);
    TEST_ASSERT_NOT_NULL(moduleName);
    TEST_ASSERT_NOT_NULL(fullPath);
    ZrCore_Module_SetInfo(g_state,
                          module,
                          moduleName,
                          ZrCore_Module_CalculatePathHash(g_state, fullPath),
                          fullPath);

    TEST_ASSERT_EQUAL_UINT64(1u,
                             ZrCore_Module_CreatePrototypesFromData(g_state, module, &entryFunction));
    exported = ZrCore_Module_GetPubExport(g_state, module, typeName);
    TEST_ASSERT_NOT_NULL(exported);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, exported->type);
    prototype = (SZrObjectPrototype *)ZR_CAST_OBJECT(g_state, exported->value.object);
    TEST_ASSERT_NOT_NULL(prototype);

    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_ImplementsProtocol(prototype, ZR_PROTOCOL_ID_TASK_HANDLE));
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_ImplementsProtocol(prototype, ZR_PROTOCOL_ID_MAX));
    for (EZrProtocolId protocolId = (EZrProtocolId)(ZR_PROTOCOL_ID_NONE + 1);
         protocolId <= ZR_PROTOCOL_ID_MAX;
         protocolId = (EZrProtocolId)(protocolId + 1)) {
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_ImplementsProtocol(prototype, protocolId));
    }

    ZrCore_Memory_RawFree(g_state->global,
                          entryFunction.prototypeInstances,
                          entryFunction.prototypeInstancesLength * sizeof(*entryFunction.prototypeInstances));
}

typedef struct SZrUtf8BoundaryCase {
    TZrUInt32 codePoint;
    TZrSize length;
    TZrByte bytes[4];
} SZrUtf8BoundaryCase;

static void test_utf8_encoder_handles_valid_scalar_boundaries(void) {
    static const SZrUtf8BoundaryCase cases[] = {
            {0x0000u, 1u, {0x00u, 0x00u, 0x00u, 0x00u}},
            {0x007fu, 1u, {0x7fu, 0x00u, 0x00u, 0x00u}},
            {0x0080u, 2u, {0xc2u, 0x80u, 0x00u, 0x00u}},
            {0x07ffu, 2u, {0xdfu, 0xbfu, 0x00u, 0x00u}},
            {0x0800u, 3u, {0xe0u, 0xa0u, 0x80u, 0x00u}},
            {0xd7ffu, 3u, {0xedu, 0x9fu, 0xbfu, 0x00u}},
            {0xe000u, 3u, {0xeeu, 0x80u, 0x80u, 0x00u}},
            {0xffffu, 3u, {0xefu, 0xbfu, 0xbfu, 0x00u}},
            {0x10000u, 4u, {0xf0u, 0x90u, 0x80u, 0x80u}},
            {0x10ffffu, 4u, {0xf4u, 0x8fu, 0xbfu, 0xbfu}},
    };

    for (TZrSize index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        TZrChar buffer[ZR_STRING_UTF8_SIZE];
        TZrSize encodedLength = ZR_MAX_SIZE;

        memset(buffer, 0x5a, sizeof(buffer));
        TEST_ASSERT_TRUE(ZrCore_Utf8_EncodeCodePoint(cases[index].codePoint, buffer, &encodedLength));
        TEST_ASSERT_EQUAL_UINT64(cases[index].length, encodedLength);
        TEST_ASSERT_EQUAL_MEMORY(cases[index].bytes, buffer, cases[index].length);
        TEST_ASSERT_EQUAL_UINT8(0u, (TZrByte)buffer[cases[index].length]);
    }
}

static void test_utf8_encoder_rejects_non_scalars_without_changing_outputs(void) {
    static const TZrUInt32 invalidCodePoints[] = {
            0xd800u,
            0xdfffu,
            0x110000u,
            0xffffffffu,
    };

    for (TZrSize index = 0u; index < sizeof(invalidCodePoints) / sizeof(invalidCodePoints[0]); ++index) {
        TZrChar buffer[ZR_STRING_UTF8_SIZE];
        TZrChar expected[ZR_STRING_UTF8_SIZE];
        TZrSize encodedLength = 0x1234u;

        memset(buffer, 0x5a, sizeof(buffer));
        memcpy(expected, buffer, sizeof(buffer));
        TEST_ASSERT_FALSE(ZrCore_Utf8_EncodeCodePoint(invalidCodePoints[index], buffer, &encodedLength));
        TEST_ASSERT_EQUAL_UINT64(0x1234u, encodedLength);
        TEST_ASSERT_EQUAL_MEMORY(expected, buffer, sizeof(buffer));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_module_restores_protocol_bits_from_seven_through_max);
    RUN_TEST(test_utf8_encoder_handles_valid_scalar_boundaries);
    RUN_TEST(test_utf8_encoder_rejects_non_scalars_without_changing_outputs);
    return UNITY_END();
}
