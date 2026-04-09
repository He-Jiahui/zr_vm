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

typedef enum EZrAotGeneratedStepFlag {
    ZR_AOT_GENERATED_STEP_FLAG_NONE = 0,
    ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW = 1u << 0,
    ZR_AOT_GENERATED_STEP_FLAG_CONTROL_FLOW = 1u << 1,
    ZR_AOT_GENERATED_STEP_FLAG_CALL = 1u << 2,
    ZR_AOT_GENERATED_STEP_FLAG_RETURN = 1u << 3
} EZrAotGeneratedStepFlag;

#define ZR_AOT_RUNTIME_RESUME_FALLTHROUGH ((TZrUInt32)0xFFFFFFFFu)

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
    TZrBool publishAllInstructions;
} ZrAotGeneratedFrame;

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

typedef enum EZrLibraryProjectExecutionMode {
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_INTERP = 0,
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_BINARY = 1,
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C = 2,
    ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM = 3
} EZrLibraryProjectExecutionMode;

typedef enum EZrLibraryExecutedVia {
    ZR_LIBRARY_EXECUTED_VIA_NONE = 0,
    ZR_LIBRARY_EXECUTED_VIA_INTERP = 1,
    ZR_LIBRARY_EXECUTED_VIA_BINARY = 2,
    ZR_LIBRARY_EXECUTED_VIA_AOT_C = 3,
    ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM = 4
} EZrLibraryExecutedVia;

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ConfigureGlobal(struct SZrGlobalState *global,
                                                            EZrLibraryProjectExecutionMode executionMode,
                                                            TZrBool requireAotPath);

ZR_LIBRARY_API void ZrLibrary_AotRuntime_FreeProjectState(struct SZrState *state,
                                                          struct SZrLibrary_Project *project);

ZR_LIBRARY_API const TZrChar *ZrLibrary_AotRuntime_ExecutedViaName(EZrLibraryExecutedVia executedVia);

ZR_LIBRARY_API EZrLibraryExecutedVia ZrLibrary_AotRuntime_GetExecutedVia(struct SZrGlobalState *global);

ZR_LIBRARY_API const TZrChar *ZrLibrary_AotRuntime_GetLastError(struct SZrGlobalState *global);

ZR_LIBRARY_API struct SZrObjectModule *ZrLibrary_AotRuntime_ModuleLoader(struct SZrState *state,
                                                                         struct SZrString *moduleName,
                                                                         TZrPtr userData);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ExecuteEntry(struct SZrState *state,
                                                         EZrAotBackendKind backendKind,
                                                         struct SZrTypeValue *result);

ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_InvokeActiveShim(struct SZrState *state,
                                                              EZrAotBackendKind backendKind);
ZR_LIBRARY_API TZrInt64 ZrLibrary_AotRuntime_InvokeCurrentClosureShim(struct SZrState *state,
                                                                      EZrAotBackendKind backendKind);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_BeginGeneratedFunction(struct SZrState *state,
                                                                   TZrUInt32 functionIndex,
                                                                   ZrAotGeneratedFrame *frame);

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

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnShare(struct SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnWeak(struct SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnDetach(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnUpgrade(struct SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_OwnRelease(struct SZrState *state,
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

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Call(struct SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 functionSlot,
                                                 TZrUInt32 argumentCount);

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

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_FinishDirectCall(struct SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             ZrAotGeneratedDirectCall *directCall,
                                                             TZrUInt32 resultCount);

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

#endif
