; ZR AOT LLVM Backend
; SemIR overlay + generated exec thunks.
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
  %t2 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 0, i32 0)
  br i1 %t2, label %zr_aot_fn_0_ins_1, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t3, label %zr_aot_fn_0_ins_1_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 1, i32 0)
  br i1 %t4, label %zr_aot_fn_0_ins_2, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t5, label %zr_aot_fn_0_ins_2_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_2_body:
  %t6 = call i1 @ZrLibrary_AotRuntime_CreateClosure(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t6, label %zr_aot_fn_0_ins_3, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_3:
  %t7 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 0)
  br i1 %t7, label %zr_aot_fn_0_ins_3_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_body:
  %t8 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 0, i32 2)
  br i1 %t8, label %zr_aot_fn_0_ins_4, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_4:
  %t9 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 0)
  br i1 %t9, label %zr_aot_fn_0_ins_4_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_4_body:
  %t10 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 4)
  br i1 %t10, label %zr_aot_fn_0_ins_5, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_5:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 1)
  br i1 %t11, label %zr_aot_fn_0_ins_5_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_5_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 0)
  br i1 %t12, label %zr_aot_fn_0_ins_6, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_6:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 0)
  br i1 %t13, label %zr_aot_fn_0_ins_6_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_6_body:
  %t14 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 3, i32 4)
  br i1 %t14, label %zr_aot_fn_0_ins_7, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_7:
  %t15 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 1)
  br i1 %t15, label %zr_aot_fn_0_ins_7_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_7_body:
  %t16 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 0)
  br i1 %t16, label %zr_aot_fn_0_ins_8, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_8:
  %t17 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 8, i32 0)
  br i1 %t17, label %zr_aot_fn_0_ins_8_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_8_body:
  %t18 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 4)
  br i1 %t18, label %zr_aot_fn_0_ins_9, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_9:
  %t19 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 9, i32 1)
  br i1 %t19, label %zr_aot_fn_0_ins_9_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_9_body:
  %t20 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 1)
  br i1 %t20, label %zr_aot_fn_0_ins_10, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_10:
  %t21 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 10, i32 1)
  br i1 %t21, label %zr_aot_fn_0_ins_10_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_10_body:
  %t22 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 5, i32 8)
  br i1 %t22, label %zr_aot_fn_0_ins_11, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_11:
  %t23 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 11, i32 0)
  br i1 %t23, label %zr_aot_fn_0_ins_11_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_11_body:
  %t24 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 6, i32 4)
  br i1 %t24, label %zr_aot_fn_0_ins_12, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_12:
  %t25 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 12, i32 1)
  br i1 %t25, label %zr_aot_fn_0_ins_12_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_12_body:
  %t26 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 7, i32 9)
  br i1 %t26, label %zr_aot_fn_0_ins_13, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_13:
  %t27 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 13, i32 1)
  br i1 %t27, label %zr_aot_fn_0_ins_13_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_13_body:
  %t28 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 8, i32 10)
  br i1 %t28, label %zr_aot_fn_0_ins_14, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_14:
  %t29 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 14, i32 0)
  br i1 %t29, label %zr_aot_fn_0_ins_14_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_14_body:
  %t30 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 9, i32 3)
  br i1 %t30, label %zr_aot_fn_0_ins_15, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_15:
  %t31 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 15, i32 5)
  br i1 %t31, label %zr_aot_fn_0_ins_15_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_15_body:
  %t32 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 5, i32 5, i32 4, ptr %direct_call)
  br i1 %t32, label %zr_aot_fn_0_ins_15_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_15_prepare_ok:
  %t33 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 5, i32 5, i32 4, i32 1)
  br i1 %t33, label %zr_aot_fn_0_ins_15_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_15_finish_ok:
  br label %zr_aot_fn_0_ins_16

zr_aot_fn_0_ins_16:
  %t34 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 16, i32 1)
  br i1 %t34, label %zr_aot_fn_0_ins_16_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_16_body:
  %t35 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 5, i32 0)
  br i1 %t35, label %zr_aot_fn_0_ins_17, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_17:
  %t36 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 17, i32 1)
  br i1 %t36, label %zr_aot_fn_0_ins_17_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_17_body:
  %t37 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 6, i32 0)
  br i1 %t37, label %zr_aot_fn_0_ins_18, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_18:
  %t38 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 18, i32 1)
  br i1 %t38, label %zr_aot_fn_0_ins_18_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_18_body:
  %t39 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 7, i32 0)
  br i1 %t39, label %zr_aot_fn_0_ins_19, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_19:
  %t40 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 19, i32 1)
  br i1 %t40, label %zr_aot_fn_0_ins_19_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_19_body:
  %t41 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 8, i32 0)
  br i1 %t41, label %zr_aot_fn_0_ins_20, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_20:
  %t42 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 20, i32 1)
  br i1 %t42, label %zr_aot_fn_0_ins_20_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_20_body:
  %t43 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 0)
  br i1 %t43, label %zr_aot_fn_0_ins_21, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_21:
  %t44 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 21, i32 0)
  br i1 %t44, label %zr_aot_fn_0_ins_21_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_21_body:
  %t45 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 6)
  br i1 %t45, label %zr_aot_fn_0_ins_22, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_22:
  %t46 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 22, i32 1)
  br i1 %t46, label %zr_aot_fn_0_ins_22_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_22_body:
  %t47 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 6, i32 6, i32 2)
  br i1 %t47, label %zr_aot_fn_0_ins_23, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_23:
  %t48 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 23, i32 0)
  br i1 %t48, label %zr_aot_fn_0_ins_23_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_23_body:
  %t49 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 5, i32 6)
  br i1 %t49, label %zr_aot_fn_0_ins_24, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_24:
  %t50 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 24, i32 1)
  br i1 %t50, label %zr_aot_fn_0_ins_24_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_24_body:
  %t51 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 6, i32 0)
  br i1 %t51, label %zr_aot_fn_0_ins_25, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_25:
  %t52 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 25, i32 0)
  br i1 %t52, label %zr_aot_fn_0_ins_25_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_25_body:
  %t53 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 6)
  br i1 %t53, label %zr_aot_fn_0_ins_26, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_26:
  %t54 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 26, i32 1)
  br i1 %t54, label %zr_aot_fn_0_ins_26_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_26_body:
  %t55 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 6, i32 6, i32 1)
  br i1 %t55, label %zr_aot_fn_0_ins_27, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_27:
  %t56 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 27, i32 1)
  br i1 %t56, label %zr_aot_fn_0_ins_27_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_27_body:
  %t57 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 7, i32 11)
  br i1 %t57, label %zr_aot_fn_0_ins_28, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_28:
  %t58 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 28, i32 0)
  br i1 %t58, label %zr_aot_fn_0_ins_28_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_28_body:
  %t59 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 8, i32 6)
  br i1 %t59, label %zr_aot_fn_0_ins_29, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_29:
  %t60 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 29, i32 1)
  br i1 %t60, label %zr_aot_fn_0_ins_29_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_29_body:
  %t61 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 12)
  br i1 %t61, label %zr_aot_fn_0_ins_30, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_30:
  %t62 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 30, i32 1)
  br i1 %t62, label %zr_aot_fn_0_ins_30_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_30_body:
  %t63 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 10, i32 13)
  br i1 %t63, label %zr_aot_fn_0_ins_31, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_31:
  %t64 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 31, i32 0)
  br i1 %t64, label %zr_aot_fn_0_ins_31_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_31_body:
  %t65 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 11, i32 5)
  br i1 %t65, label %zr_aot_fn_0_ins_32, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_32:
  %t66 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 32, i32 5)
  br i1 %t66, label %zr_aot_fn_0_ins_32_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_32_body:
  %t67 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 7, i32 7, i32 4, ptr %direct_call)
  br i1 %t67, label %zr_aot_fn_0_ins_32_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_32_prepare_ok:
  %t68 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 7, i32 7, i32 4, i32 1)
  br i1 %t68, label %zr_aot_fn_0_ins_32_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_32_finish_ok:
  br label %zr_aot_fn_0_ins_33

