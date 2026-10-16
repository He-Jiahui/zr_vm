/**
 * @file
 * ## Summary
 * 在 Test provider 的 native 边界产生有界宿主诊断，并把失败投影成脚本可捕获的 AssertionFailure。
 * 比较先于诊断格式化；TLS 副本供 CLI 报告，VM 异常对象供脚本 catch，两者遵循不同的所有权协议。
 *
 * ## Constraints and lifecycle
 * TLS 以 OS 线程划分，不以 GlobalState 或测试用例划分；宿主在用例前后负责 Clear/Get 窗口。
 * 回调通常经正常 descriptor/dispatch 进入，遵循 Test phase、参数稳定副本和托管 pin 约束。
 * BUG: 用户TO_STRING内失败断言可覆盖外层TLS；成功Assert或显式ClearLastFailure可清空它，外层Equal继续投影错归因。
 * TODO: 沿testing_snapshot/Equal补嵌套formatter回归，核查记录保存恢复方案。
 * 本批只增加契约说明；记录构造的分配失败和 GC 根边界分别在对应 helper 标记。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:172、zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:176、zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:181 是函数指针注册入口；zr_vm_lib_testing/CMakeLists.txt:11 经 zr_vm_common/CommonMacros.cmake:9 收集本源。
 * zr_vm_cli/src/zr_vm_cli/commands/test_command.c:551、zr_vm_cli/src/zr_vm_cli/commands/test_command.c:570、zr_vm_cli/src/zr_vm_cli/commands/test_command.c:592 管理宿主报告；docs/library-and-builtins/zr-testing.md 说明公共签名和诊断投影。
 */
#include "zr_vm_lib_testing/module.h"

/**
 * 运行时依赖只用于帧恢复、异常捕获、TypeId 验证和诊断文本；不把编译期测试发现放进此 provider。
 * stdio/string 的文本操作构造普通 C 副本；实际构建依赖见 zr_vm_lib_testing/CMakeLists.txt:15、zr_vm_lib_testing/CMakeLists.txt:18。
 */
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdio.h>
#include <string.h>

/**
 * ## Summary
 * 将测试失败副本绑定到平台 OS 线程局部存储，允许 CLI 为调用线程建立独立诊断窗口。
 *
 * ## Value model
 * 别名完全沿用 ZR_THREAD_LOCAL；没有 GlobalState key、自动用例清理或重入记录栈。
 *
 * ## Constraints and lifecycle
 * 同线程不同 VM 仍共享记录；存储随线程/provider 生命周期存在，普通按值副本可另行保存。
 *
 * ## References
 * zr_vm_common/include/zr_vm_common/zr_common_conf.h:129 定义平台选择；本文件的 g_last_failure/g_has_last_failure 为实际消费者。
 */
#define ZR_TESTING_THREAD_LOCAL ZR_THREAD_LOCAL

/**
 * ## Summary
 * 保存调用线程最近一次失败的内联文本、坐标和分类，供 CLI 读取及 AssertionFailure 字段投影。
 *
 * ## Invariants and lifecycle
 * g_has_last_failure 才决定记录有效性；全零文本也可表示有效失败。副本没有 VM 对象指针或 GC 根。
 * 失败 begin 覆盖全记录，成功断言及宿主 Clear 清理；Get 只复制，不消费。同线程不同 VM 共用此状态。
 * BUG: TO_STRING内失败Assert覆盖外层TLS；成功Assert或显式ClearLastFailure清空有效位，外层Equal仍继续投影当前记录。
 * 用户formatter显式调ClearLastFailure或成功断言才清TLS有效位；snapshot的ClearCurrent仅清VM异常。沿 zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:358、zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:882、zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1048 补嵌套回归。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:746 开启记录；zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:693 投影到异常；zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:795 按值交付；zr_vm_cli/src/zr_vm_cli/commands/test_command.c:570 读取后报告。
 */
static ZR_TESTING_THREAD_LOCAL SZrTestingAssertionFailure g_last_failure;
/**
 * 有效位与同线程 g_last_failure 一起开启/清理；不要用空消息或零坐标推断没有失败。
 */
static ZR_TESTING_THREAD_LOCAL TZrBool g_has_last_failure = ZR_FALSE;

/**
 * ## Summary
 * 把 throws 的一次同步 action 借用及完成状态送进 TryRun 回调，不延长 action/provider 的生命周期。
 *
 * ## Fields
 * callable 借用正常派发的稳定值副本；result 是 CallValue 写入的 VM 值槽，completed 是宿主调用完成标志。
 *
 * ## Invariants and lifecycle
 * Throws 清零后在同一同步 TryRun 窗口使用；completed 不是 native callback bool，也不表示已发生异常。
 * CallValue 先把 callable 移入 scratch，正常完成才交付 result；此局部 result 不作为 TLS 诊断或托管根。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1015、zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1016 构造；zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:910 派发；zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1023 判断未抛路径；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1781 是宿主 CallValue 边界；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1813 稳定复制，zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1861 预留scratch，zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1909 复制入栈；zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1017、zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1018 是实际callback传入点。
 */
typedef struct SZrTestingCallRequest {
    /**
     * 正常dispatcher稳定参数副本的借用；CallValue再复制入scratch，仅同步action窗口有效。
     */
    const SZrTypeValue *callable;
    /**
     * CallValue初始化并交付的局部VM值槽；不等同Throws对外异常result或TLS快照。
     */
    SZrTypeValue result;
    /**
     * 宿主CallValue完成状态；正常Fine且为真才证明action正常结束，不是callback bool。
     */
    TZrBool completed;
} SZrTestingCallRequest;

/**
 * ## Summary
 * 携带一次受保护的调试格式化请求，使诊断层能区分返回文本与用户格式化异常。
 *
 * ## Fields
 * value 借用当前稳定 VM 值；text 接收 VM 管理的格式字符串，入口为 NULL，回调异常时可保持 NULL。
 *
 * ## Invariants and lifecycle
 * 仅同步 TryRun 期间使用；text 不是 C 所有权，返回后立即无分配地复制到有界 C 数组。
 * 不得把未 root 的 text 跨下一次分配/安全点保存；格式化可执行用户元方法，帧恢复不恢复测试 TLS。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:356、zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:357 初始化；zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:264 填文本；zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:362 检查；zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:369、zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:370 紧邻消费。
 */
typedef struct SZrTestingFormatRequest {
    /**
     * 本次格式化的VM值借用；正常dispatcher稳定副本/当前异常根负责有效性，载荷不建立根。
     */
    SZrTypeValue *value;
    /**
     * 回调写入的VM字符串借用；入口NULL，返回后立即复制，禁止跨下一次分配保存裸地址。
     */
    SZrString *text;
} SZrTestingFormatRequest;

