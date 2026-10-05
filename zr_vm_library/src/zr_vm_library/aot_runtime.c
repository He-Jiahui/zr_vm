/* AOT 运行适配层把生成库的静态 ABI 注册、项目模块缓存和 core 的物理调用协议连接起来。
 * descriptor/thunk 借用装载库；VM 函数、模块由记录 pin；生成 frame/context 只借用记录和栈视图。
 * GC root 由生成序言/epilogue 和 core 调用边界管理，本层 pin 不能替代栈根或 owner 生命周期。
 * 值、泛型字典及部分返回入口在 aot_runtime/ 独立编译单元实现；此文件统一装载和共享帧协议。
 */
#include "zr_vm_library/aot_runtime.h"
#include "aot_typed_call_binding.h"

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/bridge.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/math.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/property_reference.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"

/* 借用 core 的通用 callable 桥接入口；缓存访问器将已解析的 receiver/arguments 交给它，
 * core 负责原生/VM 调用和结果锚点。本声明只在适配实现内连接，不扩展公开 object header。 */
ZR_CORE_API TZrBool ZrCore_Object_CallValue(SZrState *state,
                                            const SZrTypeValue *callable,
                                            const SZrTypeValue *receiver,
                                            const SZrTypeValue *arguments,
                                            TZrSize argumentCount,
                                            SZrTypeValue *result);
#include "aot_runtime/aot_runtime_cleanup_registration.h"
#include "aot_runtime/aot_runtime_internal.h"

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* 每个模块记录把动态库句柄、描述符、VM 函数图和 GC pin 绑定在同一生命周期；
 * 项目释放时先撤 pin，再等全局 GC 后回调关闭库，避免仍可达的 native thunk 指向卸载代码。
 * TODO: 核查合法嵌套 AOT 导入在第 5/9 个记录追加时跨 records 扩容，并在返回后继续消费 frame/context/activeRecord 的完整调用链；若 realloc 迁移数组，旧元素借用地址会失效，须验证实际可达路径及后续重定位责任，尚无完整合法触发或实测证明。 */

typedef struct SZrLibraryAotLoadedModule {
    /* 记录所属后端，参与模块缓存键和执行后端报告。 */
    EZrAotBackendKind backendKind;
    /* 记录拥有的标准化缓存键字符串。 */
    TZrChar *moduleName;
    /* 记录拥有的伴随源码路径，可无匹配文件。 */
    TZrChar *sourcePath;
    /* 记录拥有的伴随二进制输入路径。 */
    TZrChar *zroPath;
    /* 记录拥有的实际装载库路径。 */
    TZrChar *libraryPath;
    /* 本记录拥有的动态库句柄，释放后移交延迟关闭队列。 */
    void *libraryHandle;
    /* 借用动态库静态 descriptor，库卸载后失效。 */
    const ZrAotCompiledModule *descriptor;
    /* 借用动态库静态 ABI 注册数据，供布局、绑定及 import 查询。 */
    const SZrAotCodeRegistration *codeRegistration;
    /* 经 artifact loader 装载的根 VM 函数。 */
    SZrFunction *moduleFunction;
    /* 记录拥有 native 函数指针数组；其中 VM 函数由 functionPins 保持可达，与 thunk 同索引。 */
    SZrFunction **functionTable;
    /* 记录拥有的逐函数 pin 凭据数组，失败和析构须撤销。 */
    SZrGcNativeCallPin *functionPins;
    /* 函数表有效条目数，不是分配容量。 */
    TZrUInt32 functionCount;
    /* 函数表 native 分配容量，释放时按此大小归还。 */
    TZrUInt32 functionCapacity;
    /* 记录拥有的各 flat 函数物理槽数量表。 */
    TZrUInt32 *generatedFrameSlotCounts;
    /* 借用已装载模块对象，项目记录持有其 GC pin。 */
    SZrObjectModule *module;
    /* 模块对象的 pin 凭据，配对 Unpin 而非直接删除 GC 标记。 */
    SZrGcNativeCallPin modulePin;
    /* 记录自身的模块完成状态；成功发布 exports 后置真，不是借用指针。 */
    TZrBool moduleExecuted;
} SZrLibraryAotLoadedModule;

/* project->aotRuntime 拥有已载入记录；ConfigureGlobal 安装的 core loader 仅借用本状态。 */
typedef struct SZrLibraryAotRuntimeState {
    /* 宿主最近配置的请求模式，与实际执行后端分开。 */
    EZrLibraryProjectExecutionMode configuredExecutionMode;
    /* 最近记录已进入的后端，配置时清回 NONE。 */
    EZrLibraryExecutedVia executedVia;
    /* 保留配置请求；TODO：核查消费者并明确当前未读字段的执行策略。 */
    TZrBool requireAotPath;
    /* 由 AOT_C/AOT_LLVM 模式决定的严格装载标志。 */
    TZrBool strictProjectAot;
    /* 固定诊断缓冲，借用者不得保存跨后续操作的内容假设。 */
    TZrChar lastError[ZR_LIBRARY_MAX_PATH_LENGTH];
    /* 项目拥有的可迁移数组；活跃 frame 不得假设地址长期稳定。 */
    SZrLibraryAotLoadedModule *records;
    /* 已发布记录数，只在追加完成后增长。 */
    TZrSize recordCount;
    /* records 分配容量，扩容会迁移数组。 */
    TZrSize recordCapacity;
    /* 同步 thunk/shim 派发借用的活动记录，嵌套调用保存再恢复。 */
    SZrLibraryAotLoadedModule *activeRecord;
} SZrLibraryAotRuntimeState;

/* 全局 GC 后清理回调持有的库句柄队列，与项目记录分离以延迟 dlclose/FreeLibrary。 */
typedef struct SZrLibraryAotRetiredLibraries {
    /* 延迟关闭队列拥有的 native 句柄数组。 */
    void **handles;
    /* 队列已持有句柄数量。 */
    TZrSize count;
    /* 队列分配容量，供追加及释放使用。 */
    TZrSize capacity;
} SZrLibraryAotRetiredLibraries;

/* TryRun 异常边界内的入口调用参数；record 和 result 均由 ExecuteEntry 在调用期间持有。 */
typedef struct ZrLibraryAotEntryRequest {
    /* TryRun 同步借用的项目 AOT 状态。 */
    SZrLibraryAotRuntimeState *runtimeState;
    /* TryRun 同步借用的待执行记录，不能逃逸到异步回调。 */
    SZrLibraryAotLoadedModule *record;
    /* ExecuteEntry 调用方提供的 Value 输出地址，仅在调用期间借用。 */
    SZrTypeValue *result;
    /* 入口 body 完成后交回 TryRun 外层的布尔状态。 */
    TZrBool success;
} ZrLibraryAotEntryRequest;

/* 内部浮点运算分派标签；只由已选 lowering 的算术 helper 使用，不作为持久 ABI 编号。 */
typedef enum EZrAotRuntimeFloatBinaryOp {
    ZR_AOT_RUNTIME_FLOAT_BINARY_ADD = 0,
    ZR_AOT_RUNTIME_FLOAT_BINARY_SUB,
    ZR_AOT_RUNTIME_FLOAT_BINARY_MUL,
    ZR_AOT_RUNTIME_FLOAT_BINARY_DIV,
    ZR_AOT_RUNTIME_FLOAT_BINARY_MOD,
    ZR_AOT_RUNTIME_FLOAT_BINARY_POW
} EZrAotRuntimeFloatBinaryOp;

/* 内部浮点关系分派标签；由四种比较 helper 共享数值提取和失败边界。 */
typedef enum EZrAotRuntimeCompareOp {
    ZR_AOT_RUNTIME_COMPARE_GREATER = 0,
    ZR_AOT_RUNTIME_COMPARE_LESS,
    ZR_AOT_RUNTIME_COMPARE_GREATER_EQUAL,
    ZR_AOT_RUNTIME_COMPARE_LESS_EQUAL
} EZrAotRuntimeCompareOp;

/* 以下前置声明连接装载、捕获物化、生成帧和作用域 helper；它们不建立新的 owner 或根。
 * 对外共享的无 static helper 由 aot_runtime_internal.h 供独立单元使用，其契约以定义为准。
 */
static SZrLibrary_Project *aot_runtime_get_project(SZrGlobalState *global);
static SZrLibraryAotRuntimeState *aot_runtime_get_state_from_project(SZrLibrary_Project *project);
SZrLibraryAotRuntimeState *aot_runtime_get_state_from_global(SZrGlobalState *global);
static SZrLibraryAotRuntimeState *aot_runtime_ensure_state(SZrGlobalState *global);
static TZrPtr aot_runtime_reallocate(SZrGlobalState *global, TZrPtr pointer, TZrSize originalSize, TZrSize newSize);
static TZrChar *aot_runtime_duplicate_string(SZrGlobalState *global, const TZrChar *text);
static void aot_runtime_free_string(SZrGlobalState *global, TZrChar *text);
static void aot_runtime_set_error(SZrLibraryAotRuntimeState *runtimeState, const TZrChar *format, ...);
void aot_runtime_fail(SZrState *state, SZrLibraryAotRuntimeState *runtimeState, const TZrChar *format, ...);
static TZrBool aot_runtime_normalize_module_name(const TZrChar *moduleName, TZrChar *buffer, TZrSize bufferSize);
static TZrBool aot_runtime_resolve_module_file(const SZrLibrary_Project *project,
                                               const TZrChar *rootDirectory,
                                               const TZrChar *moduleName,
                                               const TZrChar *extension,
                                               TZrChar *buffer,
                                               TZrSize bufferSize);
static void aot_runtime_sanitize_module_name(const TZrChar *moduleName, TZrChar *buffer, TZrSize bufferSize);
static const TZrChar *aot_runtime_dynamic_library_extension(void);
static TZrBool aot_runtime_resolve_library_path(const SZrLibrary_Project *project,
                                                EZrAotBackendKind backendKind,
                                                const TZrChar *moduleName,
                                                TZrChar *buffer,
                                                TZrSize bufferSize);
static TZrBool aot_runtime_validate_descriptor(SZrState *state,
                                               SZrLibraryAotRuntimeState *runtimeState,
                                               const ZrAotCompiledModule *descriptor,
                                               EZrAotBackendKind backendKind,
                                               const TZrChar *normalizedModule);
static TZrBool aot_runtime_hash_file(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize);
static void *aot_runtime_open_library(const TZrChar *path);
static void aot_runtime_close_library(void *handle);
static TZrBool aot_runtime_retire_library(SZrGlobalState *global, void *handle);
static TZrPtr aot_runtime_find_symbol(void *handle, const TZrChar *symbolName);
static FZrVmGetAotCompiledModule aot_runtime_cast_descriptor_symbol(TZrPtr symbolPointer);
static SZrLibraryAotLoadedModule *aot_runtime_find_record(SZrLibraryAotRuntimeState *runtimeState,
                                                          EZrAotBackendKind backendKind,
                                                          const TZrChar *moduleName);
/* 已验证模块按需加入项目缓存，outRecord 指向缓存数组元素。 */
static TZrBool aot_runtime_append_record(SZrGlobalState *global,
                                         SZrLibraryAotRuntimeState *runtimeState,
                                         const SZrLibraryAotLoadedModule *record,
                                         SZrLibraryAotLoadedModule **outRecord);
static TZrBool aot_runtime_load_zro_function(SZrState *state, const TZrChar *zroPath, SZrFunction **outFunction);
static TZrBool aot_runtime_load_embedded_function(SZrState *state,
                                                  const TZrByte *blob,
                                                  TZrSize blobLength,
                                                  SZrFunction **outFunction);
static TZrBool aot_runtime_build_function_table(SZrState *state,
                                                SZrFunction *function,
                                                SZrFunction ***outFunctions,
                                                TZrUInt32 *outCount,
                                                TZrUInt32 *outCapacity);
static void aot_runtime_rebind_function_table_constants(SZrState *state,
                                                        SZrFunction *const *functionTable,
                                                        TZrUInt32 functionCount);
static TZrBool aot_runtime_build_generated_slot_count_table(SZrGlobalState *global,
                                                            SZrFunction *const *functionTable,
                                                            TZrUInt32 functionCount,
                                                            TZrUInt32 **outSlotCounts);
static SZrGcNativeCallPin *aot_runtime_pin_function_table(SZrState *state,
                                                         SZrFunction *const *functionTable,
                                                         TZrUInt32 functionCount);
static void aot_runtime_unpin_function_table(SZrState *state,
                                             SZrGcNativeCallPin *functionPins,
                                             TZrUInt32 functionCount);
static const TZrChar *aot_runtime_metadata_binding_status_name(
        EZrMetadataRuntimeBindingCompatibilityStatus status);
static TZrBool aot_runtime_validate_metadata_bindings(SZrState *state,
                                                      SZrLibraryAotRuntimeState *runtimeState,
                                                      const TZrChar *moduleName,
                                                      const SZrFunction *moduleFunction,
                                                      SZrFunction *const *functionTable,
                                                      TZrUInt32 functionCount);
static SZrTypeValue *aot_runtime_get_closure_capture_from_value(SZrState *state,
                                                                const SZrTypeValue *closureContainerValue,
                                                                TZrUInt32 captureIndex);
static TZrBool aot_runtime_bind_native_closure_captures_from_source(SZrState *state,
                                                                    SZrClosureNative *destinationClosure,
                                                                    const SZrTypeValue *source,
                                                                    TZrUInt32 captureCount);
static TZrBool aot_runtime_bind_native_closure_captures_from_frame(SZrState *state,
                                                                   const ZrAotGeneratedFrame *frame,
                                                                   SZrClosureNative *destinationClosure,
                                                                   SZrFunction *metadataFunction);
static TZrBool aot_runtime_materialize_callable_constant_with_context(SZrState *state,
                                                                      SZrLibraryAotLoadedModule *record,
                                                                      const SZrTypeValue *source,
                                                                      const ZrAotGeneratedFrame *frame,
                                                                      TZrBool forceClosure,
                                                                      SZrTypeValue *destination);
static TZrBool aot_runtime_materialize_callable_constant(SZrState *state,
                                                         SZrLibraryAotLoadedModule *record,
                                                         const SZrTypeValue *source,
                                                         TZrBool forceClosure,
                                                         SZrTypeValue *destination);
static TZrUInt32 aot_runtime_find_function_index_in_record(const SZrLibraryAotLoadedModule *record,
                                                           const SZrFunction *function);
static SZrLibraryAotLoadedModule *aot_runtime_find_record_for_function(SZrLibraryAotRuntimeState *runtimeState,
                                                                       const SZrFunction *function);
static TZrBool aot_runtime_record_try_get_generated_slot_count(const SZrLibraryAotLoadedModule *record,
                                                               TZrUInt32 functionIndex,
                                                               TZrUInt32 *outSlotCount);
static const SZrFunction *aot_runtime_frame_function(const ZrAotGeneratedFrame *frame);
static TZrStackValuePointer aot_runtime_frame_slot(const ZrAotGeneratedFrame *frame, TZrUInt32 slotIndex);
/* 元方法调用为 callable 与实参向 VM 借临时栈位；ReserveScratchSlots 可能迁移栈，
 * 因此返回后只能使用刷新过的 frame->slotBase 和 callBase。 */
static TZrBool aot_runtime_reserve_temp_call_base(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 scratchSlotCount,
                                                  TZrStackValuePointer *outCallBase,
                                                  TZrUInt32 *outFunctionSlot);
static TZrBool aot_runtime_refresh_frame_from_callinfo(SZrState *state, ZrAotGeneratedFrame *frame, SZrCallInfo *callInfo);
static SZrTypeValue *aot_runtime_refresh_destination_after_meta(SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                SZrLibraryAotRuntimeState *runtimeState,
                                                                const TZrChar *operationName);
static TZrBool aot_runtime_frame_resume_index(const ZrAotGeneratedFrame *frame, SZrCallInfo *callInfo, TZrUInt32 *outIndex);
static TZrBool aot_runtime_resume_pending_control_in_current_frame(SZrState *state,
                                                                   ZrAotGeneratedFrame *frame,
                                                                   TZrUInt32 *outResumeInstructionIndex);
static TZrBool aot_runtime_resolve_current_closure_capture(SZrState *state,
                                                           const ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 closureIndex,
                                                           SZrTypeValue **outClosureValue,
                                                           SZrRawObject **outBarrierObject);
static TZrBool aot_runtime_resolve_member_symbol(const SZrFunction *function,
                                                 TZrUInt32 memberId,
                                                 SZrString **outSymbol);
static TZrBool aot_runtime_execute_vm_shim_direct(SZrState *state,
                                                  SZrFunction *function,
                                                  TZrStackValuePointer *outResultBase);
static TZrBool aot_runtime_materialize_exports(SZrState *state,
                                               SZrLibraryAotLoadedModule *record,
                                               TZrStackValuePointer slotBase);
static TZrBool aot_runtime_call_record_direct(SZrState *state,
                                              SZrLibraryAotRuntimeState *runtimeState,
                                              SZrLibraryAotLoadedModule *record,
                                              TZrBool captureResult,
                                              SZrTypeValue *result);
static void aot_runtime_mark_record_executed(SZrLibraryAotRuntimeState *runtimeState,
                                             const SZrLibraryAotLoadedModule *record);
static EZrLibraryExecutedVia aot_runtime_backend_to_executed_via(EZrAotBackendKind backendKind);
static const TZrChar *aot_runtime_backend_diagnostic_name(EZrAotBackendKind backendKind);
static void aot_runtime_report_module_load_failure(SZrState *state,
                                                   const SZrLibraryAotRuntimeState *runtimeState,
                                                   EZrAotBackendKind backendKind,
                                                   SZrString *moduleName,
                                                   const TZrChar *result);
static TZrBool aot_runtime_prepare_record(SZrState *state,
                                          SZrLibraryAotRuntimeState *runtimeState,
                                          EZrAotBackendKind backendKind,
                                          const TZrChar *moduleName,
                                          SZrLibraryAotLoadedModule **outRecord);
static void aot_runtime_bind_record_code_registration(SZrLibraryAotLoadedModule *record,
                                                      const ZrAotCompiledModule *descriptor);
static void aot_runtime_execute_entry_body(SZrState *state, TZrPtr arguments);
static void aot_runtime_resolve_observation_policy(const SZrState *state,
                                                   TZrUInt32 *outObservationMask,
                                                   TZrBool *outPublishAllInstructions);
static TZrBool aot_runtime_value_is_truthy(SZrState *state, const SZrTypeValue *value);
static TZrBool aot_runtime_extract_numeric_double(const SZrTypeValue *value, TZrFloat64 *outValue);
static TZrBool aot_runtime_extract_integer_like_value(const SZrTypeValue *value, TZrInt64 *outValue);
static TZrBool aot_runtime_extract_unsigned_integer_like_value(const SZrTypeValue *value, TZrUInt64 *outValue);
static TZrBool aot_runtime_eval_binary_numeric_float(EZrAotRuntimeFloatBinaryOp operation,
                                                     TZrFloat64 leftValue,
                                                     TZrFloat64 rightValue,
                                                     TZrFloat64 *outResult);
static TZrBool aot_runtime_eval_binary_numeric_compare(EZrAotRuntimeCompareOp operation,
                                                       TZrFloat64 leftValue,
                                                       TZrFloat64 rightValue,
                                                       TZrBool *outResult);
static TZrSize aot_runtime_close_scope_registrations(SZrState *state, TZrSize cleanupCount);
static TZrBool aot_runtime_apply_float_binary_operation(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot,
                                                        EZrAotRuntimeFloatBinaryOp operation,
                                                        const TZrChar *instructionName);
static TZrBool aot_runtime_apply_float_compare_operation(SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot,
                                                         EZrAotRuntimeCompareOp operation,
                                                         const TZrChar *instructionName);
static TZrBool aot_runtime_call_temp_base_without_yield(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrStackValuePointer callBase,
                                                        TZrUInt32 argumentCount);
static TZrBool aot_runtime_invoke_binary_meta(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              const SZrTypeValue *leftValue,
                                              const SZrTypeValue *rightValue,
                                              SZrFunction *metaFunction);

static SZrLibrary_Project *aot_runtime_get_project(SZrGlobalState *global) {
    return (global != ZR_NULL && global->userData != ZR_NULL) ? (SZrLibrary_Project *)global->userData : ZR_NULL;
}

static SZrLibraryAotRuntimeState *aot_runtime_get_state_from_project(SZrLibrary_Project *project) {
    return project != ZR_NULL ? (SZrLibraryAotRuntimeState *)project->aotRuntime : ZR_NULL;
}

SZrLibraryAotRuntimeState *aot_runtime_get_state_from_global(SZrGlobalState *global) {
    return aot_runtime_get_state_from_project(aot_runtime_get_project(global));
}

static TZrPtr aot_runtime_reallocate(SZrGlobalState *global, TZrPtr pointer, TZrSize originalSize, TZrSize newSize) {
    return (global == ZR_NULL || global->allocator == ZR_NULL)
                   ? ZR_NULL
                   : global->allocator(global->userAllocationArguments,
                                       pointer,
                                       originalSize,
                                       newSize,
                                       ZR_MEMORY_NATIVE_TYPE_PROJECT);
}

/* 让记录拥有脱离临时路径缓冲的字符串副本；分配失败返回null；配对free_string并保留终止符。 */
static TZrChar *aot_runtime_duplicate_string(SZrGlobalState *global, const TZrChar *text) {
    TZrSize length;
    TZrChar *copy;

    if (global == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    length = strlen(text);
    copy = (TZrChar *)aot_runtime_reallocate(global, ZR_NULL, 0, length + 1);
    if (copy != ZR_NULL) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

static void aot_runtime_free_string(SZrGlobalState *global, TZrChar *text) {
    if (global != ZR_NULL && text != ZR_NULL) {
        aot_runtime_reallocate(global, text, strlen(text) + 1, 0);
    }
}

/* 保存装载诊断，允许无产物的探测路径清空最近错误；不修改threadStatus；文本固定缓冲并可被后续操作覆盖。 */
static void aot_runtime_set_error(SZrLibraryAotRuntimeState *runtimeState, const TZrChar *format, ...) {
    va_list arguments;

    if (runtimeState == ZR_NULL) {
        return;
    }

    runtimeState->lastError[0] = '\0';
    if (format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    vsnprintf(runtimeState->lastError, sizeof(runtimeState->lastError), format, arguments);
    va_end(arguments);
}

/* 把生成helper失败同时暴露为项目诊断与VM错误状态；只在FINE时改threadStatus，保留已有异常种类；不抛出、不清理帧。 */
void aot_runtime_fail(SZrState *state, SZrLibraryAotRuntimeState *runtimeState, const TZrChar *format, ...) {
    va_list arguments;

    if (runtimeState != ZR_NULL) {
        runtimeState->lastError[0] = '\0';
        if (format != ZR_NULL) {
            va_start(arguments, format);
            vsnprintf(runtimeState->lastError, sizeof(runtimeState->lastError), format, arguments);
            va_end(arguments);
        }
    }

    if (state != ZR_NULL && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
    }
}

/* 项目首次配置 AOT 时分配唯一运行状态；通过 project->aotRuntime 被所有导入回调复用。 */
static SZrLibraryAotRuntimeState *aot_runtime_ensure_state(SZrGlobalState *global) {
    SZrLibrary_Project *project;
    SZrLibraryAotRuntimeState *runtimeState;

    project = aot_runtime_get_project(global);
    if (project == ZR_NULL) {
        return ZR_NULL;
    }

    runtimeState = aot_runtime_get_state_from_project(project);
    if (runtimeState != ZR_NULL) {
        return runtimeState;
    }

    runtimeState = (SZrLibraryAotRuntimeState *)aot_runtime_reallocate(global, ZR_NULL, 0, sizeof(*runtimeState));
    if (runtimeState == ZR_NULL) {
        return ZR_NULL;
    }

    memset(runtimeState, 0, sizeof(*runtimeState));
    runtimeState->configuredExecutionMode = ZR_LIBRARY_PROJECT_EXECUTION_MODE_INTERP;
    runtimeState->executedVia = ZR_LIBRARY_EXECUTED_VIA_NONE;
    project->aotRuntime = runtimeState;
    return runtimeState;
}

/* 模块名规范化供记录缓存与产物定位共用，不能把无效或过长名称误认为别的模块。 */
/* TODO: 核查合法模块导入允许的名称长度与本规范化缓冲边界；循环到容量上限后当前仍返回成功，需验证超长输入能否通过模块名构建进入 prepare_record，并造成记录或产物身份混淆，尚无完整合法触发证明。 */

static TZrBool aot_runtime_normalize_module_name(const TZrChar *moduleName, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize length;
    TZrSize writeIndex = 0;

    if (moduleName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    length = strlen(moduleName);
    while (length > 0 && (moduleName[length - 1] == '/' || moduleName[length - 1] == '\\')) {
        length--;
    }
    if (length >= ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH &&
        memcmp(moduleName + length - ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH,
               ZR_VM_BINARY_MODULE_FILE_EXTENSION,
               ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH) == 0) {
        length -= ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH;
    } else if (length >= ZR_VM_SOURCE_MODULE_FILE_EXTENSION_LENGTH &&
               memcmp(moduleName + length - ZR_VM_SOURCE_MODULE_FILE_EXTENSION_LENGTH,
                      ZR_VM_SOURCE_MODULE_FILE_EXTENSION,
                      ZR_VM_SOURCE_MODULE_FILE_EXTENSION_LENGTH) == 0) {
        length -= ZR_VM_SOURCE_MODULE_FILE_EXTENSION_LENGTH;
    }

    while (length > 0 && (*moduleName == '/' || *moduleName == '\\')) {
        moduleName++;
        length--;
    }
    if (length == 0) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < length && writeIndex + 1 < bufferSize; index++) {
        buffer[writeIndex++] = moduleName[index] == '\\' ? '/' : moduleName[index];
    }
    if (writeIndex == 0 || writeIndex + 1 > bufferSize) {
        return ZR_FALSE;
    }
    buffer[writeIndex] = '\0';
    return ZR_TRUE;
}

/* TODO: PathJoin 不返回截断状态；需确认项目根路径与规范化模块名拼接超过上限时，
 * 不会将错误路径当作可加载的模块路径继续搜索。 */
static TZrBool aot_runtime_resolve_module_file(const SZrLibrary_Project *project,
                                               const TZrChar *rootDirectory,
                                               const TZrChar *moduleName,
                                               const TZrChar *extension,
                                               TZrChar *buffer,
                                               TZrSize bufferSize) {
    TZrChar rootPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar relativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize writeIndex = 0;

    if (project == ZR_NULL || rootDirectory == ZR_NULL || moduleName == ZR_NULL || extension == ZR_NULL ||
        buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin(ZrCore_String_GetNativeString(project->directory), rootDirectory, rootPath);
    for (TZrSize index = 0; moduleName[index] != '\0' && writeIndex + 1 < sizeof(relativePath); index++) {
        relativePath[writeIndex++] = moduleName[index] == '/' ? ZR_SEPARATOR : moduleName[index];
    }
    relativePath[writeIndex] = '\0';

    return snprintf(buffer, bufferSize, "%s%c%s%s", rootPath, ZR_SEPARATOR, relativePath, extension) < (int)bufferSize;
}

/* 生成动态库文件名的模块部分，与backend命名规则对应；非ASCII字母数字替换下划线；不是模块身份校验或唯一性保证。 */
static void aot_runtime_sanitize_module_name(const TZrChar *moduleName, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize cursor = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (moduleName == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; moduleName[index] != '\0' && cursor + 1 < bufferSize; index++) {
        TZrChar current = moduleName[index];
        buffer[cursor++] = (TZrChar)(((current >= 'a' && current <= 'z') ||
                                      (current >= 'A' && current <= 'Z') ||
                                      (current >= '0' && current <= '9'))
                                             ? current
                                             : '_');
    }
    buffer[cursor] = '\0';
}

static const TZrChar *aot_runtime_dynamic_library_extension(void) {
#if defined(ZR_PLATFORM_WIN)
    return ".dll";
#elif defined(ZR_PLATFORM_DARWIN)
    return ".dylib";
#else
    return ".so";
#endif
}

static TZrBool aot_runtime_member_token_remap_entry_is_valid(const SZrAotMemberTokenRemap *entry) {
    if (entry == ZR_NULL ||
        entry->sourceToken == 0u ||
        entry->targetToken == 0u ||
        ZR_METADATA_TOKEN_TABLE(entry->sourceToken) != ZR_METADATA_TABLE_MEMBER_DEF ||
        ZR_METADATA_TOKEN_TABLE(entry->targetToken) != ZR_METADATA_TABLE_MEMBER_DEF ||
        ZR_METADATA_TOKEN_RID(entry->sourceToken) == 0u ||
        ZR_METADATA_TOKEN_RID(entry->targetToken) == 0u) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool aot_runtime_metadata_token_is_type_def(TZrUInt32 token) {
    return (TZrBool)(token != 0u &&
                     ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_TYPE_DEF &&
                     ZR_METADATA_TOKEN_RID(token) != 0u);
}

static TZrBool aot_runtime_metadata_token_is_member_def(TZrUInt32 token) {
    return (TZrBool)(token != 0u &&
                     ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_MEMBER_DEF &&
                     ZR_METADATA_TOKEN_RID(token) != 0u);
}

/* 核对导出种类及可选token标志与字段的一致性；target非null；不声称验证了target内容或执行契约。 */
static TZrBool aot_runtime_manifest_export_entry_is_valid(const SZrAotManifestExportEntry *entry) {
    if (entry == ZR_NULL ||
        entry->target == ZR_NULL ||
        (entry->kind != ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE &&
         entry->kind != ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD &&
         entry->kind != ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD) ||
        (entry->flags & ~ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_KNOWN_MASK) != 0u) {
        return ZR_FALSE;
    }

    if ((entry->flags & ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN) != 0u) {
        if (!aot_runtime_metadata_token_is_type_def(entry->typeToken)) {
            return ZR_FALSE;
        }
    } else if (entry->typeToken != 0u) {
        return ZR_FALSE;
    }

    if ((entry->flags & ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN) != 0u) {
        if (!aot_runtime_metadata_token_is_member_def(entry->memberToken)) {
            return ZR_FALSE;
        }
    } else if (entry->memberToken != 0u) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 动态库描述符必须与当前后端、规范模块名、函数图和 ABI 注册表一致；
 * prepare_record 在调用任何生成 thunk 前执行此门禁。 */
static TZrBool aot_runtime_validate_descriptor(SZrState *state,
                                               SZrLibraryAotRuntimeState *runtimeState,
                                               const ZrAotCompiledModule *descriptor,
                                               EZrAotBackendKind backendKind,
                                               const TZrChar *normalizedModule) {
    if (descriptor == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': descriptor=null",
                         normalizedModule != ZR_NULL ? normalizedModule : "<unknown>");
        return ZR_FALSE;
    }

    if (descriptor->abiVersion != ZR_VM_AOT_ABI_VERSION) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': abiVersion expected=%u actual=%u",
                         normalizedModule,
                         (unsigned)ZR_VM_AOT_ABI_VERSION,
                         (unsigned)descriptor->abiVersion);
        return ZR_FALSE;
    }

    if ((EZrAotBackendKind)descriptor->backendKind != backendKind) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': backendKind expected=%u actual=%u",
                         normalizedModule,
                         (unsigned)backendKind,
                         (unsigned)descriptor->backendKind);
        return ZR_FALSE;
    }

    if (descriptor->moduleName == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': moduleName=null",
                         normalizedModule);
        return ZR_FALSE;
    }

    if (strcmp(descriptor->moduleName, normalizedModule) != 0) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': moduleName expected='%s' actual='%s'",
                         normalizedModule,
                         normalizedModule,
                         descriptor->moduleName);
        return ZR_FALSE;
    }

    if (descriptor->entryThunk == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': entryThunk=null",
                         normalizedModule);
        return ZR_FALSE;
    }

    if (descriptor->embeddedModuleBlob == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': embeddedModuleBlob=null",
                         normalizedModule);
        return ZR_FALSE;
    }

    if (descriptor->embeddedModuleBlobLength == 0) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': embeddedModuleBlobLength=0",
                         normalizedModule);
        return ZR_FALSE;
    }

    if (descriptor->functionThunks == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': functionThunks=null",
                         normalizedModule);
        return ZR_FALSE;
    }

    if (descriptor->functionThunkCount == 0) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': functionThunkCount=0",
                         normalizedModule);
        return ZR_FALSE;
    }

    if (descriptor->codeRegistration == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': codeRegistration=null",
                         normalizedModule);
        return ZR_FALSE;
    }

    if (descriptor->codeRegistration->functionPointers != descriptor->functionThunks ||
        descriptor->codeRegistration->functionCount != descriptor->functionThunkCount ||
        descriptor->codeRegistration->methodInfos != descriptor->methodInfos ||
        descriptor->codeRegistration->methodInfoCount != descriptor->methodInfoCount ||
        descriptor->codeRegistration->methodTokens != descriptor->methodTokens ||
        descriptor->codeRegistration->methodTokenCount != descriptor->methodTokenCount ||
        descriptor->codeRegistration->memberTokenRemaps != descriptor->memberTokenRemaps ||
        descriptor->codeRegistration->memberTokenRemapCount != descriptor->memberTokenRemapCount ||
        descriptor->codeRegistration->manifestExports != descriptor->manifestExports ||
        descriptor->codeRegistration->manifestExportCount != descriptor->manifestExportCount ||
        descriptor->codeRegistration->typeLayouts != descriptor->typeLayouts ||
        descriptor->codeRegistration->typeLayoutCount != descriptor->typeLayoutCount ||
        descriptor->codeRegistration->typeLayoutTokens != descriptor->typeLayoutTokens ||
        descriptor->codeRegistration->typeLayoutTokenCount != descriptor->typeLayoutTokenCount ||
        descriptor->codeRegistration->gcDescriptors != descriptor->gcDescriptors ||
        descriptor->codeRegistration->gcDescriptorCount != descriptor->gcDescriptorCount ||
        descriptor->codeRegistration->nativeImportContracts !=
                descriptor->nativeImportContracts ||
        descriptor->codeRegistration->nativeImportContractCount !=
                descriptor->nativeImportContractCount ||
        descriptor->codeRegistration->nativeImportRanges !=
                descriptor->nativeImportRanges ||
        descriptor->codeRegistration->nativeImportRangeCount !=
                descriptor->nativeImportRangeCount) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': codeRegistration table mismatch",
                         normalizedModule);
        return ZR_FALSE;
    }

    if (descriptor->methodTokenCount != descriptor->methodInfoCount ||
        (descriptor->methodTokenCount > 0u && descriptor->methodTokens == ZR_NULL) ||
        (descriptor->methodTokenCount == 0u && descriptor->methodTokens != ZR_NULL)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': method token table mismatch",
                         normalizedModule);
        return ZR_FALSE;
    }

    if ((descriptor->memberTokenRemapCount > 0u && descriptor->memberTokenRemaps == ZR_NULL) ||
        (descriptor->memberTokenRemapCount == 0u && descriptor->memberTokenRemaps != ZR_NULL)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': member token remap table mismatch",
                         normalizedModule);
        return ZR_FALSE;
    }

    if ((descriptor->manifestExportCount > 0u && descriptor->manifestExports == ZR_NULL) ||
        (descriptor->manifestExportCount == 0u && descriptor->manifestExports != ZR_NULL)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': manifest export table mismatch",
                         normalizedModule);
        return ZR_FALSE;
    }

    if (descriptor->codeRegistration->callBindingRows != descriptor->callBindingRows ||
        descriptor->codeRegistration->callBindingRowCount != descriptor->callBindingRowCount ||
        descriptor->codeRegistration->callBindingRowSize != descriptor->callBindingRowSize ||
        descriptor->codeRegistration->callBindingTargetFunctionIndices !=
                descriptor->callBindingTargetFunctionIndices ||
        descriptor->callBindingRowSize != ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE ||
        descriptor->callBindingRowCount > ZR_ARTIFACT_MAX_ROW_COUNT ||
        (descriptor->callBindingRowCount > 0u &&
         (descriptor->callBindingRows == ZR_NULL ||
          descriptor->callBindingTargetFunctionIndices == ZR_NULL)) ||
        (descriptor->callBindingRowCount == 0u &&
         (descriptor->callBindingRows != ZR_NULL ||
          descriptor->callBindingTargetFunctionIndices != ZR_NULL))) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': "
                         "call-binding registration table mismatch",
                         normalizedModule);
        return ZR_FALSE;
    }

    if ((descriptor->nativeImportContractCount > 0u &&
         descriptor->nativeImportContracts == ZR_NULL) ||
        (descriptor->nativeImportContractCount == 0u &&
         descriptor->nativeImportContracts != ZR_NULL)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': "
                         "native import contract table mismatch",
                         normalizedModule);
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u;
         index < descriptor->nativeImportContractCount;
         index++) {
        if (!ZrCommon_NativeImportContract_Validate(
                    &descriptor->nativeImportContracts[index])) {
            aot_runtime_fail(state,
                             runtimeState,
                             "AOT descriptor validation failed for module '%s': "
                             "native import contract invalid index=%u",
                             normalizedModule,
                             (unsigned)index);
            return ZR_FALSE;
        }
    }
    if (descriptor->nativeImportRangeCount != descriptor->functionThunkCount ||
        descriptor->nativeImportRanges == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': "
                         "native import range table mismatch",
                         normalizedModule);
        return ZR_FALSE;
    }
    {
        TZrUInt64 expectedContractStart = 0u;

        for (TZrUInt32 index = 0u;
             index < descriptor->nativeImportRangeCount;
             index++) {
            const SZrAotNativeImportRange *range =
                    &descriptor->nativeImportRanges[index];
            TZrUInt64 contractEnd =
                    (TZrUInt64)range->contractStart + range->contractCount;

            if (range->contractStart != expectedContractStart ||
                contractEnd > descriptor->nativeImportContractCount) {
                aot_runtime_fail(state,
                                 runtimeState,
                                 "AOT descriptor validation failed for module '%s': "
                                 "native import range invalid functionIndex=%u",
                                 normalizedModule,
                                 (unsigned)index);
                return ZR_FALSE;
            }
            expectedContractStart = contractEnd;
        }
        if (expectedContractStart != descriptor->nativeImportContractCount) {
            aot_runtime_fail(state,
                             runtimeState,
                             "AOT descriptor validation failed for module '%s': "
                             "native import ranges do not cover contract table",
                             normalizedModule);
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0u; index < descriptor->manifestExportCount; index++) {
        const SZrAotManifestExportEntry *entry = &descriptor->manifestExports[index];
        if (!aot_runtime_manifest_export_entry_is_valid(entry)) {
            aot_runtime_fail(state,
                             runtimeState,
                             "AOT descriptor validation failed for module '%s': "
                             "manifest export entry invalid index=%u kind=%u flags=%u",
                             normalizedModule,
                             (unsigned)index,
                             entry != ZR_NULL ? (unsigned)entry->kind : 0u,
                             entry != ZR_NULL ? (unsigned)entry->flags : 0u);
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0u; index < descriptor->memberTokenRemapCount; index++) {
        const SZrAotMemberTokenRemap *entry = &descriptor->memberTokenRemaps[index];
        if (!aot_runtime_member_token_remap_entry_is_valid(entry)) {
            aot_runtime_fail(state,
                             runtimeState,
                             "AOT descriptor validation failed for module '%s': "
                             "member token remap entry invalid index=%u sourceToken=0x%08x targetToken=0x%08x",
                             normalizedModule,
                             (unsigned)index,
                             (unsigned)entry->sourceToken,
                             (unsigned)entry->targetToken);
            return ZR_FALSE;
        }
        for (TZrUInt32 previousIndex = 0u; previousIndex < index; previousIndex++) {
            const SZrAotMemberTokenRemap *previous = &descriptor->memberTokenRemaps[previousIndex];
            if (previous->sourceToken == entry->sourceToken) {
                aot_runtime_fail(state,
                                 runtimeState,
                                 "AOT descriptor validation failed for module '%s': "
                                 "member token remap duplicate sourceToken index=%u previousIndex=%u sourceToken=0x%08x",
                                 normalizedModule,
                                 (unsigned)index,
                                 (unsigned)previousIndex,
                                 (unsigned)entry->sourceToken);
                return ZR_FALSE;
            }
            if (previous->targetToken == entry->targetToken) {
                aot_runtime_fail(state,
                                 runtimeState,
                                 "AOT descriptor validation failed for module '%s': "
                                 "member token remap duplicate targetToken index=%u previousIndex=%u targetToken=0x%08x",
                                 normalizedModule,
                                 (unsigned)index,
                                 (unsigned)previousIndex,
                                 (unsigned)entry->targetToken);
                return ZR_FALSE;
            }
        }
    }

    if ((descriptor->typeLayoutCount > 0u && descriptor->typeLayouts == ZR_NULL) ||
        (descriptor->typeLayoutCount == 0u && descriptor->typeLayouts != ZR_NULL)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': typeLayout table mismatch",
                         normalizedModule);
        return ZR_FALSE;
    }

    if (descriptor->typeLayoutTokenCount != descriptor->typeLayoutCount ||
        (descriptor->typeLayoutTokenCount > 0u && descriptor->typeLayoutTokens == ZR_NULL) ||
        (descriptor->typeLayoutTokenCount == 0u && descriptor->typeLayoutTokens != ZR_NULL)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': typeLayout token table mismatch",
                         normalizedModule);
        return ZR_FALSE;
    }

    if (descriptor->codeRegistration->invokers == ZR_NULL ||
        descriptor->codeRegistration->invokerCount == 0u) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT descriptor validation failed for module '%s': codeRegistration invokers missing",
                         normalizedModule);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 非 provider 模块的 AOT 产物按项目目录和后端命名规则定位；包依赖另经 provider resolver。 */
static TZrBool aot_runtime_resolve_library_path(const SZrLibrary_Project *project,
                                                EZrAotBackendKind backendKind,
                                                const TZrChar *moduleName,
                                                TZrChar *buffer,
                                                TZrSize bufferSize) {
    TZrChar sanitizedModuleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar backendRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *libraryExtension;

    if (project == ZR_NULL || moduleName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    aot_runtime_sanitize_module_name(moduleName, sanitizedModuleName, sizeof(sanitizedModuleName));
    if (sanitizedModuleName[0] == '\0') {
        return ZR_FALSE;
    }

    if (backendKind == ZR_AOT_BACKEND_KIND_C) {
        snprintf(backendRoot, sizeof(backendRoot), "%s%c%s%c%s",
                 ZrCore_String_GetNativeString(project->binary),
                 ZR_SEPARATOR,
                 "aot_c",
                 ZR_SEPARATOR,
                 "lib");
    } else if (backendKind == ZR_AOT_BACKEND_KIND_LLVM) {
        snprintf(backendRoot, sizeof(backendRoot), "%s%c%s%c%s",
                 ZrCore_String_GetNativeString(project->binary),
                 ZR_SEPARATOR,
                 "aot_llvm",
                 ZR_SEPARATOR,
                 "lib");
    } else {
        return ZR_FALSE;
    }

    libraryExtension = aot_runtime_dynamic_library_extension();
    return snprintf(buffer,
                    bufferSize,
                    "%s%c%s%c%s%s%s",
                    ZrCore_String_GetNativeString(project->directory),
                    ZR_SEPARATOR,
                    backendRoot,
                    ZR_SEPARATOR,
                    "zrvm_aot_",
                    sanitizedModuleName,
                    libraryExtension) < (int)bufferSize;
}

/* 描述符输入哈希用于装载时发现源码或 zro 与编译产物不一致。 */
/* TODO: 核查真实输入文件发生读取错误时的哈希校验路径；fread 返回零后当前未区分 ferror 与 EOF，需验证合法装载输入的 I/O 错误及部分哈希后续消费，尚无完整触发或错误注入证明。 */

static TZrBool aot_runtime_hash_file(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    FILE *file;
    TZrByte chunk[ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH];
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    TZrSize readSize;

    if (path == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    while ((readSize = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        for (TZrSize index = 0; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    fclose(file);
    snprintf(buffer, bufferSize, ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long)hash);
    return ZR_TRUE;
}

/* 把产物库装入本进程以读取描述符和thunk；返回OS句柄；发布记录前的失败由close_library回收；此调用会进入平台加载机制。 */
static void *aot_runtime_open_library(const TZrChar *path) {
    if (path == ZR_NULL) {
        return ZR_NULL;
    }
#if defined(ZR_PLATFORM_WIN)
    return (void *)LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

/* 关闭未发布库或GC结束后退役库；不得用于仍可能派发native thunk的活动记录。 */
static void aot_runtime_close_library(void *handle) {
    if (handle == ZR_NULL) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    FreeLibrary((HMODULE)handle);
#else
    dlclose(handle);
#endif
}

static TZrPtr aot_runtime_find_symbol(void *handle, const TZrChar *symbolName) {
    if (handle == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_NULL;
    }
#if defined(ZR_PLATFORM_WIN)
    return (TZrPtr)GetProcAddress((HMODULE)handle, symbolName);
#else
    return (TZrPtr)dlsym(handle, symbolName);
#endif
}

/* 把平台符号地址传给约定的描述符函数指针类型；依赖平台动态加载ABI；这里只复制表示，不调用符号。 */
static FZrVmGetAotCompiledModule aot_runtime_cast_descriptor_symbol(TZrPtr symbolPointer) {
    FZrVmGetAotCompiledModule symbol = ZR_NULL;
    if (symbolPointer != ZR_NULL) {
        memcpy(&symbol, &symbolPointer, sizeof(symbol));
    }
    return symbol;
}

typedef struct SZrAotRuntimeBlobReader {
    /* 借用 descriptor 内嵌 artifact 字节，库仍须保持装载。 */
    const TZrByte *bytes;
    /* 内嵌 blob 的字节边界。 */
    TZrSize length;
    /* reader 是否已交付 blob，防重复读取。 */
    TZrBool consumed;
} SZrAotRuntimeBlobReader;

/* 让core Io以一次借用读取消费描述符内嵌模块字节；reader consumed阻止二次返回；字节仍归动态库；失败不写size。 */
static TZrBytePtr aot_runtime_blob_reader_read(struct SZrState *state, TZrPtr customData, ZR_OUT TZrSize *size) {
    SZrAotRuntimeBlobReader *reader = (SZrAotRuntimeBlobReader *)customData;

    ZR_UNUSED_PARAMETER(state);

    if (reader == ZR_NULL || size == ZR_NULL || reader->consumed || reader->bytes == ZR_NULL || reader->length == 0) {
        return ZR_NULL;
    }

    reader->consumed = ZR_TRUE;
    *size = reader->length;
    return (TZrBytePtr)reader->bytes;
}

/* 结束借用blob reader而不释放动态库字节或栈上reader；与Io close回调配对；资源由prepare_record和库记录管理。 */
static void aot_runtime_blob_reader_close(struct SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

/* 让入口执行和递归导入按backend及规范模块键共享记录；返回records元素借用地址；后续扩容可能使地址失效。 */
static SZrLibraryAotLoadedModule *aot_runtime_find_record(SZrLibraryAotRuntimeState *runtimeState,
                                                          EZrAotBackendKind backendKind,
                                                          const TZrChar *moduleName) {
    if (runtimeState == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < runtimeState->recordCount; index++) {
        SZrLibraryAotLoadedModule *record = &runtimeState->records[index];
        if (record->backendKind == backendKind &&
            record->moduleName != ZR_NULL &&
            strcmp(record->moduleName, moduleName) == 0) {
            return record;
        }
    }
    return ZR_NULL;
}

/* TODO: 核查合法嵌套 AOT 导入在第 5/9 个记录追加时跨 records 扩容，并在返回后继续消费 frame/context/activeRecord 的完整调用链；若 realloc 迁移数组，旧元素借用地址会失效，须验证实际可达路径及后续重定位责任，尚无完整合法触发或实测证明。 */

static TZrBool aot_runtime_append_record(SZrGlobalState *global,
                                         SZrLibraryAotRuntimeState *runtimeState,
                                         const SZrLibraryAotLoadedModule *record,
                                         SZrLibraryAotLoadedModule **outRecord) {
    SZrLibraryAotLoadedModule *newRecords;
    TZrSize newCapacity;

    if (global == ZR_NULL || runtimeState == ZR_NULL || record == ZR_NULL) {
        return ZR_FALSE;
    }

    if (runtimeState->recordCount == runtimeState->recordCapacity) {
        newCapacity = runtimeState->recordCapacity == 0 ? 4 : runtimeState->recordCapacity * 2;
        newRecords = (SZrLibraryAotLoadedModule *)aot_runtime_reallocate(global,
                                                                         runtimeState->records,
                                                                         runtimeState->recordCapacity * sizeof(*runtimeState->records),
                                                                         newCapacity * sizeof(*runtimeState->records));
        if (newRecords == ZR_NULL) {
            return ZR_FALSE;
        }
        runtimeState->records = newRecords;
        runtimeState->recordCapacity = newCapacity;
    }

    runtimeState->records[runtimeState->recordCount] = *record;
    if (outRecord != ZR_NULL) {
        *outRecord = &runtimeState->records[runtimeState->recordCount];
    }
    runtimeState->recordCount++;
    return ZR_TRUE;
}

/* 从伴随zro反序列化VM函数供记录建立函数表；Io source读完立即关闭reader，加载后释放source；当前descriptor门禁已要求blob，回退可达性待核查。 */
static TZrBool aot_runtime_load_zro_function(SZrState *state, const TZrChar *zroPath, SZrFunction **outFunction) {
    SZrLibrary_File_Reader *reader;
    SZrIo io;
    SZrIoSource *ioSource;

    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }
    if (state == ZR_NULL || zroPath == ZR_NULL || outFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    reader = ZrLibrary_File_OpenRead(state->global, (TZrNativeString)zroPath, ZR_TRUE);
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Io_Init(state, &io, ZrLibrary_File_SourceReadImplementation, ZrLibrary_File_SourceCloseImplementation, reader);
    io.isBinary = ZR_TRUE;
    ioSource = ZrCore_Io_ReadSourceNew(&io);
    if (io.close != ZR_NULL) {
        io.close(state, io.customData);
    }
    if (ioSource == ZR_NULL) {
        return ZR_FALSE;
    }

    *outFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, ioSource);
    ZrCore_Io_ReadSourceFree(state->global, ioSource);
    return *outFunction != ZR_NULL;
}

/* 描述符内嵌的模块字节由 core I/O reader 反序列化；reader 不取得动态库 blob 所有权。 */
static TZrBool aot_runtime_load_embedded_function(SZrState *state,
                                                  const TZrByte *blob,
                                                  TZrSize blobLength,
                                                  SZrFunction **outFunction) {
    SZrAotRuntimeBlobReader reader;
    SZrIo io;
    SZrIoSource *ioSource;

    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }
    if (state == ZR_NULL || blob == ZR_NULL || blobLength == 0 || outFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&reader, 0, sizeof(reader));
    reader.bytes = blob;
    reader.length = blobLength;

    ZrCore_Io_Init(state, &io, aot_runtime_blob_reader_read, aot_runtime_blob_reader_close, &reader);
    io.isBinary = ZR_TRUE;
    ioSource = ZrCore_Io_ReadSourceNew(&io);
    if (io.close != ZR_NULL) {
        io.close(state, io.customData);
    }
    if (ioSource == ZR_NULL) {
        return ZR_FALSE;
    }

    *outFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, ioSource);
    ZrCore_Io_ReadSourceFree(state->global, ioSource);
    return *outFunction != ZR_NULL;
}

static SZrFunction *aot_runtime_function_from_constant_value(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->type == ZR_VALUE_TYPE_FUNCTION &&
        !value->isNative &&
        value->value.object->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_CAST_FUNCTION(state, value->value.object);
    }

    if (value->type == ZR_VALUE_TYPE_CLOSURE &&
        !value->isNative &&
        value->value.object->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, value->value.object);
        return closure != ZR_NULL ? closure->function : ZR_NULL;
    }

    return ZR_NULL;
}

/* TODO: 容量预计算不记录已访问函数；需确认函数常量引用图无环，且重复引用不会使
 * 递归深度或 uint32 容量计算超过可接受范围。 */
static TZrUInt32 aot_runtime_count_function_graph_capacity(SZrState *state, const SZrFunction *function) {
    TZrUInt32 count = 1;

    if (state == ZR_NULL || function == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < function->constantValueLength; index++) {
        SZrFunction *constantFunction =
                aot_runtime_function_from_constant_value(state, &function->constantValueList[index]);
        if (constantFunction != ZR_NULL) {
            count += aot_runtime_count_function_graph_capacity(state, constantFunction);
        }
    }

    for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
        count += aot_runtime_count_function_graph_capacity(state, &function->childFunctionList[index]);
    }
    return count;
}

/* TODO: 函数表用名称、参数数目、指令长度和源码行范围去重；需确认不同函数不会
 * 恰好共享这些属性，否则生成函数索引可能指向错误的元数据函数。 */
static TZrBool aot_runtime_functions_equivalent(const SZrFunction *left, const SZrFunction *right) {
    TZrBool sameFunctionName;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    sameFunctionName = left->functionName == right->functionName ||
                       (left->functionName == ZR_NULL && right->functionName == ZR_NULL) ||
                       (left->functionName != ZR_NULL && right->functionName != ZR_NULL &&
                        ZrCore_String_Equal(left->functionName, right->functionName));

    return sameFunctionName &&
           left->parameterCount == right->parameterCount &&
           left->instructionsLength == right->instructionsLength &&
           left->lineInSourceStart == right->lineInSourceStart &&
           left->lineInSourceEnd == right->lineInSourceEnd;
}

static TZrBool aot_runtime_function_table_contains(SZrFunction *const *functions,
                                                   TZrUInt32 count,
                                                   const SZrFunction *function) {
    if (functions == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < count; index++) {
        if (functions[index] == function || aot_runtime_functions_equivalent(functions[index], function)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 以模块、常量函数、child顺序建立thunk索引对应表；先登记再递归，可跳过重复；超出预估容量会停止登记。 */
static void aot_runtime_flatten_function_graph(SZrState *state,
                                               SZrFunction *function,
                                               SZrFunction **functions,
                                               TZrUInt32 capacity,
                                               TZrUInt32 *ioIndex) {
    if (state == ZR_NULL || function == ZR_NULL || functions == ZR_NULL || ioIndex == ZR_NULL) {
        return;
    }

    if (aot_runtime_function_table_contains(functions, *ioIndex, function)) {
        return;
    }
    if (*ioIndex >= capacity) {
        return;
    }

    functions[*ioIndex] = function;
    (*ioIndex)++;
    for (TZrUInt32 index = 0; index < function->constantValueLength; index++) {
        SZrFunction *constantFunction =
                aot_runtime_function_from_constant_value(state, &function->constantValueList[index]);
        if (constantFunction != ZR_NULL) {
            aot_runtime_flatten_function_graph(state, constantFunction, functions, capacity, ioIndex);
        }
    }
    for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
        aot_runtime_flatten_function_graph(state, &function->childFunctionList[index], functions, capacity, ioIndex);
    }
}

/* GC 已完成时才能关闭卸载队列中的库，避免回收器或对象终结过程再调用其中的 native 代码。 */
static void aot_runtime_close_retired_libraries(SZrGlobalState *global, TZrPtr state) {
    SZrLibraryAotRetiredLibraries *retired = (SZrLibraryAotRetiredLibraries *)state;

    if (global == ZR_NULL || retired == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0u; index < retired->count; index++) {
        aot_runtime_close_library(retired->handles[index]);
    }
    if (retired->handles != ZR_NULL && retired->capacity > 0u) {
        aot_runtime_reallocate(global,
                               retired->handles,
                               sizeof(*retired->handles) * retired->capacity,
                               0u);
    }
    aot_runtime_reallocate(global, retired, sizeof(*retired), 0u);
}

/* 项目结束时将动态库句柄移交 global 的 GC 后清理回调；失败意味着所有权未转移。 */
static TZrBool aot_runtime_retire_library(SZrGlobalState *global, void *handle) {
    SZrLibraryAotRetiredLibraries *retired;

    if (global == ZR_NULL || handle == ZR_NULL) {
        return handle == ZR_NULL ? ZR_TRUE : ZR_FALSE;
    }
    if (global->postGcCleanupState != ZR_NULL &&
        global->postGcCleanup != aot_runtime_close_retired_libraries) {
        return ZR_FALSE;
    }

    retired = (SZrLibraryAotRetiredLibraries *)global->postGcCleanupState;
    if (retired == ZR_NULL) {
        retired = (SZrLibraryAotRetiredLibraries *)aot_runtime_reallocate(
                global, ZR_NULL, 0u, sizeof(*retired));
        if (retired == ZR_NULL) {
            return ZR_FALSE;
        }
        memset(retired, 0, sizeof(*retired));
        if (!ZrCore_GlobalState_SetPostGcCleanup(
                    global, retired, aot_runtime_close_retired_libraries)) {
            aot_runtime_reallocate(global, retired, sizeof(*retired), 0u);
            return ZR_FALSE;
        }
    }
    if (retired->count == retired->capacity) {
        TZrSize newCapacity = retired->capacity == 0u ? 4u : retired->capacity * 2u;
        void **newHandles;

        if (newCapacity < retired->capacity) {
            return ZR_FALSE;
        }
        newHandles = (void **)aot_runtime_reallocate(
                global,
                retired->handles,
                sizeof(*retired->handles) * retired->capacity,
                sizeof(*retired->handles) * newCapacity);
        if (newHandles == ZR_NULL) {
            return ZR_FALSE;
        }
        retired->handles = newHandles;
        retired->capacity = newCapacity;
    }
    retired->handles[retired->count++] = handle;
    return ZR_TRUE;
}

/* 展平函数图后把常量闭包重新指向 canonical 函数对象，保持 thunk 索引与 VM 函数身份一致。 */
static void aot_runtime_rebind_function_table_constants(SZrState *state,
                                                        SZrFunction *const *functionTable,
                                                        TZrUInt32 functionCount) {
    if (state == ZR_NULL || functionTable == ZR_NULL) {
        return;
    }

    for (TZrUInt32 functionIndex = 0u; functionIndex < functionCount; functionIndex++) {
        SZrFunction *function = functionTable[functionIndex];

        if (function == ZR_NULL || function->constantValueList == ZR_NULL) {
            continue;
        }
        for (TZrUInt32 constantIndex = 0u;
             constantIndex < function->constantValueLength;
             constantIndex++) {
            SZrTypeValue *constant = &function->constantValueList[constantIndex];
            SZrFunction *constantFunction = aot_runtime_function_from_constant_value(state, constant);

            if (constantFunction == ZR_NULL) {
                continue;
            }
            for (TZrUInt32 candidateIndex = 0u; candidateIndex < functionCount; candidateIndex++) {
                SZrFunction *candidate = functionTable[candidateIndex];

                if (candidate == ZR_NULL ||
                    (candidate != constantFunction &&
                     !aot_runtime_functions_equivalent(candidate, constantFunction))) {
                    continue;
                }
                if (constant->type == ZR_VALUE_TYPE_FUNCTION) {
                    constant->value.object = ZR_CAST_RAW_OBJECT_AS_SUPER(candidate);
                    ZrCore_RawObject_Barrier(state,
                                             ZR_CAST_RAW_OBJECT_AS_SUPER(function),
                                             ZR_CAST_RAW_OBJECT_AS_SUPER(candidate));
                } else if (constant->type == ZR_VALUE_TYPE_CLOSURE && !constant->isNative) {
                    SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, constant->value.object);
                    if (closure != ZR_NULL) {
                        closure->function = candidate;
                        ZrCore_RawObject_Barrier(state,
                                                 ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
                                                 ZR_CAST_RAW_OBJECT_AS_SUPER(candidate));
                    }
                }
                break;
            }
        }
    }
}

/* AOT 描述符的函数序号对应整张闭包函数图；装载阶段展平并重绑后才能校验 thunk 数量。 */
static TZrBool aot_runtime_build_function_table(SZrState *state,
                                                SZrFunction *function,
                                                SZrFunction ***outFunctions,
                                                TZrUInt32 *outCount,
                                                TZrUInt32 *outCapacity) {
    SZrFunction **functions;
    TZrUInt32 capacity;
    TZrUInt32 writeIndex = 0;

    if (outFunctions != ZR_NULL) {
        *outFunctions = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (outCapacity != ZR_NULL) {
        *outCapacity = 0;
    }
    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || outFunctions == ZR_NULL ||
        outCount == ZR_NULL || outCapacity == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Function_RebindConstantFunctionValuesToChildren(function);
    ZrCore_Function_ClearChildOwnerLinks(function);
    capacity = aot_runtime_count_function_graph_capacity(state, function);
    if (capacity == 0) {
        return ZR_TRUE;
    }

    functions = (SZrFunction **)aot_runtime_reallocate(state->global, ZR_NULL, 0, sizeof(*functions) * capacity);
    if (functions == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(functions, 0, sizeof(*functions) * capacity);
    aot_runtime_flatten_function_graph(state, function, functions, capacity, &writeIndex);
    aot_runtime_rebind_function_table_constants(state, functions, writeIndex);
    *outFunctions = functions;
    *outCount = writeIndex;
    *outCapacity = capacity;
    return ZR_TRUE;
}

/* 在装载时缓存生成帧稠密范围，避免入口重复推导布局；每项来自物理frame storage计数；null函数时收回整表。 */
static TZrBool aot_runtime_build_generated_slot_count_table(SZrGlobalState *global,
                                                            SZrFunction *const *functionTable,
                                                            TZrUInt32 functionCount,
                                                            TZrUInt32 **outSlotCounts) {
    TZrUInt32 *slotCounts;

    if (outSlotCounts != ZR_NULL) {
        *outSlotCounts = ZR_NULL;
    }
    if (global == ZR_NULL || outSlotCounts == ZR_NULL || (functionCount > 0 && functionTable == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (functionCount == 0) {
        return ZR_TRUE;
    }

    slotCounts = (TZrUInt32 *)aot_runtime_reallocate(global, ZR_NULL, 0, sizeof(*slotCounts) * functionCount);
    if (slotCounts == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < functionCount; index++) {
        if (functionTable[index] == ZR_NULL) {
            aot_runtime_reallocate(global, slotCounts, sizeof(*slotCounts) * functionCount, 0);
            return ZR_FALSE;
        }
        slotCounts[index] = (TZrUInt32)ZrCore_Function_GetFrameStorageSlotCount(functionTable[index]);
    }

    *outSlotCounts = slotCounts;
    return ZR_TRUE;
}

/* record 只保存原生函数指针和 VM 函数表，必须 pin 每个函数对象直到项目释放，
 * 防止生成 thunk 执行期间 GC 迁移或回收其元数据目标。 */
static SZrGcNativeCallPin *aot_runtime_pin_function_table(SZrState *state,
                                                         SZrFunction *const *functionTable,
                                                         TZrUInt32 functionCount) {
    SZrGcNativeCallPin *functionPins;
    TZrUInt32 functionIndex;

    if (state == ZR_NULL || state->global == ZR_NULL || functionTable == ZR_NULL || functionCount == 0u) {
        return ZR_NULL;
    }

    functionPins = (SZrGcNativeCallPin *)aot_runtime_reallocate(
            state->global, ZR_NULL, 0, sizeof(*functionPins) * functionCount);
    if (functionPins == ZR_NULL) {
        return ZR_NULL;
    }
    memset(functionPins, 0, sizeof(*functionPins) * functionCount);

    for (functionIndex = 0u; functionIndex < functionCount; functionIndex++) {
        if (functionTable[functionIndex] == ZR_NULL ||
            !ZrCore_Gc_NativeCallPinObject(
                    state,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(functionTable[functionIndex]),
                    &functionPins[functionIndex])) {
            while (functionIndex > 0u) {
                functionIndex--;
                ZrCore_Gc_NativeCallUnpin(state->global, &functionPins[functionIndex]);
            }
            aot_runtime_reallocate(
                    state->global, functionPins, sizeof(*functionPins) * functionCount, 0);
            return ZR_NULL;
        }
    }

    return functionPins;
}

/* 为prepare失败和project析构撤销函数表pin并释放凭据；不释放函数对象或functionTable；仅撤本层凭据增加的根。 */
static void aot_runtime_unpin_function_table(SZrState *state,
                                             SZrGcNativeCallPin *functionPins,
                                             TZrUInt32 functionCount) {
    if (state == ZR_NULL || state->global == ZR_NULL || functionPins == ZR_NULL) {
        return;
    }

    for (TZrUInt32 functionIndex = 0u; functionIndex < functionCount; functionIndex++) {
        ZrCore_Gc_NativeCallUnpin(state->global, &functionPins[functionIndex]);
    }
    aot_runtime_reallocate(
            state->global, functionPins, sizeof(*functionPins) * functionCount, 0);
}

static const TZrChar *aot_runtime_metadata_binding_status_name(
        EZrMetadataRuntimeBindingCompatibilityStatus status) {
    switch (status) {
        case ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE:
            return "COMPATIBLE";
        case ZR_METADATA_RUNTIME_BINDING_STATUS_INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_VERSION_MISMATCH:
            return "MODULE_VERSION_MISMATCH";
        case ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_SIGNATURE_HASH_MISMATCH:
            return "MODULE_SIGNATURE_HASH_MISMATCH";
        case ZR_METADATA_RUNTIME_BINDING_STATUS_METADATA_TOKEN_MISMATCH:
            return "METADATA_TOKEN_MISMATCH";
        case ZR_METADATA_RUNTIME_BINDING_STATUS_SIGNATURE_TOKEN_MISMATCH:
            return "SIGNATURE_TOKEN_MISMATCH";
        case ZR_METADATA_RUNTIME_BINDING_STATUS_SIGNATURE_HASH_MISMATCH:
            return "SIGNATURE_HASH_MISMATCH";
        case ZR_METADATA_RUNTIME_BINDING_STATUS_LAYOUT_VERSION_MISMATCH:
            return "LAYOUT_VERSION_MISMATCH";
        case ZR_METADATA_RUNTIME_BINDING_STATUS_LAYOUT_HASH_MISMATCH:
            return "LAYOUT_HASH_MISMATCH";
        default:
            return "UNKNOWN";
    }
}

/* 动态库函数图即使 ABI 合法仍需逐函数核对 token、签名、布局和模块版本；
 * prepare_record 在发布 record 前执行，防止旧 thunk 使用新的元数据解释参数。 */
static TZrBool aot_runtime_validate_metadata_bindings(SZrState *state,
                                                      SZrLibraryAotRuntimeState *runtimeState,
                                                      const TZrChar *moduleName,
                                                      const SZrFunction *moduleFunction,
                                                      SZrFunction *const *functionTable,
                                                      TZrUInt32 functionCount) {
    SZrString *actualModuleVersion = moduleFunction != ZR_NULL ? moduleFunction->moduleVersion : ZR_NULL;

    if (functionCount > 0 && functionTable == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT metadata binding compatibility failed for module '%s': missing function table",
                         moduleName != ZR_NULL ? moduleName : "<unknown>");
        return ZR_FALSE;
    }

    for (TZrUInt32 functionIndex = 0; functionIndex < functionCount; functionIndex++) {
        const SZrMetadataTokenBinding *binding = ZR_NULL;
        SZrMetadataRuntimeBindingCompatibilityReport report;
        EZrMetadataRuntimeBindingCompatibilityStatus status;

        memset(&report, 0, sizeof(report));
        status = ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility(functionTable[functionIndex],
                                                                               actualModuleVersion,
                                                                               &binding,
                                                                               ZR_NULL,
                                                                               &report);
        if (status == ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE) {
            continue;
        }

        aot_runtime_fail(state,
                         runtimeState,
                         "AOT metadata binding compatibility failed for module '%s': "
                         "function=%u status=%s refToken=0x%08x "
                         "expectedMetadataToken=0x%08x actualMetadataToken=0x%08x "
                         "expectedSignatureToken=0x%08x actualSignatureToken=0x%08x "
                         "expectedSignatureHash=0x%016llx actualSignatureHash=0x%016llx "
                         "expectedModuleSignatureHash=0x%016llx actualModuleSignatureHash=0x%016llx "
                         "expectedLayoutVersion=%u actualLayoutVersion=%u "
                         "expectedLayoutHash=0x%016llx actualLayoutHash=0x%016llx",
                         moduleName != ZR_NULL ? moduleName : "<unknown>",
                         (unsigned)functionIndex,
                         aot_runtime_metadata_binding_status_name(status),
                         (unsigned)(binding != ZR_NULL ? binding->refToken : 0u),
                         (unsigned)report.expectedMetadataToken,
                         (unsigned)report.actualMetadataToken,
                         (unsigned)report.expectedSignatureToken,
                         (unsigned)report.actualSignatureToken,
                         (unsigned long long)report.expectedSignatureHash,
                         (unsigned long long)report.actualSignatureHash,
                         (unsigned long long)report.expectedModuleSignatureHash,
                         (unsigned long long)report.actualModuleSignatureHash,
                         (unsigned)report.expectedLayoutVersion,
                         (unsigned)report.actualLayoutVersion,
                         (unsigned long long)report.expectedLayoutHash,
                         (unsigned long long)report.actualLayoutHash);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 把closure元数据身份映射到记录thunk索引；按指针或等价属性搜索；未匹配返回UINT32_MAX。 */
static TZrUInt32 aot_runtime_find_function_index_in_record(const SZrLibraryAotLoadedModule *record,
                                                           const SZrFunction *function) {
    if (record == ZR_NULL || function == ZR_NULL || record->functionTable == ZR_NULL) {
        return UINT32_MAX;
    }

    for (TZrUInt32 index = 0; index < record->functionCount; index++) {
        const SZrFunction *candidate = record->functionTable[index];
        if (candidate == function || aot_runtime_functions_equivalent(candidate, function)) {
            return index;
        }
    }
    return UINT32_MAX;
}

/* native closure 及动态 direct call 按函数身份反查所属库记录；返回借用指针，只在项目记录有效期内可用。 */
static SZrLibraryAotLoadedModule *aot_runtime_find_record_for_function(SZrLibraryAotRuntimeState *runtimeState,
                                                                       const SZrFunction *function) {
    if (runtimeState == ZR_NULL || function == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize recordIndex = 0; recordIndex < runtimeState->recordCount; recordIndex++) {
        SZrLibraryAotLoadedModule *record = &runtimeState->records[recordIndex];
        if (aot_runtime_find_function_index_in_record(record, function) != UINT32_MAX) {
            return record;
        }
    }
    return ZR_NULL;
}

/* 为已链接AOT调用目标确认methodInfo与thunk来自同一注册表；要求两种指针和索引都匹配；不退到仅名称相似的函数。 */
static SZrLibraryAotLoadedModule *aot_runtime_find_record_for_bound_target(
        SZrLibraryAotRuntimeState *runtimeState, const SZrCallBindingTarget *target) {
    if (runtimeState == ZR_NULL || target == ZR_NULL || target->targetKind != ZR_CALL_BINDING_TARGET_AOT ||
        target->aot.methodInfo == ZR_NULL) return ZR_NULL;
    TZrUInt32 index = target->aot.methodInfo->functionIndex;
    for (TZrSize recordIndex = 0u; recordIndex < runtimeState->recordCount; ++recordIndex) {
        SZrLibraryAotLoadedModule *record = &runtimeState->records[recordIndex];
        const SZrAotCodeRegistration *registration = record->codeRegistration;
        if (registration != ZR_NULL && index < registration->methodInfoCount &&
            index < registration->functionCount && registration->methodInfos != ZR_NULL &&
            registration->functionPointers != ZR_NULL && registration->methodInfos[index] == target->aot.methodInfo &&
            registration->functionPointers[index] == target->aot.thunk) return record;
    }
    return ZR_NULL;
}

/* 已载入注册表的每个函数有自己的 native import 范围；调用方只借用其中的契约。 */
const SZrNativeImportContract *
ZrLibrary_AotRuntime_ResolveNativeImportContract(
        const SZrAotCodeRegistration *codeRegistration,
        TZrUInt32 functionIndex,
        TZrUInt32 localContractIndex) {
    const SZrAotNativeImportRange *range;
    TZrUInt64 contractIndex;

    if (codeRegistration == ZR_NULL ||
        codeRegistration->nativeImportRanges == ZR_NULL ||
        functionIndex >= codeRegistration->nativeImportRangeCount) {
        return ZR_NULL;
    }
    range = &codeRegistration->nativeImportRanges[functionIndex];
    if (localContractIndex >= range->contractCount) {
        return ZR_NULL;
    }
    contractIndex = (TZrUInt64)range->contractStart + localContractIndex;
    if (codeRegistration->nativeImportContracts == ZR_NULL ||
        contractIndex >= codeRegistration->nativeImportContractCount) {
        return ZR_NULL;
    }
    return &codeRegistration->nativeImportContracts[contractIndex];
}

/* FFI 沿活动调用帧取得 VM 元数据函数，再反查项目记录和局部契约序号；
 * 返回的契约借用记录，供调用前核对库路径与签名。 */
const SZrNativeImportContract *
ZrLibrary_AotRuntime_FindNativeImportContract(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 localContractIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrLibraryAotLoadedModule *record;
    TZrUInt32 functionIndex;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL) {
        return ZR_NULL;
    }
    runtimeState = aot_runtime_get_state_from_global(state->global);
    record = aot_runtime_find_record_for_function(runtimeState, function);
    if (record == ZR_NULL) {
        return ZR_NULL;
    }
    functionIndex = aot_runtime_find_function_index_in_record(record, function);
    if (functionIndex == UINT32_MAX) {
        return ZR_NULL;
    }
    return ZrLibrary_AotRuntime_ResolveNativeImportContract(
            record->codeRegistration, functionIndex, localContractIndex);
}

static TZrBool aot_runtime_record_try_get_generated_slot_count(const SZrLibraryAotLoadedModule *record,
                                                               TZrUInt32 functionIndex,
                                                               TZrUInt32 *outSlotCount) {
    if (outSlotCount != ZR_NULL) {
        *outSlotCount = 0u;
    }
    if (record == ZR_NULL || outSlotCount == ZR_NULL || record->generatedFrameSlotCounts == ZR_NULL ||
        functionIndex >= record->functionCount) {
        return ZR_FALSE;
    }

    *outSlotCount = record->generatedFrameSlotCounts[functionIndex];
    return ZR_TRUE;
}

static void aot_runtime_bind_record_code_registration(SZrLibraryAotLoadedModule *record,
                                                      const ZrAotCompiledModule *descriptor) {
    if (record == ZR_NULL || descriptor == ZR_NULL) {
        return;
    }

    record->codeRegistration = descriptor->codeRegistration;
}

static const SZrFunction *aot_runtime_frame_function(const ZrAotGeneratedFrame *frame) {
    return frame != ZR_NULL ? frame->function : ZR_NULL;
}

static TZrUInt32 aot_runtime_frame_slot_count(const ZrAotGeneratedFrame *frame) {
    return frame != ZR_NULL ? frame->generatedFrameSlotCount : 0u;
}

/* 按生成槽边界定位稠密值槽；返回栈借用地址，调用或扩栈后须重取。 */
static TZrStackValuePointer aot_runtime_frame_slot(const ZrAotGeneratedFrame *frame, TZrUInt32 slotIndex) {
    if (frame == ZR_NULL || frame->slotBase == ZR_NULL) {
        return ZR_NULL;
    }

    if (frame->function == ZR_NULL || slotIndex >= aot_runtime_frame_slot_count(frame)) {
        return ZR_NULL;
    }

    return frame->slotBase + slotIndex;
}

static const SZrTypeValue *aot_runtime_frame_constant(const ZrAotGeneratedFrame *frame, TZrUInt32 constantIndex) {
    if (frame == ZR_NULL || frame->function == ZR_NULL || frame->function->constantValueList == ZR_NULL ||
        constantIndex >= frame->function->constantValueLength) {
        return ZR_NULL;
    }

    return &frame->function->constantValueList[constantIndex];
}

/* 调用、元方法和 GC 都可能迁移 VM 栈；生成器重新进入指令前须从 callInfo 取回当前槽基址。 */
static TZrBool aot_runtime_refresh_frame_from_callinfo(SZrState *state, ZrAotGeneratedFrame *frame, SZrCallInfo *callInfo) {
    if (state == ZR_NULL || frame == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    frame->callInfo = callInfo;
    frame->slotBase = callInfo->functionBase.valuePointer + 1;
    state->callInfoList = callInfo;
    state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    return ZR_TRUE;
}

/* 转换元方法完成后重取结果槽；不能继续使用调用前的 destination 指针。 */
static SZrTypeValue *aot_runtime_refresh_destination_after_meta(SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                SZrLibraryAotRuntimeState *runtimeState,
                                                                const TZrChar *operationName) {
    SZrCallInfo *callInfo;
    TZrStackValuePointer destinationPointer;
    SZrTypeValue *destinationValue;

    if (state == ZR_NULL || frame == ZR_NULL || operationName == ZR_NULL) {
        return ZR_NULL;
    }

    callInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
    if (callInfo != ZR_NULL && callInfo->functionBase.valuePointer != ZR_NULL &&
        !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        aot_runtime_fail(state, runtimeState, "%s: failed to refresh frame after meta call", operationName);
        return ZR_NULL;
    }

    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    destinationValue = destinationPointer != ZR_NULL ? ZrCore_Stack_GetValue(destinationPointer) : ZR_NULL;
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "%s: destination slot lost after meta call", operationName);
        return ZR_NULL;
    }

    return destinationValue;
}

/* 将 VM handler PC 转成生成器 dispatch 索引；PC 必须位于当前函数指令数组，失败保留 fallthrough。 */
static TZrBool aot_runtime_frame_resume_index(const ZrAotGeneratedFrame *frame,
                                              SZrCallInfo *callInfo,
                                              TZrUInt32 *outIndex) {
    const SZrFunction *function;

    if (outIndex != ZR_NULL) {
        *outIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    function = aot_runtime_frame_function(frame);
    if (function == ZR_NULL || function->instructionsList == ZR_NULL || callInfo == ZR_NULL || outIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    if (callInfo->context.context.programCounter < function->instructionsList ||
        callInfo->context.context.programCounter >= function->instructionsList + function->instructionsLength) {
        return ZR_FALSE;
    }

    *outIndex = (TZrUInt32)(callInfo->context.context.programCounter - function->instructionsList);
    return ZR_TRUE;
}

/* 优先执行外层 finally，再恢复 pending 跳转；仅接受当前生成帧；无 finally 时清除 pending 后读取 PC。 */
static TZrBool aot_runtime_resume_pending_control_in_current_frame(SZrState *state,
                                                                   ZrAotGeneratedFrame *frame,
                                                                   TZrUInt32 *outResumeInstructionIndex) {
    SZrCallInfo *callInfo;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    if (state == ZR_NULL || frame == ZR_NULL || callInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
        if (callInfo != frame->callInfo || !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
            return ZR_FALSE;
        }
        return aot_runtime_frame_resume_index(frame, callInfo, outResumeInstructionIndex);
    }

    if (!execution_jump_to_instruction_offset(state,
                                              &callInfo,
                                              callInfo,
                                              state->pendingControl.targetInstructionOffset)) {
        execution_clear_pending_control(state);
        return ZR_FALSE;
    }

    execution_clear_pending_control(state);
    if (callInfo != frame->callInfo || !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        return ZR_FALSE;
    }
    return aot_runtime_frame_resume_index(frame, callInfo, outResumeInstructionIndex);
}

/* 统一取得 native 与 VM closure 的 capture 和写屏障对象；capture 借用当前 closure；native capture owner 与 closure 可能不同。 */
static TZrBool aot_runtime_resolve_current_closure_capture(SZrState *state,
                                                           const ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 closureIndex,
                                                           SZrTypeValue **outClosureValue,
                                                           SZrRawObject **outBarrierObject) {
    const SZrTypeValue *currentClosureValue;

    if (outClosureValue != ZR_NULL) {
        *outClosureValue = ZR_NULL;
    }
    if (outBarrierObject != ZR_NULL) {
        *outBarrierObject = ZR_NULL;
    }
    if (state == ZR_NULL || frame == ZR_NULL || frame->slotBase == ZR_NULL || outClosureValue == ZR_NULL) {
        return ZR_FALSE;
    }

    currentClosureValue = ZrCore_Stack_GetValue(frame->slotBase - 1);
    if (currentClosureValue == ZR_NULL || currentClosureValue->type != ZR_VALUE_TYPE_CLOSURE ||
        currentClosureValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (currentClosureValue->isNative) {
        SZrClosureNative *nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, currentClosureValue->value.object);
        SZrRawObject *captureOwner;
        if (nativeClosure == ZR_NULL || closureIndex >= nativeClosure->closureValueCount) {
            return ZR_FALSE;
        }
        captureOwner = ZrCore_ClosureNative_GetCaptureOwner(nativeClosure, closureIndex);
        *outClosureValue = ZrCore_ClosureNative_GetCaptureValue(nativeClosure, closureIndex);
        if (outBarrierObject != ZR_NULL) {
            *outBarrierObject = captureOwner != ZR_NULL ? captureOwner : ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure);
        }
        return *outClosureValue != ZR_NULL;
    }

    {
        SZrClosure *vmClosure = ZR_CAST_VM_CLOSURE(state, currentClosureValue->value.object);
        SZrClosureValue *closureValue;
        if (vmClosure == ZR_NULL || closureIndex >= vmClosure->closureValueCount) {
            return ZR_FALSE;
        }
        closureValue = vmClosure->closureValuesExtend[closureIndex];
        if (closureValue == ZR_NULL) {
            return ZR_FALSE;
        }
        *outClosureValue = ZrCore_ClosureValue_GetValue(closureValue);
        if (outBarrierObject != ZR_NULL) {
            *outBarrierObject = ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue);
        }
        return *outClosureValue != ZR_NULL;
    }
}

/* 复制直调窗口值并清除 NONE 值无效的所有权辅助字段；不直接复制未清理的 ownershipControl/WeakRef。 */
static void aot_runtime_copy_direct_call_staging_value(SZrState *state,
                                                       SZrTypeValue *destination,
                                                       const SZrTypeValue *source) {
    SZrTypeValue sanitizedSource;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL) {
        return;
    }

    sanitizedSource = *source;
    if (sanitizedSource.ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE) {
        sanitizedSource.ownershipControl = ZR_NULL;
        sanitizedSource.ownershipWeakRef = ZR_NULL;
    }
    ZrCore_Value_ResetAsNullNoProfile(destination);
    if (!ZrCore_Value_TryCopyFastNoProfile(state, destination, &sanitizedSource)) {
        ZrCore_Value_CopySlow(state, destination, &sanitizedSource);
    }
}

/* 释放未提交或已使用临时窗口的 ownership 凭据；按实际窗口计数清理，不能释放 caller 的原操作数。 */
static void aot_runtime_discard_direct_call_window(SZrState *state,
                                                   TZrStackValuePointer callBase,
                                                   TZrUInt32 valueCount) {
    if (state == ZR_NULL || callBase == ZR_NULL) {
        return;
    }

    for (TZrUInt32 offset = 0u; offset < valueCount; offset++) {
        SZrTypeValue *value = ZrCore_Stack_GetValue(callBase + offset);

        if (ZrCore_Value_HasReleasableOwnershipNoProfile(value)) {
            ZrCore_Ownership_ReleaseValue(state, value);
        } else {
            ZrCore_Value_ResetAsNullNoProfile(value);
        }
    }
}

static TZrUInt32 aot_runtime_generated_resume_instruction_index(const ZrAotGeneratedFrame *frame) {
    if (frame == ZR_NULL || frame->function == ZR_NULL ||
        frame->currentInstructionIndex >= frame->function->instructionsLength ||
        frame->currentInstructionIndex + 1 >= frame->function->instructionsLength) {
        return ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    return frame->currentInstructionIndex + 1;
}

/* 保存 Prepare 时 caller/callee 与观察策略快照；只记录上下文，不执行 thunk 或 PostCall。 */
static void aot_runtime_record_direct_call_context(const ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 calleeFunctionIndex,
                                                   ZrAotGeneratedDirectCall *directCall) {
    if (directCall == ZR_NULL) {
        return;
    }

    directCall->callerFunctionIndex = frame != ZR_NULL ? frame->functionIndex : UINT32_MAX;
    directCall->calleeFunctionIndex = calleeFunctionIndex;
    directCall->callInstructionIndex = frame != ZR_NULL ? frame->currentInstructionIndex : UINT32_MAX;
    directCall->resumeInstructionIndex = aot_runtime_generated_resume_instruction_index(frame);
    directCall->observationMaskSnapshot = frame != ZR_NULL ? frame->observationMask : 0;
    directCall->publishAllInstructionsSnapshot = frame != ZR_NULL ? frame->publishAllInstructions : ZR_FALSE;
}

/* 为静态 thunk 建立带元数据与 captures 的 native closure；分配期间锚定 callBase；无 thunk 时不替换原 callable。 */
static TZrBool aot_runtime_materialize_static_direct_call_base(
        SZrState *state,
        TZrStackValuePointer *callBase,
        const SZrLibraryAotLoadedModule *record,
        TZrUInt32 calleeFunctionIndex,
        FZrAotEntryThunk nativeThunk) {
    SZrTypeValue *callValue;
    SZrClosureNative *closure;
    SZrFunctionStackAnchor callBaseAnchor;
    SZrFunction *metadataFunction;
    TZrUInt32 captureCount;

    if (nativeThunk == ZR_NULL) {
        return ZR_TRUE;
    }
    if (state == ZR_NULL || callBase == ZR_NULL || *callBase == ZR_NULL ||
        record == ZR_NULL || record->functionTable == ZR_NULL ||
        calleeFunctionIndex >= record->functionCount) {
        return ZR_FALSE;
    }

    metadataFunction = record->functionTable[calleeFunctionIndex];
    if (metadataFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    captureCount = metadataFunction->closureValueLength;
    ZrCore_Function_StackAnchorInit(state, *callBase, &callBaseAnchor);
    closure = ZrCore_ClosureNative_New(state, captureCount);
    *callBase = ZrCore_Function_StackAnchorRestore(state, &callBaseAnchor);
    if (closure == ZR_NULL || *callBase == ZR_NULL) {
        return ZR_FALSE;
    }
    callValue = ZrCore_Stack_GetValue(*callBase);
    if (callValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (captureCount > 0 &&
        !aot_runtime_bind_native_closure_captures_from_source(
                state, closure, callValue, captureCount)) {
        return ZR_FALSE;
    }
    closure->nativeFunction = (FZrNativeFunction)nativeThunk;
    closure->aotShimFunction = metadataFunction;

    ZrCore_Ownership_ReleaseValue(state, callValue);
    ZrCore_Value_InitAsRawObject(state, callValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    callValue->type = ZR_VALUE_TYPE_CLOSURE;
    callValue->isGarbageCollectable = ZR_TRUE;
    callValue->isNative = ZR_TRUE;
    return ZR_TRUE;
}

/* 建立 core 可识别的 callee 帧，生成 thunk 才能与异常展开、栈锚点和 PostCall 共用 VM 调用协议。 */
static TZrBool aot_runtime_prepare_vm_direct_call_frame(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 functionSlot,
                                                        TZrUInt32 argumentCount,
                                                        const SZrLibraryAotLoadedModule *record,
                                                        SZrFunction *metadataFunction,
                                                        TZrUInt32 calleeFunctionIndex,
                                                        FZrAotEntryThunk staticNativeThunk,
                                                        ZrAotGeneratedDirectCall *directCall) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callerCallInfo;
    SZrFunction *callerFunction;
    TZrStackValuePointer callerFrameBase;
    TZrStackValuePointer sourceCallBase;
    TZrStackValuePointer callWindowBase;
    TZrStackValuePointer callBase;
    TZrStackValuePointer destinationPointer;
    SZrFunctionStackAnchor callWindowAnchor;
    SZrFunctionStackAnchor destinationAnchor;
    SZrCallInfo *callInfo;
    TZrUInt32 generatedFrameSlotCount;
    TZrSize frameSlotCount;
    TZrSize allocationSlotCount;

    if (directCall != ZR_NULL) {
        memset(directCall, 0, sizeof(*directCall));
    }

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callerCallInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    callerFunction = callerCallInfo != ZR_NULL
                             ? ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callerCallInfo)
                             : ZR_NULL;
    sourceCallBase = aot_runtime_frame_slot(frame, functionSlot);
    /* Accessors and meta operators stage arguments in reserved scratch slots
     * beyond the generated frame. The current call frame owns that window. */
    if (sourceCallBase == ZR_NULL && frame != ZR_NULL && frame->slotBase != ZR_NULL &&
        callerCallInfo != ZR_NULL && callerCallInfo->functionTop.valuePointer > frame->slotBase &&
        functionSlot < (TZrSize)(callerCallInfo->functionTop.valuePointer - frame->slotBase)) {
        sourceCallBase = frame->slotBase + functionSlot;
    }
    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL || runtimeState == ZR_NULL ||
        callerCallInfo == ZR_NULL || callerFunction == ZR_NULL || sourceCallBase == ZR_NULL ||
        destinationPointer == ZR_NULL || record == ZR_NULL || metadataFunction == ZR_NULL ||
        callerCallInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    if (argumentCount == UINT32_MAX ||
        functionSlot > UINT32_MAX - argumentCount - 1u ||
        state->stackTop.valuePointer == ZR_NULL ||
        state->stackTop.valuePointer < sourceCallBase + 1 + argumentCount) {
        aot_runtime_fail(state, runtimeState, "generated AOT direct call has invalid stack range");
        return ZR_FALSE;
    }

    if (!aot_runtime_record_try_get_generated_slot_count(record, calleeFunctionIndex, &generatedFrameSlotCount)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "generated AOT direct call is missing cached frame slot metadata for function index %u",
                         (unsigned)calleeFunctionIndex);
        return ZR_FALSE;
    }
    frameSlotCount = generatedFrameSlotCount;
    if (frameSlotCount < argumentCount) {
        frameSlotCount = argumentCount;
    }
    allocationSlotCount = frameSlotCount + 1u;
    if (allocationSlotCount <= frameSlotCount) {
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, destinationPointer, &destinationAnchor);
    callWindowBase = ZrCore_Function_GetCallInfoFrameStorageTop(state, callerCallInfo);
    if (callWindowBase == ZR_NULL || callWindowBase < state->stackTop.valuePointer) {
        callWindowBase = state->stackTop.valuePointer;
    }
    callBase = ZrCore_Function_CheckStackAndGc(state, allocationSlotCount, callWindowBase);
    destinationPointer = ZrCore_Function_StackAnchorRestore(state, &destinationAnchor);
    if (callBase == ZR_NULL || destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    callerFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callerCallInfo);
    if (callerFunction == ZR_NULL) {
        return ZR_FALSE;
    }
    callerFrameBase = callerCallInfo->functionBase.valuePointer + 1;
    frame->function = callerFunction;
    frame->callInfo = callerCallInfo;
    frame->slotBase = callerFrameBase;
    /* 临时窗口先纳入活动 top，再逐个复制值；复制可触发释放回调，因此每次按锚点恢复窗口与 caller 基址。 */
    for (TZrUInt32 offset = 0u; offset <= argumentCount; offset++) {
        ZrCore_Value_ResetAsNullNoProfile(ZrCore_Stack_GetValue(callBase + offset));
    }
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    ZrCore_Function_StackAnchorInit(state, callBase, &callWindowAnchor);
    for (TZrUInt32 offset = 0u; offset <= argumentCount; offset++) {
        TZrUInt32 logicalSlot = functionSlot + offset;
        const SZrFunctionFrameSlotLayout *slotLayout =
                ZrCore_Function_FindFrameSlotLayout(callerFunction, logicalSlot);
        const SZrTypeValue *sourceValue = ZrCore_Stack_GetValue(callerFrameBase + logicalSlot);
        SZrTypeValue *stagedValue = ZrCore_Stack_GetValue(callBase + offset);

    /* inline struct 不是稠密 SZrTypeValue；跳过窗口复制，由 PreCall 的原帧 argument source 根据布局传参。 */
        if (slotLayout != ZR_NULL &&
            slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
            continue;
        }
        aot_runtime_copy_direct_call_staging_value(state, stagedValue, sourceValue);
        callBase = ZrCore_Function_StackAnchorRestore(state, &callWindowAnchor);
        callerFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callerCallInfo);
        callerFrameBase = callerCallInfo->functionBase.valuePointer + 1;
        frame->function = callerFunction;
        frame->slotBase = callerFrameBase;
        if (callBase == ZR_NULL || callerFunction == ZR_NULL) {
            aot_runtime_discard_direct_call_window(
                    state, callBase, argumentCount + 1u);
            state->stackTop.valuePointer = callerCallInfo->functionTop.valuePointer;
            return ZR_FALSE;
        }
    }
    if (!aot_runtime_materialize_static_direct_call_base(
                state,
                &callBase,
                record,
                calleeFunctionIndex,
                staticNativeThunk)) {
        aot_runtime_discard_direct_call_window(
                state, callBase, argumentCount + 1u);
        state->stackTop.valuePointer = callerCallInfo->functionTop.valuePointer;
        return ZR_FALSE;
    }

    metadataFunction = record->functionTable[calleeFunctionIndex];
    destinationPointer = ZrCore_Function_StackAnchorRestore(state, &destinationAnchor);
    callerFrameBase = callerCallInfo->functionBase.valuePointer + 1;
    frame->slotBase = callerFrameBase;
    if (metadataFunction == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_discard_direct_call_window(
                state, callBase, argumentCount + 1u);
        state->stackTop.valuePointer = callerCallInfo->functionTop.valuePointer;
        return ZR_FALSE;
    }
    callInfo = ZrCore_Function_PreCallPreparedResolvedVmFunctionWithArgumentSource(
            state,
            callBase,
            metadataFunction,
            argumentCount,
            1u,
            destinationPointer,
            callerFrameBase,
            functionSlot + 1u);
    if (callInfo == ZR_NULL) {
        callBase = ZrCore_Function_StackAnchorRestore(state, &callWindowAnchor);
        aot_runtime_discard_direct_call_window(
                state, callBase, argumentCount + 1u);
        state->callInfoList = callerCallInfo;
        state->stackTop.valuePointer = callerCallInfo->functionTop.valuePointer;
        aot_runtime_fail(
                state,
                runtimeState,
                "generated AOT direct call failed to prepare callee frame");
        return ZR_FALSE;
    }
    if (callInfo->functionTop.valuePointer <
        callInfo->functionBase.valuePointer + 1 + frameSlotCount) {
        callInfo->functionTop.valuePointer =
                callInfo->functionBase.valuePointer + 1 + frameSlotCount;
    }

    directCall->callerCallInfo = callerCallInfo;
    directCall->calleeCallInfo = callInfo;
    aot_runtime_record_direct_call_context(frame, calleeFunctionIndex, directCall);
    directCall->prepared = ZR_TRUE;
    return ZR_TRUE;
}

/* 生成调用先经 core call binding 核对目标，再寻找同项目的 AOT thunk；
 * 找不到可直接调用的 thunk 时保留 prepared=false，让生成器走通用调用路径。 */
static TZrBool aot_runtime_try_prepare_direct_call(SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 functionSlot,
                                                   TZrUInt32 argumentCount,
                                                   ZrAotGeneratedDirectCall *directCall) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer callBase;
    SZrTypeValue *functionValue;
    SZrClosureNative *closureNative;
    SZrLibraryAotLoadedModule *record;
    SZrFunction *metadataFunction;
    TZrUInt32 functionIndex;
    const SZrCallBindingTarget *boundTarget = ZR_NULL;

    if (directCall != ZR_NULL) {
        memset(directCall, 0, sizeof(*directCall));
    }

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callBase = aot_runtime_frame_slot(frame, functionSlot);
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL || runtimeState == ZR_NULL || callBase == ZR_NULL) {
        return ZR_FALSE;
    }

    functionValue = ZrCore_Stack_GetValue(callBase);
    if (!aot_prepare_call_binding(state, frame, functionValue)) return ZR_FALSE;
    if (frame->function != ZR_NULL && frame->function->callBindingInstructionMap != ZR_NULL &&
        frame->currentInstructionIndex < frame->function->callBindingInstructionMapLength) {
        TZrUInt32 mapped = frame->function->callBindingInstructionMap[frame->currentInstructionIndex];
        if (mapped != 0u && mapped <= frame->function->callSiteCacheLength &&
            frame->function->callSiteCaches[mapped - 1u].binding.target.targetKind == ZR_CALL_BINDING_TARGET_AOT)
            boundTarget = &frame->function->callSiteCaches[mapped - 1u].binding.target;
    }
    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, functionValue);
    if (metadataFunction == ZR_NULL) {
        return ZR_TRUE;
    }

    if (functionValue->type == ZR_VALUE_TYPE_CLOSURE && functionValue->isNative) {
        closureNative = ZR_CAST_NATIVE_CLOSURE(state, functionValue->value.object);
        if (closureNative == ZR_NULL || closureNative->nativeFunction == ZR_NULL) {
            return ZR_TRUE;
        }
    }

    record = boundTarget != ZR_NULL
            ? aot_runtime_find_record_for_bound_target(runtimeState, boundTarget)
            : aot_runtime_find_record_for_function(runtimeState, metadataFunction);
    if (boundTarget != ZR_NULL && record == ZR_NULL) {
        state->lastCallBindingError.status = ZR_CALL_BINDING_TARGET_NOT_FOUND;
        return ZR_FALSE;
    }
    if (record == ZR_NULL || record->codeRegistration == ZR_NULL ||
        record->codeRegistration->functionPointers == ZR_NULL || record->codeRegistration->functionCount == 0) {
        return ZR_TRUE;
    }

    functionIndex = boundTarget != ZR_NULL
            ? boundTarget->aot.methodInfo->functionIndex
            : aot_runtime_find_function_index_in_record(record, metadataFunction);
    if (functionIndex == UINT32_MAX || functionIndex >= record->codeRegistration->functionCount ||
        record->codeRegistration->functionPointers[functionIndex] == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!aot_runtime_prepare_vm_direct_call_frame(state,
                                                  frame,
                                                  destinationSlot,
                                                  functionSlot,
                                                  argumentCount,
                                                  record,
                                                  metadataFunction,
                                                  functionIndex,
                                                  ZR_NULL,
                                                  directCall)) {
        return ZR_FALSE;
    }

    directCall->nativeFunction = record->codeRegistration->functionPointers[functionIndex];
    return ZR_TRUE;
}

/* 把 receiver 调用改写为 callable 加 receiver 首参；原窗口需要多一个槽；成功会移动参数并增加 stackTop。 */
static TZrBool aot_runtime_prepare_meta_target(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrStackValuePointer callBase,
                                               TZrUInt32 argumentCount,
                                               SZrFunction **outMetadataFunction) {
    SZrTypeValue *receiverValue;
    SZrMeta *metaValue;
    SZrTypeValue callable;
    TZrBool hasBinding;
    TZrStackValuePointer cursor;

    if (outMetadataFunction != ZR_NULL) {
        *outMetadataFunction = ZR_NULL;
    }
    if (state == ZR_NULL || callBase == ZR_NULL) {
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(callBase);
    if (receiverValue == ZR_NULL ||
        !aot_prepare_meta_binding(state, frame, receiverValue, &callable, &hasBinding)) return ZR_FALSE;
    if (!hasBinding) {
        metaValue = ZrCore_Value_GetMeta(state, receiverValue, ZR_META_CALL);
        if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) return ZR_FALSE;
        ZrCore_Value_InitAsRawObject(state, &callable, ZR_CAST_RAW_OBJECT_AS_SUPER(metaValue->function));
    }

    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    for (cursor = state->stackTop.valuePointer; cursor > callBase; cursor--) {
        ZrCore_Stack_CopyValue(state, cursor, ZrCore_Stack_GetValue(cursor - 1));
    }
    state->stackTop.valuePointer++;

    ZrCore_Value_Copy(state, receiverValue, &callable);
    if (outMetadataFunction != ZR_NULL) {
        *outMetadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, &callable);
    }
    return ZR_TRUE;
}

static TZrBool aot_runtime_resolve_member_symbol(const SZrFunction *function,
                                                 TZrUInt32 memberId,
                                                 SZrString **outSymbol) {
    if (outSymbol != ZR_NULL) {
        *outSymbol = ZR_NULL;
    }
    if (function == ZR_NULL || function->memberEntries == ZR_NULL || memberId >= function->memberEntryLength ||
        outSymbol == ZR_NULL) {
        return ZR_FALSE;
    }

    *outSymbol = function->memberEntries[memberId].symbol;
    return *outSymbol != ZR_NULL;
}

static TZrBool aot_runtime_resolve_cached_member_symbol(const SZrFunction *function,
                                                        TZrUInt32 cacheIndex,
                                                        TZrUInt32 expectedKind,
                                                        SZrString **outSymbol) {
    const SZrFunctionCallSiteCacheEntry *cacheEntry;

    if (outSymbol != ZR_NULL) {
        *outSymbol = ZR_NULL;
    }
    if (function == ZR_NULL || function->callSiteCaches == ZR_NULL || function->memberEntries == ZR_NULL ||
        outSymbol == ZR_NULL || cacheIndex >= function->callSiteCacheLength) {
        return ZR_FALSE;
    }

    cacheEntry = &function->callSiteCaches[cacheIndex];
    if (cacheEntry->kind != expectedKind || cacheEntry->memberEntryIndex >= function->memberEntryLength) {
        return ZR_FALSE;
    }

    *outSymbol = function->memberEntries[cacheEntry->memberEntryIndex].symbol;
    return *outSymbol != ZR_NULL;
}

/* 为活动模块 shim 建 VM closure 并执行一次通用调用；返回结果位置由栈锚点恢复，成功要求 threadStatus FINE。 */
static TZrBool aot_runtime_execute_vm_shim_direct(SZrState *state,
                                                  SZrFunction *function,
                                                  TZrStackValuePointer *outResultBase) {
    SZrClosure *closure;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor anchor;
    SZrTypeValue *closureValue;

    if (outResultBase != ZR_NULL) {
        *outResultBase = ZR_NULL;
    }
    if (state == ZR_NULL || function == ZR_NULL || outResultBase == ZR_NULL) {
        return ZR_FALSE;
    }

    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->function = function;
    ZrCore_Closure_InitValue(state, closure);
    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, function->stackSize + 1, base, base, &anchor);
    closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

    *outResultBase = ZrCore_Function_CallAndRestoreAnchor(state, &anchor, 1);
    return state->threadStatus == ZR_THREAD_STATUS_FINE && *outResultBase != ZR_NULL;
}

/* 让VM-shim投影及native closure绑定共用捕获值访问；同时支持VM/native closure；值指针借用capture owner，不能脱离owner保活。 */
static SZrTypeValue *aot_runtime_get_closure_capture_from_value(SZrState *state,
                                                                const SZrTypeValue *closureContainerValue,
                                                                TZrUInt32 captureIndex) {
    if (state == ZR_NULL || closureContainerValue == ZR_NULL || closureContainerValue->type != ZR_VALUE_TYPE_CLOSURE ||
        closureContainerValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (closureContainerValue->isNative) {
        SZrClosureNative *nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, closureContainerValue->value.object);
        if (nativeClosure == ZR_NULL || captureIndex >= nativeClosure->closureValueCount) {
            return ZR_NULL;
        }
        return ZrCore_ClosureNative_GetCaptureValue(nativeClosure, captureIndex);
    }

    {
        SZrClosure *vmClosure = ZR_CAST_VM_CLOSURE(state, closureContainerValue->value.object);
        if (vmClosure == ZR_NULL || captureIndex >= vmClosure->closureValueCount) {
            return ZR_NULL;
        }
        return vmClosure->closureValuesExtend[captureIndex] != ZR_NULL
                       ? ZrCore_ClosureValue_GetValue(vmClosure->closureValuesExtend[captureIndex])
                       : ZR_NULL;
    }
}

/* 为native closure记录捕获值的真实GC owner；VM捕获owner为ClosureValue，native捕获沿用已有owner；null不等于值不存在。 */
static SZrRawObject *aot_runtime_get_closure_capture_owner_from_value(SZrState *state,
                                                                      const SZrTypeValue *closureContainerValue,
                                                                      TZrUInt32 captureIndex) {
    if (state == ZR_NULL || closureContainerValue == ZR_NULL || closureContainerValue->type != ZR_VALUE_TYPE_CLOSURE ||
        closureContainerValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (closureContainerValue->isNative) {
        SZrClosureNative *nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, closureContainerValue->value.object);
        if (nativeClosure == ZR_NULL || captureIndex >= nativeClosure->closureValueCount) {
            return ZR_NULL;
        }
        return ZrCore_ClosureNative_GetCaptureOwner(nativeClosure, captureIndex);
    }

    {
        SZrClosure *vmClosure = ZR_CAST_VM_CLOSURE(state, closureContainerValue->value.object);
        if (vmClosure == ZR_NULL || captureIndex >= vmClosure->closureValueCount ||
            vmClosure->closureValuesExtend[captureIndex] == ZR_NULL) {
            return ZR_NULL;
        }
        return ZR_CAST_RAW_OBJECT_AS_SUPER(vmClosure->closureValuesExtend[captureIndex]);
    }
}

/* AOT closure 需要退回 VM shim 执行时，把捕获值和自引用重建到 VM closure；
 * 捕获槽仍由 core 管理，复制时维持 GC 屏障。 */
static TZrBool aot_runtime_project_closure_into_vm_shim(SZrState *state,
                                                        const SZrTypeValue *sourceClosureValue,
                                                        SZrFunction *shimFunction,
                                                        SZrClosure **outClosure) {
    SZrClosure *closure;
    TZrUInt32 captureCount;
    SZrTypeValue projectedSelfValue;

    if (outClosure != ZR_NULL) {
        *outClosure = ZR_NULL;
    }
    if (state == ZR_NULL || sourceClosureValue == ZR_NULL || shimFunction == ZR_NULL || outClosure == ZR_NULL) {
        return ZR_FALSE;
    }

    captureCount = shimFunction->closureValueLength;
    closure = ZrCore_Closure_New(state, captureCount);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->function = shimFunction;
    ZrCore_Closure_InitValue(state, closure);
    ZrCore_Value_InitAsRawObject(state, &projectedSelfValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    projectedSelfValue.type = ZR_VALUE_TYPE_CLOSURE;
    projectedSelfValue.isGarbageCollectable = ZR_TRUE;
    projectedSelfValue.isNative = ZR_FALSE;

    for (TZrUInt32 captureIndex = 0; captureIndex < captureCount; captureIndex++) {
        SZrClosureValue *destinationCapture = closure->closureValuesExtend[captureIndex];
        SZrTypeValue *sourceCaptureValue =
                aot_runtime_get_closure_capture_from_value(state, sourceClosureValue, captureIndex);
        SZrTypeValue *destinationCaptureValue;

        if (destinationCapture == ZR_NULL || sourceCaptureValue == ZR_NULL) {
            return ZR_FALSE;
        }

        if (sourceCaptureValue->type == ZR_VALUE_TYPE_CLOSURE &&
            sourceClosureValue->type == ZR_VALUE_TYPE_CLOSURE &&
            sourceCaptureValue->value.object == sourceClosureValue->value.object) {
            sourceCaptureValue = &projectedSelfValue;
        }

        destinationCaptureValue = ZrCore_ClosureValue_GetValue(destinationCapture);
        if (destinationCaptureValue == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Value_Copy(state, destinationCaptureValue, sourceCaptureValue);
        ZrCore_Gc_WriteBarrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(destinationCapture), sourceCaptureValue);
    }

    *outClosure = closure;
    return ZR_TRUE;
}

/* 让native closure既引用捕获值地址又追踪其GC owner；owner存在时对owner做barrier，否则对capture value做barrier；不复制值槽。 */
static TZrBool aot_runtime_bind_native_closure_capture(SZrState *state,
                                                       SZrClosureNative *destinationClosure,
                                                       TZrUInt32 destinationIndex,
                                                       SZrTypeValue *captureValue,
                                                       SZrRawObject *captureOwner) {
    SZrRawObject **captureOwners;

    if (state == ZR_NULL || destinationClosure == ZR_NULL || captureValue == ZR_NULL ||
        destinationIndex >= destinationClosure->closureValueCount) {
        return ZR_FALSE;
    }

    captureOwners = ZrCore_ClosureNative_GetCaptureOwners(destinationClosure);
    destinationClosure->closureValuesExtend[destinationIndex] = captureValue;
    if (captureOwners != ZR_NULL) {
        captureOwners[destinationIndex] = captureOwner;
    }
    if (captureOwner != ZR_NULL) {
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(destinationClosure), captureOwner);
    } else {
        ZrCore_Gc_WriteBarrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(destinationClosure), captureValue);
    }
    return ZR_TRUE;
}

/* 让静态direct staging或无frame物化复用源closure捕获关系；逐捕获绑定值与owner；失败可能已部分写入未发布closure。 */
static TZrBool aot_runtime_bind_native_closure_captures_from_source(SZrState *state,
                                                                    SZrClosureNative *destinationClosure,
                                                                    const SZrTypeValue *source,
                                                                    TZrUInt32 captureCount) {
    TZrUInt32 captureIndex;

    if (captureCount == 0) {
        return ZR_TRUE;
    }
    if (state == ZR_NULL || destinationClosure == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    for (captureIndex = 0; captureIndex < captureCount; captureIndex++) {
        SZrTypeValue *captureValue = aot_runtime_get_closure_capture_from_value(state, source, captureIndex);
        SZrRawObject *captureOwner = aot_runtime_get_closure_capture_owner_from_value(state, source, captureIndex);
        if (!aot_runtime_bind_native_closure_capture(state,
                                                     destinationClosure,
                                                     captureIndex,
                                                     captureValue,
                                                     captureOwner)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/* 生成函数构造 native closure 时，以当前帧或外层 closure 的真实捕获 owner 绑定槽，
 * 让 GC 在生成 thunk 的整个寿命内追踪捕获对象。 */
static TZrBool aot_runtime_bind_native_closure_captures_from_frame(SZrState *state,
                                                                   const ZrAotGeneratedFrame *frame,
                                                                   SZrClosureNative *destinationClosure,
                                                                   SZrFunction *metadataFunction) {
    const SZrTypeValue *currentClosureValue;
    TZrUInt32 captureIndex;

    if (metadataFunction == ZR_NULL || metadataFunction->closureValueLength == 0) {
        return ZR_TRUE;
    }
    if (state == ZR_NULL || frame == ZR_NULL || frame->slotBase == ZR_NULL || destinationClosure == ZR_NULL ||
        metadataFunction->closureValueList == ZR_NULL) {
        return ZR_FALSE;
    }

    currentClosureValue = ZrCore_Stack_GetValue(frame->slotBase - 1);
    for (captureIndex = 0; captureIndex < metadataFunction->closureValueLength; captureIndex++) {
        const SZrFunctionClosureVariable *closureVariable = &metadataFunction->closureValueList[captureIndex];
        SZrTypeValue *captureValue = ZR_NULL;
        SZrRawObject *captureOwner = ZR_NULL;

        if (closureVariable->inStack) {
            SZrClosureValue *closureValue = ZrCore_Closure_FindOrCreateValue(state, frame->slotBase + closureVariable->index);
            if (closureValue != ZR_NULL) {
                captureValue = ZrCore_ClosureValue_GetValue(closureValue);
                captureOwner = ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue);
            }
        } else {
            captureValue = aot_runtime_get_closure_capture_from_value(state, currentClosureValue, closureVariable->index);
            captureOwner =
                    aot_runtime_get_closure_capture_owner_from_value(state, currentClosureValue, closureVariable->index);
        }

        if (!aot_runtime_bind_native_closure_capture(state,
                                                     destinationClosure,
                                                     captureIndex,
                                                     captureValue,
                                                     captureOwner)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/* 常量函数或 VM closure 只有在 record 中找到匹配 thunk 后才能变成 native closure；
 * 生成帧的捕获优先，缺少帧时才从源 closure 复制捕获关系。 */
static TZrBool aot_runtime_materialize_callable_constant_with_context(SZrState *state,
                                                                      SZrLibraryAotLoadedModule *record,
                                                                      const SZrTypeValue *source,
                                                                      const ZrAotGeneratedFrame *frame,
                                                                      TZrBool forceClosure,
                                                                      SZrTypeValue *destination) {
    SZrFunction *metadataFunction;
    TZrUInt32 functionIndex;
    SZrClosureNative *closure;
    SZrLibraryAotRuntimeState *runtimeState;
    TZrUInt32 captureCount;
    TZrBool capturesBound = ZR_FALSE;

    if (state == ZR_NULL || record == ZR_NULL || source == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }

    runtimeState = state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, source);
    if (metadataFunction == ZR_NULL) {
        ZrCore_Value_Copy(state, destination, source);
        return ZR_TRUE;
    }

    if (record->codeRegistration == ZR_NULL || record->codeRegistration->functionPointers == ZR_NULL ||
        record->codeRegistration->functionCount == 0) {
        ZrCore_Value_Copy(state, destination, source);
        return ZR_TRUE;
    }

    functionIndex = aot_runtime_find_function_index_in_record(record, metadataFunction);
    if (functionIndex == UINT32_MAX || functionIndex >= record->codeRegistration->functionCount ||
        record->codeRegistration->functionPointers[functionIndex] == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT callable thunk missing for module '%s' function index %u",
                         record->moduleName != ZR_NULL ? record->moduleName : "<unknown>",
                         (unsigned)functionIndex);
        return ZR_FALSE;
    }

    captureCount = metadataFunction->closureValueLength;
    closure = ZrCore_ClosureNative_New(state, captureCount);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->nativeFunction = (FZrNativeFunction)record->codeRegistration->functionPointers[functionIndex];
    closure->aotShimFunction = metadataFunction;

    if (captureCount > 0) {
        if (frame != ZR_NULL) {
            capturesBound = aot_runtime_bind_native_closure_captures_from_frame(state, frame, closure, metadataFunction);
        }
        if (!capturesBound && source->type == ZR_VALUE_TYPE_CLOSURE && source->value.object != ZR_NULL) {
            capturesBound = aot_runtime_bind_native_closure_captures_from_source(state, closure, source, captureCount);
        }
        if (!capturesBound) {
            aot_runtime_fail(state,
                             runtimeState,
                             "AOT native closure capture binding failed for module '%s' function index %u",
                             record->moduleName != ZR_NULL ? record->moduleName : "<unknown>",
                             (unsigned)functionIndex);
            return ZR_FALSE;
        }
    }

    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    destination->type = forceClosure ? ZR_VALUE_TYPE_CLOSURE : ZR_VALUE_TYPE_CLOSURE;
    destination->isGarbageCollectable = ZR_TRUE;
    destination->isNative = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool aot_runtime_materialize_callable_constant(SZrState *state,
                                                         SZrLibraryAotLoadedModule *record,
                                                         const SZrTypeValue *source,
                                                         TZrBool forceClosure,
                                                         SZrTypeValue *destination) {
    return aot_runtime_materialize_callable_constant_with_context(state,
                                                                  record,
                                                                  source,
                                                                  ZR_NULL,
                                                                  forceClosure,
                                                                  destination);
}

/* 在生成模块返回/VM shim完成时发布可被后续import消费的值；public/protected出口复制到module；callable先物化；成功后moduleExecuted=true。 */
static TZrBool aot_runtime_materialize_exports(SZrState *state,
                                               SZrLibraryAotLoadedModule *record,
                                               TZrStackValuePointer slotBase) {
    TZrStackValuePointer exportedValuesTop;

    if (state == ZR_NULL || record == ZR_NULL || record->module == ZR_NULL || record->moduleFunction == ZR_NULL ||
        slotBase == ZR_NULL) {
        return ZR_FALSE;
    }

    if (record->moduleFunction->exportedVariables == ZR_NULL || record->moduleFunction->exportedVariableLength == 0) {
        record->moduleExecuted = ZR_TRUE;
        return ZR_TRUE;
    }

    exportedValuesTop = slotBase + record->moduleFunction->stackSize;
    for (TZrUInt32 index = 0; index < record->moduleFunction->exportedVariableLength; index++) {
        struct SZrFunctionExportedVariable *exportVar = &record->moduleFunction->exportedVariables[index];
        TZrStackValuePointer varPointer;
        SZrTypeValue *varValue;
        SZrTypeValue publishedValue;

        if (exportVar->name == ZR_NULL) {
            continue;
        }

        varPointer = slotBase + exportVar->stackSlot;
        if (varPointer >= exportedValuesTop) {
            continue;
        }

        varValue = ZrCore_Stack_GetValue(varPointer);
        if (varValue == ZR_NULL) {
            continue;
        }

        ZrCore_Value_ResetAsNull(&publishedValue);
        if (!aot_runtime_materialize_callable_constant(state, record, varValue, ZR_TRUE, &publishedValue)) {
            return ZR_FALSE;
        }

        if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
            ZrCore_Module_AddPubExport(state, record->module, exportVar->name, &publishedValue);
        } else if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
            ZrCore_Module_AddProExport(state, record->module, exportVar->name, &publishedValue);
        }
    }

    record->moduleExecuted = ZR_TRUE;
    return ZR_TRUE;
}

/* 从项目/模块加载链调用描述符入口：暂设 activeRecord，以便 native closure shim 找到当前函数图；
 * 运行结果经 VM 栈复制给项目调用者，恢复 activeRecord 后不再借用入口栈位置。 */
/* TODO: 核查合法嵌套 AOT 导入在第 5/9 个记录追加时跨 records 扩容，并在返回后继续消费 frame/context/activeRecord 的完整调用链；若 realloc 迁移数组，旧元素借用地址会失效，须验证实际可达路径及后续重定位责任，尚无完整合法触发或实测证明。 */

static TZrBool aot_runtime_call_record_direct(SZrState *state,
                                              SZrLibraryAotRuntimeState *runtimeState,
                                              SZrLibraryAotLoadedModule *record,
                                              TZrBool captureResult,
                                              SZrTypeValue *result) {
    SZrClosureNative *entryClosure;
    SZrLibraryAotLoadedModule *savedRecord;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor anchor;
    SZrTypeValue *closureValue;
    TZrStackValuePointer resultBase;

    FZrAotEntryThunk entryThunk = ZR_NULL;

    if (record != ZR_NULL && record->descriptor != ZR_NULL) {
        entryThunk = record->descriptor->entryThunk;
    }

    if (state == ZR_NULL || runtimeState == ZR_NULL || record == ZR_NULL || entryThunk == ZR_NULL) {
        return ZR_FALSE;
    }

    entryClosure = ZrCore_ClosureNative_New(state, 0);
    if (entryClosure == ZR_NULL) {
        return ZR_FALSE;
    }

    entryClosure->nativeFunction = (FZrNativeFunction)entryThunk;
    entryClosure->aotShimFunction = record->moduleFunction;
    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, 1, base, base, &anchor);
    closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(entryClosure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_TRUE;
    state->stackTop.valuePointer++;

    savedRecord = runtimeState->activeRecord;
    runtimeState->activeRecord = record;
    aot_runtime_mark_record_executed(runtimeState, record);
    resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &anchor, captureResult ? 1 : 0);
    runtimeState->activeRecord = savedRecord;

    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    if (captureResult) {
        if (result == ZR_NULL || resultBase == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(resultBase));
    }
    return ZR_TRUE;
}

/* 记录实际进入AOT后端而不把载入成功误记为执行成功；只更新runtimeState.executedVia；不设置moduleExecuted。 */
static void aot_runtime_mark_record_executed(SZrLibraryAotRuntimeState *runtimeState,
                                             const SZrLibraryAotLoadedModule *record) {
    if (runtimeState != ZR_NULL && record != ZR_NULL) {
        runtimeState->executedVia = aot_runtime_backend_to_executed_via(record->backendKind);
    }
}

static EZrLibraryExecutedVia aot_runtime_backend_to_executed_via(EZrAotBackendKind backendKind) {
    switch (backendKind) {
        case ZR_AOT_BACKEND_KIND_C:
            return ZR_LIBRARY_EXECUTED_VIA_AOT_C;
        case ZR_AOT_BACKEND_KIND_LLVM:
            return ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM;
        case ZR_AOT_BACKEND_KIND_NONE:
        default:
            return ZR_LIBRARY_EXECUTED_VIA_NONE;
    }
}

static const TZrChar *aot_runtime_backend_diagnostic_name(EZrAotBackendKind backendKind) {
    switch (backendKind) {
        case ZR_AOT_BACKEND_KIND_C:
            return "aot-c";
        case ZR_AOT_BACKEND_KIND_LLVM:
            return "aot-llvm";
        case ZR_AOT_BACKEND_KIND_NONE:
        default:
            return "aot";
    }
}

/* 把AOT最近错误补入core模块导入诊断；已有core诊断不覆盖；lastError为空不制造失败说明。 */
static void aot_runtime_report_module_load_failure(SZrState *state,
                                                   const SZrLibraryAotRuntimeState *runtimeState,
                                                   EZrAotBackendKind backendKind,
                                                   SZrString *moduleName,
                                                   const TZrChar *result) {
    const TZrChar *detail;
    const TZrChar *moduleNameText;

    if (state == ZR_NULL || state->global == ZR_NULL || runtimeState == ZR_NULL ||
        runtimeState->lastError[0] == '\0' ||
        ZrCore_GlobalState_GetModuleLoadDiagnostic(state->global) != ZR_NULL) {
        return;
    }

    detail = runtimeState->lastError;
    moduleNameText = moduleName != ZR_NULL ? ZrCore_String_GetNativeString(moduleName) : ZR_NULL;
    ZrCore_GlobalState_SetModuleLoadDiagnostic(state->global,
                                               "loader=aot-runtime backend=%s result=%s module='%s' detail=%s",
                                               aot_runtime_backend_diagnostic_name(backendKind),
                                               result != ZR_NULL ? result : "load-failed",
                                               moduleNameText != ZR_NULL ? moduleNameText : "<unknown>",
                                               detail);
}

/* 项目入口与 core ModuleLoader 共用的记录门禁：定位 provider 产物，校验动态库 ABI 与元数据，
 * 将函数图和模块对象 pin 住后才把记录发布到缓存；失败路径负责收回尚未移交的资源。 */
static TZrBool aot_runtime_prepare_record(SZrState *state,
                                          SZrLibraryAotRuntimeState *runtimeState,
                                          EZrAotBackendKind backendKind,
                                          const TZrChar *moduleName,
                                          SZrLibraryAotLoadedModule **outRecord) {
    SZrGlobalState *global;
    SZrLibrary_Project *project;
    SZrLibraryAotLoadedModule *existing;
    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH] = {0};
    TZrChar sourcePath[ZR_LIBRARY_MAX_PATH_LENGTH] = {0};
    TZrChar zroPath[ZR_LIBRARY_MAX_PATH_LENGTH] = {0};
    TZrChar libraryPath[ZR_LIBRARY_MAX_PATH_LENGTH] = {0};
    TZrChar providerError[ZR_LIBRARY_MAX_PATH_LENGTH] = {0};
    TZrChar sourceHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    SZrLibrary_ProjectImportProviderAotLoadRequest providerLoadRequest;
    void *handle;
    FZrVmGetAotCompiledModule descriptorSymbol;
    const ZrAotCompiledModule *descriptor = ZR_NULL;
    const TZrChar *descriptorModuleName = ZR_NULL;
    SZrFunction *moduleFunction = ZR_NULL;
    SZrFunction **functionTable = ZR_NULL;
    TZrUInt32 functionCount = 0;
    TZrUInt32 functionCapacity = 0;
    TZrUInt32 *generatedFrameSlotCounts = ZR_NULL;
    SZrGcNativeCallPin *functionPins = ZR_NULL;
    SZrLibraryAotLoadedModule record;
    SZrString *moduleNameString;
    SZrMetadataRuntime *metadataRuntime;
    TZrBool sourceExists;
    TZrBool zroExists;
    TZrBool providerRequest = ZR_FALSE;

    if (outRecord != ZR_NULL) {
        *outRecord = ZR_NULL;
    }
    if (state == ZR_NULL || state->global == ZR_NULL || runtimeState == ZR_NULL || moduleName == ZR_NULL ||
        outRecord == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    project = aot_runtime_get_project(global);
    if (project == ZR_NULL || !aot_runtime_normalize_module_name(moduleName, normalizedModule, sizeof(normalizedModule))) {
        return ZR_FALSE;
    }

    existing = aot_runtime_find_record(runtimeState, backendKind, normalizedModule);
    if (existing != ZR_NULL) {
        *outRecord = existing;
        return ZR_TRUE;
    }

    descriptorModuleName = normalizedModule;
    memset(&providerLoadRequest, 0, sizeof(providerLoadRequest));
    /* TODO: 核查合法嵌套包导入的依赖 owner：此请求使用根 entry，需对照编译后的 $包@版本/module 名、provider 依赖图及产物目录回退，验证私有依赖能否在实际构建和 loader 链中错误解析；尚无完整合法触发证明。 */

    if (normalizedModule[0] == '$' && strchr(normalizedModule, '@') != ZR_NULL &&
        ZrLibrary_Project_ResolveImportProviderAotLoadRequest(project,
                                                              ZrCore_String_GetNativeString(project->entry),
                                                              normalizedModule,
                                                              backendKind,
                                                              &providerLoadRequest,
                                                              providerError,
                                                              sizeof(providerError))) {
        providerRequest = ZR_TRUE;
        snprintf(sourcePath, sizeof(sourcePath), "%s", providerLoadRequest.sourcePath);
        snprintf(zroPath, sizeof(zroPath), "%s", providerLoadRequest.binaryPath);
        snprintf(libraryPath, sizeof(libraryPath), "%s", providerLoadRequest.libraryPath);
        if (providerLoadRequest.descriptorModuleName[0] != '\0') {
            descriptorModuleName = providerLoadRequest.descriptorModuleName;
        }
    } else {
        aot_runtime_resolve_module_file(project,
                                        ZrCore_String_GetNativeString(project->source),
                                        normalizedModule,
                                        ZR_VM_SOURCE_MODULE_FILE_EXTENSION,
                                        sourcePath,
                                        sizeof(sourcePath));
        aot_runtime_resolve_module_file(project,
                                        ZrCore_String_GetNativeString(project->binary),
                                        normalizedModule,
                                        ZR_VM_BINARY_MODULE_FILE_EXTENSION,
                                        zroPath,
                                        sizeof(zroPath));
    }

    sourceExists = sourcePath[0] != '\0' && ZrLibrary_File_Exist(sourcePath) == ZR_LIBRARY_FILE_IS_FILE;
    zroExists = zroPath[0] != '\0' && ZrLibrary_File_Exist(zroPath) == ZR_LIBRARY_FILE_IS_FILE;

    if (providerRequest && providerLoadRequest.artifactKind == ZR_LIBRARY_PROJECT_DEPENDENCY_PACKAGE_ZRM) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT provider archive entries are not dynamic libraries for module '%s'",
                         normalizedModule);
        return ZR_FALSE;
    }

    if ((!providerRequest &&
         !aot_runtime_resolve_library_path(project, backendKind, normalizedModule, libraryPath, sizeof(libraryPath))) ||
        libraryPath[0] == '\0' ||
        ZrLibrary_File_Exist(libraryPath) != ZR_LIBRARY_FILE_IS_FILE) {
        if (providerRequest) {
            aot_runtime_fail(state,
                             runtimeState,
                             "missing AOT provider library '%s' for module '%s'",
                             libraryPath,
                             normalizedModule);
            return ZR_FALSE;
        }
        if (!sourceExists && !zroExists) {
            aot_runtime_set_error(runtimeState, ZR_NULL);
            return ZR_FALSE;
        }
        aot_runtime_fail(state, runtimeState, "missing AOT artifacts for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    handle = aot_runtime_open_library(libraryPath);
    if (handle == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "failed to load AOT library '%s'", libraryPath);
        return ZR_FALSE;
    }

    descriptorSymbol =
            aot_runtime_cast_descriptor_symbol(aot_runtime_find_symbol(handle, "ZrVm_GetAotCompiledModule"));
    if (descriptorSymbol == ZR_NULL) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "AOT library '%s' is missing ZrVm_GetAotCompiledModule", libraryPath);
        return ZR_FALSE;
    }

    descriptor = descriptorSymbol();
    if (!aot_runtime_validate_descriptor(state, runtimeState, descriptor, backendKind, descriptorModuleName)) {
        aot_runtime_close_library(handle);
        return ZR_FALSE;
    }

    /* TODO: 核查已存在 source/zro 且 descriptor 声明 inputHash 时的读取失败：当前 hash_file 的 false 使匹配条件为假，需沿合法产物构建、文件可读性变化及装载后续证明是否跳过应有的拒绝；尚无完整合法触发证明。 */

    if ((EZrAotInputKind)descriptor->inputKind == ZR_AOT_INPUT_KIND_SOURCE && sourceExists &&
        descriptor->inputHash != ZR_NULL && descriptor->inputHash[0] != '\0' &&
        aot_runtime_hash_file(sourcePath, sourceHash, sizeof(sourceHash)) &&
        strcmp(descriptor->inputHash, sourceHash) != 0) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "AOT source hash mismatch for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    if ((EZrAotInputKind)descriptor->inputKind == ZR_AOT_INPUT_KIND_BINARY && zroExists &&
        descriptor->inputHash != ZR_NULL && descriptor->inputHash[0] != '\0' &&
        aot_runtime_hash_file(zroPath, zroHash, sizeof(zroHash)) &&
        strcmp(descriptor->inputHash, zroHash) != 0) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "AOT binary hash mismatch for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    if (descriptor->embeddedModuleBlob != ZR_NULL && descriptor->embeddedModuleBlobLength > 0) {
        if (!aot_runtime_load_embedded_function(state,
                                                descriptor->embeddedModuleBlob,
                                                descriptor->embeddedModuleBlobLength,
                                                &moduleFunction)) {
            aot_runtime_close_library(handle);
            aot_runtime_fail(state, runtimeState, "failed to load embedded module blob for module '%s'", normalizedModule);
            return ZR_FALSE;
        }
    } else if (!zroExists || !aot_runtime_load_zro_function(state, zroPath, &moduleFunction)) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "failed to load companion zro for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    if (!aot_runtime_build_function_table(state, moduleFunction, &functionTable, &functionCount, &functionCapacity) ||
        functionCount == 0) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "failed to build AOT function table for module '%s'", normalizedModule);
        return ZR_FALSE;
    }
    if (!aot_runtime_build_generated_slot_count_table(global, functionTable, functionCount, &generatedFrameSlotCounts)) {
        aot_runtime_close_library(handle);
        if (functionTable != ZR_NULL) {
            aot_runtime_reallocate(global, functionTable, sizeof(*functionTable) * functionCapacity, 0);
        }
        aot_runtime_fail(state,
                         runtimeState,
                         "failed to cache generated frame slot counts for module '%s'",
                         normalizedModule);
        return ZR_FALSE;
    }

    if ((descriptor->codeRegistration->functionPointers != ZR_NULL || descriptor->codeRegistration->functionCount != 0) &&
        (descriptor->codeRegistration->functionPointers == ZR_NULL ||
         descriptor->codeRegistration->functionCount != functionCount)) {
        TZrUInt32 descriptorThunkCount = descriptor != ZR_NULL ? descriptor->codeRegistration->functionCount : 0;
        aot_runtime_close_library(handle);
        if (generatedFrameSlotCounts != ZR_NULL) {
            aot_runtime_reallocate(global, generatedFrameSlotCounts, sizeof(*generatedFrameSlotCounts) * functionCount, 0);
        }
        if (functionTable != ZR_NULL) {
            aot_runtime_reallocate(global, functionTable, sizeof(*functionTable) * functionCapacity, 0);
        }
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT thunk table mismatch for module '%s' descriptor=%u runtime=%u",
                         normalizedModule,
                         (unsigned)descriptorThunkCount,
                         (unsigned)functionCount);
        return ZR_FALSE;
    }

    functionPins = aot_runtime_pin_function_table(state, functionTable, functionCount);
    if (functionPins == ZR_NULL) {
        aot_runtime_close_library(handle);
        if (generatedFrameSlotCounts != ZR_NULL) {
            aot_runtime_reallocate(global, generatedFrameSlotCounts, sizeof(*generatedFrameSlotCounts) * functionCount, 0);
        }
        if (functionTable != ZR_NULL) {
            aot_runtime_reallocate(global, functionTable, sizeof(*functionTable) * functionCapacity, 0);
        }
        aot_runtime_fail(state, runtimeState, "failed to pin AOT function table for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    memset(&record, 0, sizeof(record));
    record.backendKind = backendKind;
    record.moduleName = aot_runtime_duplicate_string(global, normalizedModule);
    record.sourcePath = sourceExists ? aot_runtime_duplicate_string(global, sourcePath) : ZR_NULL;
    record.zroPath = zroExists ? aot_runtime_duplicate_string(global, zroPath) : ZR_NULL;
    record.libraryPath = aot_runtime_duplicate_string(global, libraryPath);
    record.libraryHandle = handle;
    record.descriptor = descriptor;
    aot_runtime_bind_record_code_registration(&record, descriptor);
    record.moduleFunction = moduleFunction;
    record.functionTable = functionTable;
    record.functionPins = functionPins;
    record.functionCount = functionCount;
    record.functionCapacity = functionCapacity;
    record.generatedFrameSlotCounts = generatedFrameSlotCounts;
    record.module = ZrCore_Module_Create(state);
    if (record.moduleName == ZR_NULL || record.libraryPath == ZR_NULL || record.module == ZR_NULL ||
        (zroExists && record.zroPath == ZR_NULL) ||
        !ZrCore_Gc_NativeCallPinObject(
                state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(record.module),
                &record.modulePin)) {
        aot_runtime_close_library(handle);
        aot_runtime_free_string(global, record.moduleName);
        aot_runtime_free_string(global, record.sourcePath);
        aot_runtime_free_string(global, record.zroPath);
        aot_runtime_free_string(global, record.libraryPath);
        if (generatedFrameSlotCounts != ZR_NULL) {
            aot_runtime_reallocate(global, generatedFrameSlotCounts, sizeof(*generatedFrameSlotCounts) * functionCount, 0);
        }
        if (functionTable != ZR_NULL) {
            aot_runtime_reallocate(global, functionTable, sizeof(*functionTable) * functionCapacity, 0);
        }
        ZrCore_Gc_NativeCallUnpin(global, &record.modulePin);
        aot_runtime_unpin_function_table(state, functionPins, functionCount);
        aot_runtime_fail(state, runtimeState, "failed to allocate AOT runtime record for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    moduleNameString = ZrCore_String_CreateFromNative(state, normalizedModule);
    if (moduleNameString != ZR_NULL) {
        TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, moduleNameString);
        ZrCore_Module_SetInfo(state, record.module, moduleNameString, pathHash, moduleNameString);
    }
    metadataRuntime = ZrCore_Module_AttachMetadataRuntime(record.module, record.moduleFunction, record.codeRegistration);
    if (metadataRuntime == ZR_NULL) {
        aot_runtime_close_library(handle);
        aot_runtime_free_string(global, record.moduleName);
        aot_runtime_free_string(global, record.sourcePath);
        aot_runtime_free_string(global, record.zroPath);
        aot_runtime_free_string(global, record.libraryPath);
        if (generatedFrameSlotCounts != ZR_NULL) {
            aot_runtime_reallocate(global, generatedFrameSlotCounts, sizeof(*generatedFrameSlotCounts) * functionCount, 0);
        }
        if (functionTable != ZR_NULL) {
            aot_runtime_reallocate(global, functionTable, sizeof(*functionTable) * functionCapacity, 0);
        }
        ZrCore_Gc_NativeCallUnpin(global, &record.modulePin);
        aot_runtime_unpin_function_table(state, functionPins, functionCount);
        aot_runtime_fail(state,
                         runtimeState,
                         "failed to attach AOT metadata runtime for module '%s'",
                         normalizedModule);
        return ZR_FALSE;
    }
    for (TZrUInt32 functionIndex = 0; functionIndex < functionCount; functionIndex++) {
        ZrCore_MetadataRuntime_AttachFunction(metadataRuntime, functionTable[functionIndex]);
    }
    if (!ZrCore_MetadataRuntime_LinkCallBindings(state, metadataRuntime)) {
        const SZrCallBindingDiagnostic *diagnostic = &state->lastCallBindingError;
        aot_runtime_close_library(handle);
        aot_runtime_free_string(global, record.moduleName);
        aot_runtime_free_string(global, record.sourcePath);
        aot_runtime_free_string(global, record.zroPath);
        aot_runtime_free_string(global, record.libraryPath);
        if (generatedFrameSlotCounts != ZR_NULL) {
            aot_runtime_reallocate(global, generatedFrameSlotCounts, sizeof(*generatedFrameSlotCounts) * functionCount, 0);
        }
        if (functionTable != ZR_NULL) {
            aot_runtime_reallocate(global, functionTable, sizeof(*functionTable) * functionCapacity, 0);
        }
        ZrCore_Gc_NativeCallUnpin(global, &record.modulePin);
        aot_runtime_unpin_function_table(state, functionPins, functionCount);
        aot_runtime_fail(state,
                         runtimeState,
                         "failed to link AOT call bindings for module '%s': "
                         "status=%s token=0x%08x instruction=%u candidate=%u "
                         "expected=0x%016llx actual=0x%016llx",
                         normalizedModule,
                         ZrCore_CallBinding_StatusName(diagnostic->status),
                         (unsigned)diagnostic->targetMetadataToken,
                         (unsigned)diagnostic->instructionIndex,
                         (unsigned)diagnostic->candidateIndex,
                         (unsigned long long)diagnostic->expected,
                         (unsigned long long)diagnostic->actual);
        return ZR_FALSE;
    }
    if (!aot_runtime_validate_metadata_bindings(state,
                                                runtimeState,
                                                normalizedModule,
                                                record.moduleFunction,
                                                functionTable,
                                                functionCount)) {
        aot_runtime_close_library(handle);
        aot_runtime_free_string(global, record.moduleName);
        aot_runtime_free_string(global, record.sourcePath);
        aot_runtime_free_string(global, record.zroPath);
        aot_runtime_free_string(global, record.libraryPath);
        if (generatedFrameSlotCounts != ZR_NULL) {
            aot_runtime_reallocate(global, generatedFrameSlotCounts, sizeof(*generatedFrameSlotCounts) * functionCount, 0);
        }
        if (functionTable != ZR_NULL) {
            aot_runtime_reallocate(global, functionTable, sizeof(*functionTable) * functionCapacity, 0);
        }
        ZrCore_Gc_NativeCallUnpin(global, &record.modulePin);
        aot_runtime_unpin_function_table(state, functionPins, functionCount);
        return ZR_FALSE;
    }
    ZrCore_Reflection_AttachModuleRuntimeMetadata(state, record.module, record.moduleFunction);
    ZrCore_Module_CreatePrototypesFromConstants(state, record.module, record.moduleFunction);

    if (!aot_runtime_append_record(global, runtimeState, &record, outRecord)) {
        aot_runtime_close_library(handle);
        aot_runtime_free_string(global, record.moduleName);
        aot_runtime_free_string(global, record.sourcePath);
        aot_runtime_free_string(global, record.zroPath);
        aot_runtime_free_string(global, record.libraryPath);
        if (generatedFrameSlotCounts != ZR_NULL) {
            aot_runtime_reallocate(global, generatedFrameSlotCounts, sizeof(*generatedFrameSlotCounts) * functionCount, 0);
        }
        if (functionTable != ZR_NULL) {
            aot_runtime_reallocate(global, functionTable, sizeof(*functionTable) * functionCapacity, 0);
        }
        ZrCore_Gc_NativeCallUnpin(global, &record.modulePin);
        aot_runtime_unpin_function_table(state, functionPins, functionCount);
        aot_runtime_fail(state, runtimeState, "failed to store AOT runtime record for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static void aot_runtime_execute_entry_body(SZrState *state, TZrPtr arguments) {
    ZrLibraryAotEntryRequest *request = (ZrLibraryAotEntryRequest *)arguments;

    if (request == ZR_NULL) {
        return;
    }
    request->success =
            aot_runtime_call_record_direct(state, request->runtimeState, request->record, ZR_TRUE, request->result);
}

/* 由项目执行模式进入 AOT；严格模式把 core 的 import callback 绑定到同一项目状态。 */
TZrBool ZrLibrary_AotRuntime_ConfigureGlobal(SZrGlobalState *global,
                                             EZrLibraryProjectExecutionMode executionMode,
                                             TZrBool requireAotPath) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_ensure_state(global);

    if (runtimeState == ZR_NULL) {
        return ZR_FALSE;
    }

    runtimeState->configuredExecutionMode = executionMode;
    runtimeState->requireAotPath = requireAotPath;
    runtimeState->strictProjectAot = executionMode == ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C ||
                                     executionMode == ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM;
    runtimeState->executedVia = ZR_LIBRARY_EXECUTED_VIA_NONE;
    runtimeState->activeRecord = ZR_NULL;
    aot_runtime_set_error(runtimeState, ZR_NULL);
    ZrCore_GlobalState_SetAotModuleLoader(global,
                                          runtimeState->strictProjectAot ? ZrLibrary_AotRuntime_ModuleLoader : ZR_NULL,
                                          runtimeState->strictProjectAot ? runtimeState : ZR_NULL);
    return ZR_TRUE;
}

/* project.c 的项目释放链撤销 GC pin 并转交动态库句柄；本函数返回时 GC 后清理尚未完成。 */
void ZrLibrary_AotRuntime_FreeProjectState(SZrState *state, SZrLibrary_Project *project) {
    SZrGlobalState *global;
    SZrLibraryAotRuntimeState *runtimeState;

    if (state == ZR_NULL || project == ZR_NULL) {
        return;
    }

    global = state->global;
    runtimeState = aot_runtime_get_state_from_project(project);
    if (runtimeState == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < runtimeState->recordCount; index++) {
        SZrLibraryAotLoadedModule *record = &runtimeState->records[index];
        ZrCore_Gc_NativeCallUnpin(global, &record->modulePin);
        aot_runtime_unpin_function_table(state, record->functionPins, record->functionCount);
        record->functionPins = ZR_NULL;
        /* TODO: 核查项目释放时 retire 失败的句柄归属：当前忽略返回值后清空记录句柄，需验证合法已有 postGcCleanup 注册或分配失败与项目销毁次序，确认是否仍有关闭者及是否造成泄漏；尚无完整合法触发证明。 */

        (void)aot_runtime_retire_library(global, record->libraryHandle);
        record->libraryHandle = ZR_NULL;
        aot_runtime_free_string(global, record->moduleName);
        aot_runtime_free_string(global, record->sourcePath);
        aot_runtime_free_string(global, record->zroPath);
        aot_runtime_free_string(global, record->libraryPath);
        if (record->generatedFrameSlotCounts != ZR_NULL && record->functionCount > 0) {
            aot_runtime_reallocate(global,
                                   record->generatedFrameSlotCounts,
                                   sizeof(*record->generatedFrameSlotCounts) * record->functionCount,
                                   0);
        }
        if (record->functionTable != ZR_NULL && record->functionCapacity > 0) {
            aot_runtime_reallocate(global,
                                   record->functionTable,
                                   sizeof(*record->functionTable) * record->functionCapacity,
                                   0);
        }
    }
    if (runtimeState->records != ZR_NULL) {
        aot_runtime_reallocate(global,
                               runtimeState->records,
                               runtimeState->recordCapacity * sizeof(*runtimeState->records),
                               0);
    }
    if (global != ZR_NULL &&
        global->aotModuleLoaderUserData == runtimeState) {
        ZrCore_GlobalState_SetAotModuleLoader(global, ZR_NULL, ZR_NULL);
    }
    aot_runtime_reallocate(global, runtimeState, sizeof(*runtimeState), 0);
    project->aotRuntime = ZR_NULL;
}

const TZrChar *ZrLibrary_AotRuntime_ExecutedViaName(EZrLibraryExecutedVia executedVia) {
    switch (executedVia) {
        case ZR_LIBRARY_EXECUTED_VIA_INTERP:
            return "interp";
        case ZR_LIBRARY_EXECUTED_VIA_BINARY:
            return "binary";
        case ZR_LIBRARY_EXECUTED_VIA_AOT_C:
            return "aot_c";
        case ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM:
            return "aot_llvm";
        case ZR_LIBRARY_EXECUTED_VIA_NONE:
        default:
            return "none";
    }
}

EZrLibraryExecutedVia ZrLibrary_AotRuntime_GetExecutedVia(SZrGlobalState *global) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_get_state_from_global(global);
    return runtimeState != ZR_NULL ? runtimeState->executedVia : ZR_LIBRARY_EXECUTED_VIA_NONE;
}

const TZrChar *ZrLibrary_AotRuntime_GetLastError(SZrGlobalState *global) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_get_state_from_global(global);
    if (runtimeState == ZR_NULL || runtimeState->lastError[0] == '\0') {
        return ZR_NULL;
    }
    return runtimeState->lastError;
}

void ZrLibrary_AotRuntime_RecordError(SZrState *state, TZrNativeString message) {
    SZrLibraryAotRuntimeState *runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;

    if (runtimeState == ZR_NULL) {
        return;
    }
    aot_runtime_set_error(runtimeState, "%s", message != ZR_NULL ? message : "generated AOT runtime error");
}

/* core 模块加载器仅借用项目 AOT 状态；同一记录的模块体最多执行一次，导入方复用 module 对象。 */
/* TODO: 核查合法嵌套 AOT 导入在第 5/9 个记录追加时跨 records 扩容，并在返回后继续消费 frame/context/activeRecord 的完整调用链；若 realloc 迁移数组，旧元素借用地址会失效，须验证实际可达路径及后续重定位责任，尚无完整合法触发或实测证明。 */

SZrObjectModule *ZrLibrary_AotRuntime_ModuleLoader(SZrState *state, SZrString *moduleName, TZrPtr userData) {
    SZrLibraryAotRuntimeState *runtimeState = (SZrLibraryAotRuntimeState *)userData;
    EZrAotBackendKind backendKind;
    SZrLibraryAotLoadedModule *record = ZR_NULL;

    if (state == ZR_NULL || runtimeState == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    backendKind = runtimeState->configuredExecutionMode == ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM
                          ? ZR_AOT_BACKEND_KIND_LLVM
                          : ZR_AOT_BACKEND_KIND_C;

    if (!aot_runtime_prepare_record(state,
                                    runtimeState,
                                    backendKind,
                                    ZrCore_String_GetNativeString(moduleName),
                                    &record) ||
        record == ZR_NULL) {
        aot_runtime_report_module_load_failure(state,
                                               runtimeState,
                                               backendKind,
                                               moduleName,
                                               "descriptor-load-failed");
        return ZR_NULL;
    }

    if (!record->moduleExecuted &&
        !aot_runtime_call_record_direct(state, runtimeState, record, ZR_FALSE, ZR_NULL)) {
        aot_runtime_report_module_load_failure(state,
                                               runtimeState,
                                               backendKind,
                                               moduleName,
                                               "module-execute-failed");
        return ZR_NULL;
    }

    return record->module;
}

/* 项目入口与递归模块导入共用 prepare_record，随后经 TryRun 把生成 thunk 的异常转成 VM 状态。 */
TZrBool ZrLibrary_AotRuntime_ExecuteEntry(SZrState *state,
                                          EZrAotBackendKind backendKind,
                                          SZrTypeValue *result) {
    SZrLibrary_Project *project;
    SZrLibraryAotRuntimeState *runtimeState;
    SZrLibraryAotLoadedModule *record = ZR_NULL;
    ZrLibraryAotEntryRequest request;
    EZrThreadStatus status;

    if (state == ZR_NULL || state->global == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    project = aot_runtime_get_project(state->global);
    runtimeState = aot_runtime_get_state_from_global(state->global);
    if (project == ZR_NULL || project->entry == ZR_NULL || runtimeState == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!aot_runtime_prepare_record(state,
                                    runtimeState,
                                    backendKind,
                                    ZrCore_String_GetNativeString(project->entry),
                                    &record) ||
        record == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&request, 0, sizeof(request));
    request.runtimeState = runtimeState;
    request.record = record;
    request.result = result;
    ZrCore_Value_ResetAsNull(result);

    status = ZrCore_Exception_TryRun(state, aot_runtime_execute_entry_body, &request);
    if (status != ZR_THREAD_STATUS_FINE) {
        state->threadStatus = status;
        return ZR_FALSE;
    }
    return request.success;
}

/* 合并 state 覆盖策略与当前行调试信号；行调试强制发布每条指令，查询得到有效策略。 */
static void aot_runtime_resolve_observation_policy(const SZrState *state,
                                                   TZrUInt32 *outObservationMask,
                                                   TZrBool *outPublishAllInstructions) {
    TZrUInt32 observationMask = ZrLibrary_AotRuntime_DefaultObservationMask();
    TZrBool publishAllInstructions = ZR_FALSE;

    if (state != ZR_NULL) {
        if (state->hasAotObservationPolicyOverride) {
            observationMask = state->aotObservationMask;
            publishAllInstructions = state->aotPublishAllInstructions;
        }
        if ((state->debugHookSignal & ZR_DEBUG_HOOK_MASK_LINE) != 0u) {
            publishAllInstructions = ZR_TRUE;
        }
    }

    if (outObservationMask != ZR_NULL) {
        *outObservationMask = observationMask;
    }
    if (outPublishAllInstructions != ZR_NULL) {
        *outPublishAllInstructions = publishAllInstructions;
    }
}

/* 为通用分支和转换提供不调用元方法的真值判断；null、零与空串为 false；对象等其余类型为 true。 */
static TZrBool aot_runtime_value_is_truthy(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        return value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_INT(value->type)) {
        return value->value.nativeObject.nativeInt64 != 0;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeUInt64 != 0;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        return value->value.nativeObject.nativeDouble != 0.0;
    }
    if (ZR_VALUE_IS_TYPE_NULL(value->type)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_STRING(value->type)) {
        SZrString *stringValue = ZR_CAST_STRING(state, value->value.object);
        TZrSize length = 0;

        if (stringValue == ZR_NULL) {
            return ZR_FALSE;
        }
        length = (stringValue->shortStringLength < ZR_VM_LONG_STRING_FLAG)
                         ? stringValue->shortStringLength
                         : stringValue->longStringLength;
        return length > 0;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SetObservationPolicy(SZrState *state,
                                                  TZrUInt32 observationMask,
                                                  TZrBool publishAllInstructions) {
    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    state->aotObservationMask = observationMask;
    state->aotPublishAllInstructions = publishAllInstructions;
    state->hasAotObservationPolicyOverride = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ResetObservationPolicy(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    state->aotObservationMask = 0;
    state->aotPublishAllInstructions = ZR_FALSE;
    state->hasAotObservationPolicyOverride = ZR_FALSE;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GetObservationPolicy(SZrState *state,
                                                  TZrUInt32 *outObservationMask,
                                                  TZrBool *outPublishAllInstructions) {
    if (state == ZR_NULL || outObservationMask == ZR_NULL || outPublishAllInstructions == ZR_NULL) {
        return ZR_FALSE;
    }

    aot_runtime_resolve_observation_policy(state, outObservationMask, outPublishAllInstructions);
    return ZR_TRUE;
}

/* 生成器入口校验正在执行的 closure 与函数表索引匹配，避免代码注册表和 VM 元数据漂移。 */
/* TODO: 核查合法嵌套 AOT 导入在第 5/9 个记录追加时跨 records 扩容，并在返回后继续消费 frame/context/activeRecord 的完整调用链；若 realloc 迁移数组，旧元素借用地址会失效，须验证实际可达路径及后续重定位责任，尚无完整合法触发或实测证明。 */

TZrBool ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(SZrState *state,
                                                           TZrUInt32 functionIndex,
                                                           ZrAotGeneratedModuleContext *context) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    SZrFunction *metadataFunction;
    SZrLibraryAotLoadedModule *record;
    TZrUInt32 resolvedIndex;
    TZrUInt32 generatedFrameSlotCount;

    if (context != ZR_NULL) {
        memset(context, 0, sizeof(*context));
    }
    if (state == ZR_NULL || state->global == ZR_NULL || context == ZR_NULL) {
        return ZR_FALSE;
    }

    runtimeState = aot_runtime_get_state_from_global(state->global);
    callInfo = state->callInfoList;
    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
    record = aot_runtime_find_record_for_function(runtimeState, metadataFunction);
    if (runtimeState == ZR_NULL || callInfo == ZR_NULL || metadataFunction == ZR_NULL || record == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT function invoked without matching runtime record");
        return ZR_FALSE;
    }

    resolvedIndex = aot_runtime_find_function_index_in_record(record, metadataFunction);
    if (resolvedIndex == UINT32_MAX || resolvedIndex != functionIndex) {
        aot_runtime_fail(state,
                         runtimeState,
                         "generated AOT function index mismatch for module '%s' (expected %u, got %u)",
                         record->moduleName != ZR_NULL ? record->moduleName : "<unknown>",
                         (unsigned)functionIndex,
                         (unsigned)resolvedIndex);
        return ZR_FALSE;
    }
    if (!aot_runtime_record_try_get_generated_slot_count(record, resolvedIndex, &generatedFrameSlotCount)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "generated AOT function is missing cached frame slot metadata for function index %u",
                         (unsigned)resolvedIndex);
        return ZR_FALSE;
    }

    aot_runtime_mark_record_executed(runtimeState, record);
    context->recordHandle = record;
    context->metadataFunction = metadataFunction;
    context->codeRegistration = record->codeRegistration;
    context->methodInfo = record->codeRegistration != ZR_NULL &&
                                  record->codeRegistration->methodInfos != ZR_NULL &&
                                  resolvedIndex < record->codeRegistration->methodInfoCount
                                  ? record->codeRegistration->methodInfos[resolvedIndex]
                                  : ZR_NULL;
    context->module = record->module;
    context->moduleExecuted = &record->moduleExecuted;
    context->functionTable = record->functionTable;
    context->functionCount = record->functionCount;
    context->functionThunks =
            record->codeRegistration != ZR_NULL ? record->codeRegistration->functionPointers : ZR_NULL;
    context->functionThunkCount = record->codeRegistration != ZR_NULL ? record->codeRegistration->functionCount : 0;
    context->resolvedFunctionIndex = resolvedIndex;
    context->generatedFrameSlotCount = generatedFrameSlotCount;
    return ZR_TRUE;
}

/* 生成函数序言在可能触发 GC 的栈扩容前锚定调用帧；之后建立稠密槽与物理帧的双视图。 */
/* TODO: 核查合法嵌套 AOT 导入在第 5/9 个记录追加时跨 records 扩容，并在返回后继续消费 frame/context/activeRecord 的完整调用链；若 realloc 迁移数组，旧元素借用地址会失效，须验证实际可达路径及后续重定位责任，尚无完整合法触发或实测证明。 */

TZrBool ZrLibrary_AotRuntime_BeginGeneratedFunction(SZrState *state,
                                                    TZrUInt32 functionIndex,
                                                    ZrAotGeneratedFrame *frame) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    SZrFunction *metadataFunction;
    SZrLibraryAotLoadedModule *record;
    TZrUInt32 resolvedIndex;
    TZrUInt32 generatedFrameSlotCount;
    TZrStackValuePointer functionBase;
    TZrStackValuePointer slotBase;
    TZrSize argumentCount;
    TZrSize frameSlotCount;
    TZrStackValuePointer frameTop;
    SZrFunctionStackAnchor baseAnchor;
    SZrFunctionStackAnchor returnAnchor;
    TZrBool hasReturnAnchor;

    if (frame != ZR_NULL) {
        memset(frame, 0, sizeof(*frame));
    }
    if (state == ZR_NULL || state->global == ZR_NULL || frame == ZR_NULL) {
        return ZR_FALSE;
    }

    runtimeState = aot_runtime_get_state_from_global(state->global);
    callInfo = state->callInfoList;
    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
    record = aot_runtime_find_record_for_function(runtimeState, metadataFunction);
    if (runtimeState == ZR_NULL || callInfo == ZR_NULL || metadataFunction == ZR_NULL || record == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT function invoked without matching runtime record");
        return ZR_FALSE;
    }

    resolvedIndex = aot_runtime_find_function_index_in_record(record, metadataFunction);
    if (resolvedIndex == UINT32_MAX || resolvedIndex != functionIndex) {
        aot_runtime_fail(state,
                         runtimeState,
                         "generated AOT function index mismatch for module '%s' (expected %u, got %u)",
                         record->moduleName != ZR_NULL ? record->moduleName : "<unknown>",
                         (unsigned)functionIndex,
                         (unsigned)resolvedIndex);
        return ZR_FALSE;
    }
    if (!aot_runtime_record_try_get_generated_slot_count(record, resolvedIndex, &generatedFrameSlotCount)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "generated AOT function is missing cached frame slot metadata for function index %u",
                         (unsigned)resolvedIndex);
        return ZR_FALSE;
    }

    functionBase = callInfo->functionBase.valuePointer;
    if (functionBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT function has no call frame");
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, functionBase, &baseAnchor);
    hasReturnAnchor = callInfo->hasReturnDestination && callInfo->returnDestination != ZR_NULL;
    if (hasReturnAnchor) {
        ZrCore_Function_StackAnchorInit(state, callInfo->returnDestination, &returnAnchor);
    }

    frameSlotCount = generatedFrameSlotCount;
    slotBase = ZrCore_Function_CheckStackAndGc(state, frameSlotCount, functionBase + 1);
    functionBase = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
    callInfo->functionBase.valuePointer = functionBase;
    if (hasReturnAnchor) {
        callInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &returnAnchor);
    }
    slotBase = functionBase + 1;

    argumentCount =
            (state->stackTop.valuePointer != ZR_NULL && state->stackTop.valuePointer > slotBase)
                    ? (TZrSize)(state->stackTop.valuePointer - slotBase)
                    : 0;
    if (frameSlotCount < argumentCount) {
        frameSlotCount = argumentCount;
    }
    frameTop = slotBase + frameSlotCount;

    if (ZrCore_CallInfo_IsNative(callInfo)) {
        for (TZrUInt32 slot = (TZrUInt32)argumentCount; slot < (TZrUInt32)frameSlotCount; slot++) {
            ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(slotBase + slot));
        }
    }

    if (callInfo->functionTop.valuePointer < frameTop) {
        callInfo->functionTop.valuePointer = frameTop;
    }
    if (state->stackTop.valuePointer < frameTop) {
        state->stackTop.valuePointer = frameTop;
    }

    aot_runtime_mark_record_executed(runtimeState, record);
    frame->recordHandle = record;
    frame->function = metadataFunction;
    frame->callInfo = callInfo;
    frame->slotBase = slotBase;
    frame->module = record->module;
    frame->moduleExecuted = &record->moduleExecuted;
    frame->functionTable = record->functionTable;
    frame->functionCount = record->functionCount;
    frame->codeRegistration = record->codeRegistration;
    frame->functionThunks = record->codeRegistration != ZR_NULL ? record->codeRegistration->functionPointers : ZR_NULL;
    frame->functionThunkCount = record->codeRegistration != ZR_NULL ? record->codeRegistration->functionCount : 0;
    frame->functionIndex = resolvedIndex;
    frame->currentInstructionIndex = 0;
    frame->lastObservedInstructionIndex = UINT32_MAX;
    frame->lastObservedLine = ZR_RUNTIME_DEBUG_HOOK_LINE_NONE;
    aot_runtime_resolve_observation_policy(state, &frame->observationMask, &frame->publishAllInstructions);
    frame->generatedFrameSlotCount = generatedFrameSlotCount;
    return ZR_TRUE;
}

/* 每个可观察指令经此处同步 PC、行钩子和栈帧位置，让异常/调试链定位回生成代码。 */
TZrBool ZrLibrary_AotRuntime_BeginInstruction(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 instructionIndex,
                                              TZrUInt32 stepFlags) {
    SZrCallInfo *callInfo;
    TZrBool publishAllInstructions;
    TZrBool lineDebugEnabled;

    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL) {
        return ZR_FALSE;
    }

    callInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
    if (callInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (callInfo->functionBase.valuePointer != ZR_NULL) {
        if (!aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
            return ZR_FALSE;
        }
        callInfo = frame->callInfo;
    } else {
        frame->callInfo = callInfo;
        state->callInfoList = callInfo;
    }
    frame->currentInstructionIndex = instructionIndex;
    lineDebugEnabled = (TZrBool)((state->debugHookSignal & ZR_DEBUG_HOOK_MASK_LINE) != 0u);
    publishAllInstructions = (TZrBool)(frame->publishAllInstructions || lineDebugEnabled);
    if (!publishAllInstructions && (frame->observationMask & stepFlags) == 0u) {
        return ZR_TRUE;
    }

    callInfo->context.context.programCounter = frame->function->instructionsList + instructionIndex;
    frame->lastObservedInstructionIndex = instructionIndex;
    if (lineDebugEnabled) {
        TZrUInt32 sourceLine = ZrCore_Exception_FindSourceLine(frame->function, (TZrMemoryOffset)instructionIndex);

        if (sourceLine != ZR_RUNTIME_DEBUG_HOOK_LINE_NONE && sourceLine != frame->lastObservedLine) {
            frame->lastObservedLine = sourceLine;
            ZrCore_Debug_Hook(state, ZR_DEBUG_HOOK_EVENT_LINE, sourceLine, 0, 0);
        }
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CopyConstant(SZrState *state,
                                          ZrAotGeneratedFrame *frame,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 constantIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrLibraryAotLoadedModule *record;
    const SZrFunction *function;
    TZrStackValuePointer destinationPointer;
    const SZrTypeValue *source;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global)
                                                                : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    record = frame != ZR_NULL ? (SZrLibraryAotLoadedModule *)frame->recordHandle : ZR_NULL;
    if (record == ZR_NULL) {
        record = aot_runtime_find_record_for_function(runtimeState, function);
    }
    if (state == ZR_NULL || function == ZR_NULL || record == ZR_NULL || constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    source = &function->constantValueList[constantIndex];
    return aot_runtime_materialize_callable_constant(state, record, source, ZR_FALSE, ZrCore_Stack_GetValue(destinationPointer));
}

TZrBool ZrLibrary_AotRuntime_CreateClosure(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 constantIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrLibraryAotLoadedModule *record;
    const SZrFunction *function;
    TZrStackValuePointer destinationPointer;
    const SZrTypeValue *source;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global)
                                                                : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    record = frame != ZR_NULL ? (SZrLibraryAotLoadedModule *)frame->recordHandle : ZR_NULL;
    if (record == ZR_NULL) {
        record = aot_runtime_find_record_for_function(runtimeState, function);
    }
    if (state == ZR_NULL || function == ZR_NULL || record == ZR_NULL || constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    source = &function->constantValueList[constantIndex];
    return aot_runtime_materialize_callable_constant_with_context(state,
                                                                  record,
                                                                  source,
                                                                  frame,
                                                                  ZR_TRUE,
                                                                  ZrCore_Stack_GetValue(destinationPointer));
}

TZrBool ZrLibrary_AotRuntime_GetClosureValue(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 closureIndex) {
    TZrStackValuePointer destinationPointer;
    SZrTypeValue *closureValue = ZR_NULL;

    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (state == ZR_NULL || destinationPointer == ZR_NULL ||
        !aot_runtime_resolve_current_closure_capture(state, frame, closureIndex, &closureValue, ZR_NULL) ||
        closureValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(destinationPointer), closureValue);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SetClosureValue(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 sourceSlot,
                                             TZrUInt32 closureIndex) {
    TZrStackValuePointer sourcePointer;
    SZrTypeValue *targetValue;
    SZrTypeValue *sourceValue;
    SZrRawObject *barrierObject = ZR_NULL;

    sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    if (state == ZR_NULL || sourcePointer == ZR_NULL ||
        !aot_runtime_resolve_current_closure_capture(state, frame, closureIndex, &targetValue, &barrierObject) ||
        targetValue == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (targetValue == ZR_NULL || sourceValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, targetValue, sourceValue);
    if (barrierObject != ZR_NULL) {
        ZrCore_Gc_WriteBarrier(state, barrierObject, sourceValue);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GetGlobal(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_GLOBAL: invalid destination slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_GLOBAL: missing destination value");
        return ZR_FALSE;
    }

    if (state->global != ZR_NULL && state->global->zrObject.type == ZR_VALUE_TYPE_OBJECT) {
        ZrCore_Value_Copy(state, destinationValue, &state->global->zrObject);
    } else {
        ZrCore_Value_ResetAsNull(destinationValue);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CreateObject(SZrState *state,
                                          ZrAotGeneratedFrame *frame,
                                          TZrUInt32 destinationSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;
    SZrObject *objectValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CREATE_OBJECT: invalid destination slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CREATE_OBJECT: missing destination value");
        return ZR_FALSE;
    }

    objectValue = ZrCore_Object_New(state, ZR_NULL);
    ZrCore_Ownership_ReleaseValue(state, destinationValue);
    if (objectValue != ZR_NULL) {
        ZrCore_Object_Init(state, objectValue);
        ZrCore_Value_InitAsRawObject(state, destinationValue, ZR_CAST_RAW_OBJECT_AS_SUPER(objectValue));
    } else {
        ZrCore_Value_ResetAsNull(destinationValue);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CreateArray(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;
    SZrObject *arrayValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CREATE_ARRAY: invalid destination slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CREATE_ARRAY: missing destination value");
        return ZR_FALSE;
    }

    arrayValue = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    ZrCore_Ownership_ReleaseValue(state, destinationValue);
    if (arrayValue != ZR_NULL) {
        ZrCore_Object_Init(state, arrayValue);
        ZrCore_Value_InitAsRawObject(state, destinationValue, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayValue));
        destinationValue->type = ZR_VALUE_TYPE_ARRAY;
    } else {
        ZrCore_Value_ResetAsNull(destinationValue);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CreateInlineArray(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 elementTypeLayoutId,
                                               TZrUInt32 length) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;
    SZrObject *arrayValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL ||
        destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CREATE_INLINE_ARRAY: invalid destination or function");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CREATE_INLINE_ARRAY: missing destination value");
        return ZR_FALSE;
    }

    arrayValue = ZrCore_Object_NewInlineArray(
            state, frame->function, elementTypeLayoutId, length);
    ZrCore_Ownership_ReleaseValue(state, destinationValue);
    if (arrayValue == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        aot_runtime_fail(state, runtimeState, "CREATE_INLINE_ARRAY: invalid inline array layout");
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(
            state, destinationValue, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayValue));
    destinationValue->type = ZR_VALUE_TYPE_ARRAY;
    {
        const SZrFunctionFrameSlotLayout *destinationLayout =
                ZrCore_Function_FindFrameSlotLayout(frame->function, destinationSlot);
        SZrStackFramePlace destinationPlace;

        if (destinationLayout == ZR_NULL ||
            destinationLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
            destinationLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue) ||
            !ZrCore_Function_MakeFrameSlotPlace(
                    state,
                    frame->function,
                    frame->slotBase,
                    destinationSlot,
                    &destinationPlace)) {
            ZrCore_Value_ResetAsNull(destinationValue);
            aot_runtime_fail(state, runtimeState, "CREATE_INLINE_ARRAY: invalid destination frame place");
            return ZR_FALSE;
        }
        if (destinationPlace.address != destinationValue) {
            SZrTypeValue *frameValue = (SZrTypeValue *)destinationPlace.address;
            ZrCore_Value_InitAsRawObject(
                    state, frameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayValue));
            frameValue->type = ZR_VALUE_TYPE_ARRAY;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_BindInlineArrayElementPlace(
        SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 arraySlot,
        TZrUInt32 indexSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer indexPointer = aot_runtime_frame_slot(frame, indexSlot);
    const SZrTypeValue *indexValue;
    TZrInt64 index;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL ||
        indexPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BIND_INLINE_ARRAY_ELEMENT_PLACE: invalid frame or index slot");
        return ZR_FALSE;
    }

    indexValue = ZrCore_Stack_GetValue(indexPointer);
    if (indexValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(indexValue->type) ||
        (ZR_VALUE_IS_TYPE_UNSIGNED_INT(indexValue->type) &&
         indexValue->value.nativeObject.nativeUInt64 > (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX)) {
        aot_runtime_fail(state, runtimeState, "BIND_INLINE_ARRAY_ELEMENT_PLACE: index must be an integer");
        return ZR_FALSE;
    }
    index = ZR_VALUE_IS_TYPE_UNSIGNED_INT(indexValue->type)
                    ? (TZrInt64)indexValue->value.nativeObject.nativeUInt64
                    : indexValue->value.nativeObject.nativeInt64;

    if (!ZrCore_Function_BindFrameSlotInlineArrayElement(
                state,
                frame->function,
                frame->slotBase,
                destinationSlot,
                arraySlot,
                index)) {
        aot_runtime_fail(state, runtimeState, "BIND_INLINE_ARRAY_ELEMENT_PLACE: invalid inline array element");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_TypeOf(SZrState *state,
                                    ZrAotGeneratedFrame *frame,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TYPEOF: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL ||
        !ZrCore_Reflection_TypeOfValue(state, sourceValue, destinationValue)) {
        aot_runtime_fail(state, runtimeState, "TYPEOF: failed to materialize runtime type");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ToObject(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 sourceSlot,
                                      TZrUInt32 typeNameConstantIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;
    const SZrTypeValue *typeNameValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL ||
        typeNameConstantIndex >= function->constantValueLength) {
        aot_runtime_fail(state, runtimeState, "TO_OBJECT: invalid frame slot or type-name constant");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    typeNameValue = &function->constantValueList[typeNameConstantIndex];
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL ||
        !ZrCore_Bridge_BoxTyped(state,
                                frame != ZR_NULL ? frame->callInfo : state->callInfoList,
                                destinationValue,
                                sourceValue,
                                typeNameValue)) {
        aot_runtime_fail(state, runtimeState, "TO_OBJECT: failed to materialize object conversion");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ToStruct(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 sourceSlot,
                                      TZrUInt32 typeNameConstantIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;
    const SZrTypeValue *typeNameValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL ||
        typeNameConstantIndex >= function->constantValueLength) {
        aot_runtime_fail(state, runtimeState, "TO_STRUCT: invalid frame slot or type-name constant");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    typeNameValue = &function->constantValueList[typeNameConstantIndex];
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL ||
        !ZrCore_Bridge_UnboxTyped(state,
                                  frame != ZR_NULL ? frame->callInfo : state->callInfoList,
                                  destinationValue,
                                  sourceValue,
                                  typeNameValue)) {
        aot_runtime_fail(state, runtimeState, "TO_STRUCT: failed to materialize struct conversion");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_MetaGet(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 destinationSlot,
                                     TZrUInt32 receiverSlot,
                                     TZrUInt32 memberId) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue stableReceiver;
    SZrString *memberSymbol = ZR_NULL;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || destinationPointer == ZR_NULL || receiverPointer == ZR_NULL ||
        !aot_runtime_resolve_member_symbol(function, memberId, &memberSymbol)) {
        aot_runtime_fail(state, runtimeState, "META_GET: invalid member id");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL || ZrCore_Stack_GetValue(receiverPointer) == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "META_GET: invalid stack slot");
        return ZR_FALSE;
    }

    stableReceiver = *ZrCore_Stack_GetValue(receiverPointer);
    if (!ZrCore_Object_InvokeMember(state, &stableReceiver, memberSymbol, ZR_NULL, 0, destinationValue)) {
        aot_runtime_fail(state, runtimeState, "META_GET: receiver must define property getter");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 生成器属性写入通过成员 setter 维持 VM 元方法语义，随后把赋值结果放回表达式槽。 */
/* TODO: 核查 InvokeMember setter 后 receiverAndResultSlot 的写回位置：沿合法生成调用进入 ReserveScratchSlots/CheckStackAndGc 与 stack_realloc_internal，构造确实迁移 VM 栈且返回后重读旧来源的全链证据；确认需保存锚点或稳定副本并重取的责任，尚无合法程序/分配器触发证明。 */

TZrBool ZrLibrary_AotRuntime_MetaSet(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 receiverAndResultSlot,
                                     TZrUInt32 assignedValueSlot,
                                     TZrUInt32 memberId) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverAndResultSlot);
    TZrStackValuePointer assignedPointer = aot_runtime_frame_slot(frame, assignedValueSlot);
    SZrTypeValue *receiverValue;
    SZrTypeValue *assignedValue;
    SZrTypeValue stableReceiver;
    SZrTypeValue stableAssignedValue;
    SZrTypeValue ignoredResult;
    SZrString *memberSymbol = ZR_NULL;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || receiverPointer == ZR_NULL || assignedPointer == ZR_NULL ||
        !aot_runtime_resolve_member_symbol(function, memberId, &memberSymbol)) {
        aot_runtime_fail(state, runtimeState, "META_SET: invalid member id");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    assignedValue = ZrCore_Stack_GetValue(assignedPointer);
    if (receiverValue == ZR_NULL || assignedValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "META_SET: invalid stack slot");
        return ZR_FALSE;
    }

    stableReceiver = *receiverValue;
    stableAssignedValue = *assignedValue;
    ZrCore_Value_ResetAsNull(&ignoredResult);
    if (!ZrCore_Object_InvokeMember(state,
                                    &stableReceiver,
                                    memberSymbol,
                                    &stableAssignedValue,
                                    1,
                                    &ignoredResult)) {
        aot_runtime_fail(state, runtimeState, "META_SET: receiver must define property setter");
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, receiverValue, &stableAssignedValue);
    return ZR_TRUE;
}

/*
 * Invoke a cached property accessor through the call-binding target selected
 * at AOT registration time.  The generated C path must not recover a member
 * name and re-enter the interpreter: doing so would silently discard the
 * relocated thunk (and would make a pointer-free artifact depend on symbols
 * that may not be present after a reload).
 *
 * The normal VM member-call path already uses a temporary contiguous call
 * window before preparing a resolved VM frame.  Accessors use the same window
 * here, with the receiver in the first argument slot and the optional assigned
 * value in the second slot.  A native provider is still dispatched through
 * Object_CallValue, which preserves its ABI without introducing a name based
 * fallback.
 */
static TZrBool aot_runtime_invoke_bound_cached_accessor(
        SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 receiverSlot,
        TZrUInt32 assignedValueSlot,
        TZrUInt32 cacheIndex,
        EZrFunctionCallSiteCacheKind expectedKind,
        TZrBool expectedStatic,
        TZrBool setter,
        const TZrChar *failureLabel) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrFunction *function;
    SZrFunctionCallSiteCacheEntry *entry;
    TZrStackValuePointer receiverPointer;
    TZrStackValuePointer destinationPointer;
    TZrStackValuePointer assignedPointer = ZR_NULL;
    SZrTypeValue *receiverValue;
    SZrTypeValue *destinationValue;
    SZrTypeValue *assignedValue = ZR_NULL;
    SZrTypeValue stableReceiver;
    SZrTypeValue stableAssigned;
    SZrTypeValue callable;
    SZrFunction *metadataFunction;
    SZrLibraryAotLoadedModule *record;
    TZrUInt32 functionIndex;
    FZrAotEntryThunk thunk;
    TZrStackValuePointer callBase = ZR_NULL;
    TZrUInt32 functionSlot = 0u;
    SZrCallInfo *callerCallInfo;
    SZrFunctionStackAnchor callerTopAnchor;
    TZrBool hasCallerTopAnchor = ZR_FALSE;
    TZrMemoryOffset callerTopOffset = 0;
    TZrMemoryOffset callBaseOffset = 0;
    TZrUInt32 argumentCount = expectedStatic ? (setter ? 1u : 0u) : (setter ? 2u : 1u);
    ZrAotGeneratedDirectCall directCall;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL
                           ? aot_runtime_get_state_from_global(state->global)
                           : ZR_NULL;
    callerCallInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL
            ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    if (state != ZR_NULL && callerCallInfo != ZR_NULL &&
        callerCallInfo->functionTop.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, callerCallInfo->functionTop.valuePointer, &callerTopAnchor);
        callerTopOffset = ZrCore_Stack_SavePointerAsOffset(state, callerCallInfo->functionTop.valuePointer);
        hasCallerTopAnchor = ZR_TRUE;
    }
    function = (SZrFunction *)aot_runtime_frame_function(frame);
    receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (setter) {
        assignedPointer = aot_runtime_frame_slot(frame, assignedValueSlot);
    }
    if (state == ZR_NULL || frame == ZR_NULL || function == ZR_NULL ||
        function->callSiteCaches == ZR_NULL || cacheIndex >= function->callSiteCacheLength ||
        receiverPointer == ZR_NULL || destinationPointer == ZR_NULL ||
        (setter && assignedPointer == ZR_NULL)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: invalid bound accessor stack or cache",
                         failureLabel != ZR_NULL ? failureLabel : "META_ACCESS");
        return ZR_FALSE;
    }

    /* 缓存种类、绑定契约及静态模式共同定义 accessor 目标；验证失败必须报错，不能改成成员名查询。 */
    entry = &function->callSiteCaches[cacheIndex];
    if ((EZrFunctionCallSiteCacheKind)entry->kind != expectedKind ||
        entry->binding.contract.bindingKind == ZR_CALL_BINDING_NONE ||
        ((entry->binding.contract.bindingKind == ZR_CALL_BINDING_DIRECT ||
          entry->binding.contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION) &&
         entry->binding.target.callableObject == ZR_NULL)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: missing bound accessor contract",
                         failureLabel != ZR_NULL ? failureLabel : "META_ACCESS");
        return ZR_FALSE;
    }

    /* Static-vs-instance is part of the cache contract, not a symbol lookup. */
    if (function->memberEntries == ZR_NULL || entry->memberEntryIndex >= function->memberEntryLength ||
        ((function->memberEntries[entry->memberEntryIndex].reserved0 &
          ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR) != 0) != (expectedStatic != ZR_FALSE)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: static accessor mode mismatch",
                         failureLabel != ZR_NULL ? failureLabel : "META_ACCESS");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (receiverValue == ZR_NULL || destinationValue == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: invalid accessor value slot",
                         failureLabel != ZR_NULL ? failureLabel : "META_ACCESS");
        return ZR_FALSE;
    }
    stableReceiver = *receiverValue;
    if (setter) {
        assignedValue = ZrCore_Stack_GetValue(assignedPointer);
        if (assignedValue == ZR_NULL) {
            aot_runtime_fail(state,
                             runtimeState,
                             "%s: invalid setter value slot",
                             failureLabel != ZR_NULL ? failureLabel : "META_SET");
            return ZR_FALSE;
        }
        stableAssigned = *assignedValue;
    }

    ZrCore_Value_ResetAsNull(&callable);
    if (!ZrCore_CallBinding_PrepareMember(state,
                                          function,
                                          cacheIndex,
                                          &stableReceiver,
                                          &callable,
                                          &state->lastCallBindingError)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: bound accessor preparation failed (%s)",
                         failureLabel != ZR_NULL ? failureLabel : "META_ACCESS",
                         ZrCore_CallBinding_StatusName(state->lastCallBindingError.status));
        return ZR_FALSE;
    }

    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, &callable);
    if (metadataFunction == ZR_NULL) {
        /* A native provider has no generated thunk; invoke its callable object
         * directly while retaining the already validated receiver contract. */
        if (!callable.isNative) {
            aot_runtime_fail(state,
                             runtimeState,
                             "%s: bound accessor has no callable target",
                             failureLabel != ZR_NULL ? failureLabel : "META_ACCESS");
            return ZR_FALSE;
        }
        if (!ZrCore_Object_CallValue(state,
                                     &callable,
                                     expectedStatic ? ZR_NULL : &stableReceiver,
                                     setter ? &stableAssigned : ZR_NULL,
                                     setter ? 1u : 0u,
                                     setter ? ZrCore_Stack_GetValue(destinationPointer) : destinationValue)) {
            aot_runtime_fail(state,
                             runtimeState,
                             "%s: native accessor invocation failed",
                             failureLabel != ZR_NULL ? failureLabel : "META_ACCESS");
            return ZR_FALSE;
        }
        if (frame->callInfo == ZR_NULL ||
            !aot_runtime_refresh_frame_from_callinfo(state, frame, frame->callInfo) ||
            (destinationPointer = aot_runtime_frame_slot(frame, destinationSlot)) == ZR_NULL) {
            aot_runtime_fail(state, runtimeState,
                             "%s: native accessor lost caller frame after stack relocation",
                             failureLabel != ZR_NULL ? failureLabel : "META_ACCESS");
            return ZR_FALSE;
        }
        if (setter) {
            ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(destinationPointer), &stableAssigned);
        }
        if (hasCallerTopAnchor) {
            callerCallInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
            if (callerCallInfo != ZR_NULL) {
                callerCallInfo->functionTop.valuePointer =
                        ZrCore_Function_StackAnchorRestore(state, &callerTopAnchor);
                state->stackTop.valuePointer = callerCallInfo->functionTop.valuePointer;
            }
        }
        return ZR_TRUE;
    }

    record = entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_AOT
                     ? aot_runtime_find_record_for_bound_target(runtimeState, &entry->binding.target)
                     : aot_runtime_find_record_for_function(runtimeState, metadataFunction);
    functionIndex = record == ZR_NULL ? UINT32_MAX
            : entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_AOT
                    ? entry->binding.target.aot.methodInfo->functionIndex
                    : aot_runtime_find_function_index_in_record(record, metadataFunction);
    thunk = record != ZR_NULL && record->codeRegistration != ZR_NULL &&
                    record->codeRegistration->functionPointers != ZR_NULL &&
                    functionIndex < record->codeRegistration->functionCount
                    ? record->codeRegistration->functionPointers[functionIndex]
                    : ZR_NULL;
    if (record == ZR_NULL || thunk == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: bound accessor target has no generated thunk",
                         failureLabel != ZR_NULL ? failureLabel : "META_ACCESS");
        return ZR_FALSE;
    }

    if (!aot_runtime_reserve_temp_call_base(state,
                                            frame,
                                            argumentCount + 1u,
                                            &callBase,
                                            &functionSlot) ||
        callBase == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: failed to reserve accessor call window",
                         failureLabel != ZR_NULL ? failureLabel : "META_ACCESS");
        return ZR_FALSE;
    }
    callBaseOffset = ZrCore_Stack_SavePointerAsOffset(state, callBase);
    /* Reserving scratch slots may move the stack or the receiver. Reload
     * witnesses from traced frame/cache storage before publishing arguments. */
    function = (SZrFunction *)aot_runtime_frame_function(frame);
    entry = &function->callSiteCaches[cacheIndex];
    stableReceiver = *ZrCore_Stack_GetValue(aot_runtime_frame_slot(frame, receiverSlot));
    if (setter) stableAssigned = *ZrCore_Stack_GetValue(aot_runtime_frame_slot(frame, assignedValueSlot));
    if (!ZrCore_CallBinding_PrepareMember(state, function, cacheIndex, &stableReceiver,
            &callable, &state->lastCallBindingError)) return ZR_FALSE;
    metadataFunction = record->functionTable[functionIndex];
    ZrCore_Stack_CopyValue(state, callBase, &callable);
    if (!expectedStatic) {
        ZrCore_Stack_CopyValue(state, callBase + 1, &stableReceiver);
    }
    if (setter) {
        ZrCore_Stack_CopyValue(state,
                               callBase + 1u + (expectedStatic ? 0u : 1u),
                               &stableAssigned);
    }
    state->stackTop.valuePointer = callBase + 1u + argumentCount;
    if (frame->callInfo != ZR_NULL && frame->callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        frame->callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    memset(&directCall, 0, sizeof(directCall));
    if (!aot_runtime_prepare_vm_direct_call_frame(state,
                                                  frame,
                                                  destinationSlot,
                                                  functionSlot,
                                                  argumentCount,
                                                  record,
                                                  metadataFunction,
                                                  functionIndex,
                                                  ZR_NULL,
                                                  &directCall)) {
        return ZR_FALSE;
    }
    directCall.nativeFunction = thunk;
    if (!ZrLibrary_AotRuntime_CallPreparedOrGeneric(state,
                                                    frame,
                                                    &directCall,
                                                    destinationSlot,
                                                    functionSlot,
                                                    argumentCount,
                                                    setter ? 0u : 1u)) {
        return ZR_FALSE;
    }
    if (setter) {
        /* Property assignment expressions evaluate to their assigned value. */
        stableAssigned = *ZrCore_Stack_GetValue(aot_runtime_frame_slot(frame, assignedValueSlot));
        receiverValue = ZrCore_Stack_GetValue(aot_runtime_frame_slot(frame, receiverSlot));
        if (receiverValue != ZR_NULL) {
            ZrCore_Value_Copy(state, receiverValue, &stableAssigned);
        }
    }
    if (hasCallerTopAnchor) {
        callerCallInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
        if (callerCallInfo != ZR_NULL) {
            TZrStackValuePointer cleanupBase = ZrCore_Stack_LoadOffsetToPointer(state, callerTopOffset);
            TZrStackValuePointer cleanupCallBase = ZrCore_Stack_LoadOffsetToPointer(state, callBaseOffset);
            if (cleanupBase != ZR_NULL && cleanupCallBase != ZR_NULL && cleanupCallBase >= cleanupBase) {
                aot_runtime_discard_direct_call_window(state, cleanupBase,
                        (TZrUInt32)(cleanupCallBase - cleanupBase) + argumentCount + 1u);
            }
            callerCallInfo->functionTop.valuePointer =
                    ZrCore_Function_StackAnchorRestore(state, &callerTopAnchor);
            state->stackTop.valuePointer = callerCallInfo->functionTop.valuePointer;
            frame->slotBase = callerCallInfo->functionBase.valuePointer + 1u;
        }
    }
    return ZR_TRUE;
}

/* 校验 getter 缓存后进入绑定 accessor 派发；expectedKind 与 bindingKind 必须有效。 */
static TZrBool aot_runtime_meta_get_cached_internal(SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 cacheIndex,
                                                    TZrUInt32 expectedKind,
                                                    TZrBool expectedStatic,
                                                    const TZrChar *failureLabel) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    SZrFunctionCallSiteCacheEntry *entry;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || destinationPointer == ZR_NULL || receiverPointer == ZR_NULL ||
        function->callSiteCaches == ZR_NULL || cacheIndex >= function->callSiteCacheLength) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: invalid call-site cache or member binding",
                         failureLabel != ZR_NULL ? failureLabel : "META_GET");
        return ZR_FALSE;
    }
    entry = &function->callSiteCaches[cacheIndex];
    if (entry->kind != expectedKind || entry->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: missing bound accessor contract",
                         failureLabel != ZR_NULL ? failureLabel : "META_GET");
        return ZR_FALSE;
    }
    return aot_runtime_invoke_bound_cached_accessor(state,
                                                     frame,
                                                     destinationSlot,
                                                     receiverSlot,
                                                     0u,
                                                     cacheIndex,
                                                     (EZrFunctionCallSiteCacheKind)expectedKind,
                                                     expectedStatic,
                                                     ZR_FALSE,
                                                     failureLabel);
}

/* 校验 setter 缓存后进入绑定 accessor 派发；receiverAndResultSlot 最后保存赋值表达式值。 */
static TZrBool aot_runtime_meta_set_cached_internal(SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 receiverAndResultSlot,
                                                    TZrUInt32 assignedValueSlot,
                                                    TZrUInt32 cacheIndex,
                                                    TZrUInt32 expectedKind,
                                                    TZrBool expectedStatic,
                                                    const TZrChar *failureLabel) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    SZrFunctionCallSiteCacheEntry *entry;
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverAndResultSlot);
    TZrStackValuePointer assignedPointer = aot_runtime_frame_slot(frame, assignedValueSlot);

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || receiverPointer == ZR_NULL || assignedPointer == ZR_NULL ||
        function->callSiteCaches == ZR_NULL || cacheIndex >= function->callSiteCacheLength) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: invalid call-site cache or member binding",
                         failureLabel != ZR_NULL ? failureLabel : "META_SET");
        return ZR_FALSE;
    }
    entry = &function->callSiteCaches[cacheIndex];
    if (entry->kind != expectedKind || entry->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: missing bound accessor contract",
                         failureLabel != ZR_NULL ? failureLabel : "META_SET");
        return ZR_FALSE;
    }
    return aot_runtime_invoke_bound_cached_accessor(state,
                                                     frame,
                                                     receiverAndResultSlot,
                                                     receiverAndResultSlot,
                                                     assignedValueSlot,
                                                     cacheIndex,
                                                     (EZrFunctionCallSiteCacheKind)expectedKind,
                                                     expectedStatic,
                                                     ZR_TRUE,
                                                     failureLabel);
}

TZrBool ZrLibrary_AotRuntime_MetaGetCached(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 receiverSlot,
                                           TZrUInt32 cacheIndex) {
    return aot_runtime_meta_get_cached_internal(state,
                                                frame,
                                                destinationSlot,
                                                receiverSlot,
                                                cacheIndex,
                                                ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET,
                                                ZR_FALSE,
                                                "SUPER_META_GET_CACHED");
}

TZrBool ZrLibrary_AotRuntime_MetaSetCached(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 receiverAndResultSlot,
                                           TZrUInt32 assignedValueSlot,
                                           TZrUInt32 cacheIndex) {
    return aot_runtime_meta_set_cached_internal(state,
                                                frame,
                                                receiverAndResultSlot,
                                                assignedValueSlot,
                                                cacheIndex,
                                                ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET,
                                                ZR_FALSE,
                                                "SUPER_META_SET_CACHED");
}

TZrBool ZrLibrary_AotRuntime_MetaGetStaticCached(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 receiverSlot,
                                                 TZrUInt32 cacheIndex) {
    return aot_runtime_meta_get_cached_internal(state,
                                                frame,
                                                destinationSlot,
                                                receiverSlot,
                                                cacheIndex,
                                                ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC,
                                                ZR_TRUE,
                                                "SUPER_META_GET_STATIC_CACHED");
}

TZrBool ZrLibrary_AotRuntime_MetaSetStaticCached(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 receiverAndResultSlot,
                                                 TZrUInt32 assignedValueSlot,
                                                 TZrUInt32 cacheIndex) {
    return aot_runtime_meta_set_cached_internal(state,
                                                frame,
                                                receiverAndResultSlot,
                                                assignedValueSlot,
                                                cacheIndex,
                                                ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC,
                                                ZR_TRUE,
                                                "SUPER_META_SET_STATIC_CACHED");
}

/* 在所有权操作前后维护稠密槽和活动物理清理注册；底层 operation=false 被映射为目标 null 后返回 true；state/frame/operation 前提无效或地址刷新失败返回 false，底层拒绝不等于本桥接失败。 */
static TZrBool aot_runtime_own_value(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 destinationSlot,
                                     TZrUInt32 sourceSlot,
                                     TZrBool (*operation)(SZrState *, SZrTypeValue *, SZrTypeValue *)) {
    TZrStackValuePointer destinationPointer;
    TZrStackValuePointer sourcePointer;
    SZrCallInfo *callInfo;
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;
    TZrBool succeeded;

    if (state == ZR_NULL || frame == ZR_NULL || operation == ZR_NULL) {
        return ZR_FALSE;
    }

    /* 先撤销活动物理槽中的旧 owner，再执行稠密槽转换，最后刷新注册；释放回调之间必须重新取帧地址。 */
    callInfo = frame->callInfo;
    aot_runtime_cleanup_registration_clear(state, frame, destinationSlot);
    if (callInfo != ZR_NULL && !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        return ZR_FALSE;
    }
    if (sourceSlot != destinationSlot) {
        aot_runtime_cleanup_registration_clear(state, frame, sourceSlot);
        if (callInfo != ZR_NULL && !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
            return ZR_FALSE;
        }
    }
    if (callInfo != ZR_NULL && !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        return ZR_FALSE;
    }
    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    if (destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        return ZR_FALSE;
    }

    succeeded = operation(state, destinationValue, sourceValue);
    if (callInfo != ZR_NULL && !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        return ZR_FALSE;
    }
    destinationValue = ZrCore_Stack_GetValue(aot_runtime_frame_slot(frame, destinationSlot));
    if (destinationValue == ZR_NULL) {
        return ZR_FALSE;
    }
    /* core 拒绝转换时以 null 表达式结果继续执行；本层 true 只表示帧与注册同步完成。 */
    if (!succeeded) {
        ZrCore_Value_ResetAsNullNoProfile(destinationValue);
    }
    aot_runtime_cleanup_registration_refresh(state, frame, destinationSlot);
    if (sourceSlot != destinationSlot) {
        aot_runtime_cleanup_registration_refresh(state, frame, sourceSlot);
    }
    return ZR_TRUE;
}

/* 让 OWN_DETACH 先尝试直接 unique GC box，再尝试控制块归还；第二分支只在 IntoGcBoxValue 返回 false 时执行。 */
static TZrBool aot_runtime_into_gc_box_or_detach(
        SZrState *state,
        SZrTypeValue *destination,
        SZrTypeValue *source) {
    return ZrCore_Ownership_IntoGcBoxValue(state, destination, source) ||
           ZrCore_Ownership_DetachValue(state, destination, source);
}

TZrBool ZrLibrary_AotRuntime_OwnUnique(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_UniqueValue);
}

TZrBool ZrLibrary_AotRuntime_OwnBorrow(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_BorrowValue);
}

TZrBool ZrLibrary_AotRuntime_OwnLoan(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 destinationSlot,
                                     TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_LoanValue);
}

TZrBool ZrLibrary_AotRuntime_OwnReturnLoan(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_ReturnLoanValue);
}

TZrBool ZrLibrary_AotRuntime_OwnShare(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_ShareValue);
}

TZrBool ZrLibrary_AotRuntime_OwnDegrade(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 destinationSlot,
                                     TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_DegradeValue);
}

TZrBool ZrLibrary_AotRuntime_OwnDetach(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(
            state,
            frame,
            destinationSlot,
            sourceSlot,
            aot_runtime_into_gc_box_or_detach);
}

TZrBool ZrLibrary_AotRuntime_OwnIntoGcBox(SZrState *state,
                                          ZrAotGeneratedFrame *frame,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(
            state,
            frame,
            destinationSlot,
            sourceSlot,
            ZrCore_Ownership_IntoGcBoxValue);
}

TZrBool ZrLibrary_AotRuntime_OwnReturnToGc(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(
            state,
            frame,
            destinationSlot,
            sourceSlot,
            ZrCore_Ownership_DetachValue);
}

TZrBool ZrLibrary_AotRuntime_OwnWake(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_WakeValue);
}

TZrBool ZrLibrary_AotRuntime_OwnDrop(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 sourceSlot) {
    TZrStackValuePointer destinationPointer;
    TZrStackValuePointer sourcePointer;
    SZrCallInfo *callInfo;
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;

    if (state == ZR_NULL || frame == ZR_NULL) {
        return ZR_FALSE;
    }

    callInfo = frame->callInfo;
    aot_runtime_cleanup_registration_clear(state, frame, destinationSlot);
    if (callInfo != ZR_NULL && !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        return ZR_FALSE;
    }
    if (sourceSlot != destinationSlot) {
        aot_runtime_cleanup_registration_clear(state, frame, sourceSlot);
        if (callInfo != ZR_NULL && !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
            return ZR_FALSE;
        }
    }
    if (callInfo != ZR_NULL && !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        return ZR_FALSE;
    }
    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    if (destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Ownership_ReleaseValue(state, sourceValue);
    if (callInfo != ZR_NULL && !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        return ZR_FALSE;
    }
    destinationValue = ZrCore_Stack_GetValue(aot_runtime_frame_slot(frame, destinationSlot));
    if (destinationValue == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_ResetAsNullNoProfile(destinationValue);
    aot_runtime_cleanup_registration_refresh(state, frame, destinationSlot);
    if (sourceSlot != destinationSlot) {
        aot_runtime_cleanup_registration_refresh(state, frame, sourceSlot);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalEqual(SZrState *state,
                                          ZrAotGeneratedFrame *frame,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrBool equal;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    equal = ZrCore_Value_Equal(state, leftValue, rightValue);
    ZR_VALUE_FAST_SET(destinationValue, nativeBool, equal ? ZR_TRUE : ZR_FALSE, ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalNotEqual(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;

    if (!ZrLibrary_AotRuntime_LogicalEqual(state, frame, destinationSlot, leftSlot, rightSlot) ||
        destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL || !ZR_VALUE_IS_TYPE_BOOL(destinationValue->type)) {
        return ZR_FALSE;
    }

    destinationValue->value.nativeObject.nativeBool =
            (TZrBool)!destinationValue->value.nativeObject.nativeBool;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalEqualBool(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_BOOL(leftValue->type) || !ZR_VALUE_IS_TYPE_BOOL(rightValue->type)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftValue->value.nativeObject.nativeBool == rightValue->value.nativeObject.nativeBool,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalNotEqualBool(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;

    if (!ZrLibrary_AotRuntime_LogicalEqualBool(state, frame, destinationSlot, leftSlot, rightSlot) ||
        destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL || !ZR_VALUE_IS_TYPE_BOOL(destinationValue->type)) {
        return ZR_FALSE;
    }

    destinationValue->value.nativeObject.nativeBool =
            (TZrBool)!destinationValue->value.nativeObject.nativeBool;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalEqualSigned(SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftValue->value.nativeObject.nativeInt64 == rightValue->value.nativeObject.nativeInt64,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalNotEqualSigned(SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;

    if (!ZrLibrary_AotRuntime_LogicalEqualSigned(state, frame, destinationSlot, leftSlot, rightSlot) ||
        destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL || !ZR_VALUE_IS_TYPE_BOOL(destinationValue->type)) {
        return ZR_FALSE;
    }

    destinationValue->value.nativeObject.nativeBool =
            (TZrBool)!destinationValue->value.nativeObject.nativeBool;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalEqualUnsigned(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftValue->value.nativeObject.nativeUInt64 == rightValue->value.nativeObject.nativeUInt64,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalNotEqualUnsigned(SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;

    if (!ZrLibrary_AotRuntime_LogicalEqualUnsigned(state, frame, destinationSlot, leftSlot, rightSlot) ||
        destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL || !ZR_VALUE_IS_TYPE_BOOL(destinationValue->type)) {
        return ZR_FALSE;
    }

    destinationValue->value.nativeObject.nativeBool =
            (TZrBool)!destinationValue->value.nativeObject.nativeBool;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalEqualFloat(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_FLOAT(leftValue->type) || !ZR_VALUE_IS_TYPE_FLOAT(rightValue->type)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftValue->value.nativeObject.nativeDouble == rightValue->value.nativeObject.nativeDouble,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalNotEqualFloat(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;

    if (!ZrLibrary_AotRuntime_LogicalEqualFloat(state, frame, destinationSlot, leftSlot, rightSlot) ||
        destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL || !ZR_VALUE_IS_TYPE_BOOL(destinationValue->type)) {
        return ZR_FALSE;
    }

    destinationValue->value.nativeObject.nativeBool =
            (TZrBool)!destinationValue->value.nativeObject.nativeBool;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalEqualString(SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL ||
        leftValue->type != ZR_VALUE_TYPE_STRING || rightValue->type != ZR_VALUE_TYPE_STRING ||
        leftValue->value.object == ZR_NULL || rightValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      ZrCore_String_Equal(ZR_CAST_STRING(state, leftValue->value.object),
                                          ZR_CAST_STRING(state, rightValue->value.object)),
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalNotEqualString(SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;

    if (!ZrLibrary_AotRuntime_LogicalEqualString(state, frame, destinationSlot, leftSlot, rightSlot) ||
        destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL || !ZR_VALUE_IS_TYPE_BOOL(destinationValue->type)) {
        return ZR_FALSE;
    }

    destinationValue->value.nativeObject.nativeBool =
            (TZrBool)!destinationValue->value.nativeObject.nativeBool;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalLessSigned(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftNumber;
    TZrInt64 rightNumber;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_SIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_SIGNED: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type)) {
        leftNumber = leftValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        leftNumber = (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    } else {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_SIGNED: left operand is not integer-like");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        rightNumber = rightValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        rightNumber = (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    } else {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_SIGNED: right operand is not integer-like");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftNumber < rightNumber ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalGreaterSigned(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftNumber;
    TZrInt64 rightNumber;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_SIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_SIGNED: missing value");
        return ZR_FALSE;
    }

    if (!aot_runtime_extract_integer_like_value(leftValue, &leftNumber)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_SIGNED: left operand is not integer-like");
        return ZR_FALSE;
    }
    if (!aot_runtime_extract_integer_like_value(rightValue, &rightNumber)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_SIGNED: right operand is not integer-like");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftNumber > rightNumber ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalLessEqualSigned(SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftNumber;
    TZrInt64 rightNumber;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_SIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_SIGNED: missing value");
        return ZR_FALSE;
    }

    if (!aot_runtime_extract_integer_like_value(leftValue, &leftNumber)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_SIGNED: left operand is not integer-like");
        return ZR_FALSE;
    }
    if (!aot_runtime_extract_integer_like_value(rightValue, &rightNumber)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_SIGNED: right operand is not integer-like");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftNumber <= rightNumber ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalGreaterEqualSigned(SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftNumber;
    TZrInt64 rightNumber;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_SIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_SIGNED: missing value");
        return ZR_FALSE;
    }

    if (!aot_runtime_extract_integer_like_value(leftValue, &leftNumber)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_SIGNED: left operand is not integer-like");
        return ZR_FALSE;
    }
    if (!aot_runtime_extract_integer_like_value(rightValue, &rightNumber)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_SIGNED: right operand is not integer-like");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftNumber >= rightNumber ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_IsTruthy(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 sourceSlot,
                                      TZrBool *outTruthy) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || outTruthy == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "JUMP_IF: invalid condition slot");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "JUMP_IF: missing condition value");
        return ZR_FALSE;
    }

    *outTruthy = aot_runtime_value_is_truthy(state, sourceValue);
    return ZR_TRUE;
}

/* 为 signed 分支集中区分右槽与常量并计算跳转谓词。outShouldJump 非空，失败前置 false，signed、unsigned、bool 可提取，常量取自当前函数，只支持四种分支 opcode。 */
static TZrBool aot_runtime_should_jump_signed_compare(
        SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 leftSlot,
        TZrUInt32 rightOperand,
        EZrInstructionCode opcode,
        TZrBool rightIsConstant,
        TZrBool *outShouldJump) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer =
            rightIsConstant ? ZR_NULL : aot_runtime_frame_slot(frame, rightOperand);
    const SZrTypeValue *leftValue;
    const SZrTypeValue *rightValue;
    TZrInt64 leftNumber;
    TZrInt64 rightNumber;
    const TZrChar *operationName;
    TZrChar errorMessage[128];

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
            operationName = "JUMP_IF_GREATER_SIGNED";
            break;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):
            operationName = "JUMP_IF_LESS_EQUAL_SIGNED";
            break;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
            operationName = "JUMP_IF_NOT_EQUAL_SIGNED";
            break;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
            operationName = "JUMP_IF_NOT_EQUAL_SIGNED_CONST";
            break;
        default:
            operationName = "SIGNED_BRANCH";
            break;
    }

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (outShouldJump != ZR_NULL) {
        *outShouldJump = ZR_FALSE;
    }
    if (state == ZR_NULL || leftPointer == ZR_NULL ||
        (!rightIsConstant && rightPointer == ZR_NULL) || outShouldJump == ZR_NULL) {
        snprintf(errorMessage, sizeof(errorMessage), "%s: invalid stack slot", operationName);
        aot_runtime_fail(state, runtimeState, errorMessage);
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = rightIsConstant
                         ? aot_runtime_frame_constant(frame, rightOperand)
                         : ZrCore_Stack_GetValue(rightPointer);
    if (leftValue == ZR_NULL || rightValue == ZR_NULL) {
        snprintf(errorMessage, sizeof(errorMessage), "%s: missing value", operationName);
        aot_runtime_fail(state, runtimeState, errorMessage);
        return ZR_FALSE;
    }
    if (!aot_runtime_extract_integer_like_value(leftValue, &leftNumber)) {
        snprintf(errorMessage, sizeof(errorMessage), "%s: left operand is not integer-like", operationName);
        aot_runtime_fail(state, runtimeState, errorMessage);
        return ZR_FALSE;
    }
    if (!aot_runtime_extract_integer_like_value(rightValue, &rightNumber)) {
        snprintf(errorMessage, sizeof(errorMessage), "%s: right operand is not integer-like", operationName);
        aot_runtime_fail(state, runtimeState, errorMessage);
        return ZR_FALSE;
    }

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
            *outShouldJump = leftNumber > rightNumber ? ZR_TRUE : ZR_FALSE;
            break;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):
            *outShouldJump = leftNumber <= rightNumber ? ZR_TRUE : ZR_FALSE;
            break;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
            *outShouldJump = leftNumber != rightNumber ? ZR_TRUE : ZR_FALSE;
            break;
        default:
            snprintf(errorMessage, sizeof(errorMessage), "%s: unsupported comparison", operationName);
            aot_runtime_fail(state, runtimeState, errorMessage);
            return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ShouldJumpIfGreaterSigned(SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot,
                                                       TZrBool *outShouldJump) {
    return aot_runtime_should_jump_signed_compare(state,
                                                  frame,
                                                  leftSlot,
                                                  rightSlot,
                                                  ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED),
                                                  ZR_FALSE,
                                                  outShouldJump);
}

TZrBool ZrLibrary_AotRuntime_ShouldJumpIfLessEqualSigned(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot,
                                                        TZrBool *outShouldJump) {
    return aot_runtime_should_jump_signed_compare(state,
                                                  frame,
                                                  leftSlot,
                                                  rightSlot,
                                                  ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED),
                                                  ZR_FALSE,
                                                  outShouldJump);
}

TZrBool ZrLibrary_AotRuntime_ShouldJumpIfNotEqualSigned(SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot,
                                                       TZrBool *outShouldJump) {
    return aot_runtime_should_jump_signed_compare(state,
                                                  frame,
                                                  leftSlot,
                                                  rightSlot,
                                                  ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED),
                                                  ZR_FALSE,
                                                  outShouldJump);
}

TZrBool ZrLibrary_AotRuntime_ShouldJumpIfNotEqualSignedConst(SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 leftSlot,
                                                            TZrUInt32 constantIndex,
                                                            TZrBool *outShouldJump) {
    return aot_runtime_should_jump_signed_compare(state,
                                                  frame,
                                                  leftSlot,
                                                  constantIndex,
                                                  ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST),
                                                  ZR_TRUE,
                                                  outShouldJump);
}

/* 复用解释器 ADD 执行边界，让生成代码保留动态加法行为。需要有效 callInfo，委托 ZrCore_Execution_Add，可发生元调用及扩栈，成功后按当前 callInfo 重定位 frame 和 stackTop。 */
TZrBool ZrLibrary_AotRuntime_Add(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 leftSlot,
                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrCallInfo *callInfo;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    if (state == ZR_NULL || frame == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL ||
        rightPointer == ZR_NULL || callInfo == ZR_NULL ||
        !ZrCore_Execution_Add(state,
                              callInfo,
                              ZrCore_Stack_GetValue(destinationPointer),
                              ZrCore_Stack_GetValue(leftPointer),
                              ZrCore_Stack_GetValue(rightPointer))) {
        aot_runtime_fail(state, runtimeState, "ADD: generated AOT helper failed");
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    if (state->callInfoList != ZR_NULL && state->callInfoList->functionBase.valuePointer != ZR_NULL) {
        frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
        state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;
    }
    return ZR_TRUE;
}

/* 按值槽类型执行泛型减法并保留左操作数元方法退路。两 bool 使用现有布尔合取路径，同 signed、同 unsigned、同 float 直接运算，其他类型查 ZR_META_SUB，缺失时成功写 null。 */
TZrBool ZrLibrary_AotRuntime_Sub(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 leftSlot,
                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUB: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUB: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(leftValue->type) && ZR_VALUE_IS_TYPE_BOOL(rightValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeBool,
                          leftValue->value.nativeObject.nativeBool && rightValue->value.nativeObject.nativeBool,
                          ZR_VALUE_TYPE_BOOL);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
        rightInt = rightValue->value.nativeObject.nativeInt64;
        ZrCore_Value_InitAsInt(state, destinationValue, leftInt - rightInt);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        leftUInt = leftValue->value.nativeObject.nativeUInt64;
        rightUInt = rightValue->value.nativeObject.nativeUInt64;
        ZrCore_Value_InitAsUInt(state, destinationValue, leftUInt - rightUInt);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(leftValue->type) && ZR_VALUE_IS_TYPE_FLOAT(rightValue->type)) {
        leftDouble = leftValue->value.nativeObject.nativeDouble;
        rightDouble = rightValue->value.nativeObject.nativeDouble;
        ZrCore_Value_InitAsFloat(state, destinationValue, leftDouble - rightDouble);
        return ZR_TRUE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_SUB);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}

TZrBool ZrLibrary_AotRuntime_Mul(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 leftSlot,
                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MUL: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MUL: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
        rightInt = rightValue->value.nativeObject.nativeInt64;
        ZrCore_Value_InitAsInt(state, destinationValue, leftInt * rightInt);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        leftUInt = leftValue->value.nativeObject.nativeUInt64;
        rightUInt = rightValue->value.nativeObject.nativeUInt64;
        ZrCore_Value_InitAsUInt(state, destinationValue, leftUInt * rightUInt);
        return ZR_TRUE;
    }

    if (aot_runtime_extract_integer_like_value(leftValue, &leftInt) &&
        aot_runtime_extract_integer_like_value(rightValue, &rightInt) &&
        !ZR_VALUE_IS_TYPE_FLOAT(leftValue->type) &&
        !ZR_VALUE_IS_TYPE_FLOAT(rightValue->type)) {
        ZrCore_Value_InitAsInt(state, destinationValue, leftInt * rightInt);
        return ZR_TRUE;
    }

    if (aot_runtime_extract_numeric_double(leftValue, &leftDouble) &&
        aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Value_InitAsFloat(state, destinationValue, leftDouble * rightDouble);
        return ZR_TRUE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_MUL);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}

TZrBool ZrLibrary_AotRuntime_Div(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 leftSlot,
                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "DIV: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "DIV: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
        rightInt = rightValue->value.nativeObject.nativeInt64;
        if (rightInt == 0) {
            ZrCore_Debug_RunError(state, "divide by zero");
        }
        ZrCore_Value_InitAsInt(state, destinationValue, leftInt / rightInt);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        leftUInt = leftValue->value.nativeObject.nativeUInt64;
        rightUInt = rightValue->value.nativeObject.nativeUInt64;
        if (rightUInt == 0) {
            ZrCore_Debug_RunError(state, "divide by zero");
        }
        ZrCore_Value_InitAsUInt(state, destinationValue, leftUInt / rightUInt);
        return ZR_TRUE;
    }

    if (aot_runtime_extract_integer_like_value(leftValue, &leftInt) &&
        aot_runtime_extract_integer_like_value(rightValue, &rightInt) &&
        !ZR_VALUE_IS_TYPE_FLOAT(leftValue->type) &&
        !ZR_VALUE_IS_TYPE_FLOAT(rightValue->type)) {
        if (rightInt == 0) {
            ZrCore_Debug_RunError(state, "divide by zero");
        }
        ZrCore_Value_InitAsInt(state, destinationValue, leftInt / rightInt);
        return ZR_TRUE;
    }

    if (aot_runtime_extract_numeric_double(leftValue, &leftDouble) &&
        aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        if (rightDouble == 0.0) {
            ZrCore_Debug_RunError(state, "divide by zero");
        }
        ZrCore_Value_InitAsFloat(state, destinationValue, leftDouble / rightDouble);
        return ZR_TRUE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_DIV);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}

/* 为浮点运算和数值退路读取统一 double 标量。仅接受原生 signed、unsigned、float、bool，bool 映射为 0/1，不调用元方法、不解包对象，大整数转 double 可能舍入。 */
static TZrBool aot_runtime_extract_numeric_double(const SZrTypeValue *value, TZrFloat64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = (TZrFloat64)value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = (TZrFloat64)value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        *outValue = value->value.nativeObject.nativeDouble;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        *outValue = value->value.nativeObject.nativeBool ? 1.0 : 0.0;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

/* 为 signed 运算及分支提供整数类标量。接受 signed、unsigned、bool，unsigned 用 C 转型读作 int64，bool 映射为 0/1，不接受 float、null 或对象。 */
static TZrBool aot_runtime_extract_integer_like_value(const SZrTypeValue *value, TZrInt64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = (TZrInt64)value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        *outValue = value->value.nativeObject.nativeBool ? 1 : 0;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

/* 为 unsigned 算术及比较提供整数类标量。接受 unsigned、signed、bool，负 signed 按 C unsigned 转换，不接受 float、null 或对象。 */
static TZrBool aot_runtime_extract_unsigned_integer_like_value(const SZrTypeValue *value, TZrUInt64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = (TZrUInt64)value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        *outValue = value->value.nativeObject.nativeBool ? 1u : 0u;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

/* 集中计算浮点专用入口选定的运算。outResult 非空，未知 operation 返回 false，直接使用 C 浮点运算、fmod、pow，不添加零除或定义域门禁。 */
static TZrBool aot_runtime_eval_binary_numeric_float(EZrAotRuntimeFloatBinaryOp operation,
                                                     TZrFloat64 leftValue,
                                                     TZrFloat64 rightValue,
                                                     TZrFloat64 *outResult) {
    if (outResult == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (operation) {
        case ZR_AOT_RUNTIME_FLOAT_BINARY_ADD:
            *outResult = leftValue + rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_FLOAT_BINARY_SUB:
            *outResult = leftValue - rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_FLOAT_BINARY_MUL:
            *outResult = leftValue * rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_FLOAT_BINARY_DIV:
            *outResult = leftValue / rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_FLOAT_BINARY_MOD:
            *outResult = fmod(leftValue, rightValue);
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_FLOAT_BINARY_POW:
            *outResult = pow(leftValue, rightValue);
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

/* 集中计算浮点关系比较并输出布尔值。outResult 非空，只处理四种关系枚举，采用 C double 比较，包括 NaN 的比较结果。 */
static TZrBool aot_runtime_eval_binary_numeric_compare(EZrAotRuntimeCompareOp operation,
                                                       TZrFloat64 leftValue,
                                                       TZrFloat64 rightValue,
                                                       TZrBool *outResult) {
    if (outResult == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (operation) {
        case ZR_AOT_RUNTIME_COMPARE_GREATER:
            *outResult = leftValue > rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_COMPARE_LESS:
            *outResult = leftValue < rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_COMPARE_GREATER_EQUAL:
            *outResult = leftValue >= rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_COMPARE_LESS_EQUAL:
            *outResult = leftValue <= rightValue;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

/* CloseScope 依照生成指令给的登记数关闭当前 VM 链上的值，返回实际关闭数量。 */
/* TODO: 登记链不足 cleanupCount 时这里仅返回较小 closedCount，而 CloseScope
 * 未核对它；需验证生成器的计数约束或补不一致夹具，避免静默遗漏清理。 */
/* 按 LIFO 关闭指定数量作用域注册；临时抬高 top 保护活动帧，回调后按 offset 恢复原 top。 */
static TZrSize aot_runtime_close_scope_registrations(SZrState *state, TZrSize cleanupCount) {
    TZrSize closedCount = 0;
    TZrMemoryOffset savedStackTopOffset;
    SZrCallInfo *currentCallInfo;

    if (state == ZR_NULL || cleanupCount == 0) {
        return 0;
    }

    savedStackTopOffset = ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer);
    currentCallInfo = state->callInfoList;
    if (currentCallInfo != ZR_NULL &&
        state->stackTop.valuePointer < currentCallInfo->functionTop.valuePointer) {
        state->stackTop.valuePointer = currentCallInfo->functionTop.valuePointer;
    }

    while (closedCount < cleanupCount &&
           state->toBeClosedValueList.valuePointer > state->stackBase.valuePointer) {
        TZrStackPointer toBeClosed = state->toBeClosedValueList;
        ZrCore_Closure_CloseStackValue(state, toBeClosed.valuePointer);
        ZrCore_Closure_CloseRegisteredValues(state, 1, ZR_THREAD_STATUS_INVALID, ZR_FALSE);
        closedCount++;
    }

    state->stackTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, savedStackTopOffset);
    return closedCount;
}

/* 让浮点 opcode 共用值槽检查、标量提取和 double 结果写回。源为原生数值或 bool，无法提取会报运行错误，不查元方法，运算结果经 InitAsFloat 写为 DOUBLE。 */
static TZrBool aot_runtime_apply_float_binary_operation(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot,
                                                        EZrAotRuntimeFloatBinaryOp operation,
                                                        const TZrChar *instructionName) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrFloat64 leftNumber;
    TZrFloat64 rightNumber;
    TZrFloat64 resultValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "%s: invalid stack slot", instructionName);
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "%s: missing value", instructionName);
        return ZR_FALSE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftNumber) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightNumber) ||
        !aot_runtime_eval_binary_numeric_float(operation, leftNumber, rightNumber, &resultValue)) {
        ZrCore_Debug_RunError(state, "%s requires numeric operands", instructionName);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsFloat(state, destinationValue, resultValue);
    return ZR_TRUE;
}

/* 让浮点关系 opcode 共用值槽检查和数值提升。源为原生数值或 bool，无法提取会报运行错误，结果写为 BOOL，不查元方法。 */
static TZrBool aot_runtime_apply_float_compare_operation(SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot,
                                                         EZrAotRuntimeCompareOp operation,
                                                         const TZrChar *instructionName) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrFloat64 leftNumber;
    TZrFloat64 rightNumber;
    TZrBool resultValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "%s: invalid stack slot", instructionName);
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "%s: missing value", instructionName);
        return ZR_FALSE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftNumber) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightNumber) ||
        !aot_runtime_eval_binary_numeric_compare(operation, leftNumber, rightNumber, &resultValue)) {
        ZrCore_Debug_RunError(state, "%s requires numeric operands", instructionName);
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeBool, resultValue ? ZR_TRUE : ZR_FALSE, ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

/* 为 accessor 和运算元方法预留连续 scratch 调用窗口；输出 functionSlot 可超出生成槽数；扩栈后刷新 slotBase。 */
static TZrBool aot_runtime_reserve_temp_call_base(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 scratchSlotCount,
                                                  TZrStackValuePointer *outCallBase,
                                                  TZrUInt32 *outFunctionSlot) {
    SZrCallInfo *callerCallInfo;
    TZrStackValuePointer callBase;
    TZrStackValuePointer reservedBase;

    if (outCallBase != ZR_NULL) {
        *outCallBase = ZR_NULL;
    }
    if (outFunctionSlot != ZR_NULL) {
        *outFunctionSlot = 0;
    }
    if (state == ZR_NULL || frame == ZR_NULL || scratchSlotCount == 0 || outCallBase == ZR_NULL ||
        outFunctionSlot == ZR_NULL || frame->slotBase == ZR_NULL) {
        return ZR_FALSE;
    }

    callerCallInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
    callBase = callerCallInfo != ZR_NULL ? callerCallInfo->functionTop.valuePointer : state->stackTop.valuePointer;
    reservedBase = callBase != ZR_NULL ? ZrCore_Function_ReserveScratchSlots(state, scratchSlotCount, callBase) : ZR_NULL;
    if (reservedBase == ZR_NULL) {
        return ZR_FALSE;
    }

    callBase = callerCallInfo != ZR_NULL ? callerCallInfo->functionTop.valuePointer : reservedBase;
    if (frame->callInfo != ZR_NULL && frame->callInfo->functionBase.valuePointer != ZR_NULL) {
        frame->slotBase = frame->callInfo->functionBase.valuePointer + 1;
    }
    if (callBase == ZR_NULL || frame->slotBase == ZR_NULL || callBase < frame->slotBase) {
        return ZR_FALSE;
    }

    *outCallBase = callBase;
    *outFunctionSlot = (TZrUInt32)(callBase - frame->slotBase);
    return ZR_TRUE;
}

/* 以单结果无 yield 通用调用执行元方法退路；callBase 与 destination 都用锚点；完成后恢复 caller 帧和 top。 */
static TZrBool aot_runtime_call_temp_base_without_yield(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrStackValuePointer callBase,
                                                        TZrUInt32 argumentCount) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer;
    TZrStackValuePointer resultBase;
    SZrFunctionStackAnchor callAnchor;
    SZrFunctionStackAnchor destinationAnchor;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (state == ZR_NULL || frame == ZR_NULL || callBase == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "META_CALL: invalid call target (callBase=%p destination=%p)",
                         (void *)callBase,
                         (void *)destinationPointer);
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, callBase, &callAnchor);
    ZrCore_Function_StackAnchorInit(state, destinationPointer, &destinationAnchor);
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (frame->callInfo != ZR_NULL && frame->callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        frame->callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    resultBase = ZrCore_Function_CallWithoutYieldAndRestoreAnchor(state, &callAnchor, 1);
    destinationPointer = ZrCore_Function_StackAnchorRestore(state, &destinationAnchor);
    if (resultBase == ZR_NULL || destinationPointer == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        state->callInfoList == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "META_CALL: generic invoke failed (resultBase=%p destination=%p threadStatus=%u callInfo=%p)",
                         (void *)resultBase,
                         (void *)destinationPointer,
                         (unsigned)(state != ZR_NULL ? state->threadStatus : 0),
                         (void *)(state != ZR_NULL ? state->callInfoList : ZR_NULL));
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(destinationPointer), ZrCore_Stack_GetValue(resultBase));
    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;
    return ZR_TRUE;
}

/* 一元运算元方法可走已载入 AOT thunk，否则退回 core 的无 yield 调用。 */
/* TODO: 核查 Neg/转换元方法预留临时槽后 receiverValue 的来源：沿合法生成调用进入 ReserveScratchSlots/CheckStackAndGc 与 stack_realloc_internal，构造确实迁移 VM 栈且返回后重读旧来源的全链证据；确认需保存锚点或稳定副本并重取的责任，尚无合法程序/分配器触发证明。 */

static TZrBool aot_runtime_invoke_unary_meta(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 destinationSlot,
                                             const SZrTypeValue *receiverValue,
                                             SZrFunction *metadataFunction) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer callBase = ZR_NULL;
    TZrUInt32 functionSlot = 0;
    SZrTypeValue *functionValue;
    SZrTypeValue stableReceiver;
    SZrLibraryAotLoadedModule *record;
    TZrUInt32 functionIndex;
    ZrAotGeneratedDirectCall directCall;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || receiverValue == ZR_NULL || metadataFunction == ZR_NULL ||
        !aot_runtime_reserve_temp_call_base(state, frame, 2, &callBase, &functionSlot)) {
        return ZR_FALSE;
    }

    functionValue = ZrCore_Stack_GetValue(callBase);
    if (functionValue == ZR_NULL) {
        return ZR_FALSE;
    }

    stableReceiver = *receiverValue;
    ZrCore_Value_InitAsRawObject(state, functionValue, ZR_CAST_RAW_OBJECT_AS_SUPER(metadataFunction));
    ZrCore_Stack_CopyValue(state, callBase + 1, &stableReceiver);

    state->stackTop.valuePointer = callBase + 2;
    if (frame->callInfo != ZR_NULL && frame->callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        frame->callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    memset(&directCall, 0, sizeof(directCall));
    record = runtimeState != ZR_NULL ? aot_runtime_find_record_for_function(runtimeState, metadataFunction) : ZR_NULL;
    if (record != ZR_NULL && record->codeRegistration != ZR_NULL &&
        record->codeRegistration->functionPointers != ZR_NULL) {
        functionIndex = aot_runtime_find_function_index_in_record(record, metadataFunction);
        if (functionIndex != UINT32_MAX && functionIndex < record->codeRegistration->functionCount &&
            record->codeRegistration->functionPointers[functionIndex] != ZR_NULL) {
            if (!aot_runtime_prepare_vm_direct_call_frame(state,
                                                          frame,
                                                          destinationSlot,
                                                          functionSlot,
                                                          1,
                                                          record,
                                                          metadataFunction,
                                                          functionIndex,
                                                          ZR_NULL,
                                                          &directCall)) {
                return ZR_FALSE;
            }
            directCall.nativeFunction = record->codeRegistration->functionPointers[functionIndex];
            return ZrLibrary_AotRuntime_CallPreparedOrGeneric(state,
                                                              frame,
                                                              &directCall,
                                                              destinationSlot,
                                                              functionSlot,
                                                              1,
                                                              1);
        }
    }

    return aot_runtime_call_temp_base_without_yield(state, frame, destinationSlot, callBase, 1);
}

/* 二元元方法与一元路径共用临时调用帧，额外传递右操作数。 */
/* TODO: 核查 二元元方法预留临时槽后 receiverValue/argumentValue 的来源：沿合法生成调用进入 ReserveScratchSlots/CheckStackAndGc 与 stack_realloc_internal，构造确实迁移 VM 栈且返回后重读旧来源的全链证据；确认需保存锚点或稳定副本并重取的责任，尚无合法程序/分配器触发证明。 */

static TZrBool aot_runtime_invoke_binary_meta(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              const SZrTypeValue *receiverValue,
                                              const SZrTypeValue *argumentValue,
                                              SZrFunction *metadataFunction) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer callBase = ZR_NULL;
    TZrUInt32 functionSlot = 0;
    SZrTypeValue *functionValue;
    SZrTypeValue stableReceiver;
    SZrTypeValue stableArgument;
    SZrLibraryAotLoadedModule *record;
    TZrUInt32 functionIndex;
    ZrAotGeneratedDirectCall directCall;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || receiverValue == ZR_NULL || argumentValue == ZR_NULL ||
        metadataFunction == ZR_NULL || !aot_runtime_reserve_temp_call_base(state, frame, 3, &callBase, &functionSlot)) {
        return ZR_FALSE;
    }

    functionValue = ZrCore_Stack_GetValue(callBase);
    if (functionValue == ZR_NULL) {
        return ZR_FALSE;
    }

    stableReceiver = *receiverValue;
    stableArgument = *argumentValue;
    ZrCore_Value_InitAsRawObject(state, functionValue, ZR_CAST_RAW_OBJECT_AS_SUPER(metadataFunction));
    ZrCore_Stack_CopyValue(state, callBase + 1, &stableReceiver);
    ZrCore_Stack_CopyValue(state, callBase + 2, &stableArgument);

    state->stackTop.valuePointer = callBase + 3;
    if (frame->callInfo != ZR_NULL && frame->callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        frame->callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    memset(&directCall, 0, sizeof(directCall));
    record = runtimeState != ZR_NULL ? aot_runtime_find_record_for_function(runtimeState, metadataFunction) : ZR_NULL;
    if (record != ZR_NULL && record->codeRegistration != ZR_NULL &&
        record->codeRegistration->functionPointers != ZR_NULL) {
        functionIndex = aot_runtime_find_function_index_in_record(record, metadataFunction);
        if (functionIndex != UINT32_MAX && functionIndex < record->codeRegistration->functionCount &&
            record->codeRegistration->functionPointers[functionIndex] != ZR_NULL) {
            if (!aot_runtime_prepare_vm_direct_call_frame(state,
                                                          frame,
                                                          destinationSlot,
                                                          functionSlot,
                                                          2,
                                                          record,
                                                          metadataFunction,
                                                          functionIndex,
                                                          ZR_NULL,
                                                          &directCall)) {
                return ZR_FALSE;
            }
            directCall.nativeFunction = record->codeRegistration->functionPointers[functionIndex];
            return ZrLibrary_AotRuntime_CallPreparedOrGeneric(state,
                                                              frame,
                                                              &directCall,
                                                              destinationSlot,
                                                              functionSlot,
                                                              2,
                                                              1);
        }
    }

    return aot_runtime_call_temp_base_without_yield(state, frame, destinationSlot, callBase, 2);
}

/* TODO: 核查 两整数标签载荷的 int64 加法结果越界 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_AddInt(SZrState *state,
                                    ZrAotGeneratedFrame *frame,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (leftValue == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        leftInt = (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    } else {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        rightInt = rightValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        rightInt = (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    } else {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(destinationPointer), leftInt + rightInt);
    return ZR_TRUE;
}

/* TODO: 核查 整数源与函数常量的 int64 加法结果越界 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_AddIntConst(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
        !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(destinationPointer), leftInt + rightInt);
    return ZR_TRUE;
}

/* TODO: 核查 signed 值的 int64 加法结果越界 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_AddSigned(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 leftSlot,
                                       TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (leftValue == ZR_NULL || rightValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state,
                           ZrCore_Stack_GetValue(destinationPointer),
                           leftValue->value.nativeObject.nativeInt64 + rightValue->value.nativeObject.nativeInt64);
    return ZR_TRUE;
}

/* TODO: 核查 signed 源与常量的 int64 加法结果越界 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_AddSignedConst(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type) ||
        !ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state,
                           ZrCore_Stack_GetValue(destinationPointer),
                           leftValue->value.nativeObject.nativeInt64 + rightValue->value.nativeObject.nativeInt64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_AddUnsigned(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (leftValue == ZR_NULL || rightValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsUInt(state,
                            ZrCore_Stack_GetValue(destinationPointer),
                            leftValue->value.nativeObject.nativeUInt64 + rightValue->value.nativeObject.nativeUInt64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_AddUnsignedConst(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL || !ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) ||
        !ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsUInt(state,
                            ZrCore_Stack_GetValue(destinationPointer),
                            leftValue->value.nativeObject.nativeUInt64 + rightValue->value.nativeObject.nativeUInt64);
    return ZR_TRUE;
}

/* TODO: 核查 整数载荷的 int64 减法结果越界 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_SubInt(SZrState *state,
                                    ZrAotGeneratedFrame *frame,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (leftValue == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        leftInt = (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    } else {
        leftInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        rightInt = rightValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        rightInt = (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    } else {
        rightInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(destinationPointer),
                          nativeInt64,
                          leftInt - rightInt,
                          leftValue->type);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "SUB_INT requires numeric operands");
    }

    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(destinationPointer), leftDouble - rightDouble);
    return ZR_TRUE;
}

/* TODO: 核查 整数源与函数常量的 int64 减法结果越界 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_SubIntConst(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        leftInt = (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    } else {
        leftInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        rightInt = rightValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        rightInt = (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    } else {
        rightInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(destinationPointer),
                          nativeInt64,
                          leftInt - rightInt,
                          leftValue->type);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "SUB_INT_CONST requires numeric operands");
    }

    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(destinationPointer), leftDouble - rightDouble);
    return ZR_TRUE;
}

/* TODO: 核查 signed 值的 int64 减法结果越界 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_SubSigned(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 leftSlot,
                                       TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (leftValue == ZR_NULL || rightValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state,
                           ZrCore_Stack_GetValue(destinationPointer),
                           leftValue->value.nativeObject.nativeInt64 - rightValue->value.nativeObject.nativeInt64);
    return ZR_TRUE;
}

/* TODO: 核查 signed 源与常量的 int64 减法结果越界 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_SubSignedConst(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type) ||
        !ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state,
                           ZrCore_Stack_GetValue(destinationPointer),
                           leftValue->value.nativeObject.nativeInt64 - rightValue->value.nativeObject.nativeInt64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SubUnsigned(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (leftValue == ZR_NULL || rightValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsUInt(state,
                            ZrCore_Stack_GetValue(destinationPointer),
                            leftValue->value.nativeObject.nativeUInt64 - rightValue->value.nativeObject.nativeUInt64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SubUnsignedConst(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL || !ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) ||
        !ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsUInt(state,
                            ZrCore_Stack_GetValue(destinationPointer),
                            leftValue->value.nativeObject.nativeUInt64 - rightValue->value.nativeObject.nativeUInt64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_BitwiseXor(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_XOR: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_XOR: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        aot_runtime_fail(state, runtimeState, "BITWISE_XOR: operands must be integer values");
        return ZR_FALSE;
    }

    leftInt = ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type) ? leftValue->value.nativeObject.nativeInt64
                                                           : (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    rightInt = ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type) ? rightValue->value.nativeObject.nativeInt64
                                                             : (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, leftInt ^ rightInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

/* TODO: 核查 INT64_MIN 与 -1 的除法组合 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_DivSigned(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 leftSlot,
                                       TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (leftValue == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        leftInt = (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    } else {
        leftInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        rightInt = rightValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        rightInt = (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    } else {
        rightInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        if (rightInt == 0) {
            ZrCore_Debug_RunError(state, "divide by zero");
        }
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(destinationPointer), leftInt / rightInt);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "DIV_SIGNED requires numeric operands");
    }

    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(destinationPointer), leftDouble / rightDouble);
    return ZR_TRUE;
}

/* TODO: 核查 INT64_MIN 与常量 -1 的除法组合 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_DivSignedConst(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        leftInt = (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    } else {
        leftInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        rightInt = rightValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        rightInt = (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    } else {
        rightInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        if (rightInt == 0) {
            ZrCore_Debug_RunError(state, "divide by zero");
        }
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(destinationPointer), leftInt / rightInt);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "DIV_SIGNED_CONST requires numeric operands");
    }

    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(destinationPointer), leftDouble / rightDouble);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_DivUnsignedConst(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;
    TZrUInt64 divisor;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL || !ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) ||
        !ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        return ZR_FALSE;
    }

    divisor = rightValue->value.nativeObject.nativeUInt64;
    if (divisor == 0) {
        ZrCore_Debug_RunError(state, "divide by zero");
    }

    ZrCore_Value_InitAsUInt(state,
                            ZrCore_Stack_GetValue(destinationPointer),
                            leftValue->value.nativeObject.nativeUInt64 / divisor);
    return ZR_TRUE;
}

/* TODO: 核查 signed 值的 int64 乘法结果越界 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_MulSigned(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 leftSlot,
                                       TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (leftValue == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        ZrCore_Value_InitAsInt(state,
                               ZrCore_Stack_GetValue(destinationPointer),
                               leftValue->value.nativeObject.nativeInt64 * rightValue->value.nativeObject.nativeInt64);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(destinationPointer), leftDouble * rightDouble);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_MulUnsignedConst(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL || !ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) ||
        !ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsUInt(state,
                            ZrCore_Stack_GetValue(destinationPointer),
                            leftValue->value.nativeObject.nativeUInt64 * rightValue->value.nativeObject.nativeUInt64);
    return ZR_TRUE;
}

/* TODO: 核查 signed 源与常量的 int64 乘法结果越界 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_MulSignedConst(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        ZrCore_Value_InitAsInt(state,
                               ZrCore_Stack_GetValue(destinationPointer),
                               leftValue->value.nativeObject.nativeInt64 * rightValue->value.nativeObject.nativeInt64);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(destinationPointer), leftDouble * rightDouble);
    return ZR_TRUE;
}

/* TODO: 核查 INT64_MIN 的取负 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_Neg(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 sourceSlot) {
    SZrMeta *metaValue;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeInt64,
                          -sourceValue->value.nativeObject.nativeInt64,
                          sourceValue->type);
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZrCore_Value_InitAsInt(state, destinationValue, -(TZrInt64)sourceValue->value.nativeObject.nativeUInt64);
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeDouble,
                          -sourceValue->value.nativeObject.nativeDouble,
                          sourceValue->type);
        return ZR_TRUE;
    }

    metaValue = ZrCore_Value_GetMeta(state, sourceValue, ZR_META_NEG);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_unary_meta(state, frame, destinationSlot, sourceValue, metaValue->function);
}
/* TODO: 核查 模运算除数 INT64_MIN 的取负 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_Mod(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 leftSlot,
                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MOD: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MOD: missing value");
        return ZR_FALSE;
    }

    if ((ZR_VALUE_IS_TYPE_NUMBER(leftValue->type) || ZR_VALUE_IS_TYPE_BOOL(leftValue->type)) &&
        (ZR_VALUE_IS_TYPE_NUMBER(rightValue->type) || ZR_VALUE_IS_TYPE_BOOL(rightValue->type))) {
        if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
            if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
                TZrUInt64 rightUnsigned = rightValue->value.nativeObject.nativeUInt64;

                if (rightUnsigned == 0u) {
                    ZrCore_Debug_RunError(state, "modulo by zero");
                }
                ZR_VALUE_FAST_SET(destinationValue,
                                  nativeUInt64,
                                  leftValue->value.nativeObject.nativeUInt64 % rightUnsigned,
                                  ZR_VALUE_TYPE_UINT64);
                return ZR_TRUE;
            }

            if (!aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
                !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
                aot_runtime_fail(state, runtimeState, "MOD: integer-like extraction failed");
                return ZR_FALSE;
            }
            if (rightInt == 0) {
                ZrCore_Debug_RunError(state, "modulo by zero");
            }
            if (rightInt < 0) {
                rightInt = -rightInt;
            }
            ZrCore_Value_InitAsInt(state, destinationValue, leftInt % rightInt);
            return ZR_TRUE;
        }

        if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
            !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
            aot_runtime_fail(state, runtimeState, "MOD: numeric extraction failed");
            return ZR_FALSE;
        }
        if (rightDouble == 0.0) {
            ZrCore_Debug_RunError(state, "modulo by zero");
        }
        ZrCore_Value_InitAsFloat(state, destinationValue, fmod(leftDouble, rightDouble));
        return ZR_TRUE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_MOD);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}
/* TODO: 核查 常量模除数 INT64_MIN 的取负 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_ModSignedConst(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        if (!aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
            !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
            return ZR_FALSE;
        }
        if (rightInt == 0) {
            ZrCore_Debug_RunError(state, "modulo by zero");
        }
        if (rightInt < 0) {
            rightInt = -rightInt;
        }
        ZrCore_Value_InitAsInt(state, destinationValue, leftInt % rightInt);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "MOD_SIGNED_CONST requires numeric operands");
    }
    if (rightDouble == 0.0) {
        ZrCore_Debug_RunError(state, "modulo by zero");
    }
    ZrCore_Value_InitAsFloat(state, destinationValue, fmod(leftDouble, rightDouble));
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ModUnsignedConst(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    TZrUInt64 divisor;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        return ZR_FALSE;
    }

    divisor = rightValue->value.nativeObject.nativeUInt64;
    if (divisor == 0) {
        ZrCore_Debug_RunError(state, "modulo by zero");
    }

    ZrCore_Value_InitAsUInt(state,
                            destinationValue,
                            leftValue->value.nativeObject.nativeUInt64 % divisor);
    return ZR_TRUE;
}

/* 将值槽转换为字符串并建立生成代码可继续使用的目标槽。core 转换可能分配或调用元方法，必须按 callInfo 刷新 frame 后重新取目标槽，成功结果是 GC 字符串，空结果写 null。 */
TZrBool ZrLibrary_AotRuntime_ToString(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;
    SZrString *resultString;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_STRING: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_STRING: missing value");
        return ZR_FALSE;
    }

    resultString = ZrCore_Value_ConvertToString(state, sourceValue);
    callInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
    if (callInfo != ZR_NULL && callInfo->functionBase.valuePointer != ZR_NULL &&
        !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        aot_runtime_fail(state, runtimeState, "TO_STRING: failed to refresh frame after conversion");
        return ZR_FALSE;
    }
    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    destinationValue = destinationPointer != ZR_NULL ? ZrCore_Stack_GetValue(destinationPointer) : ZR_NULL;
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_STRING: destination slot lost after conversion");
        return ZR_FALSE;
    }
    if (resultString != ZR_NULL) {
        ZrCore_Value_InitAsRawObject(state, destinationValue, ZR_CAST_RAW_OBJECT_AS_SUPER(resultString));
        destinationValue->type = ZR_VALUE_TYPE_STRING;
    } else {
        ZrCore_Value_ResetAsNull(destinationValue);
    }
    return ZR_TRUE;
}

/** @brief 创建绑定成员位置对象。
 * @note 成员表索引按当前函数解释；内联字段保存来源帧锚点，其他分支保存原型描述符；位置对象不延长来源帧寿命。创建失败可已有外壳或字段，不能承诺目的槽原值保持。
 */
TZrBool ZrLibrary_AotRuntime_PropertyReferenceCreateMember(
        SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 receiverSlot,
        TZrUInt32 memberEntryIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    SZrFunction *function = (SZrFunction *)aot_runtime_frame_function(frame);
    SZrTypeValue *destinationValue;
    SZrTypeValue *receiverValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL
                    ? aot_runtime_get_state_from_global(state->global)
                    : ZR_NULL;
    destinationValue =
            destinationPointer != ZR_NULL ? ZrCore_Stack_GetValue(destinationPointer) : ZR_NULL;
    receiverValue =
            receiverPointer != ZR_NULL ? ZrCore_Stack_GetValue(receiverPointer) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || function == ZR_NULL ||
        destinationValue == ZR_NULL || receiverValue == ZR_NULL ||
        !ZrCore_PropertyReference_CreateMember(
                state,
                function,
                frame->slotBase,
                receiverSlot,
                receiverValue,
                memberEntryIndex,
                destinationValue)) {
        aot_runtime_fail(
                state,
                runtimeState,
                "PROPERTY_REF_CREATE_MEMBER: invalid bound Place");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 捕获稍后读写使用的接收者和键。
 * @note 捕获位置而非立即取值；位置外壳交给目的槽持有，索引协议在 Load/Store 才调用。分步建壳失败无整体回滚保证。
 */
TZrBool ZrLibrary_AotRuntime_PropertyReferenceCreateIndex(
        SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 receiverSlot,
        TZrUInt32 keySlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    TZrStackValuePointer keyPointer = aot_runtime_frame_slot(frame, keySlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL
                    ? aot_runtime_get_state_from_global(state->global)
                    : ZR_NULL;
    destinationValue =
            destinationPointer != ZR_NULL ? ZrCore_Stack_GetValue(destinationPointer) : ZR_NULL;
    receiverValue =
            receiverPointer != ZR_NULL ? ZrCore_Stack_GetValue(receiverPointer) : ZR_NULL;
    keyValue = keyPointer != ZR_NULL ? ZrCore_Stack_GetValue(keyPointer) : ZR_NULL;
    if (state == ZR_NULL || destinationValue == ZR_NULL ||
        receiverValue == ZR_NULL || keyValue == ZR_NULL ||
        !ZrCore_PropertyReference_CreateIndex(
                state, receiverValue, keyValue, destinationValue)) {
        aot_runtime_fail(
                state,
                runtimeState,
                "PROPERTY_REF_CREATE_INDEX: invalid indexed Place");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 把本地槽绑定为可稍后读写的位置。
 * @note 内联槽追踪实际别名来源，普通槽保存函数与相对栈锚点；引用不拥有活动调用帧，调用方须限制其有效期。
 */
TZrBool ZrLibrary_AotRuntime_PropertyReferenceCreateLocal(
        SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer =
            aot_runtime_frame_slot(frame, destinationSlot);
    SZrFunction *function = (SZrFunction *)aot_runtime_frame_function(frame);
    SZrTypeValue *destinationValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL
                    ? aot_runtime_get_state_from_global(state->global)
                    : ZR_NULL;
    destinationValue = destinationPointer != ZR_NULL
            ? ZrCore_Stack_GetValue(destinationPointer)
            : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || function == ZR_NULL ||
        destinationValue == ZR_NULL ||
        !ZrCore_PropertyReference_CreateFrameSlot(
                state,
                function,
                frame->slotBase,
                sourceSlot,
                destinationValue)) {
        aot_runtime_fail(
                state,
                runtimeState,
                "PROPERTY_REF_CREATE_LOCAL: invalid local Place");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 经位置种类读取到目的槽。
 * @note 由 core 校验帧锚点、成员描述符或动态索引；目的槽覆盖沿值所有权规则，回调失败不保证保留原目的值。
 */
TZrBool ZrLibrary_AotRuntime_PropertyReferenceLoad(
        SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 referenceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer referencePointer = aot_runtime_frame_slot(frame, referenceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *referenceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL
                    ? aot_runtime_get_state_from_global(state->global)
                    : ZR_NULL;
    destinationValue =
            destinationPointer != ZR_NULL ? ZrCore_Stack_GetValue(destinationPointer) : ZR_NULL;
    referenceValue =
            referencePointer != ZR_NULL ? ZrCore_Stack_GetValue(referencePointer) : ZR_NULL;
    if (state == ZR_NULL || destinationValue == ZR_NULL ||
        referenceValue == ZR_NULL ||
        !ZrCore_PropertyReference_Load(state, referenceValue, destinationValue)) {
        aot_runtime_fail(
                state,
                runtimeState,
                "PROPERTY_REF_LOAD: invalid managed property reference");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 向已捕获的位置写入源值。
 * @note 成员分支仍验证可写性，帧槽分支同步关闭语义的物理镜像；返回失败仅是操作未成功，不撤销已执行回调或部分写入。
 */
TZrBool ZrLibrary_AotRuntime_PropertyReferenceStore(
        SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 sourceSlot,
        TZrUInt32 referenceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer referencePointer = aot_runtime_frame_slot(frame, referenceSlot);
    SZrTypeValue *sourceValue;
    SZrTypeValue *referenceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL
                    ? aot_runtime_get_state_from_global(state->global)
                    : ZR_NULL;
    sourceValue = sourcePointer != ZR_NULL ? ZrCore_Stack_GetValue(sourcePointer) : ZR_NULL;
    referenceValue =
            referencePointer != ZR_NULL ? ZrCore_Stack_GetValue(referencePointer) : ZR_NULL;
    if (state == ZR_NULL || sourceValue == ZR_NULL ||
        referenceValue == ZR_NULL ||
        !ZrCore_PropertyReference_Store(state, referenceValue, sourceValue)) {
        aot_runtime_fail(
                state,
                runtimeState,
                "PROPERTY_REF_STORE: invalid managed property reference");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 按成员表符号读取对象成员。
 * @note 局部接收者副本防止目的槽与接收者同槽覆盖；副本不是新增持有凭据。生成调用须符合编译与已绑定访问上下文；core 负责模块 pending 等实际运行限制、属性回调和目的值所有权，不补全 private/protected 访问范围检查。
 */
TZrBool ZrLibrary_AotRuntime_GetMember(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 receiverSlot,
                                       TZrUInt32 memberId) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    SZrString *memberSymbol = ZR_NULL;
    SZrTypeValue stableReceiver;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || receiverPointer == ZR_NULL ||
        !aot_runtime_resolve_member_symbol(aot_runtime_frame_function(frame), memberId, &memberSymbol)) {
        aot_runtime_fail(state, runtimeState, "GET_MEMBER: invalid member id");
        return ZR_FALSE;
    }

    stableReceiver = *ZrCore_Stack_GetValue(receiverPointer);
    if (!ZrCore_Object_GetMember(state, &stableReceiver, memberSymbol, ZrCore_Stack_GetValue(destinationPointer))) {
        aot_runtime_fail(state, runtimeState, "GET_MEMBER: missing member");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 按成员表符号写入对象成员。
 * @note 调用方借用活动生成帧和源槽，写权限、域校验及写屏障交给 core；属性回调副作用不因桥接返回失败撤销。
 */
TZrBool ZrLibrary_AotRuntime_SetMember(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 sourceSlot,
                                       TZrUInt32 receiverSlot,
                                       TZrUInt32 memberId) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    SZrString *memberSymbol = ZR_NULL;
    SZrTypeValue *receiverValue;
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL || receiverPointer == ZR_NULL ||
        !aot_runtime_resolve_member_symbol(aot_runtime_frame_function(frame), memberId, &memberSymbol)) {
        aot_runtime_fail(state, runtimeState, "SET_MEMBER: invalid member id");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (receiverValue == ZR_NULL || sourceValue == ZR_NULL ||
        !ZrCore_Object_SetMember(state, receiverValue, memberSymbol, sourceValue)) {
        aot_runtime_fail(state, runtimeState, "SET_MEMBER: receiver must be a writable object member");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 为生成器提供新接收者写入优化提示。
 * @note 当前 core 对象路径还按实际年轻可移动存储检查免屏障条件；优化名不放宽成员写权限或域检查。
 * TODO: 对照生成器 backend_aot_c_slot_has_unescaped_new_owner 的控制流和别名边界及 core 年轻存储检查，核实哪些写入实际获免屏障，不据局部证明直接推定老接收者漏屏障。
 */
TZrBool ZrLibrary_AotRuntime_SetMemberNewOwnerNoWriteBarrier(SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 sourceSlot,
                                                             TZrUInt32 receiverSlot,
                                                             TZrUInt32 memberId) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    SZrString *memberSymbol = ZR_NULL;
    SZrTypeValue *receiverValue;
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL || receiverPointer == ZR_NULL ||
        !aot_runtime_resolve_member_symbol(aot_runtime_frame_function(frame), memberId, &memberSymbol)) {
        aot_runtime_fail(state, runtimeState, "SET_MEMBER: invalid member id");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (receiverValue == ZR_NULL || sourceValue == ZR_NULL ||
        !ZrCore_Object_SetMemberAssumeNewOwnerNoWriteBarrier(state, receiverValue, memberSymbol, sourceValue)) {
        aot_runtime_fail(state, runtimeState, "SET_MEMBER: receiver must be a writable object member");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 按成员缓存契约选择读取路径。
 * @note 已绑定缓存准备可调用目标而非读取普通字段，失配返回失败且不降级；无绑定缓存先试物理内联成员，再以符号访问对象。
 */
TZrBool ZrLibrary_AotRuntime_GetMemberSlot(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 receiverSlot,
                                           TZrUInt32 cacheIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    SZrString *memberSymbol = ZR_NULL;
    SZrTypeValue stableReceiver;
    const SZrFunction *function = aot_runtime_frame_function(frame);

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state != ZR_NULL && destinationPointer != ZR_NULL && receiverPointer != ZR_NULL &&
        function != ZR_NULL && function->callSiteCaches != ZR_NULL &&
        cacheIndex < function->callSiteCacheLength &&
        function->callSiteCaches[cacheIndex].kind == ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET &&
        function->callSiteCaches[cacheIndex].binding.contract.bindingKind != ZR_CALL_BINDING_NONE) {
        stableReceiver = *ZrCore_Stack_GetValue(receiverPointer);
        if (!ZrCore_CallBinding_PrepareMember(state, (SZrFunction *)function, cacheIndex,
                &stableReceiver, ZrCore_Stack_GetValue(destinationPointer), &state->lastCallBindingError)) {
            aot_runtime_fail(state, runtimeState, "GET_MEMBER_SLOT: bound target failed (%s)",
                    ZrCore_CallBinding_StatusName(state->lastCallBindingError.status));
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }
    if (state == ZR_NULL || destinationPointer == ZR_NULL || receiverPointer == ZR_NULL ||
        !aot_runtime_resolve_cached_member_symbol(function,
                                                  cacheIndex,
                                                  ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET,
                                                  &memberSymbol)) {
        aot_runtime_fail(state, runtimeState, "GET_MEMBER_SLOT: invalid cached member");
        return ZR_FALSE;
    }

    if (ZrCore_Function_GetFrameSlotInlineMember(state,
                                                 function,
                                                 frame->slotBase,
                                                 destinationSlot,
                                                 receiverSlot,
                                                 memberSymbol)) {
        return ZR_TRUE;
    }

    stableReceiver = *ZrCore_Stack_GetValue(receiverPointer);
    if (!ZrCore_Object_GetMember(state, &stableReceiver, memberSymbol, ZrCore_Stack_GetValue(destinationPointer))) {
        aot_runtime_fail(state, runtimeState, "GET_MEMBER_SLOT: missing member");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 按成员缓存符号写入物理字段或对象成员。
 * @note 内联路径成功即完成，不能继续动态写入；内联不适用时才走对象权限和屏障。cacheIndex 不是成员表索引。
 */
TZrBool ZrLibrary_AotRuntime_SetMemberSlot(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 sourceSlot,
                                           TZrUInt32 receiverSlot,
                                           TZrUInt32 cacheIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    SZrString *memberSymbol = ZR_NULL;
    SZrTypeValue *receiverValue;
    SZrTypeValue *sourceValue;
    const SZrFunction *function = aot_runtime_frame_function(frame);

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL || receiverPointer == ZR_NULL ||
        !aot_runtime_resolve_cached_member_symbol(function,
                                                  cacheIndex,
                                                  ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET,
                                                  &memberSymbol)) {
        aot_runtime_fail(state, runtimeState, "SET_MEMBER_SLOT: invalid cached member");
        return ZR_FALSE;
    }

    if (ZrCore_Function_SetFrameSlotInlineMember(state,
                                                 function,
                                                 frame->slotBase,
                                                 sourceSlot,
                                                 receiverSlot,
                                                 memberSymbol)) {
        return ZR_TRUE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (receiverValue == ZR_NULL || sourceValue == ZR_NULL ||
        !ZrCore_Object_SetMember(state, receiverValue, memberSymbol, sourceValue)) {
        aot_runtime_fail(state, runtimeState, "SET_MEMBER_SLOT: receiver must be a writable object member");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 为缓存成员的对象退路提供免屏障提示。
 * @note 内联字段仍走普通布局写入接口；仅对象退路使用带实际年轻存储校验的优化入口，不能把全部路径称为免屏障写。
 * TODO: 对照物理内联字段写入与对象退路的实际屏障，核查布局或别名变化后生成器新接收者提示是否仍适用。
 */
TZrBool ZrLibrary_AotRuntime_SetMemberSlotNewOwnerNoWriteBarrier(SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 sourceSlot,
                                                                 TZrUInt32 receiverSlot,
                                                                 TZrUInt32 cacheIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    SZrString *memberSymbol = ZR_NULL;
    SZrTypeValue *receiverValue;
    SZrTypeValue *sourceValue;
    const SZrFunction *function = aot_runtime_frame_function(frame);

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL || receiverPointer == ZR_NULL ||
        !aot_runtime_resolve_cached_member_symbol(function,
                                                  cacheIndex,
                                                  ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET,
                                                  &memberSymbol)) {
        aot_runtime_fail(state, runtimeState, "SET_MEMBER_SLOT: invalid cached member");
        return ZR_FALSE;
    }

    if (ZrCore_Function_SetFrameSlotInlineMember(state,
                                                 function,
                                                 frame->slotBase,
                                                 sourceSlot,
                                                 receiverSlot,
                                                 memberSymbol)) {
        return ZR_TRUE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (receiverValue == ZR_NULL || sourceValue == ZR_NULL ||
        !ZrCore_Object_SetMemberAssumeNewOwnerNoWriteBarrier(state, receiverValue, memberSymbol, sourceValue)) {
        aot_runtime_fail(state, runtimeState, "SET_MEMBER_SLOT: receiver must be a writable object member");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 跨动态索引调用读取并刷新生成帧。
 * @note 先稳定接收者和键并初始化临时结果，以活动 callInfo 发布栈顶；回调后重取槽基址和目的槽再按所有权复制，失败不保证回调副作用回滚。
 */
TZrBool ZrLibrary_AotRuntime_GetByIndex(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 receiverSlot,
                                        TZrUInt32 keySlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    TZrStackValuePointer keyPointer = aot_runtime_frame_slot(frame, keySlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue stableReceiver;
    SZrTypeValue stableKey;
    SZrTypeValue stableResult;
    SZrCallInfo *callInfo;
    TZrBool resolved;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || receiverPointer == ZR_NULL || keyPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_BY_INDEX: invalid slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    keyValue = ZrCore_Stack_GetValue(keyPointer);
    if (destinationValue == ZR_NULL || receiverValue == ZR_NULL || keyValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_BY_INDEX: invalid slot value");
        return ZR_FALSE;
    }
    if (receiverValue->type != ZR_VALUE_TYPE_OBJECT && receiverValue->type != ZR_VALUE_TYPE_ARRAY) {
        aot_runtime_fail(state, runtimeState, "GET_BY_INDEX: receiver must be an object or array");
        return ZR_FALSE;
    }

    stableReceiver = *receiverValue;
    stableKey = *keyValue;
    ZrCore_Value_ResetAsNull(&stableResult);
    callInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
    if (callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL ||
        callInfo->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_BY_INDEX: missing active call frame");
        return ZR_FALSE;
    }

    state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    resolved = ZrCore_Object_GetByIndex(state, &stableReceiver, &stableKey, &stableResult);
    if (!aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        aot_runtime_fail(state, runtimeState, "GET_BY_INDEX: failed to refresh frame after index access");
        return ZR_FALSE;
    }

    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    destinationValue = destinationPointer != ZR_NULL ? ZrCore_Stack_GetValue(destinationPointer) : ZR_NULL;
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_BY_INDEX: destination slot lost after index access");
        return ZR_FALSE;
    }

    if (!resolved) {
        aot_runtime_fail(state, runtimeState, "GET_BY_INDEX: receiver must be an object or array");
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, destinationValue, &stableResult);
    return ZR_TRUE;
}

/** @brief 跨动态索引调用写入并恢复生成帧锚点。
 * @note 稳定三份输入值不是转移其所有权；发布活动调用栈后允许协议再入或扩栈，返回后从 callInfo 刷新，失败不撤销已完成的写入。
 */
TZrBool ZrLibrary_AotRuntime_SetByIndex(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 sourceSlot,
                                        TZrUInt32 receiverSlot,
                                        TZrUInt32 keySlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    TZrStackValuePointer keyPointer = aot_runtime_frame_slot(frame, keySlot);
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *sourceValue;
    SZrTypeValue stableReceiver;
    SZrTypeValue stableKey;
    SZrTypeValue stableValue;
    SZrCallInfo *callInfo;
    TZrBool resolved;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL || receiverPointer == ZR_NULL || keyPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: invalid slot");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    keyValue = ZrCore_Stack_GetValue(keyPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (receiverValue == ZR_NULL || keyValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: invalid slot value");
        return ZR_FALSE;
    }
    if (receiverValue->type != ZR_VALUE_TYPE_OBJECT && receiverValue->type != ZR_VALUE_TYPE_ARRAY) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: receiver must be an object or array");
        return ZR_FALSE;
    }

    stableReceiver = *receiverValue;
    stableKey = *keyValue;
    stableValue = *sourceValue;
    callInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
    if (callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL ||
        callInfo->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: missing active call frame");
        return ZR_FALSE;
    }

    state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    resolved = ZrCore_Object_SetByIndex(state, &stableReceiver, &stableKey, &stableValue);
    if (!aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: failed to refresh frame after index write");
        return ZR_FALSE;
    }

    if (!resolved) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: receiver must be an object or array");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/** @brief 带新接收者优化提示执行动态索引写入。
 * @note 保留稳定输入和回调后帧刷新协议；core 根据实际存储决定是否跳过对象写屏障，不能据生成器提示保证 receiver 永远年轻。
 * TODO: 核查生成器局部新接收者扫描与索引回调、数组别名的关系，并在 core 实际年轻存储校验下观察增量收集；尚无老接收者绕过屏障的完整证明。
 */
TZrBool ZrLibrary_AotRuntime_SetByIndexNewOwnerNoWriteBarrier(SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 sourceSlot,
                                                              TZrUInt32 receiverSlot,
                                                              TZrUInt32 keySlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    TZrStackValuePointer keyPointer = aot_runtime_frame_slot(frame, keySlot);
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *sourceValue;
    SZrTypeValue stableReceiver;
    SZrTypeValue stableKey;
    SZrTypeValue stableValue;
    SZrCallInfo *callInfo;
    TZrBool resolved;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL || receiverPointer == ZR_NULL || keyPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: invalid slot");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    keyValue = ZrCore_Stack_GetValue(keyPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (receiverValue == ZR_NULL || keyValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: invalid slot value");
        return ZR_FALSE;
    }
    if (receiverValue->type != ZR_VALUE_TYPE_OBJECT && receiverValue->type != ZR_VALUE_TYPE_ARRAY) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: receiver must be an object or array");
        return ZR_FALSE;
    }

    stableReceiver = *receiverValue;
    stableKey = *keyValue;
    stableValue = *sourceValue;
    callInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
    if (callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL ||
        callInfo->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: missing active call frame");
        return ZR_FALSE;
    }

    state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    resolved = ZrCore_Object_SetByIndexAssumeNewOwnerNoWriteBarrier(state, &stableReceiver, &stableKey, &stableValue);
    if (!aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: failed to refresh frame after index write");
        return ZR_FALSE;
    }

    if (!resolved) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: receiver must be an object or array");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/** @brief 把接收者的当前项存储对象绑定到目的槽。
 * @note 结果是受值系统管理的数组对象引用，不是裸数据指针或独立租约；后续绑定项访问针对这个已解析对象，不重新解析接收者。
 */
TZrBool ZrLibrary_AotRuntime_SuperArrayBindItems(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 receiverSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *receiverValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || receiverPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_BIND_ITEMS: invalid slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    if (destinationValue == ZR_NULL || receiverValue == ZR_NULL ||
        !ZrCore_Object_SuperArrayBindItems(state, destinationValue, receiverValue)) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_BIND_ITEMS: receiver must be an array-like object");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 从已绑定项对象读取有符号整数索引。
 * @note 绑定槽须存放真实内部数组；这个专用入口不走通用索引协议，越界及结果语义交给项存储实现。
 */
TZrBool ZrLibrary_AotRuntime_SuperArrayGetIntBoundItems(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 itemsSlot,
                                                        TZrUInt32 keySlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer itemsPointer = aot_runtime_frame_slot(frame, itemsSlot);
    TZrStackValuePointer keyPointer = aot_runtime_frame_slot(frame, keySlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *itemsValue;
    SZrTypeValue *keyValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || itemsPointer == ZR_NULL || keyPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_GET_INT_ITEMS: invalid slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    itemsValue = ZrCore_Stack_GetValue(itemsPointer);
    keyValue = ZrCore_Stack_GetValue(keyPointer);
    if (destinationValue == ZR_NULL || itemsValue == ZR_NULL || keyValue == ZR_NULL ||
        !ZrCore_Object_SuperArrayGetIntBoundItems(state, itemsValue, keyValue, destinationValue)) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_GET_INT_ITEMS: invalid bound items access");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 向已绑定项对象写入整数载荷。
 * @note 键和值均须为有符号整数且项对象必须是内部数组；不重新解析接收者的 items 成员，也不把绑定结果当裸缓冲区。
 */
TZrBool ZrLibrary_AotRuntime_SuperArraySetIntBoundItems(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 sourceSlot,
                                                        TZrUInt32 itemsSlot,
                                                        TZrUInt32 keySlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer itemsPointer = aot_runtime_frame_slot(frame, itemsSlot);
    TZrStackValuePointer keyPointer = aot_runtime_frame_slot(frame, keySlot);
    SZrTypeValue *sourceValue;
    SZrTypeValue *itemsValue;
    SZrTypeValue *keyValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL || itemsPointer == ZR_NULL || keyPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_SET_INT_ITEMS: invalid slot");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    itemsValue = ZrCore_Stack_GetValue(itemsPointer);
    keyValue = ZrCore_Stack_GetValue(keyPointer);
    if (sourceValue == ZR_NULL || itemsValue == ZR_NULL || keyValue == ZR_NULL ||
        !ZrCore_Object_SuperArraySetIntBoundItems(state, itemsValue, keyValue, sourceValue)) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_SET_INT_ITEMS: invalid bound items access");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 先试整数数组快路径再保留通用索引退路。
 * @note 稳定接收者允许结果与 receiver 同槽；底层不适用快路径时可调用动态索引协议，下一条生成指令须从 callInfo 重取基址。
 */
TZrBool ZrLibrary_AotRuntime_SuperArrayGetInt(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 receiverSlot,
                                              TZrUInt32 keySlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    TZrStackValuePointer keyPointer = aot_runtime_frame_slot(frame, keySlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue stableReceiver;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || receiverPointer == ZR_NULL || keyPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_GET_INT: invalid slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    keyValue = ZrCore_Stack_GetValue(keyPointer);
    if (destinationValue == ZR_NULL || receiverValue == ZR_NULL || keyValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_GET_INT: invalid slot value");
        return ZR_FALSE;
    }

    stableReceiver = *receiverValue;
    if (!ZrCore_Object_SuperArrayGetInt(state, &stableReceiver, keyValue, destinationValue)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "SUPER_ARRAY_GET_INT: receiver must be an array-like object with int index");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 先试整数数组写入再保留通用索引退路。
 * @note 类型特化不代表所有 receiver 必须走快路径；退路仍有权限、域和动态调用语义，失败不提供事务回滚。
 */
TZrBool ZrLibrary_AotRuntime_SuperArraySetInt(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 sourceSlot,
                                              TZrUInt32 receiverSlot,
                                              TZrUInt32 keySlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    TZrStackValuePointer keyPointer = aot_runtime_frame_slot(frame, keySlot);
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL || receiverPointer == ZR_NULL || keyPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_SET_INT: invalid slot");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    keyValue = ZrCore_Stack_GetValue(keyPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (receiverValue == ZR_NULL || keyValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_SET_INT: invalid slot value");
        return ZR_FALSE;
    }

    if (!ZrCore_Object_SuperArraySetInt(state, receiverValue, keyValue, sourceValue)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "SUPER_ARRAY_SET_INT: receiver must be an array-like object with int index");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 为整数数组写入的通用退路提供优化提示。
 * @note 整数快路径与普通入口相同，只有通用索引退路使用新接收者提示；当前对象层实际年轻存储校验仍生效。
 * TODO: 核查生成器新接收者扫描、绑定项别名与通用索引退路；整数快路径及 core 年轻存储检查需分别观察，不能把提示当作屏障漏洞证明。
 */
TZrBool ZrLibrary_AotRuntime_SuperArraySetIntNewOwnerNoWriteBarrier(SZrState *state,
                                                                    ZrAotGeneratedFrame *frame,
                                                                    TZrUInt32 sourceSlot,
                                                                    TZrUInt32 receiverSlot,
                                                                    TZrUInt32 keySlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    TZrStackValuePointer keyPointer = aot_runtime_frame_slot(frame, keySlot);
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL || receiverPointer == ZR_NULL || keyPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_SET_INT: invalid slot");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    keyValue = ZrCore_Stack_GetValue(keyPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (receiverValue == ZR_NULL || keyValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_SET_INT: invalid slot value");
        return ZR_FALSE;
    }

    if (!ZrCore_Object_SuperArraySetIntAssumeNewOwnerNoWriteBarrier(state, receiverValue, keyValue, sourceValue)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "SUPER_ARRAY_SET_INT: receiver must be an array-like object with int index");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 向数组式接收者追加整数并可丢弃结果。
 * @note 返回值标志在这里表示无需目的槽，用已初始化临时结果接收；追加副作用仍执行，慢路径可调用 add 成员，失败不是撤销追加。
 */
TZrBool ZrLibrary_AotRuntime_SuperArrayAddInt(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 receiverSlot,
                                              TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer =
            destinationSlot == ZR_INSTRUCTION_USE_RET_FLAG ? ZR_NULL : aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *sourceValue;
    SZrTypeValue ignoredResult;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || receiverPointer == ZR_NULL || sourcePointer == ZR_NULL ||
        (destinationSlot != ZR_INSTRUCTION_USE_RET_FLAG && destinationPointer == ZR_NULL)) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT: invalid slot");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (receiverValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT: invalid slot value");
        return ZR_FALSE;
    }
    if (destinationSlot == ZR_INSTRUCTION_USE_RET_FLAG) {
        ZrCore_Value_ResetAsNull(&ignoredResult);
        destinationValue = &ignoredResult;
    } else {
        destinationValue = ZrCore_Stack_GetValue(destinationPointer);
        if (destinationValue == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT: invalid slot value");
            return ZR_FALSE;
        }
    }

    if (!ZrCore_Object_SuperArrayAddInt(state, receiverValue, sourceValue, destinationValue)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "SUPER_ARRAY_ADD_INT: receiver must be an array-like object with int payload");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 向四个连续接收者槽批量追加一个整数。
 * @note 四槽是四个接收者而非一个数组的四元素；先检查源类型和槽再由 core 准备及提交，准备或提交可能改变容量和部分接收者，失败不保证整体回滚。
 */
TZrBool ZrLibrary_AotRuntime_SuperArrayAddInt4(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 receiverBaseSlot,
                                               TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *sourceValue;
    SZrTypeValue *receiverValues[4];
    TZrUInt32 index;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4: invalid slot");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (sourceValue == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4: invalid slot value");
        return ZR_FALSE;
    }

    for (index = 0; index < 4; index++) {
        TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverBaseSlot + index);

        if (receiverPointer == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4: invalid receiver slot");
            return ZR_FALSE;
        }

        receiverValues[index] = ZrCore_Stack_GetValue(receiverPointer);
        if (receiverValues[index] == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4: invalid receiver value");
            return ZR_FALSE;
        }
    }

    if (!ZrCore_Object_SuperArrayAddInt4ValuesAssumeFast(
                state, receiverValues, sourceValue->value.nativeObject.nativeInt64)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "SUPER_ARRAY_ADD_INT4: receiver must be an array-like object with int payload");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/** @brief 从当前函数常量表取整数并批量追加。
 * @note constantIndex 是元数据索引，不是立即数；四槽接收者与普通批量入口共享准备及提交契约，失败不宣称四对象全保持原状。
 */
TZrBool ZrLibrary_AotRuntime_SuperArrayAddInt4Const(SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 receiverBaseSlot,
                                                    TZrUInt32 constantIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    const SZrTypeValue *sourceValue;
    SZrTypeValue *receiverValues[4];
    TZrUInt32 index;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || constantIndex >= function->constantValueLength) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4_CONST: invalid constant");
        return ZR_FALSE;
    }

    sourceValue = &function->constantValueList[constantIndex];
    if (!ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4_CONST: constant payload must be int");
        return ZR_FALSE;
    }

    for (index = 0; index < 4; index++) {
        TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverBaseSlot + index);

        if (receiverPointer == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4_CONST: invalid receiver slot");
            return ZR_FALSE;
        }

        receiverValues[index] = ZrCore_Stack_GetValue(receiverPointer);
        if (receiverValues[index] == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4_CONST: invalid receiver value");
            return ZR_FALSE;
        }
    }

    if (!ZrCore_Object_SuperArrayAddInt4ValuesAssumeFast(
                state, receiverValues, sourceValue->value.nativeObject.nativeInt64)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "SUPER_ARRAY_ADD_INT4_CONST: receiver must be an array-like object with int payload");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/** @brief 把常量整数重复追加到四个接收者。
 * @note countSlot 提供有符号次数，非正次数由 core 视为成功空操作；批量追加与容量准备可能部分生效，不是覆盖填充或原子事务。
 */
TZrBool ZrLibrary_AotRuntime_SuperArrayFillInt4Const(SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 receiverBaseSlot,
                                                     TZrUInt32 countSlot,
                                                     TZrUInt32 constantIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    TZrStackValuePointer countPointer;
    SZrTypeValue *countValue;
    const SZrTypeValue *sourceValue;
    SZrTypeValue *receiverValues[4];
    TZrUInt32 index;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    countPointer = aot_runtime_frame_slot(frame, countSlot);
    if (state == ZR_NULL || function == ZR_NULL || countPointer == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_FILL_INT4_CONST: invalid operand");
        return ZR_FALSE;
    }

    countValue = ZrCore_Stack_GetValue(countPointer);
    if (countValue == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(countValue->type)) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_FILL_INT4_CONST: repeat count must be int");
        return ZR_FALSE;
    }

    sourceValue = &function->constantValueList[constantIndex];
    if (!ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_FILL_INT4_CONST: constant payload must be int");
        return ZR_FALSE;
    }

    for (index = 0; index < 4; ++index) {
        TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverBaseSlot + index);

        if (receiverPointer == ZR_NULL ||
            (receiverValues[index] = ZrCore_Stack_GetValue(receiverPointer)) == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_FILL_INT4_CONST: invalid receiver slot");
            return ZR_FALSE;
        }
    }

    if (!ZrCore_Object_SuperArrayFillInt4ValuesAssumeFast(state,
                                                          receiverValues,
                                                          countValue->value.nativeObject.nativeInt64,
                                                          sourceValue->value.nativeObject.nativeInt64)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "SUPER_ARRAY_FILL_INT4_CONST: receiver must be an array-like object with int payload");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/** @brief 从可迭代值创建同步游标。
 * @note 稳定 iterable 副本支持与目的槽重叠；core 选择原型契约回调或数组默认游标，返回游标受目的槽值所有权管理，不是外部资源租约。
 */
TZrBool ZrLibrary_AotRuntime_IterInit(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 iterableSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer iterablePointer = aot_runtime_frame_slot(frame, iterableSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *iterableValue;
    SZrTypeValue stableIterable;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || iterablePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_INIT: invalid slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    iterableValue = ZrCore_Stack_GetValue(iterablePointer);
    if (destinationValue == ZR_NULL || iterableValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_INIT: invalid slot value");
        return ZR_FALSE;
    }

    stableIterable = *iterableValue;
    if (!ZrCore_Object_IterInit(state, &stableIterable, destinationValue)) {
        aot_runtime_fail(state, runtimeState, "ITER_INIT: receiver does not satisfy iterable contract");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 推进游标并把协议结果写入目的槽。
 * @note 函数返回值表示调用是否成功，不表示是否还有元素；默认数组或原型回调可更新游标，返回失败不回滚游标状态。
 */
TZrBool ZrLibrary_AotRuntime_IterMoveNext(SZrState *state,
                                          ZrAotGeneratedFrame *frame,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 iteratorSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer iteratorPointer = aot_runtime_frame_slot(frame, iteratorSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *iteratorValue;
    SZrTypeValue stableIterator;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || iteratorPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_MOVE_NEXT: invalid slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    iteratorValue = ZrCore_Stack_GetValue(iteratorPointer);
    if (destinationValue == ZR_NULL || iteratorValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_MOVE_NEXT: invalid slot value");
        return ZR_FALSE;
    }

    stableIterator = *iteratorValue;
    if (!ZrCore_Object_IterMoveNext(state, &stableIterator, destinationValue)) {
        aot_runtime_fail(state, runtimeState, "ITER_MOVE_NEXT: receiver does not satisfy iterator contract");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 读取同步游标的当前项。
 * @note 按当前成员缓存、协议函数或默认游标隐藏值读取；调用方应遵守成功推进后的协议时序，此桥接不把元素结束当作操作失败。
 */
TZrBool ZrLibrary_AotRuntime_IterCurrent(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 iteratorSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer iteratorPointer = aot_runtime_frame_slot(frame, iteratorSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *iteratorValue;
    SZrTypeValue stableIterator;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || iteratorPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_CURRENT: invalid slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    iteratorValue = ZrCore_Stack_GetValue(iteratorPointer);
    if (destinationValue == ZR_NULL || iteratorValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_CURRENT: invalid slot value");
        return ZR_FALSE;
    }

    stableIterator = *iteratorValue;
    if (!ZrCore_Object_IterCurrent(state, &stableIterator, destinationValue)) {
        aot_runtime_fail(state, runtimeState, "ITER_CURRENT: receiver does not satisfy iterator contract");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 推进同步游标并产生失败条件分支输出。
 * @note 先把非空分支输出置为假；只有成功得到布尔结果才写取反值。非布尔结果可能已推进游标，失败不撤销推进，回边安全点由生成器处理。
 * TODO: 对照 object_call 的结果槽锚点恢复与本函数随后读取的 frame->slotBase，核查用户 moveNext 回调扩栈时同条指令内是否须立即刷新生成帧；下一条指令的刷新不能替代此处复合读取。
 */
TZrBool ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 iteratorSlot,
                                                     TZrBool *outJumpIfFalse) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer;
    SZrTypeValue *destinationValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (outJumpIfFalse == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_MOVE_NEXT_JUMP_IF_FALSE: invalid branch output");
        return ZR_FALSE;
    }

    *outJumpIfFalse = ZR_FALSE;
    if (!ZrLibrary_AotRuntime_IterMoveNext(state, frame, destinationSlot, iteratorSlot)) {
        return ZR_FALSE;
    }

    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_MOVE_NEXT_JUMP_IF_FALSE: invalid destination slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL || !ZR_VALUE_IS_TYPE_BOOL(destinationValue->type)) {
        aot_runtime_fail(state, runtimeState, "ITER_MOVE_NEXT_JUMP_IF_FALSE: iterator branch result is not bool");
        return ZR_FALSE;
    }

    *outJumpIfFalse = !destinationValue->value.nativeObject.nativeBool;
    return ZR_TRUE;
}

/* 当调用绑定不能安全直接跳到已注册 thunk 时，回到 core CallAndRestoreAnchor 的完整动态调用路径。 */
TZrBool ZrLibrary_AotRuntime_Call(SZrState *state,
                                  ZrAotGeneratedFrame *frame,
                                  TZrUInt32 destinationSlot,
                                  TZrUInt32 functionSlot,
                                  TZrUInt32 argumentCount) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    TZrStackValuePointer callBase;
    TZrStackValuePointer destinationPointer;
    SZrFunctionStackAnchor callAnchor;
    SZrFunctionStackAnchor destinationAnchor;
    SZrTypeValue *callableValue;
    TZrUInt32 callableType;
    TZrBool callableIsNative;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;

    if (state == ZR_NULL || frame == ZR_NULL || frame->slotBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CALL: invalid frame");
        return ZR_FALSE;
    }

    callInfo = state->callInfoList;
    callBase = aot_runtime_frame_slot(frame, functionSlot);
    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (callInfo == ZR_NULL || callBase == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "CALL: invalid call target (callInfo=%p callBase=%p destination=%p)",
                         (void *)callInfo,
                         (void *)callBase,
                         (void *)destinationPointer);
        return ZR_FALSE;
    }

    callableValue = ZrCore_Stack_GetValue(callBase);
    if (callableValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CALL: missing callable value");
        return ZR_FALSE;
    }
    if (!aot_prepare_call_binding(state, frame, callableValue)) return ZR_FALSE;
    callableType = callableValue->type;
    callableIsNative = callableValue->isNative;

    ZrCore_Function_StackAnchorInit(state, callBase, &callAnchor);
    ZrCore_Function_StackAnchorInit(state, destinationPointer, &destinationAnchor);
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    callBase = ZrCore_Function_CallAndRestoreAnchor(state, &callAnchor, 1);
    if (callBase == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE || state->callInfoList == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "CALL: generic invoke failed (callBase=%p threadStatus=%u callInfo=%p callableType=%u isNative=%u)",
                         (void *)callBase,
                         (unsigned)state->threadStatus,
                         (void *)state->callInfoList,
                         (unsigned)callableType,
                         (unsigned)callableIsNative);
        return ZR_FALSE;
    }

    destinationPointer = ZrCore_Function_StackAnchorRestore(state, &destinationAnchor);
    if (destinationPointer == ZR_NULL) return ZR_FALSE;
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(destinationPointer), ZrCore_Stack_GetValue(callBase));
    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CallPreparedOrGeneric(SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   ZrAotGeneratedDirectCall *directCall,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 functionSlot,
                                                   TZrUInt32 argumentCount,
                                                   TZrUInt32 resultCount) {
    SZrLibraryAotRuntimeState *runtimeState;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!directCall->prepared) {
        return ZrLibrary_AotRuntime_Call(state, frame, destinationSlot, functionSlot, argumentCount);
    }

    if (directCall->nativeFunction == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT direct call is missing native thunk");
        return ZR_FALSE;
    }

    if (!directCall->nativeFunction(state)) {
        return ZR_FALSE;
    }

    return ZrLibrary_AotRuntime_FinishDirectCall(state, frame, directCall, resultCount);
}

/* 生成器根据 prepared 标志选择 thunk/通用调用，并在异常处理后用 resume 索引恢复控制流。 */
TZrBool ZrLibrary_AotRuntime_CallPreparedOrGenericWithResume(
        SZrState *state,
        ZrAotGeneratedFrame *frame,
        ZrAotGeneratedDirectCall *directCall,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 resultCount,
        TZrUInt32 *outResumeInstructionIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrBool invocationSucceeded;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }
    runtimeState = state != ZR_NULL && state->global != ZR_NULL
                           ? aot_runtime_get_state_from_global(state->global)
                           : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL ||
        outResumeInstructionIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!directCall->prepared) {
        return ZrLibrary_AotRuntime_Call(
                state, frame, destinationSlot, functionSlot, argumentCount);
    }
    if (directCall->nativeFunction == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT direct call is missing native thunk");
        return ZR_FALSE;
    }

    invocationSucceeded = (TZrBool)(directCall->nativeFunction(state) != 0);
    return ZrLibrary_AotRuntime_CompletePreparedDirectCallWithResume(
            state,
            frame,
            directCall,
            invocationSucceeded,
            resultCount,
            outResumeInstructionIndex);
}

/* callee thunk 失败后若 VM 已把异常展开到 caller，则以 caller 的 PC 继续执行 catch/finally。 */
TZrBool ZrLibrary_AotRuntime_CompletePreparedDirectCallWithResume(
        SZrState *state,
        ZrAotGeneratedFrame *frame,
        ZrAotGeneratedDirectCall *directCall,
        TZrBool invocationSucceeded,
        TZrUInt32 resultCount,
        TZrUInt32 *outResumeInstructionIndex) {
    SZrLibraryAotRuntimeState *runtimeState =
            state != ZR_NULL && state->global != ZR_NULL
                    ? aot_runtime_get_state_from_global(state->global)
                    : ZR_NULL;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL ||
        !directCall->prepared || outResumeInstructionIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    if (invocationSucceeded) {
        return ZrLibrary_AotRuntime_FinishDirectCall(
                state, frame, directCall, resultCount);
    }

    /* 失败的 callee 已被 core 展开时，不再 PostCall；仅在活动帧恰为原 caller 时把其 PC 交给生成器 dispatch。 */
    if (state->callInfoList != directCall->calleeCallInfo &&
        state->threadStatus == ZR_THREAD_STATUS_FINE &&
        state->hasCurrentException) {
        if (state->callInfoList != directCall->callerCallInfo) {
            return ZR_FALSE;
        }
        if (!aot_runtime_refresh_frame_from_callinfo(
                    state, frame, state->callInfoList) ||
            !aot_runtime_frame_resume_index(
                    frame, state->callInfoList, outResumeInstructionIndex)) {
            return ZR_FALSE;
        }
        memset(directCall, 0, sizeof(*directCall));
        return ZR_TRUE;
    }

    if (runtimeState == ZR_NULL || runtimeState->lastError[0] == '\0') {
        aot_runtime_fail(
                state,
                runtimeState,
                "generated AOT prepared direct call did not finish or resume "
                "(invoked=%u current=%p caller=%p callee=%p status=%u exception=%u)",
                (unsigned)invocationSucceeded,
                state != ZR_NULL ? (void *)state->callInfoList : ZR_NULL,
                (void *)directCall->callerCallInfo,
                (void *)directCall->calleeCallInfo,
                state != ZR_NULL ? (unsigned)state->threadStatus : 0u,
                state != ZR_NULL ? (unsigned)state->hasCurrentException : 0u);
    }
    return ZR_FALSE;
}

TZrBool ZrLibrary_AotRuntime_PrepareDirectCall(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 functionSlot,
                                               TZrUInt32 argumentCount,
                                               ZrAotGeneratedDirectCall *directCall) {
    return aot_runtime_try_prepare_direct_call(state,
                                               frame,
                                               destinationSlot,
                                               functionSlot,
                                               argumentCount,
                                               directCall);
}

/* @call 先经绑定表或动态元方法解析目标；目标确属已载入 AOT 函数时才准备直接调用。 */
TZrBool ZrLibrary_AotRuntime_PrepareMetaCall(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 receiverSlot,
                                             TZrUInt32 argumentCount,
                                             ZrAotGeneratedDirectCall *directCall) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer callBase;
    SZrFunction *metadataFunction = ZR_NULL;
    SZrLibraryAotLoadedModule *record;
    TZrUInt32 functionIndex;

    if (directCall != ZR_NULL) {
        memset(directCall, 0, sizeof(*directCall));
    }

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callBase = aot_runtime_frame_slot(frame, receiverSlot);
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL || runtimeState == ZR_NULL || callBase == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!aot_runtime_prepare_meta_target(state, frame, callBase, argumentCount, &metadataFunction)) {
        aot_runtime_fail(state, runtimeState, "generated AOT meta call target does not define @call");
        return ZR_FALSE;
    }

    /* Native meta methods are callable raw objects, not bytecode functions. */
    if (metadataFunction == ZR_NULL || metadataFunction->super.isNative) {
        return ZR_TRUE;
    }

    record = aot_runtime_find_record_for_function(runtimeState, metadataFunction);
    if (record == ZR_NULL || record->codeRegistration == ZR_NULL ||
        record->codeRegistration->functionPointers == ZR_NULL) {
        return ZR_TRUE;
    }

    functionIndex = aot_runtime_find_function_index_in_record(record, metadataFunction);
    if (functionIndex == UINT32_MAX || functionIndex >= record->codeRegistration->functionCount ||
        record->codeRegistration->functionPointers[functionIndex] == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!aot_runtime_prepare_vm_direct_call_frame(state,
                                                  frame,
                                                  destinationSlot,
                                                  receiverSlot,
                                                  argumentCount + 1,
                                                  record,
                                                  metadataFunction,
                                                  functionIndex,
                                                  ZR_NULL,
                                                  directCall)) {
        return ZR_FALSE;
    }

    directCall->nativeFunction = record->codeRegistration->functionPointers[functionIndex];
    return ZR_TRUE;
}

/* 静态直调仍核对生成表与运行时函数身份，避免 AOT 序号在描述符/元数据变化后误指。 */
TZrBool ZrLibrary_AotRuntime_PrepareStaticDirectCall(SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 functionSlot,
                                                     TZrUInt32 argumentCount,
                                                     TZrUInt32 calleeFunctionIndex,
                                                     ZrAotGeneratedDirectCall *directCall) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrLibraryAotLoadedModule *record;
    SZrFunction *metadataFunction;
    FZrAotEntryThunk calleeThunk;

    if (directCall != ZR_NULL) {
        memset(directCall, 0, sizeof(*directCall));
    }

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    record = frame != ZR_NULL ? (SZrLibraryAotLoadedModule *)frame->recordHandle : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL || runtimeState == ZR_NULL || record == ZR_NULL ||
        record->codeRegistration == ZR_NULL || record->functionTable == ZR_NULL ||
        record->codeRegistration->functionPointers == ZR_NULL || calleeFunctionIndex >= record->functionCount ||
        calleeFunctionIndex >= record->codeRegistration->functionCount ||
        record->codeRegistration->functionPointers[calleeFunctionIndex] == ZR_NULL) {
        aot_runtime_fail(
                state,
                runtimeState,
                "generated AOT static direct call is missing callee thunk metadata "
                "(record=%p registration=%p table=%p thunks=%p callee=%u functions=%u thunks=%u)",
                (void *)record,
                record != ZR_NULL ? (void *)record->codeRegistration : ZR_NULL,
                record != ZR_NULL ? (void *)record->functionTable : ZR_NULL,
                record != ZR_NULL && record->codeRegistration != ZR_NULL
                        ? (void *)record->codeRegistration->functionPointers
                        : ZR_NULL,
                (unsigned)calleeFunctionIndex,
                record != ZR_NULL ? (unsigned)record->functionCount : 0u,
                record != ZR_NULL && record->codeRegistration != ZR_NULL
                        ? (unsigned)record->codeRegistration->functionCount
                        : 0u);
        return ZR_FALSE;
    }

    metadataFunction = record->functionTable[calleeFunctionIndex];
    if (metadataFunction == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "generated AOT static direct call cannot resolve function index %u",
                         (unsigned)calleeFunctionIndex);
        return ZR_FALSE;
    }
    calleeThunk = record->codeRegistration->functionPointers[calleeFunctionIndex];
    if (!aot_prepare_call_binding(state, frame,
            ZrCore_Stack_GetValue(aot_runtime_frame_slot(frame, functionSlot)))) return ZR_FALSE;
    if (!aot_runtime_static_direct_call_identity_matches(
                frame,
                calleeFunctionIndex,
                metadataFunction,
                calleeThunk)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "generated AOT static direct call identity drift for function index %u",
                         (unsigned)calleeFunctionIndex);
        return ZR_FALSE;
    }

    if (!aot_runtime_prepare_vm_direct_call_frame(state,
                                                  frame,
                                                  destinationSlot,
                                                  functionSlot,
                                                  argumentCount,
                                                  record,
                                                  metadataFunction,
                                                  calleeFunctionIndex,
                                                  calleeThunk,
                                                  directCall)) {
        return ZR_FALSE;
    }

    directCall->nativeFunction = calleeThunk;
    return ZR_TRUE;
}

/* thunk 走完后补齐解释器的返回清理：先关 upvalue 再 PostCall，随后把物理 VALUE 槽
 * 的 owner 转回生成器读取的稠密槽，并释放临时 staged shared/weak 凭据。 */
TZrBool ZrLibrary_AotRuntime_FinishDirectCall(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              ZrAotGeneratedDirectCall *directCall,
                                              TZrUInt32 resultCount) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    TZrUInt32 resultSlot = 0u;
    TZrBool hasGeneratedResultSlot = ZR_FALSE;
    TZrMemoryOffset stagedReturnOffset = 0;
    TZrBool hasCountedStagedReturn = ZR_FALSE;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = directCall != ZR_NULL ? directCall->calleeCallInfo : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL || !directCall->prepared || callInfo == ZR_NULL ||
        directCall->callerCallInfo == ZR_NULL || frame->callInfo != directCall->callerCallInfo ||
        state->callInfoList != callInfo) {
        aot_runtime_fail(state, runtimeState, "generated AOT direct call finish is missing active call info");
        return ZR_FALSE;
    }

    if (resultCount > 0u && callInfo->expectedReturnCount == 1u && callInfo->hasReturnDestination) {
        TZrStackValuePointer callerBase = directCall->callerCallInfo->functionBase.valuePointer + 1;
        if (callInfo->returnDestination >= callerBase &&
            callInfo->returnDestination < callerBase + frame->generatedFrameSlotCount) {
            resultSlot = (TZrUInt32)(callInfo->returnDestination - callerBase);
            hasGeneratedResultSlot = ZR_TRUE;
        }
    }
    if (resultCount > 0u) {
        TZrStackValuePointer source = state->stackTop.valuePointer - resultCount;
        TZrStackValuePointer destination = callInfo->hasReturnDestination
                ? callInfo->returnDestination : callInfo->functionBase.valuePointer;
        SZrTypeValue *value = ZrCore_Stack_GetValue(source);
        if (source != destination &&
            (value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_SHARED ||
             value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_WEAK)) {
            stagedReturnOffset = ZrCore_Stack_SavePointerAsOffset(state, source);
            hasCountedStagedReturn = ZR_TRUE;
        }
    }

    if (state->threadStatus == ZR_THREAD_STATUS_FINE && callInfo->functionBase.valuePointer != ZR_NULL) {
        TZrMemoryOffset returnTop = ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer);
        if (state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
            state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
        }
        // Direct AOT thunk invocation bypasses the interpreter return path, so it
        // must close open upvalues before PostCall tears down the callee frame.
        ZrCore_Closure_CloseClosure(state,
                                    callInfo->functionBase.valuePointer + 1,
                                    ZR_THREAD_STATUS_INVALID,
                                    ZR_FALSE);
        state->stackTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, returnTop);
    }

    ZrCore_Function_PostCall(state, callInfo, resultCount);
    if (state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT direct call lost caller frame");
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;
    /* PostCall 可能把返回 owner 放入物理 VALUE 槽；生成代码读取稠密槽，须先快照再清物理 owner，避免重叠存储破坏值。 */
    if (hasGeneratedResultSlot) {
        const SZrFunctionFrameSlotLayout *layout =
                ZrCore_Function_FindFrameSlotLayout(frame->function, resultSlot);
        SZrStackFramePlace place;
        if (layout != ZR_NULL && layout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&
            layout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue) &&
            ZrCore_Function_MakeFrameSlotPlace(state, frame->function, frame->slotBase, resultSlot, &place)) {
            SZrTypeValue *physicalValue = (SZrTypeValue *)place.address;
            SZrTypeValue *generatedValue = ZrCore_Stack_GetValue(frame->slotBase + resultSlot);
            if (physicalValue != generatedValue &&
                ZrCore_Value_ShouldTransferMaterializedStackOwnership(physicalValue)) {
                /* PostCall consumes the dense owner; generated code consumes the dense slot.
                 * Snapshot first because physical and dense value storage may overlap. */
                SZrTypeValue returnedValue = *physicalValue;
                ZrCore_Value_ResetAsNull(physicalValue);
                *generatedValue = returnedValue;
            }
        }
    }
    /* shared/weak staging 源有独立计数凭据；结果搬移后释放该凭据，并在释放回调后刷新 caller 帧。 */
    if (hasCountedStagedReturn) {
        ZrCore_Ownership_ReleaseValue(state, ZrCore_Stack_GetValue(
                ZrCore_Stack_LoadOffsetToPointer(state, stagedReturnOffset)));
        aot_runtime_refresh_frame_from_callinfo(state, frame, state->callInfoList);
    }
    memset(directCall, 0, sizeof(*directCall));
    return ZR_TRUE;
}

/* 生成异常指令把元数据 handler 映射到 core 的展开栈；EndTry/EndFinally 恢复 pending control。 */
TZrBool ZrLibrary_AotRuntime_Try(SZrState *state, ZrAotGeneratedFrame *frame, TZrUInt32 handlerIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    const SZrFunction *function;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || callInfo == ZR_NULL || function == ZR_NULL || handlerIndex >= function->exceptionHandlerCount) {
        aot_runtime_fail(state, runtimeState, "generated AOT TRY has invalid handler index");
        return ZR_FALSE;
    }

    if (!execution_push_exception_handler(state, callInfo, handlerIndex)) {
        aot_runtime_fail(state, runtimeState, "generated AOT TRY failed to push exception handler");
        return ZR_FALSE;
    }

    frame->callInfo = callInfo;
    if (callInfo->functionBase.valuePointer != ZR_NULL) {
        frame->slotBase = callInfo->functionBase.valuePointer + 1;
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    }
    state->callInfoList = callInfo;

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_EndTry(SZrState *state, ZrAotGeneratedFrame *frame, TZrUInt32 handlerIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrVmExceptionHandlerState *handlerState;
    const SZrFunction *function;
    const SZrFunctionExceptionHandlerInfo *handlerInfo;
    SZrCallInfo *callInfo;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || callInfo == ZR_NULL || function == ZR_NULL || handlerIndex >= function->exceptionHandlerCount) {
        aot_runtime_fail(state, runtimeState, "generated AOT END_TRY has invalid handler index");
        return ZR_FALSE;
    }

    handlerState = execution_find_handler_state(state, callInfo, handlerIndex);
    handlerInfo = &function->exceptionHandlerList[handlerIndex];
    if (handlerState != ZR_NULL) {
        if (handlerInfo->hasFinally) {
            execution_enter_finally(state, handlerState);
        } else {
            execution_pop_exception_handler(state, handlerState);
        }
    }

    frame->callInfo = callInfo;
    if (callInfo->functionBase.valuePointer != ZR_NULL) {
        frame->slotBase = callInfo->functionBase.valuePointer + 1;
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    }
    state->callInfoList = callInfo;

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_Throw(SZrState *state,
                                   ZrAotGeneratedFrame *frame,
                                   TZrUInt32 sourceSlot,
                                   TZrUInt32 *outResumeInstructionIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    TZrStackValuePointer sourcePointer;
    SZrTypeValue payload;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    if (state == ZR_NULL || callInfo == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT THROW has invalid payload slot");
        return ZR_FALSE;
    }

    /* 先把 payload 规范化为 state 当前异常，再清 pending；pending 释放回调可能覆盖原源槽或迁移栈。 */
    payload = *ZrCore_Stack_GetValue(sourcePointer);
    if (!ZrCore_Exception_NormalizeThrownValue(state, &payload, callInfo, ZR_THREAD_STATUS_RUNTIME_ERROR)) {
        if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
            aot_runtime_fail(state, runtimeState, "generated AOT THROW failed to normalize exception");
            return ZR_FALSE;
        }
    }
    execution_clear_pending_control(state);

    if (!execution_unwind_exception_to_handler(state, &callInfo)) {
        ZrCore_Exception_Throw(state, state->currentExceptionStatus);
        return ZR_FALSE;
    }

    if (callInfo != frame->callInfo || !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        return ZR_FALSE;
    }

    return aot_runtime_frame_resume_index(frame, callInfo, outResumeInstructionIndex);
}

TZrBool ZrLibrary_AotRuntime_RequireNonNull(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 sourceSlot,
                                            TZrUInt32 *outResumeInstructionIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    TZrStackValuePointer sourcePointer;
    const SZrTypeValue *sourceValue;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    sourceValue = sourcePointer != ZR_NULL ? ZrCore_Stack_GetValue(sourcePointer) : ZR_NULL;
    if (state == ZR_NULL || callInfo == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT REQUIRE_NON_NULL has invalid source slot");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_NULL(sourceValue->type)) {
        return ZR_TRUE;
    }

    execution_clear_pending_control(state);
    if (!ZrCore_Exception_RaiseNamedRuntimeError(state,
                                                 "NullReferenceError",
                                                 "Direct access through a null receiver",
                                                 callInfo)) {
        if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
            aot_runtime_fail(state, runtimeState, "generated AOT REQUIRE_NON_NULL failed to normalize exception");
            return ZR_FALSE;
        }
    }

    if (!execution_unwind_exception_to_handler(state, &callInfo)) {
        ZrCore_Exception_Throw(state, state->currentExceptionStatus);
        return ZR_FALSE;
    }

    if (callInfo != frame->callInfo || !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        return ZR_FALSE;
    }

    return aot_runtime_frame_resume_index(frame, callInfo, outResumeInstructionIndex);
}

TZrBool ZrLibrary_AotRuntime_IsNull(SZrState *state,
                                    ZrAotGeneratedFrame *frame,
                                    TZrUInt32 sourceSlot,
                                    TZrBool *outIsNull) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer;
    const SZrTypeValue *sourceValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    sourceValue = sourcePointer != ZR_NULL ? ZrCore_Stack_GetValue(sourcePointer) : ZR_NULL;
    if (state == ZR_NULL || sourceValue == ZR_NULL || outIsNull == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT JUMP_IF_NULL has invalid source slot");
        return ZR_FALSE;
    }

    *outIsNull = (TZrBool)ZR_VALUE_IS_TYPE_NULL(sourceValue->type);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_Catch(SZrState *state, ZrAotGeneratedFrame *frame, TZrUInt32 destinationSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer;
    SZrTypeValue *destination;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (state == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT CATCH has invalid destination slot");
        return ZR_FALSE;
    }

    destination = ZrCore_Stack_GetValue(destinationPointer);
    if (destination == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT CATCH is missing destination value");
        return ZR_FALSE;
    }

    if (state->hasCurrentException) {
        ZrCore_Value_Copy(state, destination, &state->currentException);
        ZrCore_Exception_ClearCurrent(state);
    } else {
        ZrCore_Value_ResetAsNull(destination);
    }
    execution_clear_pending_control(state);
    return aot_runtime_refresh_frame_from_callinfo(state, frame,
            frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList);
}

TZrBool ZrLibrary_AotRuntime_EndFinally(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 handlerIndex,
                                        TZrUInt32 *outResumeInstructionIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrVmExceptionHandlerState *handlerState;
    SZrCallInfo *resumeCallInfo;
    TZrStackValuePointer targetSlot;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->callInfo == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT END_FINALLY is missing call frame");
        return ZR_FALSE;
    }

    handlerState = execution_find_handler_state(state, frame->callInfo, handlerIndex);
    if (handlerState != ZR_NULL) {
        execution_finish_finally(state, handlerState);
        if (!aot_runtime_refresh_frame_from_callinfo(state, frame, frame->callInfo)) {
            return ZR_FALSE;
        }
    }

    switch (state->pendingControl.kind) {
        case ZR_VM_PENDING_CONTROL_NONE:
            return ZR_TRUE;
        case ZR_VM_PENDING_CONTROL_EXCEPTION:
            resumeCallInfo = state->pendingControl.callInfo != ZR_NULL ? state->pendingControl.callInfo : frame->callInfo;
            if (!aot_runtime_refresh_frame_from_callinfo(state, frame, resumeCallInfo)) {
                return ZR_FALSE;
            }
            if (!execution_unwind_exception_to_handler(state, &resumeCallInfo)) {
                ZrCore_Exception_Throw(state, state->currentExceptionStatus);
                return ZR_FALSE;
            }
            if (resumeCallInfo != frame->callInfo || !aot_runtime_refresh_frame_from_callinfo(state, frame, resumeCallInfo)) {
                return ZR_FALSE;
            }
            return aot_runtime_frame_resume_index(frame, resumeCallInfo, outResumeInstructionIndex);
        case ZR_VM_PENDING_CONTROL_RETURN:
        case ZR_VM_PENDING_CONTROL_BREAK:
        case ZR_VM_PENDING_CONTROL_CONTINUE:
            resumeCallInfo = state->pendingControl.callInfo != ZR_NULL ? state->pendingControl.callInfo : frame->callInfo;
            if (state->pendingControl.kind == ZR_VM_PENDING_CONTROL_RETURN && state->pendingControl.hasValue &&
                resumeCallInfo != ZR_NULL && resumeCallInfo->functionBase.valuePointer != ZR_NULL) {
                targetSlot = resumeCallInfo->functionBase.valuePointer + 1 + state->pendingControl.valueSlot;
                ZrCore_Value_Copy(state, &targetSlot->value, &state->pendingControl.value);
            }
            if (!aot_runtime_refresh_frame_from_callinfo(state, frame, resumeCallInfo)) {
                return ZR_FALSE;
            }
            if (aot_runtime_resume_pending_control_in_current_frame(state, frame, outResumeInstructionIndex)) {
                return ZR_TRUE;
            }
            return ZR_FALSE;
        default:
            execution_clear_pending_control(state);
            return aot_runtime_refresh_frame_from_callinfo(state, frame, frame->callInfo);
    }
}

TZrBool ZrLibrary_AotRuntime_SetPendingReturn(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 sourceSlot,
                                              TZrUInt32 targetInstructionIndex,
                                              TZrUInt32 *outResumeInstructionIndex) {
    TZrStackValuePointer sourcePointer;
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    if (state == ZR_NULL || frame == ZR_NULL || callInfo == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT SET_PENDING_RETURN has invalid source slot");
        return ZR_FALSE;
    }

    execution_set_pending_control(state,
                                  ZR_VM_PENDING_CONTROL_RETURN,
                                  callInfo,
                                  (TZrMemoryOffset)targetInstructionIndex,
                                  sourceSlot,
                                  ZrCore_Stack_GetValue(sourcePointer));
    return aot_runtime_resume_pending_control_in_current_frame(state, frame, outResumeInstructionIndex);
}

TZrBool ZrLibrary_AotRuntime_SetPendingBreak(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 targetInstructionIndex,
                                             TZrUInt32 *outResumeInstructionIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    if (state == ZR_NULL || frame == ZR_NULL || callInfo == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT SET_PENDING_BREAK is missing call frame");
        return ZR_FALSE;
    }

    execution_set_pending_control(state,
                                  ZR_VM_PENDING_CONTROL_BREAK,
                                  callInfo,
                                  (TZrMemoryOffset)targetInstructionIndex,
                                  0,
                                  ZR_NULL);
    return aot_runtime_resume_pending_control_in_current_frame(state, frame, outResumeInstructionIndex);
}

TZrBool ZrLibrary_AotRuntime_SetPendingContinue(SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 targetInstructionIndex,
                                                TZrUInt32 *outResumeInstructionIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    if (state == ZR_NULL || frame == ZR_NULL || callInfo == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT SET_PENDING_CONTINUE is missing call frame");
        return ZR_FALSE;
    }

    execution_set_pending_control(state,
                                  ZR_VM_PENDING_CONTROL_CONTINUE,
                                  callInfo,
                                  (TZrMemoryOffset)targetInstructionIndex,
                                  0,
                                  ZR_NULL);
    return aot_runtime_resume_pending_control_in_current_frame(state, frame, outResumeInstructionIndex);
}

TZrBool ZrLibrary_AotRuntime_SetConstant(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 sourceSlot,
                                         TZrUInt32 constantIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrFunction *function;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = (SZrFunction *)aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || sourcePointer == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        aot_runtime_fail(state, runtimeState, "SET_CONSTANT: invalid operand");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SET_CONSTANT: missing source value");
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, &function->constantValueList[constantIndex], sourceValue);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GetSubFunction(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 childFunctionIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *ownerFunction;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer functionBase;
    SZrTypeValue *destinationValue;
    SZrTypeValue *functionBaseValue = ZR_NULL;
    SZrClosure *currentClosure = ZR_NULL;
    SZrClosureValue **captureList = ZR_NULL;
    SZrFunction *childFunction;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    ownerFunction = aot_runtime_frame_function(frame);
    functionBase = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo->functionBase.valuePointer : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || ownerFunction == ZR_NULL || destinationPointer == ZR_NULL ||
        frame->slotBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_SUB_FUNCTION: invalid frame");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_SUB_FUNCTION: invalid destination slot");
        return ZR_FALSE;
    }

    if (functionBase != ZR_NULL) {
        functionBaseValue = ZrCore_Stack_GetValue(functionBase);
    }
    if (functionBaseValue != ZR_NULL && functionBaseValue->type == ZR_VALUE_TYPE_CLOSURE &&
        !functionBaseValue->isNative) {
        currentClosure = ZR_CAST_VM_CLOSURE(state, functionBaseValue->value.object);
        captureList = currentClosure != ZR_NULL ? currentClosure->closureValuesExtend : ZR_NULL;
    }

    if (childFunctionIndex >= ownerFunction->childFunctionLength) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    childFunction = &((SZrFunction *)ownerFunction)->childFunctionList[childFunctionIndex];
    ZrCore_Closure_PushToStack(state, childFunction, captureList, frame->slotBase, destinationPointer);
    destinationValue->type = ZR_VALUE_TYPE_CLOSURE;
    destinationValue->isGarbageCollectable = ZR_TRUE;
    destinationValue->isNative = ZR_FALSE;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GetSubFunctionNativeClosure(SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 childFunctionIndex,
                                                         TZrUInt32 callableFlatIndex,
                                                         FZrAotEntryThunk nativeThunk) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *ownerFunction;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;
    SZrFunction *metadataFunction;
    SZrClosureNative *closure;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    ownerFunction = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || frame == ZR_NULL || ownerFunction == ZR_NULL || destinationPointer == ZR_NULL ||
        frame->slotBase == ZR_NULL || nativeThunk == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "GET_SUB_FUNCTION native closure: invalid frame or thunk index %u",
                         (unsigned)callableFlatIndex);
        return ZR_FALSE;
    }

    if (childFunctionIndex >= ownerFunction->childFunctionLength) {
        aot_runtime_fail(state,
                         runtimeState,
                         "GET_SUB_FUNCTION native closure: child index %u out of range",
                         (unsigned)childFunctionIndex);
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_SUB_FUNCTION native closure: invalid destination slot");
        return ZR_FALSE;
    }

    metadataFunction = &ownerFunction->childFunctionList[childFunctionIndex];
    ZrCore_Ownership_ReleaseValue(state, destinationValue);
    closure = ZrCore_ClosureNative_New(state, 0);
    if (closure == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_SUB_FUNCTION native closure: allocation failed");
        return ZR_FALSE;
    }

    closure->nativeFunction = (FZrNativeFunction)nativeThunk;
    closure->aotShimFunction = metadataFunction;
    ZrCore_Value_InitAsRawObject(state, destinationValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    destinationValue->type = ZR_VALUE_TYPE_CLOSURE;
    destinationValue->isGarbageCollectable = ZR_TRUE;
    destinationValue->isNative = ZR_TRUE;
    return ZR_TRUE;
}

/* 离开词法作用域时登记待关闭槽，双表示 owner 由 cleanup registration 桥接。 */
TZrBool ZrLibrary_AotRuntime_MarkToBeClosed(SZrState *state, ZrAotGeneratedFrame *frame, TZrUInt32 slotIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer slotPointer = aot_runtime_frame_slot(frame, slotIndex);
    TZrStackValuePointer registrationPointer = ZR_NULL;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || slotPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MARK_TO_BE_CLOSED: invalid slot");
        return ZR_FALSE;
    }

    if (!aot_runtime_cleanup_registration_prepare(state, frame, slotIndex, &registrationPointer) ||
        registrationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MARK_TO_BE_CLOSED: invalid physical value slot");
        return ZR_FALSE;
    }
    ZrCore_Closure_ToBeClosedValueClosureNew(state, registrationPointer);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_MarkCloseProxy(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 proxySlot,
                                           TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState =
            state != ZR_NULL && state->global != ZR_NULL
                    ? aot_runtime_get_state_from_global(state->global)
                    : ZR_NULL;
    TZrStackValuePointer denseProxy = aot_runtime_frame_slot(frame, proxySlot);
    TZrStackValuePointer denseSource = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer physicalProxy = ZR_NULL;

    if (state == ZR_NULL || denseProxy == ZR_NULL || denseSource == ZR_NULL ||
        proxySlot <= sourceSlot) {
        aot_runtime_fail(state, runtimeState, "MARK_CLOSE_PROXY: invalid logical slots");
        return ZR_FALSE;
    }
    if (!aot_runtime_cleanup_registration_prepare(state, frame, proxySlot,
                                                   &physicalProxy) ||
        physicalProxy == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MARK_CLOSE_PROXY: invalid physical proxy slot");
        return ZR_FALSE;
    }
    if (!ZrCore_Closure_MarkCloseProxy(state, physicalProxy, denseSource)) {
        aot_runtime_fail(state, runtimeState, "MARK_CLOSE_PROXY: registration failed");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CloseScope(SZrState *state, ZrAotGeneratedFrame *frame, TZrUInt32 cleanupCount) {
    ZR_UNUSED_PARAMETER(frame);

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    aot_runtime_close_scope_registrations(state, cleanupCount);
    return ZR_TRUE;
}

/* 执行 TO_BOOL 元转换或共享真值转换并写 BOOL。元方法结果为 BOOL 时保留，否则归一为 true，元调用后重新定位目标槽，无元方法时按共享真值判断。 */
TZrBool ZrLibrary_AotRuntime_ToBool(SZrState *state,
                                    ZrAotGeneratedFrame *frame,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_BOOL: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_BOOL: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, sourceValue, ZR_META_TO_BOOL);
    if (metaValue != ZR_NULL && metaValue->function != ZR_NULL) {
        if (!aot_runtime_invoke_unary_meta(state, frame, destinationSlot, sourceValue, metaValue->function)) {
            return ZR_FALSE;
        }
        destinationValue =
                aot_runtime_refresh_destination_after_meta(state, frame, destinationSlot, runtimeState, "TO_BOOL");
        if (destinationValue == ZR_NULL) {
            return ZR_FALSE;
        }
        if (destinationValue != ZR_NULL && ZR_VALUE_IS_TYPE_BOOL(destinationValue->type)) {
            return ZR_TRUE;
        }
        ZR_VALUE_FAST_SET(destinationValue, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
        return ZR_TRUE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      aot_runtime_value_is_truthy(state, sourceValue) ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

/* TODO: 核查 无元转换时 NaN、无穷大或超 int64 范围的浮点强转 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

/* 执行 TO_INT 元转换或原生值转换。元方法结果为任意整数标签时保留，否则写 signed 0，无元方法时整数原样复制，float 强转 int64，bool 转 0/1，其他类型写 0。 */
TZrBool ZrLibrary_AotRuntime_ToInt(SZrState *state,
                                   ZrAotGeneratedFrame *frame,
                                   TZrUInt32 destinationSlot,
                                   TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *sourceValue;
    SZrTypeValue *destinationValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_INT: invalid stack slot");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (sourceValue == ZR_NULL || destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_INT: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, sourceValue, ZR_META_TO_INT);
    if (metaValue != ZR_NULL && metaValue->function != ZR_NULL) {
        if (!aot_runtime_invoke_unary_meta(state, frame, destinationSlot, sourceValue, metaValue->function)) {
            return ZR_FALSE;
        }
        destinationValue =
                aot_runtime_refresh_destination_after_meta(state, frame, destinationSlot, runtimeState, "TO_INT");
        if (destinationValue == ZR_NULL) {
            return ZR_FALSE;
        }
        if (destinationValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(destinationValue->type)) {
            return ZR_TRUE;
        }
        ZrCore_Value_InitAsInt(state, destinationValue, 0);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_INT(sourceValue->type)) {
        ZrCore_Value_Copy(state, destinationValue, sourceValue);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZrCore_Value_InitAsInt(state, destinationValue, (TZrInt64)sourceValue->value.nativeObject.nativeUInt64);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZrCore_Value_InitAsInt(state, destinationValue, (TZrInt64)sourceValue->value.nativeObject.nativeDouble);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        ZrCore_Value_InitAsInt(state, destinationValue, sourceValue->value.nativeObject.nativeBool ? 1 : 0);
    } else {
        ZrCore_Value_InitAsInt(state, destinationValue, 0);
    }
    return ZR_TRUE;
}

/* TODO: 核查 无元转换时 NaN、截断后仍为负（如 -1.0）或超 uint64 范围的浮点强转；-0.5 截断为零不属该边界 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

/* 执行 TO_UINT 元转换或原生 unsigned 转换。元方法结果为任意整数标签时保留，否则写 unsigned 0，无元方法时 unsigned 复制，signed/float 转 uint64，bool 转 0/1，其他类型写 0。 */
TZrBool ZrLibrary_AotRuntime_ToUInt(SZrState *state,
                                    ZrAotGeneratedFrame *frame,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *sourceValue;
    SZrTypeValue *destinationValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_UINT: invalid stack slot");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (sourceValue == ZR_NULL || destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_UINT: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, sourceValue, ZR_META_TO_UINT);
    if (metaValue != ZR_NULL && metaValue->function != ZR_NULL) {
        if (!aot_runtime_invoke_unary_meta(state, frame, destinationSlot, sourceValue, metaValue->function)) {
            return ZR_FALSE;
        }
        destinationValue =
                aot_runtime_refresh_destination_after_meta(state, frame, destinationSlot, runtimeState, "TO_UINT");
        if (destinationValue == ZR_NULL) {
            return ZR_FALSE;
        }
        if (destinationValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(destinationValue->type)) {
            return ZR_TRUE;
        }
        ZrCore_Value_InitAsUInt(state, destinationValue, 0);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZrCore_Value_Copy(state, destinationValue, sourceValue);
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZrCore_Value_InitAsUInt(state, destinationValue, (TZrUInt64)sourceValue->value.nativeObject.nativeInt64);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZrCore_Value_InitAsUInt(state, destinationValue, (TZrUInt64)sourceValue->value.nativeObject.nativeDouble);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        ZrCore_Value_InitAsUInt(state, destinationValue, sourceValue->value.nativeObject.nativeBool ? 1u : 0u);
    } else {
        ZrCore_Value_InitAsUInt(state, destinationValue, 0u);
    }
    return ZR_TRUE;
}

/* 执行 TO_FLOAT 元转换或原生 double 转换。元方法结果为 float 类时保留，否则写 0.0，无元方法时 float 复制，整数/bool 转 double，其余写 0.0，元调用后刷新目标槽。 */
TZrBool ZrLibrary_AotRuntime_ToFloat(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 destinationSlot,
                                     TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *sourceValue;
    SZrTypeValue *destinationValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_FLOAT: invalid stack slot");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (sourceValue == ZR_NULL || destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_FLOAT: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, sourceValue, ZR_META_TO_FLOAT);
    if (metaValue != ZR_NULL && metaValue->function != ZR_NULL) {
        if (!aot_runtime_invoke_unary_meta(state, frame, destinationSlot, sourceValue, metaValue->function)) {
            return ZR_FALSE;
        }
        destinationValue =
                aot_runtime_refresh_destination_after_meta(state, frame, destinationSlot, runtimeState, "TO_FLOAT");
        if (destinationValue == ZR_NULL) {
            return ZR_FALSE;
        }
        if (destinationValue != ZR_NULL && ZR_VALUE_IS_TYPE_FLOAT(destinationValue->type)) {
            return ZR_TRUE;
        }
        ZrCore_Value_InitAsFloat(state, destinationValue, 0.0);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZrCore_Value_Copy(state, destinationValue, sourceValue);
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZrCore_Value_InitAsFloat(state, destinationValue, (TZrFloat64)sourceValue->value.nativeObject.nativeInt64);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZrCore_Value_InitAsFloat(state, destinationValue, (TZrFloat64)sourceValue->value.nativeObject.nativeUInt64);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        ZrCore_Value_InitAsFloat(state, destinationValue, sourceValue->value.nativeObject.nativeBool ? 1.0 : 0.0);
    } else {
        ZrCore_Value_InitAsFloat(state, destinationValue, 0.0);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_AddFloat(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 leftSlot,
                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_ADD,
                                                    "ADD_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_SubFloat(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 leftSlot,
                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_SUB,
                                                    "SUB_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_MulUnsigned(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MUL_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MUL_UNSIGNED: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type) &&
        aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) &&
        aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        ZrCore_Value_InitAsUInt(state, destinationValue, leftUInt * rightUInt);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "MUL_UNSIGNED requires numeric operands");
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsFloat(state, destinationValue, leftDouble * rightDouble);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_MulFloat(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 leftSlot,
                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_MUL,
                                                    "MUL_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_DivUnsigned(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "DIV_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "DIV_UNSIGNED: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type) &&
        aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) &&
        aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        if (rightUInt == 0u) {
            ZrCore_Debug_RunError(state, "divide by zero");
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsUInt(state, destinationValue, leftUInt / rightUInt);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "DIV_UNSIGNED requires numeric operands");
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsFloat(state, destinationValue, leftDouble / rightDouble);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_DivFloat(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 leftSlot,
                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_DIV,
                                                    "DIV_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_ModUnsigned(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MOD_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MOD_UNSIGNED: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type) &&
        aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) &&
        aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        if (rightUInt == 0u) {
            ZrCore_Debug_RunError(state, "modulo by zero");
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsUInt(state, destinationValue, leftUInt % rightUInt);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "MOD_UNSIGNED requires numeric operands");
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsFloat(state, destinationValue, fmod(leftDouble, rightDouble));
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ModFloat(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 leftSlot,
                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_MOD,
                                                    "MOD_FLOAT");
}

/* 为泛型幂调用左操作数元方法。只按对应 meta 分派，不自行执行原生数值计算，元方法缺失时成功写 null，元调用由临时调用帧承载。 */
TZrBool ZrLibrary_AotRuntime_Pow(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 leftSlot,
                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "POW: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "POW: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_POW);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}

TZrBool ZrLibrary_AotRuntime_PowSigned(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 leftSlot,
                                       TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "POW_SIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "POW_SIGNED: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type) &&
        aot_runtime_extract_integer_like_value(leftValue, &leftInt) &&
        aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        if ((leftInt == 0 && rightInt <= 0) || leftInt < 0) {
            ZrCore_Debug_RunError(state, "power domain error");
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsInt(state, destinationValue, ZrCore_Math_IntPower(leftInt, rightInt));
        return ZR_TRUE;
    }

    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_POW,
                                                    "POW_SIGNED");
}

TZrBool ZrLibrary_AotRuntime_PowUnsigned(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "POW_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "POW_UNSIGNED: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type) &&
        aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) &&
        aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        if (leftUInt == 0u && rightUInt == 0u) {
            ZrCore_Debug_RunError(state, "power domain error");
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsUInt(state, destinationValue, ZrCore_Math_UIntPower(leftUInt, rightUInt));
        return ZR_TRUE;
    }

    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_POW,
                                                    "POW_UNSIGNED");
}

TZrBool ZrLibrary_AotRuntime_PowFloat(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 leftSlot,
                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_POW,
                                                    "POW_FLOAT");
}

/* 为泛型左移调用左操作数元方法。只按对应 meta 分派，不自行执行原生数值计算，元方法缺失时成功写 null，元调用由临时调用帧承载。 */
TZrBool ZrLibrary_AotRuntime_ShiftLeft(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 leftSlot,
                                       TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_LEFT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_LEFT: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_SHIFT_LEFT);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}

/* TODO: 核查 整数左移的位移量越界、负操作数或结果超 int64 范围 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_ShiftLeftInt(SZrState *state,
                                          ZrAotGeneratedFrame *frame,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_LEFT_INT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_LEFT_INT: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
        !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        aot_runtime_fail(state, runtimeState, "SHIFT_LEFT_INT: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, leftInt << rightInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

/* 为泛型右移调用左操作数元方法。只按对应 meta 分派，不自行执行原生数值计算，元方法缺失时成功写 null，元调用由临时调用帧承载。 */
TZrBool ZrLibrary_AotRuntime_ShiftRight(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_RIGHT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_RIGHT: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_SHIFT_RIGHT);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}

/* TODO: 核查 整数右移的位移量不属于 [0,63] 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_ShiftRightInt(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_RIGHT_INT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_RIGHT_INT: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
        !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        aot_runtime_fail(state, runtimeState, "SHIFT_RIGHT_INT: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, leftInt >> rightInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalNot(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_NOT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_NOT: missing value");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      aot_runtime_value_is_truthy(state, sourceValue) ? ZR_FALSE : ZR_TRUE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalAnd(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_AND: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_AND: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_BOOL(leftValue->type) || !ZR_VALUE_IS_TYPE_BOOL(rightValue->type)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_AND: operands must be bool");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftValue->value.nativeObject.nativeBool && rightValue->value.nativeObject.nativeBool,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalOr(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 leftSlot,
                                       TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_OR: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_OR: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_BOOL(leftValue->type) || !ZR_VALUE_IS_TYPE_BOOL(rightValue->type)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_OR: operands must be bool");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftValue->value.nativeObject.nativeBool || rightValue->value.nativeObject.nativeBool,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalGreaterUnsigned(SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_UNSIGNED: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) ||
        !aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_UNSIGNED: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftUInt > rightUInt ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalGreaterFloat(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_compare_operation(state,
                                                     frame,
                                                     destinationSlot,
                                                     leftSlot,
                                                     rightSlot,
                                                     ZR_AOT_RUNTIME_COMPARE_GREATER,
                                                     "LOGICAL_GREATER_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_LogicalLessUnsigned(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_UNSIGNED: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) ||
        !aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_UNSIGNED: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftUInt < rightUInt ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalLessFloat(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_compare_operation(state,
                                                     frame,
                                                     destinationSlot,
                                                     leftSlot,
                                                     rightSlot,
                                                     ZR_AOT_RUNTIME_COMPARE_LESS,
                                                     "LOGICAL_LESS_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_LogicalGreaterEqualUnsigned(SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_UNSIGNED: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) ||
        !aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_UNSIGNED: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftUInt >= rightUInt ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalGreaterEqualFloat(SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_compare_operation(state,
                                                     frame,
                                                     destinationSlot,
                                                     leftSlot,
                                                     rightSlot,
                                                     ZR_AOT_RUNTIME_COMPARE_GREATER_EQUAL,
                                                     "LOGICAL_GREATER_EQUAL_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_LogicalLessEqualUnsigned(SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_UNSIGNED: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) ||
        !aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_UNSIGNED: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftUInt <= rightUInt ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalLessEqualFloat(SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_compare_operation(state,
                                                     frame,
                                                     destinationSlot,
                                                     leftSlot,
                                                     rightSlot,
                                                     ZR_AOT_RUNTIME_COMPARE_LESS_EQUAL,
                                                     "LOGICAL_LESS_EQUAL_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_BitwiseNot(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;
    TZrInt64 sourceInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_NOT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_NOT: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(sourceValue->type) || !aot_runtime_extract_integer_like_value(sourceValue, &sourceInt)) {
        aot_runtime_fail(state, runtimeState, "BITWISE_NOT: operand must be integer");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, ~sourceInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_BitwiseAnd(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_AND: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_AND: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
        !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        aot_runtime_fail(state, runtimeState, "BITWISE_AND: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, leftInt & rightInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_BitwiseOr(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 leftSlot,
                                       TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_OR: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_OR: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
        !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        aot_runtime_fail(state, runtimeState, "BITWISE_OR: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, leftInt | rightInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

/* TODO: 核查 有符号左移的位移量越界、负操作数或结果超 int64 范围 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_BitwiseShiftLeft(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_SHIFT_LEFT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_SHIFT_LEFT: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
        !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        aot_runtime_fail(state, runtimeState, "BITWISE_SHIFT_LEFT: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, leftInt << rightInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

/* TODO: 核查 uint64 逻辑右移的位移量不属于 [0,63] 的输入边界：沿合法编译输入、当前 LLVM opcode 选择及实际发射到此入口，确认 frame/tag/slot 或常量来源能否满足极值组合，并定义失败或数值契约；当前原始 C 运算未给出对应门禁，但尚无完整合法触发链或实测证明。 */

TZrBool ZrLibrary_AotRuntime_BitwiseShiftRight(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_SHIFT_RIGHT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_SHIFT_RIGHT: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) ||
        !aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        aot_runtime_fail(state, runtimeState, "BITWISE_SHIFT_RIGHT: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, (TZrInt64)(leftUInt >> rightUInt), ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

/* 入口 thunk 在返回前把生成模块导出发布到项目记录所拥有的 module 对象。 */
/* TODO: 核查合法嵌套 AOT 导入在第 5/9 个记录追加时跨 records 扩容，并在返回后继续消费 frame/context/activeRecord 的完整调用链；若 realloc 迁移数组，旧元素借用地址会失效，须验证实际可达路径及后续重定位责任，尚无完整合法触发或实测证明。 */

TZrBool ZrLibrary_AotRuntime_PublishModuleExports(SZrState *state, ZrAotGeneratedFrame *frame) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    SZrLibraryAotLoadedModule *record;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    record = frame != ZR_NULL ? (SZrLibraryAotLoadedModule *)frame->recordHandle : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (runtimeState != ZR_NULL &&
        runtimeState->activeRecord != ZR_NULL &&
        aot_runtime_find_function_index_in_record(runtimeState->activeRecord, function) != UINT32_MAX) {
        record = runtimeState->activeRecord;
    }
    if (record == ZR_NULL) {
        record = aot_runtime_find_record_for_function(runtimeState, function);
    }
    if (record == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT export publication failed");
        return ZR_FALSE;
    }
    if (record->moduleExecuted) {
        return ZR_TRUE;
    }

    return aot_runtime_materialize_exports(state, record, frame->slotBase);
}

/* 普通返回先关闭作用域和 upvalue，再从当前 callInfo 重新取得返回位置以容忍清理回调扩栈。 */
TZrInt64 ZrLibrary_AotRuntime_Return(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 sourceSlot,
                                     TZrBool publishExports) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    const SZrFunction *metadataFunction;
    TZrStackValuePointer sourcePointer;
    SZrTypeValue *resultValue;
    SZrTypeValue *callerResultValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    metadataFunction = aot_runtime_frame_function(frame);
    sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL ||
        metadataFunction == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT return failed");
        return 0;
    }

    resultValue = ZrCore_Stack_GetValue(sourcePointer);
    callerResultValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (resultValue == ZR_NULL || callerResultValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT return failed");
        return 0;
    }
    if (publishExports && !ZrLibrary_AotRuntime_PublishModuleExports(state, frame)) {
        return 0;
    }

    execution_discard_exception_handlers_for_callinfo(state, callInfo);
    aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo);
    resultValue = ZrCore_Stack_GetValue(aot_runtime_frame_slot(frame, sourceSlot));
    if (callInfo->functionTop.valuePointer != ZR_NULL &&
        (state->stackTop.valuePointer == ZR_NULL || state->stackTop.valuePointer < callInfo->functionTop.valuePointer)) {
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    }
    ZrCore_Function_ApplyReturnEscape(state, metadataFunction, sourceSlot, resultValue);
    ZrCore_Closure_CloseClosure(state,
                                callInfo->functionBase.valuePointer + 1,
                                ZR_THREAD_STATUS_INVALID,
                                ZR_FALSE);

    aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo);
    resultValue = ZrCore_Stack_GetValue(aot_runtime_frame_slot(frame, sourceSlot));
    callerResultValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    ZrCore_Function_TryCopyInlineConstructorReceiverBack(state, callInfo);
    if (metadataFunction->functionName == ZR_NULL ||
        ZrCore_NativeString_Compare(ZrCore_String_GetNativeString(metadataFunction->functionName), "constructor") != 0) {
        ZrCore_Value_Copy(state, callerResultValue, resultValue);
    }
    state->stackTop.valuePointer = callInfo->functionBase.valuePointer + 1;
    return 1;
}

TZrInt64 ZrLibrary_AotRuntime_ReportUnsupportedInstruction(SZrState *state,
                                                           TZrUInt32 functionIndex,
                                                           TZrUInt32 instructionIndex,
                                                           TZrUInt32 opcode) {
    SZrLibraryAotRuntimeState *runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;

    aot_runtime_fail(state,
                     runtimeState,
                     "unsupported generated AOT instruction: functionIndex=%u instructionIndex=%u opcode=%u",
                     (unsigned)functionIndex,
                     (unsigned)instructionIndex,
                     (unsigned)opcode);
    return 0;
}

TZrInt64 ZrLibrary_AotRuntime_FailGeneratedFunctionAt(
        SZrState *state,
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 functionIndex) {
    SZrLibraryAotRuntimeState *runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    TZrUInt32 instructionIndex =
            frame != ZR_NULL ? frame->currentInstructionIndex : ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;

    /* 异常已离开该生成帧时直接返回失败标志，避免再补 runtime error 覆盖上层 handler 正在处理的异常。 */
    if (state != ZR_NULL && frame != ZR_NULL && state->hasCurrentException &&
        state->callInfoList != ZR_NULL && frame->callInfo != ZR_NULL &&
        state->callInfoList != frame->callInfo) {
        return 0;
    }

    if (runtimeState != ZR_NULL && runtimeState->lastError[0] != '\0') {
        return 0;
    }

    if (functionIndex == UINT32_MAX) {
        aot_runtime_fail(state, runtimeState, "generated AOT function failed before frame initialization");
        return 0;
    }

    aot_runtime_fail(state,
                     runtimeState,
                     "generated AOT function failed: functionIndex=%u instructionIndex=%u",
                     (unsigned)functionIndex,
                     instructionIndex == ZR_AOT_RUNTIME_RESUME_FALLTHROUGH ? UINT32_MAX : (unsigned)instructionIndex);
    return 0;
}

TZrInt64 ZrLibrary_AotRuntime_FailGeneratedFunction(SZrState *state, const ZrAotGeneratedFrame *frame) {
    return ZrLibrary_AotRuntime_FailGeneratedFunctionAt(
            state,
            frame,
            frame != ZR_NULL ? frame->functionIndex : UINT32_MAX);
}

/* core native-call ABI 进入 AOT：从正在执行的项目记录选择后端匹配的入口 thunk。 */
/* TODO: activeRecord 指向可扩容 records 元素；若外部旧生成码仍调用此 ABI 并嵌套导入，
 * 读取 backendKind 等字段可能失效。当前生成器未见正向调用，需核实外部可达性。 */
TZrInt64 ZrLibrary_AotRuntime_InvokeActiveShim(SZrState *state, EZrAotBackendKind backendKind) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrLibraryAotLoadedModule *record;
    TZrStackValuePointer resultBase = ZR_NULL;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return 0;
    }

    runtimeState = aot_runtime_get_state_from_global(state->global);
    if (runtimeState == ZR_NULL || runtimeState->activeRecord == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "AOT entry thunk invoked without an active record");
        return 0;
    }

    record = runtimeState->activeRecord;
    if (record->backendKind != backendKind) {
        aot_runtime_fail(state, runtimeState, "AOT backend mismatch for module '%s'", record->moduleName);
        return 0;
    }

    aot_runtime_mark_record_executed(runtimeState, record);
    if (!aot_runtime_execute_vm_shim_direct(state, record->moduleFunction, &resultBase)) {
        return 0;
    }

    if (!record->moduleExecuted) {
        aot_runtime_materialize_exports(state, record, resultBase + 1);
    }
    return 1;
}

/* closure native-call ABI 使用活动记录和当前 callInfo 的元数据函数进入 VM shim。 */
/* TODO: 此入口依赖可扩容 records 内的 activeRecord；若外部旧生成码仍调用并嵌套导入，
 * 后续 record 字段可能失效。当前生成器未见正向调用，需核实外部可达性。 */
TZrInt64 ZrLibrary_AotRuntime_InvokeCurrentClosureShim(SZrState *state, EZrAotBackendKind backendKind) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrLibraryAotLoadedModule *record;
    SZrCallInfo *callInfo;
    SZrFunction *shimFunction;
    TZrStackValuePointer sourceBase;
    TZrStackValuePointer callBase;
    TZrStackValuePointer resultBase;
    SZrClosure *closure;
    SZrTypeValue *closureValue;
    SZrTypeValue *currentClosureValue;
    TZrSize argumentCount;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return 0;
    }

    runtimeState = aot_runtime_get_state_from_global(state->global);
    if (runtimeState == ZR_NULL || runtimeState->activeRecord == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "AOT closure shim invoked without an active record");
        return 0;
    }

    record = runtimeState->activeRecord;
    if (record->backendKind != backendKind) {
        aot_runtime_fail(state, runtimeState, "AOT backend mismatch for module '%s'", record->moduleName);
        return 0;
    }

    callInfo = state->callInfoList;
    shimFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
    if (callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL || shimFunction == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT closure shim is missing metadata function for module '%s'",
                         record->moduleName != ZR_NULL ? record->moduleName : "<unknown>");
        return 0;
    }

    sourceBase = callInfo->functionBase.valuePointer;
    argumentCount = (TZrSize)(state->stackTop.valuePointer - (sourceBase + 1));
    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_ReserveScratchSlots(state, argumentCount + 1, callBase);
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "AOT closure shim failed to reserve call slots");
        return 0;
    }
    sourceBase = callInfo->functionBase.valuePointer;

    currentClosureValue = ZrCore_Stack_GetValue(sourceBase);
    if (!aot_runtime_project_closure_into_vm_shim(state, currentClosureValue, shimFunction, &closure)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT closure shim failed to project captures for module '%s'",
                         record->moduleName != ZR_NULL ? record->moduleName : "<unknown>");
        return 0;
    }

    closureValue = ZrCore_Stack_GetValue(callBase);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;

    for (TZrSize index = 0; index < argumentCount; index++) {
        ZrCore_Stack_CopyValue(state, callBase + 1 + index, ZrCore_Stack_GetValue(sourceBase + 1 + index));
    }

    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    aot_runtime_mark_record_executed(runtimeState, record);
    resultBase = ZrCore_Function_CallAndRestore(state, callBase, 1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE || resultBase == ZR_NULL) {
        return 0;
    }

    return 1;
}