zr_aot_fn_0_ins_33:
  %t69 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 33, i32 1)
  br i1 %t69, label %zr_aot_fn_0_ins_33_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_33_body:
  %t70 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 7, i32 0)
  br i1 %t70, label %zr_aot_fn_0_ins_34, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_34:
  %t71 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 34, i32 1)
  br i1 %t71, label %zr_aot_fn_0_ins_34_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_34_body:
  %t72 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 8, i32 0)
  br i1 %t72, label %zr_aot_fn_0_ins_35, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_35:
  %t73 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 35, i32 1)
  br i1 %t73, label %zr_aot_fn_0_ins_35_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_35_body:
  %t74 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 0)
  br i1 %t74, label %zr_aot_fn_0_ins_36, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_36:
  %t75 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 36, i32 1)
  br i1 %t75, label %zr_aot_fn_0_ins_36_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_36_body:
  %t76 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 10, i32 0)
  br i1 %t76, label %zr_aot_fn_0_ins_37, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_37:
  %t77 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 37, i32 1)
  br i1 %t77, label %zr_aot_fn_0_ins_37_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_37_body:
  %t78 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 11, i32 0)
  br i1 %t78, label %zr_aot_fn_0_ins_38, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_38:
  %t79 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 38, i32 0)
  br i1 %t79, label %zr_aot_fn_0_ins_38_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_38_body:
  %t80 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 8)
  br i1 %t80, label %zr_aot_fn_0_ins_39, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_39:
  %t81 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 39, i32 1)
  br i1 %t81, label %zr_aot_fn_0_ins_39_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_39_body:
  %t82 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 8, i32 8, i32 3)
  br i1 %t82, label %zr_aot_fn_0_ins_40, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_40:
  %t83 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 40, i32 0)
  br i1 %t83, label %zr_aot_fn_0_ins_40_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_40_body:
  %t84 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 7, i32 8)
  br i1 %t84, label %zr_aot_fn_0_ins_41, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_41:
  %t85 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 41, i32 1)
  br i1 %t85, label %zr_aot_fn_0_ins_41_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_41_body:
  %t86 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 8, i32 0)
  br i1 %t86, label %zr_aot_fn_0_ins_42, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_42:
  %t87 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 42, i32 0)
  br i1 %t87, label %zr_aot_fn_0_ins_42_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_42_body:
  %t88 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 8)
  br i1 %t88, label %zr_aot_fn_0_ins_43, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_43:
  %t89 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 43, i32 1)
  br i1 %t89, label %zr_aot_fn_0_ins_43_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_43_body:
  %t90 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 8, i32 8, i32 1)
  br i1 %t90, label %zr_aot_fn_0_ins_44, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_44:
  %t91 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 44, i32 1)
  br i1 %t91, label %zr_aot_fn_0_ins_44_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_44_body:
  %t92 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 14)
  br i1 %t92, label %zr_aot_fn_0_ins_45, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_45:
  %t93 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 45, i32 0)
  br i1 %t93, label %zr_aot_fn_0_ins_45_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_45_body:
  %t94 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 10, i32 8)
  br i1 %t94, label %zr_aot_fn_0_ins_46, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_46:
  %t95 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 46, i32 1)
  br i1 %t95, label %zr_aot_fn_0_ins_46_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_46_body:
  %t96 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 11, i32 15)
  br i1 %t96, label %zr_aot_fn_0_ins_47, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_47:
  %t97 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 47, i32 1)
  br i1 %t97, label %zr_aot_fn_0_ins_47_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_47_body:
  %t98 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 12, i32 16)
  br i1 %t98, label %zr_aot_fn_0_ins_48, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_48:
  %t99 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 48, i32 0)
  br i1 %t99, label %zr_aot_fn_0_ins_48_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_48_body:
  %t100 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 13, i32 7)
  br i1 %t100, label %zr_aot_fn_0_ins_49, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_49:
  %t101 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 49, i32 5)
  br i1 %t101, label %zr_aot_fn_0_ins_49_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_49_body:
  %t102 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 9, i32 9, i32 4, ptr %direct_call)
  br i1 %t102, label %zr_aot_fn_0_ins_49_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_49_prepare_ok:
  %t103 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 9, i32 9, i32 4, i32 1)
  br i1 %t103, label %zr_aot_fn_0_ins_49_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_49_finish_ok:
  br label %zr_aot_fn_0_ins_50

zr_aot_fn_0_ins_50:
  %t104 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 50, i32 1)
  br i1 %t104, label %zr_aot_fn_0_ins_50_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_50_body:
  %t105 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 0)
  br i1 %t105, label %zr_aot_fn_0_ins_51, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_51:
  %t106 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 51, i32 1)
  br i1 %t106, label %zr_aot_fn_0_ins_51_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_51_body:
  %t107 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 10, i32 0)
  br i1 %t107, label %zr_aot_fn_0_ins_52, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_52:
  %t108 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 52, i32 1)
  br i1 %t108, label %zr_aot_fn_0_ins_52_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_52_body:
  %t109 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 11, i32 0)
  br i1 %t109, label %zr_aot_fn_0_ins_53, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_53:
  %t110 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 53, i32 1)
  br i1 %t110, label %zr_aot_fn_0_ins_53_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_53_body:
  %t111 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 12, i32 0)
  br i1 %t111, label %zr_aot_fn_0_ins_54, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_54:
  %t112 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 54, i32 1)
  br i1 %t112, label %zr_aot_fn_0_ins_54_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_54_body:
  %t113 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 13, i32 0)
  br i1 %t113, label %zr_aot_fn_0_ins_55, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_55:
  %t114 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 55, i32 0)
  br i1 %t114, label %zr_aot_fn_0_ins_55_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_55_body:
  %t115 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 9)
  br i1 %t115, label %zr_aot_fn_0_ins_56, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_56:
  %t116 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 56, i32 1)
  br i1 %t116, label %zr_aot_fn_0_ins_56_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_56_body:
  %t117 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 9, i32 9, i32 1)
  br i1 %t117, label %zr_aot_fn_0_ins_57, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_57:
  %t118 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 57, i32 0)
  br i1 %t118, label %zr_aot_fn_0_ins_57_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_57_body:
  %t119 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 11)
  br i1 %t119, label %zr_aot_fn_0_ins_58, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_58:
  %t120 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 58, i32 1)
  br i1 %t120, label %zr_aot_fn_0_ins_58_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_58_body:
  %t121 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 11, i32 11, i32 4)
  br i1 %t121, label %zr_aot_fn_0_ins_59, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_59:
  %t122 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 59, i32 0)
  br i1 %t122, label %zr_aot_fn_0_ins_59_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_59_body:
  %t123 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 10, i32 11)
  br i1 %t123, label %zr_aot_fn_0_ins_60, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_60:
  %t124 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 60, i32 1)
  br i1 %t124, label %zr_aot_fn_0_ins_60_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_60_body:
  %t125 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 11, i32 0)
  br i1 %t125, label %zr_aot_fn_0_ins_61, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_61:
  %t126 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 61, i32 1)
  br i1 %t126, label %zr_aot_fn_0_ins_61_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_61_body:
  %t127 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 11, i32 17)
  br i1 %t127, label %zr_aot_fn_0_ins_62, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_62:
  %t128 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 62, i32 0)
  br i1 %t128, label %zr_aot_fn_0_ins_62_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_62_body:
  %t129 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 12, i32 9)
  br i1 %t129, label %zr_aot_fn_0_ins_63, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_63:
  %t130 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 63, i32 0)
  br i1 %t130, label %zr_aot_fn_0_ins_63_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_63_body:
  %t131 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 13, i32 10)
  br i1 %t131, label %zr_aot_fn_0_ins_64, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_64:
  %t132 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 64, i32 5)
  br i1 %t132, label %zr_aot_fn_0_ins_64_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_64_body:
  %t133 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 11, i32 11, i32 2, ptr %direct_call)
  br i1 %t133, label %zr_aot_fn_0_ins_64_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_64_prepare_ok:
  %t134 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 11, i32 11, i32 2, i32 1)
  br i1 %t134, label %zr_aot_fn_0_ins_64_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_64_finish_ok:
  br label %zr_aot_fn_0_ins_65

