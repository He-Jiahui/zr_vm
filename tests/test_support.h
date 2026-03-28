#ifndef ZR_VM_TEST_SUPPORT_H
#define ZR_VM_TEST_SUPPORT_H

#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

TZrPtr ZrTests_Allocator_Default(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag);

SZrState* ZrTests_State_Create(FZrPanicHandlingFunction panicHandler);

void ZrTests_State_Destroy(SZrState* state);

TZrBool ZrTests_Function_Execute(SZrState* state, SZrFunction* function, SZrTypeValue* result);

TZrBool ZrTests_Function_ExecuteExpectInt64(SZrState* state, SZrFunction* function, TZrInt64* result);

#endif