/**
 * ## Summary
 * 在动作或诊断 formatter 的异常边界结束后还原外层调用帧，让断言继续使用原调用位置与结果槽。
 *
 * ## Parameters
 * @param[in,out] state 当前 VM 状态；NULL 正常返回 RUNTIME_ERROR。
 * @param[in] function 同步 TryRun 回调；NULL 同样返回 RUNTIME_ERROR。
 * @param[in,out] arguments 回调载荷借用，仅同步边界内有效，不转移所有权。
 *
 * ## Returns
 * @return TryRun 捕获的状态；不据此替代 currentException、completed 或 threadStatus 的各自判定。
 *
 * ## Constraints and lifecycle
 * 保存栈相对锚点而不是旧裸栈地址，允许内部扩栈；只恢复实际存在的 base/top/return 槽及处理器深度。
 * 外层 CallInfo 须仍有效；不修复回调自行破坏的资源协议，不保存/恢复测试 TLS 或当前异常。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:358;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1017;zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:30;zr_vm_core/src/zr_vm_core/function/function.c:1831;zr_vm_core/src/zr_vm_core/function/function.c:1840
 */
static EZrThreadStatus testing_try_run_preserving_frame(
        SZrState *state,
        FZrTryFunction function,
        TZrPtr arguments) {
    SZrCallInfo *savedCallInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor savedCallInfoBaseAnchor;
    SZrFunctionStackAnchor savedCallInfoTopAnchor;
    SZrFunctionStackAnchor savedCallInfoReturnAnchor;
    TZrBool hasSavedCallInfoBase = ZR_FALSE;
    TZrBool hasSavedCallInfoTop = ZR_FALSE;
    TZrBool hasSavedCallInfoReturn = ZR_FALSE;
    TZrUInt32 savedExceptionHandlerStackLength;
    EZrThreadStatus status;

    if (state == ZR_NULL || function == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    savedExceptionHandlerStackLength = state->exceptionHandlerStackLength;
    ZrCore_Function_StackAnchorInit(state, state->stackTop.valuePointer, &savedStackTopAnchor);
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionBase.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(
                state, savedCallInfo->functionBase.valuePointer, &savedCallInfoBaseAnchor);
        hasSavedCallInfoBase = ZR_TRUE;
    }
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(
                state, savedCallInfo->functionTop.valuePointer, &savedCallInfoTopAnchor);
        hasSavedCallInfoTop = ZR_TRUE;
    }
    if (savedCallInfo != ZR_NULL && savedCallInfo->hasReturnDestination &&
        savedCallInfo->returnDestination != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(
                state, savedCallInfo->returnDestination, &savedCallInfoReturnAnchor);
        hasSavedCallInfoReturn = ZR_TRUE;
    }

/**
 * 恢复外层锚点后再还原handler深度；回调异常不应使后续断言落在内部帧。
 */
    status = ZrCore_Exception_TryRun(state, function, arguments);
    state->stackTop.valuePointer =
            ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    state->callInfoList = savedCallInfo;
    if (savedCallInfo != ZR_NULL) {
        if (hasSavedCallInfoBase) {
            savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(
                    state, &savedCallInfoBaseAnchor);
        }
        if (hasSavedCallInfoTop) {
            savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(
                    state, &savedCallInfoTopAnchor);
        }
        if (hasSavedCallInfoReturn) {
            savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(
                    state, &savedCallInfoReturnAnchor);
        }
    }
    state->exceptionHandlerStackLength = savedExceptionHandlerStackLength;
    return status;
}

/**
 * ## Summary
 * 给宿主失败副本复制可选 NUL 文本，统一所有文本字段的容量和终止约束。
 *
 * ## Parameters
 * @param[out] destination capacity>0 时必须是对应容量的可写缓冲。
 * @param[in] capacity 总字节容量，包括尾 NUL；0 时不写任何输出。
 * @param[in] source 调用期有效的 NUL 文本，NULL 视为空文本。
 * @param[out] truncated 可选；capacity>0 时报告本次是否丢弃字节，capacity=0 时保持不变。
 *
 * ## Returns
 * 无返回值；正容量时最多复制 capacity-1 字节并 NUL 终止，不保留输入文本。
 *
 * ## Constraints and lifecycle
 * BUG: 按字节截断 UTF-8；86个“€”为258字节，写入257字节数组只复制256字节，留下第86个字符的首字节。
 * 快照/消息/源码名均可沿此路径传播非法编码；现有长文本测试只覆盖ASCII裁剪，未运行UTF-8 fixture。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:299;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:300;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:307;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:310;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:320;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:324;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:327;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:332;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:335;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:345;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:349;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:364;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:370;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:430;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:750;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1027;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1060;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1064;zr_vm_lib_testing/include/zr_vm_lib_testing/module.h:39;zr_vm_lib_testing/include/zr_vm_lib_testing/module.h:98;tests/testing/test_assertions.c:189
 */
static void testing_copy_bounded(TZrChar *destination,
                                 TZrSize capacity,
                                 const TZrChar *source,
                                 TZrBool *truncated) {
    TZrSize length;
    TZrSize copyLength;

    if (capacity == 0U) {
        return;
    }
    length = source != ZR_NULL ? strlen(source) : 0U;
    copyLength = length < capacity - 1U ? length : capacity - 1U;
    if (copyLength > 0U) {
        memcpy(destination, source, copyLength);
    }
    destination[copyLength] = '\0';
    if (truncated != ZR_NULL) {
        *truncated = length > copyLength ? ZR_TRUE : ZR_FALSE;
    }
}

/**
 * ## Summary
 * 在 TryRun 内把调试格式化交给 core，使诊断层可捕获元方法异常而不中断原断言场景。
 *
 * ## Parameters
 * @param[in,out] state 有效的当前VM，由受保护调用者提供。
 * @param[in,out] arguments 指向本次 SZrTestingFormatRequest；value 借用，text 由回调写入。
 *
 * ## Returns
 * 无返回值；正常交付托管文本借用，异常由 TryRun 捕获，text 可仍为 NULL。
 *
 * ## Constraints and lifecycle
 * Value_ToDebugString 可执行用户TO_STRING、分配或抛错；回调不建立新的对象根，不跨同步窗口保存载荷。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:358;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:264;zr_vm_core/src/zr_vm_core/value/value.c:553;zr_vm_core/src/zr_vm_core/value/value.c:600
 */
static void testing_format_try(SZrState *state, TZrPtr arguments) {
    SZrTestingFormatRequest *request = (SZrTestingFormatRequest *)arguments;
    request->text = ZrCore_Value_ToDebugString(state, request->value);
}

