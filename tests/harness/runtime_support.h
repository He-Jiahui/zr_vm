#ifndef ZR_VM_TESTS_RUNTIME_SUPPORT_H
#define ZR_VM_TESTS_RUNTIME_SUPPORT_H

#include <stdio.h>
#include "zr_test_log_macros.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

TZrPtr ZrTests_Runtime_Allocator_Default(TZrPtr userData,
                                         TZrPtr pointer,
                                         TZrSize originalSize,
                                         TZrSize newSize,
                                         TZrInt64 flag);

typedef void (*FZrTestsRuntimeFatalCrashHook)(SZrState *state);

SZrState *ZrTests_Runtime_State_Create(FZrPanicHandlingFunction panicHandler);

void ZrTests_Runtime_State_Destroy(SZrState *state);

void ZrTests_Runtime_SetFatalCrashHook(FZrTestsRuntimeFatalCrashHook hook);

TZrBool ZrTests_Runtime_Function_ExecuteCaptureFailure(SZrState *state, SZrFunction *function, SZrTypeValue *result);

TZrBool ZrTests_Runtime_Function_Execute(SZrState *state, SZrFunction *function, SZrTypeValue *result);

TZrBool ZrTests_Runtime_Function_ExecuteExpectInt64(SZrState *state, SZrFunction *function, TZrInt64 *result);

void ZrTests_Runtime_CrashScope_Begin(SZrState *state);

void ZrTests_Runtime_CrashScope_End(SZrState *state);

SZrState *ZrTests_Runtime_CrashState_Current(void);

void ZrTests_Runtime_ClearCrashState(void);

TZrBool ZrTests_Runtime_ReportCrashState(FILE *stream, TZrBool *printedExceptionStack);

#endif
