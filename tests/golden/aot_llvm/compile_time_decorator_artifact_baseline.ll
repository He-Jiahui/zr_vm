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
  %t39 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 0)
  br i1 %t39, label %zr_aot_fn_0_ins_2_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_2_body:
  %t40 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t41 = load ptr, ptr %t40, align 8
  %t42 = getelementptr i8, ptr %t41, i64 128
  %t43 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t44 = load ptr, ptr %t43, align 8
  %t45 = getelementptr i8, ptr %t44, i64 20
  %t46 = load i32, ptr %t45, align 4
  %t47 = getelementptr i8, ptr %t42, i64 20
  %t48 = load i32, ptr %t47, align 4
  %t55 = load i32, ptr %t44, align 4
  %t56 = getelementptr i8, ptr %t44, i64 16
  %t57 = load i8, ptr %t56, align 1
  %t49 = icmp eq i32 %t46, 2
  %t50 = icmp eq i32 %t46, 1
  %t51 = icmp eq i32 %t46, 5
  %t52 = or i1 %t50, %t51
  %t53 = or i1 %t52, %t49
  br i1 %t53, label %zr_aot_stack_copy_transfer_66, label %zr_aot_stack_copy_weak_check_66
zr_aot_stack_copy_transfer_66:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t42)
  %t67 = load %SZrTypeValue, ptr %t44, align 32
  store %SZrTypeValue %t67, ptr %t42, align 32
  %t68 = getelementptr i8, ptr %t44, i64 8
  %t69 = getelementptr i8, ptr %t44, i64 16
  %t70 = getelementptr i8, ptr %t44, i64 17
  %t71 = getelementptr i8, ptr %t44, i64 20
  %t72 = getelementptr i8, ptr %t44, i64 24
  %t73 = getelementptr i8, ptr %t44, i64 32
  store i32 0, ptr %t44, align 4
  store i64 0, ptr %t68, align 8
  store i8 0, ptr %t69, align 1
  store i8 1, ptr %t70, align 1
  store i32 0, ptr %t71, align 4
  store ptr null, ptr %t72, align 8
  store ptr null, ptr %t73, align 8
  br label %zr_aot_fn_0_ins_3
zr_aot_stack_copy_weak_check_66:
  %t54 = icmp eq i32 %t46, 3
  br i1 %t54, label %zr_aot_stack_copy_weak_66, label %zr_aot_stack_copy_fast_check_66
zr_aot_stack_copy_weak_66:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t42, ptr %t44)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t44)
  br label %zr_aot_fn_0_ins_3
zr_aot_stack_copy_fast_check_66:
  %t58 = icmp ne i8 %t57, 0
  %t59 = icmp eq i32 %t55, 18
  %t60 = and i1 %t58, %t59
  %t61 = icmp eq i32 %t46, 0
  %t62 = icmp eq i32 %t48, 0
  %t63 = and i1 %t61, %t62
  %t64 = xor i1 %t60, true
  %t65 = and i1 %t63, %t64
  br i1 %t65, label %zr_aot_stack_copy_fast_66, label %zr_aot_stack_copy_slow_66
zr_aot_stack_copy_fast_66:
  %t74 = load %SZrTypeValue, ptr %t44, align 32
  store %SZrTypeValue %t74, ptr %t42, align 32
  br label %zr_aot_fn_0_ins_3
zr_aot_stack_copy_slow_66:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t42, ptr %t44)
  br label %zr_aot_fn_0_ins_3

zr_aot_fn_0_ins_3:
  %t75 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 13)
  br i1 %t75, label %zr_aot_fn_0_ins_3_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_body:
  %t76 = call i1 @ZrLibrary_AotRuntime_PrepareStaticDirectCall(ptr %state, ptr %frame, i32 2, i32 2, i32 0, i32 1, ptr %direct_call)
  br i1 %t76, label %zr_aot_fn_0_ins_3_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_prepare_ok:
  %t77 = call i64 @zr_aot_fn_1(ptr %state)
  %t78 = icmp ne i64 %t77, 0
  br i1 %t78, label %zr_aot_fn_0_ins_3_invoke_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_invoke_ok:
  %t79 = call i1 @ZrLibrary_AotRuntime_FinishDirectCall(ptr %state, ptr %frame, ptr %direct_call, i32 1)
  br i1 %t79, label %zr_aot_fn_0_ins_3_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_finish_ok:
  %t80 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 true)
  ret i64 %t80

zr_aot_fn_0_ins_4:
  %t81 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 8)
  br i1 %t81, label %zr_aot_fn_0_ins_4_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_4_body:
  %t82 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 true)
  ret i64 %t82