/**
 * ## Summary
 * 在失败已经确定后生成独立有界诊断视图；格式化故障与相等性/异常匹配结论分开解释。
 *
 * ## Parameters
 * @param[in,out] state 当前VM，可NULL；与value缺失一样生成未填写null视图。
 * @param[in] value 稳定VM值借用，可NULL；合法VM null仍表示已有值。
 * @param[out] snapshot 非NULL可写普通结构；入口清零，无对象所有权交付。
 *
 * ## Returns
 * 无返回值；值缺失时 hasValue=false；正常VM值为true，格式化异常/无文本另置formatterFaulted。
 *
 * ## Constraints and lifecycle
 * 对象typeName来自原型诊断名；先复制名称，再经TryRun格式化；正常文本紧邻无分配复制，不跨安全点保存裸text。
 * BUG: formatter抛错会ClearCurrent；Throws对原异常的后续匹配因此失败，见其已证异常快照链。
 * BUG: TO_STRING内失败Assert覆盖TLS后抛入本边界；成功Assert或显式ClearLastFailure清空外层TLS，Equal仍继续构造失败。
 * TODO: 用嵌套formatter回归核查保存恢复，不把已有Equal formatterFaulted测试当作归因覆盖。
 * BUG: core Value_ToDebugString将snprintf所需长度累加到offset；合法对象含超过buffer容量的字符串字段时，后续写尾括号/终止NUL越界。
 * TODO: 元方法分配/GC期间obj、pair裸地址稳定性仍需对应core模块强制GC复核；本批未修复core，也未运行长字段fixture。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:882;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:883;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1048;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:358;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:365;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:370;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1052;zr_vm_core/src/zr_vm_core/value/value.c:329;zr_vm_core/src/zr_vm_core/value/value.c:335;zr_vm_core/src/zr_vm_core/value/value.c:600;zr_vm_core/src/zr_vm_core/value/value.c:604;zr_vm_core/src/zr_vm_core/value/value.c:628;zr_vm_core/src/zr_vm_core/value/value.c:695;zr_vm_core/src/zr_vm_core/exception/exception.c:407;zr_vm_core/src/zr_vm_core/exception/exception.c:684;tests/testing/test_assertions.c:395;tests/testing/test_assertions.c:432;zr_vm_core/src/zr_vm_core/object/object.c:2208
 */
static void testing_snapshot(SZrState *state,
                             const SZrTypeValue *value,
                             SZrTestingValueSnapshot *snapshot) {
    SZrTestingFormatRequest request;
    EZrThreadStatus status;
    const TZrChar *text;

    memset(snapshot, 0, sizeof(*snapshot));
    if (state == ZR_NULL || value == ZR_NULL) {
        testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "null", ZR_NULL);
        testing_copy_bounded(snapshot->text, sizeof(snapshot->text), "<null>", ZR_NULL);
        return;
    }

    snapshot->hasValue = ZR_TRUE;
    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "null", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_BOOL:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "bool", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "int", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "float", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_STRING:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "string", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE_VALUE:
        case ZR_VALUE_TYPE_CLOSURE:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "function", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_ARRAY:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "array", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_OBJECT: {
            SZrObject *object = value->value.object != ZR_NULL
                                      ? ZR_CAST_OBJECT(state, value->value.object)
                                      : ZR_NULL;
            const TZrChar *typeName = object != ZR_NULL && object->prototype != ZR_NULL &&
                                              object->prototype->name != ZR_NULL
                                              ? ZrCore_String_GetNativeString(object->prototype->name)
                                              : "object";
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), typeName, ZR_NULL);
            break;
        }
        default:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "value", ZR_NULL);
            break;
    }

/**
 * BUG: TO_STRING内失败断言覆盖TLS；成功Assert或显式ClearLastFailure清空外层TLS，TryRun仅恢复VM帧，Equal仍投影当前记录。
 */
    request.value = (SZrTypeValue *)value;
    request.text = ZR_NULL;
    status = testing_try_run_preserving_frame(state, testing_format_try, &request);
/**
 * BUG: formatter异常或正常返回NULL文本均清VM当前异常；Throws继续CatchMatches已失去原对象，抛错合法触发链见Throws契约。
 */
    if (status != ZR_THREAD_STATUS_FINE || request.text == ZR_NULL) {
        snapshot->formatterFaulted = ZR_TRUE;
        testing_copy_bounded(snapshot->text, sizeof(snapshot->text), "<format-error>", ZR_NULL);
        ZrCore_Exception_ClearCurrent(state);
        state->threadStatus = ZR_THREAD_STATUS_FINE;
        return;
    }
    text = ZrCore_String_GetNativeString(request.text);
    testing_copy_bounded(snapshot->text, sizeof(snapshot->text), text, &snapshot->truncated);
}

/**
 * ## Summary
 * 把最近脚本 VM 帧的当前位置投影为失败归因范围；native 断言帧自身不提供脚本坐标。
 *
 * ## Parameters
 * @param[in] context 正常native上下文借用；NULL/无state时只留零记录。
 * @param[out] outSpan 可写普通范围副本，可NULL表示不输出。
 *
 * ## Returns
 * 无返回值；缺帧/函数/PC/instructions时保持零；有源码但无匹配位置时允许只有sourceFile。
 *
 * ## Constraints and lifecycle
 * PC必须属于该函数instructions；位置表按offset递增，长度/list须一致，选最后一个不晚于PC的记录。
 * 只在无分配窗口复制源码名；副本不保活Function/String/CallInfo。
 * TODO: endColumn包含/排除边界及畸形metadata防御，沿io_runtime位置生产者/编译finalize与边界fixture核查。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:749;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:405;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:419;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:423;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:439;zr_vm_core/src/zr_vm_core/io/io_runtime.c:695;tests/testing/test_assertions.c:503;tests/testing/test_assertions.c:506
 */
static void testing_source_span(const ZrLibCallContext *context,
                                SZrTestingSourceSpan *outSpan) {
    SZrCallInfo *callInfo;

    if (outSpan == ZR_NULL) {
        return;
    }
    memset(outSpan, 0, sizeof(*outSpan));
    if (context == ZR_NULL || context->state == ZR_NULL) {
        return;
    }

    callInfo = context->state->callInfoList;
    while (callInfo != ZR_NULL && !ZR_CALL_INFO_IS_VM(callInfo)) {
        callInfo = callInfo->previous;
    }
    if (callInfo == ZR_NULL || callInfo->metadataFunction == ZR_NULL ||
        callInfo->context.context.programCounter == ZR_NULL ||
        callInfo->metadataFunction->instructionsList == ZR_NULL) {
        return;
    }

    SZrFunction *function = callInfo->metadataFunction;
/**
 * TODO: 当前生产者保持位置表配对和顺序；畸形Function及PC边界沿io_runtime/finalize fixture核查。
 */
    TZrMemoryOffset instructionOffset =
            (TZrMemoryOffset)(callInfo->context.context.programCounter - function->instructionsList);
    const SZrFunctionExecutionLocationInfo *best = ZR_NULL;
    for (TZrUInt32 index = 0U; index < function->executionLocationInfoLength; index++) {
        const SZrFunctionExecutionLocationInfo *location =
                &function->executionLocationInfoList[index];
        if (location->currentInstructionOffset > instructionOffset) {
            break;
        }
        best = location;
    }
    if (function->sourceCodeList != ZR_NULL) {
        testing_copy_bounded(outSpan->sourceFile,
                             sizeof(outSpan->sourceFile),
                             ZrCore_String_GetNativeString(function->sourceCodeList),
                             ZR_NULL);
    }
    if (best != ZR_NULL) {
        outSpan->startLine = best->lineInSource;
        outSpan->startColumn = best->columnInSourceStart;
        outSpan->endLine = best->lineInSourceEnd;
        outSpan->endColumn = best->columnInSourceEnd;
    }
}

