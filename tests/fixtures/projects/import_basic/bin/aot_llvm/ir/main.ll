; ZR AOT LLVM Backend
; SemIR -> AOTIR textual lowering.
; runtimeContracts:

@zr_aot_module_name = private unnamed_addr constant [5 x i8] c"main\00"
@zr_aot_input_hash = private unnamed_addr constant [17 x i8] c"68a48e779cf6a132\00"
@zr_aot_runtime_contracts = private constant [1 x ptr] [ptr null]
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
; descriptor.inputHash = 68a48e779cf6a132
; descriptor.backendKind = llvm
declare i64 @ZrLibrary_AotRuntime_InvokeActiveShim(ptr, i32)
define ptr @ZrVm_GetAotCompiledModule() {
entry_export:
  ret ptr @zr_aot_module
}