zr_aot_fn_0_end_unsupported:
  %t83 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 0, i32 5, i32 0)
  ret i64 %t83

zr_aot_fn_0_fail:
  %t84 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t84
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
  %t4 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t5 = load ptr, ptr %t4, align 8
  %t6 = getelementptr i8, ptr %t5, i64 128
  %t7 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t8 = load ptr, ptr %t7, align 8
  %t9 = getelementptr i8, ptr %t8, i64 192
  %t10 = getelementptr i8, ptr %t9, i64 20
  %t11 = load i32, ptr %t10, align 4
  %t12 = getelementptr i8, ptr %t6, i64 20
  %t13 = load i32, ptr %t12, align 4
  %t20 = load i32, ptr %t9, align 4
  %t21 = getelementptr i8, ptr %t9, i64 16
  %t22 = load i8, ptr %t21, align 1
  %t14 = icmp eq i32 %t11, 2
  %t15 = icmp eq i32 %t11, 1
  %t16 = icmp eq i32 %t11, 5
  %t17 = or i1 %t15, %t16
  %t18 = or i1 %t17, %t14
  br i1 %t18, label %zr_aot_stack_copy_transfer_31, label %zr_aot_stack_copy_weak_check_31
zr_aot_stack_copy_transfer_31:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t6)
  %t32 = load %SZrTypeValue, ptr %t9, align 32
  store %SZrTypeValue %t32, ptr %t6, align 32
  %t33 = getelementptr i8, ptr %t9, i64 8
  %t34 = getelementptr i8, ptr %t9, i64 16
  %t35 = getelementptr i8, ptr %t9, i64 17
  %t36 = getelementptr i8, ptr %t9, i64 20
  %t37 = getelementptr i8, ptr %t9, i64 24
  %t38 = getelementptr i8, ptr %t9, i64 32
  store i32 0, ptr %t9, align 4
  store i64 0, ptr %t33, align 8
  store i8 0, ptr %t34, align 1
  store i8 1, ptr %t35, align 1
  store i32 0, ptr %t36, align 4
  store ptr null, ptr %t37, align 8
  store ptr null, ptr %t38, align 8
  br label %zr_aot_fn_1_ins_2
zr_aot_stack_copy_weak_check_31:
  %t19 = icmp eq i32 %t11, 3
  br i1 %t19, label %zr_aot_stack_copy_weak_31, label %zr_aot_stack_copy_fast_check_31
zr_aot_stack_copy_weak_31:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t6, ptr %t9)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t9)
  br label %zr_aot_fn_1_ins_2
zr_aot_stack_copy_fast_check_31:
  %t23 = icmp ne i8 %t22, 0
  %t24 = icmp eq i32 %t20, 18
  %t25 = and i1 %t23, %t24
  %t26 = icmp eq i32 %t11, 0
  %t27 = icmp eq i32 %t13, 0
  %t28 = and i1 %t26, %t27
  %t29 = xor i1 %t25, true
  %t30 = and i1 %t28, %t29
  br i1 %t30, label %zr_aot_stack_copy_fast_31, label %zr_aot_stack_copy_slow_31
zr_aot_stack_copy_fast_31:
  %t39 = load %SZrTypeValue, ptr %t9, align 32
  store %SZrTypeValue %t39, ptr %t6, align 32
  br label %zr_aot_fn_1_ins_2
zr_aot_stack_copy_slow_31:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t6, ptr %t9)
  br label %zr_aot_fn_1_ins_2

zr_aot_fn_1_ins_2:
  %t40 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t40, label %zr_aot_fn_1_ins_2_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_2_body:
  %t41 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t41, label %zr_aot_fn_1_ins_3, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_3:
  %t42 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t42, label %zr_aot_fn_1_ins_3_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_3_body:
  %t43 = call i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr %state, ptr %frame, i32 2, i32 2, i32 0)
  br i1 %t43, label %zr_aot_fn_1_ins_4, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_4:
  %t44 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 0)
  br i1 %t44, label %zr_aot_fn_1_ins_4_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_4_body:
  %t45 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t46 = load ptr, ptr %t45, align 8
  %t47 = getelementptr i8, ptr %t46, i64 64
  %t48 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t49 = load ptr, ptr %t48, align 8
  %t50 = getelementptr i8, ptr %t49, i64 128
  %t51 = getelementptr i8, ptr %t50, i64 20
  %t52 = load i32, ptr %t51, align 4
  %t53 = getelementptr i8, ptr %t47, i64 20
  %t54 = load i32, ptr %t53, align 4
  %t61 = load i32, ptr %t50, align 4
  %t62 = getelementptr i8, ptr %t50, i64 16
  %t63 = load i8, ptr %t62, align 1
  %t55 = icmp eq i32 %t52, 2
  %t56 = icmp eq i32 %t52, 1
  %t57 = icmp eq i32 %t52, 5
  %t58 = or i1 %t56, %t57
  %t59 = or i1 %t58, %t55
  br i1 %t59, label %zr_aot_stack_copy_transfer_72, label %zr_aot_stack_copy_weak_check_72