/**
 * ## Summary
 * 为失败记录投影数值成员；标量封装与通用对象写入分层，保持同域写屏障协议。
 *
 * ## Parameters
 * @param[in,out] state 有效同域VM状态，调用期间借用。
 * @param[in] object 同域有效对象借用，调用期间保持有效。
 * @param[in] field 有效NUL字段名，仅调用期读取。
 * @param[in] value 按值传入的整数，通常为分类或源码坐标。
 *
 * ## Returns
 * 无返回值；字段setter为void，失败可能缺字段，不能由调用结束证明写入成功。
 *
 * ## Constraints and lifecycle
 * 整数封装不分配且标量不需要value pin；SetFieldCString按需pin owner/key，写屏障后仅撤销本调用新增pin，不转移C所有权。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:617;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:618;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:619;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:620;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:693;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1289;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1385;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1388;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1408;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:44;zr_vm_core/include/zr_vm_core/value.h:311
 */
static void testing_set_object_int(SZrState *state,
                                   SZrObject *object,
                                   const TZrChar *field,
                                   TZrInt64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetInt(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, field, &fieldValue);
}

/**
 * ## Summary
 * 把宿主诊断文本复制成VM字符串并写入脚本记录，使脚本异常不借用TLS字符数组。
 *
 * ## Parameters
 * @param[in,out] state 有效同域VM状态，调用期间借用。
 * @param[in] object 同域有效对象借用，调用期间保持有效。
 * @param[in] field 有效NUL字段名，仅调用期读取。
 * @param[in] value 调用期有效的NUL文本；NULL按空字符串处理，不转移输入所有权。
 *
 * ## Returns
 * 无返回值；字符串创建和字段写入均不反馈成功码，不能承诺完整记录。
 *
 * ## Constraints and lifecycle
 * 字符串创建先于SetField入口的owner pin；裸record跨此分配窗口的GC稳定性见new_record TODO。
 * BUG: 有效StringTable的地址cache/content均miss，短串新对象创建成功且插入越过resizeThreshold；宿主分配器仅拒绝HASH_BUCKET扩容并保留旧桶时，
 * Rehash(false)→AddRawObject(NULL)→CreateTryHitCache(NULL)→SetStringObject不写fieldValue；随后pin读取未初始化值。
 * 完整静态链支持未定义读取标签；未执行fault-injection，不把它与普通GcMalloc抛错或后续cache-null BUG混同。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:616;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:648;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:649;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:694;zr_vm_core/include/zr_vm_core/memory.h:19;zr_vm_core/src/zr_vm_core/base/hash_set.c:171;zr_vm_core/src/zr_vm_core/base/hash_set.c:181;zr_vm_core/include/zr_vm_core/hash_set.h:535;zr_vm_core/src/zr_vm_core/string/string.c:802;zr_vm_core/src/zr_vm_core/string/string.c:973;zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:320;zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:324;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1307;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1312;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1373;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1388;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:44;zr_vm_core/include/zr_vm_core/value.h:311
 */
static void testing_set_object_string(SZrState *state,
                                      SZrObject *object,
                                      const TZrChar *field,
                                      const TZrChar *value) {
/**
 * BUG: SetString可因短串表扩容失败保持此局部值未初始化；下一字段setter的pin会读取它，完整链见函数契约。
 */
    SZrTypeValue fieldValue;
    ZrLib_Value_SetString(state, &fieldValue, value != ZR_NULL ? value : "");
    ZrLib_Object_SetFieldCString(state, object, field, &fieldValue);
}

/**
 * ## Summary
 * 为ValueSnapshot投影普通诊断标志；不把有值、截断或格式化故障混成比较结果。
 *
 * ## Parameters
 * @param[in,out] state 有效同域VM状态，调用期间借用。
 * @param[in] object 同域有效对象借用，调用期间保持有效。
 * @param[in] field 有效NUL字段名，仅调用期读取。
 * @param[in] value 按值布尔标志，不持有对象或线程。
 *
 * ## Returns
 * 无返回值；底层void字段写入失败不会通过本helper报告。
 *
 * ## Constraints and lifecycle
 * Bool封装不分配且标量无需value pin；SetFieldCString按需pin owner/key并创建键/做写屏障，记录仍由VM可达性管理，可能留下缺字段。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:650;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:651;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:652;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1281;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1385;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1388;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1408;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:44;zr_vm_core/include/zr_vm_core/value.h:311
 */
static void testing_set_object_bool(SZrState *state,
                                    SZrObject *object,
                                    const TZrChar *field,
                                    TZrBool value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetBool(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, field, &fieldValue);
}

/**
 * ## Summary
 * 为SourceSpan和ValueSnapshot查找已注册原型并建立同域脚本实例，隔离C诊断布局与VM对象schema。
 *
 * ## Parameters
 * @param[in,out] state 有效VM状态；当前两个调用使用无点号静态类型名，查找只创建/查找字符串，不触发qualified lazy import。
 * @param[in] typeName 调用期有效NUL类型名，当前调用者使用静态literal。
 *
 * ## Returns
 * @return 原型缺失/实例创建返回NULL时为NULL；成功返回VM管理、未建立本地root/pin的raw对象借用。
 *
 * ## Constraints and lifecycle
 * 原型由注册物化永久标记，新record只有INITED/current generation；C返回指针不提供GC根。
 * TODO: new_record返回至首字符串分配、连续setter之间强制full/compacting GC，核查root和搬迁指针重取。
 * 当前代暂不判未引用不能证明跨代/移动安全，现有证据不足标UAF；调用者也不能把暂时pin当永久所有权。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:612;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:644;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1595;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1602;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1633;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1678;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:884;zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:889;zr_vm_core/src/zr_vm_core/object/object.c:2097;zr_vm_core/src/zr_vm_core/object/object.c:2155;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1092;zr_vm_core/src/zr_vm_core/gc/gc_internal.h:54
 */
