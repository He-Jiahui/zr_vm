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

/** @brief 生成代码向调试与异常观察点报告的指令类别；BeginInstruction 用它决定是否发布当前 PC。 */
typedef enum EZrAotGeneratedStepFlag {
    ZR_AOT_GENERATED_STEP_FLAG_NONE = 0,
    ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW = 1u << 0,
    ZR_AOT_GENERATED_STEP_FLAG_CONTROL_FLOW = 1u << 1,
    ZR_AOT_GENERATED_STEP_FLAG_CALL = 1u << 2,
    ZR_AOT_GENERATED_STEP_FLAG_RETURN = 1u << 3
} EZrAotGeneratedStepFlag;

/* 直接调用恢复协议中表示顺序执行的哨兵；生成器不得把它当成真实指令索引。 */
#define ZR_AOT_RUNTIME_RESUME_FALLTHROUGH ((TZrUInt32)0xFFFFFFFFu)

/** @brief 一次生成函数调用的运行时视图，由 BeginGeneratedFunction 建立并在该调用帧存活期内使用。
 * @note 其中的函数表、模块与 recordHandle 均借用项目 AOT 记录；栈扩容或调用返回后须通过运行时入口刷新栈位置。
 */
typedef struct ZrAotGeneratedFrame {
    TZrPtr recordHandle;
    struct SZrFunction *function;
    struct SZrCallInfo *callInfo;
    TZrStackValuePointer slotBase;
    TZrUInt32 functionIndex;
    TZrUInt32 currentInstructionIndex;
    TZrUInt32 lastObservedInstructionIndex;
    TZrUInt32 lastObservedLine;
    TZrUInt32 observationMask;
    TZrUInt32 generatedFrameSlotCount;
    TZrBool publishAllInstructions;
    struct SZrObjectModule *module;
    TZrBool *moduleExecuted;
    struct SZrFunction **functionTable;
    TZrUInt32 functionCount;
    const SZrAotCodeRegistration *codeRegistration;
    const FZrAotEntryThunk *functionThunks;
    TZrUInt32 functionThunkCount;
} ZrAotGeneratedFrame;

/** @brief 生成器按函数索引解析的模块上下文，供生成入口校验描述符与绑定函数身份。 */
typedef struct ZrAotGeneratedModuleContext {
    TZrPtr recordHandle;
    struct SZrFunction *metadataFunction;
    const SZrAotMethodInfo *methodInfo;
    struct SZrObjectModule *module;
    TZrBool *moduleExecuted;
    struct SZrFunction **functionTable;
    TZrUInt32 functionCount;
    const SZrAotCodeRegistration *codeRegistration;
    const FZrAotEntryThunk *functionThunks;
    TZrUInt32 functionThunkCount;
    TZrUInt32 resolvedFunctionIndex;
    TZrUInt32 generatedFrameSlotCount;
} ZrAotGeneratedModuleContext;

/** @brief 准备阶段与完成阶段之间的直接调用凭据，保存调用双方帧及异常恢复位置。
 * @note 仅 prepared 为真时才能调用完成入口；通用调用回退时不得使用其余字段。
 */
typedef struct ZrAotGeneratedDirectCall {
    FZrAotEntryThunk nativeFunction;
    struct SZrCallInfo *callerCallInfo;
    struct SZrCallInfo *calleeCallInfo;
    TZrUInt32 callerFunctionIndex;
    TZrUInt32 calleeFunctionIndex;
    TZrUInt32 callInstructionIndex;
    TZrUInt32 resumeInstructionIndex;
    TZrUInt32 observationMaskSnapshot;
    TZrBool publishAllInstructionsSnapshot;
    TZrBool prepared;
} ZrAotGeneratedDirectCall;

/** @brief 项目请求的执行模式；AOT 模式会为模块导入安装严格的 AOT loader。 */
typedef enum EZrLibraryProjectExecutionMode {
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_INTERP = 0,
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_BINARY = 1,
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C = 2,
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM = 3
} EZrLibraryProjectExecutionMode;

