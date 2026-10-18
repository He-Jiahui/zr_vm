/* Public Library readers with a valid direct-argument context and actual Core.
 * ReadInt keeps its exception contract; Math helpers are not used. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "zr_vm_library/native_binding.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
static Allocation allocations[32768];
static size_t liveCount;
static int allocatorError, failures;
#define EXPECT(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t index = 0;
    void *next;
    (void)data;
    if (old != NULL) {
        while (index < liveCount && allocations[index].pointer != old) ++index;
        if (index == liveCount || allocations[index].size != oldSize || allocations[index].flag != flag) {
            allocatorError = 1;
            return NULL;
        }
    } else if (oldSize != 0) { allocatorError = 1; return NULL; }
    if (newSize == 0) {
        if (old != NULL) { free(old); allocations[index] = allocations[--liveCount]; }
        return NULL;
    }
    next = realloc(old, (size_t)newSize);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        index = liveCount++;
    }
    allocations[index].pointer = next;
    allocations[index].size = newSize;
    allocations[index].flag = flag;
    return next;
}

typedef struct Read {
    ZrLibCallContext context;
    SZrTypeValue argument;
    TZrInt64 integer;
    TZrFloat64 number;
    TZrBool returned;
    int nullOutput;
    int readFloat;
} Read;
static const ZrLibFunctionDescriptor descriptor = {.name = "numericProbe"};

static void read_value(SZrState *state, TZrPtr data) {
    Read *read = (Read *)data;
    (void)state;
    if (read->readFloat)
        read->returned = ZrLib_CallContext_ReadFloat(&read->context, 0,
                read->nullOutput ? NULL : &read->number);
    else
        read->returned = ZrLib_CallContext_ReadInt(&read->context, 0,
                read->nullOutput ? NULL : &read->integer);
}
static void initialize_read(SZrState *state, Read *read) {
    memset(read, 0, sizeof(*read));
    read->context.state = state;
    read->context.functionDescriptor = &descriptor;
    read->context.argumentValues = &read->argument;
    read->context.argumentCount = 1;
    read->integer = 12345;
    read->number = 12345;
}
static void set_double(Read *read, double number) {
    read->argument.type = ZR_VALUE_TYPE_DOUBLE;
    read->argument.value.nativeObject.nativeDouble = number;
}
static const char *exception_message(SZrState *state) {
    const SZrTypeValue *message;
    if (!state->hasCurrentException || state->currentException.type != ZR_VALUE_TYPE_OBJECT ||
        state->currentException.value.object == NULL) return NULL;
    message = ZrLib_Object_GetFieldCString(state,
            (SZrObject *)state->currentException.value.object, "message");
    if (message == NULL || message->type != ZR_VALUE_TYPE_STRING ||
        message->value.object == NULL) return NULL;
    return ZrCore_String_GetNativeString((SZrString *)message->value.object);
}
static void expect_error(SZrState *state, Read *read, const char *fragment) {
    EZrThreadStatus status = ZrCore_Exception_TryRun(state, read_value, read);
    const char *message;
    EXPECT(status == ZR_THREAD_STATUS_RUNTIME_ERROR);
    EXPECT(!read->returned && read->integer == 12345 && read->number == 12345);
    EXPECT(state->exceptionRecoverPoint == NULL);
    message = exception_message(state);
    EXPECT(message != NULL && strstr(message, "numericProbe") != NULL &&
           strstr(message, fragment) != NULL);
    if (read->context.argumentCount != 0)
        EXPECT(message != NULL && strstr(message, "expected int") != NULL);
    ZrCore_Exception_ClearCurrent(state);
}
static void rejected_double(SZrState *state, double number, int nullOutput) {
    Read read;
    initialize_read(state, &read);
    set_double(&read, number);
    read.nullOutput = nullOutput;
    expect_error(state, &read, "argument 1");
}
static void accepted_double(SZrState *state, double number, TZrInt64 expected) {
    Read read;
    initialize_read(state, &read);
    set_double(&read, number);
    EXPECT(ZrCore_Exception_TryRun(state, read_value, &read) == ZR_THREAD_STATUS_FINE);
    EXPECT(read.returned && read.integer == expected && !state->hasCurrentException);
}
static void valid_ints(SZrState *state) {
    Read read;
    accepted_double(state, 7.9, 7);
    accepted_double(state, -7.9, -7);
    accepted_double(state, -0.75, 0);
    accepted_double(state, -0x1p63, INT64_MIN);
    accepted_double(state, 0x1.fffffffffffffp62, INT64_MAX - INT64_C(1023));
    initialize_read(state, &read);
    read.argument.type = ZR_VALUE_TYPE_INT64;
    read.argument.value.nativeObject.nativeInt64 = INT64_MAX;
    EXPECT(ZrCore_Exception_TryRun(state, read_value, &read) == ZR_THREAD_STATUS_FINE);
    EXPECT(read.returned && read.integer == INT64_MAX);
    initialize_read(state, &read);
    read.argument.type = ZR_VALUE_TYPE_UINT64;
    read.argument.value.nativeObject.nativeUInt64 = 42;
    EXPECT(ZrCore_Exception_TryRun(state, read_value, &read) == ZR_THREAD_STATUS_FINE);
    EXPECT(read.returned && read.integer == 42);
}
static void float_controls(SZrState *state) {
    const double inputs[] = {NAN, INFINITY, -INFINITY, 1e300, -7.9};
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        Read read;
        initialize_read(state, &read);
        set_double(&read, inputs[i]);
        read.readFloat = 1;
        EXPECT(ZrCore_Exception_TryRun(state, read_value, &read) == ZR_THREAD_STATUS_FINE);
        EXPECT(read.returned && !state->hasCurrentException);
        EXPECT(isnan(inputs[i]) ? isnan(read.number) : read.number == inputs[i]);
    }
}
static void run_case(SZrState *state, const char *name) {
    if (strcmp(name, "int_controls") == 0) valid_ints(state);
    else if (strcmp(name, "finite_range") == 0) {
        rejected_double(state, 1e300, 0);
        rejected_double(state, -1e300, 0);
    } else if (strcmp(name, "nonfinite") == 0) {
        rejected_double(state, NAN, 0);
        rejected_double(state, INFINITY, 0);
        rejected_double(state, -INFINITY, 0);
    } else if (strcmp(name, "boundaries") == 0) {
        rejected_double(state, 0x1p63, 0);
        rejected_double(state, -0x1.0000000000001p63, 0);
    } else if (strcmp(name, "null_output") == 0) {
        Read read;
        initialize_read(state, &read);
        set_double(&read, 3.9);
        read.nullOutput = 1;
        EXPECT(ZrCore_Exception_TryRun(state, read_value, &read) == ZR_THREAD_STATUS_FINE);
        EXPECT(read.returned && read.integer == 12345);
        rejected_double(state, 1e300, 1);
        rejected_double(state, NAN, 1);
    } else if (strcmp(name, "type_diagnostic") == 0) {
        Read read;
        initialize_read(state, &read);
        read.argument.type = ZR_VALUE_TYPE_BOOL;
        expect_error(state, &read, "expected int");
    } else if (strcmp(name, "arity_diagnostic") == 0) {
        Read read;
        initialize_read(state, &read);
        read.context.argumentCount = 0;
        expect_error(state, &read, "expected at least 1 arguments");
    } else if (strcmp(name, "float_controls") == 0) float_controls(state);
    else ++failures;
}
int main(int argc, char **argv) {
    SZrGlobalState *global;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    EXPECT(global != NULL && ZrCore_GlobalState_IsInitialized(global));
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) {
            global->garbageCollector->stopGcFlag = ZR_TRUE;
            run_case(global->mainThreadState, argv[1]);
        }
        ZrCore_GlobalState_Free(global);
    }
    EXPECT(liveCount == 0 && !allocatorError);
    return failures == 0 ? 0 : 1;
}

/**
 * @struct Allocation
 *
 * ## Summary
 * 记录宿主分配器当前持有块的地址、申请尺寸和 Core 内存类别，供释放及重分配校验；它不持有另一份存储。
 *
 * ## Fields and invariants
 * pointer 是仍归此分配器管理的非空块；size 和 flag 是下一次旧块操作须复用的元数据。记录仅在 allocations 的 [0, liveCount) 有效前缀中解释，尾部过期内容不代表仍存活的块。
 *
 * ## Constraints and lifecycle
 * 回调成功后才发布新记录；释放以最后一项填补空位，故索引与记录地址不稳定。真实块由 realloc/free 管理；复制记录不复制或延长块生命周期。所有表访问要求串行，结构不提供锁或指针可读性认证。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:20
 * tests/library/test_native_numeric_boundaries.c:32
 * tests/library/test_native_numeric_boundaries.c:33
 * tests/library/test_native_numeric_boundaries.c:39
 * tests/library/test_native_numeric_boundaries.c:48
 */

