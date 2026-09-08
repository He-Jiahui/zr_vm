#ifndef ZR_VM_PARSER_COMPILER_NATIVE_CALL_BINDING_H
#define ZR_VM_PARSER_COMPILER_NATIVE_CALL_BINDING_H

#include "zr_vm_parser/compiler.h"

/* Native descriptors are linked by provider hash and metadata identity.  A
 * non-zero module hash is the explicit marker that no local function constant
 * may be synthesized for this member. */
TZrBool compiler_native_call_binding_is_provider_contract(
        const SZrCallBindingContract *contract);

TZrBool compiler_native_call_binding_prepare_cache(
        SZrFunctionCallSiteCacheEntry *entry,
        const SZrCallBindingContract *contract,
        TZrUInt32 memberEntryIndex);

TZrBool compiler_finalize_native_call_binding(
        SZrCompilerState *compiler, SZrFunctionCallSiteCacheEntry *entry);

#endif
