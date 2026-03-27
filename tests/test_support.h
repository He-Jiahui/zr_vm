#ifndef ZR_VM_TEST_SUPPORT_H
#define ZR_VM_TEST_SUPPORT_H

#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

TZrPtr zr_test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TInt64 flag);

SZrState* zr_test_create_state(FZrPanicHandlingFunction panicHandler);

void zr_test_destroy_state(SZrState* state);

TBool zr_test_execute_function(SZrState* state, SZrFunction* function, SZrTypeValue* result);

TBool zr_test_execute_function_expect_int64(SZrState* state, SZrFunction* function, TInt64* result);

#endif