/**
 * @var allocations
 *
 * ## Summary
 * 在一个测试进程内保存最多 32768 个活跃宿主块的账目，供 Core 构造、数值读取和销毁共用。
 *
 * ## Constraints and lifecycle
 * 只有 [0, liveCount) 有效；静态零初始化不等于对块的申请，表本身无需 free。成功的新分配增加前缀，释放会搬入末项，重分配更新原项。容量上限不是故障注入：满表时，新块已由 realloc 取得后会 abort，不能依赖正常尾部清理。表和三个计数/错误状态均无同步，须由宿主按串行回调前提使用。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:21
 * tests/library/test_native_numeric_boundaries.c:22
 * tests/library/test_native_numeric_boundaries.c:23
 * tests/library/test_native_numeric_boundaries.c:39
 * tests/library/test_native_numeric_boundaries.c:45
 * tests/library/test_native_numeric_boundaries.c:198
 * tests/library/test_native_numeric_boundaries.c:207
 */

/**
 * @def EXPECT(c)
 *
 * ## Summary
 * 在保留后续诊断和清理的前提下检查条件；失败时向 stderr 写原调用行与表达式拼写，并增加 failures。
 *
 * ## Parameters
 * @param[in] c 作为条件求值一次的表达式；字符串化仅输出其拼写，不再次求值。
 *
 * ## Returns
 * 无返回值；do/while(0) 形成一条语句。条件为假仍继续当前函数，不执行 return、abort 或非局部跳转；fprintf 的结果不作检查。
 *
 * ## Constraints and lifecycle
 * 需要本翻译单元的 failures 可写及 stderr 可用；它不建立异常恢复点，也不确保条件表达式抛出或进程被终止时还能执行清理。原 __LINE__ 是诊断的一部分，调用位置须保持。仅适用于本进程串行断言。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:24
 * tests/library/test_native_numeric_boundaries.c:101
 * tests/library/test_native_numeric_boundaries.c:109
 * tests/library/test_native_numeric_boundaries.c:199
 * tests/library/test_native_numeric_boundaries.c:205
 * tests/library/test_native_numeric_boundaries.c:208
 */

