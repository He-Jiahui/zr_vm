#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_ffi_fixture_path.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_parser.h"

#ifndef ZR_ARRAY_COUNT
/**
 * @brief 对本例静态导出名称数组取得元素数量
 *
 * ## Summary
 * 仅在公共定义不可见时提供本文件的后备宏；当前用于导出名称检查循环。
 *
 * ## Parameters
 * @param value 须为真实数组表达式，本例传入 exports 数组。
 *
 * ## Returns
 * @return 编译期 sizeof 比值表示的元素数量。
 *
 * ## Constraints and lifecycle
 * 不能把数组退化后的指针传入；本例仅使用固定数组，没有 VLA 参数求值情形。
 *
 * ## References
 * test_zr_ffi_import_exposes_known_types_and_functions。
 */
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

/* TODO: 这里用 0x1000 区分可释放地址和低值指针；需核对 VM allocator 合约是否保证真实堆指针不会落入低值区。 */
/**
 * @brief 为每例 VM 提供宿主堆分配回调
 *
 * ## Summary
 * 由 create_test_state 交给 GlobalState 构造器，随后成为预算分配器的 upstream 回调。
 *
 * ## Parameters
 * @param userData 本测试忽略的宿主上下文。
 * @param pointer 旧块地址；只有被低地址判断视为可释放的地址会传给 realloc/free。
 * @param originalSize VM 传入的旧块大小；本回调不以此记账。
 * @param newSize 新块字节数；零请求采用本回调的释放协议。
 * @param flag VM 内存类别；本回调不按类别选择分配策略。
 *
 * ## Returns
 * @return 成功的新块地址；释放或申请失败为 NULL，调用者按其请求类型解释。
 *
 * ## Constraints and lifecycle
 * 固定用于本文件测试，真实申请由 malloc/realloc 提供；内存统计在外层预算分配器完成。
 * TODO: 延续现有低地址 TODO；TZrPtr 是 void*，须核对宿主地址与工具链比较规则，并确认低值旧指针是否为合法输入协议。
 *
 * ## References
 * create_test_state -> ZrCore_GlobalState_New -> ZrCore_ExecutionBudget_Allocate。
 */
static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr)pointer >= (TZrPtr)0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    if ((TZrPtr)pointer >= (TZrPtr)0x1000) {
        return realloc(pointer, newSize);
    }

    return malloc(newSize);
}

/* 每个用例新建已注册 parser 和 provider 的隔离状态，避免动态库句柄跨用例复用。 */
/**
 * @brief 建立每个 FFI 用例独占的主线程 VM
 *
 * ## Summary
 * 为模块查询、源码编译和原生执行准备同一组内建模块；各测试直接持有返回 state，而非共享全局夹具。
 *
 * ## Returns
 * @return 初始化成功路径的主线程 state；GlobalState 构造返回 NULL 时本函数也返回 NULL。
 *
 * ## Constraints and lifecycle
 * 返回 state 关联的 global 由调用用例负责交给 destroy_test_state；不得单独释放主线程对象。
 * TODO: 注册过程没有在本函数安装外层 TryRun，也没有逐项检查注册结果；核对分配/注册失败及 mainThreadState 的构造失败协议。
 *
 * ## References
 * 全部 30 个用例；destroy_test_state；ZrCore_GlobalState_New。
 */
static SZrState *create_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 0x4646495F54455354ULL, &callbacks);
    SZrState *mainState;

    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    mainState = global->mainThreadState;
    if (mainState != ZR_NULL) {
        ZrCore_GlobalState_InitRegistry(mainState, global);
        ZrParser_ToGlobalState_Register(mainState);
        ZrVmLibMath_Register(global);
        ZrVmLibSystem_Register(global);
        ZrVmLibContainer_Register(global);
        ZrVmLibFfi_Register(global);
    }

    return mainState;
}

/**
 * @brief 结束本文件用例拥有的整个 VM 夹具
 *
 * ## Summary
 * 消费 create_test_state 交出的生命周期边界；测试先完成依赖 VM 的数值、对象或字符串断言，再销毁 global。
 *
 * ## Parameters
 * @param state 可为空；非空时须为本用例独占、尚未销毁的 state。
 *
 * ## Returns
 * @return 无返回值；非空 state 的 global 在这里进入整体释放流程。
 *
 * ## Constraints and lifecycle
 * 它不是对任意借用 state 的单线程清理接口；所有 VM 对象、结果借用及 native 字符地址在销毁后失效。
 * 本文件没有通过 tearDown 统一调用此入口；失败清理缺口见 tearDown 的 BUG。
 *
 * ## References
 * create_test_state；各例的正常收尾；ZrCore_GlobalState_Free。
 */
static void destroy_test_state(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrCore_GlobalState_Free(state->global);
    }
}

/* 动态脚本先经过 parser 再进入 VM；失败时打印该阶段的异常以区分绑定和调用错误。 */
/**
 * @brief 把本例源码编译为可交给执行 helper 的入口函数
 *
 * ## Summary
 * 保持源码名称与编译诊断关联，供正向执行及编译拒绝样本共用；NULL 是失败结果，不是精确的诊断类别。
 *
 * ## Parameters
 * @param state 已登记 parser 与内建模块的本例 VM。
 * @param source NUL 结尾的借用源码；以 strlen 获取长度，不能包含需要解析的内嵌 NUL。
 * @param sourceNameText 用于诊断的 NUL 结尾源码名称。
 *
 * ## Returns
 * @return 编译函数，供调用者执行并按本例生命周期释放；输入缺失或解析/编译失败为 NULL。
 *
 * ## Constraints and lifecycle
 * 编译尝试正常返回后释放 AST；打印出的状态/异常仅用于诊断，本函数不把失败状态重置成干净线程。
 * 负向用例若只断言 NULL，不能据此确定失败发生于预期的语义规则；须另外核对诊断。
 *
 * ## References
 * 28 个源码用例；ZrParser_Parse/ZrParser_Compiler_Compile；执行 helper。
 */
static SZrFunction *compile_source(SZrState *state, const TZrChar *source, const TZrChar *sourceNameText) {
    SZrAstNode *ast;
    SZrString *sourceName;
    SZrFunction *compiled;

    if (state == ZR_NULL || source == ZR_NULL || sourceNameText == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_CreateFromNative(state, (TZrNativeString)sourceNameText);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        fprintf(stderr,
                "parse_source failed for %s (threadStatus=%d hasCurrentException=%d)\n",
                sourceNameText,
                (int)state->threadStatus,
                (int)state->hasCurrentException);
        if (state->hasCurrentException) {
            ZrCore_Exception_PrintUnhandled(state, &state->currentException, stderr);
        }
        return ZR_NULL;
    }

    compiled = ZrParser_Compiler_Compile(state, ast);
    ZrParser_Ast_Free(state, ast);
    if (compiled == ZR_NULL) {
        fprintf(stderr,
                "compile_source failed for %s (threadStatus=%d hasCurrentException=%d)\n",
                sourceNameText,
                (int)state->threadStatus,
                (int)state->hasCurrentException);
        if (state->hasCurrentException) {
            ZrCore_Exception_PrintUnhandled(state, &state->currentException, stderr);
        }
    }

    return compiled;
}

/**
 * @brief 在 VM 有效期内借用字符串字符地址
 *
 * ## Summary
 * 版本测试及错误取证 helper 需要 C 字符串接口；错误取证必须先复制字符再重置线程。
 *
 * ## Parameters
 * @param state 拥有该字符串对象的有效 VM。
 * @param value 待检查的借用值，须保持对象有效。
 *
 * ## Returns
 * @return STRING 对象的借用字符地址；输入缺失、非 STRING 或对象为空时为 NULL。
 *
 * ## Constraints and lifecycle
 * 返回地址不转移所有权，也不能跨越 VM 销毁；本接口不负责把 Error 对象格式化为字符串。
 *
 * ## References
 * execute_function_capture_status；版本字符串测试。
 */
