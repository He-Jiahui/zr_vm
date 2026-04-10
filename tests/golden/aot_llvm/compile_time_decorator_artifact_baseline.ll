; ZR AOT LLVM Backend
; SemIR overlay + generated exec thunks.
declare i1 @ZrCore_Reflection_TypeOfValue(ptr, ptr, ptr)
; runtimeContracts: reflection.typeof

; [0] TYPEOF exec=2 type=0 effect=0 dst=2 op0=2 op1=0 deopt=1
; [1] TYPEOF exec=8 type=0 effect=1 dst=3 op0=3 op1=0 deopt=2
@zr_aot_module_name = private unnamed_addr constant [10 x i8] c"__entry__\00"
@zr_aot_input_hash = private unnamed_addr constant [8 x i8] c"unknown\00"
@zr_aot_runtime_contract_0 = private unnamed_addr constant [18 x i8] c"reflection.typeof\00"
@zr_aot_runtime_contracts = private constant [2 x ptr] [ptr @zr_aot_runtime_contract_0, ptr null]
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
  %t2 = call i1 @ZrLibrary_AotRuntime_CreateClosure(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t2, label %zr_aot_fn_0_ins_1, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 0)
  br i1 %t3, label %zr_aot_fn_0_ins_1_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 0, i32 1)
  br i1 %t4, label %zr_aot_fn_0_ins_2, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 0)
  br i1 %t5, label %zr_aot_fn_0_ins_2_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_2_body:
  %t6 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 2, i32 0)
  br i1 %t6, label %zr_aot_fn_0_ins_3, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_3:
  %t7 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 13)
  br i1 %t7, label %zr_aot_fn_0_ins_3_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_body:
  %t8 = call i1 @ZrLibrary_AotRuntime_PrepareStaticDirectCall(ptr %state, ptr %frame, i32 2, i32 2, i32 0, i32 1, ptr %direct_call)
  br i1 %t8, label %zr_aot_fn_0_ins_3_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_prepare_ok:
  %t9 = call i64 @zr_aot_fn_1(ptr %state)
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
  %t2 = call i1 @ZrLibrary_AotRuntime_GetClosureValue(ptr %state, ptr %frame, i32 3, i32 0)
  br i1 %t2, label %zr_aot_fn_1_ins_1, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 0)
  br i1 %t3, label %zr_aot_fn_1_ins_1_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 2, i32 3)
  br i1 %t4, label %zr_aot_fn_1_ins_2, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t5, label %zr_aot_fn_1_ins_2_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_2_body:
  %t6 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t6, label %zr_aot_fn_1_ins_3, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_3:
  %t7 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t7, label %zr_aot_fn_1_ins_3_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_3_body:
  %t8 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 2, i32 2, i32 0)
  br i1 %t8, label %zr_aot_fn_1_ins_4, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_4:
  %t9 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 0)
  br i1 %t9, label %zr_aot_fn_1_ins_4_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_4_body:
  %t10 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 1, i32 2)
  br i1 %t10, label %zr_aot_fn_1_ins_5, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_5:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 0)
  br i1 %t11, label %zr_aot_fn_1_ins_5_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_5_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 4)
  br i1 %t12, label %zr_aot_fn_1_ins_6, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_6:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 1)
  br i1 %t13, label %zr_aot_fn_1_ins_6_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_6_body:
  %t14 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 1)
  br i1 %t14, label %zr_aot_fn_1_ins_7, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_7:
  %t15 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 0)
  br i1 %t15, label %zr_aot_fn_1_ins_7_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_7_body:
  %t16 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 3, i32 4)
  br i1 %t16, label %zr_aot_fn_1_ins_8, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_8:
  %t17 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 8, i32 1)
  br i1 %t17, label %zr_aot_fn_1_ins_8_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_8_body:
  %t18 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 3, i32 3)
  br i1 %t18, label %zr_aot_fn_1_ins_9, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_9:
  %t19 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 9, i32 1)
  br i1 %t19, label %zr_aot_fn_1_ins_9_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_9_body:
  %t20 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 3, i32 3, i32 0)
  br i1 %t20, label %zr_aot_fn_1_ins_10, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_10:
  %t21 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 10, i32 0)
  br i1 %t21, label %zr_aot_fn_1_ins_10_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_10_body:
  %t22 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 2, i32 3)
  br i1 %t22, label %zr_aot_fn_1_ins_11, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_11:
  %t23 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 11, i32 0)
  br i1 %t23, label %zr_aot_fn_1_ins_11_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_11_body:
  %t24 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t24, label %zr_aot_fn_1_ins_12, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_12:
  %t25 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 12, i32 0)
  br i1 %t25, label %zr_aot_fn_1_ins_12_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_12_body:
  %t26 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 3, i32 4)
  br i1 %t26, label %zr_aot_fn_1_ins_13, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_13:
  %t27 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 13, i32 0)
  br i1 %t27, label %zr_aot_fn_1_ins_13_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_13_body:
  %t28 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 4, i32 2)
  br i1 %t28, label %zr_aot_fn_1_ins_14, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_14:
  %t29 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 14, i32 0)
  br i1 %t29, label %zr_aot_fn_1_ins_14_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_14_body:
  %t30 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 3, i32 4)
  br i1 %t30, label %zr_aot_fn_1_ins_15, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_15:
  %t31 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 15, i32 0)
  br i1 %t31, label %zr_aot_fn_1_ins_15_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_15_body:
  %t32 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 4, i32 0)
  br i1 %t32, label %zr_aot_fn_1_ins_16, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_16:
  %t33 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 16, i32 0)
  br i1 %t33, label %zr_aot_fn_1_ins_16_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_16_body:
  %t34 = call i1 @ZrLibrary_AotRuntime_AddIntConst(ptr %state, ptr %frame, i32 4, i32 4, i32 1)
  br i1 %t34, label %zr_aot_fn_1_ins_17, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_17:
  %t35 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 17, i32 0)
  br i1 %t35, label %zr_aot_fn_1_ins_17_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_17_body:
  %t36 = call i1 @ZrLibrary_AotRuntime_AddIntConst(ptr %state, ptr %frame, i32 4, i32 4, i32 2)
  br i1 %t36, label %zr_aot_fn_1_ins_18, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_18:
  %t37 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 18, i32 8)
  br i1 %t37, label %zr_aot_fn_1_ins_18_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_18_body:
  %t38 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 4, i1 false)
  ret i64 %t38

