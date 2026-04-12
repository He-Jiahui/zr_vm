; ZR AOT LLVM Backend
; SemIR overlay + generated exec thunks.
; runtimeContracts:

@zr_aot_module_name = private unnamed_addr constant [10 x i8] c"__entry__\00"
@zr_aot_input_hash = private unnamed_addr constant [8 x i8] c"unknown\00"
@zr_aot_runtime_contracts = private constant [1 x ptr] [ptr null]
%SZrTypeValue = type { [48 x i8] }
%SZrTypeValueOnStack = type { [64 x i8] }
%ZrAotGeneratedFrame = type { ptr, ptr, ptr, ptr, i32, i32, i32, i32, i32, i32, i8 }
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
  %t4 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t5 = load ptr, ptr %t4, align 8
  %t6 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t7 = load ptr, ptr %t6, align 8
  %t8 = getelementptr i8, ptr %t7, i64 64
  %t9 = getelementptr i8, ptr %t8, i64 20
  %t10 = load i32, ptr %t9, align 4
  %t11 = getelementptr i8, ptr %t5, i64 20
  %t12 = load i32, ptr %t11, align 4
  %t19 = load i32, ptr %t8, align 4
  %t20 = getelementptr i8, ptr %t8, i64 16
  %t21 = load i8, ptr %t20, align 1
  %t13 = icmp eq i32 %t10, 2
  %t14 = icmp eq i32 %t10, 1
  %t15 = icmp eq i32 %t10, 5
  %t16 = or i1 %t14, %t15
  %t17 = or i1 %t16, %t13
  br i1 %t17, label %zr_aot_stack_copy_transfer_30, label %zr_aot_stack_copy_weak_check_30
zr_aot_stack_copy_transfer_30:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t5)
  %t31 = load %SZrTypeValue, ptr %t8, align 32
  store %SZrTypeValue %t31, ptr %t5, align 32
  %t32 = getelementptr i8, ptr %t8, i64 8
  %t33 = getelementptr i8, ptr %t8, i64 16
  %t34 = getelementptr i8, ptr %t8, i64 17
  %t35 = getelementptr i8, ptr %t8, i64 20
  %t36 = getelementptr i8, ptr %t8, i64 24
  %t37 = getelementptr i8, ptr %t8, i64 32
  store i32 0, ptr %t8, align 4
  store i64 0, ptr %t32, align 8
  store i8 0, ptr %t33, align 1
  store i8 1, ptr %t34, align 1
  store i32 0, ptr %t35, align 4
  store ptr null, ptr %t36, align 8
  store ptr null, ptr %t37, align 8
  br label %zr_aot_fn_0_ins_2
zr_aot_stack_copy_weak_check_30:
  %t18 = icmp eq i32 %t10, 3
  br i1 %t18, label %zr_aot_stack_copy_weak_30, label %zr_aot_stack_copy_fast_check_30
zr_aot_stack_copy_weak_30:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t5, ptr %t8)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t8)
  br label %zr_aot_fn_0_ins_2
zr_aot_stack_copy_fast_check_30:
  %t22 = icmp ne i8 %t21, 0
  %t23 = icmp eq i32 %t19, 18
  %t24 = and i1 %t22, %t23
  %t25 = icmp eq i32 %t10, 0
  %t26 = icmp eq i32 %t12, 0
  %t27 = and i1 %t25, %t26
  %t28 = xor i1 %t24, true
  %t29 = and i1 %t27, %t28
  br i1 %t29, label %zr_aot_stack_copy_fast_30, label %zr_aot_stack_copy_slow_30
zr_aot_stack_copy_fast_30:
  %t38 = load %SZrTypeValue, ptr %t8, align 32
  store %SZrTypeValue %t38, ptr %t5, align 32
  br label %zr_aot_fn_0_ins_2
zr_aot_stack_copy_slow_30:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t5, ptr %t8)
  br label %zr_aot_fn_0_ins_2

zr_aot_fn_0_ins_2:
  %t39 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t39, label %zr_aot_fn_0_ins_2_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_2_body:
  %t40 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t41 = load ptr, ptr %t40, align 8
  %t42 = getelementptr i8, ptr %t41, i64 64
  %t43 = getelementptr i8, ptr %t42, i64 8
  %t44 = getelementptr i8, ptr %t42, i64 16
  %t45 = getelementptr i8, ptr %t42, i64 17
  %t46 = getelementptr i8, ptr %t42, i64 20
  %t47 = getelementptr i8, ptr %t42, i64 24
  %t48 = getelementptr i8, ptr %t42, i64 32
  store i32 0, ptr %t42, align 4
  store i64 0, ptr %t43, align 8
  store i8 0, ptr %t44, align 1
  store i8 1, ptr %t45, align 1
  store i32 0, ptr %t46, align 4
  store ptr null, ptr %t47, align 8
  store ptr null, ptr %t48, align 8
  br label %zr_aot_fn_0_ins_3

