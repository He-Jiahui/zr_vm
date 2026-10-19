/* Native registry attach and record OOM paths must leave owned state intact. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif

#include "native_binding/native_binding_internal.h"
#include "zr_vm_core/global.h"

typedef struct RegistryOomAllocator {
    TZrBool armed;
    TZrSize failArrayAllocation;
    TZrSize arrayAllocationAttempts;
    TZrBool failArrayGrowth;
    TZrSize failGlobalAllocation;
    TZrSize globalAllocationAttempts;
    TZrSize liveArrayBlocks;
    TZrSize liveGlobalBlocks;
} RegistryOomAllocator;

static TZrPtr registry_oom_allocator(TZrPtr userData,
                                     TZrPtr pointer,
                                     TZrSize originalSize,
                                     TZrSize newSize,
                                     TZrInt64 type) {
    RegistryOomAllocator *tracker = (RegistryOomAllocator *)userData;
    TZrBool fail = ZR_FALSE;
    TZrPtr result;

    if (tracker->armed && type == ZR_MEMORY_NATIVE_TYPE_GLOBAL &&
        pointer == ZR_NULL && newSize != 0u && tracker->failGlobalAllocation != 0u) {
        fail = ++tracker->globalAllocationAttempts == tracker->failGlobalAllocation;
    }

    if (tracker->armed && type == ZR_MEMORY_NATIVE_TYPE_ARRAY && newSize != 0u &&
        ((tracker->failArrayAllocation != 0u && pointer == ZR_NULL) ||
         tracker->failArrayGrowth)) {
        ++tracker->arrayAllocationAttempts;
        fail = tracker->failArrayAllocation != 0u
                       ? tracker->arrayAllocationAttempts == tracker->failArrayAllocation
                       : (tracker->failArrayGrowth && pointer != ZR_NULL && newSize > originalSize);
    }
    if (fail) {
        return ZR_NULL;
    }

    result = newSize == 0u ? (free(pointer), ZR_NULL) : realloc(pointer, newSize);
    if (type == ZR_MEMORY_NATIVE_TYPE_ARRAY) {
        if (pointer == ZR_NULL && result != ZR_NULL) {
            ++tracker->liveArrayBlocks;
        } else if (pointer != ZR_NULL && newSize == 0u) {
            --tracker->liveArrayBlocks;
        }
    }
    if (type == ZR_MEMORY_NATIVE_TYPE_GLOBAL) {
        if (pointer == ZR_NULL && result != ZR_NULL && newSize != 0u) {
            ++tracker->liveGlobalBlocks;
        } else if (pointer != ZR_NULL && newSize == 0u) {
            --tracker->liveGlobalBlocks;
        }
    }
    return result;
}

static SZrGlobalState *registry_oom_global_new(RegistryOomAllocator *tracker) {
    SZrCallbackGlobal callbacks = {0};
    return ZrCore_GlobalState_New(registry_oom_allocator, tracker, 0x4f4f4d, &callbacks);
}

static void registry_oom_fill_array(SZrArray *array) {
    memset(array->head + array->length * array->elementSize,
           0,
           (array->capacity - array->length) * array->elementSize);
    array->length = array->capacity;
}

static void registry_oom_prepare_descriptor(ZrLibModuleDescriptor *descriptor,
                                            const TZrChar *moduleName,
                                            const ZrLibFunctionDescriptor *functions,
                                            TZrSize functionCount) {
    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION;
    descriptor->moduleName = moduleName;
    descriptor->functions = functions;
    descriptor->functionCount = functionCount;
    descriptor->providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME;
}

static TZrBool registry_oom_native_callback(ZrLibCallContext *context,
                                            SZrTypeValue *result) {
    ZR_UNUSED_PARAMETER(context);
    ZR_UNUSED_PARAMETER(result);
    return ZR_TRUE;
}

static int test_attach_initial_array_oom(TZrSize failureOrdinal) {
    RegistryOomAllocator tracker = {0};
    SZrGlobalState *global = registry_oom_global_new(&tracker);
    ZrLibrary_NativeRegistryState *registry;
    TZrSize liveArrays;
    TZrSize liveGlobals;

    if (global == ZR_NULL) return 1;
    liveArrays = tracker.liveArrayBlocks;
    liveGlobals = tracker.liveGlobalBlocks;
    tracker.armed = ZR_TRUE;
    tracker.failArrayAllocation = failureOrdinal;
    if (ZrLibrary_NativeRegistry_Attach(global) ||
        global->nativeRegistryState != ZR_NULL ||
        global->nativeRegistryStateCleanup != ZR_NULL ||
        tracker.liveArrayBlocks != liveArrays || tracker.liveGlobalBlocks != liveGlobals) {
        ZrCore_GlobalState_Free(global);
        return 2;
    }

    tracker.armed = ZR_FALSE;
    tracker.failArrayAllocation = 0u;
    if (!ZrLibrary_NativeRegistry_Attach(global)) {
        ZrCore_GlobalState_Free(global);
        return 3;
    }
    registry = native_registry_get(global);
    if (registry == ZR_NULL || !registry->moduleRecords.isValid ||
        !registry->bindingEntries.isValid || !registry->pluginHandles.isValid) {
        ZrLibrary_NativeRegistry_Free(global);
        ZrCore_GlobalState_Free(global);
        return 4;
    }
    ZrLibrary_NativeRegistry_Free(global);
    ZrCore_GlobalState_Free(global);
    return 0;
}

static int test_module_record_growth_oom(void) {
    RegistryOomAllocator tracker = {0};
    SZrGlobalState *global = registry_oom_global_new(&tracker);
    ZrLibrary_NativeRegistryState *registry;
    ZrLibModuleDescriptor descriptor;
    TZrSize liveGlobals;

    if (global == ZR_NULL || !ZrLibrary_NativeRegistry_Attach(global)) {
        if (global != ZR_NULL) ZrCore_GlobalState_Free(global);
        return 10;
    }
    registry = native_registry_get(global);
    registry_oom_fill_array(&registry->moduleRecords);
    registry_oom_prepare_descriptor(&descriptor, "zr.test.oom-record", ZR_NULL, 0u);
    tracker.armed = ZR_TRUE;
    tracker.failArrayGrowth = ZR_TRUE;
    liveGlobals = tracker.liveGlobalBlocks;

    if (ZrLibrary_NativeRegistry_RegisterModule(global, &descriptor) ||
        ZrLibrary_NativeRegistry_FindModule(global, descriptor.moduleName) != ZR_NULL ||
        tracker.liveGlobalBlocks != liveGlobals) {
        tracker.armed = ZR_FALSE;
        ZrLibrary_NativeRegistry_Free(global);
        ZrCore_GlobalState_Free(global);
        return 11;
    }
    tracker.armed = ZR_FALSE;
    ZrLibrary_NativeRegistry_Free(global);
    ZrCore_GlobalState_Free(global);
    return 0;
}

static int test_binding_entry_growth_oom(void) {
    RegistryOomAllocator tracker = {0};
    SZrGlobalState *global = registry_oom_global_new(&tracker);
    ZrLibrary_NativeRegistryState *registry;
    ZrLibModuleDescriptor descriptor;
    ZrLibFunctionDescriptor functionDescriptor = {0};
    SZrTypeValue value;
    TZrSize bindingCount;

    if (global == ZR_NULL || !ZrLibrary_NativeRegistry_Attach(global)) {
        if (global != ZR_NULL) ZrCore_GlobalState_Free(global);
        return 20;
    }
    registry = native_registry_get(global);
    registry_oom_fill_array(&registry->bindingEntries);
    registry_oom_prepare_descriptor(&descriptor, "zr.test.oom-binding", &functionDescriptor, 1u);
    functionDescriptor.name = "invoke";
    functionDescriptor.callback = registry_oom_native_callback;
    bindingCount = registry->bindingEntries.length;
    tracker.armed = ZR_TRUE;
    tracker.failArrayGrowth = ZR_TRUE;

    if (native_binding_make_callable_value(global->mainThreadState,
                                           registry,
                                           ZR_LIB_RESOLVED_BINDING_FUNCTION,
                                           &descriptor,
                                           ZR_NULL,
                                           ZR_NULL,
                                           &functionDescriptor,
                                           &value) ||
        registry->bindingEntries.length != bindingCount) {
        tracker.armed = ZR_FALSE;
        ZrLibrary_NativeRegistry_Free(global);
        ZrCore_GlobalState_Free(global);
        return 21;
    }
    tracker.armed = ZR_FALSE;
    ZrLibrary_NativeRegistry_Free(global);
    ZrCore_GlobalState_Free(global);
    return 0;
}

/* Each mode executes in its own CTest process; never dereference a failed-load descriptor. */
static int test_plugin_transaction_oom(int mode, const char *fixturePath) {
    RegistryOomAllocator tracker = {0};
    SZrGlobalState *global = registry_oom_global_new(&tracker);
    ZrLibrary_NativeRegistryState *registry;
    const ZrLibModuleDescriptor *previous = ZR_NULL;
    const ZrLibModuleDescriptor *loaded;
    TZrSize moduleCount, handleCount, liveGlobals;
    int result = 0;

    if (global == ZR_NULL || !ZrLibrary_NativeRegistry_Attach(global)) {
        if (global != ZR_NULL) ZrCore_GlobalState_Free(global);
        return 30;
    }
    registry = native_registry_get(global);
    if (mode == 4) {
        previous = native_registry_load_plugin_descriptor(
                global->mainThreadState, registry, fixturePath, "zr.pluginprobe");
        if (previous == ZR_NULL) {
            ZrCore_GlobalState_Free(global);
            return 31;
        }
    }
    if (mode == 1) registry_oom_fill_array(&registry->pluginHandles);
    if (mode == 3) registry_oom_fill_array(&registry->moduleRecords);
    moduleCount = registry->moduleRecords.length;
    handleCount = registry->pluginHandles.length;
    liveGlobals = tracker.liveGlobalBlocks;
    tracker.failArrayGrowth = mode == 1 || mode == 3;
    tracker.failGlobalAllocation = mode == 2 || mode == 4 ? 3u : 0u;
    tracker.armed = ZR_TRUE;
    loaded = native_registry_load_plugin_descriptor(
            global->mainThreadState, registry, fixturePath, "zr.pluginprobe");
    tracker.armed = ZR_FALSE;
    if (loaded != ZR_NULL ||
        ZrLibrary_NativeRegistry_FindModule(global, "zr.pluginprobe") != previous ||
        registry->moduleRecords.length != moduleCount ||
        registry->pluginHandles.length != handleCount ||
        tracker.liveGlobalBlocks != liveGlobals) {
        result = 32;
    }
    ZrLibrary_NativeRegistry_Free(global);
    ZrCore_GlobalState_Free(global);
    return result;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc == 3) {
        if (strcmp(argv[1], "plugin_handle_growth") == 0) return test_plugin_transaction_oom(1, argv[2]);
        if (strcmp(argv[1], "plugin_string_allocation") == 0) return test_plugin_transaction_oom(2, argv[2]);
        if (strcmp(argv[1], "plugin_record_growth") == 0) return test_plugin_transaction_oom(3, argv[2]);
        if (strcmp(argv[1], "plugin_reload_allocation") == 0) return test_plugin_transaction_oom(4, argv[2]);
    }
    if (argc != 2) return 64;
    if (strcmp(argv[1], "attach_array_1") == 0) return test_attach_initial_array_oom(1u);
    if (strcmp(argv[1], "attach_array_2") == 0) return test_attach_initial_array_oom(2u);
    if (strcmp(argv[1], "attach_array_3") == 0) return test_attach_initial_array_oom(3u);
    if (strcmp(argv[1], "module_record_growth") == 0) return test_module_record_growth_oom();
    if (strcmp(argv[1], "binding_entry_growth") == 0) return test_binding_entry_growth_oom();
    return 65;
}