zr_aot_stack_copy_transfer_72:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t47)
  %t73 = load %SZrTypeValue, ptr %t50, align 32
  store %SZrTypeValue %t73, ptr %t47, align 32
  %t74 = getelementptr i8, ptr %t50, i64 8
  %t75 = getelementptr i8, ptr %t50, i64 16
  %t76 = getelementptr i8, ptr %t50, i64 17
  %t77 = getelementptr i8, ptr %t50, i64 20
  %t78 = getelementptr i8, ptr %t50, i64 24
  %t79 = getelementptr i8, ptr %t50, i64 32
  store i32 0, ptr %t50, align 4
  store i64 0, ptr %t74, align 8
  store i8 0, ptr %t75, align 1
  store i8 1, ptr %t76, align 1
  store i32 0, ptr %t77, align 4
  store ptr null, ptr %t78, align 8
  store ptr null, ptr %t79, align 8
  br label %zr_aot_fn_1_ins_5
zr_aot_stack_copy_weak_check_72:
  %t60 = icmp eq i32 %t52, 3
  br i1 %t60, label %zr_aot_stack_copy_weak_72, label %zr_aot_stack_copy_fast_check_72
zr_aot_stack_copy_weak_72:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t47, ptr %t50)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t50)
  br label %zr_aot_fn_1_ins_5
zr_aot_stack_copy_fast_check_72:
  %t64 = icmp ne i8 %t63, 0
  %t65 = icmp eq i32 %t61, 18
  %t66 = and i1 %t64, %t65
  %t67 = icmp eq i32 %t52, 0
  %t68 = icmp eq i32 %t54, 0
  %t69 = and i1 %t67, %t68
  %t70 = xor i1 %t66, true
  %t71 = and i1 %t69, %t70
  br i1 %t71, label %zr_aot_stack_copy_fast_72, label %zr_aot_stack_copy_slow_72
zr_aot_stack_copy_fast_72:
  %t80 = load %SZrTypeValue, ptr %t50, align 32
  store %SZrTypeValue %t80, ptr %t47, align 32
  br label %zr_aot_fn_1_ins_5
zr_aot_stack_copy_slow_72:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t47, ptr %t50)
  br label %zr_aot_fn_1_ins_5

zr_aot_fn_1_ins_5:
  %t81 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 0)
  br i1 %t81, label %zr_aot_fn_1_ins_5_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_5_body:
  %t82 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 4)
  br i1 %t82, label %zr_aot_fn_1_ins_6, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_6:
  %t83 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 1)
  br i1 %t83, label %zr_aot_fn_1_ins_6_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_6_body:
  %t84 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 1)
  br i1 %t84, label %zr_aot_fn_1_ins_7, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_7:
  %t85 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 0)
  br i1 %t85, label %zr_aot_fn_1_ins_7_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_7_body:
  %t86 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t87 = load ptr, ptr %t86, align 8
  %t88 = getelementptr i8, ptr %t87, i64 192
  %t89 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t90 = load ptr, ptr %t89, align 8
  %t91 = getelementptr i8, ptr %t90, i64 256
  %t92 = getelementptr i8, ptr %t91, i64 20
  %t93 = load i32, ptr %t92, align 4
  %t94 = getelementptr i8, ptr %t88, i64 20
  %t95 = load i32, ptr %t94, align 4
  %t102 = load i32, ptr %t91, align 4
  %t103 = getelementptr i8, ptr %t91, i64 16
  %t104 = load i8, ptr %t103, align 1
  %t96 = icmp eq i32 %t93, 2
  %t97 = icmp eq i32 %t93, 1
  %t98 = icmp eq i32 %t93, 5
  %t99 = or i1 %t97, %t98
  %t100 = or i1 %t99, %t96
  br i1 %t100, label %zr_aot_stack_copy_transfer_113, label %zr_aot_stack_copy_weak_check_113
