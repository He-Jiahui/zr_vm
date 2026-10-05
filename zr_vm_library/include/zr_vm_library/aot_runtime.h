#ifndef ZR_VM_LIBRARY_AOT_RUNTIME_H
#define ZR_VM_LIBRARY_AOT_RUNTIME_H

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/state.h"
#include "zr_vm_library/conf.h"

struct SZrGlobalState;
struct SZrObjectModule;
struct SZrLibrary_Project;
struct SZrFunction;
struct SZrMetadataRuntime;
struct SZrString;
struct SZrTypeValue;

/** @brief 生成器给 BeginInstruction 的观察边界标志；NONE 不主动发布，MAY_THROW、CONTROL_FLOW、CALL、RETURN 按策略发布 PC 和调试事件。 */
typedef enum EZrAotGeneratedStepFlag {
    ZR_AOT_GENERATED_STEP_FLAG_NONE = 0,
    ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW = 1u << 0,
    ZR_AOT_GENERATED_STEP_FLAG_CONTROL_FLOW = 1u << 1,
    ZR_AOT_GENERATED_STEP_FLAG_CALL = 1u << 2,
    ZR_AOT_GENERATED_STEP_FLAG_RETURN = 1u << 3
} EZrAotGeneratedStepFlag;

/** @brief 调用恢复协议的顺序执行哨兵；生成 dispatch 不得把它当真实指令索引。 */
#define ZR_AOT_RUNTIME_RESUME_FALLTHROUGH ((TZrUInt32)0xFFFFFFFFu)

/** @brief 一次生成函数调用的物理帧视图，供值槽、观察与恢复 helper 使用。
 * @note 函数表、模块和 recordHandle 借用项目记录；仅在本次调用帧存活期间使用。
 * 调用或扩栈后须按生成协议重取 slotBase；保存 frame 结构本身不建立 GC 根或固定记录地址。
 */
typedef struct ZrAotGeneratedFrame {
    /** @brief 借用项目记录；记录数组迁移和项目析构可使地址失效。 */
    TZrPtr recordHandle;
    /** @brief 当前生成函数的 VM 元数据，供槽、常量、PC 与异常恢复校验。 */
    struct SZrFunction *function;
    /** @brief 当前物理调用帧，扩栈后通过栈锚点或运行时刷新。 */
    struct SZrCallInfo *callInfo;
    /** @brief 当前稠密值槽借用视图，调用或扩栈后须重取。 */
    TZrStackValuePointer slotBase;
    /** @brief 注册表的 flat function 索引，须匹配当前 closure 元数据。 */
    TZrUInt32 functionIndex;
    /** @brief 生成 dispatch 当前索引，BeginInstruction 在此发布观察状态。 */
    TZrUInt32 currentInstructionIndex;
    /** @brief 最近已发布的观察索引，供避免重复调试通知。 */
    TZrUInt32 lastObservedInstructionIndex;
    /** @brief 最近已发布源码行，供行钩子去重。 */
    TZrUInt32 lastObservedLine;
    /** @brief 此帧建立时保存的指令观察策略。 */
    TZrUInt32 observationMask;
    /** @brief 此函数生成物理槽数量，用于槽边界检查与栈预留。 */
    TZrUInt32 generatedFrameSlotCount;
    /** @brief 此帧的全指令观察开关，行调试可强制开启。 */
    TZrBool publishAllInstructions;
    /** @brief 借用已装载模块对象，项目记录持有其 GC pin。 */
    struct SZrObjectModule *module;
    /** @brief 借用记录完成位；模块成功发布 exports 后才置真。 */
    TZrBool *moduleExecuted;
    /** @brief 借用与 thunk 同索引的 VM 函数表。 */
    struct SZrFunction **functionTable;
    /** @brief 函数表有效条目数，不是分配容量。 */
    TZrUInt32 functionCount;
    /** @brief 借用动态库静态 ABI 注册数据，供布局、绑定及 import 查询。 */
    const SZrAotCodeRegistration *codeRegistration;
    /** @brief 借用动态库生成入口数组，按 flat 索引调用。 */
    const FZrAotEntryThunk *functionThunks;
    /** @brief thunk 数组边界，必须与 VM 函数身份一起校验。 */
    TZrUInt32 functionThunkCount;
} ZrAotGeneratedFrame;

/** @brief 生成序言校验当前 closure 身份后借用的模块上下文。
 * @note 此视图不创建 callee 帧、取得 owner 或 pin；项目和动态库必须覆盖消费期间。
 */
typedef struct ZrAotGeneratedModuleContext {
    /** @brief 借用项目记录；记录数组迁移和项目析构可使地址失效。 */
    TZrPtr recordHandle;
    /** @brief 当前 closure 对应的 VM 元数据函数。 */
    struct SZrFunction *metadataFunction;
    /** @brief 借用注册表中对应 methodInfo，供生成器读取类型与调用信息。 */
    const SZrAotMethodInfo *methodInfo;
    /** @brief 借用已装载模块对象，项目记录持有其 GC pin。 */
    struct SZrObjectModule *module;
    /** @brief 借用记录完成位；模块成功发布 exports 后才置真。 */
    TZrBool *moduleExecuted;
    /** @brief 借用与 thunk 同索引的 VM 函数表。 */
    struct SZrFunction **functionTable;
    /** @brief 函数表有效条目数，不是分配容量。 */
    TZrUInt32 functionCount;
    /** @brief 借用动态库静态 ABI 注册数据，供布局、绑定及 import 查询。 */
    const SZrAotCodeRegistration *codeRegistration;
    /** @brief 借用动态库生成入口数组，按 flat 索引调用。 */
    const FZrAotEntryThunk *functionThunks;
    /** @brief thunk 数组边界，必须与 VM 函数身份一起校验。 */
    TZrUInt32 functionThunkCount;
    /** @brief 经 closure 身份验证后的 flat 索引。 */
    TZrUInt32 resolvedFunctionIndex;
    /** @brief 此函数生成物理槽数量，用于槽边界检查与栈预留。 */
    TZrUInt32 generatedFrameSlotCount;
} ZrAotGeneratedModuleContext;

/** @brief Prepare/Complete 两阶段之间的 callee 与 caller 凭据。
 * @note prepared 为真才可执行 nativeFunction 并完成调用；完成入口要求仍处该 callee 帧。
 * 未准备的通用回退不得消费其余字段；此凭据不允许跨异步调度或已结束的调用保存。
 */
typedef struct ZrAotGeneratedDirectCall {
    /** @brief prepared 为真时可调用的 callee thunk。 */
    FZrAotEntryThunk nativeFunction;
    /** @brief 准备调用时的 caller 帧凭据，完成入口据此恢复。 */
    struct SZrCallInfo *callerCallInfo;
    /** @brief 准备调用建立的 callee 帧，完成时必须仍为活动帧。 */
    struct SZrCallInfo *calleeCallInfo;
    /** @brief 准备时 caller 的 flat 索引，防跨函数误用凭据。 */
    TZrUInt32 callerFunctionIndex;
    /** @brief 绑定确定的 callee flat 索引。 */
    TZrUInt32 calleeFunctionIndex;
    /** @brief 发起调用的生成指令索引，供异常和观察协议使用。 */
    TZrUInt32 callInstructionIndex;
    /** @brief 调用后恢复索引，fallthrough 哨兵表示顺序执行。 */
    TZrUInt32 resumeInstructionIndex;
    /** @brief 准备时保存的 caller 观察掩码，完成时恢复。 */
    TZrUInt32 observationMaskSnapshot;
    /** @brief 准备时保存的 caller 全指令观察开关。 */
    TZrBool publishAllInstructionsSnapshot;
    /** @brief 有效凭据开关；false 时其余字段不可用于完成调用。 */
    TZrBool prepared;
} ZrAotGeneratedDirectCall;

/** @brief 宿主请求模式；AOT 模式安装本 loader，非 AOT 模式撤销它；不是成功执行后端证明。 */
typedef enum EZrLibraryProjectExecutionMode {
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_INTERP = 0,
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_BINARY = 1,
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C = 2,
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM = 3
} EZrLibraryProjectExecutionMode;

/** @brief 实际后端报告枚举；当前本实现只记录 NONE 或 AOT 后端，INTERP/BINARY 仍用于名称转换和 ABI 枚举。 */
typedef enum EZrLibraryExecutedVia {
    ZR_LIBRARY_EXECUTED_VIA_NONE = 0,
    ZR_LIBRARY_EXECUTED_VIA_INTERP = 1,
    ZR_LIBRARY_EXECUTED_VIA_BINARY = 2,
    ZR_LIBRARY_EXECUTED_VIA_AOT_C = 3,
    ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM = 4
} EZrLibraryExecutedVia;

/** @brief 在项目首次执行前建立 AOT 缓存，并按模式安装 core 模块装载回调。
 * @note global->userData 必须关联有效项目；AOT_C/AOT_LLVM 决定严格模式，requireAotPath 当前只保存未参与判断；再次配置会重置诊断、执行后端及活动记录。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ConfigureGlobal(struct SZrGlobalState *global,
                                                            EZrLibraryProjectExecutionMode executionMode,
                                                            TZrBool requireAotPath);

/** @brief 沿项目析构链撤销函数和模块 pin，并将库句柄延迟到 global GC 后关闭。
 * @note state 必須仍属于该项目 global；项目记录和借用的 frame/context 随调用失效；必须在 global GC 与分配器销毁前调用。
 */
ZR_LIBRARY_API void ZrLibrary_AotRuntime_FreeProjectState(struct SZrState *state,
                                                          struct SZrLibrary_Project *project);

/** @brief 将执行后端枚举转换成宿主可记录的稳定名称。
 * @note 返回静态借用文本；未知枚举与 NONE 均按未执行处理。
 */
ZR_LIBRARY_API const TZrChar *ZrLibrary_AotRuntime_ExecutedViaName(EZrLibraryExecutedVia executedVia);

/** @brief 读取项目最近记录的执行后端。
 * @note 无 global、项目或 AOT 状态返回 NONE；它记录已走过的后端，不证明整次执行成功。
 */
ZR_LIBRARY_API EZrLibraryExecutedVia ZrLibrary_AotRuntime_GetExecutedVia(struct SZrGlobalState *global);