/**
 * @fn static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize, TZrSize newSize, TZrInt64 flag)
 *
 * ## Summary
 * 作为实际 Core 的宿主分配回调，在 realloc/free 周围核对旧块协议并维护活跃账目，供 main 检查销毁后的回收结果。
 *
 * ## Parameters
 * @param[in] data 回调用户数据，当前忽略；main 传 NULL。
 * @param[in,out] old NULL 表示新申请，否则须是本账表中仍存活、可由 C realloc/free 处理的旧块。
 * @param[in] oldSize old 的当前真实登记尺寸；old 为 NULL 时必须为 0。
 * @param[in] newSize 非零时申请或重分配，零时释放匹配旧块。
 * @param[in] flag 须与旧块的登记类别一致；新块保存此类别供下一次检查。
 *
 * ## Returns
 * @return 非零尺寸成功时返回取得所有权的新块，调用方此后须经同一回调/类别释放；释放、协议拒绝或 realloc 失败返回 NULL。协议拒绝置 allocatorError 且不修改旧账目/块；自然 realloc 失败保留旧块和账目。释放成功会 free 旧块并缩短有效前缀。
 *
 * ## Constraints and lifecycle
 * Core 在构造入口直接调用，并安装预算包装器继续转交 upstreamAllocator；本回调没有 OOM ordinal 或主动故障注入。元数据真实不代替任意地址认证；不得并发访问无锁账表。新申请成功后若已有 32768 条活跃记录则 abort，且不保证普通清理；需在正常使用中不超过该容量。成功 realloc 可搬移地址，旧借用随之失效；失败时旧借用保持原分配器语义。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:26
 * tests/library/test_native_numeric_boundaries.c:33
 * tests/library/test_native_numeric_boundaries.c:37
 * tests/library/test_native_numeric_boundaries.c:39
 * tests/library/test_native_numeric_boundaries.c:42
 * tests/library/test_native_numeric_boundaries.c:45
 * tests/library/test_native_numeric_boundaries.c:48
 * tests/library/test_native_numeric_boundaries.c:198
 * zr_vm_common/include/zr_vm_common/zr_common_conf.h:182
 * zr_vm_core/src/zr_vm_core/runtime/global.c:370
 * zr_vm_core/src/zr_vm_core/runtime/global.c:374
 * zr_vm_core/src/zr_vm_core/execution/execution_memory.c:65
 */