/** @brief 本次项目执行实际经过的后端，供运行记录读取与测试断言使用。 */
typedef enum EZrLibraryExecutedVia {
    ZR_LIBRARY_EXECUTED_VIA_NONE = 0,
    ZR_LIBRARY_EXECUTED_VIA_INTERP = 1,
    ZR_LIBRARY_EXECUTED_VIA_BINARY = 2,
    ZR_LIBRARY_EXECUTED_VIA_AOT_C = 3,
    ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM = 4
} EZrLibraryExecutedVia;

/** @brief 在项目执行前配置 AOT 状态，并按模式向 core 注册模块加载回调。
 * @pre global->userData 已关联项目；项目结束时应调用 FreeProjectState 释放记录。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ConfigureGlobal(struct SZrGlobalState *global,
                                                            EZrLibraryProjectExecutionMode executionMode,
                                                            TZrBool requireAotPath);

/** @brief 由项目释放链撤销模块和函数的 GC pin；动态库交给 global 的 GC 后清理回调延迟关闭。 */
ZR_LIBRARY_API void ZrLibrary_AotRuntime_FreeProjectState(struct SZrState *state,
                                                          struct SZrLibrary_Project *project);

/** @brief 将实际执行后端转换为 CLI 可报告的稳定名称。 */
ZR_LIBRARY_API const TZrChar *ZrLibrary_AotRuntime_ExecutedViaName(EZrLibraryExecutedVia executedVia);

/** @brief 查询项目运行记录中的实际后端；未配置时返回 NONE。 */
ZR_LIBRARY_API EZrLibraryExecutedVia ZrLibrary_AotRuntime_GetExecutedVia(struct SZrGlobalState *global);

/** @brief 借用最近一次 AOT 诊断文字；下一次配置或失败会覆盖它。 */
ZR_LIBRARY_API const TZrChar *ZrLibrary_AotRuntime_GetLastError(struct SZrGlobalState *global);

/** @brief 记录生成代码即将抛出的稳定诊断文本；调用方随后负责进入 VM 异常链。 */
ZR_LIBRARY_API void ZrLibrary_AotRuntime_RecordError(struct SZrState *state, TZrNativeString message);

/** @brief 从已验证的 AOT 注册表按函数及局部序号定位 native import 契约，供桥接层核对。 */
ZR_LIBRARY_API const struct SZrNativeImportContract *
ZrLibrary_AotRuntime_ResolveNativeImportContract(
        const SZrAotCodeRegistration *codeRegistration,
        TZrUInt32 functionIndex,
        TZrUInt32 localContractIndex);

/** @brief FFI 从活动调用帧的 VM 元数据函数反查 native import 契约；结果借用项目记录。 */
ZR_LIBRARY_API const struct SZrNativeImportContract *
ZrLibrary_AotRuntime_FindNativeImportContract(
        struct SZrState *state,
        const struct SZrFunction *function,
        TZrUInt32 localContractIndex);

/** @brief core 模块导入回调：载入、校验并至多执行一次 AOT 模块，返回项目持有的模块对象。
 * @note 由 ConfigureGlobal 安装；userData 必须是该 global 的有效 AOT 状态。
 */
ZR_LIBRARY_API struct SZrObjectModule *ZrLibrary_AotRuntime_ModuleLoader(struct SZrState *state,
                                                                         struct SZrString *moduleName,
                                                                         TZrPtr userData);

/** @brief 项目执行路径的 AOT 入口；加载根模块并在异常边界内调用其入口 thunk。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ExecuteEntry(struct SZrState *state,
                                                         EZrAotBackendKind backendKind,
                                                         struct SZrTypeValue *result);

/** @brief 兼容 VM 对 native closure 的调用约定，将当前活动 AOT 记录转给生成入口。 */
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_InvokeActiveShim(struct SZrState *state,
                                                              EZrAotBackendKind backendKind);
/** @brief 在活动 AOT 记录中，以当前 closure 元数据函数进入 VM shim。 */
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_InvokeCurrentClosureShim(struct SZrState *state,
                                                                      EZrAotBackendKind backendKind);

