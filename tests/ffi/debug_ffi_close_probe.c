#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_parser.h"

/**
 * ## Summary
 * 为独立命令行探针创建 VM 状态，并在 global 存在时登记注入源所需的 parser、math、system、container、FFI provider。
 * ## Parameters
 * 无参数；使用测试 runtime 支持提供的默认状态创建入口。
 * ## Returns
 * @return 状态创建结果；仅空 state 对应创建失败，global 为空不是本函数额外的空返回条件。
 * ## Constraints and lifecycle
 * 登记借用该状态的 global，不转移状态所有权；global 为空时跳过登记并原样返回非空 state。所有非空返回值由 main 在退出前销毁。
 * ## References
 * tests/ffi/debug_ffi_close_probe.c:32-52；tests/ffi/debug_ffi_close_probe.c:682-691；tests/harness/runtime_support.c:385
 */
/* 探针独立于 Unity 测试运行，状态需自行注册动态 FFI 所依赖的 provider。 */
static SZrState *create_probe_state(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);

    /**
     * ## Summary
     * 让同一状态的解析和执行能解析各模式 import 的 provider。
     * ## Constraints and lifecycle
     * 仅在 state/global 均存在时登记；当前调用顺序不等于已证明的依赖顺序。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:43-49
     */
    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
        ZrVmLibMath_Register(state->global);
        ZrVmLibSystem_Register(state->global);
        ZrVmLibContainer_Register(state->global);
        ZrVmLibFfi_Register(state->global);
    }

    return state;
}

/**
 * ## Summary
 * 把选定模式的借用 ZR 文本编译为当前状态中的一次性函数，供后续捕获执行状态。
 * ## Parameters
 * @param[in] state 非空 VM 状态，调用期间借用。
 * @param[in] source 所选 ZR 源文本，长度以 strlen 取得。
 * @param[in] sourceNameText 编译诊断使用的源名文本。
 * ## Returns
 * @return 编译函数，调用方用 ZrCore_Function_Free 释放；空输入、源名创建或编译失败时为空。
 * ## Constraints and lifecycle
 * source 和 sourceNameText 必须在调用期间为可读 NUL 结尾文本；返回函数先于状态销毁释放，sourceName 对象由 VM/编译器管理，不作为宿主 malloc 缓冲区释放。
 * ## References
 * tests/ffi/debug_ffi_close_probe.c:68-89；tests/ffi/debug_ffi_close_probe.c:688-693；tests/ffi/debug_ffi_close_probe.c:746-747
 */
static SZrFunction *compile_source(SZrState *state, const char *source, const char *sourceNameText) {
    SZrString *sourceName;

    /**
     * ## Summary
     * 在交给编译器之前确认借用输入可用并建立诊断源名。
     * ## Constraints and lifecycle
     * 空参数/源名创建失败直接拒绝，strlen 仍要求可读 NUL 文本。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:79-88
     */
    if (state == ZR_NULL || source == ZR_NULL || sourceNameText == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_CreateFromNative(state, (TZrNativeString) sourceNameText);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

/**
 * ## Summary
 * 从执行失败时的字符串错误槽借出 native 文本，供宿主在清理 VM 线程之前复制。
 * ## Parameters
 * @param[in] state 持有该值的 VM 状态。
 * @param[in] value 待读错误值；须有非空字符串对象。
 * ## Returns
 * @return 字符串内部只读指针；状态/值为空、类型不符或对象为空时为空。
 * ## Constraints and lifecycle
 * 仅借用状态和字符串对象；不得释放返回指针，也不得跨 ResetThread 或状态销毁保存。非字符串错误不由本函数格式化。
 * ## References
 * tests/ffi/debug_ffi_close_probe.c:104-110；tests/ffi/debug_ffi_close_probe.c:243-258
 */
static const char *string_value_native(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object));
}

/**
 * ## Summary
 * 外层执行器与同步 TryRun 回调之间的单次交接记录；同时表达函数借用、结果栈锚和调用完成状态。
 * ## Constraints and lifecycle
 * 外层栈分配并清零，只在该次 TryRun 和紧随其后的结果复制阶段存活。不得异步保存；resultBase 不取得 VM 栈所有权。
 * function 是待调用函数借用；resultBase 是结果槽锚，失败时可为空；callCompleted 仅反映回调调用后的线程状态。
 * ## References
 * tests/ffi/debug_ffi_close_probe.c:122-129；tests/ffi/debug_ffi_close_probe.c:188-189；tests/ffi/debug_ffi_close_probe.c:214-232
 */