/**
 * @file test_native_registry_oom.c
 *
 * ## Summary
 * 通过宿主分配器的类别和请求序号，核对 native registry 初始化、记录增容和插件准备失败时的有限所有权边界。CTest 每次仅选择一个场景，在独立进程中构造并销毁 Core global；本文件不使用 Unity，也不执行夹具中的 native 函数。
 *
 * ## Constraints and lifecycle
 * 分配器 tracker 位于场景栈上，必须覆盖 global 的完整生存期；初始化和准备基线时不注入，重试及增容/插件场景清理前撤销注入；首轮 Attach 的观察失败分支仍可在 arm 状态下直接销毁 global，释放请求本身不受注入。断言限于各场景实际比较的指针、长度和 ARRAY/GLOBAL 块数，不证明所有字段回滚、最终零存量或全 OOM 点覆盖。插件模式读取 CMake 提供的动态库，并可能在临时目录生成影子映像；关闭和删除由加载器/注册器承担，删除结果不在本测试中断言。
 *
 * ## References
 * main 解释九个 selector；RegistryOomAllocator 实现故障窗口；ZrLibrary_NativeRegistry_Attach、native_binding_make_callable_value 与 native_registry_load_plugin_descriptor 提供被测路径；tests/cmake/native-registry-oom-tests.cmake 注册目标、夹具依赖和 30 秒 CTest 限时。
 */