/** @brief 借用项目最近的 AOT 诊断字符串。
 * @note 未配置或诊断为空时返回 null；配置、探测和后续失败均可覆盖缓冲，不转移所有权。
 */
ZR_LIBRARY_API const TZrChar *ZrLibrary_AotRuntime_GetLastError(struct SZrGlobalState *global);

/** @brief 保存生成代码准备报告的错误文本。
 * @note 未配置 AOT 状态时无操作；本入口不改 threadStatus、不抛出、不关闭作用域，生成调用方负责后续失败或异常协议。
 */
ZR_LIBRARY_API void ZrLibrary_AotRuntime_RecordError(struct SZrState *state, TZrNativeString message);

/** @brief 按注册表的函数区间及局部序号借用 native import 契约。
 * @note 区间、索引或契约表无效返回 null；不解析符号、不取得 FFI 调用租约。
 */
ZR_LIBRARY_API const struct SZrNativeImportContract *
ZrLibrary_AotRuntime_ResolveNativeImportContract(
        const SZrAotCodeRegistration *codeRegistration,
        TZrUInt32 functionIndex,
        TZrUInt32 localContractIndex);

/** @brief 为 FFI 从 VM 元数据函数定位所属 AOT 记录并借用 import 契约。
 * @note 需同 global 的已装载记录；找不到函数或区间返回 null；指针寿命受项目记录和动态库限制。
 */
ZR_LIBRARY_API const struct SZrNativeImportContract *
ZrLibrary_AotRuntime_FindNativeImportContract(
        struct SZrState *state,
        const struct SZrFunction *function,
        TZrUInt32 localContractIndex);

/** @brief 由 core 导入链装载并至多成功执行一次 AOT 模块。
 * @note ConfigureGlobal 注册本回调；userData 必须是同项目 AOT 状态；返回模块借用项目 pin；失败返回 null，core 还按 threadStatus 决定是否继续其他 loader。
 */
ZR_LIBRARY_API struct SZrObjectModule *ZrLibrary_AotRuntime_ModuleLoader(struct SZrState *state,
                                                                         struct SZrString *moduleName,
                                                                         TZrPtr userData);

/** @brief 装载项目根模块并在 TryRun 异常边界调用入口 thunk。
 * @note result 非空且归调用方；装载失败发生在结果置 null 之前，执行失败不承诺结果或副作用回滚；返回 true 后结果仍须按 VM Value 生命周期管理。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ExecuteEntry(struct SZrState *state,
                                                         EZrAotBackendKind backendKind,
                                                         struct SZrTypeValue *result);

/** @brief 从活动模块进入 VM shim 并按需发布导出。
 * @note 仓内当前生成器未正向调用；依赖活动记录与 backendKind 一致。
 * TODO: 核对仓外旧生成产物是否仍消费此 ABI，并对照当前 C/LLVM descriptor entry/thunk emit；验证活动记录、backendKind、元函数及 capture 投影前提，只能同步借用，不得跨未来 callback 保存。
 */
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_InvokeActiveShim(struct SZrState *state,
                                                              EZrAotBackendKind backendKind);
/** @brief 投影当前 native closure captures 到 VM shim 后调用。
 * @note 仓内当前生成器未正向调用；activeRecord 与当前 metadataFunction 必需。
 * TODO: 核对仓外旧生成产物是否仍消费此 ABI，并对照当前 C/LLVM descriptor entry/thunk emit；验证活动记录、backendKind、元函数及 capture 投影前提，只能同步借用，不得跨未来 callback 保存。
 */
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_InvokeCurrentClosureShim(struct SZrState *state,
                                                                      EZrAotBackendKind backendKind);

/** @brief 为 LLVM 生成序言建立帧与生成槽视图。
 * @note 当前 callInfo 必须可解析到匹配 AOT 记录；扩栈前锚定基址和 return destination。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BeginGeneratedFunction(struct SZrState *state,
                                                                   TZrUInt32 functionIndex,
                                                                   ZrAotGeneratedFrame *frame);

/** @brief 按正在执行的 closure 校验 flat index 并借用模块上下文。
 * @note context 不分配帧；索引必须与当前 callInfo 元数据匹配。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(struct SZrState *state,
                                                                          TZrUInt32 functionIndex,
                                                                          ZrAotGeneratedModuleContext *context);

/** @brief 给生成序言提供可抛错、控制流、调用和返回的默认观察掩码。
 * @note 没有专属观察覆盖时使用；掩码描述发布边界，不注册 GC 根或异常处理器。
 */
ZR_FORCE_INLINE TZrUInt32 ZrLibrary_AotRuntime_DefaultObservationMask(void) {
    return ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW |
           ZR_AOT_GENERATED_STEP_FLAG_CONTROL_FLOW |
           ZR_AOT_GENERATED_STEP_FLAG_CALL |
           ZR_AOT_GENERATED_STEP_FLAG_RETURN;
}