zr_aot_stack_copy_transfer_113:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t88)
  %t114 = load %SZrTypeValue, ptr %t91, align 32
  store %SZrTypeValue %t114, ptr %t88, align 32
  %t115 = getelementptr i8, ptr %t91, i64 8
  %t116 = getelementptr i8, ptr %t91, i64 16
  %t117 = getelementptr i8, ptr %t91, i64 17
  %t118 = getelementptr i8, ptr %t91, i64 20
  %t119 = getelementptr i8, ptr %t91, i64 24
  %t120 = getelementptr i8, ptr %t91, i64 32
  store i32 0, ptr %t91, align 4
  store i64 0, ptr %t115, align 8
  store i8 0, ptr %t116, align 1
  store i8 1, ptr %t117, align 1
  store i32 0, ptr %t118, align 4
  store ptr null, ptr %t119, align 8
  store ptr null, ptr %t120, align 8
  br label %zr_aot_fn_1_ins_8
zr_aot_stack_copy_weak_check_113:
  %t101 = icmp eq i32 %t93, 3
  br i1 %t101, label %zr_aot_stack_copy_weak_113, label %zr_aot_stack_copy_fast_check_113
zr_aot_stack_copy_weak_113:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t88, ptr %t91)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t91)
  br label %zr_aot_fn_1_ins_8
zr_aot_stack_copy_fast_check_113:
  %t105 = icmp ne i8 %t104, 0
  %t106 = icmp eq i32 %t102, 18
  %t107 = and i1 %t105, %t106
  %t108 = icmp eq i32 %t93, 0
  %t109 = icmp eq i32 %t95, 0
  %t110 = and i1 %t108, %t109
  %t111 = xor i1 %t107, true
  %t112 = and i1 %t110, %t111
  br i1 %t112, label %zr_aot_stack_copy_fast_113, label %zr_aot_stack_copy_slow_113
zr_aot_stack_copy_fast_113:
  %t121 = load %SZrTypeValue, ptr %t91, align 32
  store %SZrTypeValue %t121, ptr %t88, align 32
  br label %zr_aot_fn_1_ins_8
zr_aot_stack_copy_slow_113:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t88, ptr %t91)
  br label %zr_aot_fn_1_ins_8

zr_aot_fn_1_ins_8:
  %t122 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 8, i32 1)
  br i1 %t122, label %zr_aot_fn_1_ins_8_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_8_body:
  %t123 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 3, i32 3)
  br i1 %t123, label %zr_aot_fn_1_ins_9, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_9:
  %t124 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 9, i32 1)
  br i1 %t124, label %zr_aot_fn_1_ins_9_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_9_body:
  %t125 = call i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr %state, ptr %frame, i32 3, i32 3, i32 1)
  br i1 %t125, label %zr_aot_fn_1_ins_10, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_10:
  %t126 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 10, i32 0)
  br i1 %t126, label %zr_aot_fn_1_ins_10_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_10_body:
  %t127 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t128 = load ptr, ptr %t127, align 8
  %t129 = getelementptr i8, ptr %t128, i64 128
  %t130 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t131 = load ptr, ptr %t130, align 8
  %t132 = getelementptr i8, ptr %t131, i64 192
  %t133 = getelementptr i8, ptr %t132, i64 20
  %t134 = load i32, ptr %t133, align 4
  %t135 = getelementptr i8, ptr %t129, i64 20
  %t136 = load i32, ptr %t135, align 4
  %t143 = load i32, ptr %t132, align 4
  %t144 = getelementptr i8, ptr %t132, i64 16
  %t145 = load i8, ptr %t144, align 1
  %t137 = icmp eq i32 %t134, 2
  %t138 = icmp eq i32 %t134, 1
  %t139 = icmp eq i32 %t134, 5
  %t140 = or i1 %t138, %t139
  %t141 = or i1 %t140, %t137
  br i1 %t141, label %zr_aot_stack_copy_transfer_154, label %zr_aot_stack_copy_weak_check_154
zr_aot_stack_copy_transfer_154:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t129)
  %t155 = load %SZrTypeValue, ptr %t132, align 32
  store %SZrTypeValue %t155, ptr %t129, align 32
  %t156 = getelementptr i8, ptr %t132, i64 8
  %t157 = getelementptr i8, ptr %t132, i64 16
  %t158 = getelementptr i8, ptr %t132, i64 17
  %t159 = getelementptr i8, ptr %t132, i64 20
  %t160 = getelementptr i8, ptr %t132, i64 24
  %t161 = getelementptr i8, ptr %t132, i64 32
  store i32 0, ptr %t132, align 4
  store i64 0, ptr %t156, align 8
  store i8 0, ptr %t157, align 1
  store i8 1, ptr %t158, align 1
  store i32 0, ptr %t159, align 4
  store ptr null, ptr %t160, align 8
  store ptr null, ptr %t161, align 8
  br label %zr_aot_fn_1_ins_11
zr_aot_stack_copy_weak_check_154:
  %t142 = icmp eq i32 %t134, 3
  br i1 %t142, label %zr_aot_stack_copy_weak_154, label %zr_aot_stack_copy_fast_check_154