/**
 * @struct Read
 *
 * ## Summary
 * 把一个栈上数值探针的直接实参视图、输出哨兵和 reader 选择放在一起，供同步 TryRun 回调读取。
 *
 * ## Fields and invariants
 * context 借用 state 和静态 descriptor；argumentValues 经 initialize_read 指向本记录的 argument。integer/number 初值均为 12345；returned 初值为假，仅在 reader 普通返回后赋值。nullOutput 控制传 NULL 输出，readFloat 选择浮点 reader；二者非零视为真。
 *
 * ## Constraints and lifecycle
 * 初始化后不得直接复制或移动记录而继续使用原 argumentValues；若换地址须重新绑定。记录及 state 需活到同步 TryRun 返回，不能延迟保留 data；不拥有 VM、描述符或 GC 根。当前实参仅为原生标量，不用此布局证明 GC 对象保活或完整 native 调用帧合法性。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:54
 * tests/library/test_native_numeric_boundaries.c:66
 * tests/library/test_native_numeric_boundaries.c:76
 * tests/library/test_native_numeric_boundaries.c:78
 * tests/library/test_native_numeric_boundaries.c:79
 * tests/library/test_native_numeric_boundaries.c:81
 * tests/library/test_native_numeric_boundaries.c:99
 * zr_vm_library/include/zr_vm_library/native_binding.h:137
 * zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:80
 */

/**
 * @var descriptor
 *
 * ## Summary
 * 为手工构造的读取上下文提供进程期调用名 numericProbe，使类型和参数个数错误带上可断言的名称。
 *
 * ## Constraints and lifecycle
 * 只初始化 name，其余字段静态为零，包括 callback；它未注册成 VM native 函数，也不提供执行回调。上下文只借用本静态只读记录和字符串，无需释放。读取错误通过 native_binding_call_name 选择此名；min/max 的零值不是此探针执行自动 arity 检查的证据。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:63
 * tests/library/test_native_numeric_boundaries.c:78
 * tests/library/test_native_numeric_boundaries.c:105
 * zr_vm_library/include/zr_vm_library/native_binding.h:217
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:306
 */

/**
 * @fn static void read_value(SZrState *state, TZrPtr data)
 *
 * ## Summary
 * 适配 Core TryRun 的回调签名，用 Read.context 对第 0 个实参执行整数或浮点 reader，并记录普通返回结果。
 *
 * ## Parameters
 * @param[in] state TryRun 传入的活动状态；本适配器忽略此参数，真实 reader 使用 context.state，调用方须使二者对应。
 * @param[in,out] data 非空且已初始化、绑定稳定的 Read；在整个同步调用期间可读写。
 *
 * ## Returns
 * 无 C 返回值；reader 普通返回后写 returned，成功输出由 reader 写入 integer 或 number。reader 发生 Throw 时赋值不完成，传播到外层 TryRun；不在本函数中捕获或清理异常。
 *
 * ## Constraints and lifecycle
 * readFloat 非零选择 ReadFloat，其他情况选择 ReadInt；nullOutput 非零只改变输出指针，不跳过类型/范围/arity 验证。data/context/state 与可写输出不可发生破坏性重叠；不分配或接管 Read，不承诺底层错误构造没有分配。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:65
 * tests/library/test_native_numeric_boundaries.c:69
 * tests/library/test_native_numeric_boundaries.c:72
 * tests/library/test_native_numeric_boundaries.c:99
 * zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:80
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1120
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1150
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1164
 */