static const TZrChar *string_value_native(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object));
}

/**
 * @brief 取得模块元数据对象上的命名属性以检查入口可调用性
 *
 * ## Summary
 * 仅供 BufferHandle.allocate 元数据测试使用，沿对象查询 API 获取属性值供类型断言。
 *
 * ## Parameters
 * @param state 拥有对象和临时字段名称字符串的本例 VM。
 * @param object 借用的 BufferHandle 元数据对象。
 * @param fieldName NUL 结尾的借用属性名称。
 *
 * ## Returns
 * @return 对象查询得到的借用属性值，或输入/查询失败时为 NULL；该结果不是新分配的独立值。
 *
 * ## Constraints and lifecycle
 * 有效期受对象和 VM 约束；查询协议由 ZrCore_Object_GetValue 决定，本例只把结果用于 allocate 的可调用类型检查。
 * 本 helper 不提供独立的 VM 异常恢复点；分配或查询通过 VM 异常退出时须遵循调用方的恢复协议。
 *
 * ## References
 * test_zr_ffi_buffer_handle_allocate_is_callable -> ZrCore_Object_GetValue。
 */
static const SZrTypeValue *object_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

/* TODO: 调用方把转义后的路径写入更小的脚本缓冲且未核对 snprintf 截断；需用长构建路径验证。 */
/**
 * @brief 把 fixture 路径转为 ZR 字符串源码片段
 *
 * ## Summary
 * 路径来源是构建生成的头文件；本 helper 只为随后插入带引号的 ZR 源码转义双引号和反斜杠，不承担文件路径解析。
 *
 * ## Parameters
 * @param destination 可写输出缓冲区。
 * @param destinationSize 输出容量，含 NUL 结尾。
 * @param source 借用的 NUL 结尾路径；NULL 时生成空字符串。
 *
 * ## Returns
 * @return 无返回值；有效输出缓冲区得到 NUL 结尾片段，容量不足时只能得到前缀。
 *
 * ## Constraints and lifecycle
 * 不编码其他控制字符；无截断状态供调用者检查。
 * TODO: 延续现有容量 TODO；核对转义后路径与各例 snprintf 源码容量，以及截断/控制字符是否产生有效且符合原意的源码。
 *
 * ## References
 * 带 fixture 路径的源码用例；zr_vm_write_ffi_fixture_path.cmake。
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

/* 异常保护回调与外层共享结果栈位；成功后再由外层复制返回值。 */
/**
 * @brief 连接 TryRun 回调与错误取证调用方的栈上请求
 *
 * ## Summary
 * execute_function_capture_status 初始化并借用本请求；回调在正常 C 返回路径写入结果位置与完成标记，外层再判断是否可用。
 *
 * ## Constraints and lifecycle
 * 不拥有 function 或 VM 栈；request 必须持续有效到 TryRun 返回。
 * 结果位置只适用于当前恢复后的栈，调用方须在再次改变 VM 栈或销毁 state 前消费；未完成请求或状态非 FINE 时不能把结果位置当作有效输出。
 *
 * ## References
 * execute_function_capture_status -> zr_ffi_execute_capture_body -> ZrCore_Function_CallAndRestoreAnchor。
 */
typedef struct ZrFfiExecuteCaptureRequest {
    /** 借用输入函数，须在 TryRun 与 closure 建立期间保持可达。 */
    SZrFunction *function;
    /** 回调正常返回路径写入；须同时检查状态与完成标记，按锚点恢复后的栈位置不能跨后续栈变更保存。 */
    TZrStackValuePointer resultBase;
    /** 只表示回调正常收尾时 threadStatus 为 FINE；外层仍须检查 TryRun 状态及结果槽。 */
    TZrBool callCompleted;
} ZrFfiExecuteCaptureRequest;

/* 在恢复锚点保护下调用脚本入口，允许异常用例读回原始错误状态。 */
/**
 * @brief 在 TryRun 内调用入口并保留可读结果栈位
 *
 * ## Summary
 * 供错误测试的捕获 helper 使用，保留原始状态检查机会；栈锚点服务于原生调用可能重入 VM 后的栈地址恢复。
 *
 * ## Parameters
 * @param state 已经 launch 的本例主线程 VM。
 * @param arguments 借用的 ZrFfiExecuteCaptureRequest，必须持续有效至 TryRun 返回。
 *
 * ## Returns
 * @return 无返回值；回调正常返回时写入恢复后的结果栈位及 threadStatus 完成标记；写入前的提前返回或异常跳出保持缺省值。
 *
 * ## Constraints and lifecycle
 * 闭包入栈前暂存于忽略表，入栈后撤销临时登记；执行过程中应由 VM 栈保持可达。
 * TODO: 核对 Closure_New 分配前输入 function 的 GC 根，以及 IgnoreObject 登记失败或栈准备抛错时的临时保活平衡；共享 runtime helper 会先登记 function。
 *
 * ## References
 * execute_function_capture_status -> ZrCore_Exception_TryRun；ZrCore_Function_CallAndRestoreAnchor。
 */
static void zr_ffi_execute_capture_body(SZrState *state, TZrPtr arguments) {
    ZrFfiExecuteCaptureRequest *request = (ZrFfiExecuteCaptureRequest *)arguments;
    SZrClosure *closure;
    TZrBool ignoredClosure = ZR_FALSE;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor baseAnchor;
    SZrTypeValue *closureValue;

    if (state == ZR_NULL || request == ZR_NULL || request->function == ZR_NULL) {
        return;
    }

    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        return;
    }

    ignoredClosure = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closure->function = request->function;
    ZrCore_Closure_InitValue(state, closure);

    /** 原生符号可重入脚本并扩展 VM 栈；调用与结果读取须使用可恢复的栈位置。 */
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

/* 错误测试需要状态和栈顶字符串；取证后才重置线程，避免后续用例继承异常。 */
/**
 * @brief 为错误用例保留状态及重置前的字符串证据
 *
 * ## Summary
 * 与通用布尔执行 helper 区别在于提供错误状态和 C 字符串副本，供 LoadError、SymbolError、MarshalError 和 stdcall 平台分支断言。
 *
 * ## Parameters
 * @param state 有效且本例独占的已启动 VM；不能为空。
 * @param function 借用的入口函数，必须在本次调用期间有效。
 * @param result 可为空的全新输出值；仅成功时写入，失败时未初始化或清理此参数。
 * @param errorBuffer 可为空的错误副本缓冲区；仅复制 STRING 栈顶值。
 * @param errorBufferSize 错误缓冲区容量，含 NUL；为零时不写入。
 *
 * ## Returns
 * @return 成功为 FINE；否则为 TryRun 捕获状态、非 FINE 的 threadStatus 或 helper 合成的 RUNTIME_ERROR。
 *
 * ## Constraints and lifecycle
 * 在 ResetThread 前取出错误字符串；副本可供之后 C 断言使用，可能被容量截断；非字符串异常没有在此格式化。
 * result 的 VM 对象仍受本例 VM 生命周期约束；合成 RUNTIME_ERROR 不代表已经捕获特定 VM 异常。
 * TODO: ResetThread 可因清理失败改写其返回状态，而本 helper 忽略返回值并报告原状态；核对这种失败下对栈边界与诊断一致性的期望。
 *
 * ## References
 * 四个状态/文本用例；zr_ffi_execute_capture_body；ZrCore_State_ResetThread。
 */
