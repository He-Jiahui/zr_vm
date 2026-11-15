#ifndef ZR_VM_CORE_FUNCTION_REST_PARAMETERS_H
#define ZR_VM_CORE_FUNCTION_REST_PARAMETERS_H

#include "zr_vm_core/function.h"

/* The active call window roots the inputs. On success the returned array is
 * temporarily rooted until the caller installs it in the callee frame. */
struct SZrObject *function_pack_rest_parameters(struct SZrState *state,
                                               TZrStackValuePointer callBase,
                                               TZrSize fixedCount,
                                               TZrSize actualCount);

void function_bind_rest_parameter(struct SZrState *state,
                                  const SZrFunction *function,
                                  TZrStackValuePointer frameBase,
                                  struct SZrObject *array);

#endif
