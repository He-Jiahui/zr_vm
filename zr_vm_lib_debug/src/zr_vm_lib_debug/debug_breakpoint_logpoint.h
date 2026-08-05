#ifndef ZR_VM_DEBUG_BREAKPOINT_LOGPOINT_H
#define ZR_VM_DEBUG_BREAKPOINT_LOGPOINT_H

#include "debug_internal.h"

ZR_DEBUG_API TZrBool zr_debug_breakpoint_logpoint_format(ZrDebugAgent *agent,
                                                         const TZrChar *logMessage,
                                                         TZrChar *outText,
                                                         TZrSize outTextSize);

#endif // ZR_VM_DEBUG_BREAKPOINT_LOGPOINT_H