/**
 * @fn static void initialize_read(SZrState *state, Read *read)
 *
 * ## Summary
 * 在每次读取前建立独立直接实参上下文和输出哨兵，使成功写入、拒绝保留输出及诊断名称可分别检查。
 *
 * ## Parameters
 * @param[in] state 非空且已成功初始化、在探针结束前保持有效的 Core state；仅借用。
 * @param[out] read 独立可写的 Read 存储；被 memset 覆盖，不得与 state/descriptor 重叠或承载待释放的外部资源。
 *
 * ## Returns
 * 无返回值；清零整个记录后绑定 state、descriptor 和自身 argument，设置一个实参以及两个 12345 哨兵。它不执行 reader，也不报告输入地址是否合法。
 *
 * ## Constraints and lifecycle
 * 当前调用方用栈上记录反复初始化；旧记录本身不拥有待清理资源。不得通过复制成品记录延长自指视图；只初始化 context 不建立完整 VM 调用帧或新的 GC 根。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:75
 * tests/library/test_native_numeric_boundaries.c:76
 * tests/library/test_native_numeric_boundaries.c:78
 * tests/library/test_native_numeric_boundaries.c:79
 * tests/library/test_native_numeric_boundaries.c:80
 * tests/library/test_native_numeric_boundaries.c:81
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:735
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:741
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:409
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:445
 */

/**
 * @fn static void set_double(Read *read, double number)
 *
 * ## Summary
 * 把待测 C double 放入 Read 的 DOUBLE 类型实参，保留其余上下文、开关及输出哨兵，供两类 reader 对同一数值输入作比较。
 *
 * ## Parameters
 * @param[in,out] read 已经 initialize_read 绑定的非空可写记录。
 * @param[in] number 原样保存的待测 double，包括 NaN、无穷或整数范围外值。
 *
 * ## Returns
 * 无返回值；仅设置 argument.type 和 nativeDouble。此准备步骤不验证范围、不执行整数转换，也不产生异常状态。
 *
 * ## Constraints and lifecycle
 * 只用于当前不拥有对象资源的标量实参；覆盖旧 argument 不执行 VM 值释放。number 的机器表示由 C 实现提供，本函数不证明所有平台均支持这些边界常量。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:84
 * tests/library/test_native_numeric_boundaries.c:85
 * tests/library/test_native_numeric_boundaries.c:86
 * tests/library/test_native_numeric_boundaries.c:114
 * tests/library/test_native_numeric_boundaries.c:148
 * zr_vm_common/include/zr_vm_common/zr_common_conf.h:65
 */

/**
 * @fn static const char *exception_message(SZrState *state)
 *
 * ## Summary
 * 在错误已被 TryRun 捕获后，从当前异常对象的 message 字段借出 NUL 结尾文本，供立即进行诊断片段断言。
 *
 * ## Parameters
 * @param[in,out] state 非空活动 state；读取其当前异常，字段查询可能经 VM 字符串/对象接口分配或修改运行时内部状态。
 *
 * ## Returns
 * @return 当前异常为非空对象且 message 为非空字符串时返回借用字符指针；这些形状检查不满足时返回 NULL。字段查询的非局部失败继续传播，不转换成统一 NULL 事务。
 *
 * ## Constraints and lifecycle
 * 返回文本不归调用方 free，须在异常清除、对象修改/释放或可能移动 GC 之前消费。当前 main 停止通常 GC，expect_error 在 ClearCurrent 前完成 strstr；这不证明任意强制 GC、OOM 或异常创建失败下均能读取诊断。函数不清除异常、不复制文本。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:88
 * tests/library/test_native_numeric_boundaries.c:90
 * tests/library/test_native_numeric_boundaries.c:92
 * tests/library/test_native_numeric_boundaries.c:94
 * tests/library/test_native_numeric_boundaries.c:96
 * tests/library/test_native_numeric_boundaries.c:104
 * tests/library/test_native_numeric_boundaries.c:109
 * tests/library/test_native_numeric_boundaries.c:202
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1404
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1417
 * zr_vm_core/include/zr_vm_core/string.h:218
 */

