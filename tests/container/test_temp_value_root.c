#include <string.h>

#include "unity.h"

#include "container_test_common.h"
#include "runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser/parser.h"

void setUp(void) {}

void tearDown(void) {}

static SZrFunction *compile_test_script(SZrState *state, const char *path, const char *source) {
    SZrString *sourceName;

    if (state == ZR_NULL || path == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)path, strlen(path));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static TZrBool execute_array_factory_and_root(SZrState *state,
                                              SZrFunction *factoryFunction,
                                              ZrLibTempValueRoot *root) {
    SZrTypeValue resultValue;

    if (state == ZR_NULL || factoryFunction == ZR_NULL || root == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_TempValueRoot_Begin(state, root)) {
        return ZR_FALSE;
    }
    if (!ZrTests_Runtime_Function_Execute(state, factoryFunction, &resultValue) ||
        !ZrLib_TempValueRoot_SetValue(root, &resultValue)) {
        ZrLib_TempValueRoot_End(root);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static void test_temp_value_root_restores_existing_function_top_without_growth(void) {
    SZrState *state = ZrContainerTests_CreateState();
    ZrLibTempValueRoot root;
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer originalFunctionTop;
    TZrMemoryOffset savedStackTopOffset;
    TZrMemoryOffset originalFunctionTopOffset;
    SZrObject *object;
    SZrTypeValue *rootValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->callInfoList);

    savedStackTop = state->stackTop.valuePointer;
    TEST_ASSERT_NOT_NULL(savedStackTop);
    TEST_ASSERT_TRUE((state->stackTail.valuePointer - savedStackTop) >= 4);

    originalFunctionTop = savedStackTop + 3;
    savedStackTopOffset = ZrCore_Stack_SavePointerAsOffset(state, savedStackTop);
    originalFunctionTopOffset = ZrCore_Stack_SavePointerAsOffset(state, originalFunctionTop);
    state->callInfoList->functionTop.valuePointer = originalFunctionTop;

    memset(&root, 0, sizeof(root));
    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);

    TEST_ASSERT_TRUE(ZrLib_TempValueRoot_Begin(state, &root));
    TEST_ASSERT_TRUE(ZrLib_TempValueRoot_SetObject(&root, object, ZR_VALUE_TYPE_OBJECT));

    rootValue = ZrLib_TempValueRoot_Value(&root);
    TEST_ASSERT_NOT_NULL(rootValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, rootValue->type);
    TEST_ASSERT_EQUAL_PTR(object, rootValue->value.object);

    ZrLib_TempValueRoot_End(&root);

    TEST_ASSERT_EQUAL_PTR(ZrCore_Stack_LoadOffsetToPointer(state, savedStackTopOffset), state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(ZrCore_Stack_LoadOffsetToPointer(state, originalFunctionTopOffset),
                          state->callInfoList->functionTop.valuePointer);

    ZrContainerTests_DestroyState(state);
}

static void test_object_invoke_member_restores_existing_function_top_without_growth(void) {
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *factoryFunction;
    ZrLibTempValueRoot root;
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer originalFunctionTop;
    TZrMemoryOffset preInvokeStackTopOffset;
    TZrMemoryOffset originalFunctionTopOffset;
    SZrTypeValue *receiverValue;
    SZrTypeValue argumentValue;
    SZrTypeValue resultValue;
    SZrString *memberName;
    const char *factorySource =
            "var container = %import(\"zr.container\");\n"
            "return new container.Array<int>();\n";

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->callInfoList);

    factoryFunction = compile_test_script(state, "temp_value_root_array_factory.zr", factorySource);
    TEST_ASSERT_NOT_NULL(factoryFunction);

    savedStackTop = state->stackTop.valuePointer;
    TEST_ASSERT_NOT_NULL(savedStackTop);
    TEST_ASSERT_TRUE((state->stackTail.valuePointer - savedStackTop) >= 6);

    originalFunctionTop = savedStackTop + 4;
    originalFunctionTopOffset = ZrCore_Stack_SavePointerAsOffset(state, originalFunctionTop);
    state->callInfoList->functionTop.valuePointer = originalFunctionTop;

    memset(&root, 0, sizeof(root));
    TEST_ASSERT_TRUE(execute_array_factory_and_root(state, factoryFunction, &root));
    TEST_ASSERT_EQUAL_PTR(originalFunctionTop, state->callInfoList->functionTop.valuePointer);
    preInvokeStackTopOffset = ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer);
    receiverValue = ZrLib_TempValueRoot_Value(&root);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_TRUE(receiverValue->type == ZR_VALUE_TYPE_OBJECT || receiverValue->type == ZR_VALUE_TYPE_ARRAY);

    memberName = ZrCore_String_Create(state, (TZrNativeString)"add", 3);
    TEST_ASSERT_NOT_NULL(memberName);
    ZrCore_Value_InitAsInt(state, &argumentValue, 7);
    ZrCore_Value_ResetAsNull(&resultValue);

    TEST_ASSERT_TRUE(ZrCore_Object_InvokeMember(state, receiverValue, memberName, &argumentValue, 1, &resultValue));
    TEST_ASSERT_EQUAL_PTR(ZrCore_Stack_LoadOffsetToPointer(state, preInvokeStackTopOffset), state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(ZrCore_Stack_LoadOffsetToPointer(state, originalFunctionTopOffset),
                          state->callInfoList->functionTop.valuePointer);

    ZrLib_TempValueRoot_End(&root);
    ZrCore_Function_Free(state, factoryFunction);
    ZrContainerTests_DestroyState(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_temp_value_root_restores_existing_function_top_without_growth);
    RUN_TEST(test_object_invoke_member_restores_existing_function_top_without_growth);

    return UNITY_END();
}
