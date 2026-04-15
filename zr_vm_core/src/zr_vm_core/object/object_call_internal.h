#ifndef ZR_VM_CORE_OBJECT_CALL_INTERNAL_H
#define ZR_VM_CORE_OBJECT_CALL_INTERNAL_H

#include "object/object_internal.h"

TZrBool ZrCore_Object_CallValue(SZrState *state,
                                const SZrTypeValue *callable,
                                const SZrTypeValue *receiver,
                                const SZrTypeValue *arguments,
                                TZrSize argumentCount,
                                SZrTypeValue *result);

TZrBool ZrCore_Object_CallFunctionWithReceiver(SZrState *state,
                                               struct SZrFunction *function,
                                               SZrTypeValue *receiver,
                                               const SZrTypeValue *arguments,
                                               TZrSize argumentCount,
                                               SZrTypeValue *result);

#endif
