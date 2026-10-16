#ifndef ZR_VM_CORE_EXCEPTION_ERROR_INTERNAL_H
#define ZR_VM_CORE_EXCEPTION_ERROR_INTERNAL_H

#include "zr_vm_core/exception.h"

struct SZrObject;

/* Borrowed result: callers must root the containing object across allocations. */
const SZrTypeValue *exception_get_object_field_cstring(struct SZrState *state,
                                                     struct SZrObject *object,
                                                     const TZrChar *fieldName);
TZrBool exception_value_is_error_object(struct SZrState *state, const SZrTypeValue *value);

#endif
