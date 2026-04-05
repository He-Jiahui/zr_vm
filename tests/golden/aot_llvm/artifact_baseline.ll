; ZR AOT LLVM Backend
; SemIR overlay + generated exec thunks.
; runtimeContracts:

@zr_aot_module_name = private unnamed_addr constant [10 x i8] c"__entry__\00"
@zr_aot_input_hash = private unnamed_addr constant [8 x i8] c"unknown\00"
@zr_aot_runtime_contracts = private constant [1 x ptr] [ptr null]
%ZrAotGeneratedFrame = type { ptr, ptr, ptr, ptr, i32, i32, i32, i32, i32, i1 }
%ZrAotGeneratedDirectCall = type { ptr, ptr, ptr, i32, i32, i32, i32, i32, i1, i1 }
%ZrAotCompiledModule = type { i32, i32, ptr, i32, ptr, ptr, ptr, i64, ptr, i32, ptr }

define internal i64 @zr_aot_fn_0(ptr %state) {
entry:
  %frame = alloca %ZrAotGeneratedFrame, align 8
  %direct_call = alloca %ZrAotGeneratedDirectCall, align 8
  %resume_instruction = alloca i32, align 4
  %truthy_value = alloca i8, align 1
  %t0 = call i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr %state, i32 0, ptr %frame)
  br i1 %t0, label %zr_aot_fn_0_ins_0, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_0:
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 1)
  br i1 %t1, label %zr_aot_fn_0_ins_0_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_0_body:
  %t2 = call i1 @ZrLibrary_AotRuntime_CreateClosure(ptr %state, ptr %frame, i32 0, i32 0)
  br i1 %t2, label %zr_aot_fn_0_ins_1, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t3, label %zr_aot_fn_0_ins_1_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_CreateClosure(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t4, label %zr_aot_fn_0_ins_2, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 0)
  br i1 %t5, label %zr_aot_fn_0_ins_2_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_2_body:
  %t6 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t6, label %zr_aot_fn_0_ins_3, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_3:
  %t7 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 13)
  br i1 %t7, label %zr_aot_fn_0_ins_3_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_body:
  %t8 = call i1 @ZrLibrary_AotRuntime_PrepareStaticDirectCall(ptr %state, ptr %frame, i32 2, i32 2, i32 0, i32 2, ptr %direct_call)
  br i1 %t8, label %zr_aot_fn_0_ins_3_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_prepare_ok:
  %t9 = call i64 @zr_aot_fn_2(ptr %state)
  %t10 = icmp ne i64 %t9, 0
  br i1 %t10, label %zr_aot_fn_0_ins_3_invoke_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_invoke_ok:
  %t11 = call i1 @ZrLibrary_AotRuntime_FinishDirectCall(ptr %state, ptr %frame, ptr %direct_call, i32 1)
  br i1 %t11, label %zr_aot_fn_0_ins_3_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_finish_ok:
  %t12 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 true)
  ret i64 %t12

zr_aot_fn_0_ins_4:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 8)
  br i1 %t13, label %zr_aot_fn_0_ins_4_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_4_body:
  %t14 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 true)
  ret i64 %t14

zr_aot_fn_0_end_unsupported:
  %t15 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 0, i32 5, i32 0)
  ret i64 %t15

zr_aot_fn_0_fail:
  %t16 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t16
}

define internal i64 @zr_aot_fn_1(ptr %state) {
entry:
  %frame = alloca %ZrAotGeneratedFrame, align 8
  %direct_call = alloca %ZrAotGeneratedDirectCall, align 8
  %resume_instruction = alloca i32, align 4
  %truthy_value = alloca i8, align 1
  %t0 = call i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr %state, i32 1, ptr %frame)
  br i1 %t0, label %zr_aot_fn_1_ins_0, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_0:
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 1)
  br i1 %t1, label %zr_aot_fn_1_ins_0_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_0_body:
  %t2 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 0, i32 0)
  br i1 %t2, label %zr_aot_fn_1_ins_1, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 8)
  br i1 %t3, label %zr_aot_fn_1_ins_1_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_1_body:
  %t4 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 0, i1 false)
  ret i64 %t4

