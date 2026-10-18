/* Actual Math support API boundaries. Fractional numbers retain truncation.
 * The test computes no out-of-range floating-to-integer casts or signed products. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "zr_vm_lib_math/math_common.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"

typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
static Allocation allocations[32768];
static size_t liveCount;
static int failures, allocatorError;
#define EXPECT(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t i = 0;
    void *next;
    (void)data;
    if (old != NULL) {
        while (i < liveCount && allocations[i].pointer != old) ++i;
        if (i == liveCount || allocations[i].size != oldSize || allocations[i].flag != flag) {
            allocatorError = 1;
            return NULL;
        }
    } else if (oldSize != 0) { allocatorError = 1; return NULL; }
    if (newSize == 0) {
        if (old != NULL) { free(old); allocations[i] = allocations[--liveCount]; }
        return NULL;
    }
    next = realloc(old, (size_t)newSize);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        i = liveCount++;
    }
    allocations[i].pointer = next;
    allocations[i].size = newSize;
    allocations[i].flag = flag;
    return next;
}

static SZrTypeValue float_value(double input) {
    SZrTypeValue value;
    memset(&value, 0, sizeof(value));
    value.type = ZR_VALUE_TYPE_DOUBLE;
    value.value.nativeObject.nativeDouble = input;
    return value;
}
static void rejected_float(double input) {
    SZrTypeValue value = float_value(input);
    TZrInt64 out = 12345;
    EXPECT(!ZrMath_IntFromValue(&value, &out));
    EXPECT(out == 12345);
}
static void accepted_float(double input, TZrInt64 expected) {
    SZrTypeValue value = float_value(input);
    TZrInt64 out = 12345;
    EXPECT(ZrMath_IntFromValue(&value, &out));
    EXPECT(out == expected);
}
static void int_controls(void) {
    SZrTypeValue value;
    TZrInt64 out = 0;
    accepted_float(7.9, 7);
    accepted_float(-7.9, -7);
    accepted_float(-0.75, 0);
    accepted_float(0.0, 0);
    memset(&value, 0, sizeof(value));
    value.type = ZR_VALUE_TYPE_INT64;
    value.value.nativeObject.nativeInt64 = INT64_MAX;
    EXPECT(ZrMath_IntFromValue(&value, &out) && out == INT64_MAX);
    value.value.nativeObject.nativeInt64 = INT64_MIN;
    EXPECT(ZrMath_IntFromValue(&value, &out) && out == INT64_MIN);
    value.type = ZR_VALUE_TYPE_BOOL;
    out = 12345;
    EXPECT(!ZrMath_IntFromValue(&value, &out) && out == 12345);
}
static void int_boundaries(void) {
    /* Double(INT64_MAX) rounds to +2^63: it must not be used as an inclusive bound. */
    accepted_float(-0x1p63, INT64_MIN);
    accepted_float(0x1.fffffffffffffp62, INT64_MAX - INT64_C(1023));
    rejected_float(0x1p63);
    rejected_float(-0x1.0000000000001p63);
}
static SZrObject *new_array(SZrState *state) {
    SZrObject *result = ZrLib_Array_New(state);
    EXPECT(result != NULL);
    return result;
}
static void push_int(SZrState *state, SZrObject *array, TZrInt64 input) {
    SZrTypeValue value;
    ZrLib_Value_SetInt(state, &value, input);
    EXPECT(ZrLib_Array_PushValue(state, array, &value));
}
static void push_float(SZrState *state, SZrObject *array, double input) {
    SZrTypeValue value = float_value(input);
    EXPECT(ZrLib_Array_PushValue(state, array, &value));
}
static SZrObject *new_tensor_shell(SZrState *state) {
    SZrObject *tensor = ZrCore_Object_New(state, NULL);
    EXPECT(tensor != NULL);
    if (tensor != NULL) ZrCore_Object_Init(state, tensor);
    return tensor;
}
static void shape_product(SZrState *state) {
    SZrObject *shape = new_array(state);
    SZrObject *data = new_array(state);
    SZrObject *tensor = new_tensor_shell(state);
    if (shape == NULL || data == NULL || tensor == NULL) return;
    push_int(state, shape, INT64_MAX);
    push_int(state, shape, 2);
    EXPECT(ZrMath_TensorTotalSize(state, shape) == -1);
    EXPECT(!ZrMath_TensorPopulate(state, tensor, shape, data));
}
static void array_conversion(SZrState *state) {
    SZrObject *shape = new_array(state);
    TZrInt64 out = 12345;
    if (shape == NULL) return;
    push_float(state, shape, 1e300);
    EXPECT(!ZrMath_ArrayReadInt(state, shape, 0, &out) && out == 12345);
    EXPECT(ZrMath_TensorTotalSize(state, shape) == -1);
}
static void tensor_data(SZrState *state) {
    SZrObject *shape = new_array(state);
    SZrTypeValue invalid[3];
    if (shape == NULL) return;
    push_int(state, shape, 1);
    ZrLib_Value_SetString(state, &invalid[0], "not-a-number");
    ZrLib_Value_SetBool(state, &invalid[1], ZR_TRUE);
    ZrCore_Value_ResetAsNull(&invalid[2]);
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        SZrObject *data = new_array(state);
        SZrObject *tensor = new_tensor_shell(state);
        if (data == NULL || tensor == NULL) return;
        EXPECT(ZrLib_Array_PushValue(state, data, &invalid[i]));
        EXPECT(!ZrMath_TensorPopulate(state, tensor, shape, data));
    }
}
static void tensor_controls(SZrState *state, int invalidStorage) {
    SZrObject *shape = new_array(state);
    SZrObject *data = new_array(state);
    SZrObject *tensor = new_tensor_shell(state);
    ZrMathTensorStorage storage;
    if (shape == NULL || data == NULL || tensor == NULL) return;
    /* Existing shape policy truncates positive fractional dimensions. */
    push_float(state, shape, 2.9);
    push_float(state, shape, 1.9);
    push_int(state, data, 4);
    push_float(state, data, 5.5);
    EXPECT(ZrMath_TensorTotalSize(state, shape) == 2);
    EXPECT(ZrMath_TensorPopulate(state, tensor, shape, data));
    EXPECT(ZrMath_TensorGetStorage(state, tensor, &storage));
    EXPECT(storage.rank == 2 && storage.size == 2 && storage.shape == shape && storage.data == data);
    if (invalidStorage) {
        SZrTypeValue bad = float_value(1e300);
        /* Existing public rank/size fields feed the same support conversion. */
        ZrLib_Object_SetFieldCString(state, tensor, "rank", &bad);
        EXPECT(!ZrMath_TensorGetStorage(state, tensor, &storage));
        ZrMath_WriteIntField(state, tensor, "rank", 2);
        ZrLib_Object_SetFieldCString(state, tensor, "size", &bad);
        EXPECT(!ZrMath_TensorGetStorage(state, tensor, &storage));
    }
}
int main(int argc, char **argv) {
    SZrGlobalState *global = NULL;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    if (strcmp(argv[1], "int_controls") == 0) int_controls();
    else if (strcmp(argv[1], "int_finite_range") == 0) {
        rejected_float(1e300);
        rejected_float(-1e300);
    } else if (strcmp(argv[1], "int_nonfinite") == 0) {
        rejected_float(NAN);
        rejected_float(INFINITY);
        rejected_float(-INFINITY);
    } else if (strcmp(argv[1], "int_boundaries") == 0) int_boundaries();
    else {
        global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
        EXPECT(global != NULL && ZrCore_GlobalState_IsInitialized(global));
        if (global != NULL && ZrCore_GlobalState_IsInitialized(global)) {
            /* Fixture addresses remain borrowed for the short support calls. */
            global->garbageCollector->stopGcFlag = ZR_TRUE;
            if (strcmp(argv[1], "shape_product") == 0) shape_product(global->mainThreadState);
            else if (strcmp(argv[1], "array_conversion") == 0) array_conversion(global->mainThreadState);
            else if (strcmp(argv[1], "tensor_data") == 0) tensor_data(global->mainThreadState);
            else if (strcmp(argv[1], "tensor_controls") == 0) tensor_controls(global->mainThreadState, 0);
            else if (strcmp(argv[1], "storage_conversion") == 0) tensor_controls(global->mainThreadState, 1);
            else ++failures;
        }
        if (global != NULL) ZrCore_GlobalState_Free(global);
    }
    EXPECT(liveCount == 0 && !allocatorError);
    return failures == 0 ? 0 : 1;
}

