; ZR AOT LLVM Backend
; SemIR -> AOTIR textual lowering.
declare i1 @ZrCore_Reflection_TypeOfValue(ptr, ptr, ptr)
; runtimeContracts: reflection.typeof

; [0] TYPEOF exec=4 type=0 effect=0 dst=1 op0=1 op1=0 deopt=1
; [1] TYPEOF exec=10 type=0 effect=1 dst=2 op0=2 op1=0 deopt=2
; [2] TYPEOF exec=21 type=0 effect=2 dst=3 op0=3 op1=0 deopt=3
; [3] TYPEOF exec=32 type=0 effect=3 dst=4 op0=4 op1=0 deopt=4
; [4] TYPEOF exec=42 type=0 effect=4 dst=5 op0=5 op1=0 deopt=5
@zr_aot_module_name = private unnamed_addr constant [10 x i8] c"__entry__\00"
@zr_aot_input_hash = private unnamed_addr constant [8 x i8] c"unknown\00"
@zr_aot_runtime_contract_0 = private unnamed_addr constant [18 x i8] c"reflection.typeof\00"
@zr_aot_runtime_contracts = private constant [2 x ptr] [ptr @zr_aot_runtime_contract_0, ptr null]
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
; descriptor.moduleName = __entry__
; descriptor.inputKind = 1
; descriptor.inputHash = unknown
; descriptor.backendKind = llvm
declare i64 @ZrLibrary_AotRuntime_InvokeActiveShim(ptr, i32)
define ptr @ZrVm_GetAotCompiledModule() {
entry_export:
  ret ptr @zr_aot_module
}