/** @brief 为此 state 保存生成指令观察覆盖策略。
 * @note 仅影响此 state；已建立帧保存原策略快照。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetObservationPolicy(struct SZrState *state,
                                                                 TZrUInt32 observationMask,
                                                                 TZrBool publishAllInstructions);

/** @brief 撤销 state 的观察覆盖以恢复默认策略。
 * @note 行调试信号仍可强制全指令发布。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ResetObservationPolicy(struct SZrState *state);

/** @brief 读取当前有效观察策略供测试与调试消费。
 * @note 两个输出指针都必需；结果包含行钩子强制策略。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetObservationPolicy(struct SZrState *state,
                                                                 TZrUInt32 *outObservationMask,
                                                                 TZrBool *outPublishAllInstructions);

/** @brief 刷新帧并按策略发布当前 PC 与行钩子。
 * @note instructionIndex 来自当前函数生成器；跳过发布仍更新 currentInstructionIndex。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BeginInstruction(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 instructionIndex,
                                                             TZrUInt32 stepFlags);

/** @brief 将函数常量物化到生成槽供值指令使用。
 * @note 函数常量可转成 callable；constantIndex 必须属于当前函数。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CopyConstant(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 constantIndex);

/** @brief 把生成槽值复制回当前函数常量列表。
 * @note constantIndex 校验当前函数；不替换列表存储。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetConstant(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 sourceSlot,
                                                        TZrUInt32 constantIndex);

/** @brief 按常量函数与当前捕获上下文建立 callable。
 * @note 不同于 CopyConstant，需要当前 frame 的捕获环境。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CreateClosure(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 constantIndex);

/** @brief 将当前 closure 的 capture 复制到生成槽。
 * @note closureIndex 按当前 closure capture 数量校验。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetClosureValue(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 closureIndex);

/** @brief 更新当前 capture 并向真实 capture owner 做 GC 写屏障。
 * @note native capture owner 或 VM closureValue 承担屏障。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetClosureValue(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 sourceSlot,
                                                            TZrUInt32 closureIndex);

/** @brief 把源物理槽赋给目的槽，复用生成器的值移动或内联布局协议。
 * @note 内联结构要求同一布局并由帧复制；普通值走 assign 协议，不能把它当 GetStack 的保源副本。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CopyStack(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 sourceSlot);

/** @brief 为 preserveSource lowering 复制源值到目的槽。
 * @note 普通值走 Value_Copy；内联结构要求兼容布局；返回 false 时不承诺整次赋值回滚，槽和 frame 必须仍有效。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetStack(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot);

/** @brief 释放一个生成槽的旧值并恢复 null。
 * @note 按帧物理槽处理；释放可运行清理逻辑，调用方须遵守值寿命和生成帧有效期。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ResetStackNull(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 destinationSlot);

/** @brief 校验两个生成槽后依次释放并清空旧值。
 * @note 先校验两槽，再按给定顺序覆写；释放可运行清理逻辑，不承诺异常时已经完成的副作用回滚。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ResetStackNull2(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 firstSlot,
                                                            TZrUInt32 secondSlot);

/** @brief 将 primitive 泛型槽写为布尔值。
 * @note 这是 aot_runtime_values 中的直接转换入口，不调用元转换协议；仅接受实现列出的 primitive 类型，浮点转整数和目标覆写约束仍见实现 TODO/BUG。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ConvertGenericToBool(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 sourceSlot);

/** @brief 将 primitive 泛型槽写为有符号整数值。
 * @note 这是 aot_runtime_values 中的直接转换入口，不调用元转换协议；仅接受实现列出的 primitive 类型，浮点转整数和目标覆写约束仍见实现 TODO/BUG。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ConvertGenericToInt(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 sourceSlot);

/** @brief 将 primitive 泛型槽写为无符号整数值。
 * @note 这是 aot_runtime_values 中的直接转换入口，不调用元转换协议；仅接受实现列出的 primitive 类型，浮点转整数和目标覆写约束仍见实现 TODO/BUG。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ConvertGenericToUInt(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 sourceSlot);

/** @brief 将 primitive 泛型槽写为浮点值。
 * @note 这是 aot_runtime_values 中的直接转换入口，不调用元转换协议；仅接受实现列出的 primitive 类型，浮点转整数和目标覆写约束仍见实现 TODO/BUG。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ConvertGenericToFloat(struct SZrState *state,
                                                                  ZrAotGeneratedFrame *frame,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 sourceSlot);

/** @brief 执行 C lowering 的 primitive 数值加法。
 * @note 两源必须为数值类型；浮点参与时按浮点算，纯整数按当前 signed/unsigned 分支；不提供元运算回退，失败无目的槽保持保证。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericNumericAdd(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

/** @brief 执行 C lowering 的 primitive 数值减法。
 * @note 两源必须为数值类型；浮点参与时按浮点算，纯整数按当前 signed/unsigned 分支；不提供元运算回退，失败无目的槽保持保证。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericNumericSub(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

/** @brief 执行 C lowering 的 primitive 数值乘法。
 * @note 两源必须为数值类型；浮点参与时按浮点算，纯整数按当前 signed/unsigned 分支；不提供元运算回退，失败无目的槽保持保证。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericNumericMul(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

/** @brief 执行 C lowering 的 primitive 数值除法。
 * @note 两源必须为数值类型；浮点参与时按浮点算，纯整数按当前 signed/unsigned 分支；不提供元运算回退，失败无目的槽保持保证。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericNumericDiv(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

/** @brief 执行 C lowering 的 primitive 数值取模。
 * @note 两源必须为数值类型；浮点参与时按浮点算，纯整数按当前 signed/unsigned 分支；不提供元运算回退，失败无目的槽保持保证。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericNumericMod(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

/** @brief 对 primitive 数值槽取负。
 * @note 只接受数值类型，不调用元方法；整数最小值及 unsigned narrowing 的现有边界须按实现约束处理。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericNumericNeg(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 sourceSlot);

/** @brief 为 C lowering 的未支持 POW 退路返回明确状态。
 * @note 当前没有数值计算或元调用实现；没有 POW 元方法时返回 true 并写 null，有元方法时报告错误并返回 false，不能以 true 推断已算出数值。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericPower(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot);

/** @brief 从物理槽同步生成器的signed 整数局部缓存。
 * @note outValue 必须非空；槽有效即返回 true，类型不匹配会保留原缓存；false 表示参数或帧槽无效，不表示类型转换失败。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SyncSignedIntLocal(struct SZrState *state,
                                                               ZrAotGeneratedFrame *frame,
                                                               TZrUInt32 sourceSlot,
                                                               TZrInt64 *outValue);

/** @brief 从物理槽同步生成器的unsigned 整数局部缓存。
 * @note outValue 必须非空；槽有效即返回 true，类型不匹配会保留原缓存；false 表示参数或帧槽无效，不表示类型转换失败。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SyncUnsignedIntLocal(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 sourceSlot,
                                                                 TZrUInt64 *outValue);

/** @brief 从物理槽同步生成器的浮点局部缓存。
 * @note outValue 必须非空；槽有效即返回 true，类型不匹配会保留原缓存；false 表示参数或帧槽无效，不表示类型转换失败。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SyncFloatLocal(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 sourceSlot,
                                                           TZrFloat64 *outValue);

/** @brief 从物理槽同步生成器的布尔局部缓存。
 * @note outValue 必须非空；槽有效即返回 true，类型不匹配会保留原缓存；false 表示参数或帧槽无效，不表示类型转换失败。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SyncBoolLocal(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 sourceSlot,
                                                          TZrBool *outValue);

/** @brief 把 global zrObject 放入生成表达式槽。
 * @note 无有效 OBJECT 全局值时写 null 并返回 true。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetGlobal(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot);

/** @brief 按 child 索引创建 VM closure 并关联当前 captures。
 * @note 索引越界写 null 并成功；native caller 不读取 VM captureList。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetSubFunction(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 childFunctionIndex);

/** @brief 给无 VM closure 路径建立关联 child 元数据的 native thunk closure。
 * @note nativeThunk 必需；创建零 capture closure，callableFlatIndex 用于诊断。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetSubFunctionNativeClosure(struct SZrState *state,
                                                                        ZrAotGeneratedFrame *frame,
                                                                        TZrUInt32 destinationSlot,
                                                                        TZrUInt32 childFunctionIndex,
                                                                        TZrUInt32 callableFlatIndex,
                                                                        FZrAotEntryThunk nativeThunk);

/** @brief 为生成 CREATE_OBJECT 分配并发布对象值。
 * @note 分配返回 null 时目标写 null；布尔 true 不证明对象分配成功。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CreateObject(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot);

/** @brief 为生成 CREATE_ARRAY 分配并发布数组值。
 * @note 分配返回 null 时目标写 null；数组通过 internalType 标识。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CreateArray(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot);

/** @brief 按当前函数类型布局创建内联元素数组。
 * @note 同时建立稠密与分离物理 VALUE 表示；布局或分配失败返回 false。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CreateInlineArray(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 elementTypeLayoutId,
                                                              TZrUInt32 length);

/** @brief 把目标帧槽绑定为内联数组元素位置。
 * @note 索引须能表示为 int64；core 校验元素和位置布局。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BindInlineArrayElementPlace(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 arraySlot,
        TZrUInt32 indexSlot);

/** @brief 向目标槽物化源值的运行时类型对象。
 * @note 委托 Reflection_TypeOfValue，失败写 AOT 诊断。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_TypeOf(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 sourceSlot);

/** @brief 通过 typed bridge 对指定类型名称执行装箱。
 * @note typeNameConstantIndex 属于当前函数；bridge 使用当前 callInfo。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToObject(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot,
                                                     TZrUInt32 typeNameConstantIndex);

/** @brief 通过 typed bridge 对指定类型名称执行拆箱。
 * @note typeNameConstantIndex 属于当前函数；源与目标必须是有效生成槽。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToStruct(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot,
                                                     TZrUInt32 typeNameConstantIndex);

/** @brief 执行实例 getter 的已绑定 SUPER 缓存。
 * @note 要求 META_GET cache kind 和实例 accessor 模式。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MetaGetCached(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 receiverSlot,
                                                          TZrUInt32 cacheIndex);

/** @brief 按成员符号调用 getter 并得到表达式值。
 * @note 采用通用 InvokeMember；memberId 不是 call-site cache index。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MetaGet(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 memberId);

/** @brief 执行实例 setter 的已绑定 SUPER 缓存。
 * @note 要求 META_SET cache kind；表达式结果是 assigned value。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MetaSetCached(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 receiverAndResultSlot,
                                                          TZrUInt32 assignedValueSlot,
                                                          TZrUInt32 cacheIndex);

/** @brief 按成员符号调用 setter 并使表达式结果成为赋值值。
 * @note InvokeMember 结果忽略，成功后复制 stableAssignedValue。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MetaSet(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 receiverAndResultSlot,
                                                    TZrUInt32 assignedValueSlot,
                                                    TZrUInt32 memberId);

/** @brief 执行静态 getter 的已绑定 SUPER 缓存。
 * @note 要求 META_GET_STATIC；不把 receiver 作为参数。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MetaGetStaticCached(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 receiverSlot,
                                                                TZrUInt32 cacheIndex);

/** @brief 执行静态 setter 的已绑定 SUPER 缓存。
 * @note 要求 META_SET_STATIC；仅传 assigned value。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MetaSetStaticCached(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 receiverAndResultSlot,
                                                                TZrUInt32 assignedValueSlot,
                                                                TZrUInt32 cacheIndex);

/** @brief 为 OWN_UNIQUE 调用 core unique 转换并同步清理注册。
 * @note 可移动直接 unique；其他对象须为 ownership NONE；转换拒绝映射为 null。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnUnique(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 sourceSlot);

/** @brief 为 OWN_BORROW/VIEW_SHARED 建立借用并同步清理注册。
 * @note 借用不获得独立释放所有权；底层拒绝映射为 null。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnBorrow(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 sourceSlot);

/** @brief 为 OWN_LOAN/VIEW_MUT 移交 loan 并同步清理注册。
 * @note 源目标不能同槽，非 null 源须 unique；null 源按 core 空值路径成功；底层拒绝映射为 null。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnLoan(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 sourceSlot);

/** @brief 将 loan 交回 unique 并同步清理注册。
 * @note 源目标不能同槽，非 null 源须 loaned；null 源按 core 空值路径成功；底层拒绝映射为 null。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnReturnLoan(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 sourceSlot);

/** @brief 把 unique 转为 shared 并同步清理注册。
 * @note 非 null 源须满足 core unique 门禁，null 源走成功空值路径；core 负责建控制块和 root 后清空源；底层拒绝映射为 null。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnShare(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot);

/** @brief 从 shared 保留 weak 凭据并同步清理注册。
 * @note 非 null 源须 shared；null 源按 core 空值路径成功；底层拒绝映射为 null。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnDegrade(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 sourceSlot);

/** @brief 执行 OWN_DETACH 的 GC box 或归还 GC 转换。
 * @note 直接 unique 优先 IntoGcBox，其余尝试 DetachValue。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnDetach(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 sourceSlot);

/** @brief 把直接 unique 资源转换为 GC box。
 * @note core 校验资源所属 state；转换拒绝映射为 null。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnIntoGcBox(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 sourceSlot);

/** @brief 把控制块 ownership 交还 GC。
 * @note 调用 DetachValue/ReturnToGcValue；转换拒绝映射为 null。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnReturnToGc(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 sourceSlot);

/** @brief 尝试把 weak 提升为 shared。
 * @note 资源已失效时 core 正常写 null；true 不保证得到活对象。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnWake(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot);

/** @brief 释放源所有权并清空目标，同步作用域清理注册。
 * @note 释放回调后重取帧地址；同槽只清理一次 registration。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnDrop(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot);

/**
 * @brief 把通用值相等结果写入布尔目标槽。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 依赖 core 值相等规则：类型标签必须一致，字符串比较内容，GC 对象比较身份；不调用相等元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalEqual(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot);

/**
 * @brief 把通用值相等取反结果写入布尔目标槽。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 依赖 core 值相等规则：类型标签必须一致，字符串比较内容，GC 对象比较身份；不调用相等元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNotEqual(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 rightSlot);

/** @brief 比较 primitive 泛型值并把相等结果写入布尔槽。
 * @note 先比较类型标签，异类型直接判定；同类型仅支持 null、bool 和数值，其他同类型返回 false；不调用相等元方法。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(struct SZrState *state,
                                                                         ZrAotGeneratedFrame *frame,
                                                                         TZrUInt32 destinationSlot,
                                                                         TZrUInt32 leftSlot,
                                                                         TZrUInt32 rightSlot);

/** @brief 比较 primitive 泛型值并把不等结果写入布尔槽。
 * @note 先比较类型标签，异类型直接判定；同类型仅支持 null、bool 和数值，其他同类型返回 false；不调用相等元方法。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual(struct SZrState *state,
                                                                            ZrAotGeneratedFrame *frame,
                                                                            TZrUInt32 destinationSlot,
                                                                            TZrUInt32 leftSlot,
                                                                            TZrUInt32 rightSlot);

/**
 * @brief 执行BOOL专用相等比较并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须属于BOOL；直接比较原生载荷；类型不匹配返回 false，不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalEqualBool(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 rightSlot);

/**
 * @brief 执行BOOL专用相等取反比较并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须属于BOOL；直接比较原生载荷；类型不匹配返回 false，不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNotEqualBool(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 leftSlot,
                                                                TZrUInt32 rightSlot);

/**
 * @brief 执行有符号整数类专用相等比较并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须属于有符号整数类；直接比较原生载荷；类型不匹配返回 false，不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalEqualSigned(struct SZrState *state,
                                                               ZrAotGeneratedFrame *frame,
                                                               TZrUInt32 destinationSlot,
                                                               TZrUInt32 leftSlot,
                                                               TZrUInt32 rightSlot);

/**
 * @brief 执行有符号整数类专用相等取反比较并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须属于有符号整数类；直接比较原生载荷；类型不匹配返回 false，不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNotEqualSigned(struct SZrState *state,
                                                                  ZrAotGeneratedFrame *frame,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 leftSlot,
                                                                  TZrUInt32 rightSlot);

/**
 * @brief 执行无符号整数类专用相等比较并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须属于无符号整数类；直接比较原生载荷；类型不匹配返回 false，不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalEqualUnsigned(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 leftSlot,
                                                                 TZrUInt32 rightSlot);

/**
 * @brief 执行无符号整数类专用相等取反比较并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须属于无符号整数类；直接比较原生载荷；类型不匹配返回 false，不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNotEqualUnsigned(struct SZrState *state,
                                                                    ZrAotGeneratedFrame *frame,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 leftSlot,
                                                                    TZrUInt32 rightSlot);

/**
 * @brief 执行浮点类专用相等比较并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须属于浮点类；直接比较原生载荷；类型不匹配返回 false，不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalEqualFloat(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

/**
 * @brief 执行浮点类专用相等取反比较并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须属于浮点类；直接比较原生载荷；类型不匹配返回 false，不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNotEqualFloat(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 leftSlot,
                                                                 TZrUInt32 rightSlot);

/**
 * @brief 执行非空 STRING 对象专用相等比较并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须属于非空 STRING 对象；比较字符串内容；类型不匹配返回 false，不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalEqualString(struct SZrState *state,
                                                               ZrAotGeneratedFrame *frame,
                                                               TZrUInt32 destinationSlot,
                                                               TZrUInt32 leftSlot,
                                                               TZrUInt32 rightSlot);

/**
 * @brief 执行非空 STRING 对象专用相等取反比较并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须属于非空 STRING 对象；比较字符串内容；类型不匹配返回 false，不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNotEqualString(struct SZrState *state,
                                                                  ZrAotGeneratedFrame *frame,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 leftSlot,
                                                                  TZrUInt32 rightSlot);

/**
 * @brief 为 signed 关系 opcode 写入 BOOL 比较结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note unsigned 先转 int64；只接受 signed/unsigned，不接受 bool；槽或类型失败记录运行时错误。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalLessSigned(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 rightSlot);

/**
 * @brief 为 unsigned 关系 opcode 写入 BOOL 比较结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源必须是 signed/unsigned 整数标签；signed 转 uint64 后比较；bool 和 float 拒绝；不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalLessUnsigned(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 leftSlot,
                                                                TZrUInt32 rightSlot);

/**
 * @brief 为浮点关系 opcode 提升两源并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 源可为原生数值或 bool；经 double 比较，整数可能舍入；不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalLessFloat(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 rightSlot);

/**
 * @brief 为 signed 关系 opcode 写入 BOOL 比较结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note unsigned 先转 int64；接受 signed/unsigned/bool 整数类；槽或类型失败记录运行时错误。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalGreaterSigned(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 leftSlot,
                                                                 TZrUInt32 rightSlot);

/**
 * @brief 为 unsigned 关系 opcode 写入 BOOL 比较结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源必须是 signed/unsigned 整数标签；signed 转 uint64 后比较；bool 和 float 拒绝；不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalGreaterUnsigned(struct SZrState *state,
                                                                   ZrAotGeneratedFrame *frame,
                                                                   TZrUInt32 destinationSlot,
                                                                   TZrUInt32 leftSlot,
                                                                   TZrUInt32 rightSlot);

/**
 * @brief 为浮点关系 opcode 提升两源并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 源可为原生数值或 bool；经 double 比较，整数可能舍入；不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalGreaterFloat(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 leftSlot,
                                                                TZrUInt32 rightSlot);

/**
 * @brief 为 signed 关系 opcode 写入 BOOL 比较结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note unsigned 先转 int64；接受 signed/unsigned/bool 整数类；槽或类型失败记录运行时错误。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalLessEqualSigned(struct SZrState *state,
                                                                   ZrAotGeneratedFrame *frame,
                                                                   TZrUInt32 destinationSlot,
                                                                   TZrUInt32 leftSlot,
                                                                   TZrUInt32 rightSlot);

/**
 * @brief 为 unsigned 关系 opcode 写入 BOOL 比较结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源必须是 signed/unsigned 整数标签；signed 转 uint64 后比较；bool 和 float 拒绝；不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalLessEqualUnsigned(struct SZrState *state,
                                                                     ZrAotGeneratedFrame *frame,
                                                                     TZrUInt32 destinationSlot,
                                                                     TZrUInt32 leftSlot,
                                                                     TZrUInt32 rightSlot);

/**
 * @brief 为浮点关系 opcode 提升两源并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 源可为原生数值或 bool；经 double 比较，整数可能舍入；不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalLessEqualFloat(struct SZrState *state,
                                                                  ZrAotGeneratedFrame *frame,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 leftSlot,
                                                                  TZrUInt32 rightSlot);

/**
 * @brief 为 signed 关系 opcode 写入 BOOL 比较结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note unsigned 先转 int64；接受 signed/unsigned/bool 整数类；槽或类型失败记录运行时错误。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalGreaterEqualSigned(struct SZrState *state,
                                                                      ZrAotGeneratedFrame *frame,
                                                                      TZrUInt32 destinationSlot,
                                                                      TZrUInt32 leftSlot,
                                                                      TZrUInt32 rightSlot);

/**
 * @brief 为 unsigned 关系 opcode 写入 BOOL 比较结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源必须是 signed/unsigned 整数标签；signed 转 uint64 后比较；bool 和 float 拒绝；不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalGreaterEqualUnsigned(struct SZrState *state,
                                                                        ZrAotGeneratedFrame *frame,
                                                                        TZrUInt32 destinationSlot,
                                                                        TZrUInt32 leftSlot,
                                                                        TZrUInt32 rightSlot);

/**
 * @brief 为浮点关系 opcode 提升两源并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 源可为原生数值或 bool；经 double 比较，整数可能舍入；不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalGreaterEqualFloat(struct SZrState *state,
                                                                     ZrAotGeneratedFrame *frame,
                                                                     TZrUInt32 destinationSlot,
                                                                     TZrUInt32 leftSlot,
                                                                     TZrUInt32 rightSlot);

/**
 * @brief 向条件跳转返回值槽真值。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 返回值是成功状态；outTruthy 非空；null、数值零、空字符串为假，其余按共享真值规则；不执行 TO_BOOL 元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_IsTruthy(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 sourceSlot,
                                                     TZrBool *outTruthy);

/** @brief 把 primitive 真值写给生成分支的 outTruthy。
 * @note outTruthy 必需，失败前置 false；仅支持 null、bool、整数和浮点，不调用对象真值元方法。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(struct SZrState *state,
                                                                     ZrAotGeneratedFrame *frame,
                                                                     TZrUInt32 sourceSlot,
                                                                     TZrBool *outTruthy);

/** @brief 将 primitive 真值的逻辑非写入目标槽。
 * @note 源须支持 primitive 真值协议；不调用对象元方法，目的槽覆写受实现现有生命周期约束。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(struct SZrState *state,
                                                                       ZrAotGeneratedFrame *frame,
                                                                       TZrUInt32 destinationSlot,
                                                                       TZrUInt32 sourceSlot);

/**
 * @brief 向生成分支返回比较谓词，供调用者决定目标标签。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 返回值是操作成功状态；outShouldJump 是跳转条件；整数类接受 signed、unsigned、bool；右参数为帧槽索引。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShouldJumpIfGreaterSigned(struct SZrState *state,
                                                                      ZrAotGeneratedFrame *frame,
                                                                      TZrUInt32 leftSlot,
                                                                      TZrUInt32 rightSlot,
                                                                      TZrBool *outShouldJump);
/**
 * @brief 向生成分支返回比较谓词，供调用者决定目标标签。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 返回值是操作成功状态；outShouldJump 是跳转条件；整数类接受 signed、unsigned、bool；右参数为帧槽索引。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShouldJumpIfLessEqualSigned(struct SZrState *state,
                                                                        ZrAotGeneratedFrame *frame,
                                                                        TZrUInt32 leftSlot,
                                                                        TZrUInt32 rightSlot,
                                                                        TZrBool *outShouldJump);
/**
 * @brief 向生成分支返回比较谓词，供调用者决定目标标签。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 返回值是操作成功状态；outShouldJump 是跳转条件；整数类接受 signed、unsigned、bool；右参数为帧槽索引。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShouldJumpIfNotEqualSigned(struct SZrState *state,
                                                                       ZrAotGeneratedFrame *frame,
                                                                       TZrUInt32 leftSlot,
                                                                       TZrUInt32 rightSlot,
                                                                       TZrBool *outShouldJump);
/**
 * @brief 向生成分支返回比较谓词，供调用者决定目标标签。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 返回值是操作成功状态；outShouldJump 是跳转条件；整数类接受 signed、unsigned、bool；右参数为函数常量索引。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShouldJumpIfNotEqualSignedConst(struct SZrState *state,
                                                                            ZrAotGeneratedFrame *frame,
                                                                            TZrUInt32 leftSlot,
                                                                            TZrUInt32 constantIndex,
                                                                            TZrBool *outShouldJump);

/**
 * @brief 复用解释器 ADD 执行边界，让生成代码保留动态加法行为。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 需要有效 callInfo；委托 ZrCore_Execution_Add，可发生元调用及扩栈；成功后按当前 callInfo 重定位 frame 和 stackTop。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Add(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot);

/**
 * @brief 以 double 完成加法专用 opcode。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 接受原生 signed/unsigned/float/bool；输出 DOUBLE；不查元方法，不添加零除或定义域门禁。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddFloat(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);

/**
 * @brief 按值槽类型执行泛型减法并保留左操作数元方法退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两 bool 使用现有布尔合取路径；同 signed、同 unsigned、同 float 直接运算；其他类型查 ZR_META_SUB，缺失时成功写 null。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Sub(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot);

/**
 * @brief 以 double 完成减法专用 opcode。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 接受原生 signed/unsigned/float/bool；输出 DOUBLE；不查元方法，不添加零除或定义域门禁。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubFloat(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);

/**
 * @brief 按值槽数值类型执行乘法并保留左操作数元方法退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 同 signed/unsigned 保留对应数值结果；混合整数类（含 bool）用 int64；其余可提取数值用 double；无元方法时成功写 null。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Mul(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot);

/**
 * @brief 执行 unsigned 乘法值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两整数标签转 uint64，结果写 UINT64；其余可提取原生数值/bool 经 double，结果写 DOUBLE；不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MulUnsigned(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot);

/**
 * @brief 以 double 完成乘法专用 opcode。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 接受原生 signed/unsigned/float/bool；输出 DOUBLE；不查元方法，不添加零除或定义域门禁。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MulFloat(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);

/**
 * @brief 按值槽数值类型执行除法并保留左操作数元方法退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 整数同类及混合整数类检查零除；double 数值退路也检查零；其他类型查 ZR_META_DIV，缺失时成功写 null。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Div(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot);

/**
 * @brief 执行 unsigned 除法值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两整数标签转 uint64，结果写 UINT64；其余可提取原生数值/bool 经 double，结果写 DOUBLE；整数路径检查零除；不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_DivUnsigned(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot);

/**
 * @brief 以 double 完成除法专用 opcode。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 接受原生 signed/unsigned/float/bool；输出 DOUBLE；不查元方法，不添加零除或定义域门禁。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_DivFloat(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);

/**
 * @brief 执行数值余数并保留左操作数元方法退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两 unsigned 用 unsigned 余数；其余整数把负除数取绝对值后取模；含 bool/float 数值退路用 fmod；检查零；无元方法时成功写 null。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Mod(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot);

/**
 * @brief 用函数常量完成 signed 余数。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两整数类按 int64 计算，负除数取绝对值；其余原生数值/bool 走 fmod；检查零除；右参数是常量索引。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ModSignedConst(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 leftSlot,
                                                           TZrUInt32 constantIndex);

/**
 * @brief 执行 unsigned 余数值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两整数标签转 uint64，结果写 UINT64；其余可提取原生数值/bool 经 double，结果写 DOUBLE；整数路径检查零除；不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ModUnsigned(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot);

/**
 * @brief 执行 unsigned 余数值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须为 unsigned 整数类；结果写 UINT64；整数路径检查零除；不查元方法；右操作数从当前函数常量表读取，索引不代表帧槽。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ModUnsignedConst(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 constantIndex);

/**
 * @brief 以 double 完成余数专用 opcode。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 接受原生 signed/unsigned/float/bool；输出 DOUBLE；不查元方法，不添加零除或定义域门禁。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ModFloat(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);

/**
 * @brief 执行通用整数加法的值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须为整数标签，unsigned 按 signed 载荷参与；结果写 INT64；类型不符返回 false。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddInt(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot);

/**
 * @brief 执行通用整数加法的值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 整数类提取允许 bool；结果写 INT64；类型不符返回 false；右操作数从当前函数常量表读取，索引不代表帧槽。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddIntConst(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 constantIndex);

/**
 * @brief 执行有符号加法专用值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须为有符号整数类；写 INT64；类型不符返回 false，不做元方法回退。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddSigned(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

/**
 * @brief 执行有符号加法专用值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须为有符号整数类；写 INT64；类型不符返回 false，不做元方法回退；右操作数从当前函数常量表读取，索引不代表帧槽。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddSignedConst(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 leftSlot,
                                                           TZrUInt32 constantIndex);

/**
 * @brief 执行无符号加法专用值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须为无符号整数类；写 UINT64；类型不符返回 false，不做元方法回退。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddUnsigned(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot);

/**
 * @brief 执行无符号加法专用值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须为无符号整数类；写 UINT64；类型不符返回 false，不做元方法回退；右操作数从当前函数常量表读取，索引不代表帧槽。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddUnsignedConst(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 constantIndex);

/**
 * @brief 执行整数减法并允许原生数值提升退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两整数先转 int64，相减后保留左源标签；其余原生数值/bool 经 double 相减；不可提取值报运行错误。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubInt(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot);

/**
 * @brief 执行整数减法并允许原生数值提升退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两整数先转 int64，相减后保留左源标签；其余原生数值/bool 经 double 相减；不可提取值报运行错误；右操作数从当前函数常量表读取，索引不代表帧槽。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubIntConst(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 constantIndex);

/**
 * @brief 执行有符号减法专用值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须为有符号整数类；写 INT64；类型不符返回 false，不做元方法回退。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubSigned(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

/**
 * @brief 执行有符号减法专用值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须为有符号整数类；写 INT64；类型不符返回 false，不做元方法回退；右操作数从当前函数常量表读取，索引不代表帧槽。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubSignedConst(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 leftSlot,
                                                           TZrUInt32 constantIndex);

/**
 * @brief 执行无符号减法专用值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须为无符号整数类；写 UINT64；类型不符返回 false，不做元方法回退。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubUnsigned(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot);

/**
 * @brief 执行无符号减法专用值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须为无符号整数类；写 UINT64；类型不符返回 false，不做元方法回退；右操作数从当前函数常量表读取，索引不代表帧槽。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubUnsignedConst(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 constantIndex);

/**
 * @brief 执行整数值槽位操作并写入 INT64 结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 源标签必须为 signed/unsigned 整数，不接受 bool 或 float；结果为 INT64，不调用元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BitwiseXor(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot);

/**
 * @brief 执行整数值槽位操作并写入 INT64 结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 源标签必须为 signed/unsigned 整数，不接受 bool 或 float；结果为 INT64，不调用元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BitwiseNot(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot);

/**
 * @brief 执行整数值槽位操作并写入 INT64 结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 源标签必须为 signed/unsigned 整数，不接受 bool 或 float；结果为 INT64，不调用元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BitwiseAnd(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot);

/**
 * @brief 执行整数值槽位操作并写入 INT64 结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 源标签必须为 signed/unsigned 整数，不接受 bool 或 float；结果为 INT64，不调用元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BitwiseOr(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

/**
 * @brief 执行整数值槽位操作并写入 INT64 结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 源标签必须为 signed/unsigned 整数，不接受 bool 或 float；按 int64 载荷进行位运算；TODO：核合法 lowering 输入与槽标签能否满足 C 左移的位宽、符号和结果边界；尚无完整合法触发证明。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BitwiseShiftLeft(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 rightSlot);

/**
 * @brief 执行整数值槽位操作并写入 INT64 结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 源标签必须为 signed/unsigned 整数，不接受 bool 或 float；右移先转 uint64，保留逻辑右移；移位量必须满足实际 C 右移宽度边界，极值约束见实现已有注释。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BitwiseShiftRight(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

/**
 * @brief 执行 signed 乘法并允许原生数值提升退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两整数标签直接读取 nativeInt64 相乘并写 INT64；其余原生数值/bool 经 double 相乘；不可提取返回 false。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MulSigned(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

/**
 * @brief 执行 signed 乘法并允许原生数值提升退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两整数标签直接读取 nativeInt64 相乘并写 INT64；其余原生数值/bool 经 double 相乘；不可提取返回 false；右操作数从当前函数常量表读取，索引不代表帧槽。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MulSignedConst(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 leftSlot,
                                                            TZrUInt32 constantIndex);

/**
 * @brief 执行 unsigned 乘法值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须为 unsigned 整数类；结果写 UINT64；不查元方法；右操作数从当前函数常量表读取，索引不代表帧槽。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MulUnsignedConst(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 constantIndex);

/**
 * @brief 执行 signed 除法并允许原生数值提升退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两整数先转 int64，检查零除后写 INT64；其余原生数值/bool 经 double 除法，不追加浮点零除门禁；不可提取报运行错误。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_DivSigned(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

/**
 * @brief 执行 signed 除法并允许原生数值提升退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两整数先转 int64，检查零除后写 INT64；其余原生数值/bool 经 double 除法，不追加浮点零除门禁；不可提取报运行错误；右操作数从当前函数常量表读取，索引不代表帧槽。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_DivSignedConst(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 leftSlot,
                                                            TZrUInt32 constantIndex);

/**
 * @brief 执行 unsigned 除法值槽退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源须为 unsigned 整数类；结果写 UINT64；整数路径检查零除；不查元方法；右操作数从当前函数常量表读取，索引不代表帧槽。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_DivUnsignedConst(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 constantIndex);

/**
 * @brief 为泛型幂调用左操作数元方法。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 只按对应 meta 分派，不自行执行原生数值计算；元方法缺失时成功写 null；元调用由临时调用帧承载。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Pow(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot);

/**
 * @brief 执行整数幂或提升后的浮点幂并写回值槽。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两整数标签使用 int64 与 IntPower；负底数及零底数非正指数报错；其他可提取数值（含 bool）走 double pow；不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PowSigned(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

/**
 * @brief 执行整数幂或提升后的浮点幂并写回值槽。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两整数标签使用 uint64 与 UIntPower；0^0 报错；其他可提取数值（含 bool）走 double pow；不查元方法。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PowUnsigned(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot);

/**
 * @brief 以 double 完成幂专用 opcode。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 接受原生 signed/unsigned/float/bool；输出 DOUBLE；不查元方法，不添加零除或定义域门禁。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PowFloat(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);

/**
 * @brief 为泛型左移调用左操作数元方法。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 只按对应 meta 分派，不自行执行原生数值计算；元方法缺失时成功写 null；元调用由临时调用帧承载。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShiftLeft(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

/**
 * @brief 执行整数值槽位操作并写入 INT64 结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 源标签必须为 signed/unsigned 整数，不接受 bool 或 float；按 int64 载荷进行位运算；TODO：核合法 lowering 输入与槽标签能否满足 C 左移的位宽、符号和结果边界；尚无完整合法触发证明。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShiftLeftInt(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot);

/**
 * @brief 为泛型右移调用左操作数元方法。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 只按对应 meta 分派，不自行执行原生数值计算；元方法缺失时成功写 null；元调用由临时调用帧承载。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShiftRight(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot);

/**
 * @brief 执行整数值槽位操作并写入 INT64 结果。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 源标签必须为 signed/unsigned 整数，不接受 bool 或 float；按 int64 载荷进行位运算；移位量必须满足实际 C 右移宽度边界，极值约束见实现已有注释。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShiftRightInt(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 leftSlot,
                                                          TZrUInt32 rightSlot);

/**
 * @brief 为数值取负并保留非数值元方法退路。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note signed/float 保留源标签；unsigned 转 signed 再取负；其他类型查 ZR_META_NEG，缺失时成功写 null。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Neg(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 sourceSlot);

/**
 * @brief 把值槽共享真值的否定写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 不调用 TO_BOOL 元方法；字符串按长度，null 和数值零为假；有效源槽可为任意类型。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 * TODO: 核查 LLVM unary/type-conversion 的 destinationSlot 与 operandA1 及 compile_expression 槽分配是否允许同槽；FAST_SET 先改目标标签再求真值，尚无合法同槽触发证明，不承诺 in-place 安全。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNot(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot);

/**
 * @brief 合并两个已求值 BOOL 槽并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源必须是 BOOL；本 helper 不控制右表达式求值或承担语言短路；槽或类型失败记录错误。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalAnd(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot);

/**
 * @brief 合并两个已求值 BOOL 槽并写入 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 两源必须是 BOOL；本 helper 不控制右表达式求值或承担语言短路；槽或类型失败记录错误。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalOr(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

/**
 * @brief 将值槽转换为字符串并建立生成代码可继续使用的目标槽。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note core 转换可能分配或调用元方法；必须按 callInfo 刷新 frame 后重新取目标槽；成功结果是 GC 字符串，空结果写 null。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToString(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot);

/** @brief 创建绑定成员位置对象。
 * @note 成员表索引按当前函数解释；内联字段保存来源帧锚点，其他分支保存原型描述符；位置对象不延长来源帧寿命。创建失败可已有外壳或字段，不能承诺目的槽原值保持。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PropertyReferenceCreateMember(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 receiverSlot,
        TZrUInt32 memberEntryIndex);

/** @brief 捕获稍后读写使用的接收者和键。
 * @note 捕获位置而非立即取值；位置外壳交给目的槽持有，索引协议在 Load/Store 才调用。分步建壳失败无整体回滚保证。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PropertyReferenceCreateIndex(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 receiverSlot,
        TZrUInt32 keySlot);

/** @brief 把本地槽绑定为可稍后读写的位置。
 * @note 内联槽追踪实际别名来源，普通槽保存函数与相对栈锚点；引用不拥有活动调用帧，调用方须限制其有效期。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PropertyReferenceCreateLocal(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot);

/** @brief 经位置种类读取到目的槽。
 * @note 由 core 校验帧锚点、成员描述符或动态索引；目的槽覆盖沿值所有权规则，回调失败不保证保留原目的值。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PropertyReferenceLoad(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 referenceSlot);

/** @brief 向已捕获的位置写入源值。
 * @note 成员分支仍验证可写性，帧槽分支同步关闭语义的物理镜像；返回失败仅是操作未成功，不撤销已执行回调或部分写入。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PropertyReferenceStore(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 sourceSlot,
        TZrUInt32 referenceSlot);

/** @brief 按成员表符号读取对象成员。
 * @note 局部接收者副本防止目的槽与接收者同槽覆盖；副本不是新增持有凭据。生成调用须符合编译与已绑定访问上下文；core 负责模块 pending 等实际运行限制、属性回调和目的值所有权，不补全 private/protected 访问范围检查。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetMember(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 receiverSlot,
                                                      TZrUInt32 memberId);

/** @brief 按成员表符号写入对象成员。
 * @note 调用方借用活动生成帧和源槽，写权限、域校验及写屏障交给 core；属性回调副作用不因桥接返回失败撤销。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetMember(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 sourceSlot,
                                                      TZrUInt32 receiverSlot,
                                                      TZrUInt32 memberId);

/** @brief 为生成器提供新接收者写入优化提示。
 * @note 当前 core 对象路径还按实际年轻可移动存储检查免屏障条件；优化名不放宽成员写权限或域检查。
 * TODO: 对照生成器 backend_aot_c_slot_has_unescaped_new_owner 的控制流和别名边界及 core 年轻存储检查，核实哪些写入实际获免屏障，不据局部证明直接推定老接收者漏屏障。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetMemberNewOwnerNoWriteBarrier(struct SZrState *state,
                                                                            ZrAotGeneratedFrame *frame,
                                                                            TZrUInt32 sourceSlot,
                                                                            TZrUInt32 receiverSlot,
                                                                            TZrUInt32 memberId);

/** @brief 按成员缓存契约选择读取路径。
 * @note 已绑定缓存准备可调用目标而非读取普通字段，失配返回失败且不降级；无绑定缓存先试物理内联成员，再以符号访问对象。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetMemberSlot(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 receiverSlot,
                                                          TZrUInt32 cacheIndex);

/** @brief 按成员缓存符号写入物理字段或对象成员。
 * @note 内联路径成功即完成，不能继续动态写入；内联不适用时才走对象权限和屏障。cacheIndex 不是成员表索引。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetMemberSlot(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 sourceSlot,
                                                          TZrUInt32 receiverSlot,
                                                          TZrUInt32 cacheIndex);

/** @brief 为缓存成员的对象退路提供免屏障提示。
 * @note 内联字段仍走普通布局写入接口；仅对象退路使用带实际年轻存储校验的优化入口，不能把全部路径称为免屏障写。
 * TODO: 对照物理内联字段写入与对象退路的实际屏障，核查布局或别名变化后生成器新接收者提示是否仍适用。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetMemberSlotNewOwnerNoWriteBarrier(struct SZrState *state,
                                                                                ZrAotGeneratedFrame *frame,
                                                                                TZrUInt32 sourceSlot,
                                                                                TZrUInt32 receiverSlot,
                                                                                TZrUInt32 cacheIndex);

/** @brief 跨动态索引调用读取并刷新生成帧。
 * @note 先稳定接收者和键并初始化临时结果，以活动 callInfo 发布栈顶；回调后重取槽基址和目的槽再按所有权复制，失败不保证回调副作用回滚。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetByIndex(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 receiverSlot,
                                                       TZrUInt32 keySlot);

/** @brief 跨动态索引调用写入并恢复生成帧锚点。
 * @note 稳定三份输入值不是转移其所有权；发布活动调用栈后允许协议再入或扩栈，返回后从 callInfo 刷新，失败不撤销已完成的写入。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetByIndex(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 sourceSlot,
                                                       TZrUInt32 receiverSlot,
                                                       TZrUInt32 keySlot);

/** @brief 带新接收者优化提示执行动态索引写入。
 * @note 保留稳定输入和回调后帧刷新协议；core 根据实际存储决定是否跳过对象写屏障，不能据生成器提示保证 receiver 永远年轻。
 * TODO: 核查生成器局部新接收者扫描与索引回调、数组别名的关系，并在 core 实际年轻存储校验下观察增量收集；尚无老接收者绕过屏障的完整证明。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetByIndexNewOwnerNoWriteBarrier(struct SZrState *state,
                                                                             ZrAotGeneratedFrame *frame,
                                                                             TZrUInt32 sourceSlot,
                                                                             TZrUInt32 receiverSlot,
                                                                             TZrUInt32 keySlot);

/** @brief 把接收者的当前项存储对象绑定到目的槽。
 * @note 结果是受值系统管理的数组对象引用，不是裸数据指针或独立租约；后续绑定项访问针对这个已解析对象，不重新解析接收者。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayBindItems(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 receiverSlot);

/** @brief 从已绑定项对象读取有符号整数索引。
 * @note 绑定槽须存放真实内部数组；这个专用入口不走通用索引协议，越界及结果语义交给项存储实现。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayGetIntBoundItems(struct SZrState *state,
                                                                       ZrAotGeneratedFrame *frame,
                                                                       TZrUInt32 destinationSlot,
                                                                       TZrUInt32 itemsSlot,
                                                                       TZrUInt32 keySlot);

/** @brief 向已绑定项对象写入整数载荷。
 * @note 键和值均须为有符号整数且项对象必须是内部数组；不重新解析接收者的 items 成员，也不把绑定结果当裸缓冲区。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArraySetIntBoundItems(struct SZrState *state,
                                                                       ZrAotGeneratedFrame *frame,
                                                                       TZrUInt32 sourceSlot,
                                                                       TZrUInt32 itemsSlot,
                                                                       TZrUInt32 keySlot);

/** @brief 先试整数数组快路径再保留通用索引退路。
 * @note 稳定接收者允许结果与 receiver 同槽；底层不适用快路径时可调用动态索引协议，下一条生成指令须从 callInfo 重取基址。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayGetInt(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 receiverSlot,
                                                             TZrUInt32 keySlot);

/** @brief 先试整数数组写入再保留通用索引退路。
 * @note 类型特化不代表所有 receiver 必须走快路径；退路仍有权限、域和动态调用语义，失败不提供事务回滚。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArraySetInt(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 sourceSlot,
                                                             TZrUInt32 receiverSlot,
                                                             TZrUInt32 keySlot);

/** @brief 为整数数组写入的通用退路提供优化提示。
 * @note 整数快路径与普通入口相同，只有通用索引退路使用新接收者提示；当前对象层实际年轻存储校验仍生效。
 * TODO: 核查生成器新接收者扫描、绑定项别名与通用索引退路；整数快路径及 core 年轻存储检查需分别观察，不能把提示当作屏障漏洞证明。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArraySetIntNewOwnerNoWriteBarrier(struct SZrState *state,
                                                                                   ZrAotGeneratedFrame *frame,
                                                                                   TZrUInt32 sourceSlot,
                                                                                   TZrUInt32 receiverSlot,
                                                                                   TZrUInt32 keySlot);

/** @brief 向数组式接收者追加整数并可丢弃结果。
 * @note 返回值标志在这里表示无需目的槽，用已初始化临时结果接收；追加副作用仍执行，慢路径可调用 add 成员，失败不是撤销追加。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayAddInt(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 receiverSlot,
                                                             TZrUInt32 sourceSlot);

/** @brief 向四个连续接收者槽批量追加一个整数。
 * @note 四槽是四个接收者而非一个数组的四元素；先检查源类型和槽再由 core 准备及提交，准备或提交可能改变容量和部分接收者，失败不保证整体回滚。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayAddInt4(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 receiverBaseSlot,
                                                              TZrUInt32 sourceSlot);

/** @brief 从当前函数常量表取整数并批量追加。
 * @note constantIndex 是元数据索引，不是立即数；四槽接收者与普通批量入口共享准备及提交契约，失败不宣称四对象全保持原状。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayAddInt4Const(struct SZrState *state,
                                                                   ZrAotGeneratedFrame *frame,
                                                                   TZrUInt32 receiverBaseSlot,
                                                                   TZrUInt32 constantIndex);

/** @brief 把常量整数重复追加到四个接收者。
 * @note countSlot 提供有符号次数，非正次数由 core 视为成功空操作；批量追加与容量准备可能部分生效，不是覆盖填充或原子事务。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayFillInt4Const(struct SZrState *state,
                                                                    ZrAotGeneratedFrame *frame,
                                                                    TZrUInt32 receiverBaseSlot,
                                                                    TZrUInt32 countSlot,
                                                                    TZrUInt32 constantIndex);

/** @brief 从可迭代值创建同步游标。
 * @note 稳定 iterable 副本支持与目的槽重叠；core 选择原型契约回调或数组默认游标，返回游标受目的槽值所有权管理，不是外部资源租约。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_IterInit(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 iterableSlot);

/** @brief 推进游标并把协议结果写入目的槽。
 * @note 函数返回值表示调用是否成功，不表示是否还有元素；默认数组或原型回调可更新游标，返回失败不回滚游标状态。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_IterMoveNext(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 iteratorSlot);

/** @brief 读取同步游标的当前项。
 * @note 按当前成员缓存、协议函数或默认游标隐藏值读取；调用方应遵守成功推进后的协议时序，此桥接不把元素结束当作操作失败。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_IterCurrent(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 iteratorSlot);

/** @brief 推进同步游标并产生失败条件分支输出。
 * @note 先把非空分支输出置为假；只有成功得到布尔结果才写取反值。非布尔结果可能已推进游标，失败不撤销推进，回边安全点由生成器处理。
 * TODO: 对照 object_call 的结果槽锚点恢复与本函数随后读取的 frame->slotBase，核查用户 moveNext 回调扩栈时同条指令内是否须立即刷新生成帧；下一条指令的刷新不能替代此处复合读取。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(struct SZrState *state,
                                                                    ZrAotGeneratedFrame *frame,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 iteratorSlot,
                                                                    TZrBool *outJumpIfFalse);

/** @brief 用通用 VM 调用执行未准备的 callable 并取单结果。
 * @note functionSlot 后需有 argumentCount 连续参数；call binding 在调用前校验。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Call(struct SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 functionSlot,
                                                 TZrUInt32 argumentCount);

/** @brief 从生成泛型字典借用类型布局。
 * @note 静态布局优先，否则通过 metadataRuntime 解析 token 并写 resolvedSlots；该可变缓存只能用于同一元数据运行时，不能视为跨 global 的缓存。
 */
