#ifndef ZR_VM_CORE_FUNCTION_IDENTITY_H
#define ZR_VM_CORE_FUNCTION_IDENTITY_H

#include "zr_vm_core/function.h"

ZR_CORE_API TZrBool ZrCore_Function_HasSameDefinition(const SZrFunction *left, const SZrFunction *right);
ZR_CORE_API TZrBool ZrCore_Function_FindConstantChildAlias(const SZrFunction *function,
        TZrUInt32 constantIndex, TZrUInt32 *childIndex, TZrBool *hasDefinition);

#endif
