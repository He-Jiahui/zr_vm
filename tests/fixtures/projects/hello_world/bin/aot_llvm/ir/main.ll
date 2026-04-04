; ZR AOT LLVM Backend
; SemIR -> AOTIR textual lowering.
; runtimeContracts:

@zr_aot_module_name = private unnamed_addr constant [5 x i8] c"main\00"
@zr_aot_source_hash = private unnamed_addr constant [17 x i8] c"17a814494fbf5749\00"
@zr_aot_zro_hash = private unnamed_addr constant [17 x i8] c"6d91b1d9c68abab0\00"
@zr_aot_runtime_contracts = private constant [1 x ptr] [ptr null]
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
; descriptor.sourceHash = 17a814494fbf5749
; descriptor.zroHash = 6d91b1d9c68abab0
; descriptor.backendKind = llvm
declare i64 @ZrLibrary_AotRuntime_InvokeActiveShim(ptr, i32)
define ptr @ZrVm_GetAotCompiledModule_v1() {
entry_export:
  ret ptr @zr_aot_module
}