/**
 * @struct RegistryOomAllocator
 *
 * ## Summary
 * 保存单个测试进程中宿主分配器的注入开关、匹配请求计数和两类原生块存量，供场景比较操作前后的有限资源状态。
 *
 * ## Fields
 * armed 只在被测调用窗口启用注入；未启用时仍跟踪两类存量。
 * failArrayAllocation 是匹配 ARRAY 请求的一基失败序号，0 表示不按序号失败；failArrayGrowth 请求拒绝已有 ARRAY 块的实际增容。
 * arrayAllocationAttempts 累计已启用且符合 ARRAY 筛选条件的非零请求；启用 growth 时也会计入不增长的候选请求。
 * failGlobalAllocation 是新建 GLOBAL 块的一基失败序号，0 表示不注入；globalAllocationAttempts 仅在非零失败序号已配置时，累计已启用、旧指针为空且新大小非零的 GLOBAL 请求。
 * liveArrayBlocks 和 liveGlobalBlocks 记录对应类别成功取得且尚未释放的块数，不统计字节数、其他类别或操作系统句柄。
 *
 * ## Invariants and lifecycle
 * 各场景以全零值建立 tracker，再将地址借给 Core 作为 upstream allocator 参数，直到 GlobalState_Free 返回后才结束栈上对象生存期。计数跨同一 tracker 的多次 arm 保留；本文件的首次预加载未 arm，不消耗故障序号。两个 ARRAY 模式同时配置时非零序号优先；当前调用点分别选择一种模式。状态无内部同步，只用于本进程串行场景。
 *
 * ## References
 * registry_oom_allocator 读取故障配置并更新计数；registry_oom_global_new 借用 tracker；test_attach_initial_array_oom、test_module_record_growth_oom、test_binding_entry_growth_oom、test_plugin_transaction_oom 建立各自基线。
 */