/**
 * @fn static void expect_error(SZrState *state, Read *read, const char *fragment)
 *
 * ## Summary
 * 通过真实异常恢复入口执行一个预期被拒绝的读取，核对状态、未写输出、恢复点撤销及命名诊断，随后清除当前异常。
 *
 * ## Parameters
 * @param[in,out] state 非空且初始化的 state；入口没有外层 exceptionRecoverPoint，以便断言返回后为 NULL，并由 context.state 指向它。
 * @param[in,out] read 初始化且其 reader 设置预期产生运行时错误的 Read；输出哨兵尚未被修改。
 * @param[in] fragment 非空 NUL 结尾的必含诊断片段，在本函数返回前可读。
 *
 * ## Returns
 * 无返回值；每项 EXPECT 失败累加 failures 而仍继续后续检查和 ClearCurrent。底层普通恢复后总会走显式清异常；abort、未捕获的二次异常或外部终止不受此尾部清理保证。
 *
 * ## Constraints and lifecycle
 * 正常恢复必须检查 RUNTIME_ERROR、returned 为假、两个输出仍为 12345、恢复点为 NULL；消息须含 numericProbe 与 fragment，argumentCount 非零时再检查 expected int。只检查片段而非完整诊断/准确类型编码；ClearCurrent 不等于销毁 Error 或重置 threadStatus。read 和 fragment 为借用，函数不释放它们。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:98
 * tests/library/test_native_numeric_boundaries.c:99
 * tests/library/test_native_numeric_boundaries.c:101
 * tests/library/test_native_numeric_boundaries.c:102
 * tests/library/test_native_numeric_boundaries.c:103
 * tests/library/test_native_numeric_boundaries.c:105
 * tests/library/test_native_numeric_boundaries.c:107
 * tests/library/test_native_numeric_boundaries.c:109
 * zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:83
 * zr_vm_core/src/zr_vm_core/exception/exception.c:145
 * zr_vm_core/src/zr_vm_core/exception/exception.c:150
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:867
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:882
 */

/**
 * @fn static void rejected_double(SZrState *state, double number, int nullOutput)
 *
 * ## Summary
 * 为一个应被整数 reader 拒绝的 double 建立栈上探针，统一验证第一个参数的异常及输出保留。
 *
 * ## Parameters
 * @param[in,out] state 满足 expect_error 前提的活动 state。
 * @param[in] number 本场景预期位于整数转换允许区间之外或非有限的输入。
 * @param[in] nullOutput 非零时传 NULL 输出；仍要求 reader 拒绝数值，不能因此绕过验证。
 *
 * ## Returns
 * 无返回值；expect_error 累积失败并在普通恢复后清异常，局部 Read 不拥有需 free 的资源。
 *
 * ## Constraints and lifecycle
 * 每次独立初始化，不沿用上一场景 returned 或输出值；实参指向局部 Read，必须保持同步调用。它不覆盖 allocator 故障、所有 IEEE 位模式或不同整数宽度。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:111
 * tests/library/test_native_numeric_boundaries.c:113
 * tests/library/test_native_numeric_boundaries.c:114
 * tests/library/test_native_numeric_boundaries.c:115
 * tests/library/test_native_numeric_boundaries.c:116
 * tests/library/test_native_numeric_boundaries.c:158
 * tests/library/test_native_numeric_boundaries.c:165
 * tests/library/test_native_numeric_boundaries.c:174
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1150
 */

