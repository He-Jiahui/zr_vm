; ZR AOT LLVM Backend
; SemIR -> AOTIR textual lowering.
declare ptr @ZrCore_Function_PreCall(ptr, ptr, i64, ptr)
declare i1 @ZrCore_Ownership_NativeShared(ptr)
declare i1 @ZrCore_Ownership_NativeWeak(ptr)
declare i1 @ZrCore_Ownership_UpgradeValue(ptr, ptr, ptr)
declare void @ZrCore_Ownership_ReleaseValue(ptr, ptr)
; runtimeContracts: dispatch.precall ownership.share ownership.weak ownership.upgrade ownership.release

; [0] META_GET exec=21 type=5 effect=0 dst=5 op0=5 op1=1 deopt=1
; [1] META_SET exec=24 type=5 effect=1 dst=7 op0=7 op1=2 deopt=2
; [2] META_GET exec=28 type=5 effect=2 dst=6 op0=6 op1=1 deopt=3
; [3] OWN_USING exec=45 type=6 effect=3 dst=8 op0=9 op1=0 deopt=0
; [4] OWN_SHARE exec=50 type=7 effect=4 dst=9 op0=10 op1=0 deopt=0
; [5] OWN_WEAK exec=58 type=8 effect=5 dst=10 op0=11 op1=0 deopt=0
; [6] OWN_UPGRADE exec=63 type=9 effect=6 dst=11 op0=12 op1=0 deopt=0
; [7] OWN_RELEASE exec=65 type=10 effect=7 dst=12 op0=9 op1=0 deopt=0
; [8] OWN_RELEASE exec=66 type=10 effect=8 dst=13 op0=11 op1=0 deopt=0
; [9] OWN_UPGRADE exec=70 type=9 effect=9 dst=14 op0=15 op1=0 deopt=0
; [10] DYN_TAIL_CALL exec=2 type=0 effect=0 dst=2 op0=2 op1=1 deopt=1
@zr_aot_module_name = private unnamed_addr constant [5 x i8] c"main\00"
@zr_aot_input_hash = private unnamed_addr constant [17 x i8] c"b183ebc8741d44a7\00"
@zr_aot_runtime_contract_0 = private unnamed_addr constant [17 x i8] c"dispatch.precall\00"
@zr_aot_runtime_contract_1 = private unnamed_addr constant [16 x i8] c"ownership.share\00"
@zr_aot_runtime_contract_2 = private unnamed_addr constant [15 x i8] c"ownership.weak\00"
@zr_aot_runtime_contract_3 = private unnamed_addr constant [18 x i8] c"ownership.upgrade\00"
@zr_aot_runtime_contract_4 = private unnamed_addr constant [18 x i8] c"ownership.release\00"
@zr_aot_runtime_contracts = private constant [6 x ptr] [ptr @zr_aot_runtime_contract_0, ptr @zr_aot_runtime_contract_1, ptr @zr_aot_runtime_contract_2, ptr @zr_aot_runtime_contract_3, ptr @zr_aot_runtime_contract_4, ptr null]
%ZrAotCompiledModule = type { i32, i32, ptr, i32, ptr, ptr, ptr, i64, ptr, i32, ptr }
define i64 @zr_aot_entry(ptr %state) {
entry:
  %ret = call i64 @ZrLibrary_AotRuntime_InvokeActiveShim(ptr %state, i32 2)
  ret i64 %ret
}

@zr_aot_module = private constant %ZrAotCompiledModule {
  i32 2,
  i32 2,
  ptr @zr_aot_module_name,
  i32 1,
  ptr @zr_aot_input_hash,
  ptr @zr_aot_runtime_contracts,
  ptr null,
  i64 0,
  ptr null,
  i32 0,
  ptr @zr_aot_entry
}

; export-symbol: ZrVm_GetAotCompiledModule
; descriptor.moduleName = main
; descriptor.inputKind = 1
; descriptor.inputHash = b183ebc8741d44a7
; descriptor.backendKind = llvm
declare i64 @ZrLibrary_AotRuntime_InvokeActiveShim(ptr, i32)
define ptr @ZrVm_GetAotCompiledModule() {
entry_export:
  ret ptr @zr_aot_module
}