/**
 * @fn static TZrPtr registry_oom_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 type)
 *
 * ## Summary
 * 作为 Core 的 upstream FZrAllocator 回调，将指定 ARRAY/GLOBAL 请求变为普通 NULL 失败；其余请求交给 C realloc/free，并保留可比较的块计数。
 *
 * ## Parameters
 * @param[in,out] userData 指向存活的 RegistryOomAllocator；由 registry_oom_global_new 借入，不转移所有权。
 * @param[in,out] pointer 同一 C 分配器取得的旧块或 NULL；非零请求失败时旧块仍归原持有者，零大小请求释放它。
 * @param[in] originalSize 旧请求的字节数，仅用于判断 ARRAY 请求是否实际增长；须由 Core 与旧块配对传入。
 * @param[in] newSize 新请求字节数；0 走释放路径，且不触发故障注入。
 * @param[in] type Core 透传的内存类别；只有 ARRAY 和 GLOBAL 参与本测试的注入及存量统计。
 *
 * ## Returns
 * @return 注入失败或宿主非零请求失败返回 NULL；后者沿用 realloc 保留旧块的语义。成功返回新块地址；newSize 为 0 时释放旧块并返回 NULL。
 *
 * ## Constraints and lifecycle
 * 注入发生在 realloc/free 前，因此不会释放或改变旧块。仅成功的首次申请增加块计数，释放既有块才减少计数；成功搬迁和失败重分配均不改变块数。tracker 和同一旧块不得被并发操作，本回调不取得锁，也不承担 GC、异常恢复或全部资源泄漏检查。
 *
 * ## References
 * FZrAllocator 是 Core 的宿主分配接口；ZrCore_GlobalState_New 保存该回调，ZrCore_ExecutionBudget_Allocate 透传日后的请求；ZrCore_Array_TryInit 和 zr_core_array_try_reserve 分别产生初始 ARRAY 分配及增容。
 */