/** @brief 生成函数序言的必经入口：校验函数索引，建立并锚定物理调用帧和稠密槽视图。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BeginGeneratedFunction(struct SZrState *state,
                                                                   TZrUInt32 functionIndex,
                                                                   ZrAotGeneratedFrame *frame);

/** @brief 在生成入口按当前 callInfo 解析模块、元数据函数与 thunk，防止跨记录索引漂移。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(struct SZrState *state,
                                                                          TZrUInt32 functionIndex,
                                                                          ZrAotGeneratedModuleContext *context);

/** @brief 生成器默认仅在可抛错、控制流、调用、返回指令同步可观察 PC。 */
ZR_FORCE_INLINE TZrUInt32 ZrLibrary_AotRuntime_DefaultObservationMask(void) {
    return ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW |
           ZR_AOT_GENERATED_STEP_FLAG_CONTROL_FLOW |
           ZR_AOT_GENERATED_STEP_FLAG_CALL |
           ZR_AOT_GENERATED_STEP_FLAG_RETURN;
}

/** @brief 由调试或测试调用链覆盖当前 state 的指令观察策略。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetObservationPolicy(struct SZrState *state,
                                                                 TZrUInt32 observationMask,
                                                                 TZrBool publishAllInstructions);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ResetObservationPolicy(struct SZrState *state);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetObservationPolicy(struct SZrState *state,
                                                                 TZrUInt32 *outObservationMask,
                                                                 TZrBool *outPublishAllInstructions);

/** @brief 生成指令的观察边界；维护异常 PC 和行调试钩子，并在栈移动后刷新帧视图。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BeginInstruction(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 instructionIndex,
                                                             TZrUInt32 stepFlags);

/** @brief 生成代码的值槽与常量操作入口；函数索引和槽位由已校验的生成帧约束。
 * @note CopyStack 与 GetStack 的保源和所有权语义不同，生成器须按 lowering 的 preserveSource 选择。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CopyConstant(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetConstant(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 sourceSlot,
                                                        TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CreateClosure(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetClosureValue(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 closureIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetClosureValue(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 sourceSlot,
                                                            TZrUInt32 closureIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CopyStack(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetStack(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ResetStackNull(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 destinationSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ResetStackNull2(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 firstSlot,
                                                            TZrUInt32 secondSlot);

/** @brief 把泛型值槽转成生成器使用的标量缓存；失败时由 AOT 诊断和 VM 状态承载原因。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ConvertGenericToBool(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ConvertGenericToInt(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ConvertGenericToUInt(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ConvertGenericToFloat(struct SZrState *state,
                                                                  ZrAotGeneratedFrame *frame,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 sourceSlot);

/** @brief 类型特化无法确定时的数值退路，仍从生成帧取值并保持解释器可见的异常状态。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericNumericAdd(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericNumericSub(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericNumericMul(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericNumericDiv(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericNumericMod(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericNumericNeg(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericPower(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot);

/** @brief 从 VM 物理值槽同步标量局部缓存；类型不匹配时保留调用方已有缓存。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SyncSignedIntLocal(struct SZrState *state,
                                                               ZrAotGeneratedFrame *frame,
                                                               TZrUInt32 sourceSlot,
                                                               TZrInt64 *outValue);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SyncUnsignedIntLocal(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 sourceSlot,
                                                                 TZrUInt64 *outValue);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SyncFloatLocal(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 sourceSlot,
                                                           TZrFloat64 *outValue);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SyncBoolLocal(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 sourceSlot,
                                                          TZrBool *outValue);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetGlobal(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetSubFunction(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 childFunctionIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetSubFunctionNativeClosure(struct SZrState *state,
                                                                        ZrAotGeneratedFrame *frame,
                                                                        TZrUInt32 destinationSlot,
                                                                        TZrUInt32 childFunctionIndex,
                                                                        TZrUInt32 callableFlatIndex,
                                                                        FZrAotEntryThunk nativeThunk);

/** @brief 生成器对象与内联数组构造入口；新对象创建后由 VM 值槽承担可达性。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CreateObject(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CreateArray(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CreateInlineArray(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 elementTypeLayoutId,
                                                              TZrUInt32 length);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BindInlineArrayElementPlace(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 arraySlot,
        TZrUInt32 indexSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_TypeOf(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToObject(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot,
                                                     TZrUInt32 typeNameConstantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToStruct(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot,
                                                     TZrUInt32 typeNameConstantIndex);

/** @brief 元成员访问复用已链接的 call-site/cache 信息；不匹配时由通用访问路径处理。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MetaGetCached(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 receiverSlot,
                                                          TZrUInt32 cacheIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MetaGet(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 memberId);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MetaSetCached(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 receiverAndResultSlot,
                                                          TZrUInt32 assignedValueSlot,
                                                          TZrUInt32 cacheIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MetaSet(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 receiverAndResultSlot,
                                                    TZrUInt32 assignedValueSlot,
                                                    TZrUInt32 memberId);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MetaGetStaticCached(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 receiverSlot,
                                                                TZrUInt32 cacheIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MetaSetStaticCached(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 receiverAndResultSlot,
                                                                TZrUInt32 assignedValueSlot,
                                                                TZrUInt32 cacheIndex);

/** @brief 生成代码的所有权指令适配层，必须保持 core Ownership 的借用、共享和释放约束。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnUnique(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnBorrow(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnLoan(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnReturnLoan(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnShare(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnDegrade(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnDetach(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnIntoGcBox(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnReturnToGc(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnWake(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnDrop(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot);

/** @brief 生成器比较、真值和条件跳转的通用入口；返回值反映运行时操作是否完成。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalEqual(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNotEqual(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(struct SZrState *state,
                                                                         ZrAotGeneratedFrame *frame,
                                                                         TZrUInt32 destinationSlot,
                                                                         TZrUInt32 leftSlot,
                                                                         TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual(struct SZrState *state,
                                                                            ZrAotGeneratedFrame *frame,
                                                                            TZrUInt32 destinationSlot,
                                                                            TZrUInt32 leftSlot,
                                                                            TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalEqualBool(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNotEqualBool(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 leftSlot,
                                                                TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalEqualSigned(struct SZrState *state,
                                                               ZrAotGeneratedFrame *frame,
                                                               TZrUInt32 destinationSlot,
                                                               TZrUInt32 leftSlot,
                                                               TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNotEqualSigned(struct SZrState *state,
                                                                  ZrAotGeneratedFrame *frame,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 leftSlot,
                                                                  TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalEqualUnsigned(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 leftSlot,
                                                                 TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNotEqualUnsigned(struct SZrState *state,
                                                                    ZrAotGeneratedFrame *frame,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 leftSlot,
                                                                    TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalEqualFloat(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNotEqualFloat(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 leftSlot,
                                                                 TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalEqualString(struct SZrState *state,
                                                               ZrAotGeneratedFrame *frame,
                                                               TZrUInt32 destinationSlot,
                                                               TZrUInt32 leftSlot,
                                                               TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNotEqualString(struct SZrState *state,
                                                                  ZrAotGeneratedFrame *frame,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 leftSlot,
                                                                  TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalLessSigned(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalLessUnsigned(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 leftSlot,
                                                                TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalLessFloat(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalGreaterSigned(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 leftSlot,
                                                                 TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalGreaterUnsigned(struct SZrState *state,
                                                                   ZrAotGeneratedFrame *frame,
                                                                   TZrUInt32 destinationSlot,
                                                                   TZrUInt32 leftSlot,
                                                                   TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalGreaterFloat(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 leftSlot,
                                                                TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalLessEqualSigned(struct SZrState *state,
                                                                   ZrAotGeneratedFrame *frame,
                                                                   TZrUInt32 destinationSlot,
                                                                   TZrUInt32 leftSlot,
                                                                   TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalLessEqualUnsigned(struct SZrState *state,
                                                                     ZrAotGeneratedFrame *frame,
                                                                     TZrUInt32 destinationSlot,
                                                                     TZrUInt32 leftSlot,
                                                                     TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalLessEqualFloat(struct SZrState *state,
                                                                  ZrAotGeneratedFrame *frame,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 leftSlot,
                                                                  TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalGreaterEqualSigned(struct SZrState *state,
                                                                      ZrAotGeneratedFrame *frame,
                                                                      TZrUInt32 destinationSlot,
                                                                      TZrUInt32 leftSlot,
                                                                      TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalGreaterEqualUnsigned(struct SZrState *state,
                                                                        ZrAotGeneratedFrame *frame,
                                                                        TZrUInt32 destinationSlot,
                                                                        TZrUInt32 leftSlot,
                                                                        TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalGreaterEqualFloat(struct SZrState *state,
                                                                     ZrAotGeneratedFrame *frame,
                                                                     TZrUInt32 destinationSlot,
                                                                     TZrUInt32 leftSlot,
                                                                     TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_IsTruthy(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 sourceSlot,
                                                     TZrBool *outTruthy);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(struct SZrState *state,
                                                                     ZrAotGeneratedFrame *frame,
                                                                     TZrUInt32 sourceSlot,
                                                                     TZrBool *outTruthy);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(struct SZrState *state,
                                                                       ZrAotGeneratedFrame *frame,
                                                                       TZrUInt32 destinationSlot,
                                                                       TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShouldJumpIfGreaterSigned(struct SZrState *state,
                                                                      ZrAotGeneratedFrame *frame,
                                                                      TZrUInt32 leftSlot,
                                                                      TZrUInt32 rightSlot,
                                                                      TZrBool *outShouldJump);
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShouldJumpIfLessEqualSigned(struct SZrState *state,
                                                                        ZrAotGeneratedFrame *frame,
                                                                        TZrUInt32 leftSlot,
                                                                        TZrUInt32 rightSlot,
                                                                        TZrBool *outShouldJump);
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShouldJumpIfNotEqualSigned(struct SZrState *state,
                                                                       ZrAotGeneratedFrame *frame,
                                                                       TZrUInt32 leftSlot,
                                                                       TZrUInt32 rightSlot,
                                                                       TZrBool *outShouldJump);
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShouldJumpIfNotEqualSignedConst(struct SZrState *state,
                                                                            ZrAotGeneratedFrame *frame,
                                                                            TZrUInt32 leftSlot,
                                                                            TZrUInt32 constantIndex,
                                                                            TZrBool *outShouldJump);

/** @brief VM 值槽上的算术指令入口；具体类型特化由生成器调用下列标量重载。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Add(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddFloat(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Sub(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubFloat(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Mul(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MulUnsigned(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MulFloat(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Div(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_DivUnsigned(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_DivFloat(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Mod(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ModSignedConst(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 leftSlot,
                                                           TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ModUnsigned(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ModUnsignedConst(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ModFloat(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddInt(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddIntConst(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddSigned(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddSignedConst(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 leftSlot,
                                                           TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddUnsigned(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddUnsignedConst(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubInt(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubIntConst(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubSigned(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubSignedConst(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 leftSlot,
                                                           TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubUnsigned(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SubUnsignedConst(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BitwiseXor(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BitwiseNot(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BitwiseAnd(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BitwiseOr(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BitwiseShiftLeft(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BitwiseShiftRight(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 leftSlot,
                                                              TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MulSigned(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MulSignedConst(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 leftSlot,
                                                            TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MulUnsignedConst(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_DivSigned(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_DivSignedConst(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 leftSlot,
                                                            TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_DivUnsignedConst(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Pow(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PowSigned(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PowUnsigned(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PowFloat(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShiftLeft(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShiftLeftInt(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShiftRight(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ShiftRightInt(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 leftSlot,
                                                          TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Neg(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalNot(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalAnd(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_LogicalOr(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToString(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot);

/** @brief 属性引用先创建位置对象，再由 Load/Store 与 core property-reference 语义衔接。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PropertyReferenceCreateMember(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 receiverSlot,
        TZrUInt32 memberEntryIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PropertyReferenceCreateIndex(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 receiverSlot,
        TZrUInt32 keySlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PropertyReferenceCreateLocal(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PropertyReferenceLoad(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 referenceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PropertyReferenceStore(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 sourceSlot,
        TZrUInt32 referenceSlot);

/** @brief 成员与索引访问复用 core 对象/数组边界；NewOwner 变体仅供生成器已证明新 owner 的写入点。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetMember(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 receiverSlot,
                                                      TZrUInt32 memberId);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetMember(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 sourceSlot,
                                                      TZrUInt32 receiverSlot,
                                                      TZrUInt32 memberId);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetMemberNewOwnerNoWriteBarrier(struct SZrState *state,
                                                                            ZrAotGeneratedFrame *frame,
                                                                            TZrUInt32 sourceSlot,
                                                                            TZrUInt32 receiverSlot,
                                                                            TZrUInt32 memberId);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetMemberSlot(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 receiverSlot,
                                                          TZrUInt32 cacheIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetMemberSlot(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 sourceSlot,
                                                          TZrUInt32 receiverSlot,
                                                          TZrUInt32 cacheIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetMemberSlotNewOwnerNoWriteBarrier(struct SZrState *state,
                                                                                ZrAotGeneratedFrame *frame,
                                                                                TZrUInt32 sourceSlot,
                                                                                TZrUInt32 receiverSlot,
                                                                                TZrUInt32 cacheIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetByIndex(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 receiverSlot,
                                                       TZrUInt32 keySlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetByIndex(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 sourceSlot,
                                                       TZrUInt32 receiverSlot,
                                                       TZrUInt32 keySlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetByIndexNewOwnerNoWriteBarrier(struct SZrState *state,
                                                                             ZrAotGeneratedFrame *frame,
                                                                             TZrUInt32 sourceSlot,
                                                                             TZrUInt32 receiverSlot,
                                                                             TZrUInt32 keySlot);

/** @brief 对 super array 的绑定项做类型特化访问；绑定句柄不得越过原数组和生成帧寿命。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayBindItems(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 receiverSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayGetIntBoundItems(struct SZrState *state,
                                                                       ZrAotGeneratedFrame *frame,
                                                                       TZrUInt32 destinationSlot,
                                                                       TZrUInt32 itemsSlot,
                                                                       TZrUInt32 keySlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArraySetIntBoundItems(struct SZrState *state,
                                                                       ZrAotGeneratedFrame *frame,
                                                                       TZrUInt32 sourceSlot,
                                                                       TZrUInt32 itemsSlot,
                                                                       TZrUInt32 keySlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayGetInt(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 receiverSlot,
                                                             TZrUInt32 keySlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArraySetInt(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 sourceSlot,
                                                             TZrUInt32 receiverSlot,
                                                             TZrUInt32 keySlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArraySetIntNewOwnerNoWriteBarrier(struct SZrState *state,
                                                                                   ZrAotGeneratedFrame *frame,
                                                                                   TZrUInt32 sourceSlot,
                                                                                   TZrUInt32 receiverSlot,
                                                                                   TZrUInt32 keySlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayAddInt(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 receiverSlot,
                                                             TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayAddInt4(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 receiverBaseSlot,
                                                              TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayAddInt4Const(struct SZrState *state,
                                                                   ZrAotGeneratedFrame *frame,
                                                                   TZrUInt32 receiverBaseSlot,
                                                                   TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SuperArrayFillInt4Const(struct SZrState *state,
                                                                    ZrAotGeneratedFrame *frame,
                                                                    TZrUInt32 receiverBaseSlot,
                                                                    TZrUInt32 countSlot,
                                                                    TZrUInt32 constantIndex);

/** @brief 迭代器指令的 VM 桥接，保留 MoveNext 与 Current 的异常和跳转语义。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_IterInit(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 iterableSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_IterMoveNext(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 iteratorSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_IterCurrent(struct SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 iteratorSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(struct SZrState *state,
                                                                    ZrAotGeneratedFrame *frame,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 iteratorSlot,
                                                                    TZrBool *outJumpIfFalse);

/** @brief 生成代码的通用调用退路；已准备的 AOT 直接调用通过后续 Prepare/Finish 协议执行。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Call(struct SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 functionSlot,
                                                 TZrUInt32 argumentCount);

/** @brief 从当前模块的泛型字典解析类型布局；生成静态缓存只可复用同一元数据运行时的结果。 */
ZR_LIBRARY_API const struct SZrTypeLayout *ZrLibrary_AotRuntime_GenericSlot_TypeLayout(
        struct SZrState *state,
        const SZrAotGenericDictionary *dictionary,
        struct SZrMetadataRuntime *metadataRuntime,
        TZrUInt32 slotIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GenericSlot_TryGetSizeOf(
        struct SZrState *state,
        const SZrAotGenericDictionary *dictionary,
        struct SZrMetadataRuntime *metadataRuntime,
        TZrUInt32 slotIndex,
        TZrSize *outSize);

ZR_LIBRARY_API FZrAotEntryThunk ZrLibrary_AotRuntime_GenericSlot_Method(
        struct SZrState *state,
        const SZrAotGenericDictionary *dictionary,
        const struct SZrFunction *metadataFunction,
        TZrUInt32 slotIndex);

/** @brief 完成已准备的 AOT thunk 或回退普通调用；Resume 变体还把异常恢复位置交回生成器。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallPreparedOrGeneric(struct SZrState *state,
                                                                  ZrAotGeneratedFrame *frame,
                                                                  ZrAotGeneratedDirectCall *directCall,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 functionSlot,
                                                                  TZrUInt32 argumentCount,
                                                                  TZrUInt32 resultCount);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallPreparedOrGenericWithResume(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        ZrAotGeneratedDirectCall *directCall,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 resultCount,
        TZrUInt32 *outResumeInstructionIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CompletePreparedDirectCallWithResume(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        ZrAotGeneratedDirectCall *directCall,
        TZrBool invocationSucceeded,
        TZrUInt32 resultCount,
        TZrUInt32 *outResumeInstructionIndex);

/** @brief 动态调用/deopt 桥接的 VM 值槽调用入口，须在可能扩栈前保存并恢复栈锚点。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallStackValue(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 functionSlot,
                                                           TZrUInt32 argumentCount,
                                                           const TZrChar *errorLabel);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallSpread(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 prefixArgumentCount,
        const TZrChar *errorLabel);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CanUseTypedDirectCall(struct SZrState *state,
                                                                  ZrAotGeneratedFrame *frame,
                                                                  TZrUInt32 calleeFunctionIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_DeoptTypedDirectCall(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 argumentCount,
                                                                 TZrUInt32 calleeFunctionIndex,
                                                                 const TZrChar *errorLabel);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallDynamicDeoptBridge(struct SZrState *state,
                                                                    ZrAotGeneratedFrame *frame,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 functionSlot,
                                                                    TZrUInt32 argumentCount,
                                                                    TZrUInt32 deoptId,
                                                                    const TZrChar *errorLabel);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(struct SZrState *state,
                                                                       ZrAotGeneratedFrame *frame,
                                                                       TZrUInt32 deoptId,
                                                                       const TZrChar *errorLabel);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_UnsupportedMetaCall(struct SZrState *state,
                                                                ZrAotGeneratedFrame *frame,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 receiverSlot,
                                                                TZrUInt32 argumentCount,
                                                                const TZrChar *errorLabel);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(struct SZrState *state,
                                                                       ZrAotGeneratedFrame *frame,
                                                                       TZrUInt32 primarySlot,
                                                                       TZrUInt32 secondarySlot,
                                                                       TZrUInt32 memberOrCacheIndex,
                                                                       const TZrChar *opcodeName);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_UnsupportedDynamicValueAccess(struct SZrState *state,
                                                                          ZrAotGeneratedFrame *frame,
                                                                          TZrUInt32 primarySlot,
                                                                          TZrUInt32 secondarySlot,
                                                                          TZrUInt32 operandIndex,
                                                                          const TZrChar *opcodeName);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallStaticDirect(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 functionSlot,
                                                             TZrUInt32 argumentCount,
                                                             TZrUInt32 calleeFunctionIndex,
                                                             FZrAotEntryThunk calleeThunk);

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

/** @brief 直接调用准备阶段：用当前指令绑定解析元数据与 thunk，并建立 VM callee 帧。
 * @note 调用方先检查 directCall.prepared，再决定执行 nativeFunction 或普通调用。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PrepareDirectCall(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 functionSlot,
                                                              TZrUInt32 argumentCount,
                                                              ZrAotGeneratedDirectCall *directCall);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PrepareMetaCall(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 receiverSlot,
                                                            TZrUInt32 argumentCount,
                                                            ZrAotGeneratedDirectCall *directCall);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PrepareStaticDirectCall(struct SZrState *state,
                                                                    ZrAotGeneratedFrame *frame,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 functionSlot,
                                                                    TZrUInt32 argumentCount,
                                                                    TZrUInt32 calleeFunctionIndex,
                                                                    ZrAotGeneratedDirectCall *directCall);

/** @brief 直接调用完成阶段：关闭 callee upvalue，执行 VM PostCall，恢复 caller 帧和结果所有权。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_FinishDirectCall(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             ZrAotGeneratedDirectCall *directCall,
                                                             TZrUInt32 resultCount);

/** @brief 生成代码异常区间与 pending control 的协议入口；handlerIndex 必须来自当前函数元数据。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Try(struct SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 handlerIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_EndTry(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 handlerIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Throw(struct SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 sourceSlot,
                                                  TZrUInt32 *outResumeInstructionIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_RequireNonNull(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 sourceSlot,
                                                           TZrUInt32 *outResumeInstructionIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_IsNull(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 sourceSlot,
                                                   TZrBool *outIsNull);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Catch(struct SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_EndFinally(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 handlerIndex,
                                                       TZrUInt32 *outResumeInstructionIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetPendingReturn(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 sourceSlot,
                                                             TZrUInt32 targetInstructionIndex,
                                                             TZrUInt32 *outResumeInstructionIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetPendingBreak(struct SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 targetInstructionIndex,
                                                            TZrUInt32 *outResumeInstructionIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetPendingContinue(struct SZrState *state,
                                                               ZrAotGeneratedFrame *frame,
                                                               TZrUInt32 targetInstructionIndex,
                                                               TZrUInt32 *outResumeInstructionIndex);

/** @brief 离开作用域前登记并关闭待清理值；双槽表示的所有权需由 cleanup registration 同步。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MarkToBeClosed(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 slotIndex);

/** @brief Register a high physical proxy for an existing dense local. */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MarkCloseProxy(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 proxySlot,
                                                           TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CloseScope(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 cleanupCount);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToBool(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToInt(struct SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToUInt(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToFloat(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 sourceSlot);

/** @brief 模块入口完成前发布生成模块导出，使后续 import 可复用同一 module 对象。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PublishModuleExports(struct SZrState *state,
                                                                 ZrAotGeneratedFrame *frame);

/** @brief 生成函数返回协议；关闭作用域和 upvalue 后把返回值交给 caller 调用帧。 */
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_Return(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 sourceSlot,
                                                    TZrBool publishExports);
/** @brief 标量返回的直接写回入口；调用者须处于有效 generated callInfo。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ReturnI64(struct SZrState *state, TZrInt64 value);
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ReturnBool(struct SZrState *state, TZrBool value);
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ReturnU64(struct SZrState *state, TZrUInt64 value);
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ReturnF64(struct SZrState *state, TZrFloat64 value);
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ReturnInlineStruct(struct SZrState *state,
                                                               ZrAotGeneratedFrame *frame,
                                                               TZrUInt32 sourceSlot,
                                                               TZrUInt32 sourceTypeLayoutId,
                                                               TZrUInt32 sourceByteOffset,
                                                               TZrUInt32 sourceByteSize,
                                                               TZrUInt32 *outSkipDropSlot);

/** @brief 生成器无法表达的指令统一报告 VM 运行错误，避免静默执行错误代码。 */
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_ReportUnsupportedInstruction(struct SZrState *state,
                                                                          TZrUInt32 functionIndex,
                                                                          TZrUInt32 instructionIndex,
                                                                          TZrUInt32 opcode);

ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_FailGeneratedFunction(struct SZrState *state,
                                                                   const ZrAotGeneratedFrame *frame);
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_FailGeneratedFunctionAt(
        struct SZrState *state,
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 functionIndex);

#endif