ZR_LIBRARY_API const struct SZrTypeLayout *ZrLibrary_AotRuntime_GenericSlot_TypeLayout(
        struct SZrState *state,
        const SZrAotGenericDictionary *dictionary,
        struct SZrMetadataRuntime *metadataRuntime,
        TZrUInt32 slotIndex);

/** @brief 取得泛型字典槽的布局大小。
 * @note outSize 必需且先置零；类型布局无法解析返回 false，不新建值或实例。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericSlot_TryGetSizeOf(
        struct SZrState *state,
        const SZrAotGenericDictionary *dictionary,
        struct SZrMetadataRuntime *metadataRuntime,
        TZrUInt32 slotIndex,
        TZrSize *outSize);

/** @brief 取得泛型字典槽的静态方法 thunk。
 * @note 当前只读 staticMethod，不由 metadataFunction 动态解析；无字典、索引错误或无静态方法返回 null，返回地址借用代码注册生命周期。
 */
ZR_LIBRARY_API FZrAotEntryThunk ZrLibrary_AotRuntime_GenericSlot_Method(
        struct SZrState *state,
        const SZrAotGenericDictionary *dictionary,
        const struct SZrFunction *metadataFunction,
        TZrUInt32 slotIndex);

/** @brief 执行 prepared thunk 并完成帧，或通用回退。
 * @note prepared=false 回退只取一个结果；prepared thunk 必须非 null。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallPreparedOrGeneric(struct SZrState *state,
                                                                  ZrAotGeneratedFrame *frame,
                                                                  ZrAotGeneratedDirectCall *directCall,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 functionSlot,
                                                                  TZrUInt32 argumentCount,
                                                                  TZrUInt32 resultCount);

/** @brief 执行 prepared thunk 并把当前 caller handler 索引交回生成器。
 * @note 未 prepared 回退仍保留 fallthrough；输出索引指针必需。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallPreparedOrGenericWithResume(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        ZrAotGeneratedDirectCall *directCall,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 resultCount,
        TZrUInt32 *outResumeInstructionIndex);

/** @brief 完成 thunk 结果或接受已展开到 caller 的异常继续点。
 * @note 成功调用走 Finish；失败只在 FINE、hasCurrentException 且已回 caller 时继续。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CompletePreparedDirectCallWithResume(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        ZrAotGeneratedDirectCall *directCall,
        TZrBool invocationSucceeded,
        TZrUInt32 resultCount,
        TZrUInt32 *outResumeInstructionIndex);

/** @brief 从生成物理槽调用值并在扩栈后恢复帧视图。
 * @note 先试 prepared AOT 调用，未准备时走 core 通用调用；保存 stack anchor 后重取槽；失败状态交给调用方异常协议，不保证参数、结果或副作用回滚。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallStackValue(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 functionSlot,
                                                           TZrUInt32 argumentCount,
                                                           const TZrChar *errorLabel);

/** @brief 展开尾部 spread 数组并调用前缀实参与展开值。
 * @note spread 源必须是实现支持的数组；展开与扩栈后重定位调用窗口，再委托 CallStackValue；失败可能已展开或准备调用。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallSpread(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 prefixArgumentCount,
        const TZrChar *errorLabel);

/** @brief 按 caller/callee 元数据绑定兼容性判断是否允许 typed 直接调用。
 * @note false 是选择 deopt 退路的判断结果；不创建 callee 帧，也不执行函数。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CanUseTypedDirectCall(struct SZrState *state,
                                                                  ZrAotGeneratedFrame *frame,
                                                                  TZrUInt32 calleeFunctionIndex);

/** @brief 为 typed 调用不兼容路径转用 VM 值槽通用调用。
 * @note calleeFunctionIndex 当前不参与重建，参数已由生成器物化到函数及实参槽；本入口不恢复编译器局部缓存。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_DeoptTypedDirectCall(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 argumentCount,
                                                                 TZrUInt32 calleeFunctionIndex,
                                                                 const TZrChar *errorLabel);

/** @brief 检查 deopt 标识后执行 VM 值槽通用调用。
 * @note 调用方须事先物化函数及参数；通过 ValidateDynamicDeoptBridge 后仍可能调用失败，失败由 VM 状态承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallDynamicDeoptBridge(struct SZrState *state,
                                                                    ZrAotGeneratedFrame *frame,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 functionSlot,
                                                                    TZrUInt32 argumentCount,
                                                                    TZrUInt32 deoptId,
                                                                    const TZrChar *errorLabel);

/** @brief 核对动态 deopt 标识能否用于当前函数。
 * @note NONE 或未配置 deopt 表当前可直接通过；true 只说明此检查通过，不证明已完成状态重建或执行。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(struct SZrState *state,
                                                                       ZrAotGeneratedFrame *frame,
                                                                       TZrUInt32 deoptId,
                                                                       const TZrChar *errorLabel);

/** @brief 为当前生成器无法支持的元调用写入明确运行时错误。
 * @note 始终返回 false；参数用于入口一致性和错误标签，不执行元操作或写回结果；调用方必须走失败协议。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_UnsupportedMetaCall(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 receiverSlot,
                                                                TZrUInt32 argumentCount,
                                                                const TZrChar *errorLabel);

/** @brief 为当前生成器无法支持的元成员访问写入明确运行时错误。
 * @note 始终返回 false；参数用于入口一致性和错误标签，不执行元操作或写回结果；调用方必须走失败协议。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(struct SZrState *state,
                                                                       ZrAotGeneratedFrame *frame,
                                                                       TZrUInt32 primarySlot,
                                                                       TZrUInt32 secondarySlot,
                                                                       TZrUInt32 memberOrCacheIndex,
                                                                       const TZrChar *opcodeName);

/** @brief 为当前生成器无法支持的动态值访问写入明确运行时错误。
 * @note 始终返回 false；参数用于入口一致性和错误标签，不执行元操作或写回结果；调用方必须走失败协议。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_UnsupportedDynamicValueAccess(struct SZrState *state,
                                                                          ZrAotGeneratedFrame *frame,
                                                                          TZrUInt32 primarySlot,
                                                                          TZrUInt32 secondarySlot,
                                                                          TZrUInt32 operandIndex,
                                                                          const TZrChar *opcodeName);

/** @brief 在 caller 临时窗口以已绑定 thunk 执行静态直接调用。
 * @note 要求物理槽中 callable、calleeFunctionIndex 和 calleeThunk 身份一致；临时实参 Value_Copy 后准备 callee，成功后 PostCall 和清窗；失败清理仍以实现路径为准。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallStaticDirect(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 functionSlot,
                                                             TZrUInt32 argumentCount,
                                                             TZrUInt32 calleeFunctionIndex,
                                                             FZrAotEntryThunk calleeThunk);

/** @brief 以静态 thunk 调用并把结果放入 caller 内联结构返回位置。
 * @note 要求 callable 身份及返回布局、偏移、大小匹配；调用方提供活的 caller 帧和目标位置，callee 使用内联返回协议，普通值槽返回不能替代它。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallInlineStruct(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 functionSlot,
                                                             TZrUInt32 argumentCount,
                                                             TZrUInt32 calleeFunctionIndex,
                                                             TZrUInt32 destinationTypeLayoutId,
                                                             TZrUInt32 destinationByteOffset,
                                                             TZrUInt32 destinationByteSize,
                                                             FZrAotEntryThunk calleeThunk);

/** @brief 通过通用 VM 调用完成内联结构动态 deopt 退路。
 * @note 先核 deopt 与内联返回位置，再进行无 yield 通用调用；失败不能视为位置、参数或作用域已回滚。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 destinationTypeLayoutId,
        TZrUInt32 destinationByteOffset,
        TZrUInt32 destinationByteSize,
        TZrUInt32 deoptId,
        const TZrChar *errorLabel);

/** @brief 按当前 call binding 尝试建立 AOT 直调凭据。
 * @note true 不等于 prepared；凭据由本次调用初始化。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PrepareDirectCall(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 functionSlot,
                                                              TZrUInt32 argumentCount,
                                                              ZrAotGeneratedDirectCall *directCall);

/** @brief 把 receiver 窗口改写为 @call 参数后尝试建立 AOT 直调。
 * @note 通用回退也已插入 receiver 首参，调用者应传 argumentCount+1。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PrepareMetaCall(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 receiverSlot,
                                                            TZrUInt32 argumentCount,
                                                            ZrAotGeneratedDirectCall *directCall);

/** @brief 核验 static flat index 和元数据/thunk 身份后建立直调帧。
 * @note 缺失 thunk 或身份漂移均失败，不走通用回退。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PrepareStaticDirectCall(struct SZrState *state,
                                                                    ZrAotGeneratedFrame *frame,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 functionSlot,
                                                                    TZrUInt32 argumentCount,
                                                                    TZrUInt32 calleeFunctionIndex,
                                                                    ZrAotGeneratedDirectCall *directCall);

/** @brief 完成 callee 关闭和 PostCall，再把结果 owner 交回生成槽。
 * @note 须仍处 callee 且 caller/frame 匹配；成功消费并清零 directCall。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_FinishDirectCall(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             ZrAotGeneratedDirectCall *directCall,
                                                             TZrUInt32 resultCount);

/** @brief 将当前函数 handler 注册到 core 异常栈。
 * @note handlerIndex 必须小于当前函数 exceptionHandlerCount。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Try(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 handlerIndex);

/** @brief 让已登记 handler 进入 finally 或弹出无 finally handler。
 * @note handlerIndex 属于当前函数；不存在活动 handler 时仍成功。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_EndTry(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 handlerIndex);

/** @brief 先规范化 payload 再清 pending，展开并返回当前帧 handler 索引。
 * @note 跨帧展开不能在旧生成函数继续；无 handler 时向 core 抛出。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Throw(struct SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 sourceSlot,
                                                  TZrUInt32 *outResumeInstructionIndex);

/** @brief 为直接访问的 null receiver 建 NullReferenceError 并进入 handler。
 * @note 非 null 正常返回且索引保持 fallthrough；跨帧展开不能继续旧帧。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_RequireNonNull(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 sourceSlot,
                                                           TZrUInt32 *outResumeInstructionIndex);

/** @brief 为 JUMP_IF_NULL 提供源值 null 类型判定。
 * @note 输出 bool 与执行是否成功分开，outIsNull 必需。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_IsNull(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 sourceSlot,
                                                   TZrBool *outIsNull);

/** @brief 复制当前异常到目标槽并释放当前异常与 pending 状态。
 * @note 无异常写 null；清 pending 可触发 drop 后需刷新帧。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Catch(struct SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot);

/** @brief 结束 handler 后恢复 exception 或 return/break/continue。
 * @note pending return 值先恢复到所属 valueSlot；输出索引供 dispatch。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_EndFinally(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 handlerIndex,
                                                       TZrUInt32 *outResumeInstructionIndex);

/** @brief 保留返回值并优先经过外层 finally。
 * @note 目标索引为当前函数生成指令索引，pending 值由 core 持有。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetPendingReturn(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 sourceSlot,
                                                             TZrUInt32 targetInstructionIndex,
                                                             TZrUInt32 *outResumeInstructionIndex);

/** @brief 保存 break 目标并执行需要离开的 finally。
 * @note 不携带返回值；最终目标由 core 跳转校验。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetPendingBreak(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 targetInstructionIndex,
                                                            TZrUInt32 *outResumeInstructionIndex);

/** @brief 保存 continue 目标并执行需要离开的 finally。
 * @note 不携带返回值；最终目标由 core 跳转校验。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetPendingContinue(struct SZrState *state,
                                                               ZrAotGeneratedFrame *frame,
                                                               TZrUInt32 targetInstructionIndex,
                                                               TZrUInt32 *outResumeInstructionIndex);

/** @brief 把逻辑槽转换为物理清理注册并挂入 core 关闭链。
 * @note 双表示槽由 cleanup registration 选择，登记顺序须单调。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MarkToBeClosed(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 slotIndex);

/** @brief 登记较高 proxy 以在内层作用域关闭原 dense local。
 * @note proxySlot 必须大于 sourceSlot；实际 proxy 使用物理注册槽。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MarkCloseProxy(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 proxySlot,
                                                           TZrUInt32 sourceSlot);

/** @brief 执行生成 CLOSE_SCOPE 的计数清理。
 * @note frame 参数不参与清理；链不足时 helper 只关闭已有项。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CloseScope(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 cleanupCount);

/**
 * @brief 执行 TO_BOOL 元转换或共享真值转换并写 BOOL。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 元方法结果为 BOOL 时保留，否则归一为 true；元调用后重新定位目标槽；无元方法时按共享真值判断。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 * TODO: 核查 LLVM unary/type-conversion 的 destinationSlot 与 operandA1 及 compile_expression 槽分配是否允许同槽；FAST_SET 先改目标标签再求真值，尚无合法同槽触发证明，不承诺 in-place 安全。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToBool(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 sourceSlot);

/**
 * @brief 执行 TO_INT 元转换或原生值转换。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 元方法结果为任意整数标签时保留，否则写 signed 0；无元方法时整数原样复制，float 强转 int64，bool 转 0/1，其他类型写 0。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToInt(struct SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 sourceSlot);

/**
 * @brief 执行 TO_UINT 元转换或原生 unsigned 转换。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 元方法结果为任意整数标签时保留，否则写 unsigned 0；无元方法时 unsigned 复制，signed/float 转 uint64，bool 转 0/1，其他类型写 0。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToUInt(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 sourceSlot);

/**
 * @brief 执行 TO_FLOAT 元转换或原生 double 转换。
 * @pre state 和当前生成帧有效，参数槽或常量索引属于该帧的函数。
 * @note 元方法结果为 float 类时保留，否则写 0.0；无元方法时 float 复制，整数/bool 转 double，其余写 0.0；元调用后刷新目标槽。
 * @return 操作成功状态；比较和转换的实际结果由目标槽或 out 参数承载。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToFloat(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 sourceSlot);

/** @brief 将模块入口生成槽中的导出发布到项目 module。
 * @note 已 moduleExecuted 时直接成功；优先选择匹配 activeRecord。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PublishModuleExports(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame);

/** @brief 关闭异常 handler/upvalue 后按 native-call ABI 发布单结果。
 * @note 返回 1 是结果数量；constructor 保留 receiver 返回位置。
 */
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_Return(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 sourceSlot,
                                                    TZrBool publishExports);