/* TryRun 回调只传回栈锚结果，错误详情由外层在重置线程前提取。 */
typedef struct ZrProbeExecuteCaptureRequest {
    /** 借用待执行函数；TryRun 期间有效，记录不负责释放。 */
    SZrFunction *function;
    /** 借用调用结果的 VM 栈槽锚；未完成或失败时可为空，外层仅立即复制。 */
    TZrStackValuePointer resultBase;
    /** 记录 CallAndRestoreAnchor 返回后线程是否为 FINE；须与 TryRun 状态和结果锚共同判断。 */
    TZrBool callCompleted;
} ZrProbeExecuteCaptureRequest;

/**
 * ## Summary
 * 在 TryRun 的同步调用边界中把函数包装为临时 closure，建立可恢复的栈锚并执行一次，向 request 回传结果位置和完成状态。
 * ## Parameters
 * @param[in] state TryRun 提供的当前 VM 状态。
 * @param[in,out] arguments 指向同步存活的 ZrProbeExecuteCaptureRequest，写结果锚及完成标志。
 * ## Returns
 * @return 无；空输入或 closure 创建失败可正常返回但不标记完成，外层必须检查 request。
 * ## Constraints and lifecycle
 * arguments 只在同步回调内借用。临时 ignore 在栈准备和 callee 值发布期间保留，成功登记的 ignore 在值入栈后解除。CheckStackAndAnchor 在栈发布及 unignore 之前；不得据名字声称它一定触发 GC。
 * ## References
 * tests/ffi/debug_ffi_close_probe.c:144-190；tests/ffi/debug_ffi_close_probe.c:226；zr_vm_core/src/zr_vm_core/function/function.c:1847-1860
 */
static void probe_execute_capture_body(SZrState *state, TZrPtr arguments) {
    ZrProbeExecuteCaptureRequest *request = (ZrProbeExecuteCaptureRequest *)arguments;
    SZrClosure *closure;
    TZrBool ignoredClosure = ZR_FALSE;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor baseAnchor;
    SZrTypeValue *closureValue;

    if (state == ZR_NULL || request == ZR_NULL || request->function == ZR_NULL) {
        return;
    }

    /**
     * ## Summary
     * 从临时 closure 保护过渡到已发布的 VM 栈 callee，再进入可恢复锚的调用。
     * ## Constraints and lifecycle
     * ignore 成功时跨栈准备保留至 callee 发布；结果槽随后只给外层立即读取，不是异步存储。
     * TODO: 对照共享 runtime helper，核查 Closure_New 分配前 function 的 GC 根，以及 IgnoreObject 失败或栈准备抛错时的临时保活与解除；当前零捕获 InitValue 不产生捕获单元分配。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:165-189；tests/harness/runtime_support.c:328；tests/harness/runtime_support.c:347；tests/ffi/test_ffi_module.c:376；zr_vm_core/src/zr_vm_core/closure/closure.c:270；zr_vm_core/src/zr_vm_core/closure/closure.c:285；zr_vm_core/src/zr_vm_core/gc/gc.c:696
     */
    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        return;
    }

    ignoredClosure = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closure->function = request->function;
    ZrCore_Closure_InitValue(state, closure);

    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, request->function->stackSize + 1, base, base, &baseAnchor);

    closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

    if (ignoredClosure) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    }

    request->resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &baseAnchor, 1);
    request->callCompleted = (TZrBool)(state->threadStatus == ZR_THREAD_STATUS_FINE);
}

/**
 * ## Summary
 * 同步调用编译函数，把 TryRun 的异常边界、回调完成条件和线程状态合并为宿主可报告的执行结果。
 * ## Parameters
 * @param[in] state 当前状态，调用期间借用。
 * @param[in] function 待执行函数，调用方保留释放责任。
 * @param[in,out] result 可选、已初始化的值槽；成功时复制结果，失败不写。须为有效、已初始化的值槽；全新输出槽可先 ResetAsNull。当前 main 未满足此条件，见 BUG。
 * @param[out] errorBuffer 可选宿主缓冲区；有容量时先清空，再复制可取得的字符串错误。
 * @param[in] errorBufferSize 缓冲区总容量；为零时不写。
 * ## Returns
 * @return 三项成功条件同时满足才返回 FINE；异常或非 FINE 线程状态先 ResetThread 再返回原状态；正常返回但没有完成/锚时返回 synthetic RUNTIME_ERROR。
 * ## Constraints and lifecycle
 * 错误文本须先复制再 ResetThread；result 和 errorBuffer 属于调用方。本探针现有模板返回标量，不把 TryRun 对调用的保护扩大为任意对象结果复制的保护。
 * ## References
 * tests/ffi/debug_ffi_close_probe.c:209-272；tests/ffi/debug_ffi_close_probe.c:620；tests/ffi/debug_ffi_close_probe.c:714；zr_vm_core/include/zr_vm_core/value.h:147-164；zr_vm_core/include/zr_vm_core/value.h:218-236
 */