/**
 * @file tests/library/test_math_input_boundaries.c
 *
 * ## Summary
 * 以真实 Math 支持 API 和实际 Core/Library 验证数值输入边界、Tensor 字段和普通销毁账目。每次 CTest 进程只运行一个既有 selector。
 *
 * ## Constraints and lifecycle
 * 保留小数向零截断策略；测试不自行执行超界浮点转整数或溢出的有符号乘法。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:1 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:21 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:173 — 本文件场景/调用/生命周期证据。
 * tests/cmake/math-input-boundary-tests.cmake:6 — 配套生产实现或实际构建入口。
 * tests/cmake/math-input-boundary-tests.cmake:24 — 配套生产实现或实际构建入口。
 * tests/cmake/math-input-boundary-tests.cmake:26 — 配套生产实现或实际构建入口。
 * tests/cmake/math-input-boundary-tests.cmake:28 — 配套生产实现或实际构建入口。
 */

/**
 * @struct Allocation
 *
 * ## Summary
 * 记录分配器当前持有块的地址、尺寸和内存类别；记录不是另一份内存所有权。
 *
 * ## Constraints and lifecycle
 * pointer 指向仍由 C realloc/free 管理的非空块；size、flag 必须对应下一次旧块操作。只有 allocations 的 [0, liveCount) 前缀有效；释放会把末项搬入空位，记录地址和下标不稳定。复制记录不会复制块或延长生命周期。
 *
 * ## Fields
 * pointer 是实际块地址，size 是 TZrSize（本项目为 size_t）的当前申请尺寸，flag 是 Core 传回的内存类别。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:17 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:18 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:29 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:30 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:36 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:45 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:46 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:47 — 本文件场景/调用/生命周期证据。
 */