/** @brief 为生成 epilogue 把signed 整数返回值写到 caller result。
 * @note 要求仍处有效 callee callInfo；关闭 closure 并发布 callee 返回栈位置，不执行 caller PostCall；TODO：核合法清理回调经扩栈迁移后的 caller result 重定位责任，不继承旧 BUG 标签的完整触发信用。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ReturnI64(struct SZrState *state, TZrInt64 value);
/** @brief 为生成 epilogue 把布尔返回值写到 caller result。
 * @note 要求仍处有效 callee callInfo；关闭 closure 并发布 callee 返回栈位置，不执行 caller PostCall；TODO：核合法清理回调经扩栈迁移后的 caller result 重定位责任，不继承旧 BUG 标签的完整触发信用。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ReturnBool(struct SZrState *state, TZrBool value);
/** @brief 为生成 epilogue 把unsigned 整数返回值写到 caller result。
 * @note 要求仍处有效 callee callInfo；关闭 closure 并发布 callee 返回栈位置，不执行 caller PostCall；TODO：核合法清理回调经扩栈迁移后的 caller result 重定位责任，不继承旧 BUG 标签的完整触发信用。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ReturnU64(struct SZrState *state, TZrUInt64 value);
/** @brief 为生成 epilogue 把浮点返回值写到 caller result。
 * @note 要求仍处有效 callee callInfo；关闭 closure 并发布 callee 返回栈位置，不执行 caller PostCall；TODO：核合法清理回调经扩栈迁移后的 caller result 重定位责任，不继承旧 BUG 标签的完整触发信用。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ReturnF64(struct SZrState *state, TZrFloat64 value);
/** @brief 为生成 epilogue 发布内联结构返回槽与 skipDrop 槽。
 * @note 校验 source 布局、字节位置与返回约定；不在此复制到 caller、关闭 closure 或 PostCall，调用方必须继续生成器返回与清理协议。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ReturnInlineStruct(struct SZrState *state,
                                                               ZrAotGeneratedFrame *frame,
                                                               TZrUInt32 sourceSlot,
                                                               TZrUInt32 sourceTypeLayoutId,
                                                               TZrUInt32 sourceByteOffset,
                                                               TZrUInt32 sourceByteSize,
                                                               TZrUInt32 *outSkipDropSlot);

/** @brief 给无法生成的指令记录函数/指令/opcode 诊断。
 * @note 返回 0 让生成函数失败出口结束。
 */
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_ReportUnsupportedInstruction(struct SZrState *state,
                                                                          TZrUInt32 functionIndex,
                                                                          TZrUInt32 instructionIndex,
                                                                          TZrUInt32 opcode);

/** @brief 为 LLVM 失败 label 使用 frame 索引补诊断。
 * @note 空 frame 传 UINT32_MAX，其他规则交 FailGeneratedFunctionAt。
 */
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_FailGeneratedFunction(struct SZrState *state,
                                                                   const ZrAotGeneratedFrame *frame);
/** @brief 为 guard 失败补充上下文诊断而保留已有错误。
 * @note 已跨帧异常或已有 lastError 不覆盖；UINT32_MAX 表示序言失败。
 */
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_FailGeneratedFunctionAt(
        struct SZrState *state,
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 functionIndex);

#endif
