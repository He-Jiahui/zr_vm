//
// zr.ffi runtime helpers and native callbacks.
//

#ifndef ZR_VM_LIB_FFI_RUNTIME_H
#define ZR_VM_LIB_FFI_RUNTIME_H

#include "zr_vm_library.h"

const ZrLibModuleDescriptor *ZrVmLibFfiRuntime_GetModuleDescriptor(void);

TZrBool ZrFfi_LoadLibrary(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_CreateCallback(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_SizeOf(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_AlignOf(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_NullPointer(ZrLibCallContext *context, SZrTypeValue *result);

TZrBool ZrFfi_Library_Close(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_Library_IsClosed(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_Library_GetSymbol(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_Library_GetVersion(ZrLibCallContext *context, SZrTypeValue *result);

TZrBool ZrFfi_Symbol_Call(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_Symbol_MetaCall(ZrLibCallContext *context, SZrTypeValue *result);

TZrBool ZrFfi_Callback_Close(ZrLibCallContext *context, SZrTypeValue *result);

TZrBool ZrFfi_Pointer_As(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_Pointer_Read(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_Pointer_Close(ZrLibCallContext *context, SZrTypeValue *result);

TZrBool ZrFfi_Buffer_Allocate(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_Buffer_Close(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_Buffer_Pin(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_Buffer_Read(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_Buffer_Write(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrFfi_Buffer_Slice(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_FFI_RUNTIME_H
