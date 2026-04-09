#include <string.h>

#include "unity.h"

#include "container_test_common.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"

void setUp(void) {}

void tearDown(void) {}

static void test_temp_value_root_restores_existing_function_top_without_growth(void) {
    SZrState *state = ZrContainerTests_CreateState();
    ZrLibTempValueRoot root;
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer originalFunctionTop;
    SZrObject *object;
    SZrTypeValue *rootValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->callInfoList);

    savedStackTop = state->stackTop.valuePointer;
    TEST_ASSERT_NOT_NULL(savedStackTop);
    TEST_ASSERT_TRUE((state->stackTail.valuePointer - savedStackTop) >= 4);

    originalFunctionTop = savedStackTop + 3;
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

    TEST_ASSERT_EQUAL_PTR(savedStackTop, state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(originalFunctionTop, state->callInfoList->functionTop.valuePointer);

    ZrContainerTests_DestroyState(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_temp_value_root_restores_existing_function_top_without_growth);

    return UNITY_END();
}