/**
 * @var allocations
 *
 * ## Summary
 * 以固定容量账表保存单次测试进程中最多 32768 个活跃宿主块，供回调和最终清理守门共用。
 *
 * ## Constraints and lifecycle
 * 静态存储本身无需 free，尾部过期记录不表示活跃块。只有成功的新申请增加有效前缀；重分配成功更新原项；释放缩短前缀并补洞。新块 realloc 成功后才检查满表，满表会 abort，不能依赖 main 的正常清理。表和计数均无锁，使用前提是回调串行。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:18 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:19 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:20 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:36 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:39 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:42 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:43 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:206 — 本文件场景/调用/生命周期证据。
 */

/**
 * @var liveCount
 *
 * ## Summary
 * 界定分配账表的有效前缀，并在正常销毁之后确认本回调没有登记中的块。
 *
 * ## Constraints and lifecycle
 * 静态零初始化；新增成功块递增，释放匹配块递减并交换末项；自然 realloc 失败不改变计数。一个 selector 使用一个新进程，代码不提供重置或并发同步。最终为零只证明本账表已清空。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:19 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:18 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:36 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:40 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:43 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:206 — 本文件场景/调用/生命周期证据。
 */

/**
 * @var failures
 *
 * ## Summary
 * 累积 EXPECT 失败和未知 selector，再映射为 main 的正常退出码。
 *
 * ## Constraints and lifecycle
 * 进程启动时为零，断言失败递增，未知 selector 在 VM 已初始化时也递增；不在 helper 间清零。只支持本进程串行检查；无界重复失败可能超过 int，当前固定场景不把它当作无限计数器。异常展开或进程终止时不保证走到最终返回。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:20 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:21 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:181 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:202 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:207 — 本文件场景/调用/生命周期证据。
 */

/**
 * @var allocatorError
 *
 * ## Summary
 * 把分配器旧块协议不匹配记录为粘性错误，交给 main 最终检查。
 *
 * ## Constraints and lifecycle
 * 静态零初始化，未知旧指针、尺寸/类别不匹配，或 NULL 旧指针配非零旧尺寸时置 1，之后不复位。普通 realloc 返回 NULL 不置此标志；它既不是 OOM 次数，也不证明真实块已被释放。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:20 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:28 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:30 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:31 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:34 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:40 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:206 — 本文件场景/调用/生命周期证据。
 */