zr_aot_stack_copy_weak_154:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t129, ptr %t132)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t132)
  br label %zr_aot_fn_1_ins_11
zr_aot_stack_copy_fast_check_154:
  %t146 = icmp ne i8 %t145, 0
  %t147 = icmp eq i32 %t143, 18
  %t148 = and i1 %t146, %t147
  %t149 = icmp eq i32 %t134, 0
  %t150 = icmp eq i32 %t136, 0
  %t151 = and i1 %t149, %t150
  %t152 = xor i1 %t148, true
  %t153 = and i1 %t151, %t152
  br i1 %t153, label %zr_aot_stack_copy_fast_154, label %zr_aot_stack_copy_slow_154
zr_aot_stack_copy_fast_154:
  %t162 = load %SZrTypeValue, ptr %t132, align 32
  store %SZrTypeValue %t162, ptr %t129, align 32
  br label %zr_aot_fn_1_ins_11
zr_aot_stack_copy_slow_154:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t129, ptr %t132)
  br label %zr_aot_fn_1_ins_11

zr_aot_fn_1_ins_11:
  %t163 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 11, i32 0)
  br i1 %t163, label %zr_aot_fn_1_ins_11_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_11_body:
  %t164 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t165 = load ptr, ptr %t164, align 8
  %t166 = getelementptr i8, ptr %t165, i64 256
  %t167 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t168 = load ptr, ptr %t167, align 8
  %t169 = getelementptr i8, ptr %t168, i64 64
  %t170 = getelementptr i8, ptr %t169, i64 20
  %t171 = load i32, ptr %t170, align 4
  %t172 = getelementptr i8, ptr %t166, i64 20
  %t173 = load i32, ptr %t172, align 4
  %t180 = load i32, ptr %t169, align 4
  %t181 = getelementptr i8, ptr %t169, i64 16
  %t182 = load i8, ptr %t181, align 1
  %t174 = icmp eq i32 %t171, 2
  %t175 = icmp eq i32 %t171, 1
  %t176 = icmp eq i32 %t171, 5
  %t177 = or i1 %t175, %t176
  %t178 = or i1 %t177, %t174
  br i1 %t178, label %zr_aot_stack_copy_transfer_191, label %zr_aot_stack_copy_weak_check_191
zr_aot_stack_copy_transfer_191:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t166)
  %t192 = load %SZrTypeValue, ptr %t169, align 32
  store %SZrTypeValue %t192, ptr %t166, align 32
  %t193 = getelementptr i8, ptr %t169, i64 8
  %t194 = getelementptr i8, ptr %t169, i64 16
  %t195 = getelementptr i8, ptr %t169, i64 17
  %t196 = getelementptr i8, ptr %t169, i64 20
  %t197 = getelementptr i8, ptr %t169, i64 24
  %t198 = getelementptr i8, ptr %t169, i64 32
  store i32 0, ptr %t169, align 4
  store i64 0, ptr %t193, align 8
  store i8 0, ptr %t194, align 1
  store i8 1, ptr %t195, align 1
  store i32 0, ptr %t196, align 4
  store ptr null, ptr %t197, align 8
  store ptr null, ptr %t198, align 8
  br label %zr_aot_fn_1_ins_12
zr_aot_stack_copy_weak_check_191:
  %t179 = icmp eq i32 %t171, 3
  br i1 %t179, label %zr_aot_stack_copy_weak_191, label %zr_aot_stack_copy_fast_check_191
zr_aot_stack_copy_weak_191:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t166, ptr %t169)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t169)
  br label %zr_aot_fn_1_ins_12
zr_aot_stack_copy_fast_check_191:
  %t183 = icmp ne i8 %t182, 0
  %t184 = icmp eq i32 %t180, 18
  %t185 = and i1 %t183, %t184
  %t186 = icmp eq i32 %t171, 0
  %t187 = icmp eq i32 %t173, 0
  %t188 = and i1 %t186, %t187
  %t189 = xor i1 %t185, true
  %t190 = and i1 %t188, %t189
  br i1 %t190, label %zr_aot_stack_copy_fast_191, label %zr_aot_stack_copy_slow_191
zr_aot_stack_copy_fast_191:
  %t199 = load %SZrTypeValue, ptr %t169, align 32
  store %SZrTypeValue %t199, ptr %t166, align 32
  br label %zr_aot_fn_1_ins_12
zr_aot_stack_copy_slow_191:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t166, ptr %t169)
  br label %zr_aot_fn_1_ins_12