zr_aot_fn_0_ins_65:
  %t135 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 65, i32 1)
  br i1 %t135, label %zr_aot_fn_0_ins_65_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_65_body:
  %t136 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 11, i32 0)
  br i1 %t136, label %zr_aot_fn_0_ins_66, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_66:
  %t137 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 66, i32 1)
  br i1 %t137, label %zr_aot_fn_0_ins_66_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_66_body:
  %t138 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 12, i32 0)
  br i1 %t138, label %zr_aot_fn_0_ins_67, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_67:
  %t139 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 67, i32 1)
  br i1 %t139, label %zr_aot_fn_0_ins_67_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_67_body:
  %t140 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 13, i32 0)
  br i1 %t140, label %zr_aot_fn_0_ins_68, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_68:
  %t141 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 68, i32 1)
  br i1 %t141, label %zr_aot_fn_0_ins_68_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_68_body:
  %t142 = call i1 @ZrLibrary_AotRuntime_CreateClosure(ptr %state, ptr %frame, i32 11, i32 18)
  br i1 %t142, label %zr_aot_fn_0_ins_69, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_69:
  %t143 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 69, i32 0)
  br i1 %t143, label %zr_aot_fn_0_ins_69_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_69_body:
  %t144 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 1, i32 11)
  br i1 %t144, label %zr_aot_fn_0_ins_70, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_70:
  %t145 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 70, i32 0)
  br i1 %t145, label %zr_aot_fn_0_ins_70_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_70_body:
  %t146 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 13, i32 0)
  br i1 %t146, label %zr_aot_fn_0_ins_71, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_71:
  %t147 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 71, i32 0)
  br i1 %t147, label %zr_aot_fn_0_ins_71_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_71_body:
  %t148 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 12, i32 13)
  br i1 %t148, label %zr_aot_fn_0_ins_72, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_72:
  %t149 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 72, i32 1)
  br i1 %t149, label %zr_aot_fn_0_ins_72_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_72_body:
  %t150 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 13, i32 0)
  br i1 %t150, label %zr_aot_fn_0_ins_73, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_73:
  %t151 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 73, i32 1)
  br i1 %t151, label %zr_aot_fn_0_ins_73_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_73_body:
  %t152 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 13, i32 19)
  br i1 %t152, label %zr_aot_fn_0_ins_74, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_74:
  %t153 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 74, i32 0)
  br i1 %t153, label %zr_aot_fn_0_ins_74_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_74_body:
  %t154 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 14, i32 1)
  br i1 %t154, label %zr_aot_fn_0_ins_75, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_75:
  %t155 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 75, i32 0)
  br i1 %t155, label %zr_aot_fn_0_ins_75_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_75_body:
  %t156 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 15, i32 12)
  br i1 %t156, label %zr_aot_fn_0_ins_76, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_76:
  %t157 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 76, i32 5)
  br i1 %t157, label %zr_aot_fn_0_ins_76_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_76_body:
  %t158 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 13, i32 13, i32 2, ptr %direct_call)
  br i1 %t158, label %zr_aot_fn_0_ins_76_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_76_prepare_ok:
  %t159 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 13, i32 13, i32 2, i32 1)
  br i1 %t159, label %zr_aot_fn_0_ins_76_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_76_finish_ok:
  br label %zr_aot_fn_0_ins_77

zr_aot_fn_0_ins_77:
  %t160 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 77, i32 0)
  br i1 %t160, label %zr_aot_fn_0_ins_77_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_77_body:
  %t161 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 1, i32 13)
  br i1 %t161, label %zr_aot_fn_0_ins_78, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_78:
  %t162 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 78, i32 1)
  br i1 %t162, label %zr_aot_fn_0_ins_78_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_78_body:
  %t163 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 13, i32 0)
  br i1 %t163, label %zr_aot_fn_0_ins_79, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_79:
  %t164 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 79, i32 1)
  br i1 %t164, label %zr_aot_fn_0_ins_79_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_79_body:
  %t165 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 14, i32 0)
  br i1 %t165, label %zr_aot_fn_0_ins_80, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_80:
  %t166 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 80, i32 1)
  br i1 %t166, label %zr_aot_fn_0_ins_80_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_80_body:
  %t167 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 15, i32 0)
  br i1 %t167, label %zr_aot_fn_0_ins_81, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_81:
  %t168 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 81, i32 0)
  br i1 %t168, label %zr_aot_fn_0_ins_81_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_81_body:
  %t169 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 13, i32 1)
  br i1 %t169, label %zr_aot_fn_0_ins_82, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_82:
  %t170 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 82, i32 13)
  br i1 %t170, label %zr_aot_fn_0_ins_82_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_82_body:
  %t171 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 13, i32 13, i32 0, ptr %direct_call)
  br i1 %t171, label %zr_aot_fn_0_ins_82_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_82_prepare_ok:
  %t172 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 13, i32 13, i32 0, i32 1)
  br i1 %t172, label %zr_aot_fn_0_ins_82_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_82_finish_ok:
  %t173 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 13, i1 true)
  ret i64 %t173

zr_aot_fn_0_ins_83:
  %t174 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 83, i32 8)
  br i1 %t174, label %zr_aot_fn_0_ins_83_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_83_body:
  %t175 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 13, i1 true)
  ret i64 %t175

zr_aot_fn_0_end_unsupported:
  %t176 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 0, i32 84, i32 0)
  ret i64 %t176

zr_aot_fn_0_fail:
  %t177 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t177
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
  %t2 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 0)
  br i1 %t2, label %zr_aot_fn_1_ins_1, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 0)
  br i1 %t3, label %zr_aot_fn_1_ins_1_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t4, label %zr_aot_fn_1_ins_2, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t5, label %zr_aot_fn_1_ins_2_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_2_body:
  %t6 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 3, i32 3, i32 0)
  br i1 %t6, label %zr_aot_fn_1_ins_3, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_3:
  %t7 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t7, label %zr_aot_fn_1_ins_3_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_3_body:
  %t8 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t8, label %zr_aot_fn_1_ins_4, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_4:
  %t9 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t9, label %zr_aot_fn_1_ins_4_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_4_body:
  %t10 = call i1 @ZrLibrary_AotRuntime_SetByIndex(ptr %state, ptr %frame, i32 2, i32 3, i32 4)
  br i1 %t10, label %zr_aot_fn_1_ins_5, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_5:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 1)
  br i1 %t11, label %zr_aot_fn_1_ins_5_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_5_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 2)
  br i1 %t12, label %zr_aot_fn_1_ins_6, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_6:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 1)
  br i1 %t13, label %zr_aot_fn_1_ins_6_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_6_body:
  %t14 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 2)
  br i1 %t14, label %zr_aot_fn_1_ins_7, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_7:
  %t15 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 1)
  br i1 %t15, label %zr_aot_fn_1_ins_7_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_7_body:
  %t16 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t16, label %zr_aot_fn_1_ins_8, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_8:
  %t17 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 8, i32 1)
  br i1 %t17, label %zr_aot_fn_1_ins_8_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_8_body:
  %t18 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t18, label %zr_aot_fn_1_ins_9, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_9:
  %t19 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 9, i32 8)
  br i1 %t19, label %zr_aot_fn_1_ins_9_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_9_body:
  %t20 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 false)
  ret i64 %t20

zr_aot_fn_1_end_unsupported:
  %t21 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 1, i32 10, i32 0)
  ret i64 %t21

zr_aot_fn_1_fail:
  %t22 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t22
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
  %t2 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 1, i32 0)
  br i1 %t2, label %zr_aot_fn_2_ins_1, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 0)
  br i1 %t3, label %zr_aot_fn_2_ins_1_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 2, i32 0)
  br i1 %t4, label %zr_aot_fn_2_ins_2, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t5, label %zr_aot_fn_2_ins_2_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_2_body:
  %t6 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 2, i32 2, i32 0)
  br i1 %t6, label %zr_aot_fn_2_ins_3, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_3:
  %t7 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t7, label %zr_aot_fn_2_ins_3_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_3_body:
  %t8 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t8, label %zr_aot_fn_2_ins_4, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_4:
  %t9 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t9, label %zr_aot_fn_2_ins_4_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_4_body:
  %t10 = call i1 @ZrLibrary_AotRuntime_SetByIndex(ptr %state, ptr %frame, i32 1, i32 2, i32 3)
  br i1 %t10, label %zr_aot_fn_2_ins_5, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_5:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 1)
  br i1 %t11, label %zr_aot_fn_2_ins_5_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_5_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 2)
  br i1 %t12, label %zr_aot_fn_2_ins_6, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_6:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 1)
  br i1 %t13, label %zr_aot_fn_2_ins_6_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_6_body:
  %t14 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t14, label %zr_aot_fn_2_ins_7, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_7:
  %t15 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 1)
  br i1 %t15, label %zr_aot_fn_2_ins_7_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_7_body:
  %t16 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 1, i32 2)
  br i1 %t16, label %zr_aot_fn_2_ins_8, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_8:
  %t17 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 8, i32 1)
  br i1 %t17, label %zr_aot_fn_2_ins_8_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_8_body:
  %t18 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 1, i32 2)
  br i1 %t18, label %zr_aot_fn_2_ins_9, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_9:
  %t19 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 9, i32 8)
  br i1 %t19, label %zr_aot_fn_2_ins_9_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_9_body:
  %t20 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 1, i1 false)
  ret i64 %t20

