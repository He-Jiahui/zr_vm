#ifndef ZR_VM_LIBRARY_AOT_TYPED_CALL_BINDING_H
#define ZR_VM_LIBRARY_AOT_TYPED_CALL_BINDING_H

#include "zr_vm_library/aot_runtime.h"

TZrBool aot_prepare_call_binding(SZrState *state, ZrAotGeneratedFrame *frame,
                                SZrTypeValue *callable);
TZrBool aot_prepare_meta_binding(SZrState *state, ZrAotGeneratedFrame *frame,
        const SZrTypeValue *receiver, SZrTypeValue *callable, TZrBool *hasBinding);

#endif