/**
 * @fn static void accepted_double(SZrState *state, double number, TZrInt64 expected)
 *
 * ## Summary
 * 对一个可表示的 double 运行整数 reader，确认普通返回、预期向零截断结果及无当前异常，作为拒绝场景的正对照。
 *
 * ## Parameters
 * @param[in,out] state 初始化且无当前异常的 state，需与新 Read.context.state 一致。
 * @param[in] number 当前平台可表示并位于 [-2^63, 2^63) 内的待测 double。
 * @param[in] expected 场景为该输入指定的 int64 结果。
 *
 * ## Returns
 * 无返回值；EXPECT 累积状态或结果不符。TryRun 捕获意外异常后仍检查输出，但本 helper 不 ClearCurrent；错误状态由全局销毁最终回收，而非本函数保证恢复到干净场景。
 *
 * ## Constraints and lifecycle
 * 只在 reader 已核范围后发生整数转换；记录在栈上、回调同步，不接管 state。五个调用是有限样本；不得由成功状态推断所有转换或无输出路径都已验证。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:118
 * tests/library/test_native_numeric_boundaries.c:120
 * tests/library/test_native_numeric_boundaries.c:122
 * tests/library/test_native_numeric_boundaries.c:123
 * tests/library/test_native_numeric_boundaries.c:127
 * tests/library/test_native_numeric_boundaries.c:130
 * tests/library/test_native_numeric_boundaries.c:131
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1146
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1150
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1154
 */

/**
 * @fn static void valid_ints(SZrState *state)
 *
 * ## Summary
 * 组合五个 double 正例与两个原生整数类型正例，区分截断、下界/上界邻值以及直接整数读取路径。
 *
 * ## Parameters
 * @param[in,out] state 初始化且无当前异常的活动 state；由 int_controls selector 借用。
 *
 * ## Returns
 * 无返回值；断言失败计入 failures 并继续。局部 Read 无堆资源；没有为意外异常主动清除当前异常。
 *
 * ## Constraints and lifecycle
 * 五个 double 为 7.9、-7.9、-0.75、-2^63 和 2^63-1024，预期分别 7、-7、0、INT64_MIN、INT64_MAX-1023。另只检查 INT64_MAX 的有符号路径和 UINT64 值 42；不覆盖大于 INT64_MAX 的无符号转换或每种整数类型。后两次断言不额外检查 hasCurrentException。边界常量依赖当前 double/64 位整数表示；不是全平台运行见证。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:125
 * tests/library/test_native_numeric_boundaries.c:127
 * tests/library/test_native_numeric_boundaries.c:130
 * tests/library/test_native_numeric_boundaries.c:131
 * tests/library/test_native_numeric_boundaries.c:133
 * tests/library/test_native_numeric_boundaries.c:135
 * tests/library/test_native_numeric_boundaries.c:138
 * tests/library/test_native_numeric_boundaries.c:140
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1126
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1139
 * zr_vm_common/include/zr_vm_common/zr_common_conf.h:57
 * zr_vm_common/include/zr_vm_common/zr_common_conf.h:65
 */

/**
 * @fn static void float_controls(SZrState *state)
 *
 * ## Summary
 * 以浮点 reader 对照整数拒绝策略，验证 NaN、两种无穷、大有限数和负小数可以按浮点类型直接读出。
 *
 * ## Parameters
 * @param[in,out] state 初始化且无当前异常的活动 state；借用到所有同步读取结束。
 *
 * ## Returns
 * 无返回值；每次记录独立初始化，失败经 EXPECT 累积，不中断循环；不显式清除意外异常。
 *
 * ## Constraints and lifecycle
 * 循环输入恰为 NAN、INFINITY、-INFINITY、1e300、-7.9；断言 TryRun FINE、returned 且无当前异常，NaN 用 isnan，其他值用 ==。不检查 NaN payload、符号零或全部位模式，也不证明整数 reader 应接受同一输入。数组与 Read 均为局部自动存储，无需手动释放。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:143
 * tests/library/test_native_numeric_boundaries.c:144
 * tests/library/test_native_numeric_boundaries.c:147
 * tests/library/test_native_numeric_boundaries.c:149
 * tests/library/test_native_numeric_boundaries.c:150
 * tests/library/test_native_numeric_boundaries.c:151
 * tests/library/test_native_numeric_boundaries.c:152
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1187
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1189
 */