/**
 * @def EXPECT(c)
 *
 * ## Summary
 * 检查条件并保留后续诊断与普通清理；失败向 stderr 写原调用行和表达式，再增加 failures。
 *
 * ## Parameters
 * @param[in] c 作为条件求值一次的表达式；求值过程必须满足被调用 API 的约束。
 *
 * ## Returns
 * 无返回值；条件为假只写诊断并增加 failures，仍继续所属函数。
 *
 * ## Constraints and lifecycle
 * 条件只求值一次；#c 仅字符串化。do/while(0) 形成单条语句。宏不建立异常恢复点、不 return、不 abort，也不验证 fprintf 的结果。__LINE__ 是输出的一部分，注释只能追加在文件尾以保持调用行。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:21 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:20 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:61 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:96 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:193 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:206 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:207 — 本文件场景/调用/生命周期证据。
 */

/**
 * @fn static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize, TZrSize newSize, TZrInt64 flag)
 *
 * ## Summary
 * 作为 GlobalState_New 的宿主分配回调，在 realloc/free 周围核对旧块协议并维护活跃账目。
 *
 * ## Parameters
 * @param[in] data 用户数据，main 传 NULL；本实现不读取。
 * @param[in,out] old NULL 表示新申请；否则必须是仍在账表中的可重分配/释放块。
 * @param[in] oldSize old 的登记尺寸；old 为 NULL 时必须为零。
 * @param[in] newSize 非零为申请/重分配请求，零为释放。
 * @param[in] flag 旧块必须复用登记类别；新块成功时保存此类别。
 *
 * ## Returns
 * @return 非零请求成功时返回新块，由调用方以后经同一回调和类别释放；释放、协议拒绝或申请失败返回 NULL。协议拒绝不转移旧块，自然 realloc 失败保留旧块。
 *
 * ## Constraints and lifecycle
 * data 当前忽略。旧块检索必须在有效前缀内且尺寸/类别完全匹配，拒绝只置 allocatorError 并返回 NULL，不触碰旧块和账目。newSize 为零时 free 并交换删除；非零 realloc 失败保留旧块与记录。新块成功后才作容量检查，满表 abort 不保证清理。所有回调须串行，未实现锁、重入保护或故障注入。
 *
 * ## Protocol and release blocks
 * 先校验旧块，才允许释放或重分配。释放用末项填洞，之后该索引不再代表原记录。容量上限在 realloc 之后检查，因此满表是异常终止，不能视为已登记且可恢复的新申请。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:23 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:28 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:30 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:34 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:35 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:36 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:39 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:40 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:42 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:45 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:48 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:206 — 本文件场景/调用/生命周期证据。
 * zr_vm_common/include/zr_vm_common/zr_common_conf.h:182 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/runtime/global.c:370 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/runtime/global.c:374 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/runtime/global.c:734 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/runtime/global.c:778 — 配套生产实现或实际构建入口。
 */

/**
 * @fn static SZrTypeValue float_value(double input)
 *
 * ## Summary
 * 构造按值返回的 DOUBLE 标量夹具，保留 input 的原始 double 数值供支持层读取。
 *
 * ## Parameters
 * @param[in] input 待保存的 double，可为有限数、NaN 或正负 Infinity。
 *
 * ## Returns
 * @return 按值返回 DOUBLE 标签的标量；无堆资源移交。
 *
 * ## Constraints and lifecycle
 * 先清零整个 SZrTypeValue，再设置 DOUBLE 标签和 nativeDouble。函数不做范围检查、整数强转或 VM 分配；允许场景传入 NaN、Infinity 和超界有限数。该标量不拥有 GC 对象；不能把其他 union 成员当作已设值。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:51 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:53 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:54 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:55 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:59 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:65 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:105 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:164 — 本文件场景/调用/生命周期证据。
 */