zr_aot_fn_1_end_unsupported:
  %t39 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 1, i32 19, i32 0)
  ret i64 %t39

zr_aot_fn_1_fail:
  %t40 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t40
}

@zr_aot_function_thunks = private constant [2 x ptr] [ptr @zr_aot_fn_0, ptr @zr_aot_fn_1]

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
  i32 2,
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
declare i1 @ZrLibrary_AotRuntime_SetConstant(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_CreateClosure(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetSubFunction(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetClosureValue(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SetClosureValue(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_CopyStack(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetGlobal(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_CreateObject(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_CreateArray(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_TypeOf(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ToBool(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ToObject(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ToStruct(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ToUInt(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ToFloat(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaGet(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaSet(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaGetCached(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaSetCached(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaGetStaticCached(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaSetStaticCached(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnUnique(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnBorrow(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnLoan(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnShare(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnWeak(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnDetach(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnUpgrade(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnRelease(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalEqual(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalNotEqual(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalNot(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalAnd(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalOr(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalGreaterSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalGreaterUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalGreaterFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalLessSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalLessUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalLessFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalGreaterEqualSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalGreaterEqualUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalGreaterEqualFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalLessEqualSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalLessEqualUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalLessEqualFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_IsTruthy(ptr, ptr, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_Add(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_AddFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_Sub(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SubFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_Mul(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_AddInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_AddIntConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SubInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SubIntConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_BitwiseNot(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_BitwiseAnd(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_BitwiseOr(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_BitwiseXor(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_BitwiseShiftLeft(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_BitwiseShiftRight(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MulSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MulSignedConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MulUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MulFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_Div(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_DivSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_DivSignedConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_DivUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_DivFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_Mod(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ModSignedConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ModUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ModFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_Pow(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_PowSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_PowUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_PowFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ShiftLeft(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ShiftLeftInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ShiftRight(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ShiftRightInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_Neg(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ToString(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetMember(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SetMember(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetByIndex(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SetByIndex(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SuperArrayGetInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SuperArraySetInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SuperArrayAddInt4(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SuperArrayAddInt4Const(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SuperArrayFillInt4Const(ptr, ptr, i32, i32, i32)
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
declare i1 @ZrLibrary_AotRuntime_MarkToBeClosed(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_CloseScope(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_ToInt(ptr, ptr, i32, i32)
declare i64 @ZrLibrary_AotRuntime_Return(ptr, ptr, i32, i1)
declare i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr, ptr)
declare i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr, i32, i32, i32)
define ptr @ZrVm_GetAotCompiledModule() {
entry_export:
  ret ptr @zr_aot_module
}