/**
 * @fn static SZrGlobalState *registry_oom_global_new(RegistryOomAllocator *tracker)
 *
 * ## Summary
 * 在故障注入尚未启用的场景准备期，以固定 uniqueNumber 和全零宿主回调表创建 Core global，统一安装本文件的受控分配器。
 *
 * ## Parameters
 * @param[in,out] tracker 已全零初始化且地址稳定的分配状态；创建和销毁 global 期间会更新其存量，生存期必须覆盖返回状态的完整使用期。
 *
 * ## Returns
 * @return Core 创建的 global 或 NULL；非 NULL 结果由场景使用并交给 ZrCore_GlobalState_Free，tracker 仍归调用者。
 *
 * ## Constraints and lifecycle
 * 局部 callbacks 的值由 Core 复制，不借出该局部表。helper 不自动 Attach registry，不停止 GC，也不新增异常恢复点；构造失败和异常策略沿用 Core 的实际路径。固定 uniqueNumber 仅用于本测试初始化，不构成跨进程身份或隔离保证。
 *
 * ## References
 * ZrCore_GlobalState_New 复制 SZrCallbackGlobal 并保存 upstream allocator；registry_oom_allocator 使用 tracker；四个 test_* 场景在返回后负责清理 global。
 */

/**
 * @fn static void registry_oom_fill_array(SZrArray *array)
 *
 * ## Summary
 * 把已初始化注册器数组的未使用槽清零，并将逻辑长度提升到容量，使下一次追加必须经过容量检查和增容路径。
 *
 * ## Parameters
 * @param[in,out] array head 非空、elementSize 非零、length 不超过 capacity，且容量字节区间可写、尺寸运算可表示的已初始化数组；现有缓冲区和元素仍归注册器。
 *
 * ## Returns
 * 完成后原有元素不变，原空闲槽为全零且 length 等于 capacity；没有返回值，也不申请新缓冲区。
 *
 * ## Constraints and lifecycle
 * 本 helper 只用于本文件三类记录数组；它依赖全零附加记录表示不持有字符串、闭包或动态库句柄。它不是通用元素构造器，不调用析构或注册逻辑，不得用于未初始化数组或含需特别初始化状态的其他元素类型。填充不产生有效模块或绑定。
 *
 * ## References
 * test_module_record_growth_oom、test_binding_entry_growth_oom 和 test_plugin_transaction_oom 分别填满 moduleRecords、bindingEntries 或 pluginHandles；ZrCore_Array_TryPush 在长度已满时调用 zr_core_array_try_reserve。
 */

/**
 * @fn static void registry_oom_prepare_descriptor(ZrLibModuleDescriptor *descriptor, const TZrChar *moduleName, const ZrLibFunctionDescriptor *functions, TZrSize functionCount)
 *
 * ## Summary
 * 为记录和绑定增容场景建立最小运行期描述符，避免无关元数据影响本测试的容量边界。
 *
 * ## Parameters
 * @param[out] descriptor 可写的完整描述符对象；先整体清零，再写入 ABI、名称、函数表与运行期阶段。
 * @param[in] moduleName 调用期间有效的 NUL 结尾模块名；只保存借用指针，不复制字符。
 * @param[in] functions 借用的函数表；当前调用点使用 NULL/0 或一项表/1，本 helper 不验证表和数量是否配对。
 * @param[in] functionCount 函数表元素数，与 functions 的实际可读范围一致。
 *
 * ## Returns
 * 通过 descriptor 交付浅层初始化结果；无返回值，不申请或转移资源，也不声明任意输入均可通过生产者校验。
 *
 * ## Constraints and lifecycle
 * 输出描述符不得与 moduleName 字符或 functions 表存储重叠；整体清零不释放旧指向资源。名称、函数表和描述符本体须覆盖生产者借用它们的阶段；两个调用点均在销毁 registry 后才离开其栈帧。未显式填写的字段保持零值。后续函数描述符字段可由场景在注入前补齐。
 *
 * ## References
 * test_module_record_growth_oom 和 test_binding_entry_growth_oom 构造输入；ZrLibModuleDescriptor 声明浅层字段；ZrLibrary_NativeRegistry_RegisterModule 与 native_binding_make_callable_value 消费描述符。
 */

