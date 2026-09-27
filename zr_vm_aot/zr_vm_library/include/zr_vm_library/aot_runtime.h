#ifndef ZR_VM_LIBRARY_AOT_RUNTIME_H
#define ZR_VM_LIBRARY_AOT_RUNTIME_H

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/state.h"
#include "zr_vm_library/conf.h"

struct SZrGlobalState;
struct SZrObjectModule;
struct SZrLibrary_Project;
struct SZrFunction;
struct SZrString;
struct SZrTypeValue;

/** @brief 生成代码向运行时声明指令的可观察副作用，供调试和异常状态同步。 */
typedef enum EZrAotGeneratedStepFlag {
    ZR_AOT_GENERATED_STEP_FLAG_NONE = 0,
    ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW = 1u << 0,
    ZR_AOT_GENERATED_STEP_FLAG_CONTROL_FLOW = 1u << 1,
    ZR_AOT_GENERATED_STEP_FLAG_CALL = 1u << 2,
    ZR_AOT_GENERATED_STEP_FLAG_RETURN = 1u << 3
} EZrAotGeneratedStepFlag;

// 例外控制流未指定目标时的哨兵，生成 thunk 应继续顺序执行下一条指令。
#define ZR_AOT_RUNTIME_RESUME_FALLTHROUGH ((TZrUInt32)0xFFFFFFFFu)

/**
 * @brief 一次生成函数执行的 VM 帧视图；BeginGeneratedFunction 建立，后续 helper 共用。
 * @note recordHandle 指向加载器记录，slotBase 指向可搬移的 VM 栈；发生调用或扩栈后
 *       必须以 callInfo 重新取得槽地址。指令索引和观察策略供异常恢复与调试使用。
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
} ZrAotGeneratedFrame;

/**
 * @brief 直接本机调用的准备结果，由 Prepare* 与 FinishDirectCall 成对消费。
 * @note calleeCallInfo 只在 prepared 期间有效；调用失败也须按约定清理 VM 帧。
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

/** @brief 项目入口选用的执行模式，由项目配置传给 ConfigureGlobal。 */
typedef enum EZrLibraryProjectExecutionMode {
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_INTERP = 0,
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_BINARY = 1,
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C = 2,
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM = 3
} EZrLibraryProjectExecutionMode;

/** @brief 最近实际执行路径，供项目层报告与严格 AOT 路径核验。 */
typedef enum EZrLibraryExecutedVia {
    ZR_LIBRARY_EXECUTED_VIA_NONE = 0,
    ZR_LIBRARY_EXECUTED_VIA_INTERP = 1,
    ZR_LIBRARY_EXECUTED_VIA_BINARY = 2,
    ZR_LIBRARY_EXECUTED_VIA_AOT_C = 3,
    ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM = 4
} EZrLibraryExecutedVia;

/** @brief 为项目安装模块加载回调及执行策略；需在入口执行和 import 之前调用。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ConfigureGlobal(struct SZrGlobalState *global,
                                                            EZrLibraryProjectExecutionMode executionMode,
                                                            TZrBool requireAotPath);

/** @brief 项目退出时释放加载记录，并延迟卸载仍可能被 GC 引用的动态库。 */
ZR_LIBRARY_API void ZrLibrary_AotRuntime_FreeProjectState(struct SZrState *state,
                                                          struct SZrLibrary_Project *project);

ZR_LIBRARY_API const TZrChar *ZrLibrary_AotRuntime_ExecutedViaName(EZrLibraryExecutedVia executedVia);

ZR_LIBRARY_API EZrLibraryExecutedVia ZrLibrary_AotRuntime_GetExecutedVia(struct SZrGlobalState *global);

ZR_LIBRARY_API const TZrChar *ZrLibrary_AotRuntime_GetLastError(struct SZrGlobalState *global);

/** @brief import 回调：按项目策略查找、验证并执行模块，返回可供 import 复用的模块对象。 */
ZR_LIBRARY_API struct SZrObjectModule *ZrLibrary_AotRuntime_ModuleLoader(struct SZrState *state,
                                                                         struct SZrString *moduleName,
                                                                         TZrPtr userData);

/** @brief 运行项目入口的指定 AOT 后端，并把入口结果交还项目层。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ExecuteEntry(struct SZrState *state,
                                                         EZrAotBackendKind backendKind,
                                                         struct SZrTypeValue *result);

ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_InvokeActiveShim(struct SZrState *state,
                                                              EZrAotBackendKind backendKind);
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_InvokeCurrentClosureShim(struct SZrState *state,
                                                                      EZrAotBackendKind backendKind);

/** @brief 生成 thunk 首先调用此函数，将函数索引绑定到活动加载记录和 VM 调用帧。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BeginGeneratedFunction(struct SZrState *state,
                                                                   TZrUInt32 functionIndex,
                                                                   ZrAotGeneratedFrame *frame);

/** @brief 默认只公布可能影响异常、跳转、调用或返回的指令观察点。 */
ZR_FORCE_INLINE TZrUInt32 ZrLibrary_AotRuntime_DefaultObservationMask(void) {
    return ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW |
           ZR_AOT_GENERATED_STEP_FLAG_CONTROL_FLOW |
           ZR_AOT_GENERATED_STEP_FLAG_CALL |
           ZR_AOT_GENERATED_STEP_FLAG_RETURN;
}

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetObservationPolicy(struct SZrState *state,
                                                                 TZrUInt32 observationMask,
                                                                 TZrBool publishAllInstructions);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ResetObservationPolicy(struct SZrState *state);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetObservationPolicy(struct SZrState *state,
                                                                 TZrUInt32 *outObservationMask,
                                                                 TZrBool *outPublishAllInstructions);

