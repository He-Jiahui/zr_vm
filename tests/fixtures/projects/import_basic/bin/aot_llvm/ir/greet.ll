; ZR AOT LLVM Backend
; SemIR -> AOTIR textual lowering.
; runtimeContracts:

@zr_aot_module_name = private unnamed_addr constant [6 x i8] c"greet\00"
@zr_aot_source_hash = private unnamed_addr constant [17 x i8] c"eab7c6bedfa9ab35\00"
@zr_aot_zro_hash = private unnamed_addr constant [17 x i8] c"1a891718ba82dd83\00"
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
; descriptor.moduleName = greet
; descriptor.sourceHash = eab7c6bedfa9ab35
; descriptor.zroHash = 1a891718ba82dd83
; descriptor.backendKind = llvm
declare i64 @ZrLibrary_AotRuntime_InvokeActiveShim(ptr, i32)
define ptr @ZrVm_GetAotCompiledModule_v1() {
entry_export:
  ret ptr @zr_aot_module
}