/**
 * @fn static TZrBool registry_oom_native_callback(ZrLibCallContext *context, SZrTypeValue *result)
 *
 * ## Summary
 * 为绑定增容场景提供非空的 callback 身份，使被测输入具有函数指针；该场景只尝试建立 callable，不派发它。
 *
 * ## Parameters
 * @param[in] context 回调 ABI 的调用上下文；本占位实现不读取，可为 NULL。
 * @param[in] result 回调 ABI 提供的结果槽地址；本占位实现不访问也不初始化，可为 NULL，调用者不得从此实现推断有效结果值。
 *
 * ## Returns
 * @return 固定返回 ZR_TRUE；仅是占位状态，不表示 result 已写入或某个数值计算完成。
 *
 * ## Constraints and lifecycle
 * 本函数地址存入栈上 functionDescriptor；目标路径在 bindingEntries 追加失败时返回，测试没有调用该函数。实现不保存参数、不分配资源，也不提供通用可执行 native 函数的结果契约。
 *
 * ## References
 * test_binding_entry_growth_oom 设置 ZrLibFunctionDescriptor.callback；FZrLibBoundCallback 定义 ABI；native_binding_make_callable_value 建立绑定时借用该描述符。
 */

/**
 * @fn static int test_attach_initial_array_oom(TZrSize failureOrdinal)
 *
 * ## Summary
 * 按 failureOrdinal 选择 Attach 的前三次初始 ARRAY 申请之一失败，检查未发布的 registry 清理，并在同一 global 关闭注入后重试 Attach。
 *
 * ## Parameters
 * @param[in] failureOrdinal 当前 main 仅传 1、2、3，分别对应 moduleRecords、bindingEntries 和 pluginHandles 的初始申请。
 *
 * ## Returns
 * @return 0 表示失败观察、重试及三个 isValid 检查均满足；1 表示 global 创建返回 NULL，2 表示首轮失败状态不符，3 表示无注入重试失败，4 表示重试后的数组状态不符。
 *
 * ## Constraints and lifecycle
 * global 先成功创建，再记录 ARRAY/GLOBAL 基线并 arm。首轮要求 Attach 返回假、两个 registry 发布字段为空且两类块数回到基线；不比较其他宿主钩子。重试前关闭注入并清零失败序号。普通返回路径释放已持有状态；未加入新的异常恢复层，不能把进程异常或中止称为已执行尾部清理。
 *
 * ## References
 * main 的 attach_array_1、attach_array_2、attach_array_3 分支调用本函数；ZrLibrary_NativeRegistry_Attach 先 TryInit 三数组再发布状态；ZrLibrary_NativeRegistry_Free 与 ZrCore_GlobalState_Free 负责普通清理。
 */

/**
 * @fn static int test_module_record_growth_oom(void)
 *
 * ## Summary
 * 填满 moduleRecords 后拒绝 ARRAY 增容，核对静态模块注册返回失败、不按名称发布该模块，且临时 GLOBAL 字符串块被归还。
 *
 * ## Returns
 * @return 0 表示三项实际观察均满足；10 表示 global 创建或 Attach 失败，11 表示注册结果、名称查找或 GLOBAL 块数不符。
 *
 * ## Constraints and lifecycle
 * 注册器先正常 Attach，空槽填充不会创建模块身份；描述符及其名称保持有效直到 registry 被释放。仅在注册调用前启用增长失败，观察不符和普通结束均先关闭注入再清理。本测试不直接断言数组长度、全部回调状态或最终零存量，不能据 0 推出这些额外保证。
 *
 * ## References
 * main 的 module_record_growth 分支为入口；registry_oom_fill_array 制造已满容量；ZrLibrary_NativeRegistry_RegisterModule 经 native_registry_register_module_record 复制定位字符串，TryPush 失败时释放它们；ZrLibrary_NativeRegistry_FindModule 检查未发布。
 */

