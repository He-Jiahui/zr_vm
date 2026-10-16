/* CString field writes share Core's complete key-creation/root transaction. */
#include "zr_vm_library/native_binding.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"

TZrBool ZrLib_Object_TrySetFieldCString(SZrState *state, SZrObject *object,
                                      const TZrChar *fieldName, const SZrTypeValue *value) {
    return ZrCore_Object_TrySetFieldCString(state, object, fieldName, value);
}

void ZrLib_Object_SetFieldCString(SZrState *state, SZrObject *object,
                                 const TZrChar *fieldName, const SZrTypeValue *value) {
    if (!ZrLib_Object_TrySetFieldCString(state, object, fieldName, value) && state != ZR_NULL) {
        ZrCore_Exception_Throw(state, state->threadStatus);
    }
}