zr_aot_fn_2_end_unsupported:
  %t21 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 2, i32 10, i32 0)
  ret i64 %t21

zr_aot_fn_2_fail:
  %t22 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t22
}

define internal i64 @zr_aot_fn_3(ptr %state) {
entry:
  %frame = alloca %ZrAotGeneratedFrame, align 8
  %direct_call = alloca %ZrAotGeneratedDirectCall, align 8
  %resume_instruction = alloca i32, align 4
  %truthy_value = alloca i8, align 1
  %t0 = call i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr %state, i32 3, ptr %frame)
  br i1 %t0, label %zr_aot_fn_3_ins_0, label %zr_aot_fn_3_fail

zr_aot_fn_3_ins_0:
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 1)
  br i1 %t1, label %zr_aot_fn_3_ins_0_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_0_body:
  %t2 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 0)
  br i1 %t2, label %zr_aot_fn_3_ins_1, label %zr_aot_fn_3_fail

zr_aot_fn_3_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 0)
  br i1 %t3, label %zr_aot_fn_3_ins_1_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t4, label %zr_aot_fn_3_ins_2, label %zr_aot_fn_3_fail

zr_aot_fn_3_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t5, label %zr_aot_fn_3_ins_2_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_2_body:
  %t6 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 3, i32 3, i32 0)
  br i1 %t6, label %zr_aot_fn_3_ins_3, label %zr_aot_fn_3_fail

zr_aot_fn_3_ins_3:
  %t7 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t7, label %zr_aot_fn_3_ins_3_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_3_body:
  %t8 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t8, label %zr_aot_fn_3_ins_4, label %zr_aot_fn_3_fail

zr_aot_fn_3_ins_4:
  %t9 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t9, label %zr_aot_fn_3_ins_4_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_4_body:
  %t10 = call i1 @ZrLibrary_AotRuntime_SetByIndex(ptr %state, ptr %frame, i32 2, i32 3, i32 4)
  br i1 %t10, label %zr_aot_fn_3_ins_5, label %zr_aot_fn_3_fail

zr_aot_fn_3_ins_5:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 1)
  br i1 %t11, label %zr_aot_fn_3_ins_5_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_5_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 2)
  br i1 %t12, label %zr_aot_fn_3_ins_6, label %zr_aot_fn_3_fail

zr_aot_fn_3_ins_6:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 1)
  br i1 %t13, label %zr_aot_fn_3_ins_6_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_6_body:
  %t14 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 2)
  br i1 %t14, label %zr_aot_fn_3_ins_7, label %zr_aot_fn_3_fail

zr_aot_fn_3_ins_7:
  %t15 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 1)
  br i1 %t15, label %zr_aot_fn_3_ins_7_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_7_body:
  %t16 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t16, label %zr_aot_fn_3_ins_8, label %zr_aot_fn_3_fail

zr_aot_fn_3_ins_8:
  %t17 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 8, i32 1)
  br i1 %t17, label %zr_aot_fn_3_ins_8_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_8_body:
  %t18 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t18, label %zr_aot_fn_3_ins_9, label %zr_aot_fn_3_fail

zr_aot_fn_3_ins_9:
  %t19 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 9, i32 8)
  br i1 %t19, label %zr_aot_fn_3_ins_9_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_9_body:
  %t20 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 false)
  ret i64 %t20

zr_aot_fn_3_end_unsupported:
  %t21 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 3, i32 10, i32 0)
  ret i64 %t21

zr_aot_fn_3_fail:
  %t22 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t22
}

define internal i64 @zr_aot_fn_4(ptr %state) {
entry:
  %frame = alloca %ZrAotGeneratedFrame, align 8
  %direct_call = alloca %ZrAotGeneratedDirectCall, align 8
  %resume_instruction = alloca i32, align 4
  %truthy_value = alloca i8, align 1
  %t0 = call i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr %state, i32 4, ptr %frame)
  br i1 %t0, label %zr_aot_fn_4_ins_0, label %zr_aot_fn_4_fail

zr_aot_fn_4_ins_0:
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 1)
  br i1 %t1, label %zr_aot_fn_4_ins_0_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_0_body:
  %t2 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 0)
  br i1 %t2, label %zr_aot_fn_4_ins_1, label %zr_aot_fn_4_fail

zr_aot_fn_4_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 0)
  br i1 %t3, label %zr_aot_fn_4_ins_1_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t4, label %zr_aot_fn_4_ins_2, label %zr_aot_fn_4_fail

zr_aot_fn_4_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t5, label %zr_aot_fn_4_ins_2_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_2_body:
  %t6 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 3, i32 3, i32 0)
  br i1 %t6, label %zr_aot_fn_4_ins_3, label %zr_aot_fn_4_fail

zr_aot_fn_4_ins_3:
  %t7 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t7, label %zr_aot_fn_4_ins_3_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_3_body:
  %t8 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t8, label %zr_aot_fn_4_ins_4, label %zr_aot_fn_4_fail

zr_aot_fn_4_ins_4:
  %t9 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t9, label %zr_aot_fn_4_ins_4_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_4_body:
  %t10 = call i1 @ZrLibrary_AotRuntime_SetByIndex(ptr %state, ptr %frame, i32 2, i32 3, i32 4)
  br i1 %t10, label %zr_aot_fn_4_ins_5, label %zr_aot_fn_4_fail

zr_aot_fn_4_ins_5:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 1)
  br i1 %t11, label %zr_aot_fn_4_ins_5_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_5_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 2)
  br i1 %t12, label %zr_aot_fn_4_ins_6, label %zr_aot_fn_4_fail

zr_aot_fn_4_ins_6:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 1)
  br i1 %t13, label %zr_aot_fn_4_ins_6_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_6_body:
  %t14 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 2)
  br i1 %t14, label %zr_aot_fn_4_ins_7, label %zr_aot_fn_4_fail

zr_aot_fn_4_ins_7:
  %t15 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 1)
  br i1 %t15, label %zr_aot_fn_4_ins_7_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_7_body:
  %t16 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t16, label %zr_aot_fn_4_ins_8, label %zr_aot_fn_4_fail

zr_aot_fn_4_ins_8:
  %t17 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 8, i32 1)
  br i1 %t17, label %zr_aot_fn_4_ins_8_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_8_body:
  %t18 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t18, label %zr_aot_fn_4_ins_9, label %zr_aot_fn_4_fail

zr_aot_fn_4_ins_9:
  %t19 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 9, i32 8)
  br i1 %t19, label %zr_aot_fn_4_ins_9_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_9_body:
  %t20 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 false)
  ret i64 %t20

zr_aot_fn_4_end_unsupported:
  %t21 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 4, i32 10, i32 0)
  ret i64 %t21

zr_aot_fn_4_fail:
  %t22 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t22
}

define internal i64 @zr_aot_fn_5(ptr %state) {
entry:
  %frame = alloca %ZrAotGeneratedFrame, align 8
  %direct_call = alloca %ZrAotGeneratedDirectCall, align 8
  %resume_instruction = alloca i32, align 4
  %truthy_value = alloca i8, align 1
  %t0 = call i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr %state, i32 5, ptr %frame)
  br i1 %t0, label %zr_aot_fn_5_ins_0, label %zr_aot_fn_5_fail