static SZrObject *testing_new_record(SZrState *state, const TZrChar *typeName) {
    SZrObjectPrototype *prototype = ZrLib_Type_FindPrototype(state, typeName);
    return prototype != ZR_NULL
                   ? ZrLib_Type_NewInstanceWithPrototype(state, prototype)
                   : ZR_NULL;
}

/**
 * ## Summary
 * 把嵌套诊断record挂接到AssertionFailure，NULL保持为VM null而不创建替代对象。
 *
 * ## Parameters
 * @param[in,out] state 有效同域VM状态，调用期间借用。
 * @param[in] object 同域有效对象借用，调用期间保持有效。
 * @param[in] field 有效NUL字段名，仅调用期读取。
 * @param[in] record 同域有效嵌套对象借用，可NULL；调用前须已保持有效。
 *
 * ## Returns
 * 无返回值；void字段setter失败可保留缺字段，未建立调用者可持有的永久根。
 *
 * ## Constraints and lifecycle
 * SetObject只构造tagged value，不pin；紧邻SetField先pin outer/value后分配键，成功后字段写屏障保留VM引用。
 * 不转移nested C所有权；builder在进入此helper前的raw对象分配窗口仍需独立GC核查。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:695;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:697;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:699;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:701;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1320;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1385;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1388;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1408;zr_vm_core/src/zr_vm_core/object/object.c:1873;zr_vm_core/src/zr_vm_core/object/object.c:1875
 */
static void testing_set_object_record(SZrState *state,
                                      SZrObject *object,
                                      const TZrChar *field,
                                      SZrObject *record) {
    SZrTypeValue fieldValue;
    if (record == ZR_NULL) {
        ZrLib_Value_SetNull(&fieldValue);
    } else {
        ZrLib_Value_SetObject(state, &fieldValue, record, ZR_VALUE_TYPE_OBJECT);
    }
    ZrLib_Object_SetFieldCString(state, object, field, &fieldValue);
}

/**
 * ## Summary
 * 把普通SourceSpan副本按descriptor schema投影为脚本失败位置记录，供raise建立sourceSpan字段。
 *
 * ## Parameters
 * @param[in,out] state 有效同域VM状态。
 * @param[in] span 本次调用借用的C坐标/源码文本结构，可NULL。
 *
 * ## Returns
 * @return 新建raw SourceSpan或NULL；span为NULL时返回已创建的空record，不保证返回NULL。
 *
 * ## Constraints and lifecycle
 * 输入文本复制进VM字符串；四个坐标按值写入，不持有span。void setter失败可产生部分记录，无回滚。
 * TODO: 首个sourceFile分配前及后续setter间raw owner的GC稳定性沿new_record强制GC入口核查。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:696;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:267;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:412;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:612;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:616;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:617
 */
static SZrObject *testing_build_source_span_object(SZrState *state,
                                                   const SZrTestingSourceSpan *span) {
    SZrObject *object = testing_new_record(state, "SourceSpan");
    if (object == ZR_NULL || span == ZR_NULL) {
        return object;
    }
    testing_set_object_string(state, object, "sourceFile", span->sourceFile);
    testing_set_object_int(state, object, "startLine", span->startLine);
    testing_set_object_int(state, object, "startColumn", span->startColumn);
    testing_set_object_int(state, object, "endLine", span->endLine);
    testing_set_object_int(state, object, "endColumn", span->endColumn);
    return object;
}

/**
 * ## Summary
 * 把已截取的C诊断快照投影为ValueSnapshot对象，保持比较/格式化标志各自语义。
 *
 * ## Parameters
 * @param[in,out] state 有效同域VM状态。
 * @param[in] snapshot 本次调用借用的有界文本/标志副本，可NULL。
 *
 * ## Returns
 * @return 新建raw ValueSnapshot或NULL；snapshot为NULL时仍返回已创建的空record。
 *
 * ## Constraints and lifecycle
 * 两次字符串创建后写普通标志，输入C缓冲不转移；void写入失败可留下部分字段。
 * TODO: 首字符串及连续setter间的raw记录稳定性需强制GC验证；SetString NULL的未初始化BUG仍适用。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:698;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:700;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:702;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:303;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:433;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:648;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:649;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:650
 */
static SZrObject *testing_build_snapshot_object(SZrState *state,
                                                const SZrTestingValueSnapshot *snapshot) {
    SZrObject *object = testing_new_record(state, "ValueSnapshot");
    if (object == ZR_NULL || snapshot == ZR_NULL) {
        return object;
    }
    testing_set_object_string(state, object, "typeName", snapshot->typeName);
    testing_set_object_string(state, object, "text", snapshot->text);
    testing_set_object_bool(state, object, "hasValue", snapshot->hasValue);
    testing_set_object_bool(state, object, "truncated", snapshot->truncated);
    testing_set_object_bool(state, object, "formatterFaulted", snapshot->formatterFaulted);
    return object;
}

/**
 * ## Summary
 * 把当前线程失败同时交给宿主TLS报告和脚本AssertionFailure；结构化路径不可用时退回普通VM运行错误。
 *
 * ## Parameters
 * @param[in,out] context 正常native上下文借用，可NULL；非NULL时state须有效，失败记录已由begin/快照填好。
 *
 * ## Returns
 * ZR_NO_RETURN：Normalize成功后Throw；可达fallback调用Debug_RunError，无state或意外返回最终Abort。
 *
 * ## Constraints and lifecycle
 * 原型查找/实例创建软返回NULL才进入fallback；GC分配OOM可直接Throw/longjmp而不到fallback。void字段失败未必被检测，仍可能发布部分记录，无回滚。
 * Throw可longjmp，不能依赖本函数C局部清理；TLS仍是独立普通副本。
 * TODO: outer/nested raw记录和payload在字段、traceback分配到currentException发布前建立何种根，沿Exception normalize强制GC核查。
 * BUG: 本函数读取当前TLS；上游formatter失败Assert覆盖归因，成功Assert或显式ClearLastFailure清空记录，均不恢复外层失败。
 * BUG: descriptor将脚本exception声明为ValueSnapshot；本函数先投影快照，Normalize已有Error随后将同一字段改为self，普通成员读取返回AssertionFailure自身。
 * 宿主C TLS的exception仍是快照；TODO: 沿字段投影→Normalize→脚本failure.exception补schema回归并核查规范化协议。
 * TODO: 独立核查保存恢复策略；不能把临时根风险与已证TLS破坏混为同一结论。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:835;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:884;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1031;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1071;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:455;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:460;zr_vm_core/src/zr_vm_core/exception/exception.c:666;zr_vm_core/src/zr_vm_core/exception/exception.c:684;zr_vm_core/src/zr_vm_core/exception/exception.c:691;zr_vm_cli/src/zr_vm_cli/commands/test_command.c:570;zr_vm_core/src/zr_vm_core/base/memory.c:39;zr_vm_core/src/zr_vm_core/base/memory.c:46;zr_vm_core/src/zr_vm_core/gc/gc_object.c:1069;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:243;zr_vm_core/src/zr_vm_core/exception/exception.c:36;zr_vm_core/src/zr_vm_core/exception/exception.c:54;zr_vm_core/src/zr_vm_core/exception/exception.c:407;zr_vm_core/src/zr_vm_core/object/object.c:1846;zr_vm_core/src/zr_vm_core/object/object.c:1886;zr_vm_core/src/zr_vm_core/object/object.c:2393;zr_vm_core/src/zr_vm_core/object/object.c:2399
 */