zr_aot_fn_1_ins_12:
  %t200 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 12, i32 0)
  br i1 %t200, label %zr_aot_fn_1_ins_12_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_12_body:
  %t201 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t202 = load ptr, ptr %t201, align 8
  %t203 = getelementptr i8, ptr %t202, i64 192
  %t204 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t205 = load ptr, ptr %t204, align 8
  %t206 = getelementptr i8, ptr %t205, i64 256
  %t207 = getelementptr i8, ptr %t206, i64 20
  %t208 = load i32, ptr %t207, align 4
  %t209 = getelementptr i8, ptr %t203, i64 20
  %t210 = load i32, ptr %t209, align 4
  %t217 = load i32, ptr %t206, align 4
  %t218 = getelementptr i8, ptr %t206, i64 16
  %t219 = load i8, ptr %t218, align 1
  %t211 = icmp eq i32 %t208, 2
  %t212 = icmp eq i32 %t208, 1
  %t213 = icmp eq i32 %t208, 5
  %t214 = or i1 %t212, %t213
  %t215 = or i1 %t214, %t211
  br i1 %t215, label %zr_aot_stack_copy_transfer_228, label %zr_aot_stack_copy_weak_check_228
zr_aot_stack_copy_transfer_228:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t203)
  %t229 = load %SZrTypeValue, ptr %t206, align 32
  store %SZrTypeValue %t229, ptr %t203, align 32
  %t230 = getelementptr i8, ptr %t206, i64 8
  %t231 = getelementptr i8, ptr %t206, i64 16
  %t232 = getelementptr i8, ptr %t206, i64 17
  %t233 = getelementptr i8, ptr %t206, i64 20
  %t234 = getelementptr i8, ptr %t206, i64 24
  %t235 = getelementptr i8, ptr %t206, i64 32
  store i32 0, ptr %t206, align 4
  store i64 0, ptr %t230, align 8
  store i8 0, ptr %t231, align 1
  store i8 1, ptr %t232, align 1
  store i32 0, ptr %t233, align 4
  store ptr null, ptr %t234, align 8
  store ptr null, ptr %t235, align 8
  br label %zr_aot_fn_1_ins_13
zr_aot_stack_copy_weak_check_228:
  %t216 = icmp eq i32 %t208, 3
  br i1 %t216, label %zr_aot_stack_copy_weak_228, label %zr_aot_stack_copy_fast_check_228
zr_aot_stack_copy_weak_228:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t203, ptr %t206)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t206)
  br label %zr_aot_fn_1_ins_13
zr_aot_stack_copy_fast_check_228:
  %t220 = icmp ne i8 %t219, 0
  %t221 = icmp eq i32 %t217, 18
  %t222 = and i1 %t220, %t221
  %t223 = icmp eq i32 %t208, 0
  %t224 = icmp eq i32 %t210, 0
  %t225 = and i1 %t223, %t224
  %t226 = xor i1 %t222, true
  %t227 = and i1 %t225, %t226
  br i1 %t227, label %zr_aot_stack_copy_fast_228, label %zr_aot_stack_copy_slow_228
zr_aot_stack_copy_fast_228:
  %t236 = load %SZrTypeValue, ptr %t206, align 32
  store %SZrTypeValue %t236, ptr %t203, align 32
  br label %zr_aot_fn_1_ins_13
zr_aot_stack_copy_slow_228:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t203, ptr %t206)
  br label %zr_aot_fn_1_ins_13

zr_aot_fn_1_ins_13:
  %t237 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 13, i32 0)
  br i1 %t237, label %zr_aot_fn_1_ins_13_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_13_body:
  %t238 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t239 = load ptr, ptr %t238, align 8
  %t240 = getelementptr i8, ptr %t239, i64 256
  %t241 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t242 = load ptr, ptr %t241, align 8
  %t243 = getelementptr i8, ptr %t242, i64 128
  %t244 = getelementptr i8, ptr %t243, i64 20
  %t245 = load i32, ptr %t244, align 4
  %t246 = getelementptr i8, ptr %t240, i64 20
  %t247 = load i32, ptr %t246, align 4
  %t254 = load i32, ptr %t243, align 4
  %t255 = getelementptr i8, ptr %t243, i64 16
  %t256 = load i8, ptr %t255, align 1
  %t248 = icmp eq i32 %t245, 2
  %t249 = icmp eq i32 %t245, 1
  %t250 = icmp eq i32 %t245, 5
  %t251 = or i1 %t249, %t250
  %t252 = or i1 %t251, %t248
  br i1 %t252, label %zr_aot_stack_copy_transfer_265, label %zr_aot_stack_copy_weak_check_265
