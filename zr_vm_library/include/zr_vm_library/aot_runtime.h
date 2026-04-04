#ifndef ZR_VM_LIBRARY_AOT_RUNTIME_H
#define ZR_VM_LIBRARY_AOT_RUNTIME_H

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_library/conf.h"

struct SZrGlobalState;
struct SZrObjectModule;
struct SZrLibrary_Project;
struct SZrState;
struct SZrFunction;
struct SZrString;
struct SZrTypeValue;

typedef struct ZrAotGeneratedFrame {
    TZrPtr recordHandle;
    struct SZrFunction *function;
    TZrStackValuePointer slotBase;
    TZrUInt32 functionIndex;
} ZrAotGeneratedFrame;

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

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CopyConstant(struct SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CreateClosure(struct SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 constantIndex);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_CopyStack(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 sourceSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_AddInt(struct SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_GetMember(struct SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 receiverSlot,
                                                      TZrUInt32 memberId);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_Call(struct SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 functionSlot,
                                                 TZrUInt32 argumentCount);

ZR_LIBRARY_API TZrBool ZrLibrary_AotRuntime_ToInt(struct SZrState *state,
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

#endif