static EZrThreadStatus execute_function_capture_status(SZrState *state,
                                                       SZrFunction *function,
                                                       SZrTypeValue *result,
                                                       char *errorBuffer,
                                                       size_t errorBufferSize) {
    ZrFfiExecuteCaptureRequest request;
    EZrThreadStatus status;

    if (errorBuffer != NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }
    if (state == ZR_NULL || function == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    memset(&request, 0, sizeof(request));
    request.function = function;
    status = ZrCore_Exception_TryRun(state, zr_ffi_execute_capture_body, &request);

    if (status == ZR_THREAD_STATUS_FINE && request.callCompleted && request.resultBase != ZR_NULL) {
        if (result != ZR_NULL) {
            ZrCore_Value_ResetAsNull(result);
            ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(request.resultBase));
        }
        return ZR_THREAD_STATUS_FINE;
    }

    /** 错误证据先复制到调用方缓冲区；随后线程恢复会清理异常与执行栈。 */
    if (errorBuffer != NULL && errorBufferSize > 0 && state->stackTop.valuePointer > state->stackBase.valuePointer) {
        const SZrTypeValue *errorValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer - 1);
        const TZrChar *nativeError = string_value_native(state, errorValue);
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

    return ZR_THREAD_STATUS_RUNTIME_ERROR;
}

/**
 * @brief 保留 Unity 用例开始回调的空实现
 *
 * ## Summary
 * Unity 每次 RUN_TEST 都会调用它；各例自行构造 VM，因此没有跨用例初始化状态。
 *
 * ## Returns
 * @return 无返回值；没有夹具副作用。
 *
 * ## Constraints and lifecycle
 * 本回调不建立测试 VM，也不为后续硬断言失败登记资源清理。
 *
 * ## References
 * UnityDefaultTestRun；各例 create_test_state。
 */
void setUp(void) {}

/**
 * @brief 保留 Unity 用例结束回调的空实现
 *
 * ## Summary
 * Unity 在正常完成或用例断言跳出后调用它，但本文件的资源当前由用例体正常尾部自行释放。
 *
 * ## Returns
 * @return 无返回值；当前没有释放本例资源的副作用。
 *
 * ## Constraints and lifecycle
 * BUG: 本例 VM 建立成功后，任一后续 Unity 硬断言失败会跳出用例体，越过其函数/VM 清理；空 tearDown 没有该局部 state 的所有权，无法补做释放。
 * 该缺口在所有 30 个用例的失败路径中存在；成功日志没有覆盖失败时的资源回收。
 *
 * ## References
 * UnityDefaultTestRun -> TEST_ABORT；destroy_test_state；本文件全部用例。
 */
void tearDown(void) {}

#include "test_ffi_integer_precision_contract_cases.h"

/**
 * @brief 核对 zr.ffi 的基础导出入口
 *
 * ## Summary
 * 通过注册表查询及必要的 Import 路径取得模块，逐项检查列出的 15 个名称存在。此例承担导出集合的契约检查。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 仅检查名称对应的值非空；类型、签名和方法执行由其他用例分别验证。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * ZrLib_Module_GetLoaded；ZrCore_Module_ImportByPath；ZrCore_Module_GetPubExport。
 */