zr_aot_fn_5_ins_0:
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 1)
  br i1 %t1, label %zr_aot_fn_5_ins_0_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_0_body:
  %t2 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 0)
  br i1 %t2, label %zr_aot_fn_5_ins_1, label %zr_aot_fn_5_fail

zr_aot_fn_5_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 0)
  br i1 %t3, label %zr_aot_fn_5_ins_1_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t4, label %zr_aot_fn_5_ins_2, label %zr_aot_fn_5_fail

zr_aot_fn_5_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t5, label %zr_aot_fn_5_ins_2_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_2_body:
  %t6 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 3, i32 3, i32 0)
  br i1 %t6, label %zr_aot_fn_5_ins_3, label %zr_aot_fn_5_fail

zr_aot_fn_5_ins_3:
  %t7 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t7, label %zr_aot_fn_5_ins_3_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_3_body:
  %t8 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t8, label %zr_aot_fn_5_ins_4, label %zr_aot_fn_5_fail

zr_aot_fn_5_ins_4:
  %t9 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t9, label %zr_aot_fn_5_ins_4_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_4_body:
  %t10 = call i1 @ZrLibrary_AotRuntime_SetByIndex(ptr %state, ptr %frame, i32 2, i32 3, i32 4)
  br i1 %t10, label %zr_aot_fn_5_ins_5, label %zr_aot_fn_5_fail

zr_aot_fn_5_ins_5:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 1)
  br i1 %t11, label %zr_aot_fn_5_ins_5_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_5_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 2)
  br i1 %t12, label %zr_aot_fn_5_ins_6, label %zr_aot_fn_5_fail

zr_aot_fn_5_ins_6:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 1)
  br i1 %t13, label %zr_aot_fn_5_ins_6_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_6_body:
  %t14 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 2)
  br i1 %t14, label %zr_aot_fn_5_ins_7, label %zr_aot_fn_5_fail

zr_aot_fn_5_ins_7:
  %t15 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 1)
  br i1 %t15, label %zr_aot_fn_5_ins_7_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_7_body:
  %t16 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t16, label %zr_aot_fn_5_ins_8, label %zr_aot_fn_5_fail

zr_aot_fn_5_ins_8:
  %t17 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 8, i32 1)
  br i1 %t17, label %zr_aot_fn_5_ins_8_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_8_body:
  %t18 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t18, label %zr_aot_fn_5_ins_9, label %zr_aot_fn_5_fail

zr_aot_fn_5_ins_9:
  %t19 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 9, i32 8)
  br i1 %t19, label %zr_aot_fn_5_ins_9_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_9_body:
  %t20 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 false)
  ret i64 %t20

zr_aot_fn_5_end_unsupported:
  %t21 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 5, i32 10, i32 0)
  ret i64 %t21

zr_aot_fn_5_fail:
  %t22 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t22
}

define internal i64 @zr_aot_fn_6(ptr %state) {
entry:
  %frame = alloca %ZrAotGeneratedFrame, align 8
  %direct_call = alloca %ZrAotGeneratedDirectCall, align 8
  %resume_instruction = alloca i32, align 4
  %truthy_value = alloca i8, align 1
  %t0 = call i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr %state, i32 6, ptr %frame)
  br i1 %t0, label %zr_aot_fn_6_ins_0, label %zr_aot_fn_6_fail

zr_aot_fn_6_ins_0:
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 0)
  br i1 %t1, label %zr_aot_fn_6_ins_0_body, label %zr_aot_fn_6_fail
zr_aot_fn_6_ins_0_body:
  %t2 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t2, label %zr_aot_fn_6_ins_1, label %zr_aot_fn_6_fail

zr_aot_fn_6_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 8)
  br i1 %t3, label %zr_aot_fn_6_ins_1_body, label %zr_aot_fn_6_fail
zr_aot_fn_6_ins_1_body:
  %t4 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 false)
  ret i64 %t4

zr_aot_fn_6_end_unsupported:
  %t5 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 6, i32 2, i32 0)
  ret i64 %t5

zr_aot_fn_6_fail:
  %t6 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t6
}

define internal i64 @zr_aot_fn_7(ptr %state) {
entry:
  %frame = alloca %ZrAotGeneratedFrame, align 8
  %direct_call = alloca %ZrAotGeneratedDirectCall, align 8
  %resume_instruction = alloca i32, align 4
  %truthy_value = alloca i8, align 1
  %t0 = call i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr %state, i32 7, ptr %frame)
  br i1 %t0, label %zr_aot_fn_7_ins_0, label %zr_aot_fn_7_fail

zr_aot_fn_7_ins_0:
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 0)
  br i1 %t1, label %zr_aot_fn_7_ins_0_body, label %zr_aot_fn_7_fail
zr_aot_fn_7_ins_0_body:
  %t2 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 1, i32 0)
  br i1 %t2, label %zr_aot_fn_7_ins_1, label %zr_aot_fn_7_fail

zr_aot_fn_7_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t3, label %zr_aot_fn_7_ins_1_body, label %zr_aot_fn_7_fail
zr_aot_fn_7_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 1, i32 1, i32 0)
  br i1 %t4, label %zr_aot_fn_7_ins_2, label %zr_aot_fn_7_fail

zr_aot_fn_7_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 8)
  br i1 %t5, label %zr_aot_fn_7_ins_2_body, label %zr_aot_fn_7_fail
zr_aot_fn_7_ins_2_body:
  %t6 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 1, i1 false)
  ret i64 %t6

zr_aot_fn_7_end_unsupported:
  %t7 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 7, i32 3, i32 0)
  ret i64 %t7

zr_aot_fn_7_fail:
  %t8 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t8
}