/**
 * @fn static void rejected_float(double input)
 *
 * ## Summary
 * 验证一个所选 double 被 IntFromValue 拒绝，并且失败时保留输出哨兵。
 *
 * ## Parameters
 * @param[in] input 预期由支持层拒绝的 double；调用点提供超界、NaN 或 Infinity。
 *
 * ## Returns
 * 无返回值；两项检查累积到 failures，值和输出均为局部自动存储。
 *
 * ## Constraints and lifecycle
 * out 初始化为 12345；先检查返回 false，再单独检查 out 仍为 12345。EXPECT 失败也继续执行第二次检查。这里不执行输入到整数的强转，不创建 VM，也不检查异常对象或 threadStatus。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:58 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:59 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:60 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:61 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:62 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:184 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:187 — 本文件场景/调用/生命周期证据。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:135 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:159 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:160 — 配套生产实现或实际构建入口。
 */

/**
 * @fn static void accepted_float(double input, TZrInt64 expected)
 *
 * ## Summary
 * 验证一个合法 double 的转换成功，并核对调用方给定的精确整数结果。
 *
 * ## Parameters
 * @param[in] input 已选在 IntFromValue 接受区间内的 double。
 * @param[in] expected 生产转换向零截断后应等于的 TZrInt64 常量。
 *
 * ## Returns
 * 无返回值；状态或结果不符只累积 failures。
 *
 * ## Constraints and lifecycle
 * 预期结果由常量提供，不在测试中对超界浮点数做整数强转。两次 EXPECT 分别检查状态与结果；成功数的截断由生产支持层完成。无 VM、堆分配或异常恢复点。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:64 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:65 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:66 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:67 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:68 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:73 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:89 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:90 — 本文件场景/调用/生命周期证据。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:158 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:159 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:162 — 配套生产实现或实际构建入口。
 */

/**
 * @fn static void int_controls(void)
 *
 * ## Summary
 * 为转换拒绝场景提供成功对照：四个截断样本、两个 int64 极值和 BOOL 标签拒绝。
 *
 * ## Returns
 * 无返回值；失败累积到 failures，所有值为栈上标量。
 *
 * ## Constraints and lifecycle
 * 7.9、-7.9、-0.75、0.0 分别期望 7、-7、0、0；INT64_MAX/MIN 直接以整数标签读取；随后仅改标签为 BOOL，输出重置为 12345 并断言拒绝且不改写。未覆盖 UINT64 大值、所有标签或 NULL 参数。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:70 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:73 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:74 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:75 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:76 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:78 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:80 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:82 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:83 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:85 — 本文件场景/调用/生命周期证据。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:140 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:145 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:165 — 配套生产实现或实际构建入口。
 */

/**
 * @fn static void int_boundaries(void)
 *
 * ## Summary
 * 检查二进制浮点表示下 int64 转换半开区间的所选四个边界常量。
 *
 * ## Returns
 * 无返回值；通过 accepted_float/rejected_float 累积状态、结果和哨兵检查。
 *
 * ## Constraints and lifecycle
 * 接受精确 -2^63 和 +2^63-1024；后者对应 INT64_MAX-1023。拒绝 +2^63 和 -2^63-2048。不能用舍入成 +2^63 的 double(INT64_MAX) 作包含上界。测试只保存常量并调用支持层，不自行执行越界强转或有符号乘法。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:87 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:88 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:89 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:90 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:91 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:92 — 本文件场景/调用/生命周期证据。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:156 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:158 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:159 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:162 — 配套生产实现或实际构建入口。
 */

/**
 * @fn static SZrObject *new_array(SZrState *state)
 *
 * ## Summary
 * 为单个 VM 场景取得真实 Library 数组，并把 NULL 返回记为测试失败。
 *
 * ## Parameters
 * @param[in,out] state 活动 VM 的主 state，调用期间有效；当前调用点均在初始化成功之后。
 *
 * ## Returns
 * @return 新数组的借用指针，最终由 GlobalState_Free 收尾；普通失败可为 NULL，已同时记入 failures。
 *
 * ## Constraints and lifecycle
 * state 借用当前已初始化 global 的主 state。取得的对象由该 VM 管理，helper 不增加持久根；调用方窗口设置 stopGcFlag，但它不提供持久根或强制 GC/OOM 安全保证。NULL 失败仅记录并返回，调用方显式短路；底层分配也可能抛出而不能普通返回。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:94 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:95 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:96 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:97 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:118 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:127 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:135 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:143 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:153 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:196 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:204 — 本文件场景/调用/生命周期证据。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1374 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1379 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1381 — 配套生产实现或实际构建入口。
 */