zr_aot_fn_0_ins_3:
  %t49 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 8)
  br i1 %t49, label %zr_aot_fn_0_ins_3_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_body:
  %t50 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 1, i1 true)
  ret i64 %t50

zr_aot_fn_0_end_unsupported:
  %t51 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 0, i32 4, i32 0)
  ret i64 %t51

zr_aot_fn_0_fail:
  %t52 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t52
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
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 0)
  br i1 %t1, label %zr_aot_fn_1_ins_0_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_0_body:
  %t2 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t3 = load ptr, ptr %t2, align 8
  %t4 = getelementptr i8, ptr %t3, i64 128
  %t5 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t6 = load ptr, ptr %t5, align 8
  %t7 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t8 = load ptr, ptr %t7, align 8
  %t9 = getelementptr i8, ptr %t8, i64 64
  %t10 = load i32, ptr %t6, align 4
  %t11 = load i32, ptr %t9, align 4
  %t12 = getelementptr i8, ptr %t6, i64 8
  %t13 = load i64, ptr %t12, align 8
  %t14 = getelementptr i8, ptr %t9, i64 8
  %t15 = load i64, ptr %t14, align 8
  %t16 = icmp uge i32 %t10, 2
  %t17 = icmp ule i32 %t10, 5
  %t18 = and i1 %t16, %t17
  %t19 = icmp uge i32 %t11, 2
  %t20 = icmp ule i32 %t11, 5
  %t21 = and i1 %t19, %t20
  %t22 = and i1 %t18, %t21
  br i1 %t22, label %zr_aot_add_int_fast_23, label %zr_aot_fn_1_fail
zr_aot_add_int_fast_23:
  %t24 = add i64 %t13, %t15
  %t25 = getelementptr i8, ptr %t4, i64 8
  %t26 = getelementptr i8, ptr %t4, i64 16
  %t27 = getelementptr i8, ptr %t4, i64 17
  %t28 = getelementptr i8, ptr %t4, i64 20
  %t29 = getelementptr i8, ptr %t4, i64 24
  %t30 = getelementptr i8, ptr %t4, i64 32
  store i32 5, ptr %t4, align 4
  store i64 %t24, ptr %t25, align 8
  store i8 0, ptr %t26, align 1
  store i8 1, ptr %t27, align 1
  store i32 0, ptr %t28, align 4
  store ptr null, ptr %t29, align 8
  store ptr null, ptr %t30, align 8
  br label %zr_aot_fn_1_ins_1

zr_aot_fn_1_ins_1:
  %t31 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 8)
  br i1 %t31, label %zr_aot_fn_1_ins_1_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_1_body:
  %t32 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 false)
  ret i64 %t32

zr_aot_fn_1_end_unsupported:
  %t33 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 1, i32 2, i32 0)
  ret i64 %t33

zr_aot_fn_1_fail:
  %t34 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t34
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
declare void @ZrCore_Value_CopySlow(ptr, ptr, ptr)
declare void @ZrCore_Ownership_ReleaseValue(ptr, ptr)
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
declare i1 @ZrLibrary_AotRuntime_LogicalEqualBool(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalNotEqualBool(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalEqualSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalNotEqualSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalEqualUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalNotEqualUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalEqualFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalNotEqualFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalEqualString(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalNotEqualString(ptr, ptr, i32, i32, i32)
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
declare i1 @ZrLibrary_AotRuntime_ShouldJumpIfGreaterSigned(ptr, ptr, i32, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_Add(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_AddFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_Sub(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SubFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_Mul(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_AddInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_AddIntConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SubInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SubIntConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_AddSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_AddSignedConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_AddUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_AddUnsignedConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SubSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SubSignedConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SubUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SubUnsignedConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_BitwiseNot(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_BitwiseAnd(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_BitwiseOr(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_BitwiseXor(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_BitwiseShiftLeft(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_BitwiseShiftRight(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MulSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MulSignedConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MulUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MulUnsignedConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MulFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_Div(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_DivSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_DivSignedConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_DivUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_DivUnsignedConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_DivFloat(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_Mod(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ModSignedConst(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ModUnsigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ModUnsignedConst(ptr, ptr, i32, i32, i32)
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
declare i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SetMemberSlot(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetByIndex(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SetByIndex(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SuperArrayGetInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SuperArraySetInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SuperArrayAddInt(ptr, ptr, i32, i32, i32)
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
