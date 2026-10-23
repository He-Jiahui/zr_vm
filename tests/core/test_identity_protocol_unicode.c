#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/utf8.h"


static void test_raw_object_hash_uses_object_address(void) {
    SZrRawObject object;

    ZrCore_RawObject_Construct(&object, ZR_RAW_OBJECT_TYPE_OBJECT);

    TEST_ASSERT_EQUAL_UINT64(((TZrUInt64)&object) / ZR_ALIGN_SIZE, object.hash);
}

static void test_module_restores_protocol_bits_above_array_like(void) {
    static const TZrUInt32 protocolId = ZR_PROTOCOL_ID_THREAD_SYNC;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *entryFunction;
    SZrCompiledPrototypeInfo prototypeInfo;
    TZrByte prototypeData[sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo)] = {0};
    TZrUInt32 prototypeCount = 1u;
    SZrTypeValue constant;
    SZrObjectModule *module;
    SZrString *typeName;
    SZrString *moduleName;
    SZrString *fullPath;
    const SZrTypeValue *exported;
    SZrObjectPrototype *prototype;

    TEST_ASSERT_NOT_NULL(state);
    entryFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrCore_Function_AcquireHostRoot(state, entryFunction));
    typeName = ZrCore_String_Create(state, "AuditProtocolTarget", sizeof("AuditProtocolTarget") - 1u);
    ZrCore_Value_InitAsRawObject(state, &constant, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
    constant.type = ZR_VALUE_TYPE_STRING;

    memset(&prototypeInfo, 0, sizeof(prototypeInfo));
    prototypeInfo.nameStringIndex = 0u;
    prototypeInfo.type = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    prototypeInfo.accessModifier = ZR_ACCESS_CONSTANT_PUBLIC;
    prototypeInfo.protocolMask = ZR_PROTOCOL_BIT(protocolId);
    memcpy(prototypeData, &prototypeCount, sizeof(prototypeCount));
    memcpy(prototypeData + sizeof(prototypeCount), &prototypeInfo, sizeof(prototypeInfo));

    entryFunction->prototypeCount = prototypeCount;
    entryFunction->prototypeData = prototypeData;
    entryFunction->prototypeDataLength = sizeof(prototypeData);
    entryFunction->constantValueList = &constant;
    entryFunction->constantValueLength = 1u;

    module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);
    moduleName = ZrCore_String_Create(state, "audit_protocol", sizeof("audit_protocol") - 1u);
    fullPath = ZrCore_String_Create(state, "audit_protocol.zr", sizeof("audit_protocol.zr") - 1u);
    ZrCore_Module_SetInfo(state, module, moduleName, ZrCore_Module_CalculatePathHash(state, fullPath), fullPath);

    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Module_CreatePrototypesFromData(state, module, entryFunction));
    exported = ZrCore_Module_GetPubExport(state, module, typeName);
    TEST_ASSERT_NOT_NULL(exported);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, exported->type);
    prototype = (SZrObjectPrototype *)ZR_CAST_OBJECT(state, exported->value.object);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_ImplementsProtocol(prototype, (EZrProtocolId)protocolId));

    /* The fixture lends stack buffers; the runtime function owns its instance table. */
    entryFunction->prototypeData = ZR_NULL;
    entryFunction->prototypeDataLength = 0u;
    entryFunction->constantValueList = ZR_NULL;
    entryFunction->constantValueLength = 0u;
    ZrCore_Function_Free(state, entryFunction);
    ZrTests_Runtime_State_Destroy(state);
}
static void test_utf8_encoder_rejects_non_scalar_values(void) {
    TZrChar buffer[ZR_STRING_UTF8_SIZE] = {0};
    TZrSize encodedLength = 99u;

    TEST_ASSERT_FALSE(ZrCore_Utf8_EncodeCodePoint(0xd800u, buffer, &encodedLength));
    TEST_ASSERT_EQUAL_UINT64(99u, encodedLength);
    TEST_ASSERT_FALSE(ZrCore_Utf8_EncodeCodePoint(0xdfffu, buffer, &encodedLength));
    TEST_ASSERT_FALSE(ZrCore_Utf8_EncodeCodePoint(0x110000u, buffer, &encodedLength));
    TEST_ASSERT_TRUE(ZrCore_Utf8_EncodeCodePoint(0x10ffffu, buffer, &encodedLength));
    TEST_ASSERT_EQUAL_UINT64(4u, encodedLength);
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_raw_object_hash_uses_object_address);
    RUN_TEST(test_module_restores_protocol_bits_above_array_like);
    RUN_TEST(test_utf8_encoder_rejects_non_scalar_values);
    return UNITY_END();
}