zr_aot_fn_1_end_unsupported:
  %t5 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 1, i32 2, i32 0)
  ret i64 %t5

zr_aot_fn_1_fail:
  %t6 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t6
}

define internal i64 @zr_aot_fn_2(ptr %state) {
entry:
  %frame = alloca %ZrAotGeneratedFrame, align 8
  %direct_call = alloca %ZrAotGeneratedDirectCall, align 8
  %resume_instruction = alloca i32, align 4
  %truthy_value = alloca i8, align 1
  %t0 = call i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr %state, i32 2, ptr %frame)
  br i1 %t0, label %zr_aot_fn_2_ins_0, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_0:
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 1)
  br i1 %t1, label %zr_aot_fn_2_ins_0_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_0_body:
  %t2 = call i1 @ZrLibrary_AotRuntime_GetClosureValue(ptr %state, ptr %frame, i32 0, i32 0)
  br i1 %t2, label %zr_aot_fn_2_ins_1, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 13)
  br i1 %t3, label %zr_aot_fn_2_ins_1_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 0, i32 0, i32 0, ptr %direct_call)
  br i1 %t4, label %zr_aot_fn_2_ins_1_prepare_ok, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_1_prepare_ok:
  %t5 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 0, i32 0, i32 0, i32 1)
  br i1 %t5, label %zr_aot_fn_2_ins_1_finish_ok, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_1_finish_ok:
  %t6 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 0, i1 false)
  ret i64 %t6

zr_aot_fn_2_ins_2:
  %t7 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 8)
  br i1 %t7, label %zr_aot_fn_2_ins_2_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_2_body:
  %t8 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 0, i1 false)
  ret i64 %t8

zr_aot_fn_2_end_unsupported:
  %t9 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 2, i32 3, i32 0)
  ret i64 %t9

zr_aot_fn_2_fail:
  %t10 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t10
}

@zr_aot_function_thunks = private constant [3 x ptr] [ptr @zr_aot_fn_0, ptr @zr_aot_fn_1, ptr @zr_aot_fn_2]

define i64 @zr_aot_entry(ptr %state) {
entry:
  %ret = call i64 @zr_aot_fn_0(ptr %state)
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
  ptr @zr_aot_function_thunks,
  i32 3,
  ptr @zr_aot_entry
}

; export-symbol: ZrVm_GetAotCompiledModule
; descriptor.moduleName = __entry__
; descriptor.inputKind = 1
; descriptor.inputHash = unknown
; descriptor.backendKind = llvm
declare i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_CopyConstant(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_CreateClosure(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetClosureValue(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SetClosureValue(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_CopyStack(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetGlobal(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_CreateObject(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_CreateArray(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_TypeOf(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ToObject(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ToStruct(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaGet(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaSet(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaGetCached(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaSetCached(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaGetStaticCached(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaSetStaticCached(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnUsing(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnShare(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnWeak(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnUpgrade(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnRelease(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalEqual(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalNotEqual(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalLessSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_IsTruthy(ptr, ptr, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_Add(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_AddInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SubInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MulSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_DivSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_Neg(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetMember(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SetMember(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetByIndex(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SetByIndex(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_IterInit(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_IterMoveNext(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_IterCurrent(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr, ptr, i32, i32, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_PrepareMetaCall(ptr, ptr, i32, i32, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_PrepareStaticDirectCall(ptr, ptr, i32, i32, i32, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr, ptr, ptr, i32, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_FinishDirectCall(ptr, ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_Try(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_EndTry(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_Throw(ptr, ptr, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_Catch(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_EndFinally(ptr, ptr, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_SetPendingReturn(ptr, ptr, i32, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_SetPendingBreak(ptr, ptr, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_SetPendingContinue(ptr, ptr, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_ToInt(ptr, ptr, i32, i32)
declare i64 @ZrLibrary_AotRuntime_Return(ptr, ptr, i32, i1)
declare i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr, ptr)
declare i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr, i32, i32, i32)
define ptr @ZrVm_GetAotCompiledModule() {
entry_export:
  ret ptr @zr_aot_module
}