/* 失败模式需要可打印的 VM 错误，同时让宿主进程以约定退出码返回。 */
static EZrThreadStatus execute_function_capture_status(SZrState *state,
                                                       SZrFunction *function,
                                                       SZrTypeValue *result,
                                                       char *errorBuffer,
                                                       size_t errorBufferSize) {
    ZrProbeExecuteCaptureRequest request;
    EZrThreadStatus status;

    if (errorBuffer != NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }
    if (state == ZR_NULL || function == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    memset(&request, 0, sizeof(request));
    request.function = function;
    status = ZrCore_Exception_TryRun(state, probe_execute_capture_body, &request);

    if (status == ZR_THREAD_STATUS_FINE && request.callCompleted && request.resultBase != ZR_NULL) {
        if (result != ZR_NULL) {
            ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(request.resultBase));
        }
        return ZR_THREAD_STATUS_FINE;
    }

    /**
     * ## Summary
     * 把 VM 借用错误文本复制到宿主缓冲区后再清理线程，使诊断不依赖失效栈槽。
     * ## Constraints and lifecycle
     * ResetThread 可能执行额外清理；当前返回保存的原状态，而非给出清理后的独立状态保证。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:243-259
     */
    if (errorBuffer != NULL && errorBufferSize > 0 && state->stackTop.valuePointer > state->stackBase.valuePointer) {
        const SZrTypeValue *errorValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer - 1);
        const char *nativeError = string_value_native(state, errorValue);
        if (nativeError != ZR_NULL) {
            strncpy(errorBuffer, nativeError, errorBufferSize - 1);
            errorBuffer[errorBufferSize - 1] = '\0';
        }
    }

    if (status != ZR_THREAD_STATUS_FINE) {
        ZrCore_State_ResetThread(state, status);
        return status;
    }
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        status = state->threadStatus;
        ZrCore_State_ResetThread(state, status);
        return status;
    }

    /**
     * ## Summary
     * 区分 TryRun 正常返回与确有完成结果，避免仅凭 FINE 当成执行成功。
     * ## Constraints and lifecycle
     * 该分支当前不 ResetThread，也没有新的字符串错误保证。
     * TODO: 核查 closure 创建失败导致正常回调早退时，synthetic RUNTIME_ERROR 应如何恢复线程和报告原因；对照回调早退及两个已重置失败分支。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:271-271
     */
    return ZR_THREAD_STATUS_RUNTIME_ERROR;
}

/**
 * ## Summary
 * 把宿主动态库路径中的反斜杠和双引号转义成适用于本探针 ZR 双引号字面量的前缀。
 * ## Parameters
 * @param[out] destination 借用输出缓冲区；为空时不写。
 * @param[in] destinationSize 缓冲区总容量；为零时不写。
 * @param[in] source 借用 NUL 结尾路径；为空时输出空串。
 * ## Returns
 * @return 无；不报告是否发生截断。
 * ## Constraints and lifecycle
 * destination 为调用方可写缓冲区；有容量时保留 NUL 空间，容量不足静默截断。仅处理两类字符，不是处理任意控制字符的通用编码器。
 * TODO: 核查超长路径以及换行等未转义字符经 snprintf 后的源有效性和诊断策略；两个调用点当前均未检查 snprintf 返回值。
 * ## References
 * tests/ffi/debug_ffi_close_probe.c:289-321；tests/ffi/debug_ffi_close_probe.c:661-670
 */
static void escape_for_zr_string_literal(char *destination, size_t destinationSize, const char *source) {
    size_t writeIndex = 0;
    size_t readIndex = 0;

    if (destination == NULL || destinationSize == 0) {
        return;
    }

    destination[0] = '\0';
    if (source == NULL) {
        return;
    }

    /**
     * ## Summary
     * 为转义字符和末尾 NUL 保留容量，提供不会越过宿主输出缓冲区的前缀。
     * ## Constraints and lifecycle
     * 空间不够即停止，但不向调用方报告截断；此边界不能替代 snprintf 的长度检查。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:310-320
     */
    while (source[readIndex] != '\0' && writeIndex + 1 < destinationSize) {
        if (source[readIndex] == '\\' || source[readIndex] == '"') {
            if (writeIndex + 2 >= destinationSize) {
                break;
            }
            destination[writeIndex++] = '\\';
        }
        destination[writeIndex++] = source[readIndex++];
    }

    destination[writeIndex] = '\0';
}

