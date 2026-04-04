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
    // [0] META_GET exec=21 type=5 effect=0 dst=5 op0=5 op1=1 deopt=1
    // [1] META_SET exec=24 type=5 effect=1 dst=7 op0=7 op1=2 deopt=2
    // [2] META_GET exec=28 type=5 effect=2 dst=6 op0=6 op1=1 deopt=3
    // [3] OWN_USING exec=45 type=6 effect=3 dst=8 op0=9 op1=0 deopt=0
    // [4] OWN_SHARE exec=50 type=7 effect=4 dst=9 op0=10 op1=0 deopt=0
    // [5] OWN_WEAK exec=58 type=8 effect=5 dst=10 op0=11 op1=0 deopt=0
    // [6] OWN_UPGRADE exec=63 type=9 effect=6 dst=11 op0=12 op1=0 deopt=0
    // [7] OWN_RELEASE exec=65 type=10 effect=7 dst=12 op0=9 op1=0 deopt=0
    // [8] OWN_RELEASE exec=66 type=10 effect=8 dst=13 op0=11 op1=0 deopt=0
    // [9] OWN_UPGRADE exec=70 type=9 effect=9 dst=14 op0=15 op1=0 deopt=0
    // [10] DYN_TAIL_CALL exec=2 type=0 effect=0 dst=2 op0=2 op1=1 deopt=1
    return ZrLibrary_AotRuntime_InvokeActiveShim(state, ZR_AOT_BACKEND_KIND_C);
}

static const ZrAotCompiledModuleV1 zr_aot_module = {
    ZR_VM_AOT_ABI_VERSION,
    ZR_AOT_BACKEND_KIND_C,
    "main",
    "b183ebc8741d44a7",
    "b17dfee1737faf80",
    zr_aot_runtime_contracts,
    zr_aot_entry,
};

ZR_VM_AOT_EXPORT const ZrAotCompiledModuleV1 *ZrVm_GetAotCompiledModule_v1(void) {
    return &zr_aot_module;
}
