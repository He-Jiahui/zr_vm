; ZR AOT LLVM Backend
; SemIR -> AOTIR textual lowering.
declare ptr @ZrCore_Function_PreCall(ptr, ptr, i64, ptr)
declare i1 @ZrCore_Ownership_NativeShared(ptr)
declare i1 @ZrCore_Ownership_NativeWeak(ptr)
declare i1 @ZrCore_Ownership_UpgradeValue(ptr, ptr, ptr)
declare void @ZrCore_Ownership_ReleaseValue(ptr, ptr)
; runtimeContracts: dispatch.precall ownership.share ownership.weak ownership.upgrade ownership.release

; [0] OWN_SHARE exec=13 type=3 effect=0 dst=5 op0=6 op1=0 deopt=0
; [1] OWN_WEAK exec=18 type=4 effect=1 dst=6 op0=7 op1=0 deopt=0
; [2] OWN_UPGRADE exec=23 type=5 effect=2 dst=7 op0=8 op1=0 deopt=0
; [3] META_CALL exec=36 type=0 effect=3 dst=10 op0=10 op1=2 deopt=1
; [4] OWN_RELEASE exec=43 type=8 effect=4 dst=12 op0=5 op1=0 deopt=0
; [5] OWN_RELEASE exec=44 type=8 effect=5 dst=13 op0=7 op1=0 deopt=0
; [6] OWN_UPGRADE exec=48 type=5 effect=6 dst=14 op0=15 op1=0 deopt=0
@zr_aot_module_name = private unnamed_addr constant [5 x i8] c"main\00"
@zr_aot_source_hash = private unnamed_addr constant [17 x i8] c"63a4b97fc8b2c45f\00"
@zr_aot_zro_hash = private unnamed_addr constant [17 x i8] c"ad491d165e4a6e23\00"
@zr_aot_runtime_contract_0 = private unnamed_addr constant [17 x i8] c"dispatch.precall\00"
@zr_aot_runtime_contract_1 = private unnamed_addr constant [16 x i8] c"ownership.share\00"
@zr_aot_runtime_contract_2 = private unnamed_addr constant [15 x i8] c"ownership.weak\00"
@zr_aot_runtime_contract_3 = private unnamed_addr constant [18 x i8] c"ownership.upgrade\00"
@zr_aot_runtime_contract_4 = private unnamed_addr constant [18 x i8] c"ownership.release\00"
@zr_aot_runtime_contracts = private constant [6 x ptr] [ptr @zr_aot_runtime_contract_0, ptr @zr_aot_runtime_contract_1, ptr @zr_aot_runtime_contract_2, ptr @zr_aot_runtime_contract_3, ptr @zr_aot_runtime_contract_4, ptr null]
%ZrAotCompiledModuleV1 = type { i32, i32, ptr, ptr, ptr, ptr, ptr }
define i64 @zr_aot_entry(ptr %state) {
entry:
  %ret = call i64 @ZrLibrary_AotRuntime_InvokeActiveShim(ptr %state, i32 2)
  ret i64 %ret
}

@zr_aot_module = private constant %ZrAotCompiledModuleV1 {
  i32 1,
  i32 2,
  ptr @zr_aot_module_name,
  ptr @zr_aot_source_hash,
  ptr @zr_aot_zro_hash,
  ptr @zr_aot_runtime_contracts,
  ptr @zr_aot_entry
}

; export-symbol: ZrVm_GetAotCompiledModule_v1
; descriptor.moduleName = main
; descriptor.sourceHash = 63a4b97fc8b2c45f
; descriptor.zroHash = ad491d165e4a6e23
; descriptor.backendKind = llvm
declare i64 @ZrLibrary_AotRuntime_InvokeActiveShim(ptr, i32)
define ptr @ZrVm_GetAotCompiledModule_v1() {
entry_export:
  ret ptr @zr_aot_module
}