/**
 * ## Summary
 * 为手工/GDB 诊断选择最小 FFI 或无库基线场景，创建状态、编译并执行源，打印观察结果后释放函数与状态。
 * ## Parameters
 * @param[in] argc argv 中可读项数。
 * @param[in] argv 可选 mode 及动态库路径；需要库的模式必须提供第二个参数。
 * ## Returns
 * @return 普通执行 FINE 后返回0，但未断言结果值；load-failure 任意非 FINE 也返回0，意外 FINE 返回5。缺路径/创建/编译/普通执行失败分别返回1/2/3/4。
 * ## Constraints and lifecycle
 * 命令行及源模板只在本次同步入口借用；函数释放先于状态销毁。未知 mode 保持默认 all-no-lib，包含 --verbose。成功退出不代表结果值或每种 close 行为已有断言。正常路径 result 初始化缺失另见 BUG。
 * ## References
 * tests/ffi/debug_ffi_close_probe.c:337-749；tests/CMakeLists.txt:6574-6589；tests/ffi/gdb_zr_vm_ffi_close_crash.gdb:1-6；tests/ffi/gdb_ffi_load_failure_probe.gdb:1-6
 */
/* 命令行 mode 选择最小复现场景；涉及动态库的 mode 还要求第二个参数为 fixture 路径。 */
int main(int argc, char **argv) {
    /**
     * ## Summary
     * 观察 LibraryHandle 从打开到两次 logical close 的状态；源把三态组合为整数。
     * ## Constraints and lifecycle
     * %s 只由已转义路径替换；不含 SymbolHandle 延迟卸载场景；宿主未断言返回1。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:346-354；tests/ffi/debug_ffi_close_probe.c:648-662；tests/ffi/debug_ffi_close_probe.c:346-354；zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c:100-123
     */
    static const char *kLibraryTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var beforeClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterFirstClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterSecondClose = lib.isClosed();\n"
            "return (!beforeClose && afterFirstClose && afterSecondClose) ? 1 : 0;\n";
    /**
     * ## Summary
     * 用 bool 返回同一 library close 三态条件，区分整数与布尔输出形状。
     * ## Constraints and lifecycle
     * 路径模板借用 escapedPath；打印 false 仍可退出0，结果判据另见 TODO。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:363-371；tests/ffi/debug_ffi_close_probe.c:664-671；tests/ffi/debug_ffi_close_probe.c:363-371；tests/ffi/debug_ffi_close_probe.c:735-748
     */
    static const char *kLibraryBoolTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var beforeClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterFirstClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterSecondClose = lib.isClosed();\n"
            "return !beforeClose && afterFirstClose && afterSecondClose;\n";
    /**
     * ## Summary
     * 把 i32、f64、UTF-8 参数的三个动态符号调用放在同一诊断源中观察。
     * ## Constraints and lifecycle
     * 需匹配 fixture 导出/签名；strlen fixture 返回 size_t，源声明 u64 的平台宽度疑点沿已有 fixture TODO；本入口只打印结果。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:380-395；tests/ffi/debug_ffi_close_probe.c:650-662；tests/ffi/ffi_fixture.c:249；tests/ffi/ffi_fixture.c:305；tests/ffi/ffi_fixture.c:324；tests/ffi/ffi_fixture.c:691-709
     */
    static const char *kPrimitiveTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var add = lib.getSymbol(\"zr_ffi_add_i32\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [{ type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "var mul = lib.getSymbol(\"zr_ffi_mul_f64\", {\n"
            "  returnType: \"f64\",\n"
            "  parameters: [{ type: \"f64\" }, { type: \"f64\" }]\n"
            "});\n"
            "var strlenUtf8 = lib.getSymbol(\"zr_ffi_strlen_utf8\", {\n"
            "  returnType: \"u64\",\n"
            "  parameters: [{ type: { kind: \"string\", encoding: \"utf8\" } }]\n"
            "});\n"
            "return add.call([7, 5]) + mul.call([2.0, 4.0]) + strlenUtf8.call([\"hello\"]);\n";
    /**
     * ## Summary
     * 隔离 i32 加法符号调用，缩小动态调用故障的观察范围。
     * ## Constraints and lifecycle
     * 需可加载的 fixture 路径和匹配的 i32 参数/返回描述；宿主不判定数值。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:404-411；tests/ffi/debug_ffi_close_probe.c:652-662；tests/ffi/ffi_fixture.c:249；tests/ffi/ffi_fixture.c:691-693
     */
    static const char *kPrimitiveAddTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var add = lib.getSymbol(\"zr_ffi_add_i32\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [{ type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "return add.call([7, 5]);\n";
    /**
     * ## Summary
     * 隔离 f64 乘法符号调用，观察与整数调用不同的值形状。
     * ## Constraints and lifecycle
     * 需匹配 fixture 的 double 参数/返回；不据模板声明推断跨平台 ABI 验收。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:420-427；tests/ffi/debug_ffi_close_probe.c:654-662；tests/ffi/ffi_fixture.c:305；tests/ffi/ffi_fixture.c:703-705
     */
    static const char *kPrimitiveMulTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var mul = lib.getSymbol(\"zr_ffi_mul_f64\", {\n"
            "  returnType: \"f64\",\n"
            "  parameters: [{ type: \"f64\" }, { type: \"f64\" }]\n"
            "});\n"
            "return mul.call([2.0, 4.0]);\n";
    /**
     * ## Summary
     * 隔离 UTF-8 字符串参数到 native 长度返回的动态调用。
     * ## Constraints and lifecycle
     * 源声明 u64，但 fixture 的返回是 size_t；宽度限制和 TODO 在 fixture 契约中保留。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:436-443；tests/ffi/debug_ffi_close_probe.c:656-662；tests/ffi/ffi_fixture.c:324；tests/ffi/ffi_fixture.c:707-709
     */
    static const char *kPrimitiveStrlenTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var strlenUtf8 = lib.getSymbol(\"zr_ffi_strlen_utf8\", {\n"
            "  returnType: \"u64\",\n"
            "  parameters: [{ type: { kind: \"string\", encoding: \"utf8\" } }]\n"
            "});\n"
            "return strlenUtf8.call([\"hello\"]);\n";
    /**
     * ## Summary
     * 单独观察显式 stdcall 描述与 fixture 加法导出的结合。
     * ## Constraints and lifecycle
     * 是否支持该 ABI 由当前平台/构建决定；本探针不承诺跨平台可执行。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:452-460；tests/ffi/debug_ffi_close_probe.c:658-662；tests/ffi/ffi_fixture.c:436；tests/ffi/ffi_fixture.c:753-755
     */
    static const char *kPrimitiveStdcallTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var add = lib.getSymbol(\"zr_ffi_stdcall_add_i32\", {\n"
            "  returnType: \"i32\",\n"
            "  abi: \"stdcall\",\n"
            "  parameters: [{ type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "return add.call([4, 9]);\n";
    /**
     * ## Summary
     * 无动态库数值基线，用于区分 VM 数值执行/输出与动态加载故障。
     * ## Constraints and lifecycle
     * 固定源直接编译；不需要 libraryPath，结果仍只是打印。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:469-470；tests/ffi/debug_ffi_close_probe.c:638-639；tests/ffi/debug_ffi_close_probe.c:469-470
     */
    static const char *kPrimitiveNumericFoldSource =
            "return 12 + 8.0 + 5;\n";
    /**
     * ## Summary
     * 无动态库条件表达式基线，用于缩小 parser/执行链的诊断范围。
     * ## Constraints and lifecycle
     * 直接编译一次；不提供 FFI close 或 native ABI 覆盖。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:479-480；tests/ffi/debug_ffi_close_probe.c:634-635；tests/ffi/debug_ffi_close_probe.c:479-480
     */
    static const char *kTernarySource =
            "return true ? 1 : 0;\n";
    /**
     * ## Summary
     * 用预期缺失库名触发错误捕获和宿主退出码路径。
     * ## Constraints and lifecycle
     * 名称预期缺失，不保证目标系统永远不存在同名库；非 FINE 返回不证明失败类别为加载错误，另见 TODO。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:489-492；tests/ffi/debug_ffi_close_probe.c:636-637；tests/ffi/debug_ffi_close_probe.c:489-492；tests/ffi/debug_ffi_close_probe.c:704-710
     */
    static const char *kLoadFailureSource =
            "let ffi = import(\"zr.ffi\");\n"
            "ffi.loadLibrary(\"__zr_ffi_missing_fixture__\");\n"
            "return 0;\n";
    /**
     * ## Summary
     * 意图组合 library 双 close、callback 与 buffer 双 close；当前 all 源含已移除的 lambda 语法，编译被拒。
     * ## Constraints and lifecycle
     * BUG: 参数列表后的 (value) => 缺 fn；普通 Source_Compile 禁用旧语法，整段源在执行前被拒，不能取得任何 close 场景信用。字符串本轮保持原样。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:521；tests/ffi/debug_ffi_close_probe.c:643；tests/ffi/debug_ffi_close_probe.c:662；tests/ffi/debug_ffi_close_probe.c:688；tests/ffi/debug_ffi_close_probe.c:692；zr_vm_parser/src/zr_vm_parser/compiler/compiler.c:1279；zr_vm_parser/src/zr_vm_parser/compiler/compiler.c:1282；zr_vm_parser/src/zr_vm_parser/compiler/compiler.c:1292；zr_vm_parser/src/zr_vm_parser/compiler/compiler.c:1387；zr_vm_parser/src/zr_vm_parser/parser/parser.c:20；zr_vm_parser/src/zr_vm_parser/parser/parser.c:302；zr_vm_parser/src/zr_vm_parser/parser/parser.c:309；zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c:73；zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c:848；zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c:849；zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c:853；zr_vm_parser/src/zr_vm_parser/parser/parser_state.c:252；zr_vm_parser/src/zr_vm_parser/parser/parser_state.c:290
     */
    /**
     * ## Summary
     * 旧式源码原想提供 f64 identity 函数给 ffi.callback；它是被解析器拒绝的注入函数文本，不是已创建的 C/VM 回调。
     * ## Parameters
     * @param[in] value 按原设计为 callback 的 f64 参数；当前 all 源未通过编译，没有实际派发。
     * ## Returns
     * @return 原设计原样返回 value；当前解析失败，函数对象和此返回路径都未建立。
     * ## Constraints and lifecycle
     * BUG: 此 (value) => 缺当前语法要求的 fn；普通 parser 标 fatal 并令 compile_source 返回 NULL，main 销毁状态后返回3。无 runtime 复现信用，代码字符串未修复。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:521；tests/ffi/debug_ffi_close_probe.c:643；tests/ffi/debug_ffi_close_probe.c:662；tests/ffi/debug_ffi_close_probe.c:688；tests/ffi/debug_ffi_close_probe.c:692；zr_vm_parser/src/zr_vm_parser/compiler/compiler.c:1279；zr_vm_parser/src/zr_vm_parser/compiler/compiler.c:1282；zr_vm_parser/src/zr_vm_parser/compiler/compiler.c:1292；zr_vm_parser/src/zr_vm_parser/compiler/compiler.c:1387；zr_vm_parser/src/zr_vm_parser/parser/parser.c:20；zr_vm_parser/src/zr_vm_parser/parser/parser.c:302；zr_vm_parser/src/zr_vm_parser/parser/parser.c:309；zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c:73；zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c:848；zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c:849；zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c:853；zr_vm_parser/src/zr_vm_parser/parser/parser_state.c:252；zr_vm_parser/src/zr_vm_parser/parser/parser_state.c:290
     */
    static const char *kAllTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var beforeClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterFirstClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterSecondClose = lib.isClosed();\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, (value) => {\n"
            "    return value;\n"
            "});\n"
            "cb.close();\n"
            "cb.close();\n"
            "var buffer = ffi.BufferHandle.allocate(4);\n"
            "buffer.close();\n"
            "buffer.close();\n"
            "return (!beforeClose && afterFirstClose && afterSecondClose) ? 1 : 0;\n";
    /**
     * ## Summary
     * 无库地创建 callback 并关闭两次，观察重复关闭而非 callback 派发。
     * ## Constraints and lifecycle
     * 注入函数仅用于创建 handle；源无 native 调用，返回1是固定值，不是 ABI 断言。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:550-557；tests/ffi/debug_ffi_close_probe.c:630-631；tests/ffi/debug_ffi_close_probe.c:550-557；zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c:171-180
     */
    /**
     * ## Summary
     * 供 ffi.callback 创建使用的 ZR f64 identity 函数；函数体属于紧随的数据源，不是 C 回调入口。
     * ## Parameters
     * @param[in] value 真正派发时由 callback 调用方提供的 f64 值；本场景未派发。
     * ## Returns
     * @return 真正派发时原样返回 f64 value；该返回路径在此诊断场景未执行。
     * ## Constraints and lifecycle
     * 当前所在源只创建和关闭 handle，不派发该函数；函数体和其字符串字节保持不变，不能声称 native callback ABI 已运行。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:552-554
     */
    static const char *kCallbackSource =
            "let ffi = import(\"zr.ffi\");\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, fn(value) => {\n"
            "    return value;\n"
            "});\n"
            "cb.close();\n"
            "cb.close();\n"
            "return 1;\n";
    /**
     * ## Summary
     * 无库地分配和关闭 BufferHandle，观察重复关闭路径。
     * ## Constraints and lifecycle
     * 四字节分配后无 bytes 读写/pin 操作；返回1是固定值，宿主仅打印。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:566-571；tests/ffi/debug_ffi_close_probe.c:632-633；tests/ffi/debug_ffi_close_probe.c:566-571；zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c:652-667
     */
    static const char *kBufferSource =
            "let ffi = import(\"zr.ffi\");\n"
            "var buffer = ffi.BufferHandle.allocate(4);\n"
            "buffer.close();\n"
            "buffer.close();\n"
            "return 1;\n";
    /**
     * ## Summary
     * 默认或未知 mode 的无库组合场景，隔离 callback/buffer 创建关闭与 library 加载。
     * ## Constraints and lifecycle
     * callback 未派发、buffer 未读写；source 本身执行一次，不能据固定返回1推断全部资源不变量。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:592-602；tests/ffi/debug_ffi_close_probe.c:605-605；tests/ffi/debug_ffi_close_probe.c:592-606；tests/ffi/debug_ffi_close_probe.c:714-748
     */
    /**
     * ## Summary
     * 供 ffi.callback 创建使用的 ZR f64 identity 函数；函数体属于紧随的数据源，不是 C 回调入口。
     * ## Parameters
     * @param[in] value 真正派发时由 callback 调用方提供的 f64 值；本场景未派发。
     * ## Returns
     * @return 真正派发时原样返回 f64 value；该返回路径在此诊断场景未执行。
     * ## Constraints and lifecycle
     * 当前所在源只创建和关闭 handle，不派发该函数；函数体和其字符串字节保持不变，不能声称 native callback ABI 已运行。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:594-596
     */
    static const char *kAllWithoutLibrarySource =
            "let ffi = import(\"zr.ffi\");\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, fn(value) => {\n"
            "    return value;\n"
            "});\n"
            "cb.close();\n"
            "cb.close();\n"
            "var buffer = ffi.BufferHandle.allocate(4);\n"
            "buffer.close();\n"
            "buffer.close();\n"
            "return 1;\n";
    const char *mode = argc > 1 ? argv[1] : "all-no-lib";
    const char *libraryPath = argc > 2 ? argv[2] : NULL;
    const char *source = kAllWithoutLibrarySource;
    char sourceBuffer[4096];
    char escapedPath[4096];
    char errorBuffer[512];
    SZrState *state;
    SZrFunction *function;
    /**
     * ## Summary
     * 为普通模式接收执行结果的宿主值槽，其旧所有权状态是复制函数的输入前提。
     * ## Constraints and lifecycle
     * 非空 result 目标须为有效、已初始化的值槽；当前全新输出槽可先 ResetAsNull。load-failure 传空目标，不走此复制。
     * BUG: 此处自动变量只有声明、没有初始化；正常执行成功时 Value_Copy 先读取旧 ownershipKind/control/weakRef，构成未初始化读取，可能错误释放所有权或崩溃。修复入口是在传址前建立 ResetAsNull 状态，本轮只记录。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:620；tests/ffi/debug_ffi_close_probe.c:228-230；tests/ffi/debug_ffi_close_probe.c:714；zr_vm_core/include/zr_vm_core/value.h:147-164；zr_vm_core/include/zr_vm_core/value.h:218-236
     */
    SZrTypeValue result;

    /**
     * ## Summary
     * 把诊断模式映射到最小源，库模式在编译前把宿主路径放入模板。
     * ## Constraints and lifecycle
     * 未知模式沿用默认源；两个 snprintf 调用均未检查截断，错误可在后续编译阶段表现。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:630-672
     */
    if (strcmp(mode, "callback") == 0) {
        source = kCallbackSource;
    } else if (strcmp(mode, "buffer") == 0) {
        source = kBufferSource;
    } else if (strcmp(mode, "ternary") == 0) {
        source = kTernarySource;
    } else if (strcmp(mode, "load-failure") == 0) {
        source = kLoadFailureSource;
    } else if (strcmp(mode, "primitive-numeric-fold") == 0) {
        source = kPrimitiveNumericFoldSource;
    } else if (strcmp(mode, "library") == 0 || strcmp(mode, "all") == 0 || strcmp(mode, "primitive") == 0 ||
               strcmp(mode, "primitive-add") == 0 || strcmp(mode, "primitive-mul") == 0 ||
               strcmp(mode, "primitive-strlen") == 0 || strcmp(mode, "primitive-stdcall") == 0) {
        const char *templateSource = kAllTemplate;
        if (libraryPath == NULL) {
            fprintf(stderr, "library path is required for mode=%s\n", mode);
            return 1;
        }
        if (strcmp(mode, "library") == 0) {
            templateSource = kLibraryTemplate;
        } else if (strcmp(mode, "primitive") == 0) {
            templateSource = kPrimitiveTemplate;
        } else if (strcmp(mode, "primitive-add") == 0) {
            templateSource = kPrimitiveAddTemplate;
        } else if (strcmp(mode, "primitive-mul") == 0) {
            templateSource = kPrimitiveMulTemplate;
        } else if (strcmp(mode, "primitive-strlen") == 0) {
            templateSource = kPrimitiveStrlenTemplate;
        } else if (strcmp(mode, "primitive-stdcall") == 0) {
            templateSource = kPrimitiveStdcallTemplate;
        }
        escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), libraryPath);
        snprintf(sourceBuffer, sizeof(sourceBuffer), templateSource, escapedPath);
        source = sourceBuffer;
    } else if (strcmp(mode, "library-bool") == 0) {
        if (libraryPath == NULL) {
            fprintf(stderr, "library path is required for mode=%s\n", mode);
            return 1;
        }
        escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), libraryPath);
        snprintf(sourceBuffer, sizeof(sourceBuffer), kLibraryBoolTemplate, escapedPath);
        source = sourceBuffer;
    }

    /**
     * ## Summary
     * 按状态创建、源编译分阶段取得资源，并在编译失败时销毁已创建状态。
     * ## Constraints and lifecycle
     * 成功函数在最终退出前释放；不把创建成功扩大为 provider 全部登记成功。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:682-693
     */
    state = create_probe_state();
    if (state == ZR_NULL) {
        fprintf(stderr, "failed to create probe state\n");
        return 2;
    }

    function = compile_source(state, source, "debug_ffi_close_probe.zr");
    if (function == ZR_NULL) {
        fprintf(stderr, "failed to compile probe source\n");
        ZrTests_Runtime_State_Destroy(state);
        return 3;
    }

    /**
     * ## Summary
     * 把错误捕获的观察结果和期望失败模式转换为手工诊断的退出码。
     * ## Constraints and lifecycle
     * 仅按 FINE/non-FINE 分支，不校验失败确属缺失库加载。
     * TODO: 核查加载错误分类契约，并在此区分预期缺失库与编译/调用等其他运行错误，或明确保留纯观察性入口。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:704-710
     */
    if (strcmp(mode, "load-failure") == 0) {
        EZrThreadStatus status = execute_function_capture_status(state, function, ZR_NULL, errorBuffer, sizeof(errorBuffer));
        printf("capture mode=%s status=%d error='%s'\n", mode, (int) status, errorBuffer);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
        return status == ZR_THREAD_STATUS_FINE ? 5 : 0;
    }

    {
        EZrThreadStatus status =
                execute_function_capture_status(state, function, &result, errorBuffer, sizeof(errorBuffer));
        if (status != ZR_THREAD_STATUS_FINE) {
            fprintf(stderr,
                    "runtime execute failed: status=%d error=%s\n",
                    (int) status,
                    errorBuffer[0] != '\0' ? errorBuffer : "<non-string>");
            ZrCore_Function_Free(state, function);
            ZrTests_Runtime_State_Destroy(state);
            return 4;
        }
    }

    /**
     * ## Summary
     * 输出不同标量值形状并释放函数/状态，供手工比较诊断场景。
     * ## Constraints and lifecycle
     * 当前只打印，不按所选模式校验期待值；callback/buffer 模板的固定返回1也不等于资源断言。
     * TODO: 在此明确各模式的期望值检查或纯观察性目标；library bool 为 false、整数为0等不符合预期值时当前仍返回0。
     * ## References
     * tests/ffi/debug_ffi_close_probe.c:735-748
     */
    printf("probe mode=%s resultType=%d", mode, (int) result.type);
    if (ZR_VALUE_IS_TYPE_FLOAT(result.type)) {
        printf(" value=%f", result.value.nativeObject.nativeDouble);
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)) {
        printf(" value=%lld", (long long) result.value.nativeObject.nativeInt64);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        printf(" value=%llu", (unsigned long long) result.value.nativeObject.nativeUInt64);
    } else if (result.type == ZR_VALUE_TYPE_BOOL) {
        printf(" value=%s", result.value.nativeObject.nativeBool ? "true" : "false");
    }
    printf("\n");
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    return 0;
}
