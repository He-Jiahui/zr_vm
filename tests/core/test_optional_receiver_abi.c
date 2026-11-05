#include "unity.h"
#include <string.h>
#include "tests/harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

void setUp(void) {}
void tearDown(void) {}

static TZrInstruction test_create_instruction_return_1(TZrUInt16 returnCount, TZrUInt16 resultSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    instruction.instruction.operandExtra = returnCount;
    instruction.instruction.operand.operand1[0] = resultSlot;
    instruction.instruction.operand.operand1[1] = 0u;
    return instruction;
}

static SZrFunction *test_create_noop_function_with_signature(SZrState *state, TZrUInt16 parameterCount) {
    SZrFunction *function;
    TZrInstruction instruction;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = ZR_INSTRUCTION_ENUM(NOP);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = instruction;
    function->instructionsLength = 1u;

    function->constantValueList = ZR_NULL;
    function->constantValueLength = 0u;
    function->stackSize = parameterCount;
    function->parameterCount = parameterCount;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}


static void test_optional_receiver_identity_shadowing_and_native_modes(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *name;
    SZrObjectPrototype *prototype;
    SZrObject *object;
    SZrFunction *method, *plain;
    SZrClosureNative *native;
    SZrMemberDescriptor descriptor;
    SZrTypeValue receiver, callee, key, argument, result;
    TEST_ASSERT_NOT_NULL(state);
    name = ZrCore_String_CreateFromNative(state, "id");
    prototype = ZrCore_ObjectPrototype_New(state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    object = ZrCore_Object_New(state, prototype);
    method = test_create_noop_function_with_signature(state, 2u);
    plain = test_create_noop_function_with_signature(state, 1u);
    plain->instructionsList[0] = test_create_instruction_return_1(1u, 0u);
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.name = name;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_METHOD;
    descriptor.methodFunction = method;
    descriptor.isDynamicWrite = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor));
    ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    ZrCore_Value_InitAsRawObject(state, &callee, ZR_CAST_RAW_OBJECT_AS_SUPER(method));
    TEST_ASSERT_TRUE(ZrCore_Object_MemberCallUsesReceiver(state, &receiver, name, &callee));
    prototype->memberDescriptors[0].isStatic = ZR_TRUE;
    TEST_ASSERT_FALSE(ZrCore_Object_MemberCallUsesReceiver(state, &receiver, name, &callee));
    prototype->memberDescriptors[0].isStatic = ZR_FALSE;
    ZrCore_Value_InitAsRawObject(state, &callee, ZR_CAST_RAW_OBJECT_AS_SUPER(plain));
    TEST_ASSERT_FALSE(ZrCore_Object_MemberCallUsesReceiver(state, &receiver, name, &callee));
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    ZrCore_Object_SetValue(state, object, &key, &callee);
    ZrCore_Value_InitAsInt(state, &argument, 41);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrCore_Object_InvokeMember(state, &receiver, name, &argument, 1u, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(41, result.value.nativeObject.nativeInt64);
    native = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(native);
    ZrCore_Value_InitAsRawObject(state, &callee, ZR_CAST_RAW_OBJECT_AS_SUPER(native));
    native->nativeBindingUsesReceiver = ZR_NATIVE_BINDING_RECEIVER_FRAME;
    TEST_ASSERT_TRUE(ZrCore_Object_MemberCallUsesReceiver(state, &receiver, name, &callee));
    native->nativeBindingUsesReceiver = ZR_NATIVE_BINDING_RECEIVER_CAPTURED;
    TEST_ASSERT_FALSE(ZrCore_Object_MemberCallUsesReceiver(state, &receiver, name, &callee));
    native->nativeBindingUsesReceiver = ZR_NATIVE_BINDING_RECEIVER_NONE;
    TEST_ASSERT_FALSE(ZrCore_Object_MemberCallUsesReceiver(state, &receiver, name, &callee));
    native->aotShimFunction = method;
    TEST_ASSERT_TRUE(ZrCore_Object_MemberCallUsesReceiver(state, &receiver, name, &callee));
    ZrTests_Runtime_State_Destroy(state);
}

static void test_builtin_native_method_and_host_invocation_use_receiver_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue receiver, callable, result;
    SZrString *name;
    TEST_ASSERT_NOT_NULL(state);
    name = ZrCore_String_CreateFromNative(state, "toArray");
    ZrCore_Value_InitAsRawObject(state, &receiver,
            ZR_CAST_RAW_OBJECT_AS_SUPER(ZrCore_String_CreateFromNative(state, "A")));
    ZrCore_Value_ResetAsNull(&callable);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrCore_Object_GetMember(state, &receiver, name, &callable));
    TEST_ASSERT_TRUE(ZrCore_Object_MemberCallUsesReceiver(state, &receiver, name, &callable));
    TEST_ASSERT_TRUE(ZrCore_Object_InvokeMember(state, &receiver, name, ZR_NULL, 0u, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, result.type);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_host_descriptorless_prototype_protocol_preserves_receiver_but_own_field_drops(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *name;
    SZrObjectPrototype *prototype;
    SZrObject *object;
    SZrFunction *protocol, *plain;
    SZrTypeValue receiver, callee, key, argument, result;
    TEST_ASSERT_NOT_NULL(state);
    name = ZrCore_String_CreateFromNative(state, "legacyProtocol");
    prototype = ZrCore_ObjectPrototype_New(state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    object = ZrCore_Object_New(state, prototype);
    protocol = test_create_noop_function_with_signature(state, 2u);
    protocol->instructionsList[0] = test_create_instruction_return_1(1u, 1u);
    plain = test_create_noop_function_with_signature(state, 1u);
    plain->instructionsList[0] = test_create_instruction_return_1(1u, 0u);
    ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    ZrCore_Value_InitAsRawObject(state, &callee, ZR_CAST_RAW_OBJECT_AS_SUPER(protocol));
    ZrCore_Object_SetValue(state, &prototype->super, &key, &callee);
    TEST_ASSERT_FALSE(ZrCore_Object_MemberCallUsesReceiver(state, &receiver, name, &callee));
    ZrCore_Value_InitAsInt(state, &argument, 41);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrCore_Object_InvokeMember(state, &receiver, name, &argument, 1u, &result));
    TEST_ASSERT_EQUAL_INT64(41, result.value.nativeObject.nativeInt64);
    ZrCore_Value_InitAsRawObject(state, &callee, ZR_CAST_RAW_OBJECT_AS_SUPER(plain));
    ZrCore_Object_SetValue(state, object, &key, &callee);
    TEST_ASSERT_TRUE(ZrCore_Object_InvokeMember(state, &receiver, name, &argument, 1u, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(41, result.value.nativeObject.nativeInt64);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_optional_receiver_identity_shadowing_and_native_modes);
    RUN_TEST(test_builtin_native_method_and_host_invocation_use_receiver_metadata);
    RUN_TEST(test_host_descriptorless_prototype_protocol_preserves_receiver_but_own_field_drops);
    return UNITY_END();
}