static ZR_NO_RETURN void testing_raise_failure(ZrLibCallContext *context) {
    SZrState *state = context != ZR_NULL ? context->state : ZR_NULL;
    SZrObjectPrototype *prototype;
    SZrObject *object;
    SZrTypeValue payload;

    if (state != ZR_NULL) {
        prototype = ZrLib_Type_FindPrototype(state, "AssertionFailure");
        object = prototype != ZR_NULL
                 ? ZrLib_Type_NewInstanceWithPrototype(state, prototype)
                 : ZR_NULL;
/**
 * TODO: outer record跨nested构造分配；字段setter临时pin不覆盖全投影窗口，沿forced moving GC核查。
 */
        if (object != ZR_NULL) {
            testing_set_object_int(state, object, "assertionKind", g_last_failure.assertionKind);
            testing_set_object_string(state, object, "message", g_last_failure.message);
            testing_set_object_record(state, object, "sourceSpan",
                                      testing_build_source_span_object(state, &g_last_failure.sourceSpan));
            testing_set_object_record(state, object, "expected",
                                      testing_build_snapshot_object(state, &g_last_failure.expected));
            testing_set_object_record(state, object, "actual",
                                      testing_build_snapshot_object(state, &g_last_failure.actual));
            testing_set_object_record(state, object, "exception",
                                      testing_build_snapshot_object(state, &g_last_failure.exception));
            ZrLib_Value_SetObject(state, &payload, object, ZR_VALUE_TYPE_OBJECT);
/**
 * BUG: Normalize已有Error把exception覆为self，与Testing声明的ValueSnapshot不符；普通成员读取返回该self。
 * TODO: 宿主TLS快照不受此字段覆写影响；独立核查payload/object临时根、搬迁重取及脚本schema回归。
 */
            if (ZrCore_Exception_NormalizeThrownValue(
                        state, &payload, state->callInfoList, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
                ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
            }
        }
/**
 * 结构化记录未建立或normalize拒绝时走运行错误；TLS仍由宿主独立读取，不声称字段构造回滚。
 */
        ZrCore_Debug_RunError(
                state,
                "AssertionFailure: %s",
                g_last_failure.message[0] != '\0' ? g_last_failure.message : "assertion failed");
    }
    ZR_ABORT();
}

/**
 * ## Summary
 * 开启调用线程的一次新失败归因，先清旧快照再填分类、调用位置和有界消息。
 *
 * ## Parameters
 * @param[in] context 正常native上下文借用，可NULL时源码位置留零。
 * @param[in] kind 当前producer的ASSERT/EQUAL/THROWS分类，不是threadStatus。
 * @param[in] message 调用期有效的可选NUL文本，NULL按空消息处理。
 *
 * ## Returns
 * 无返回值；has标志为真，即使消息/位置为空也构成有效失败。
 *
 * ## Constraints and lifecycle
 * 复制普通值，不保活VM；同线程后续失败覆盖整条记录，成功断言/Clear会清理。
 * 消息按字节裁剪，UTF-8边界BUG见copy_bounded；同线程formatter重入不形成记录栈。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:831;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:881;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1024;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1056;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:746;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:747;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:749;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:750;zr_vm_lib_testing/include/zr_vm_lib_testing/module.h:177
 */
static void testing_begin_failure(ZrLibCallContext *context,
                                  EZrTestingAssertionKind kind,
                                  const TZrChar *message) {
    memset(&g_last_failure, 0, sizeof(g_last_failure));
    g_has_last_failure = ZR_TRUE;
    g_last_failure.assertionKind = kind;
    testing_source_span(context, &g_last_failure.sourceSpan);
    testing_copy_bounded(
            g_last_failure.message,
            sizeof(g_last_failure.message),
            message != ZR_NULL ? message : "",
            ZR_NULL);
}

/**
 * ## Summary
 * 关闭当前OS线程的诊断窗口，供CLI在用例开始前与报告完成后隔离旧失败归因。
 *
 * ## Returns
 * 无返回值；清零TLS并使Get失败，不改变VM currentException/threadStatus或已复制的宿主副本。
 *
 * ## Constraints and lifecycle
 * 无需VM，不清其它线程；同线程多个GlobalState共用窗口，三条成功断言路径也会调用。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:837;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:886;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1079;zr_vm_lib_testing/include/zr_vm_lib_testing/module.h:258
 */
void ZrVmLibTesting_ClearLastFailure(void) {
    memset(&g_last_failure, 0, sizeof(g_last_failure));
    g_has_last_failure = ZR_FALSE;
}

/**
 * ## Summary
 * 向宿主按值交付当前线程最近失败，允许报告脱离原VM对象生命周期而不消费TLS。
 *
 * ## Parameters
 * @param[out] outFailure 可写完整结构，NULL或无记录时保持原内容，成功时整体覆盖。
 *
 * ## Returns
 * @return 仅有效位为真且输出非NULL时返回TRUE；否则FALSE且不得把未写输出当有效记录。
 *
 * ## Constraints and lifecycle
 * 可重复读取；无需VM，不按GlobalState查找；内容都是普通内联值，无VM所有权或跨进程布局承诺。
 *
 * ## References
 * zr_vm_lib_testing/include/zr_vm_lib_testing/module.h:278
 */
TZrBool ZrVmLibTesting_GetLastFailure(SZrTestingAssertionFailure *outFailure) {
    if (!g_has_last_failure || outFailure == ZR_NULL) {
        return ZR_FALSE;
    }
    *outFailure = g_last_failure;
    return ZR_TRUE;
}

/**
 * ## Summary
 * 实现严格bool条件断言；失败先记录宿主归因再以VM异常返回脚本catch边界。
 *
 * ## Parameters
 * @param[in,out] context 正常descriptor派发的有效上下文/state，仅回调期间借用。
 * @param[out] result 派发者拥有的有效结果槽，正常成功写null，不转移参数/VM所有权。
 *
 * ## Returns
 * @return 参数合法且条件真时TRUE并清TLS；形状拒绝可FALSE；条件假经raise不返回普通FALSE。
 *
 * ## Constraints and lifecycle
 * arity为1或2，第二参存在时须string；读取消息后紧邻复制，不保留borrowed文本。
 * Test phase由注册/物化层控制；callback FALSE且线程Fine可被host export解释为完成/null，不能混淆两层bool。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:821;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:831;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:837;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:619;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1997
 */
TZrBool ZrVmLibTesting_Assert(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrBool condition;
    SZrString *message = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 1U, 2U) ||
        !ZrLib_CallContext_ReadBool(context, 0U, &condition)) {
        return ZR_FALSE;
    }
    if (ZrLib_CallContext_ArgumentCount(context) > 1U &&
        !ZrLib_CallContext_ReadString(context, 1U, &message)) {
        return ZR_FALSE;
    }
    if (!condition) {
        testing_begin_failure(
                context,
                ZR_TESTING_ASSERTION_KIND_ASSERT,
                message != ZR_NULL ? ZrCore_String_GetNativeString(message) : "");
        testing_raise_failure(context);
    }
    ZrVmLibTesting_ClearLastFailure();
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 先用VM canonical equality决定断言，仅在不等时生成expected/actual独立诊断而非以debug文本比较。
 *
 * ## Parameters
 * @param[in,out] context 正常descriptor派发的有效上下文/state，仅回调期间借用。
 * @param[out] result 派发者拥有的有效结果槽，正常成功写null，不转移参数/VM所有权。
 *
 * ## Returns
 * @return 相等正常TRUE并清TLS；参数缺失/形状拒绝可FALSE；不等开始失败、填快照并raise不返回。
 *
 * ## Constraints and lifecycle
 * 恰两个值；正常fixed-2 lane提供稳定C副本/托管pin，CompareDirectly/Value_Equal本身不执行TO_STRING或分配。
 * 诊断formatter可执行用户代码；formatterFaulted不改变既定不等结论。
 * BUG: TO_STRING内失败Assert重开TLS并Throw，外层随后投影内层ASSERT；成功Assert或显式ClearLastFailure清空外层记录，raise也不恢复EQUAL归因。
 * TODO: 沿expected→actual→raise补嵌套回归及保存恢复设计；尚未运行该fixture。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/value/value.c:127;zr_vm_core/src/zr_vm_core/value/value.c:275;zr_vm_core/src/zr_vm_core/value/value.c:600;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:243;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:320;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:746;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:747;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:772;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:835;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:837;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:882;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:883;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:884;tests/testing/test_assertions.c:387
 */