/**
 * @fn static void run_case(SZrState *state, const char *name)
 *
 * ## Summary
 * 把 CTest 传入的单一 selector 映射到八个现有数值读取场景，让每个进程拥有独立 Core 与分配账目。
 *
 * ## Parameters
 * @param[in,out] state 已成功初始化且通常 GC 被 main 停止的活动 state。
 * @param[in] name 非空 NUL 结尾 selector；main 已确认 argv 中存在该元素。
 *
 * ## Returns
 * 无返回值；已知 selector 执行对应断言，未知名称增加一次 failures。各辅助函数的普通断言不早退；非局部错误、分配失败或终止沿其真实路径处理。
 *
 * ## Constraints and lifecycle
 * finite_range 拒绝 ±1e300；nonfinite 拒绝 NaN/±Infinity；boundaries 拒绝 +2^63 和低于 -2^63 的相邻测试常量。null_output 先验证 3.9 在无输出时成功且 integer 不变，再拒绝 1e300/NaN。type_diagnostic 用 BOOL 检查 expected int；arity_diagnostic 把 count 改为 0 检查至少一个参数；其余两项交给正对照 helper。没有通过 VM native 描述符回调或编译脚本来执行这些读取；没有完整 GC/并发/OOM/Math 路径信用。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:155
 * tests/library/test_native_numeric_boundaries.c:156
 * tests/library/test_native_numeric_boundaries.c:158
 * tests/library/test_native_numeric_boundaries.c:161
 * tests/library/test_native_numeric_boundaries.c:165
 * tests/library/test_native_numeric_boundaries.c:169
 * tests/library/test_native_numeric_boundaries.c:172
 * tests/library/test_native_numeric_boundaries.c:174
 * tests/library/test_native_numeric_boundaries.c:179
 * tests/library/test_native_numeric_boundaries.c:184
 * tests/library/test_native_numeric_boundaries.c:186
 * tests/library/test_native_numeric_boundaries.c:187
 * tests/cmake/native-numeric-boundary-tests.cmake:26
 * tests/cmake/native-numeric-boundary-tests.cmake:29
 */

/**
 * @fn int main(int argc, char **argv)
 *
 * ## Summary
 * 为一个 selector 创建真实 Core，停止通常 GC 后执行读取场景，销毁全局状态并检查宿主账目，向 CTest 返回失败汇总。
 *
 * ## Parameters
 * @param[in] argc 启动参数个数；必须等于 2 才创建 Core。
 * @param[in] argv 运行时提供的参数数组；argv[1] 在 argc 为 2 时为借用 selector，需是有效 NUL 字符串。
 *
 * ## Returns
 * @return 参数数目不为 2 时返回 2；普通完成时 failures 为零返回 0，否则返回 1。分配器满表 abort、运行时 panic 或外部终止不返回这些正常退出码。
 *
 * ## Constraints and lifecycle
 * Windows 分支先设置进程/CRT 错误报告方式，不恢复这些进程设置；其他平台不执行该分支。GlobalState_New 的第三实参 1 是 uniqueNumber，不是线程数；回调账表仍要求串行使用。构造结果为空或未初始化会累积失败，非空 global 在普通路径仍销毁；仅初始化成功后访问 garbageCollector、设置 stopGcFlag 并运行 selector。清理后只检查 liveCount==0 且无协议错误，不证明全部系统资源或任意并发/显式 GC 均正确；本程序不创建文件夹具，不注册 CMocka/Unity case。
 *
 * ## References
 * tests/library/test_native_numeric_boundaries.c:189
 * tests/library/test_native_numeric_boundaries.c:192
 * tests/library/test_native_numeric_boundaries.c:197
 * tests/library/test_native_numeric_boundaries.c:198
 * tests/library/test_native_numeric_boundaries.c:199
 * tests/library/test_native_numeric_boundaries.c:201
 * tests/library/test_native_numeric_boundaries.c:202
 * tests/library/test_native_numeric_boundaries.c:203
 * tests/library/test_native_numeric_boundaries.c:205
 * tests/library/test_native_numeric_boundaries.c:207
 * tests/library/test_native_numeric_boundaries.c:208
 * tests/cmake/native-numeric-boundary-tests.cmake:9
 * tests/cmake/native-numeric-boundary-tests.cmake:26
 * tests/cmake/native-numeric-boundary-tests.cmake:30
 * zr_vm_core/src/zr_vm_core/runtime/global.c:360
 * zr_vm_core/src/zr_vm_core/runtime/global.c:733
 * zr_vm_core/include/zr_vm_core/global.h:214
 */
