#ifndef ZR_VM_CORE_CLOSURE_CLOSE_PROXY_TOKEN_H
#define ZR_VM_CORE_CLOSURE_CLOSE_PROXY_TOKEN_H

#include "zr_vm_core/stack.h"

struct SZrState;

/* Private runtime token. Neither the tag nor its representation is an artifact ABI. */
TZrBool ZrCore_ClosureProxyToken_Install(struct SZrState *state,
                                        TZrStackValuePointer proxySlot,
                                        TZrStackValuePointer sourceSlot);
TZrBool ZrCore_ClosureProxyToken_GetSourceOffset(struct SZrState *state,
                                                TZrStackValuePointer proxySlot,
                                                TZrMemoryOffset *outSourceOffset);

#endif