TZrBool ZrVmLibTesting_Equal(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *actual;
    SZrTypeValue *expected;

    if (context == ZR_NULL || result == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 2U, 2U)) {
        return ZR_FALSE;
    }
    actual = ZrLib_CallContext_Argument(context, 0U);
    expected = ZrLib_CallContext_Argument(context, 1U);
    if (actual == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrCore_Value_CompareDirectly(context->state, actual, expected) &&
        !ZrCore_Value_Equal(context->state, actual, expected)) {
/**
 * BUG: 两次formatter共用TLS；内层失败Assert覆盖归因，成功Assert或显式ClearLastFailure清空记录；后续raise不会恢复外层归因。
 * TODO: 添加嵌套回归后核查TLS保存恢复。
 */
        testing_begin_failure(context, ZR_TESTING_ASSERTION_KIND_EQUAL, "values are not equal");
        testing_snapshot(context->state, expected, &g_last_failure.expected);
        testing_snapshot(context->state, actual, &g_last_failure.actual);
        testing_raise_failure(context);
    }
    ZrVmLibTesting_ClearLastFailure();
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 在TryRun边界内同步调用throws action，将宿主完成状态与VM异常分开交给外层判断。
 *
 * ## Parameters
 * @param[in,out] state 当前有效VM状态。
 * @param[in,out] arguments 指向本次局部CallRequest，callable只借用，result由CallValue初始化并写入。
 *
 * ## Returns
 * 无返回值；completed接收host CallValue完成状态，不等同action callback bool。
 *
 * ## Constraints and lifecycle
 * 无receiver/无参数，内部再把callable复制进scratch；不等待Task，不跨同步调用保存request。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1017;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1018;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:910;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1781;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1813;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1861;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1909
 */
static void testing_call_try(SZrState *state, TZrPtr arguments) {
    SZrTestingCallRequest *request = (SZrTestingCallRequest *)arguments;
    request->completed = ZrLib_CallValue(
            state, request->callable, ZR_NULL, ZR_NULL, 0U, &request->result);
}

/**
 * ## Summary
 * 认证编译器隐藏的E TypeId并核对Error继承链，避免在无合法预期异常类型时执行action。
 *
 * ## Parameters
 * @param[in] context 正常native context/state借用；arg1须为已认证object TypeId。
 * @param[out] outTypeName 可写borrowed字符串槽；入口清NULL，认证成功后可在后续继承检查失败时仍非NULL。
 *
 * ## Returns
 * @return 仅TypeId认证、原型查找与Error继承匹配全部成功时TRUE；FALSE时调用者不得使用输出。
 *
 * ## Constraints and lifecycle
 * 正常fixed-2 lane pin TypeId，Throws仍在action后重读；prototype永久不等同canonical-name普通借用。
 * TODO: ReadTypeId返回name后FindPrototype可能分配，需沿reflection输出契约及type/string compaction核查地址稳定和重取。
 * Error原型缺失时按名称查找；FALSE不保证清除已有VM异常，不能承诺所有失败输出仍NULL。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1012;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1020;zr_vm_core/src/zr_vm_core/reflection/reflection_type_identity.c:249;zr_vm_core/src/zr_vm_core/reflection/reflection_type_identity.c:252;zr_vm_core/src/zr_vm_core/reflection/reflection_type_identity.c:320;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:955;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:962;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:966;tests/testing/test_assertions.c:337;tests/testing/test_assertions.c:356
 */
