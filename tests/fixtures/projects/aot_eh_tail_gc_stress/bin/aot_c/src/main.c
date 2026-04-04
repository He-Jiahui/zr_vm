/* ZR AOT C Backend */
/* SemIR -> AOTIR textual lowering. */
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_library/aot_runtime.h"
/* runtime contract: ZrCore_Function_PreCall */
/* runtime contract: ZrCore_Ownership_NativeShared */
/* runtime contract: ZrCore_Ownership_NativeWeak */
/* runtime contract: ZrCore_Ownership_UpgradeValue */
/* runtime contract: ZrCore_Ownership_ReleaseValue */

static const TZrChar *const zr_aot_runtime_contracts[] = {
    "dispatch.precall",
    "ownership.share",
    "ownership.weak",
    "ownership.upgrade",
    "ownership.release",
    ZR_NULL,
};

static TZrInt64 zr_aot_entry(struct SZrState *state) {
    // [0] OWN_SHARE exec=13 type=3 effect=0 dst=5 op0=6 op1=0 deopt=0
    // [1] OWN_WEAK exec=18 type=4 effect=1 dst=6 op0=7 op1=0 deopt=0
    // [2] OWN_UPGRADE exec=23 type=5 effect=2 dst=7 op0=8 op1=0 deopt=0
    // [3] META_CALL exec=36 type=0 effect=3 dst=10 op0=10 op1=2 deopt=1
    // [4] OWN_RELEASE exec=43 type=8 effect=4 dst=12 op0=5 op1=0 deopt=0
    // [5] OWN_RELEASE exec=44 type=8 effect=5 dst=13 op0=7 op1=0 deopt=0
    // [6] OWN_UPGRADE exec=48 type=5 effect=6 dst=14 op0=15 op1=0 deopt=0
    return ZrLibrary_AotRuntime_InvokeActiveShim(state, ZR_AOT_BACKEND_KIND_C);
}

static const ZrAotCompiledModuleV1 zr_aot_module = {
    ZR_VM_AOT_ABI_VERSION,
    ZR_AOT_BACKEND_KIND_C,
    "main",
    "63a4b97fc8b2c45f",
    "ad491d165e4a6e23",
    zr_aot_runtime_contracts,
    zr_aot_entry,
};

ZR_VM_AOT_EXPORT const ZrAotCompiledModuleV1 *ZrVm_GetAotCompiledModule_v1(void) {
    return &zr_aot_module;
}
