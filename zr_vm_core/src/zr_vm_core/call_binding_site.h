#ifndef ZR_VM_CORE_CALL_BINDING_SITE_H
#define ZR_VM_CORE_CALL_BINDING_SITE_H

#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"

TZrBool zr_call_binding_site_matches(const SZrFunction *function, TZrUInt32 cacheIndex);
TZrBool zr_call_binding_descriptor_matches(const SZrFunction *function,
        const SZrFunctionCallSiteCacheEntry *entry, const SZrMemberDescriptor *descriptor);

#endif