/**
 * @fn static void push_int(SZrState *state, SZrObject *array, TZrInt64 input)
 *
 * ## Summary
 * 将一个原生整数标量追加到真实数组，并检查 Library 返回的布尔状态。
 *
 * ## Parameters
 * @param[in,out] state 数组所属活动 state，可能更新 VM 状态。
 * @param[in,out] array 本次场景的数组借用指针，成功时内容增长。
 * @param[in] input 直接保存的 TZrInt64 数据，不进行有符号乘法。
 *
 * ## Returns
 * 无返回值；追加失败通过 failures 表示，helper 不提供回滚或异常恢复。
 *
 * ## Constraints and lifecycle
 * state 和 array 借用同一活动 VM，array 须为当前场景取得的数组。SetInt 构造标签值，Array_PushValue 复制到数组；栈上 value 不交给调用方释放。EXPECT false 只累积失败；底层非局部异常不由本 helper 捕获。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:99 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:101 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:102 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:119 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:136 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:157 — 本文件场景/调用/生命周期证据。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1301 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1474 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1486 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1490 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1502 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/value/value.c:79 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/value/value.c:82 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/value/value.c:83 — 配套生产实现或实际构建入口。
 */

/**
 * @fn static void push_float(SZrState *state, SZrObject *array, double input)
 *
 * ## Summary
 * 把 DOUBLE 标量追加到数组，既用于超界负例，也保留小数维度的现有截断策略。
 *
 * ## Parameters
 * @param[in,out] state 数组所属的活动 state。
 * @param[in,out] array 要追加的当前场景数组，借用至调用结束及场景后续读取。
 * @param[in] input 原样写入 DOUBLE 标量的数值；当前包括有限超界数和小数。
 *
 * ## Returns
 * 无返回值；追加布尔失败累积到 failures。
 *
 * ## Constraints and lifecycle
 * input 原样交给 float_value，不在此验证有限性或转为整数。数组与 state 同 VM，栈上 value 仅供追加调用借用；普通失败被 EXPECT 记录，非局部异常不捕获。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:104 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:105 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:106 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:128 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:155 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:156 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:158 — 本文件场景/调用/生命周期证据。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1474 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1495 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1499 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1502 — 配套生产实现或实际构建入口。
 */

/**
 * @fn static SZrObject *new_tensor_shell(SZrState *state)
 *
 * ## Summary
 * 建立供 Tensor 支持函数读写字段的普通 Object 外壳，避免把场景夸大为完整 Tensor 类型构造。
 *
 * ## Parameters
 * @param[in,out] state 初始化成功的当前 VM 主 state，供对象分配和 map 初始化。
 *
 * ## Returns
 * @return 已调用 Object_Init 的普通 Object 借用指针，或 Object_New 返回并已记失败的 NULL；不单独验证 nodeMap.isValid；普通路径最终由 GlobalState_Free 收尾。
 *
 * ## Constraints and lifecycle
 * Object_New 使用 NULL 元数据；非空后再 Object_Init。它没有通过 ZrLib_Type_NewInstance 注册 Tensor 类。返回对象归 VM 管理，helper 不单独 root 或 free；普通 NULL 已记失败，初始化分配异常不在此捕获。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:108 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:109 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:110 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:111 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:112 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:204 — 本文件场景/调用/生命周期证据。
 * zr_vm_core/src/zr_vm_core/object/object.c:1449 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/object/object.c:1496 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:468 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:469 — 配套生产实现或实际构建入口。
 */

/**
 * @fn static void shape_product(SZrState *state)
 *
 * ## Summary
 * 用 shape=[INT64_MAX,2] 与空 data 检查乘法溢出前拒绝，并确认 Populate 拒绝该形状。
 *
 * ## Parameters
 * @param[in,out] state 当前初始化且场景存续的 VM 主 state。
 *
 * ## Returns
 * 无返回值；分配或检查失败累积到 failures，普通路径的对象由 main 统一销毁。
 *
 * ## Constraints and lifecycle
 * 先取得两个数组和对象外壳，任一普通 NULL 就返回，先前对象留给 VM 销毁。只追加整数常量；测试自身不计算 INT64_MAX*2。TensorTotalSize 预期 -1，Populate 预期 false；两个条件各自检查，不验证异常诊断或写入回滚。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:114 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:115 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:116 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:117 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:118 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:119 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:120 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:121 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:122 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:204 — 本文件场景/调用/生命周期证据。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:380 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:385 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:388 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:441 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:442 — 配套生产实现或实际构建入口。
 */

