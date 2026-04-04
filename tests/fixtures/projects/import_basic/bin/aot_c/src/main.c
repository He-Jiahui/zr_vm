/* ZR AOT C Backend */
/* SemIR -> AOTIR textual lowering. */
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_library/aot_runtime.h"

static const TZrChar *const zr_aot_runtime_contracts[] = {
    ZR_NULL,
};

static TZrInt64 zr_aot_entry(struct SZrState *state) {
    return ZrLibrary_AotRuntime_InvokeActiveShim(state, ZR_AOT_BACKEND_KIND_C);
}

static const ZrAotCompiledModuleV1 zr_aot_module = {
    ZR_VM_AOT_ABI_VERSION,
    ZR_AOT_BACKEND_KIND_C,
    "main",
    "68a48e779cf6a132",
    "696c895be51161b9",
    zr_aot_runtime_contracts,
    zr_aot_entry,
};

ZR_VM_AOT_EXPORT const ZrAotCompiledModuleV1 *ZrVm_GetAotCompiledModule_v1(void) {
    return &zr_aot_module;
}