define internal i64 @zr_aot_fn_8(ptr %state) {
entry:
  %frame = alloca %ZrAotGeneratedFrame, align 8
  %direct_call = alloca %ZrAotGeneratedDirectCall, align 8
  %resume_instruction = alloca i32, align 4
  %truthy_value = alloca i8, align 1
  %t0 = call i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr %state, i32 8, ptr %frame)
  br i1 %t0, label %zr_aot_fn_8_ins_0, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_0:
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 0)
  br i1 %t1, label %zr_aot_fn_8_ins_0_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_0_body:
  %t2 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 2)
  br i1 %t2, label %zr_aot_fn_8_ins_1, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t3, label %zr_aot_fn_8_ins_1_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 2, i32 2, i32 0)
  br i1 %t4, label %zr_aot_fn_8_ins_2, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 0)
  br i1 %t5, label %zr_aot_fn_8_ins_2_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_2_body:
  %t6 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 1, i32 2)
  br i1 %t6, label %zr_aot_fn_8_ins_3, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_3:
  %t7 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t7, label %zr_aot_fn_8_ins_3_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_3_body:
  %t8 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 0)
  br i1 %t8, label %zr_aot_fn_8_ins_4, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_4:
  %t9 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t9, label %zr_aot_fn_8_ins_4_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_4_body:
  %t10 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t10, label %zr_aot_fn_8_ins_5, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_5:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 1)
  br i1 %t11, label %zr_aot_fn_8_ins_5_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_5_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 1, i32 1, i32 1)
  br i1 %t12, label %zr_aot_fn_8_ins_6, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_6:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 0)
  br i1 %t13, label %zr_aot_fn_8_ins_6_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_6_body:
  %t14 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 3)
  br i1 %t14, label %zr_aot_fn_8_ins_7, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_7:
  %t15 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 1)
  br i1 %t15, label %zr_aot_fn_8_ins_7_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_7_body:
  %t16 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 3, i32 3, i32 0)
  br i1 %t16, label %zr_aot_fn_8_ins_8, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_8:
  %t17 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 8, i32 0)
  br i1 %t17, label %zr_aot_fn_8_ins_8_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_8_body:
  %t18 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 2, i32 3)
  br i1 %t18, label %zr_aot_fn_8_ins_9, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_9:
  %t19 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 9, i32 1)
  br i1 %t19, label %zr_aot_fn_8_ins_9_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_9_body:
  %t20 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 0)
  br i1 %t20, label %zr_aot_fn_8_ins_10, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_10:
  %t21 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 10, i32 1)
  br i1 %t21, label %zr_aot_fn_8_ins_10_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_10_body:
  %t22 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t22, label %zr_aot_fn_8_ins_11, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_11:
  %t23 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 11, i32 1)
  br i1 %t23, label %zr_aot_fn_8_ins_11_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_11_body:
  %t24 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 2, i32 2, i32 2)
  br i1 %t24, label %zr_aot_fn_8_ins_12, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_12:
  %t25 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 12, i32 1)
  br i1 %t25, label %zr_aot_fn_8_ins_12_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_12_body:
  %t26 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 2, i32 2, i32 3)
  br i1 %t26, label %zr_aot_fn_8_ins_13, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_13:
  %t27 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 13, i32 1)
  br i1 %t27, label %zr_aot_fn_8_ins_13_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_13_body:
  %t28 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t28, label %zr_aot_fn_8_ins_14, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_14:
  %t29 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 14, i32 1)
  br i1 %t29, label %zr_aot_fn_8_ins_14_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_14_body:
  %t30 = call i1 @ZrLibrary_AotRuntime_GetByIndex(ptr %state, ptr %frame, i32 2, i32 2, i32 3)
  br i1 %t30, label %zr_aot_fn_8_ins_15, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_15:
  %t31 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 15, i32 1)
  br i1 %t31, label %zr_aot_fn_8_ins_15_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_15_body:
  %t32 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 0)
  br i1 %t32, label %zr_aot_fn_8_ins_16, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_16:
  %t33 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 16, i32 1)
  br i1 %t33, label %zr_aot_fn_8_ins_16_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_16_body:
  %t34 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 2, i32 2, i32 1)
  br i1 %t34, label %zr_aot_fn_8_ins_17, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_17:
  %t35 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 17, i32 0)
  br i1 %t35, label %zr_aot_fn_8_ins_17_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_17_body:
  %t36 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 4)
  br i1 %t36, label %zr_aot_fn_8_ins_18, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_18:
  %t37 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 18, i32 1)
  br i1 %t37, label %zr_aot_fn_8_ins_18_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_18_body:
  %t38 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 0)
  br i1 %t38, label %zr_aot_fn_8_ins_19, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_19:
  %t39 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 19, i32 0)
  br i1 %t39, label %zr_aot_fn_8_ins_19_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_19_body:
  %t40 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 3, i32 4)
  br i1 %t40, label %zr_aot_fn_8_ins_20, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_20:
  %t41 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 20, i32 1)
  br i1 %t41, label %zr_aot_fn_8_ins_20_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_20_body:
  %t42 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 0)
  br i1 %t42, label %zr_aot_fn_8_ins_21, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_21:
  %t43 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 21, i32 1)
  br i1 %t43, label %zr_aot_fn_8_ins_21_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_21_body:
  %t44 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 3, i32 3)
  br i1 %t44, label %zr_aot_fn_8_ins_22, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_22:
  %t45 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 22, i32 1)
  br i1 %t45, label %zr_aot_fn_8_ins_22_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_22_body:
  %t46 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 3, i32 3, i32 2)
  br i1 %t46, label %zr_aot_fn_8_ins_23, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_23:
  %t47 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 23, i32 1)
  br i1 %t47, label %zr_aot_fn_8_ins_23_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_23_body:
  %t48 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 3, i32 3, i32 4)
  br i1 %t48, label %zr_aot_fn_8_ins_24, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_24:
  %t49 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 24, i32 1)
  br i1 %t49, label %zr_aot_fn_8_ins_24_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_24_body:
  %t50 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t50, label %zr_aot_fn_8_ins_25, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_25:
  %t51 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 25, i32 1)
  br i1 %t51, label %zr_aot_fn_8_ins_25_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_25_body:
  %t52 = call i1 @ZrLibrary_AotRuntime_GetByIndex(ptr %state, ptr %frame, i32 3, i32 3, i32 4)
  br i1 %t52, label %zr_aot_fn_8_ins_26, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_26:
  %t53 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 26, i32 1)
  br i1 %t53, label %zr_aot_fn_8_ins_26_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_26_body:
  %t54 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 0)
  br i1 %t54, label %zr_aot_fn_8_ins_27, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_27:
  %t55 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 27, i32 1)
  br i1 %t55, label %zr_aot_fn_8_ins_27_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_27_body:
  %t56 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 3, i32 3, i32 1)
  br i1 %t56, label %zr_aot_fn_8_ins_28, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_28:
  %t57 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 28, i32 0)
  br i1 %t57, label %zr_aot_fn_8_ins_28_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_28_body:
  %t58 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 5)
  br i1 %t58, label %zr_aot_fn_8_ins_29, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_29:
  %t59 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 29, i32 1)
  br i1 %t59, label %zr_aot_fn_8_ins_29_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_29_body:
  %t60 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 5, i32 5, i32 0)
  br i1 %t60, label %zr_aot_fn_8_ins_30, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_30:
  %t61 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 30, i32 0)
  br i1 %t61, label %zr_aot_fn_8_ins_30_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_30_body:
  %t62 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 4, i32 5)
  br i1 %t62, label %zr_aot_fn_8_ins_31, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_31:
  %t63 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 31, i32 1)
  br i1 %t63, label %zr_aot_fn_8_ins_31_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_31_body:
  %t64 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 5, i32 0)
  br i1 %t64, label %zr_aot_fn_8_ins_32, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_32:
  %t65 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 32, i32 1)
  br i1 %t65, label %zr_aot_fn_8_ins_32_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_32_body:
  %t66 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 4, i32 4)
  br i1 %t66, label %zr_aot_fn_8_ins_33, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_33:
  %t67 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 33, i32 1)
  br i1 %t67, label %zr_aot_fn_8_ins_33_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_33_body:
  %t68 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 2)
  br i1 %t68, label %zr_aot_fn_8_ins_34, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_34:
  %t69 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 34, i32 1)
  br i1 %t69, label %zr_aot_fn_8_ins_34_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_34_body:
  %t70 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 5)
  br i1 %t70, label %zr_aot_fn_8_ins_35, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_35:
  %t71 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 35, i32 1)
  br i1 %t71, label %zr_aot_fn_8_ins_35_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_35_body:
  %t72 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 5, i32 1)
  br i1 %t72, label %zr_aot_fn_8_ins_36, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_36:
  %t73 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 36, i32 1)
  br i1 %t73, label %zr_aot_fn_8_ins_36_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_36_body:
  %t74 = call i1 @ZrLibrary_AotRuntime_GetByIndex(ptr %state, ptr %frame, i32 4, i32 4, i32 5)
  br i1 %t74, label %zr_aot_fn_8_ins_37, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_37:
  %t75 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 37, i32 1)
  br i1 %t75, label %zr_aot_fn_8_ins_37_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_37_body:
  %t76 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 5, i32 0)
  br i1 %t76, label %zr_aot_fn_8_ins_38, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_38:
  %t77 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 38, i32 1)
  br i1 %t77, label %zr_aot_fn_8_ins_38_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_38_body:
  %t78 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 1)
  br i1 %t78, label %zr_aot_fn_8_ins_39, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_39:
  %t79 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 39, i32 1)
  br i1 %t79, label %zr_aot_fn_8_ins_39_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_39_body:
  %t80 = call i1 @ZrLibrary_AotRuntime_GetClosureValue(ptr %state, ptr %frame, i32 6, i32 0)
  br i1 %t80, label %zr_aot_fn_8_ins_40, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_40:
  %t81 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 40, i32 0)
  br i1 %t81, label %zr_aot_fn_8_ins_40_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_40_body:
  %t82 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 5, i32 6)
  br i1 %t82, label %zr_aot_fn_8_ins_41, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_41:
  %t83 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 41, i32 1)
  br i1 %t83, label %zr_aot_fn_8_ins_41_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_41_body:
  %t84 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 6, i32 0)
  br i1 %t84, label %zr_aot_fn_8_ins_42, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_42:
  %t85 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 42, i32 1)
  br i1 %t85, label %zr_aot_fn_8_ins_42_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_42_body:
  %t86 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 5, i32 5)
  br i1 %t86, label %zr_aot_fn_8_ins_43, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_43:
  %t87 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 43, i32 1)
  br i1 %t87, label %zr_aot_fn_8_ins_43_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_43_body:
  %t88 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 5, i32 5, i32 1)
  br i1 %t88, label %zr_aot_fn_8_ins_44, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_44:
  %t89 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 44, i32 0)
  br i1 %t89, label %zr_aot_fn_8_ins_44_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_44_body:
  %t90 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 6, i32 1)
  br i1 %t90, label %zr_aot_fn_8_ins_45, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_45:
  %t91 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 45, i32 1)
  br i1 %t91, label %zr_aot_fn_8_ins_45_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_45_body:
  %t92 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 6, i32 6, i32 6)
  br i1 %t92, label %zr_aot_fn_8_ins_46, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_46:
  %t93 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 46, i32 2)
  br i1 %t93, label %zr_aot_fn_8_ins_46_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_46_body:
  %t94 = call i1 @ZrLibrary_AotRuntime_IsTruthy(ptr %state, ptr %frame, i32 6, ptr %truthy_value)
  br i1 %t94, label %zr_aot_fn_8_ins_46_truthy, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_46_truthy:
  %t95 = load i8, ptr %truthy_value, align 1
  %t96 = icmp eq i8 %t95, 0
  br i1 %t96, label %zr_aot_fn_8_ins_55, label %zr_aot_fn_8_ins_47