static TZrBool testing_read_expected_type_name(ZrLibCallContext *context,
                                               SZrString **outTypeName) {
    SZrTypeValue *typeValue;
    SZrObject *typeObject;
    SZrReflectionTypeIdentity identity;
    SZrObjectPrototype *prototype;
    SZrObjectPrototype *errorPrototype;
    const TZrChar *typeNameText;

    if (outTypeName != ZR_NULL) {
        *outTypeName = ZR_NULL;
    }
    if (context == ZR_NULL || outTypeName == ZR_NULL) {
        return ZR_FALSE;
    }
    typeValue = ZrLib_CallContext_Argument(context, 1U);
    if (typeValue == ZR_NULL || typeValue->type != ZR_VALUE_TYPE_OBJECT ||
        typeValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    typeObject = ZR_CAST_OBJECT(context->state, typeValue->value.object);
    memset(&identity, 0, sizeof(identity));
    if (!ZrCore_Reflection_ReadTypeIdObject(
                context->state, typeObject, &identity, outTypeName) ||
        *outTypeName == ZR_NULL) {
        return ZR_FALSE;
    }
    typeNameText = ZrCore_String_GetNativeString(*outTypeName);
    prototype = typeNameText != ZR_NULL
                        ? ZrLib_Type_FindPrototype(context->state, typeNameText)
                        : ZR_NULL;
    errorPrototype = context->state->global->errorPrototype;
    if (errorPrototype == ZR_NULL) {
        errorPrototype = ZrLib_Type_FindPrototype(context->state, "Error");
    }
    if (errorPrototype == ZR_NULL) {
        return ZR_FALSE;
    }
    while (prototype != ZR_NULL && prototype != errorPrototype) {
        prototype = prototype->superPrototype;
    }
    return prototype == errorPrototype;
}

/**
 * ## Summary
 * 同步运行action，捕获Error派生E并成功交付异常值；未抛出或类型不符另产生AssertionFailure。
 *
 * ## Parameters
 * @param[in,out] context 正常派发上下文；native恰两个参数：arg0无参callable、arg1编译器隐藏TypeId E。
 * @param[out] result 已初始化且遵循VM Value_Copy协议的派发结果槽；匹配后复制异常，不是普通诊断副本。
 *
 * ## Returns
 * @return 捕获并匹配E时TRUE；参数/TypeId/normalize拒绝可FALSE；未抛或错型通过raise不返回。
 *
 * ## Constraints and lifecycle
 * pre-read E→受保护action→post-read E；Fine且completed表示正常结束。缺currentException时按status规范化，随后线程设Fine。
 * 快照是诊断副作用；错配先保存exception快照再begin重置TLS，清VM异常后抛断言失败。
 * 匹配先Value_Copy交付再ClearCurrent/Fine/ClearTLS；同步、不等待Task，host bool仍受dispatcher完成协议影响。
 * BUG: 在公开AssertionFailure原型通过ObjectPrototype_AddMeta安装抛错TO_STRING，合法throws<AssertionFailure>(assert(false))先由Normalize写入exception自字段。
 * DebugString遍历该字段触发元方法异常，snapshot清currentException，随后CatchMatches(null)为false，把本来匹配的异常判成错型。
 * 即便TryRun状态Fine，只要formatter返回NULL文本也会清原异常；该正常无文本路径与元方法Throw分开。
 * TODO: 补合法formatter故障及无文本回归并核查异常保存恢复；本轮只记录静态链，未执行组合fixture。
 * TODO: canonical-name跨formatter分配和TLS重入稳定性沿read_expected_type_name/testing_snapshot核查。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1017;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1020;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1033;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1048;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1052;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:1076;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:619;zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1997;zr_vm_core/src/zr_vm_core/exception/exception.c:407;zr_vm_core/src/zr_vm_core/exception/exception.c:684;zr_vm_core/src/zr_vm_core/exception/exception.c:691;zr_vm_core/src/zr_vm_core/value/value.c:329;zr_vm_core/src/zr_vm_core/value/value.c:600;zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:460;tests/testing/test_assertions.c:250;tests/testing/test_assertions.c:432;tests/testing/test_assertions.c:433;zr_vm_core/include/zr_vm_core/object.h:525;zr_vm_core/src/zr_vm_core/object/object.c:2208
 */
TZrBool ZrVmLibTesting_Throws(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *callable;
    SZrString *expectedTypeName;
    SZrTestingCallRequest request;
    EZrThreadStatus status;

    if (context == ZR_NULL || result == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 2U, 2U) ||
        !ZrLib_CallContext_ReadFunction(context, 0U, &callable)) {
        return ZR_FALSE;
    }
    if (!testing_read_expected_type_name(context, &expectedTypeName)) {
        return ZR_FALSE;
    }
    memset(&request, 0, sizeof(request));
    request.callable = callable;
    status = testing_try_run_preserving_frame(
            context->state, testing_call_try, &request);
    /* 被调用动作可能触发 GC 并搬迁托管参数；重新从调用上下文读取隐藏 TypeId。 */
    if (!testing_read_expected_type_name(context, &expectedTypeName)) {
        return ZR_FALSE;
    }
    if (status == ZR_THREAD_STATUS_FINE && request.completed) {
        testing_begin_failure(
                context, ZR_TESTING_ASSERTION_KIND_THROWS, "action completed without throwing");
        g_last_failure.expected.hasValue = ZR_TRUE;
        testing_copy_bounded(g_last_failure.expected.typeName,
                             sizeof(g_last_failure.expected.typeName),
                             ZrCore_String_GetNativeString(expectedTypeName),
                             ZR_NULL);
        testing_raise_failure(context);
    }
    if (!context->state->hasCurrentException) {
        if (!ZrCore_Exception_NormalizeStatus(
                    context->state,
                    status != ZR_THREAD_STATUS_FINE
                            ? status
                            : ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
            return ZR_FALSE;
        }
    }
    context->state->threadStatus = ZR_THREAD_STATUS_FINE;
/**
 * BUG: 公开AddMeta可为AssertionFailure安装抛错TO_STRING；正常throws<AssertionFailure>(assert(false))的Normalize把exception写为self。
 * DebugString字段转换触发该元方法，snapshot清currentException后CatchMatches(null)拒绝原本匹配的异常。
 * TODO: 沿公开AddMeta/现有test_formatter_throws与编译throws fixture增加回归；本轮未执行该组合。
 */
    testing_snapshot(
            context->state,
            &context->state->currentException,
            &g_last_failure.exception);
    if (!ZrCore_Exception_CatchMatchesTypeName(context->state,
                                               &context->state->currentException,
                                               expectedTypeName)) {
        SZrTestingValueSnapshot exceptionSnapshot = g_last_failure.exception;
        testing_begin_failure(context,
                              ZR_TESTING_ASSERTION_KIND_THROWS,
                              "thrown exception type does not match E");
        g_last_failure.expected.hasValue = ZR_TRUE;
        testing_copy_bounded(g_last_failure.expected.typeName,
                             sizeof(g_last_failure.expected.typeName),
                             ZrCore_String_GetNativeString(expectedTypeName),
                             ZR_NULL);
        testing_copy_bounded(g_last_failure.expected.text,
                             sizeof(g_last_failure.expected.text),
                             ZrCore_String_GetNativeString(expectedTypeName),
                             ZR_NULL);
        g_last_failure.exception = exceptionSnapshot;
        ZrCore_Exception_ClearCurrent(context->state);
        context->state->threadStatus = ZR_THREAD_STATUS_FINE;
        testing_raise_failure(context);
    }
/**
 * 先复制到派发者结果槽，再清currentException与TLS；普通C诊断副本没有这种VM值所有权。
 */
    ZrCore_Value_Copy(context->state, result, &context->state->currentException);
    ZrCore_Exception_ClearCurrent(context->state);
    context->state->threadStatus = ZR_THREAD_STATUS_FINE;
    ZrVmLibTesting_ClearLastFailure();
    return ZR_TRUE;
}
