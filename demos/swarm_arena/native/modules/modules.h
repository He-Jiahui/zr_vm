#ifndef ZRG_MODULES_H
#define ZRG_MODULES_H

#include "zr_vm_library/native_binding.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"

const ZrLibModuleDescriptor *ZrGame_HostModule(void);
const ZrLibModuleDescriptor *ZrGame_GfxModule(void);

/* Return through the descriptor dispatcher so ZR try/catch can handle input errors. */
static inline TZrBool zrg_module_error(ZrLibCallContext *ctx, const char *message) {
    if (!ZrCore_Exception_RaiseNamedRuntimeError(ctx->state, "RuntimeError", message, ctx->state->callInfoList))
        ZrCore_Debug_RunError(ctx->state, "%s", message);
    ctx->state->threadStatus = ctx->state->currentExceptionStatus;
    return ZR_FALSE;
}

/* All successful callbacks, including void callbacks, explicitly write result. */
#define ZRG_FUNCTION(NAME, CALLBACK, RETURN, PARAMETERS, COUNT) \
    {.name = NAME, .callback = CALLBACK, .returnTypeName = RETURN, \
     .minArgumentCount = COUNT, .maxArgumentCount = COUNT, \
     .parameters = PARAMETERS, .parameterCount = COUNT, \
     .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN}

#endif