zr_aot_fn_8_ins_47:
  %t97 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 47, i32 0)
  br i1 %t97, label %zr_aot_fn_8_ins_47_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_47_body:
  %t98 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 7, i32 0)
  br i1 %t98, label %zr_aot_fn_8_ins_48, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_48:
  %t99 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 48, i32 1)
  br i1 %t99, label %zr_aot_fn_8_ins_48_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_48_body:
  %t100 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 8, i32 2)
  br i1 %t100, label %zr_aot_fn_8_ins_49, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_49:
  %t101 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 49, i32 0)
  br i1 %t101, label %zr_aot_fn_8_ins_49_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_49_body:
  %t102 = call i1 @ZrLibrary_AotRuntime_AddInt(ptr %state, ptr %frame, i32 9, i32 7, i32 8)
  br i1 %t102, label %zr_aot_fn_8_ins_50, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_50:
  %t103 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 50, i32 0)
  br i1 %t103, label %zr_aot_fn_8_ins_50_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_50_body:
  %t104 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 0, i32 9)
  br i1 %t104, label %zr_aot_fn_8_ins_51, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_51:
  %t105 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 51, i32 1)
  br i1 %t105, label %zr_aot_fn_8_ins_51_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_51_body:
  %t106 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 7, i32 0)
  br i1 %t106, label %zr_aot_fn_8_ins_52, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_52:
  %t107 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 52, i32 1)
  br i1 %t107, label %zr_aot_fn_8_ins_52_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_52_body:
  %t108 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 8, i32 0)
  br i1 %t108, label %zr_aot_fn_8_ins_53, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_53:
  %t109 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 53, i32 1)
  br i1 %t109, label %zr_aot_fn_8_ins_53_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_53_body:
  %t110 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 0)
  br i1 %t110, label %zr_aot_fn_8_ins_54, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_54:
  %t111 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 54, i32 2)
  br i1 %t111, label %zr_aot_fn_8_ins_54_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_54_body:
  br label %zr_aot_fn_8_ins_55

zr_aot_fn_8_ins_55:
  %t112 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 55, i32 0)
  br i1 %t112, label %zr_aot_fn_8_ins_55_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_55_body:
  %t113 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 7, i32 2)
  br i1 %t113, label %zr_aot_fn_8_ins_56, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_56:
  %t114 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 56, i32 1)
  br i1 %t114, label %zr_aot_fn_8_ins_56_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_56_body:
  %t115 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 7, i32 7, i32 7)
  br i1 %t115, label %zr_aot_fn_8_ins_57, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_57:
  %t116 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 57, i32 2)
  br i1 %t116, label %zr_aot_fn_8_ins_57_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_57_body:
  %t117 = call i1 @ZrLibrary_AotRuntime_IsTruthy(ptr %state, ptr %frame, i32 7, ptr %truthy_value)
  br i1 %t117, label %zr_aot_fn_8_ins_57_truthy, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_57_truthy:
  %t118 = load i8, ptr %truthy_value, align 1
  %t119 = icmp eq i8 %t118, 0
  br i1 %t119, label %zr_aot_fn_8_ins_66, label %zr_aot_fn_8_ins_58

zr_aot_fn_8_ins_58:
  %t120 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 58, i32 0)
  br i1 %t120, label %zr_aot_fn_8_ins_58_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_58_body:
  %t121 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 8, i32 0)
  br i1 %t121, label %zr_aot_fn_8_ins_59, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_59:
  %t122 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 59, i32 1)
  br i1 %t122, label %zr_aot_fn_8_ins_59_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_59_body:
  %t123 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 3)
  br i1 %t123, label %zr_aot_fn_8_ins_60, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_60:
  %t124 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 60, i32 0)
  br i1 %t124, label %zr_aot_fn_8_ins_60_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_60_body:
  %t125 = call i1 @ZrLibrary_AotRuntime_AddInt(ptr %state, ptr %frame, i32 10, i32 8, i32 9)
  br i1 %t125, label %zr_aot_fn_8_ins_61, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_61:
  %t126 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 61, i32 0)
  br i1 %t126, label %zr_aot_fn_8_ins_61_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_61_body:
  %t127 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 0, i32 10)
  br i1 %t127, label %zr_aot_fn_8_ins_62, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_62:
  %t128 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 62, i32 1)
  br i1 %t128, label %zr_aot_fn_8_ins_62_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_62_body:
  %t129 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 8, i32 0)
  br i1 %t129, label %zr_aot_fn_8_ins_63, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_63:
  %t130 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 63, i32 1)
  br i1 %t130, label %zr_aot_fn_8_ins_63_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_63_body:
  %t131 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 0)
  br i1 %t131, label %zr_aot_fn_8_ins_64, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_64:
  %t132 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 64, i32 1)
  br i1 %t132, label %zr_aot_fn_8_ins_64_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_64_body:
  %t133 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 10, i32 0)
  br i1 %t133, label %zr_aot_fn_8_ins_65, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_65:
  %t134 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 65, i32 2)
  br i1 %t134, label %zr_aot_fn_8_ins_65_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_65_body:
  br label %zr_aot_fn_8_ins_66

zr_aot_fn_8_ins_66:
  %t135 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 66, i32 0)
  br i1 %t135, label %zr_aot_fn_8_ins_66_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_66_body:
  %t136 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 8, i32 3)
  br i1 %t136, label %zr_aot_fn_8_ins_67, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_67:
  %t137 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 67, i32 1)
  br i1 %t137, label %zr_aot_fn_8_ins_67_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_67_body:
  %t138 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 8, i32 8, i32 8)
  br i1 %t138, label %zr_aot_fn_8_ins_68, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_68:
  %t139 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 68, i32 2)
  br i1 %t139, label %zr_aot_fn_8_ins_68_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_68_body:
  %t140 = call i1 @ZrLibrary_AotRuntime_IsTruthy(ptr %state, ptr %frame, i32 8, ptr %truthy_value)
  br i1 %t140, label %zr_aot_fn_8_ins_68_truthy, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_68_truthy:
  %t141 = load i8, ptr %truthy_value, align 1
  %t142 = icmp eq i8 %t141, 0
  br i1 %t142, label %zr_aot_fn_8_ins_77, label %zr_aot_fn_8_ins_69

zr_aot_fn_8_ins_69:
  %t143 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 69, i32 0)
  br i1 %t143, label %zr_aot_fn_8_ins_69_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_69_body:
  %t144 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 9, i32 0)
  br i1 %t144, label %zr_aot_fn_8_ins_70, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_70:
  %t145 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 70, i32 1)
  br i1 %t145, label %zr_aot_fn_8_ins_70_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_70_body:
  %t146 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 10, i32 4)
  br i1 %t146, label %zr_aot_fn_8_ins_71, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_71:
  %t147 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 71, i32 0)
  br i1 %t147, label %zr_aot_fn_8_ins_71_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_71_body:
  %t148 = call i1 @ZrLibrary_AotRuntime_AddInt(ptr %state, ptr %frame, i32 11, i32 9, i32 10)
  br i1 %t148, label %zr_aot_fn_8_ins_72, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_72:
  %t149 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 72, i32 0)
  br i1 %t149, label %zr_aot_fn_8_ins_72_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_72_body:
  %t150 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 0, i32 11)
  br i1 %t150, label %zr_aot_fn_8_ins_73, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_73:
  %t151 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 73, i32 1)
  br i1 %t151, label %zr_aot_fn_8_ins_73_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_73_body:
  %t152 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 0)
  br i1 %t152, label %zr_aot_fn_8_ins_74, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_74:
  %t153 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 74, i32 1)
  br i1 %t153, label %zr_aot_fn_8_ins_74_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_74_body:
  %t154 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 10, i32 0)
  br i1 %t154, label %zr_aot_fn_8_ins_75, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_75:
  %t155 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 75, i32 1)
  br i1 %t155, label %zr_aot_fn_8_ins_75_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_75_body:
  %t156 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 11, i32 0)
  br i1 %t156, label %zr_aot_fn_8_ins_76, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_76:
  %t157 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 76, i32 2)
  br i1 %t157, label %zr_aot_fn_8_ins_76_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_76_body:
  br label %zr_aot_fn_8_ins_77