/**
 * @fn static void array_conversion(SZrState *state)
 *
 * ## Summary
 * 通过真实数组读取 1e300，检查整数转换拒绝保留输出，并使该维度的总大小无效。
 *
 * ## Parameters
 * @param[in,out] state 当前场景的活动 VM 主 state，拥有 shape 数组。
 *
 * ## Returns
 * 无返回值；失败累积到 failures，数组普通路径由 main 销毁。
 *
 * ## Constraints and lifecycle
 * shape 取得后 NULL 则短路；成功追加的仍是 DOUBLE。ArrayReadInt false 与 out==12345 在同一短路表达式中检查；TotalSize 单独期望 -1。没有直接越界强转、负索引或空数组场景。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:124 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:125 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:126 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:127 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:128 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:129 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:130 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:204 — 本文件场景/调用/生命周期证据。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:362 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:363 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:380 — 配套生产实现或实际构建入口。
 */

/**
 * @fn static void tensor_data(SZrState *state)
 *
 * ## Summary
 * 对 shape=[1] 分别构造字符串、BOOL 和 NULL 数据，检查真实 Populate 拒绝非数值标签。
 *
 * ## Parameters
 * @param[in,out] state 当前活动主 state，用于字符串、数组、对象及字段支持函数。
 *
 * ## Returns
 * 无返回值；普通 NULL 提前返回，其他失败累积；所有成功分配对象在正常 main 清理中交回 VM。
 *
 * ## Constraints and lifecycle
 * 三个 invalid 值由真实 Value API 构造；每次循环各建独立 data 和外壳，先检查 push 再检查 Populate false，避免前一项污染下一外壳。字符串对象和数组都归 VM；局部值/裸指针不等于持久 GC 根。SetString 可能因分配失败抛出，helper 不建立恢复点。生产数值 NaN/Inf 仍可作 Tensor data，本场景不声称拒绝它们。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:132 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:133 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:134 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:135 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:136 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:137 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:138 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:139 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:140 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:141 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:142 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:143 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:144 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:145 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:204 — 本文件场景/调用/生命周期证据。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1315 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1320 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1322 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:445 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:446 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:449 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/value/value.c:46 — 配套生产实现或实际构建入口。
 * zr_vm_core/include/zr_vm_core/value.h:90 — 配套生产实现或实际构建入口。
 */

/**
 * @fn static void tensor_controls(SZrState *state, int invalidStorage)
 *
 * ## Summary
 * 检查小数 shape 的现有截断策略和成功字段读取，并按 selector 变异公开 rank/size 字段验证拒绝。
 *
 * ## Parameters
 * @param[in,out] state 当前活动 VM 主 state，拥有全部场景对象。
 * @param[in] invalidStorage 零只跑成功对照；非零继续变异 rank 和 size。当前两个 selector 分别传 0 和 1。
 *
 * ## Returns
 * 无返回值；EXPECT 累积失败；对象由 main 清理，storage 中指针仅借用同一 VM 对象。
 *
 * ## Constraints and lifecycle
 * 基线保留 shape=[2.9,1.9]，整数维度按向零截断得到 [2,1]，data=[4,5.5]；断言 total=2、Populate true、GetStorage true，以及 rank/size 和原数组身份。GetStorage 自己先清零输出，失败后下一条字段断言不会读取未初始化的 storage。invalidStorage 非零才把 rank 改成 1e300，拒绝后恢复 rank=2，再把 size 改成 1e300 并拒绝；size 不恢复，随 VM 销毁。void 字段 setter 可抛出，不能把无返回值写成可恢复软失败。
 *
 * ## Fractional baseline block
 * 浮点 shape 原数组不改写成整数；total、rank、size 使用支持层转换结果。这里只检查所选两维、两项数据和指针身份。
 *
 * ## Public storage mutation block
 * rank 拒绝后先恢复，再测试 size，隔离两个转换入口；最后不恢复 size。GetStorage 只作字段类型/整数转换检查，不保证任意脚本修改后四字段持续一致。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:148 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:149 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:150 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:151 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:152 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:153 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:154 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:155 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:156 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:157 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:158 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:159 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:160 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:161 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:162 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:163 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:166 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:167 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:168 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:169 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:170 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:201 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:204 — 本文件场景/调用/生命周期证据。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:383 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:405 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:416 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:423 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:428 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:459 — 配套生产实现或实际构建入口。
 * zr_vm_lib_math/src/zr_vm_lib_math/common.c:463 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c:12 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c:14 — 配套生产实现或实际构建入口。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c:15 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/object/object_write.c:132 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/object/object_write.c:145 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/object/object_write.c:151 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/object/object_write.c:156 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/object/object_write.c:186 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/object/object_write.c:192 — 配套生产实现或实际构建入口。
 */

