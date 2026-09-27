#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/session_checkpoint.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

void setUp(void) {}
/* BUG: checkpoint_test_state 创建的 global 和 checkpoint 都由用例局部变量
 * 持有，任何 Unity 断言提前退出都会跳过 Free；空 teardown 无法回收。 */
void tearDown(void) {}

/* checkpoint API 要求空闲的主线程状态，并依赖已初始化的模块注册表。 */
static SZrState *checkpoint_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(
            ZrTests_Runtime_Allocator_Default, ZR_NULL, 0x43504f494e54u, &callbacks);

    TEST_ASSERT_NOT_NULL(global);
    TEST_ASSERT_NOT_NULL(global->mainThreadState);
    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    return global->mainThreadState;
}

static SZrTypeValue string_key(SZrState *state, const char *text) {
    SZrTypeValue key;
    SZrString *string = ZrCore_String_CreateFromNative(state, (TZrNativeString)text);

    TEST_ASSERT_NOT_NULL(string);
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(string));
    return key;
}

static void test_checkpoint_restores_cycles_aliases_and_loaded_module_registry(void) {
    SZrState *state = checkpoint_test_state();
    SZrGlobalState *global = state->global;
    SZrSessionCheckpoint *checkpoint = ZR_NULL;
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    SZrObject *retained = ZrCore_Object_New(state, ZR_NULL);
    SZrObject *lazyModule = ZrCore_Object_New(state, ZR_NULL);
    SZrTypeValue retainedKey = string_key(state, "retained");
    SZrTypeValue selfKey = string_key(state, "self");
    SZrTypeValue counterKey = string_key(state, "counter");
    SZrTypeValue lazyKey = string_key(state, "lazy-module");
    SZrTypeValue retainedValue;
    SZrTypeValue counterValue;
    SZrTypeValue lazyValue;
    const SZrTypeValue *restored;

    TEST_ASSERT_NOT_NULL(registry);
    TEST_ASSERT_NOT_NULL(retained);
    TEST_ASSERT_NOT_NULL(lazyModule);
    ZrCore_Object_Init(state, retained);
    ZrCore_Object_Init(state, lazyModule);
    ZrCore_Value_InitAsRawObject(state, &retainedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(retained));
    ZrCore_Value_InitAsInt(state, &counterValue, 1);
    ZrCore_Object_SetValue(state, registry, &retainedKey, &retainedValue);
    ZrCore_Object_SetValue(state, retained, &selfKey, &retainedValue);
    ZrCore_Object_SetValue(state, retained, &counterKey, &counterValue);

    /* 快照保留对象身份及环/别名，随后同一 registry 的新增项应在 rollback
     * 后消失；这里直接复用原对象指针以检验原位恢复契约。 */
    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Create(state, &checkpoint));
    TEST_ASSERT_NOT_NULL(checkpoint);

    ZrCore_Value_InitAsInt(state, &counterValue, 99);
    ZrCore_Object_SetValue(state, retained, &counterKey, &counterValue);
    ZrCore_Value_InitAsRawObject(state, &lazyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(lazyModule));
    ZrCore_Object_SetValue(state, registry, &lazyKey, &lazyValue);

    TEST_ASSERT_TRUE(ZrCore_SessionCheckpoint_Rollback(state, checkpoint));
    restored = ZrCore_Object_GetValue(state, retained, &counterKey);
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_EQUAL_INT64(1, restored->value.nativeObject.nativeInt64);
    restored = ZrCore_Object_GetValue(state, retained, &selfKey);
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_EQUAL_PTR(retained, restored->value.object);
    TEST_ASSERT_NULL(ZrCore_Object_GetValue(state, registry, &lazyKey));

    ZrCore_SessionCheckpoint_Free(state, checkpoint);
    ZrCore_GlobalState_Free(global);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_checkpoint_restores_cycles_aliases_and_loaded_module_registry);
    return UNITY_END();
}