zr_aot_fn_8_ins_77:
  %t158 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 77, i32 0)
  br i1 %t158, label %zr_aot_fn_8_ins_77_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_77_body:
  %t159 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 9, i32 4)
  br i1 %t159, label %zr_aot_fn_8_ins_78, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_78:
  %t160 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 78, i32 1)
  br i1 %t160, label %zr_aot_fn_8_ins_78_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_78_body:
  %t161 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 9, i32 9, i32 9)
  br i1 %t161, label %zr_aot_fn_8_ins_79, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_79:
  %t162 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 79, i32 2)
  br i1 %t162, label %zr_aot_fn_8_ins_79_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_79_body:
  %t163 = call i1 @ZrLibrary_AotRuntime_IsTruthy(ptr %state, ptr %frame, i32 9, ptr %truthy_value)
  br i1 %t163, label %zr_aot_fn_8_ins_79_truthy, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_79_truthy:
  %t164 = load i8, ptr %truthy_value, align 1
  %t165 = icmp eq i8 %t164, 0
  br i1 %t165, label %zr_aot_fn_8_ins_88, label %zr_aot_fn_8_ins_80

zr_aot_fn_8_ins_80:
  %t166 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 80, i32 0)
  br i1 %t166, label %zr_aot_fn_8_ins_80_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_80_body:
  %t167 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 10, i32 0)
  br i1 %t167, label %zr_aot_fn_8_ins_81, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_81:
  %t168 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 81, i32 1)
  br i1 %t168, label %zr_aot_fn_8_ins_81_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_81_body:
  %t169 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 11, i32 5)
  br i1 %t169, label %zr_aot_fn_8_ins_82, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_82:
  %t170 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 82, i32 0)
  br i1 %t170, label %zr_aot_fn_8_ins_82_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_82_body:
  %t171 = call i1 @ZrLibrary_AotRuntime_AddInt(ptr %state, ptr %frame, i32 12, i32 10, i32 11)
  br i1 %t171, label %zr_aot_fn_8_ins_83, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_83:
  %t172 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 83, i32 0)
  br i1 %t172, label %zr_aot_fn_8_ins_83_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_83_body:
  %t173 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 0, i32 12)
  br i1 %t173, label %zr_aot_fn_8_ins_84, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_84:
  %t174 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 84, i32 1)
  br i1 %t174, label %zr_aot_fn_8_ins_84_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_84_body:
  %t175 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 10, i32 0)
  br i1 %t175, label %zr_aot_fn_8_ins_85, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_85:
  %t176 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 85, i32 1)
  br i1 %t176, label %zr_aot_fn_8_ins_85_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_85_body:
  %t177 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 11, i32 0)
  br i1 %t177, label %zr_aot_fn_8_ins_86, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_86:
  %t178 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 86, i32 1)
  br i1 %t178, label %zr_aot_fn_8_ins_86_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_86_body:
  %t179 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 12, i32 0)
  br i1 %t179, label %zr_aot_fn_8_ins_87, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_87:
  %t180 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 87, i32 2)
  br i1 %t180, label %zr_aot_fn_8_ins_87_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_87_body:
  br label %zr_aot_fn_8_ins_88

zr_aot_fn_8_ins_88:
  %t181 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 88, i32 0)
  br i1 %t181, label %zr_aot_fn_8_ins_88_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_88_body:
  %t182 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 10, i32 5)
  br i1 %t182, label %zr_aot_fn_8_ins_89, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_89:
  %t183 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 89, i32 1)
  br i1 %t183, label %zr_aot_fn_8_ins_89_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_89_body:
  %t184 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 10, i32 10, i32 10)
  br i1 %t184, label %zr_aot_fn_8_ins_90, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_90:
  %t185 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 90, i32 2)
  br i1 %t185, label %zr_aot_fn_8_ins_90_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_90_body:
  %t186 = call i1 @ZrLibrary_AotRuntime_IsTruthy(ptr %state, ptr %frame, i32 10, ptr %truthy_value)
  br i1 %t186, label %zr_aot_fn_8_ins_90_truthy, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_90_truthy:
  %t187 = load i8, ptr %truthy_value, align 1
  %t188 = icmp eq i8 %t187, 0
  br i1 %t188, label %zr_aot_fn_8_ins_99, label %zr_aot_fn_8_ins_91

zr_aot_fn_8_ins_91:
  %t189 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 91, i32 0)
  br i1 %t189, label %zr_aot_fn_8_ins_91_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_91_body:
  %t190 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 11, i32 0)
  br i1 %t190, label %zr_aot_fn_8_ins_92, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_92:
  %t191 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 92, i32 1)
  br i1 %t191, label %zr_aot_fn_8_ins_92_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_92_body:
  %t192 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 12, i32 6)
  br i1 %t192, label %zr_aot_fn_8_ins_93, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_93:
  %t193 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 93, i32 0)
  br i1 %t193, label %zr_aot_fn_8_ins_93_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_93_body:
  %t194 = call i1 @ZrLibrary_AotRuntime_AddInt(ptr %state, ptr %frame, i32 13, i32 11, i32 12)
  br i1 %t194, label %zr_aot_fn_8_ins_94, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_94:
  %t195 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 94, i32 0)
  br i1 %t195, label %zr_aot_fn_8_ins_94_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_94_body:
  %t196 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 0, i32 13)
  br i1 %t196, label %zr_aot_fn_8_ins_95, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_95:
  %t197 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 95, i32 1)
  br i1 %t197, label %zr_aot_fn_8_ins_95_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_95_body:
  %t198 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 11, i32 0)
  br i1 %t198, label %zr_aot_fn_8_ins_96, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_96:
  %t199 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 96, i32 1)
  br i1 %t199, label %zr_aot_fn_8_ins_96_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_96_body:
  %t200 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 12, i32 0)
  br i1 %t200, label %zr_aot_fn_8_ins_97, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_97:
  %t201 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 97, i32 1)
  br i1 %t201, label %zr_aot_fn_8_ins_97_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_97_body:
  %t202 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 13, i32 0)
  br i1 %t202, label %zr_aot_fn_8_ins_98, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_98:
  %t203 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 98, i32 2)
  br i1 %t203, label %zr_aot_fn_8_ins_98_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_98_body:
  br label %zr_aot_fn_8_ins_99

zr_aot_fn_8_ins_99:
  %t204 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 99, i32 0)
  br i1 %t204, label %zr_aot_fn_8_ins_99_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_99_body:
  %t205 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 11, i32 0)
  br i1 %t205, label %zr_aot_fn_8_ins_100, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_100:
  %t206 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 100, i32 8)
  br i1 %t206, label %zr_aot_fn_8_ins_100_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_100_body:
  %t207 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 11, i1 false)
  ret i64 %t207

zr_aot_fn_8_end_unsupported:
  %t208 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 8, i32 101, i32 0)
  ret i64 %t208

zr_aot_fn_8_fail:
  %t209 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t209
}

@zr_aot_function_thunks = private constant [9 x ptr] [ptr @zr_aot_fn_0, ptr @zr_aot_fn_1, ptr @zr_aot_fn_2, ptr @zr_aot_fn_3, ptr @zr_aot_fn_4, ptr @zr_aot_fn_5, ptr @zr_aot_fn_6, ptr @zr_aot_fn_7, ptr @zr_aot_fn_8]

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
  i32 9,
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
