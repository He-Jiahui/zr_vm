#include "zr_vm_library/native_binding.h"

static TZrBool swarm_plugin_value(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrLib_Value_SetInt(context->state, result, 42);
    return ZR_TRUE;
}

static const ZrLibFunctionDescriptor functions[] = {{
    .name = "value",
    .callback = swarm_plugin_value,
    .returnTypeName = "int",
    .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN,
}};

static const ZrLibModuleDescriptor module = {
    .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
    .moduleName = "zr.swarmprobe",
    .functions = functions,
    .functionCount = 1,
    .moduleVersion = "1.0.0",
    .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
    .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
    .publicContractHash = "zr.swarmprobe:v1",
};

const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) { return &module; }
