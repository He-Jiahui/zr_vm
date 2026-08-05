#ifndef ZR_VM_DEBUG_BREAKPOINT_CONDITION_H
#define ZR_VM_DEBUG_BREAKPOINT_CONDITION_H

#include "debug_internal.h"

ZR_DEBUG_API TZrBool zr_debug_breakpoint_condition_evaluate(ZrDebugAgent *agent,
                                                            const TZrChar *condition,
                                                            TZrBool *outSatisfied,
                                                            TZrChar *errorBuffer,
                                                            TZrSize errorBufferSize);

#endif // ZR_VM_DEBUG_BREAKPOINT_CONDITION_H