/**
 * @fn int main(int argc, char **argv)
 *
 * ## Summary
 * 作为真实 CTest 可执行入口，每进程只选择一个已有场景，统一汇总失败并在普通路径销毁 VM。
 *
 * ## Parameters
 * @param[in] argc 按 C 入口约定给出的参数数目；当前注册要求恰好 2。
 * @param[in] argv C 运行库提供的参数数组；argc==2 时 argv[1] 是以 NUL 结束的 selector，当前九个名字由 CMake 注册。
 *
 * ## Returns
 * @return 参数数目不符为 2；到达普通尾部且 failures 为零为 0，否则为 1。abort、未捕获异常、系统错误或超时不保证这些返回码。
 *
 * ## Constraints and lifecycle
 * Windows 先设置进程/CRT 错误报告方式，不恢复这些设置。argc!=2 直接返回 2；四个纯值 selector 不创建 VM，其余创建真实 global。第三实参 1 是 uniqueNumber，不是线程数。仅初始化成功后设置 stopGcFlag 并借用主 state 运行五个 VM 场景；未知 selector 在该分支计失败。非空 global 在普通路径均 Free；最后只检查本分配账表清空且无协议错误。stopGcFlag 不能证明强制 GC/OOM 安全，栈上裸指针不构成持久根；自然 OOM 可能走强制 GC/Throw，程序未为场景装 TryRun。
 *
 * ## Process and fixture lifetime
 * 每个 selector 由独立进程使用静态零初始化状态，函数未提供进程内重复运行或并发隔离。本文件不创建目录/文件夹具，不运行 ZR 脚本，不注册 Unity/CMocka case。
 *
 * ## Windows reporting block
 * SetErrorMode 和 CRT assert 输出设置是进程期副作用；只影响 Windows 条件分支，不由此推导其他平台行为。
 *
 * ## References
 * tests/library/test_math_input_boundaries.c:173 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:175 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:176 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:177 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:178 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:179 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:181 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:182 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:183 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:186 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:190 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:191 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:192 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:193 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:194 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:196 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:197 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:198 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:199 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:200 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:201 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:202 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:204 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:206 — 本文件场景/调用/生命周期证据。
 * tests/library/test_math_input_boundaries.c:207 — 本文件场景/调用/生命周期证据。
 * zr_vm_core/include/zr_vm_core/global.h:174 — 配套生产实现或实际构建入口。
 * zr_vm_core/include/zr_vm_core/global.h:175 — 配套生产实现或实际构建入口。
 * zr_vm_core/include/zr_vm_core/global.h:214 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/runtime/global.c:360 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/runtime/global.c:374 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/runtime/global.c:499 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/runtime/global.c:502 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/runtime/global.c:733 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/runtime/global.c:758 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/runtime/global.c:778 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/base/memory.c:17 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/base/memory.c:46 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/gc/gc_full.c:102 — 配套生产实现或实际构建入口。
 * zr_vm_core/src/zr_vm_core/exception/exception.c:85 — 配套生产实现或实际构建入口。
 * tests/cmake/math-input-boundary-tests.cmake:24 — 配套生产实现或实际构建入口。
 * tests/cmake/math-input-boundary-tests.cmake:25 — 配套生产实现或实际构建入口。
 * tests/cmake/math-input-boundary-tests.cmake:28 — 配套生产实现或实际构建入口。
 */