/**
 * @fn static int test_binding_entry_growth_oom(void)
 *
 * ## Summary
 * 填满 bindingEntries 后拒绝 ARRAY 增容，检查 callable 建立返回失败且注册器的绑定逻辑长度不增加。
 *
 * ## Returns
 * @return 0 表示被测函数返回假且 bindingEntries.length 保持基线；20 表示 global 创建或 Attach 失败，21 表示返回状态或长度不符。
 *
 * ## Constraints and lifecycle
 * 描述符、函数表和未初始化的 value 均位于本栈帧；失败后不读取 value，也不执行占位 callback。closure 创建及身份计算发生在绑定 TryPush 之前，失败对象的 GC 所有权沿用 Core；本测试没有对象类别存量或立即回收断言。普通路径先撤销注入，再释放 registry 和 global；不能据长度不变声称所有中间资源均即时回滚。
 *
 * ## References
 * main 的 binding_entry_growth 分支调用本函数；registry_oom_native_callback 提供指针身份；native_binding_make_callable_value 在 TryPush 成功后才 MarkAsPermanent 并写出 value；ZrCore_ClosureNative_New 使用 Core GC 分配。
 */

/**
 * @fn static int test_plugin_transaction_oom(int mode, const char *fixturePath)
 *
 * ## Summary
 * 围绕真实描述符动态库的加载准备阶段注入四种失败，比较当前模块身份、模块/句柄逻辑长度与 GLOBAL 块数，防止失败加载提交半个注册结果。
 *
 * ## Parameters
 * @param[in] mode main 仅传 1..4：1 填满 pluginHandles 后拒绝增长；2 在首次加载的第三次新 GLOBAL 申请失败；3 填满 moduleRecords 后拒绝增长；4 先成功预载，再令重载的第三次新 GLOBAL 申请失败。
 * @param[in] fixturePath CMake 传入的描述符夹具动态库路径，NUL 结尾且在整个场景期间稳定；不转移原始库文件所有权。
 *
 * ## Returns
 * @return 0 表示被测加载返回 NULL、名称查找仍为 previous、两数组长度及 GLOBAL 块数等于基线；30 表示 global/Attach 失败，31 表示模式 4 预载失败，32 表示上述任一观察不符。
 *
 * ## Constraints and lifecycle
 * 预载和容量填充均在 arm 前完成，基线包含预载资源和填充槽。第三次 GLOBAL 申请在当前生产顺序中对应 handleRecord.loadedPath 的复制；此失败发生在新句柄提交之前。previous 只作借用指针身份比较，loaded 失败时不得解引用。撤销注入后再做观察和普通清理；registry 释放关闭所持句柄并尝试删除影子文件，不保证删除成功。模式 1..3 没有预载成功控制，代码也不检查故障计数，所以返回 0 只证明列出的观察，不单独证明必已到达目标失败请求。
 *
 * ## References
 * main 的四个 plugin_* selector 调用本函数；native_registry_load_plugin_descriptor 先准备路径字符串、保留句柄，再提交模块记录；native_registry_release_plugin_handle_record 和 ZrLibrary_NativeRegistry_Free 负责句柄与字符串；tests/cmake/native-registry-oom-tests.cmake 提供夹具与临时目录。
 */

/**
 * @fn int main(int argc, char **argv)
 *
 * ## Summary
 * 把九个 CTest selector 分派到独立的 OOM 场景，并以场景整数结果作为进程退出状态；不启动 Unity 或执行语言脚本。
 *
 * ## Parameters
 * @param[in] argc 含程序名的参数数目；基础五个场景需要 2，四个插件场景需要 3。
 * @param[in] argv 进程借用的 NUL 结尾参数数组；argv[1] 是精确 selector，插件模式的 argv[2] 是描述符夹具路径。
 *
 * ## Returns
 * @return 已识别场景原样返回其结果；argc 不符合已识别插件形式且不等于 2 时返回 64；argc 等于 2 但 selector 未识别时返回 65。场景 0 仅代表其源码实际比较通过。
 *
 * ## Constraints and lifecycle
 * Windows 分支在分派前改变本进程错误框及 CRT 报告策略，并把断言报告导向 stderr；不恢复设置，依赖 CTest 的一次一场景进程边界。五个基础 selector 不接收文件路径；四个插件 selector 由 CMake 提供同一构建夹具路径并设置 TMP/TEMP/TMPDIR。所有既有 CTest 条目限时 30 秒；本文件不自行实施 watchdog，也不输出测试计数或 PASS 记录。
 *
 * ## References
 * test_attach_initial_array_oom、test_module_record_growth_oom、test_binding_entry_growth_oom 和 test_plugin_transaction_oom 是直接下游；tests/cmake/native-registry-oom-tests.cmake 注册全部九项并依赖 zr_vm_descriptor_plugin_fixture_int。
 */