static void test_zr_ffi_import_exposes_known_types_and_functions(void) {
    SZrTestTimer timer;
    SZrState *state;
    SZrObjectModule *module;
    /** 当前导出集合的存在性清单；方法签名和运行行为由另外的用例承担。 */
    const TZrChar *exports[] = {"loadLibrary", "callback", "sizeof", "alignof", "nullPointer",
                                "LibraryHandle", "SymbolHandle", "CallbackHandle", "PointerHandle", "BufferHandle",
                                "Ptr", "Ptr32", "Ptr64", "Char", "WChar"};
    TZrSize index;

    ZR_TEST_START("zr.ffi import exposes known types and functions");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    module = ZrLib_Module_GetLoaded(state, "zr.ffi");
    if (module == ZR_NULL) {
        SZrString *modulePath = ZrCore_String_CreateFromNative(state, "zr.ffi");
        TEST_ASSERT_NOT_NULL(modulePath);
        TEST_ASSERT_NOT_NULL(ZrCore_Module_ImportByPath(state, modulePath));
        module = ZrLib_Module_GetLoaded(state, "zr.ffi");
    }

    TEST_ASSERT_NOT_NULL(module);
    for (index = 0; index < ZR_ARRAY_COUNT(exports); index++) {
        SZrString *exportName = ZrCore_String_CreateFromNative(state, (TZrNativeString)exports[index]);
        TEST_ASSERT_NOT_NULL(exportName);
        TEST_ASSERT_NOT_NULL(ZrCore_Module_GetPubExport(state, module, exportName));
    }

    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi import exposes known types and functions");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查 BufferHandle.allocate 的可调用元数据
 *
 * ## Summary
 * 沿 zr.ffi 模块、BufferHandle 对象及 allocate 属性查询路径检查该入口为 FUNCTION 或 CLOSURE，为后续脚本分配测试确认元数据可见性。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 本例没有调用 allocate；不能据此证明分配、释放或容量语义。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * 模块元数据查询；object_field -> ZrCore_Object_GetValue；不执行源码函数。
 */
static void test_zr_ffi_buffer_handle_allocate_is_callable(void) {
    SZrTestTimer timer;
    SZrState *state;
    SZrObjectModule *module;
    const SZrTypeValue *bufferHandleValue;
    const SZrTypeValue *allocateValue;
    SZrObject *bufferHandleObject;

    ZR_TEST_START("zr.ffi BufferHandle.allocate is callable");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    module = ZrLib_Module_GetLoaded(state, "zr.ffi");
    if (module == ZR_NULL) {
        SZrString *modulePath = ZrCore_String_CreateFromNative(state, "zr.ffi");
        TEST_ASSERT_NOT_NULL(modulePath);
        TEST_ASSERT_NOT_NULL(ZrCore_Module_ImportByPath(state, modulePath));
        module = ZrLib_Module_GetLoaded(state, "zr.ffi");
    }

    TEST_ASSERT_NOT_NULL(module);
    bufferHandleValue = ZrLib_Module_GetExport(state, "zr.ffi", "BufferHandle");
    TEST_ASSERT_NOT_NULL(bufferHandleValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, bufferHandleValue->type);

    bufferHandleObject = ZR_CAST_OBJECT(state, bufferHandleValue->value.object);
    TEST_ASSERT_NOT_NULL(bufferHandleObject);
    allocateValue = object_field(state, bufferHandleObject, "allocate");
    TEST_ASSERT_NOT_NULL(allocateValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FUNCTION(allocateValue->type) || allocateValue->type == ZR_VALUE_TYPE_CLOSURE);

    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi BufferHandle.allocate is callable");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 核对整数、浮点和 UTF-8 字符串的原生往返
 *
 * ## Summary
 * 脚本加载 fixture 并调用 add_i32、mul_f64、strlen_utf8，以 12、8 和 5 的合计 25 检查结果；C 判据接受对应整数或浮点表示。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 只覆盖给定签名与参数；合计值不能逐项定位封送错误，也没有覆盖 NULL 字符串。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_can_load_fixture_and_call_primitive_symbols(void) {
    static const TZrChar *kSourceTemplate =
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
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi can load fixture and call primitive symbols");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_primitive_roundtrip.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type) || ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
                     ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));

    if (ZR_VALUE_IS_TYPE_FLOAT(result.type)) {
        TEST_ASSERT_DOUBLE_WITHIN(0.0001, 25.0, result.value.nativeObject.nativeDouble);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(25, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(25, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi can load fixture and call primitive symbols");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 验证 i64/u64 原生调用保留选定整数的精度
 *
 * ## Summary
 * 通过 identity_u64 和 identity_i64 返回并在脚本内比较选定的大整数，最终断言布尔真，检查原生往返没有经浮点中转丢失这些值。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * u64 样本超过 2^53；这两个样本不能代表完整整数边界或所有参数布局。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_preserves_64_bit_integer_precision(void) {
    static const TZrChar *kSourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var identityU64 = lib.getSymbol(\"zr_ffi_identity_u64\", {\n"
            "  returnType: \"u64\",\n"
            "  parameters: [{ type: \"u64\" }]\n"
            "});\n"
            "var identityI64 = lib.getSymbol(\"zr_ffi_identity_i64\", {\n"
            "  returnType: \"i64\",\n"
            "  parameters: [{ type: \"i64\" }]\n"
            "});\n"
            "var high = identityU64.call([9007199254740993]);\n"
            "var signed = identityI64.call([-9007199254740991]);\n"
            "var signedHigh = identityI64.call([9007199254740993]);\n"
            "var signedLow = identityI64.call([-9007199254740993]);\n"
            "return high == 9007199254740993 && signed == -9007199254740991"
            " && signedHigh == 9007199254740993 && signedLow == -9007199254740993;\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi preserves 64-bit integer precision");
    timer.startTime = clock();

    ffi_assert_signed64_precision_contract();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_64_bit_integer_precision.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.type);
    TEST_ASSERT_TRUE(result.value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi preserves 64-bit integer precision");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查 Point 按值返回及再次按值传参
 *
 * ## Summary
 * 脚本描述两个 i32 字段的 Point，接收 make_point(3,9) 并传给 sum_point，以 12 检查这条聚合值 ABI 路径。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 仅覆盖当前 fixture 的 Point 布局及两字段样本；未验证不同打包、嵌套聚合或其他 ABI 分类。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_can_roundtrip_struct_symbols(void) {
    static const TZrChar *kSourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var pointType = {\n"
            "  kind: \"struct\",\n"
            "  name: \"FixturePoint\",\n"
            "  fields: [\n"
            "    { name: \"x\", type: \"i32\" },\n"
            "    { name: \"y\", type: \"i32\" }\n"
            "  ]\n"
            "};\n"
            "var makePoint = lib.getSymbol(\"zr_ffi_make_point\", {\n"
            "  returnType: pointType,\n"
            "  parameters: [{ type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "var sumPoint = lib.getSymbol(\"zr_ffi_sum_point\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [{ type: pointType }]\n"
            "});\n"
            "var point = makePoint.call([3, 9]);\n"
            "return sumPoint.call([point]);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi can roundtrip struct symbols");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_struct_roundtrip.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(12, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(12, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi can roundtrip struct symbols");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查缓冲区读写及指针视图的建立
 *
 * ## Summary
 * 脚本分配 8 字节 buffer，建立 pin 及 direction=inout 的 u8 指针视图，并用 buffer 的 write/read 使两个采样字节合计为 5。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * TODO: typed 视图建立后没有参与读写；核对是否应单独验证视图的访问与有效期，当前 5 的断言只检查 buffer 采样。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_buffer_and_pointer_methods_work(void) {
    static const TZrChar *kSource =
            "let ffi = import(\"zr.ffi\");\n"
            "var buffer = ffi.BufferHandle.allocate(8);\n"
            "var ptr = buffer.pin();\n"
            "var typed = ptr.as({ kind: \"pointer\", to: \"u8\", direction: \"inout\" });\n"
            "buffer.write(0, [1, 2, 3, 4]);\n"
            "var bytes = buffer.read(0, 4);\n"
            "return bytes[0] + bytes[3];\n";
    SZrTestTimer timer;
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi buffer and pointer methods work");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, kSource, "ffi_buffer_pointer_methods.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(5, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(5, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi buffer and pointer methods work");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 通过显式 toPointer 向原生符号交付缓冲区
 *
 * ## Summary
 * 把 BufferHandle 显式转为 pointer 传给 fill_bytes，使用写入数量 4 与首字节 10 的合计 14 检查可写地址传递。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 仅核对长度返回值和首字节；没有逐字节确认整段写入或指针失效后的访问限制。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_can_fill_buffer_via_symbol(void) {
    static const TZrChar *kSourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var fillBytes = lib.getSymbol(\"zr_ffi_fill_bytes\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [\n"
            "    { type: { kind: \"pointer\", to: \"u8\", direction: \"inout\" } },\n"
            "    { type: \"u64\" },\n"
            "    { type: \"u8\" }\n"
            "  ]\n"
            "});\n"
            "var buffer = ffi.BufferHandle.allocate(8);\n"
            "var ptr = buffer.pin();\n"
            "var written = fillBytes.call([ptr, 4, 10]);\n"
            "return written + buffer.read(0, 4)[0];\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi can fill buffer via symbol");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_fill_buffer_symbol.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(14, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(14, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi can fill buffer via symbol");
    ZR_TEST_DIVIDER();
}

/* 与显式 pin 的用例成对：仅 native 参数入口可把 BufferHandle 隐式降为指针。 */
/**
 * @brief 检查原生符号参数接受直接 BufferHandle
 *
 * ## Summary
 * 与显式 toPointer 用例配对，直接将 BufferHandle 交给 fill_bytes 的 pointer 参数，以合计 14 检查原生边界的隐式降低。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 此许可限于本例原生符号调用边界，不能推广为普通 ZR 函数的 pointer 参数转换。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_can_lower_buffer_handle_directly_to_pointer_argument(void) {
    static const TZrChar *kSourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var fillBytes = lib.getSymbol(\"zr_ffi_fill_bytes\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [\n"
            "    { type: { kind: \"pointer\", to: \"u8\", direction: \"inout\" } },\n"
            "    { type: \"u64\" },\n"
            "    { type: \"u8\" }\n"
            "  ]\n"
            "});\n"
            "var buffer = ffi.BufferHandle.allocate(8);\n"
            "var written = fillBytes.call([buffer, 4, 10]);\n"
            "return written + buffer.read(0, 4)[0];\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi can lower BufferHandle directly to pointer arguments");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_buffer_handle_pointer_lowering.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(14, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(14, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi can lower BufferHandle directly to pointer arguments");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查脚本函数可形成 CallbackHandle
 *
 * ## Summary
 * 把两倍输入的脚本函数交给 ffi.callback，并断言得到非空结果，验证 callback 对象的创建入口。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 没有原生调用该 callback；回调执行、跨线程拒绝和关闭协议由另外的用例承担。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_can_create_callback_handle(void) {
    static const TZrChar *kSource =
            "let ffi = import(\"zr.ffi\");\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, fn(value) => {\n"
            "  return value * 2.0;\n"
            "});\n"
            "return cb != null;\n";
    SZrTestTimer timer;
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi can create callback handle");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, kSource, "ffi_create_callback_handle.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.type);
    TEST_ASSERT_TRUE(result.value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi can create callback handle");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 核对同线程原生到脚本的 callback 往返
 *
 * ## Summary
 * 原生 apply_callback 调用两倍输入的脚本 callback，再加 0.5；输入 5 的预期结果为 10.5，覆盖一次原生重入 VM。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 只覆盖成功的单次同步回调；不能证明回调异常策略、外线程准入或调用结束后的地址有效期。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_can_call_callback_symbol(void) {
    static const TZrChar *kSourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var applyCallback = lib.getSymbol(\"zr_ffi_apply_callback\", {\n"
            "  returnType: \"f64\",\n"
            "  parameters: [\n"
            "    { type: \"f64\" },\n"
            "    { type: { kind: \"function\", returnType: \"f64\", parameters: [{ type: \"f64\" }] } }\n"
            "  ]\n"
            "});\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, fn(value) => {\n"
            "  return value * 2.0;\n"
            "});\n"
            "return applyCallback.call([5.0, cb]);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi can call callback symbol");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_callback_symbol.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 10.5, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi can call callback symbol");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 组合检查结构体、可写缓冲区和 callback 的原生往返
 *
 * ## Summary
 * 将结构体返回、结构体输出、字节填充、i32 输出和 callback 的可观察结果合计为 58.5，验证这些路径可在同一 VM 执行中协作。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * TODO: outBuffer 用原始四字节表示 i32 的 10；核对该样例的字节序及宽度前提。合计判据也不能逐项定位错误。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_can_roundtrip_structs_buffers_and_callbacks(void) {
    static const TZrChar *kSourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var pointType = {\n"
            "  kind: \"struct\",\n"
            "  name: \"FixturePoint\",\n"
            "  fields: [\n"
            "    { name: \"x\", type: \"i32\" },\n"
            "    { name: \"y\", type: \"i32\" }\n"
            "  ]\n"
            "};\n"
            "var pointPtr = { kind: \"pointer\", to: pointType, direction: \"out\" };\n"
            "var makePoint = lib.getSymbol(\"zr_ffi_make_point\", {\n"
            "  returnType: pointType,\n"
            "  parameters: [{ type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "var sumPoint = lib.getSymbol(\"zr_ffi_sum_point\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [{ type: pointType }]\n"
            "});\n"
            "var fillPoint = lib.getSymbol(\"zr_ffi_fill_point\", {\n"
            "  returnType: \"void\",\n"
            "  parameters: [{ type: pointPtr }, { type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "var fillBytes = lib.getSymbol(\"zr_ffi_fill_bytes\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [\n"
            "    { type: { kind: \"pointer\", to: \"u8\", direction: \"inout\" } },\n"
            "    { type: \"u64\" },\n"
            "    { type: \"u8\" }\n"
            "  ]\n"
            "});\n"
            "var incrementI32 = lib.getSymbol(\"zr_ffi_increment_i32\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [{ type: { kind: \"pointer\", to: \"i32\", direction: \"inout\" } }]\n"
            "});\n"
            "var applyCallback = lib.getSymbol(\"zr_ffi_apply_callback\", {\n"
            "  returnType: \"f64\",\n"
            "  parameters: [\n"
            "    { type: \"f64\" },\n"
            "    { type: { kind: \"function\", returnType: \"f64\", parameters: [{ type: \"f64\" }] } }\n"
            "  ]\n"
            "});\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, fn(value) => {\n"
            "  return value * 2.0;\n"
            "});\n"
            "var point = makePoint.call([3, 9]);\n"
            "var sumA = sumPoint.call([point]);\n"
            "var pointBuffer = ffi.BufferHandle.allocate(8);\n"
            "var pointOutPtr = pointBuffer.pin().as(pointPtr);\n"
            "fillPoint.call([pointOutPtr, 4, 7]);\n"
            "var pointFromBuffer = pointOutPtr.read(pointType);\n"
            "var sumB = sumPoint.call([pointFromBuffer]);\n"
            "var buffer = ffi.BufferHandle.allocate(8);\n"
            "var bufferPtr = buffer.pin();\n"
            "var filled = fillBytes.call([bufferPtr, 4, 10]);\n"
            "var first = buffer.read(0, 4)[0];\n"
            "var outBuffer = ffi.BufferHandle.allocate(4);\n"
            "outBuffer.write(0, [10, 0, 0, 0]);\n"
            "var outPtr = outBuffer.pin().as({ kind: \"pointer\", to: \"i32\", direction: \"inout\" });\n"
            "incrementI32.call([outPtr]);\n"
            "var outValue = outPtr.read(\"i32\");\n"
            "var callbackResult = applyCallback.call([5.0, cb]);\n"
            "var total = 0.0;\n"
            "total = total + sumA + sumB + filled + first + outValue + callbackResult;\n"
            "return total;\n";
    SZrTestTimer timer;
    char source[8192];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi can roundtrip structs buffers and callbacks");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_struct_buffer_callback.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 58.5, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi can roundtrip structs buffers and callbacks");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 核对 LibraryHandle.getVersion 返回 fixture 的版本文本
 *
 * ## Summary
 * 通过脚本的 getVersion 入口读取原生版本字符串，再在 VM 有效期内检查 STRING 类型及完整文本 1.2.3-fixture。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * string_value_native 返回借用字符地址；该地址不能在本例销毁 VM 后继续使用。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_library_get_version_reads_fixture_symbol(void) {
    static const char *kSourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "return lib.getVersion();\n";
    SZrTestTimer timer;
    char source[1024];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;
    const TZrChar *versionText;

    ZR_TEST_START("zr.ffi LibraryHandle.getVersion reads version symbol");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_library_get_version.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    versionText = string_value_native(state, &result);
    TEST_ASSERT_NOT_NULL(versionText);
    TEST_ASSERT_EQUAL_STRING("1.2.3-fixture", versionText);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi LibraryHandle.getVersion reads version symbol");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查重复 close 的可观察行为
 *
 * ## Summary
 * 对 library、callback、buffer 分别重复 close；通过 library 的 isClosed 前后状态及脚本执行完成检查本次关闭序列。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * TODO: callback/buffer 仅由重复 close 后仍能完成脚本间接观察；核对是否需要各自关闭状态与再次使用的判据。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_handle_close_methods_are_idempotent(void) {
    static const char *kSourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var beforeClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterFirstClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterSecondClose = lib.isClosed();\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, fn(value) => {\n"
            "    return value;\n"
            "});\n"
            "cb.close();\n"
            "cb.close();\n"
            "var buffer = ffi.BufferHandle.allocate(4);\n"
            "buffer.close();\n"
            "buffer.close();\n"
            "return !beforeClose && afterFirstClose && afterSecondClose;\n";
    SZrTestTimer timer;
    char source[2048];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi handle close methods are idempotent");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_handle_close_idempotent.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(result.type == ZR_VALUE_TYPE_BOOL || ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
                     ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (result.type == ZR_VALUE_TYPE_BOOL) {
        TEST_ASSERT_TRUE(result.value.nativeObject.nativeBool);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(1, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(1, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi handle close methods are idempotent");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查缺失库的 LoadError 诊断类别
 *
 * ## Summary
 * 执行固定缺失库名的脚本，通过捕获 helper 检查非 FINE 状态及复制出的 [LoadError] 标签，使库装载失败与正常返回可区分。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 缺失库名称须在测试环境中确实不可解析；错误字符串只验证类别片段，不验证完整平台装载消息。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> execute_function_capture_status -> 状态/文本断言。
 */
static void test_zr_ffi_load_library_failure_reports_load_error(void) {
    static const char *kSource =
            "let ffi = import(\"zr.ffi\");\n"
            "ffi.loadLibrary(\"__zr_ffi_missing_fixture__\");\n"
            "return 0;\n";
    SZrTestTimer timer;
    SZrState *state;
    SZrFunction *entryFunction;
    char errorBuffer[512];

    ZR_TEST_START("zr.ffi loadLibrary failure reports LoadError");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, kSource, "ffi_load_failure.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_NOT_EQUAL_INT(ZR_THREAD_STATUS_FINE,
                              execute_function_capture_status(state, entryFunction, ZR_NULL, errorBuffer, sizeof(errorBuffer)));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "[LoadError]"));

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi loadLibrary failure reports LoadError");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查缺失符号的 SymbolError 及栈边界恢复
 *
 * ## Summary
 * 加载有效 fixture 后查询固定缺失符号，检查非 FINE 状态、[SymbolError] 标签及恢复后的入口槽和栈顶边界。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 这里只断言列出的栈边界；不代表 ResetThread 的所有 handler、pending control 或宿主配置都已验证。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> execute_function_capture_status -> 状态/文本断言。
 */
static void test_zr_ffi_get_symbol_failure_reports_symbol_error(void) {
    static const char *kSourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "lib.getSymbol(\"zr_ffi_missing_symbol\", {\n"
            "    returnType: \"i32\",\n"
            "    parameters: [{ type: \"i32\" }]\n"
            "});\n"
            "return 0;\n";
    SZrTestTimer timer;
    char source[1024];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    char errorBuffer[512];

    ZR_TEST_START("zr.ffi getSymbol failure reports SymbolError");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_missing_symbol.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_NOT_EQUAL_INT(ZR_THREAD_STATUS_FINE,
                              execute_function_capture_status(state, entryFunction, ZR_NULL, errorBuffer, sizeof(errorBuffer)));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "[SymbolError]"));
    TEST_ASSERT_EQUAL_PTR(state->stackBase.valuePointer + 1, state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, state->stackBase.valuePointer->value.type);
    TEST_ASSERT_FALSE(state->stackBase.valuePointer->value.isGarbageCollectable);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi getSymbol failure reports SymbolError");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 用显式展开签名验证固定的 varargs 调用
 *
 * ## Summary
 * 以 varargs:true 描述 count 和三个 i32 实参，调用 sum_varargs_i32(3,5,6,7) 并断言 18，检查该显式参数列表可传到 fixture。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 调用方仍负责显式列出实际原生参数；不能据此推导运行时自动识别任意变参类型。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_varargs_symbol_accepts_explicit_signature(void) {
    static const char *kSourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var sumVarargs = lib.getSymbol(\"zr_ffi_sum_varargs_i32\", {\n"
            "    returnType: \"i32\",\n"
            "    varargs: true,\n"
            "    parameters: [\n"
            "        { type: \"i32\" },\n"
            "        { type: \"i32\" },\n"
            "        { type: \"i32\" },\n"
            "        { type: \"i32\" }\n"
            "    ]\n"
            "});\n"
            "return sumVarargs.call([3, 5, 6, 7]);\n";
    SZrTestTimer timer;
    char source[2048];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi varargs symbol accepts explicit signature");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_varargs_success.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(18, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(18, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi varargs symbol accepts explicit signature");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查变参调用实参数量与显式签名不符时的诊断
 *
 * ## Summary
 * 给 sum_varargs_i32 只声明一个 i32 参数却提交四个值，检查非 FINE、[MarshalError] 及 expected 1 arguments but got 4 文本。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 本例检查 FFI 调用前的签名/参数数量一致性；没有让不匹配的 C 变参读取实际执行。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> execute_function_capture_status -> 状态/文本断言。
 */
static void test_zr_ffi_varargs_call_without_matching_signature_reports_marshal_error(void) {
    static const char *kSourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var sumVarargs = lib.getSymbol(\"zr_ffi_sum_varargs_i32\", {\n"
            "    returnType: \"i32\",\n"
            "    parameters: [{ type: \"i32\" }]\n"
            "});\n"
            "return sumVarargs.call([3, 5, 6, 7]);\n";
    SZrTestTimer timer;
    char source[1024];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    char errorBuffer[512];

    ZR_TEST_START("zr.ffi varargs call without matching signature reports MarshalError");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_varargs_marshal_error.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_NOT_EQUAL_INT(ZR_THREAD_STATUS_FINE,
                              execute_function_capture_status(state, entryFunction, ZR_NULL, errorBuffer, sizeof(errorBuffer)));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "[MarshalError]"));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "expected 1 arguments but got 4"));

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi varargs call without matching signature reports MarshalError");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 按平台分别检查 stdcall 签名准入
 *
 * ## Summary
 * Windows 分支断言 stdcall fixture 调用返回整数 13；其他平台分支检查非 FINE、[MarshalError] 及不支持 stdcall 的诊断。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 判据由 _WIN32 选择；只证明本构建与该签名的行为，不能泛化为所有体系结构上 stdcall 的 ABI 差别。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> execute_function_capture_status -> 状态/文本断言。
 */
static void test_zr_ffi_stdcall_signature_matches_platform_support(void) {
    static const char *kSourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var add = lib.getSymbol(\"zr_ffi_stdcall_add_i32\", {\n"
            "    returnType: \"i32\",\n"
            "    abi: \"stdcall\",\n"
            "    parameters: [{ type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "return add.call([4, 9]);\n";
    SZrTestTimer timer;
    char source[1024];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;
    char errorBuffer[512];
    EZrThreadStatus status;

    ZR_TEST_START("zr.ffi stdcall signature matches platform support");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_stdcall_support.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    status = execute_function_capture_status(state, entryFunction, &result, errorBuffer, sizeof(errorBuffer));
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, status);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(13, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(13, result.value.nativeObject.nativeInt64);
    }
#else
    TEST_ASSERT_NOT_EQUAL_INT(ZR_THREAD_STATUS_FINE, status);
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "[MarshalError]"));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "stdcall is not supported"));
#endif

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi stdcall signature matches platform support");
    ZR_TEST_DIVIDER();
}

/* fixture 在新宿主线程回调，验证未附着线程不会擅自进入原 VM state。
 * TODO: 目前只断言执行失败，需检查诊断确实指向跨线程策略。 */
/**
 * @brief 检查外线程 callback 的调用被执行入口判为失败
 *
 * ## Summary
 * fixture 建立宿主线程、调用 callback 后等待该线程结束；本例只用执行返回值为假检查这一脚本没有成功完成。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * TODO: 延续现有诊断 TODO，核对 callback thread 专用错误类别及 foreign thread 消息；布尔失败也可能来自其他执行错误。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_foreign_thread_callback_reports_error(void) {
    static const TZrChar *kSourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var applyCallback = lib.getSymbol(\"zr_ffi_apply_callback_foreign_thread\", {\n"
            "  returnType: \"f64\",\n"
            "  parameters: [\n"
            "    { type: \"f64\" },\n"
            "    { type: { kind: \"function\", returnType: \"f64\", parameters: [{ type: \"f64\" }] } }\n"
            "  ]\n"
            "});\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, fn(value) => {\n"
            "  return 3.0;\n"
            "});\n"
            "return applyCallback.call([2.0, cb]);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi foreign thread callback reports error");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_foreign_thread_callback.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi foreign thread callback reports error");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查源码 nativeExtern 声明可绑定并调用符号
 *
 * ## Summary
 * 把源码 native extern 及 entry 声明绑定到 fixture 的 add_i32，以 7+5 返回整数 12，覆盖声明到原生入口的集成链。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 仅验证给定外部符号和签名；运行库路径由本构建生成的 fixture 头文件提供。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_source_extern_can_bind_and_call_symbol(void) {
    static const TZrChar *kSourceTemplate =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_add_i32\")# fn Add(lhs:i32, rhs:i32): i32;\n"
            "}\n"
            "return Add(7, 5);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi source extern can bind and call symbol");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_source_extern_add.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(12, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(12, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi source extern can bind and call symbol");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查源码 extern delegate 与 CallbackHandle 协作
 *
 * ## Summary
 * 使用 Unary delegate 及 callbackLifetime(call)、callbackThread(caller)、callbackException(returnDefault) 声明，正常 callback 路径返回 10.5。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * TODO: 此例的 callback 不抛错、也不跨线程或在调用后再次执行；核对三项 policy 的负向判据，成功结果不能单独验证这些限制。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_source_extern_delegate_works_with_callback(void) {
    static const TZrChar *kSourceTemplate =
            "native extern(\"%s\") {\n"
            "  delegate Unary(value:f64): f64;\n"
            "  #zr.ffi.entry(\"zr_ffi_apply_callback\")#\n"
            "  #zr.ffi.callbackLifetime(\"call\")#\n"
            "  #zr.ffi.callbackThread(\"caller\")#\n"
            "  #zr.ffi.callbackException(\"returnDefault\")#\n"
            "  fn Apply(value:f64, cb:Unary): f64;\n"
            "}\n"
            "var ffi = import(\"zr.ffi\");\n"
            "var cb = ffi.callback(Unary, fn(value) => {\n"
            "  return value * 2.0;\n"
            "});\n"
            "return Apply(5.0, cb);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi source extern delegate works with callback");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_source_extern_delegate.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 10.5, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi source extern delegate works with callback");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查源码 extern pointer 参数接受 BufferHandle
 *
 * ## Summary
 * 在源码 native extern Fill 声明处直接传递 BufferHandle，以写入数量与首字节合计 14 检查编译声明及原生调用的指针降低链。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 与普通函数负向用例共同限定转换边界；该许可不等于普通调用普遍接受 BufferHandle 到 pointer 的转换。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_source_extern_pointer_parameter_accepts_buffer_handle(void) {
    static const TZrChar *kSourceTemplate =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_fill_bytes\")# fn Fill(buffer: pointer<u8>, length:u64, seed:u8): i32;\n"
            "}\n"
            "var ffi = import(\"zr.ffi\");\n"
            "var buffer = ffi.BufferHandle.allocate(8);\n"
            "var written = Fill(buffer, 4, 10);\n"
            "return written + buffer.read(0, 4)[0];\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi source extern pointer parameter accepts BufferHandle");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_source_extern_pointer_lowering.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(14, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(14, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi source extern pointer parameter accepts BufferHandle");
    ZR_TEST_DIVIDER();
}

/* 普通 ZR 函数不能借用 native extern 的 delegate 包装转换。
 * TODO: 这里只断言编译失败，需核对诊断确实来自包装转换限制。 */
/**
 * @brief 检查普通调用未获得 delegate 的原生降低许可
 *
 * ## Summary
 * 把 ffi callback 对象交给普通 ZR delegate 形参，要求 compile_source 返回 NULL，作为普通调用边界拒绝该转换的负向样本。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * TODO: 延续现有 TODO，核对 NULL 对应的编译诊断与失败阶段；当前判据不能排除源码因其他错误而编译失败。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> NULL 编译判据；此负向样本不调用执行 helper。
 */
static void test_zr_ffi_wrapper_lowering_does_not_apply_to_ordinary_calls(void) {
    static const TZrChar *kSourceTemplate =
            "native extern(\"%s\") {\n"
            "  delegate Unary(value:f64): f64;\n"
            "}\n"
            "var ffi = import(\"zr.ffi\");\n"
            "fn ApplyLocal(cb: Unary): f64 {\n"
            "  return 1.0;\n"
            "}\n"
            "var cb = ffi.callback(Unary, fn(value) => {\n"
            "  return value * 2.0;\n"
            "});\n"
            "return ApplyLocal(cb);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;

    ZR_TEST_START("zr.ffi wrapper lowering does not apply to ordinary calls");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_wrapper_not_ordinary_call.zr");
    TEST_ASSERT_NULL(entryFunction);

    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi wrapper lowering does not apply to ordinary calls");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查 source wrapper 在 extern handle_id 参数处的降低
 *
 * ## Summary
 * 以 ModeHandle 的 lowering/underlying 声明把 handleId 值 1 交给原生 Flip 的 i32 参数，断言 fixture 翻转结果为 0。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 仅覆盖该包装类型和原生参数边界；普通 ZR i32 形参的拒绝规则由另一个负向样本检查。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_source_extern_handle_id_parameter_accepts_source_wrapper(void) {
    static const TZrChar *kSourceTemplate =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_flip_mode\")# fn Flip(mode:i32): i32;\n"
            "}\n"
            "#zr.ffi.lowering(\"handle_id\")#\n"
            "#zr.ffi.underlying(\"i32\")#\n"
            "class ModeHandle {\n"
            "  pub var handleId:i32;\n"
            "}\n"
            "var mode = new ModeHandle();\n"
            "mode.handleId = 1;\n"
            "return Flip(mode);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi source extern handle_id parameter accepts source wrapper");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_source_extern_handle_id_lowering.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(0, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(0, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi source extern handle_id parameter accepts source wrapper");
    ZR_TEST_DIVIDER();
}

/* handle_id 解包只属于 native extern 边界，普通函数调用仍按声明类型检查。 */
/**
 * @brief 检查普通 i32 形参不接受 handle_id 包装对象
 *
 * ## Summary
 * 把 ModeHandle 直接交给普通 ZR 函数的 i32 形参，要求编译返回 NULL，以限定 wrapper 降低的适用调用边界。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * TODO: 核对对应的编译诊断与阶段；仅有 NULL 不能确认失败确由普通调用的转换限制产生。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> NULL 编译判据；此负向样本不调用执行 helper。
 */
static void test_zr_ffi_handle_id_lowering_does_not_apply_to_ordinary_calls(void) {
    static const TZrChar *kSourceTemplate =
            "#zr.ffi.lowering(\"handle_id\")#\n"
            "#zr.ffi.underlying(\"i32\")#\n"
            "class ModeHandle {\n"
            "  pub var handleId:i32;\n"
            "}\n"
            "fn FlipLocal(mode:i32): i32 {\n"
            "  return mode;\n"
            "}\n"
            "var mode = new ModeHandle();\n"
            "mode.handleId = 1;\n"
            "return FlipLocal(mode);\n";
    SZrTestTimer timer;
    SZrState *state;
    SZrFunction *entryFunction;

    ZR_TEST_START("zr.ffi handle_id lowering does not apply to ordinary calls");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, kSourceTemplate, "ffi_handle_id_not_ordinary_call.zr");
    TEST_ASSERT_NULL(entryFunction);

    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi handle_id lowering does not apply to ordinary calls");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查 handle_id wrapper 不支持字符串 underlying 的负向样本
 *
 * ## Summary
 * 仅声明字符串 underlying 的 handle_id wrapper，并要求编译返回 NULL；本样本没有调用原生 i32 参数入口。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * TODO: 核对诊断是否明确指向 unsupported underlying；当前 NULL 判据可能包含其他声明或编译错误。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> NULL 编译判据；此负向样本不调用执行 helper。
 */
static void test_zr_ffi_handle_id_wrapper_rejects_unsupported_underlying_type(void) {
    static const TZrChar *kSourceTemplate =
            "#zr.ffi.lowering(\"handle_id\")#\n"
            "#zr.ffi.underlying(\"string\")#\n"
            "class ModeHandle {\n"
            "  var handleId:i32;\n"
            "}\n"
            "return 0;\n";
    SZrTestTimer timer;
    SZrState *state;
    SZrFunction *entryFunction;

    ZR_TEST_START("zr.ffi handle_id wrapper rejects unsupported underlying type");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, kSourceTemplate, "ffi_handle_id_invalid_underlying.zr");
    TEST_ASSERT_NULL(entryFunction);

    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi handle_id wrapper rejects unsupported underlying type");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查 wrapper viewType 不接受普通非 extern struct 的负向样本
 *
 * ## Summary
 * 让 lowering(value) wrapper 的 viewType 指向普通 PlainView 结构体，并要求编译返回 NULL。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * TODO: 核对诊断是否确由 viewType 的 extern 布局限制产生；当前样本没有检查具体错误类别。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> NULL 编译判据；此负向样本不调用执行 helper。
 */
static void test_zr_ffi_wrapper_view_type_rejects_non_extern_struct(void) {
    static const TZrChar *kSourceTemplate =
            "struct PlainView {\n"
            "  var raw:i32;\n"
            "}\n"
            "#zr.ffi.lowering(\"value\")#\n"
            "#zr.ffi.viewType(\"PlainView\")#\n"
            "class PlainViewWrapper {\n"
            "  var raw:i32;\n"
            "}\n"
            "return 0;\n";
    SZrTestTimer timer;
    SZrState *state;
    SZrFunction *entryFunction;

    ZR_TEST_START("zr.ffi wrapper viewType rejects non-extern struct");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, kSourceTemplate, "ffi_wrapper_invalid_view_type.zr");
    TEST_ASSERT_NULL(entryFunction);

    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi wrapper viewType rejects non-extern struct");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查源码 system 调用约定映射到本平台默认调用
 *
 * ## Summary
 * 在 native extern 声明中选择 callconv(system)，调用 fixture add_i32(4,9) 并断言 13，覆盖当前平台的声明到原生调用链。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * 本例只验证本平台默认约定的成功样本；没有比较不同显式调用约定的 ABI 行为。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_source_extern_system_callconv_uses_platform_default(void) {
    static const TZrChar *kSourceTemplate =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_add_i32\")#\n"
            "  #zr.ffi.callconv(\"system\")#\n"
            "  fn Add(lhs:i32, rhs:i32): i32;\n"
            "}\n"
            "return Add(4, 9);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi source extern system callconv uses platform default");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_source_extern_system_callconv.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(13, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(13, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi source extern system callconv uses platform default");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 检查 extern struct 的 pack 对布局查询的可观察影响
 *
 * ## Summary
 * 声明 pack(1) 的 u8/u32 PackedPair，以 sizeof 与 alignof 的合计 6 检查布局元数据可被脚本查询。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * TODO: 核对是否应分别断言 sizeof=5 与 alignof=1；合计 6 会接受互相抵消的两个错误值。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_source_extern_struct_pack_affects_sizeof_and_alignof(void) {
    static const TZrChar *kSourceTemplate =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.pack(1)#\n"
            "  struct PackedPair {\n"
            "    var tag:u8;\n"
            "    var value:u32;\n"
            "  }\n"
            "}\n"
            "var ffi = import(\"zr.ffi\");\n"
            "return ffi.sizeof(PackedPair) + ffi.alignof(PackedPair);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi source extern struct pack affects sizeof and alignof");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_source_extern_pack_layout.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(6, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(6, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi source extern struct pack affects sizeof and alignof");
    ZR_TEST_DIVIDER();
}

/* 偏移叠加视图必须让两个字段读取同一四字节区域，检验读取布局而非仅检验 sizeof。 */
/**
 * @brief 通过 pointer read 检查 extern overlay 的共享偏移布局
 *
 * ## Summary
 * 把 u32/f32 字段都声明在 offset 0，通过填充原始字节的 buffer 建立指针视图并读 overlay，以整数位模式与浮点值的合计 1065353217 检查共享布局。
 *
 * ## Returns
 * @return 无返回值；检查失败通过 Unity 报告，正常尾部清理本例夹具并打印采样日志。
 *
 * ## Constraints and lifecycle
 * 由 main 经 Unity 调用；用例不接收共享 VM。
 * TODO: 样例字节编码采用小端 IEEE 754 binary32 的 1.0；核对测试支持平台及独立字段断言，当前合计不能逐项定位解释错误。
 * 日志 timer 记录整个正常用例路径的进程 CPU 时间，不能解释为单个原生符号的耗时。
 * BUG: VM 创建成功后的 Unity 硬断言失败会跳过本例局部清理；空 tearDown 没有该 state，无法回收，见该回调的共同失败路径记录。
 *
 * ## References
 * main -> RUN_TEST -> 本例；create_test_state/destroy_test_state。
 * compile_source -> ZrTests_Runtime_Function_Execute -> 本例类型/结果断言。
 */
static void test_zr_ffi_source_extern_struct_offset_overlay_controls_pointer_read(void) {
    static const TZrChar *kSourceTemplate =
            "native extern(\"%s\") {\n"
            "  struct Overlay32 {\n"
            "    #zr.ffi.offset(0)# var raw:u32;\n"
            "    #zr.ffi.offset(0)# var asFloat:f32;\n"
            "  }\n"
            "}\n"
            "var ffi = import(\"zr.ffi\");\n"
            "var buffer = ffi.BufferHandle.allocate(8);\n"
            "buffer.write(0, [0, 0, 128, 63, 0, 0, 0, 0]);\n"
            "var value = buffer.pin().read(Overlay32);\n"
            "return value.raw + value.asFloat;\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi source extern struct offset overlay controls pointer read");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_source_extern_offset_overlay.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 1065353217.0, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi source extern struct offset overlay controls pointer read");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 派发 FFI 模块用例并向进程调用方返回 Unity 失败计数
 *
 * ## Summary
 * 作为 zr_vm_ffi_test 的进程入口，登记当前 30 个用例，围绕同一 fixture 分别验证元数据、原生调用和语言声明边界。
 *
 * ## Returns
 * @return 正常返回时为 UNITY_END 汇总的失败用例计数；忽略用例另行计账。
 *
 * ## Constraints and lifecycle
 * 不创建共享 VM；各例使用独立 state，setUp/tearDown 当前为空，失败清理缺口见 tearDown。
 * 目标只有在 parser/core 构建门禁满足且没有提前返回时建立；fixture 路径头文件由目标依赖生成。
 * TODO: 核对本目标的自动运行入口；当前显式目标引用检索确认构建依赖，但尚未确认 suite/CTest 会实际启动 zr_vm_ffi_test。
 *
 * ## References
 * tests/CMakeLists.txt 的 zr_vm_add_unity_test_target；UnityDefaultTestRun；可直接运行已构建目标。
 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zr_ffi_import_exposes_known_types_and_functions);
    RUN_TEST(test_zr_ffi_buffer_handle_allocate_is_callable);
    RUN_TEST(test_zr_ffi_can_load_fixture_and_call_primitive_symbols);
    RUN_TEST(test_zr_ffi_preserves_64_bit_integer_precision);
    RUN_TEST(test_zr_ffi_can_roundtrip_struct_symbols);
    RUN_TEST(test_zr_ffi_buffer_and_pointer_methods_work);
    RUN_TEST(test_zr_ffi_can_fill_buffer_via_symbol);
    RUN_TEST(test_zr_ffi_can_lower_buffer_handle_directly_to_pointer_argument);
    RUN_TEST(test_zr_ffi_can_create_callback_handle);
    RUN_TEST(test_zr_ffi_can_call_callback_symbol);
    RUN_TEST(test_zr_ffi_can_roundtrip_structs_buffers_and_callbacks);
    RUN_TEST(test_zr_ffi_library_get_version_reads_fixture_symbol);
    RUN_TEST(test_zr_ffi_handle_close_methods_are_idempotent);
    RUN_TEST(test_zr_ffi_load_library_failure_reports_load_error);
    RUN_TEST(test_zr_ffi_get_symbol_failure_reports_symbol_error);
    RUN_TEST(test_zr_ffi_varargs_symbol_accepts_explicit_signature);
    RUN_TEST(test_zr_ffi_varargs_call_without_matching_signature_reports_marshal_error);
    RUN_TEST(test_zr_ffi_stdcall_signature_matches_platform_support);
    RUN_TEST(test_zr_ffi_foreign_thread_callback_reports_error);
    RUN_TEST(test_zr_ffi_source_extern_can_bind_and_call_symbol);
    RUN_TEST(test_zr_ffi_source_extern_delegate_works_with_callback);
    RUN_TEST(test_zr_ffi_source_extern_pointer_parameter_accepts_buffer_handle);
    RUN_TEST(test_zr_ffi_wrapper_lowering_does_not_apply_to_ordinary_calls);
    RUN_TEST(test_zr_ffi_source_extern_handle_id_parameter_accepts_source_wrapper);
    RUN_TEST(test_zr_ffi_handle_id_lowering_does_not_apply_to_ordinary_calls);
    RUN_TEST(test_zr_ffi_handle_id_wrapper_rejects_unsupported_underlying_type);
    RUN_TEST(test_zr_ffi_wrapper_view_type_rejects_non_extern_struct);
    RUN_TEST(test_zr_ffi_source_extern_system_callconv_uses_platform_default);
    RUN_TEST(test_zr_ffi_source_extern_struct_pack_affects_sizeof_and_alignof);
    RUN_TEST(test_zr_ffi_source_extern_struct_offset_overlay_controls_pointer_read);
    return UNITY_END();
}