zr_aot_stack_copy_transfer_265:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t240)
  %t266 = load %SZrTypeValue, ptr %t243, align 32
  store %SZrTypeValue %t266, ptr %t240, align 32
  %t267 = getelementptr i8, ptr %t243, i64 8
  %t268 = getelementptr i8, ptr %t243, i64 16
  %t269 = getelementptr i8, ptr %t243, i64 17
  %t270 = getelementptr i8, ptr %t243, i64 20
  %t271 = getelementptr i8, ptr %t243, i64 24
  %t272 = getelementptr i8, ptr %t243, i64 32
  store i32 0, ptr %t243, align 4
  store i64 0, ptr %t267, align 8
  store i8 0, ptr %t268, align 1
  store i8 1, ptr %t269, align 1
  store i32 0, ptr %t270, align 4
  store ptr null, ptr %t271, align 8
  store ptr null, ptr %t272, align 8
  br label %zr_aot_fn_1_ins_14
zr_aot_stack_copy_weak_check_265:
  %t253 = icmp eq i32 %t245, 3
  br i1 %t253, label %zr_aot_stack_copy_weak_265, label %zr_aot_stack_copy_fast_check_265
zr_aot_stack_copy_weak_265:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t240, ptr %t243)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t243)
  br label %zr_aot_fn_1_ins_14
zr_aot_stack_copy_fast_check_265:
  %t257 = icmp ne i8 %t256, 0
  %t258 = icmp eq i32 %t254, 18
  %t259 = and i1 %t257, %t258
  %t260 = icmp eq i32 %t245, 0
  %t261 = icmp eq i32 %t247, 0
  %t262 = and i1 %t260, %t261
  %t263 = xor i1 %t259, true
  %t264 = and i1 %t262, %t263
  br i1 %t264, label %zr_aot_stack_copy_fast_265, label %zr_aot_stack_copy_slow_265
zr_aot_stack_copy_fast_265:
  %t273 = load %SZrTypeValue, ptr %t243, align 32
  store %SZrTypeValue %t273, ptr %t240, align 32
  br label %zr_aot_fn_1_ins_14
zr_aot_stack_copy_slow_265:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t240, ptr %t243)
  br label %zr_aot_fn_1_ins_14

zr_aot_fn_1_ins_14:
  %t274 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 14, i32 0)
  br i1 %t274, label %zr_aot_fn_1_ins_14_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_14_body:
  %t275 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t276 = load ptr, ptr %t275, align 8
  %t277 = getelementptr i8, ptr %t276, i64 192
  %t278 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t279 = load ptr, ptr %t278, align 8
  %t280 = getelementptr i8, ptr %t279, i64 256
  %t281 = getelementptr i8, ptr %t280, i64 20
  %t282 = load i32, ptr %t281, align 4
  %t283 = getelementptr i8, ptr %t277, i64 20
  %t284 = load i32, ptr %t283, align 4
  %t291 = load i32, ptr %t280, align 4
  %t292 = getelementptr i8, ptr %t280, i64 16
  %t293 = load i8, ptr %t292, align 1
  %t285 = icmp eq i32 %t282, 2
  %t286 = icmp eq i32 %t282, 1
  %t287 = icmp eq i32 %t282, 5
  %t288 = or i1 %t286, %t287
  %t289 = or i1 %t288, %t285
  br i1 %t289, label %zr_aot_stack_copy_transfer_302, label %zr_aot_stack_copy_weak_check_302
zr_aot_stack_copy_transfer_302:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t277)
  %t303 = load %SZrTypeValue, ptr %t280, align 32
  store %SZrTypeValue %t303, ptr %t277, align 32
  %t304 = getelementptr i8, ptr %t280, i64 8
  %t305 = getelementptr i8, ptr %t280, i64 16
  %t306 = getelementptr i8, ptr %t280, i64 17
  %t307 = getelementptr i8, ptr %t280, i64 20
  %t308 = getelementptr i8, ptr %t280, i64 24
  %t309 = getelementptr i8, ptr %t280, i64 32
  store i32 0, ptr %t280, align 4
  store i64 0, ptr %t304, align 8
  store i8 0, ptr %t305, align 1
  store i8 1, ptr %t306, align 1
  store i32 0, ptr %t307, align 4
  store ptr null, ptr %t308, align 8
  store ptr null, ptr %t309, align 8
  br label %zr_aot_fn_1_ins_15
zr_aot_stack_copy_weak_check_302:
  %t290 = icmp eq i32 %t282, 3
  br i1 %t290, label %zr_aot_stack_copy_weak_302, label %zr_aot_stack_copy_fast_check_302
zr_aot_stack_copy_weak_302:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t277, ptr %t280)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t280)
  br label %zr_aot_fn_1_ins_15