/** @brief 在每条生成指令前按观察策略公布位置，并处理待恢复的 VM 状态。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BeginInstruction(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 instructionIndex,
                                                             TZrUInt32 stepFlags);

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

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetGlobal(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetSubFunction(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 childFunctionIndex);

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

/** @brief 将独占资源转为可由 GC 持有的结果；需与解释器 OWN_DETACH 的两阶段转换保持一致。 */
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

/** @brief 通用 DIV 的动态元方法路径，零除语义须与解释器通用 DIV 核对。 */
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

/** @brief 把对象成员绑定为可传递的 place；之后 Load/Store 才真正访问属性。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PropertyReferenceCreateMember(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 receiverSlot,
        TZrUInt32 memberEntryIndex);

/** @brief 把对象和键绑定为索引 place；键及接收者须满足核心引用生命周期约束。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PropertyReferenceCreateIndex(
        struct SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 receiverSlot,
        TZrUInt32 keySlot);

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

/** @brief 只允许未发布的新 owner 使用的成员写入路径，生成器须先证明无需 GC 写屏障。 */
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

/** @brief 新建 owner 的索引写入特化；逃逸后的对象必须走有写屏障变体。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_SetByIndexNewOwnerNoWriteBarrier(struct SZrState *state,
                                                                             ZrAotGeneratedFrame *frame,
                                                                             TZrUInt32 sourceSlot,
                                                                             TZrUInt32 receiverSlot,
                                                                             TZrUInt32 keySlot);

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

/** @brief 迭代协议入口；对象可触发自定义方法，结果槽在核心调用层通过栈锚点保护。 */
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

/** @brief 无法直接绑定本机 thunk 时调用通用 VM callable；调用后栈槽地址必须重取。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Call(struct SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 functionSlot,
                                                 TZrUInt32 argumentCount);

/**
 * @brief 消费已准备的直接调用，或退回通用 VM 调用。
 * @note BUG: 缺少 CallPreparedOrGenericWithResume 只是归档 API 与当前生成器不兼容的一例；
 *       重接本头和实现前须对齐完整 helper 集合，否则生成 C/LLVM 工件会编译或链接失败。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CallPreparedOrGeneric(struct SZrState *state,
                                                                  ZrAotGeneratedFrame *frame,
                                                                  ZrAotGeneratedDirectCall *directCall,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 functionSlot,
                                                                  TZrUInt32 argumentCount,
                                                                  TZrUInt32 resultCount);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_PrepareDirectCall(struct SZrState *state,
                                                              ZrAotGeneratedFrame *frame,
                                                              TZrUInt32 destinationSlot,
                                                              TZrUInt32 functionSlot,
                                                              TZrUInt32 argumentCount,
                                                              ZrAotGeneratedDirectCall *directCall);

/** @brief 为 @call 注入接收者并尝试绑定直接 thunk，失败时交给通用调用。 */
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

/** @brief 在生成 thunk 返回后关闭捕获并恢复调用方 VM 帧；只能消费 prepared 记录。 */
ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_FinishDirectCall(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             ZrAotGeneratedDirectCall *directCall,
                                                             TZrUInt32 resultCount);

/** @brief 将生成代码的异常处理索引压入当前 VM 调用帧。 */
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

/** @brief finally 收尾后返回异常或延迟控制流的恢复指令索引。 */
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

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_MarkToBeClosed(struct SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 slotIndex);

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

/** @brief 生成函数的返回边界；入口模块可在此发布导出，返回前应完成逃逸与闭包收尾。 */
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_Return(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 sourceSlot,
                                                    TZrBool publishExports);

ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_ReportUnsupportedInstruction(struct SZrState *state,
                                                                          TZrUInt32 functionIndex,
                                                                          TZrUInt32 instructionIndex,
                                                                          TZrUInt32 opcode);

ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_FailGeneratedFunction(struct SZrState *state,
                                                                   const ZrAotGeneratedFrame *frame);

/* BUG: 当前仍编译的 backend_aot 会生成本归档头及实现未覆盖的 runtime 调用；
 * CallStackValue、CompletePreparedDirectCallWithResume、ReturnI64、ResetStackNull、
 * ResolveGeneratedModuleContext、CallPreparedOrGenericWithResume 只是已核实的示例。
 * 根构建链接现役 runtime；重接归档版本前须核对完整生成符号集与 ABI。 */
#endif