zr_aot_stack_copy_fast_check_302:
  %t294 = icmp ne i8 %t293, 0
  %t295 = icmp eq i32 %t291, 18
  %t296 = and i1 %t294, %t295
  %t297 = icmp eq i32 %t282, 0
  %t298 = icmp eq i32 %t284, 0
  %t299 = and i1 %t297, %t298
  %t300 = xor i1 %t296, true
  %t301 = and i1 %t299, %t300
  br i1 %t301, label %zr_aot_stack_copy_fast_302, label %zr_aot_stack_copy_slow_302
zr_aot_stack_copy_fast_302:
  %t310 = load %SZrTypeValue, ptr %t280, align 32
  store %SZrTypeValue %t310, ptr %t277, align 32
  br label %zr_aot_fn_1_ins_15
zr_aot_stack_copy_slow_302:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t277, ptr %t280)
  br label %zr_aot_fn_1_ins_15

zr_aot_fn_1_ins_15:
  %t311 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 15, i32 0)
  br i1 %t311, label %zr_aot_fn_1_ins_15_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_15_body:
  %t312 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t313 = load ptr, ptr %t312, align 8
  %t314 = getelementptr i8, ptr %t313, i64 256
  %t315 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t316 = load ptr, ptr %t315, align 8
  %t317 = load i32, ptr %t316, align 4
  %t318 = getelementptr i8, ptr %t316, i64 8
  %t319 = load i64, ptr %t318, align 8
  %t320 = icmp uge i32 %t317, 2
  %t321 = icmp ule i32 %t317, 5
  %t322 = and i1 %t320, %t321
  br i1 %t322, label %zr_aot_add_int_const_fast_323, label %zr_aot_fn_1_fail
zr_aot_add_int_const_fast_323:
  %t324 = add i64 %t319, 5
  %t325 = getelementptr i8, ptr %t314, i64 8
  %t326 = getelementptr i8, ptr %t314, i64 16
  %t327 = getelementptr i8, ptr %t314, i64 17
  %t328 = getelementptr i8, ptr %t314, i64 20
  %t329 = getelementptr i8, ptr %t314, i64 24
  %t330 = getelementptr i8, ptr %t314, i64 32
  store i32 5, ptr %t314, align 4
  store i64 %t324, ptr %t325, align 8
  store i8 0, ptr %t326, align 1
  store i8 1, ptr %t327, align 1
  store i32 0, ptr %t328, align 4
  store ptr null, ptr %t329, align 8
  store ptr null, ptr %t330, align 8
  br label %zr_aot_fn_1_ins_16

zr_aot_fn_1_ins_16:
  %t331 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 16, i32 0)
  br i1 %t331, label %zr_aot_fn_1_ins_16_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_16_body:
  %t332 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t333 = load ptr, ptr %t332, align 8
  %t334 = getelementptr i8, ptr %t333, i64 256
  %t335 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t336 = load ptr, ptr %t335, align 8
  %t337 = getelementptr i8, ptr %t336, i64 256
  %t338 = load i32, ptr %t337, align 4
  %t339 = getelementptr i8, ptr %t337, i64 8
  %t340 = load i64, ptr %t339, align 8
  %t341 = icmp uge i32 %t338, 2
  %t342 = icmp ule i32 %t338, 5
  %t343 = and i1 %t341, %t342
  br i1 %t343, label %zr_aot_add_int_const_fast_344, label %zr_aot_fn_1_fail
zr_aot_add_int_const_fast_344:
  %t345 = add i64 %t340, 2
  %t346 = getelementptr i8, ptr %t334, i64 8
  %t347 = getelementptr i8, ptr %t334, i64 16
  %t348 = getelementptr i8, ptr %t334, i64 17
  %t349 = getelementptr i8, ptr %t334, i64 20
  %t350 = getelementptr i8, ptr %t334, i64 24
  %t351 = getelementptr i8, ptr %t334, i64 32
  store i32 5, ptr %t334, align 4
  store i64 %t345, ptr %t346, align 8
  store i8 0, ptr %t347, align 1
  store i8 1, ptr %t348, align 1
  store i32 0, ptr %t349, align 4
  store ptr null, ptr %t350, align 8
  store ptr null, ptr %t351, align 8
  br label %zr_aot_fn_1_ins_17

zr_aot_fn_1_ins_17:
  %t352 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 17, i32 8)
  br i1 %t352, label %zr_aot_fn_1_ins_17_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_17_body:
  %t353 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 4, i1 false)
  ret i64 %t353

zr_aot_fn_1_end_unsupported:
  %t354 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 1, i32 18, i32 0)
  ret i64 %t354

zr_aot_fn_1_fail:
  %t355 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t355
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
