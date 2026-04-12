; ZR AOT LLVM Backend
; SemIR overlay + generated exec thunks.
declare i1 @ZrCore_Reflection_TypeOfValue(ptr, ptr, ptr)
; runtimeContracts: reflection.typeof

; [0] TYPEOF exec=3 type=0 effect=0 dst=2 op0=2 op1=0 deopt=1
; [1] TYPEOF exec=9 type=0 effect=1 dst=3 op0=3 op1=0 deopt=2
; [2] TYPEOF exec=19 type=0 effect=2 dst=4 op0=4 op1=0 deopt=3
; [3] TYPEOF exec=29 type=0 effect=3 dst=5 op0=5 op1=0 deopt=4
; [4] TYPEOF exec=38 type=0 effect=4 dst=6 op0=6 op1=0 deopt=5
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
  %t2 = call i1 @ZrLibrary_AotRuntime_CreateClosure(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t2, label %zr_aot_fn_0_ins_1, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 0)
  br i1 %t3, label %zr_aot_fn_0_ins_1_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_1_body:
  %t4 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t5 = load ptr, ptr %t4, align 8
  %t6 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t7 = load ptr, ptr %t6, align 8
  %t8 = getelementptr i8, ptr %t7, i64 128
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
  %t40 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 4)
  br i1 %t40, label %zr_aot_fn_0_ins_3, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_3:
  %t41 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t41, label %zr_aot_fn_0_ins_3_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_body:
  %t42 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 0)
  br i1 %t42, label %zr_aot_fn_0_ins_4, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_4:
  %t43 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 0)
  br i1 %t43, label %zr_aot_fn_0_ins_4_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_4_body:
  %t44 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t45 = load ptr, ptr %t44, align 8
  %t46 = getelementptr i8, ptr %t45, i64 192
  %t47 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t48 = load ptr, ptr %t47, align 8
  %t49 = getelementptr i8, ptr %t48, i64 256
  %t50 = getelementptr i8, ptr %t49, i64 20
  %t51 = load i32, ptr %t50, align 4
  %t52 = getelementptr i8, ptr %t46, i64 20
  %t53 = load i32, ptr %t52, align 4
  %t60 = load i32, ptr %t49, align 4
  %t61 = getelementptr i8, ptr %t49, i64 16
  %t62 = load i8, ptr %t61, align 1
  %t54 = icmp eq i32 %t51, 2
  %t55 = icmp eq i32 %t51, 1
  %t56 = icmp eq i32 %t51, 5
  %t57 = or i1 %t55, %t56
  %t58 = or i1 %t57, %t54
  br i1 %t58, label %zr_aot_stack_copy_transfer_71, label %zr_aot_stack_copy_weak_check_71
zr_aot_stack_copy_transfer_71:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t46)
  %t72 = load %SZrTypeValue, ptr %t49, align 32
  store %SZrTypeValue %t72, ptr %t46, align 32
  %t73 = getelementptr i8, ptr %t49, i64 8
  %t74 = getelementptr i8, ptr %t49, i64 16
  %t75 = getelementptr i8, ptr %t49, i64 17
  %t76 = getelementptr i8, ptr %t49, i64 20
  %t77 = getelementptr i8, ptr %t49, i64 24
  %t78 = getelementptr i8, ptr %t49, i64 32
  store i32 0, ptr %t49, align 4
  store i64 0, ptr %t73, align 8
  store i8 0, ptr %t74, align 1
  store i8 1, ptr %t75, align 1
  store i32 0, ptr %t76, align 4
  store ptr null, ptr %t77, align 8
  store ptr null, ptr %t78, align 8
  br label %zr_aot_fn_0_ins_5
zr_aot_stack_copy_weak_check_71:
  %t59 = icmp eq i32 %t51, 3
  br i1 %t59, label %zr_aot_stack_copy_weak_71, label %zr_aot_stack_copy_fast_check_71
zr_aot_stack_copy_weak_71:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t46, ptr %t49)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t49)
  br label %zr_aot_fn_0_ins_5
zr_aot_stack_copy_fast_check_71:
  %t63 = icmp ne i8 %t62, 0
  %t64 = icmp eq i32 %t60, 18
  %t65 = and i1 %t63, %t64
  %t66 = icmp eq i32 %t51, 0
  %t67 = icmp eq i32 %t53, 0
  %t68 = and i1 %t66, %t67
  %t69 = xor i1 %t65, true
  %t70 = and i1 %t68, %t69
  br i1 %t70, label %zr_aot_stack_copy_fast_71, label %zr_aot_stack_copy_slow_71
zr_aot_stack_copy_fast_71:
  %t79 = load %SZrTypeValue, ptr %t49, align 32
  store %SZrTypeValue %t79, ptr %t46, align 32
  br label %zr_aot_fn_0_ins_5
zr_aot_stack_copy_slow_71:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t46, ptr %t49)
  br label %zr_aot_fn_0_ins_5

zr_aot_fn_0_ins_5:
  %t80 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 0)
  br i1 %t80, label %zr_aot_fn_0_ins_5_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_5_body:
  %t81 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 4)
  br i1 %t81, label %zr_aot_fn_0_ins_6, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_6:
  %t82 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 1)
  br i1 %t82, label %zr_aot_fn_0_ins_6_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_6_body:
  %t83 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 1)
  br i1 %t83, label %zr_aot_fn_0_ins_7, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_7:
  %t84 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 1)
  br i1 %t84, label %zr_aot_fn_0_ins_7_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_7_body:
  %t85 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t86 = load ptr, ptr %t85, align 8
  %t87 = getelementptr i8, ptr %t86, i64 320
  %t88 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 5, i32 8)
  br i1 %t88, label %zr_aot_fn_0_ins_8, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_8:
  %t89 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 8, i32 0)
  br i1 %t89, label %zr_aot_fn_0_ins_8_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_8_body:
  %t90 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t91 = load ptr, ptr %t90, align 8
  %t92 = getelementptr i8, ptr %t91, i64 384
  %t93 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t94 = load ptr, ptr %t93, align 8
  %t95 = getelementptr i8, ptr %t94, i64 256
  %t96 = getelementptr i8, ptr %t95, i64 20
  %t97 = load i32, ptr %t96, align 4
  %t98 = getelementptr i8, ptr %t92, i64 20
  %t99 = load i32, ptr %t98, align 4
  %t106 = load i32, ptr %t95, align 4
  %t107 = getelementptr i8, ptr %t95, i64 16
  %t108 = load i8, ptr %t107, align 1
  %t100 = icmp eq i32 %t97, 2
  %t101 = icmp eq i32 %t97, 1
  %t102 = icmp eq i32 %t97, 5
  %t103 = or i1 %t101, %t102
  %t104 = or i1 %t103, %t100
  br i1 %t104, label %zr_aot_stack_copy_transfer_117, label %zr_aot_stack_copy_weak_check_117
zr_aot_stack_copy_transfer_117:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t92)
  %t118 = load %SZrTypeValue, ptr %t95, align 32
  store %SZrTypeValue %t118, ptr %t92, align 32
  %t119 = getelementptr i8, ptr %t95, i64 8
  %t120 = getelementptr i8, ptr %t95, i64 16
  %t121 = getelementptr i8, ptr %t95, i64 17
  %t122 = getelementptr i8, ptr %t95, i64 20
  %t123 = getelementptr i8, ptr %t95, i64 24
  %t124 = getelementptr i8, ptr %t95, i64 32
  store i32 0, ptr %t95, align 4
  store i64 0, ptr %t119, align 8
  store i8 0, ptr %t120, align 1
  store i8 1, ptr %t121, align 1
  store i32 0, ptr %t122, align 4
  store ptr null, ptr %t123, align 8
  store ptr null, ptr %t124, align 8
  br label %zr_aot_fn_0_ins_9
zr_aot_stack_copy_weak_check_117:
  %t105 = icmp eq i32 %t97, 3
  br i1 %t105, label %zr_aot_stack_copy_weak_117, label %zr_aot_stack_copy_fast_check_117
zr_aot_stack_copy_weak_117:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t92, ptr %t95)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t95)
  br label %zr_aot_fn_0_ins_9
zr_aot_stack_copy_fast_check_117:
  %t109 = icmp ne i8 %t108, 0
  %t110 = icmp eq i32 %t106, 18
  %t111 = and i1 %t109, %t110
  %t112 = icmp eq i32 %t97, 0
  %t113 = icmp eq i32 %t99, 0
  %t114 = and i1 %t112, %t113
  %t115 = xor i1 %t111, true
  %t116 = and i1 %t114, %t115
  br i1 %t116, label %zr_aot_stack_copy_fast_117, label %zr_aot_stack_copy_slow_117
zr_aot_stack_copy_fast_117:
  %t125 = load %SZrTypeValue, ptr %t95, align 32
  store %SZrTypeValue %t125, ptr %t92, align 32
  br label %zr_aot_fn_0_ins_9
zr_aot_stack_copy_slow_117:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t92, ptr %t95)
  br label %zr_aot_fn_0_ins_9

zr_aot_fn_0_ins_9:
  %t126 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 9, i32 1)
  br i1 %t126, label %zr_aot_fn_0_ins_9_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_9_body:
  %t127 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t128 = load ptr, ptr %t127, align 8
  %t129 = getelementptr i8, ptr %t128, i64 448
  %t130 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 7, i32 9)
  br i1 %t130, label %zr_aot_fn_0_ins_10, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_10:
  %t131 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 10, i32 1)
  br i1 %t131, label %zr_aot_fn_0_ins_10_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_10_body:
  %t132 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t133 = load ptr, ptr %t132, align 8
  %t134 = getelementptr i8, ptr %t133, i64 512
  %t135 = getelementptr i8, ptr %t134, i64 8
  %t136 = getelementptr i8, ptr %t134, i64 16
  %t137 = getelementptr i8, ptr %t134, i64 17
  %t138 = getelementptr i8, ptr %t134, i64 20
  %t139 = getelementptr i8, ptr %t134, i64 24
  %t140 = getelementptr i8, ptr %t134, i64 32
  store i32 5, ptr %t134, align 4
  store i64 1, ptr %t135, align 8
  store i8 0, ptr %t136, align 1
  store i8 1, ptr %t137, align 1
  store i32 0, ptr %t138, align 4
  store ptr null, ptr %t139, align 8
  store ptr null, ptr %t140, align 8
  br label %zr_aot_fn_0_ins_11

zr_aot_fn_0_ins_11:
  %t141 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 11, i32 0)
  br i1 %t141, label %zr_aot_fn_0_ins_11_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_11_body:
  %t142 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t143 = load ptr, ptr %t142, align 8
  %t144 = getelementptr i8, ptr %t143, i64 576
  %t145 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t146 = load ptr, ptr %t145, align 8
  %t147 = getelementptr i8, ptr %t146, i64 192
  %t148 = getelementptr i8, ptr %t147, i64 20
  %t149 = load i32, ptr %t148, align 4
  %t150 = getelementptr i8, ptr %t144, i64 20
  %t151 = load i32, ptr %t150, align 4
  %t158 = load i32, ptr %t147, align 4
  %t159 = getelementptr i8, ptr %t147, i64 16
  %t160 = load i8, ptr %t159, align 1
  %t152 = icmp eq i32 %t149, 2
  %t153 = icmp eq i32 %t149, 1
  %t154 = icmp eq i32 %t149, 5
  %t155 = or i1 %t153, %t154
  %t156 = or i1 %t155, %t152
  br i1 %t156, label %zr_aot_stack_copy_transfer_169, label %zr_aot_stack_copy_weak_check_169
zr_aot_stack_copy_transfer_169:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t144)
  %t170 = load %SZrTypeValue, ptr %t147, align 32
  store %SZrTypeValue %t170, ptr %t144, align 32
  %t171 = getelementptr i8, ptr %t147, i64 8
  %t172 = getelementptr i8, ptr %t147, i64 16
  %t173 = getelementptr i8, ptr %t147, i64 17
  %t174 = getelementptr i8, ptr %t147, i64 20
  %t175 = getelementptr i8, ptr %t147, i64 24
  %t176 = getelementptr i8, ptr %t147, i64 32
  store i32 0, ptr %t147, align 4
  store i64 0, ptr %t171, align 8
  store i8 0, ptr %t172, align 1
  store i8 1, ptr %t173, align 1
  store i32 0, ptr %t174, align 4
  store ptr null, ptr %t175, align 8
  store ptr null, ptr %t176, align 8
  br label %zr_aot_fn_0_ins_12
zr_aot_stack_copy_weak_check_169:
  %t157 = icmp eq i32 %t149, 3
  br i1 %t157, label %zr_aot_stack_copy_weak_169, label %zr_aot_stack_copy_fast_check_169
zr_aot_stack_copy_weak_169:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t144, ptr %t147)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t147)
  br label %zr_aot_fn_0_ins_12
zr_aot_stack_copy_fast_check_169:
  %t161 = icmp ne i8 %t160, 0
  %t162 = icmp eq i32 %t158, 18
  %t163 = and i1 %t161, %t162
  %t164 = icmp eq i32 %t149, 0
  %t165 = icmp eq i32 %t151, 0
  %t166 = and i1 %t164, %t165
  %t167 = xor i1 %t163, true
  %t168 = and i1 %t166, %t167
  br i1 %t168, label %zr_aot_stack_copy_fast_169, label %zr_aot_stack_copy_slow_169
zr_aot_stack_copy_fast_169:
  %t177 = load %SZrTypeValue, ptr %t147, align 32
  store %SZrTypeValue %t177, ptr %t144, align 32
  br label %zr_aot_fn_0_ins_12
zr_aot_stack_copy_slow_169:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t144, ptr %t147)
  br label %zr_aot_fn_0_ins_12

zr_aot_fn_0_ins_12:
  %t178 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 12, i32 5)
  br i1 %t178, label %zr_aot_fn_0_ins_12_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_12_body:
  %t179 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 5, i32 5, i32 4, ptr %direct_call)
  br i1 %t179, label %zr_aot_fn_0_ins_12_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_12_prepare_ok:
  %t180 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 5, i32 5, i32 4, i32 1)
  br i1 %t180, label %zr_aot_fn_0_ins_12_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_12_finish_ok:
  br label %zr_aot_fn_0_ins_13

zr_aot_fn_0_ins_13:
  %t181 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 13, i32 0)
  br i1 %t181, label %zr_aot_fn_0_ins_13_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_13_body:
  %t182 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 6)
  br i1 %t182, label %zr_aot_fn_0_ins_14, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_14:
  %t183 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 14, i32 1)
  br i1 %t183, label %zr_aot_fn_0_ins_14_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_14_body:
  %t184 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 6, i32 6, i32 2)
  br i1 %t184, label %zr_aot_fn_0_ins_15, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_15:
  %t185 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 15, i32 0)
  br i1 %t185, label %zr_aot_fn_0_ins_15_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_15_body:
  %t186 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t187 = load ptr, ptr %t186, align 8
  %t188 = getelementptr i8, ptr %t187, i64 320
  %t189 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t190 = load ptr, ptr %t189, align 8
  %t191 = getelementptr i8, ptr %t190, i64 384
  %t192 = getelementptr i8, ptr %t191, i64 20
  %t193 = load i32, ptr %t192, align 4
  %t194 = getelementptr i8, ptr %t188, i64 20
  %t195 = load i32, ptr %t194, align 4
  %t202 = load i32, ptr %t191, align 4
  %t203 = getelementptr i8, ptr %t191, i64 16
  %t204 = load i8, ptr %t203, align 1
  %t196 = icmp eq i32 %t193, 2
  %t197 = icmp eq i32 %t193, 1
  %t198 = icmp eq i32 %t193, 5
  %t199 = or i1 %t197, %t198
  %t200 = or i1 %t199, %t196
  br i1 %t200, label %zr_aot_stack_copy_transfer_213, label %zr_aot_stack_copy_weak_check_213
zr_aot_stack_copy_transfer_213:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t188)
  %t214 = load %SZrTypeValue, ptr %t191, align 32
  store %SZrTypeValue %t214, ptr %t188, align 32
  %t215 = getelementptr i8, ptr %t191, i64 8
  %t216 = getelementptr i8, ptr %t191, i64 16
  %t217 = getelementptr i8, ptr %t191, i64 17
  %t218 = getelementptr i8, ptr %t191, i64 20
  %t219 = getelementptr i8, ptr %t191, i64 24
  %t220 = getelementptr i8, ptr %t191, i64 32
  store i32 0, ptr %t191, align 4
  store i64 0, ptr %t215, align 8
  store i8 0, ptr %t216, align 1
  store i8 1, ptr %t217, align 1
  store i32 0, ptr %t218, align 4
  store ptr null, ptr %t219, align 8
  store ptr null, ptr %t220, align 8
  br label %zr_aot_fn_0_ins_16
zr_aot_stack_copy_weak_check_213:
  %t201 = icmp eq i32 %t193, 3
  br i1 %t201, label %zr_aot_stack_copy_weak_213, label %zr_aot_stack_copy_fast_check_213
zr_aot_stack_copy_weak_213:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t188, ptr %t191)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t191)
  br label %zr_aot_fn_0_ins_16
zr_aot_stack_copy_fast_check_213:
  %t205 = icmp ne i8 %t204, 0
  %t206 = icmp eq i32 %t202, 18
  %t207 = and i1 %t205, %t206
  %t208 = icmp eq i32 %t193, 0
  %t209 = icmp eq i32 %t195, 0
  %t210 = and i1 %t208, %t209
  %t211 = xor i1 %t207, true
  %t212 = and i1 %t210, %t211
  br i1 %t212, label %zr_aot_stack_copy_fast_213, label %zr_aot_stack_copy_slow_213
zr_aot_stack_copy_fast_213:
  %t221 = load %SZrTypeValue, ptr %t191, align 32
  store %SZrTypeValue %t221, ptr %t188, align 32
  br label %zr_aot_fn_0_ins_16
zr_aot_stack_copy_slow_213:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t188, ptr %t191)
  br label %zr_aot_fn_0_ins_16

zr_aot_fn_0_ins_16:
  %t222 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 16, i32 0)
  br i1 %t222, label %zr_aot_fn_0_ins_16_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_16_body:
  %t223 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 6)
  br i1 %t223, label %zr_aot_fn_0_ins_17, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_17:
  %t224 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 17, i32 1)
  br i1 %t224, label %zr_aot_fn_0_ins_17_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_17_body:
  %t225 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 6, i32 6, i32 1)
  br i1 %t225, label %zr_aot_fn_0_ins_18, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_18:
  %t226 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 18, i32 1)
  br i1 %t226, label %zr_aot_fn_0_ins_18_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_18_body:
  %t227 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t228 = load ptr, ptr %t227, align 8
  %t229 = getelementptr i8, ptr %t228, i64 448
  %t230 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 7, i32 11)
  br i1 %t230, label %zr_aot_fn_0_ins_19, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_19:
  %t231 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 19, i32 0)
  br i1 %t231, label %zr_aot_fn_0_ins_19_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_19_body:
  %t232 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t233 = load ptr, ptr %t232, align 8
  %t234 = getelementptr i8, ptr %t233, i64 512
  %t235 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t236 = load ptr, ptr %t235, align 8
  %t237 = getelementptr i8, ptr %t236, i64 384
  %t238 = getelementptr i8, ptr %t237, i64 20
  %t239 = load i32, ptr %t238, align 4
  %t240 = getelementptr i8, ptr %t234, i64 20
  %t241 = load i32, ptr %t240, align 4
  %t248 = load i32, ptr %t237, align 4
  %t249 = getelementptr i8, ptr %t237, i64 16
  %t250 = load i8, ptr %t249, align 1
  %t242 = icmp eq i32 %t239, 2
  %t243 = icmp eq i32 %t239, 1
  %t244 = icmp eq i32 %t239, 5
  %t245 = or i1 %t243, %t244
  %t246 = or i1 %t245, %t242
  br i1 %t246, label %zr_aot_stack_copy_transfer_259, label %zr_aot_stack_copy_weak_check_259
zr_aot_stack_copy_transfer_259:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t234)
  %t260 = load %SZrTypeValue, ptr %t237, align 32
  store %SZrTypeValue %t260, ptr %t234, align 32
  %t261 = getelementptr i8, ptr %t237, i64 8
  %t262 = getelementptr i8, ptr %t237, i64 16
  %t263 = getelementptr i8, ptr %t237, i64 17
  %t264 = getelementptr i8, ptr %t237, i64 20
  %t265 = getelementptr i8, ptr %t237, i64 24
  %t266 = getelementptr i8, ptr %t237, i64 32
  store i32 0, ptr %t237, align 4
  store i64 0, ptr %t261, align 8
  store i8 0, ptr %t262, align 1
  store i8 1, ptr %t263, align 1
  store i32 0, ptr %t264, align 4
  store ptr null, ptr %t265, align 8
  store ptr null, ptr %t266, align 8
  br label %zr_aot_fn_0_ins_20
zr_aot_stack_copy_weak_check_259:
  %t247 = icmp eq i32 %t239, 3
  br i1 %t247, label %zr_aot_stack_copy_weak_259, label %zr_aot_stack_copy_fast_check_259
zr_aot_stack_copy_weak_259:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t234, ptr %t237)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t237)
  br label %zr_aot_fn_0_ins_20
zr_aot_stack_copy_fast_check_259:
  %t251 = icmp ne i8 %t250, 0
  %t252 = icmp eq i32 %t248, 18
  %t253 = and i1 %t251, %t252
  %t254 = icmp eq i32 %t239, 0
  %t255 = icmp eq i32 %t241, 0
  %t256 = and i1 %t254, %t255
  %t257 = xor i1 %t253, true
  %t258 = and i1 %t256, %t257
  br i1 %t258, label %zr_aot_stack_copy_fast_259, label %zr_aot_stack_copy_slow_259
zr_aot_stack_copy_fast_259:
  %t267 = load %SZrTypeValue, ptr %t237, align 32
  store %SZrTypeValue %t267, ptr %t234, align 32
  br label %zr_aot_fn_0_ins_20
zr_aot_stack_copy_slow_259:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t234, ptr %t237)
  br label %zr_aot_fn_0_ins_20

zr_aot_fn_0_ins_20:
  %t268 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 20, i32 1)
  br i1 %t268, label %zr_aot_fn_0_ins_20_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_20_body:
  %t269 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t270 = load ptr, ptr %t269, align 8
  %t271 = getelementptr i8, ptr %t270, i64 576
  %t272 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 12)
  br i1 %t272, label %zr_aot_fn_0_ins_21, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_21:
  %t273 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 21, i32 1)
  br i1 %t273, label %zr_aot_fn_0_ins_21_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_21_body:
  %t274 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t275 = load ptr, ptr %t274, align 8
  %t276 = getelementptr i8, ptr %t275, i64 640
  %t277 = getelementptr i8, ptr %t276, i64 8
  %t278 = getelementptr i8, ptr %t276, i64 16
  %t279 = getelementptr i8, ptr %t276, i64 17
  %t280 = getelementptr i8, ptr %t276, i64 20
  %t281 = getelementptr i8, ptr %t276, i64 24
  %t282 = getelementptr i8, ptr %t276, i64 32
  store i32 5, ptr %t276, align 4
  store i64 2, ptr %t277, align 8
  store i8 0, ptr %t278, align 1
  store i8 1, ptr %t279, align 1
  store i32 0, ptr %t280, align 4
  store ptr null, ptr %t281, align 8
  store ptr null, ptr %t282, align 8
  br label %zr_aot_fn_0_ins_22

zr_aot_fn_0_ins_22:
  %t283 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 22, i32 0)
  br i1 %t283, label %zr_aot_fn_0_ins_22_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_22_body:
  %t284 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t285 = load ptr, ptr %t284, align 8
  %t286 = getelementptr i8, ptr %t285, i64 704
  %t287 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t288 = load ptr, ptr %t287, align 8
  %t289 = getelementptr i8, ptr %t288, i64 320
  %t290 = getelementptr i8, ptr %t289, i64 20
  %t291 = load i32, ptr %t290, align 4
  %t292 = getelementptr i8, ptr %t286, i64 20
  %t293 = load i32, ptr %t292, align 4
  %t300 = load i32, ptr %t289, align 4
  %t301 = getelementptr i8, ptr %t289, i64 16
  %t302 = load i8, ptr %t301, align 1
  %t294 = icmp eq i32 %t291, 2
  %t295 = icmp eq i32 %t291, 1
  %t296 = icmp eq i32 %t291, 5
  %t297 = or i1 %t295, %t296
  %t298 = or i1 %t297, %t294
  br i1 %t298, label %zr_aot_stack_copy_transfer_311, label %zr_aot_stack_copy_weak_check_311
zr_aot_stack_copy_transfer_311:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t286)
  %t312 = load %SZrTypeValue, ptr %t289, align 32
  store %SZrTypeValue %t312, ptr %t286, align 32
  %t313 = getelementptr i8, ptr %t289, i64 8
  %t314 = getelementptr i8, ptr %t289, i64 16
  %t315 = getelementptr i8, ptr %t289, i64 17
  %t316 = getelementptr i8, ptr %t289, i64 20
  %t317 = getelementptr i8, ptr %t289, i64 24
  %t318 = getelementptr i8, ptr %t289, i64 32
  store i32 0, ptr %t289, align 4
  store i64 0, ptr %t313, align 8
  store i8 0, ptr %t314, align 1
  store i8 1, ptr %t315, align 1
  store i32 0, ptr %t316, align 4
  store ptr null, ptr %t317, align 8
  store ptr null, ptr %t318, align 8
  br label %zr_aot_fn_0_ins_23
zr_aot_stack_copy_weak_check_311:
  %t299 = icmp eq i32 %t291, 3
  br i1 %t299, label %zr_aot_stack_copy_weak_311, label %zr_aot_stack_copy_fast_check_311
zr_aot_stack_copy_weak_311:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t286, ptr %t289)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t289)
  br label %zr_aot_fn_0_ins_23
zr_aot_stack_copy_fast_check_311:
  %t303 = icmp ne i8 %t302, 0
  %t304 = icmp eq i32 %t300, 18
  %t305 = and i1 %t303, %t304
  %t306 = icmp eq i32 %t291, 0
  %t307 = icmp eq i32 %t293, 0
  %t308 = and i1 %t306, %t307
  %t309 = xor i1 %t305, true
  %t310 = and i1 %t308, %t309
  br i1 %t310, label %zr_aot_stack_copy_fast_311, label %zr_aot_stack_copy_slow_311
zr_aot_stack_copy_fast_311:
  %t319 = load %SZrTypeValue, ptr %t289, align 32
  store %SZrTypeValue %t319, ptr %t286, align 32
  br label %zr_aot_fn_0_ins_23
zr_aot_stack_copy_slow_311:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t286, ptr %t289)
  br label %zr_aot_fn_0_ins_23

zr_aot_fn_0_ins_23:
  %t320 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 23, i32 5)
  br i1 %t320, label %zr_aot_fn_0_ins_23_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_23_body:
  %t321 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 7, i32 7, i32 4, ptr %direct_call)
  br i1 %t321, label %zr_aot_fn_0_ins_23_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_23_prepare_ok:
  %t322 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 7, i32 7, i32 4, i32 1)
  br i1 %t322, label %zr_aot_fn_0_ins_23_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_23_finish_ok:
  br label %zr_aot_fn_0_ins_24

zr_aot_fn_0_ins_24:
  %t323 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 24, i32 0)
  br i1 %t323, label %zr_aot_fn_0_ins_24_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_24_body:
  %t324 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 8)
  br i1 %t324, label %zr_aot_fn_0_ins_25, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_25:
  %t325 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 25, i32 1)
  br i1 %t325, label %zr_aot_fn_0_ins_25_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_25_body:
  %t326 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 8, i32 8, i32 3)
  br i1 %t326, label %zr_aot_fn_0_ins_26, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_26:
  %t327 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 26, i32 0)
  br i1 %t327, label %zr_aot_fn_0_ins_26_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_26_body:
  %t328 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t329 = load ptr, ptr %t328, align 8
  %t330 = getelementptr i8, ptr %t329, i64 448
  %t331 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t332 = load ptr, ptr %t331, align 8
  %t333 = getelementptr i8, ptr %t332, i64 512
  %t334 = getelementptr i8, ptr %t333, i64 20
  %t335 = load i32, ptr %t334, align 4
  %t336 = getelementptr i8, ptr %t330, i64 20
  %t337 = load i32, ptr %t336, align 4
  %t344 = load i32, ptr %t333, align 4
  %t345 = getelementptr i8, ptr %t333, i64 16
  %t346 = load i8, ptr %t345, align 1
  %t338 = icmp eq i32 %t335, 2
  %t339 = icmp eq i32 %t335, 1
  %t340 = icmp eq i32 %t335, 5
  %t341 = or i1 %t339, %t340
  %t342 = or i1 %t341, %t338
  br i1 %t342, label %zr_aot_stack_copy_transfer_355, label %zr_aot_stack_copy_weak_check_355
zr_aot_stack_copy_transfer_355:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t330)
  %t356 = load %SZrTypeValue, ptr %t333, align 32
  store %SZrTypeValue %t356, ptr %t330, align 32
  %t357 = getelementptr i8, ptr %t333, i64 8
  %t358 = getelementptr i8, ptr %t333, i64 16
  %t359 = getelementptr i8, ptr %t333, i64 17
  %t360 = getelementptr i8, ptr %t333, i64 20
  %t361 = getelementptr i8, ptr %t333, i64 24
  %t362 = getelementptr i8, ptr %t333, i64 32
  store i32 0, ptr %t333, align 4
  store i64 0, ptr %t357, align 8
  store i8 0, ptr %t358, align 1
  store i8 1, ptr %t359, align 1
  store i32 0, ptr %t360, align 4
  store ptr null, ptr %t361, align 8
  store ptr null, ptr %t362, align 8
  br label %zr_aot_fn_0_ins_27
zr_aot_stack_copy_weak_check_355:
  %t343 = icmp eq i32 %t335, 3
  br i1 %t343, label %zr_aot_stack_copy_weak_355, label %zr_aot_stack_copy_fast_check_355
zr_aot_stack_copy_weak_355:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t330, ptr %t333)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t333)
  br label %zr_aot_fn_0_ins_27
zr_aot_stack_copy_fast_check_355:
  %t347 = icmp ne i8 %t346, 0
  %t348 = icmp eq i32 %t344, 18
  %t349 = and i1 %t347, %t348
  %t350 = icmp eq i32 %t335, 0
  %t351 = icmp eq i32 %t337, 0
  %t352 = and i1 %t350, %t351
  %t353 = xor i1 %t349, true
  %t354 = and i1 %t352, %t353
  br i1 %t354, label %zr_aot_stack_copy_fast_355, label %zr_aot_stack_copy_slow_355
zr_aot_stack_copy_fast_355:
  %t363 = load %SZrTypeValue, ptr %t333, align 32
  store %SZrTypeValue %t363, ptr %t330, align 32
  br label %zr_aot_fn_0_ins_27
zr_aot_stack_copy_slow_355:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t330, ptr %t333)
  br label %zr_aot_fn_0_ins_27

zr_aot_fn_0_ins_27:
  %t364 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 27, i32 0)
  br i1 %t364, label %zr_aot_fn_0_ins_27_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_27_body:
  %t365 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 8)
  br i1 %t365, label %zr_aot_fn_0_ins_28, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_28:
  %t366 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 28, i32 1)
  br i1 %t366, label %zr_aot_fn_0_ins_28_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_28_body:
  %t367 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 8, i32 8, i32 1)
  br i1 %t367, label %zr_aot_fn_0_ins_29, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_29:
  %t368 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 29, i32 1)
  br i1 %t368, label %zr_aot_fn_0_ins_29_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_29_body:
  %t369 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t370 = load ptr, ptr %t369, align 8
  %t371 = getelementptr i8, ptr %t370, i64 576
  %t372 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 14)
  br i1 %t372, label %zr_aot_fn_0_ins_30, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_30:
  %t373 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 30, i32 0)
  br i1 %t373, label %zr_aot_fn_0_ins_30_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_30_body:
  %t374 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t375 = load ptr, ptr %t374, align 8
  %t376 = getelementptr i8, ptr %t375, i64 640
  %t377 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t378 = load ptr, ptr %t377, align 8
  %t379 = getelementptr i8, ptr %t378, i64 512
  %t380 = getelementptr i8, ptr %t379, i64 20
  %t381 = load i32, ptr %t380, align 4
  %t382 = getelementptr i8, ptr %t376, i64 20
  %t383 = load i32, ptr %t382, align 4
  %t390 = load i32, ptr %t379, align 4
  %t391 = getelementptr i8, ptr %t379, i64 16
  %t392 = load i8, ptr %t391, align 1
  %t384 = icmp eq i32 %t381, 2
  %t385 = icmp eq i32 %t381, 1
  %t386 = icmp eq i32 %t381, 5
  %t387 = or i1 %t385, %t386
  %t388 = or i1 %t387, %t384
  br i1 %t388, label %zr_aot_stack_copy_transfer_401, label %zr_aot_stack_copy_weak_check_401
zr_aot_stack_copy_transfer_401:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t376)
  %t402 = load %SZrTypeValue, ptr %t379, align 32
  store %SZrTypeValue %t402, ptr %t376, align 32
  %t403 = getelementptr i8, ptr %t379, i64 8
  %t404 = getelementptr i8, ptr %t379, i64 16
  %t405 = getelementptr i8, ptr %t379, i64 17
  %t406 = getelementptr i8, ptr %t379, i64 20
  %t407 = getelementptr i8, ptr %t379, i64 24
  %t408 = getelementptr i8, ptr %t379, i64 32
  store i32 0, ptr %t379, align 4
  store i64 0, ptr %t403, align 8
  store i8 0, ptr %t404, align 1
  store i8 1, ptr %t405, align 1
  store i32 0, ptr %t406, align 4
  store ptr null, ptr %t407, align 8
  store ptr null, ptr %t408, align 8
  br label %zr_aot_fn_0_ins_31
zr_aot_stack_copy_weak_check_401:
  %t389 = icmp eq i32 %t381, 3
  br i1 %t389, label %zr_aot_stack_copy_weak_401, label %zr_aot_stack_copy_fast_check_401
zr_aot_stack_copy_weak_401:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t376, ptr %t379)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t379)
  br label %zr_aot_fn_0_ins_31
zr_aot_stack_copy_fast_check_401:
  %t393 = icmp ne i8 %t392, 0
  %t394 = icmp eq i32 %t390, 18
  %t395 = and i1 %t393, %t394
  %t396 = icmp eq i32 %t381, 0
  %t397 = icmp eq i32 %t383, 0
  %t398 = and i1 %t396, %t397
  %t399 = xor i1 %t395, true
  %t400 = and i1 %t398, %t399
  br i1 %t400, label %zr_aot_stack_copy_fast_401, label %zr_aot_stack_copy_slow_401
zr_aot_stack_copy_fast_401:
  %t409 = load %SZrTypeValue, ptr %t379, align 32
  store %SZrTypeValue %t409, ptr %t376, align 32
  br label %zr_aot_fn_0_ins_31
zr_aot_stack_copy_slow_401:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t376, ptr %t379)
  br label %zr_aot_fn_0_ins_31

zr_aot_fn_0_ins_31:
  %t410 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 31, i32 1)
  br i1 %t410, label %zr_aot_fn_0_ins_31_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_31_body:
  %t411 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t412 = load ptr, ptr %t411, align 8
  %t413 = getelementptr i8, ptr %t412, i64 704
  %t414 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 11, i32 15)
  br i1 %t414, label %zr_aot_fn_0_ins_32, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_32:
  %t415 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 32, i32 1)
  br i1 %t415, label %zr_aot_fn_0_ins_32_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_32_body:
  %t416 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t417 = load ptr, ptr %t416, align 8
  %t418 = getelementptr i8, ptr %t417, i64 768
  %t419 = getelementptr i8, ptr %t418, i64 8
  %t420 = getelementptr i8, ptr %t418, i64 16
  %t421 = getelementptr i8, ptr %t418, i64 17
  %t422 = getelementptr i8, ptr %t418, i64 20
  %t423 = getelementptr i8, ptr %t418, i64 24
  %t424 = getelementptr i8, ptr %t418, i64 32
  store i32 5, ptr %t418, align 4
  store i64 3, ptr %t419, align 8
  store i8 0, ptr %t420, align 1
  store i8 1, ptr %t421, align 1
  store i32 0, ptr %t422, align 4
  store ptr null, ptr %t423, align 8
  store ptr null, ptr %t424, align 8
  br label %zr_aot_fn_0_ins_33

zr_aot_fn_0_ins_33:
  %t425 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 33, i32 0)
  br i1 %t425, label %zr_aot_fn_0_ins_33_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_33_body:
  %t426 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t427 = load ptr, ptr %t426, align 8
  %t428 = getelementptr i8, ptr %t427, i64 832
  %t429 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t430 = load ptr, ptr %t429, align 8
  %t431 = getelementptr i8, ptr %t430, i64 448
  %t432 = getelementptr i8, ptr %t431, i64 20
  %t433 = load i32, ptr %t432, align 4
  %t434 = getelementptr i8, ptr %t428, i64 20
  %t435 = load i32, ptr %t434, align 4
  %t442 = load i32, ptr %t431, align 4
  %t443 = getelementptr i8, ptr %t431, i64 16
  %t444 = load i8, ptr %t443, align 1
  %t436 = icmp eq i32 %t433, 2
  %t437 = icmp eq i32 %t433, 1
  %t438 = icmp eq i32 %t433, 5
  %t439 = or i1 %t437, %t438
  %t440 = or i1 %t439, %t436
  br i1 %t440, label %zr_aot_stack_copy_transfer_453, label %zr_aot_stack_copy_weak_check_453
zr_aot_stack_copy_transfer_453:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t428)
  %t454 = load %SZrTypeValue, ptr %t431, align 32
  store %SZrTypeValue %t454, ptr %t428, align 32
  %t455 = getelementptr i8, ptr %t431, i64 8
  %t456 = getelementptr i8, ptr %t431, i64 16
  %t457 = getelementptr i8, ptr %t431, i64 17
  %t458 = getelementptr i8, ptr %t431, i64 20
  %t459 = getelementptr i8, ptr %t431, i64 24
  %t460 = getelementptr i8, ptr %t431, i64 32
  store i32 0, ptr %t431, align 4
  store i64 0, ptr %t455, align 8
  store i8 0, ptr %t456, align 1
  store i8 1, ptr %t457, align 1
  store i32 0, ptr %t458, align 4
  store ptr null, ptr %t459, align 8
  store ptr null, ptr %t460, align 8
  br label %zr_aot_fn_0_ins_34
zr_aot_stack_copy_weak_check_453:
  %t441 = icmp eq i32 %t433, 3
  br i1 %t441, label %zr_aot_stack_copy_weak_453, label %zr_aot_stack_copy_fast_check_453
zr_aot_stack_copy_weak_453:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t428, ptr %t431)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t431)
  br label %zr_aot_fn_0_ins_34
zr_aot_stack_copy_fast_check_453:
  %t445 = icmp ne i8 %t444, 0
  %t446 = icmp eq i32 %t442, 18
  %t447 = and i1 %t445, %t446
  %t448 = icmp eq i32 %t433, 0
  %t449 = icmp eq i32 %t435, 0
  %t450 = and i1 %t448, %t449
  %t451 = xor i1 %t447, true
  %t452 = and i1 %t450, %t451
  br i1 %t452, label %zr_aot_stack_copy_fast_453, label %zr_aot_stack_copy_slow_453
zr_aot_stack_copy_fast_453:
  %t461 = load %SZrTypeValue, ptr %t431, align 32
  store %SZrTypeValue %t461, ptr %t428, align 32
  br label %zr_aot_fn_0_ins_34
zr_aot_stack_copy_slow_453:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t428, ptr %t431)
  br label %zr_aot_fn_0_ins_34

zr_aot_fn_0_ins_34:
  %t462 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 34, i32 5)
  br i1 %t462, label %zr_aot_fn_0_ins_34_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_34_body:
  %t463 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 9, i32 9, i32 4, ptr %direct_call)
  br i1 %t463, label %zr_aot_fn_0_ins_34_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_34_prepare_ok:
  %t464 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 9, i32 9, i32 4, i32 1)
  br i1 %t464, label %zr_aot_fn_0_ins_34_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_34_finish_ok:
  br label %zr_aot_fn_0_ins_35

zr_aot_fn_0_ins_35:
  %t465 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 35, i32 0)
  br i1 %t465, label %zr_aot_fn_0_ins_35_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_35_body:
  %t466 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 9)
  br i1 %t466, label %zr_aot_fn_0_ins_36, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_36:
  %t467 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 36, i32 1)
  br i1 %t467, label %zr_aot_fn_0_ins_36_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_36_body:
  %t468 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 9, i32 9, i32 1)
  br i1 %t468, label %zr_aot_fn_0_ins_37, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_37:
  %t469 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 37, i32 0)
  br i1 %t469, label %zr_aot_fn_0_ins_37_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_37_body:
  %t470 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 11)
  br i1 %t470, label %zr_aot_fn_0_ins_38, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_38:
  %t471 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 38, i32 1)
  br i1 %t471, label %zr_aot_fn_0_ins_38_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_38_body:
  %t472 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 11, i32 11, i32 4)
  br i1 %t472, label %zr_aot_fn_0_ins_39, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_39:
  %t473 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 39, i32 0)
  br i1 %t473, label %zr_aot_fn_0_ins_39_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_39_body:
  %t474 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t475 = load ptr, ptr %t474, align 8
  %t476 = getelementptr i8, ptr %t475, i64 640
  %t477 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t478 = load ptr, ptr %t477, align 8
  %t479 = getelementptr i8, ptr %t478, i64 704
  %t480 = getelementptr i8, ptr %t479, i64 20
  %t481 = load i32, ptr %t480, align 4
  %t482 = getelementptr i8, ptr %t476, i64 20
  %t483 = load i32, ptr %t482, align 4
  %t490 = load i32, ptr %t479, align 4
  %t491 = getelementptr i8, ptr %t479, i64 16
  %t492 = load i8, ptr %t491, align 1
  %t484 = icmp eq i32 %t481, 2
  %t485 = icmp eq i32 %t481, 1
  %t486 = icmp eq i32 %t481, 5
  %t487 = or i1 %t485, %t486
  %t488 = or i1 %t487, %t484
  br i1 %t488, label %zr_aot_stack_copy_transfer_501, label %zr_aot_stack_copy_weak_check_501
zr_aot_stack_copy_transfer_501:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t476)
  %t502 = load %SZrTypeValue, ptr %t479, align 32
  store %SZrTypeValue %t502, ptr %t476, align 32
  %t503 = getelementptr i8, ptr %t479, i64 8
  %t504 = getelementptr i8, ptr %t479, i64 16
  %t505 = getelementptr i8, ptr %t479, i64 17
  %t506 = getelementptr i8, ptr %t479, i64 20
  %t507 = getelementptr i8, ptr %t479, i64 24
  %t508 = getelementptr i8, ptr %t479, i64 32
  store i32 0, ptr %t479, align 4
  store i64 0, ptr %t503, align 8
  store i8 0, ptr %t504, align 1
  store i8 1, ptr %t505, align 1
  store i32 0, ptr %t506, align 4
  store ptr null, ptr %t507, align 8
  store ptr null, ptr %t508, align 8
  br label %zr_aot_fn_0_ins_40
zr_aot_stack_copy_weak_check_501:
  %t489 = icmp eq i32 %t481, 3
  br i1 %t489, label %zr_aot_stack_copy_weak_501, label %zr_aot_stack_copy_fast_check_501
zr_aot_stack_copy_weak_501:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t476, ptr %t479)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t479)
  br label %zr_aot_fn_0_ins_40
zr_aot_stack_copy_fast_check_501:
  %t493 = icmp ne i8 %t492, 0
  %t494 = icmp eq i32 %t490, 18
  %t495 = and i1 %t493, %t494
  %t496 = icmp eq i32 %t481, 0
  %t497 = icmp eq i32 %t483, 0
  %t498 = and i1 %t496, %t497
  %t499 = xor i1 %t495, true
  %t500 = and i1 %t498, %t499
  br i1 %t500, label %zr_aot_stack_copy_fast_501, label %zr_aot_stack_copy_slow_501
zr_aot_stack_copy_fast_501:
  %t509 = load %SZrTypeValue, ptr %t479, align 32
  store %SZrTypeValue %t509, ptr %t476, align 32
  br label %zr_aot_fn_0_ins_40
zr_aot_stack_copy_slow_501:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t476, ptr %t479)
  br label %zr_aot_fn_0_ins_40

zr_aot_fn_0_ins_40:
  %t510 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 40, i32 1)
  br i1 %t510, label %zr_aot_fn_0_ins_40_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_40_body:
  %t511 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t512 = load ptr, ptr %t511, align 8
  %t513 = getelementptr i8, ptr %t512, i64 704
  %t514 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 11, i32 17)
  br i1 %t514, label %zr_aot_fn_0_ins_41, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_41:
  %t515 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 41, i32 0)
  br i1 %t515, label %zr_aot_fn_0_ins_41_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_41_body:
  %t516 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t517 = load ptr, ptr %t516, align 8
  %t518 = getelementptr i8, ptr %t517, i64 768
  %t519 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t520 = load ptr, ptr %t519, align 8
  %t521 = getelementptr i8, ptr %t520, i64 576
  %t522 = getelementptr i8, ptr %t521, i64 20
  %t523 = load i32, ptr %t522, align 4
  %t524 = getelementptr i8, ptr %t518, i64 20
  %t525 = load i32, ptr %t524, align 4
  %t532 = load i32, ptr %t521, align 4
  %t533 = getelementptr i8, ptr %t521, i64 16
  %t534 = load i8, ptr %t533, align 1
  %t526 = icmp eq i32 %t523, 2
  %t527 = icmp eq i32 %t523, 1
  %t528 = icmp eq i32 %t523, 5
  %t529 = or i1 %t527, %t528
  %t530 = or i1 %t529, %t526
  br i1 %t530, label %zr_aot_stack_copy_transfer_543, label %zr_aot_stack_copy_weak_check_543
zr_aot_stack_copy_transfer_543:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t518)
  %t544 = load %SZrTypeValue, ptr %t521, align 32
  store %SZrTypeValue %t544, ptr %t518, align 32
  %t545 = getelementptr i8, ptr %t521, i64 8
  %t546 = getelementptr i8, ptr %t521, i64 16
  %t547 = getelementptr i8, ptr %t521, i64 17
  %t548 = getelementptr i8, ptr %t521, i64 20
  %t549 = getelementptr i8, ptr %t521, i64 24
  %t550 = getelementptr i8, ptr %t521, i64 32
  store i32 0, ptr %t521, align 4
  store i64 0, ptr %t545, align 8
  store i8 0, ptr %t546, align 1
  store i8 1, ptr %t547, align 1
  store i32 0, ptr %t548, align 4
  store ptr null, ptr %t549, align 8
  store ptr null, ptr %t550, align 8
  br label %zr_aot_fn_0_ins_42
zr_aot_stack_copy_weak_check_543:
  %t531 = icmp eq i32 %t523, 3
  br i1 %t531, label %zr_aot_stack_copy_weak_543, label %zr_aot_stack_copy_fast_check_543
zr_aot_stack_copy_weak_543:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t518, ptr %t521)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t521)
  br label %zr_aot_fn_0_ins_42
zr_aot_stack_copy_fast_check_543:
  %t535 = icmp ne i8 %t534, 0
  %t536 = icmp eq i32 %t532, 18
  %t537 = and i1 %t535, %t536
  %t538 = icmp eq i32 %t523, 0
  %t539 = icmp eq i32 %t525, 0
  %t540 = and i1 %t538, %t539
  %t541 = xor i1 %t537, true
  %t542 = and i1 %t540, %t541
  br i1 %t542, label %zr_aot_stack_copy_fast_543, label %zr_aot_stack_copy_slow_543
zr_aot_stack_copy_fast_543:
  %t551 = load %SZrTypeValue, ptr %t521, align 32
  store %SZrTypeValue %t551, ptr %t518, align 32
  br label %zr_aot_fn_0_ins_42
zr_aot_stack_copy_slow_543:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t518, ptr %t521)
  br label %zr_aot_fn_0_ins_42

zr_aot_fn_0_ins_42:
  %t552 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 42, i32 0)
  br i1 %t552, label %zr_aot_fn_0_ins_42_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_42_body:
  %t553 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t554 = load ptr, ptr %t553, align 8
  %t555 = getelementptr i8, ptr %t554, i64 832
  %t556 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t557 = load ptr, ptr %t556, align 8
  %t558 = getelementptr i8, ptr %t557, i64 640
  %t559 = getelementptr i8, ptr %t558, i64 20
  %t560 = load i32, ptr %t559, align 4
  %t561 = getelementptr i8, ptr %t555, i64 20
  %t562 = load i32, ptr %t561, align 4
  %t569 = load i32, ptr %t558, align 4
  %t570 = getelementptr i8, ptr %t558, i64 16
  %t571 = load i8, ptr %t570, align 1
  %t563 = icmp eq i32 %t560, 2
  %t564 = icmp eq i32 %t560, 1
  %t565 = icmp eq i32 %t560, 5
  %t566 = or i1 %t564, %t565
  %t567 = or i1 %t566, %t563
  br i1 %t567, label %zr_aot_stack_copy_transfer_580, label %zr_aot_stack_copy_weak_check_580
zr_aot_stack_copy_transfer_580:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t555)
  %t581 = load %SZrTypeValue, ptr %t558, align 32
  store %SZrTypeValue %t581, ptr %t555, align 32
  %t582 = getelementptr i8, ptr %t558, i64 8
  %t583 = getelementptr i8, ptr %t558, i64 16
  %t584 = getelementptr i8, ptr %t558, i64 17
  %t585 = getelementptr i8, ptr %t558, i64 20
  %t586 = getelementptr i8, ptr %t558, i64 24
  %t587 = getelementptr i8, ptr %t558, i64 32
  store i32 0, ptr %t558, align 4
  store i64 0, ptr %t582, align 8
  store i8 0, ptr %t583, align 1
  store i8 1, ptr %t584, align 1
  store i32 0, ptr %t585, align 4
  store ptr null, ptr %t586, align 8
  store ptr null, ptr %t587, align 8
  br label %zr_aot_fn_0_ins_43
zr_aot_stack_copy_weak_check_580:
  %t568 = icmp eq i32 %t560, 3
  br i1 %t568, label %zr_aot_stack_copy_weak_580, label %zr_aot_stack_copy_fast_check_580
zr_aot_stack_copy_weak_580:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t555, ptr %t558)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t558)
  br label %zr_aot_fn_0_ins_43
zr_aot_stack_copy_fast_check_580:
  %t572 = icmp ne i8 %t571, 0
  %t573 = icmp eq i32 %t569, 18
  %t574 = and i1 %t572, %t573
  %t575 = icmp eq i32 %t560, 0
  %t576 = icmp eq i32 %t562, 0
  %t577 = and i1 %t575, %t576
  %t578 = xor i1 %t574, true
  %t579 = and i1 %t577, %t578
  br i1 %t579, label %zr_aot_stack_copy_fast_580, label %zr_aot_stack_copy_slow_580
zr_aot_stack_copy_fast_580:
  %t588 = load %SZrTypeValue, ptr %t558, align 32
  store %SZrTypeValue %t588, ptr %t555, align 32
  br label %zr_aot_fn_0_ins_43
zr_aot_stack_copy_slow_580:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t555, ptr %t558)
  br label %zr_aot_fn_0_ins_43

zr_aot_fn_0_ins_43:
  %t589 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 43, i32 5)
  br i1 %t589, label %zr_aot_fn_0_ins_43_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_43_body:
  %t590 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 11, i32 11, i32 2, ptr %direct_call)
  br i1 %t590, label %zr_aot_fn_0_ins_43_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_43_prepare_ok:
  %t591 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 11, i32 11, i32 2, i32 1)
  br i1 %t591, label %zr_aot_fn_0_ins_43_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_43_finish_ok:
  br label %zr_aot_fn_0_ins_44

zr_aot_fn_0_ins_44:
  %t592 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 44, i32 1)
  br i1 %t592, label %zr_aot_fn_0_ins_44_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_44_body:
  %t593 = call i1 @ZrLibrary_AotRuntime_CreateClosure(ptr %state, ptr %frame, i32 11, i32 18)
  br i1 %t593, label %zr_aot_fn_0_ins_45, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_45:
  %t594 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 45, i32 0)
  br i1 %t594, label %zr_aot_fn_0_ins_45_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_45_body:
  %t595 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t596 = load ptr, ptr %t595, align 8
  %t597 = getelementptr i8, ptr %t596, i64 64
  %t598 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t599 = load ptr, ptr %t598, align 8
  %t600 = getelementptr i8, ptr %t599, i64 704
  %t601 = getelementptr i8, ptr %t600, i64 20
  %t602 = load i32, ptr %t601, align 4
  %t603 = getelementptr i8, ptr %t597, i64 20
  %t604 = load i32, ptr %t603, align 4
  %t611 = load i32, ptr %t600, align 4
  %t612 = getelementptr i8, ptr %t600, i64 16
  %t613 = load i8, ptr %t612, align 1
  %t605 = icmp eq i32 %t602, 2
  %t606 = icmp eq i32 %t602, 1
  %t607 = icmp eq i32 %t602, 5
  %t608 = or i1 %t606, %t607
  %t609 = or i1 %t608, %t605
  br i1 %t609, label %zr_aot_stack_copy_transfer_622, label %zr_aot_stack_copy_weak_check_622
zr_aot_stack_copy_transfer_622:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t597)
  %t623 = load %SZrTypeValue, ptr %t600, align 32
  store %SZrTypeValue %t623, ptr %t597, align 32
  %t624 = getelementptr i8, ptr %t600, i64 8
  %t625 = getelementptr i8, ptr %t600, i64 16
  %t626 = getelementptr i8, ptr %t600, i64 17
  %t627 = getelementptr i8, ptr %t600, i64 20
  %t628 = getelementptr i8, ptr %t600, i64 24
  %t629 = getelementptr i8, ptr %t600, i64 32
  store i32 0, ptr %t600, align 4
  store i64 0, ptr %t624, align 8
  store i8 0, ptr %t625, align 1
  store i8 1, ptr %t626, align 1
  store i32 0, ptr %t627, align 4
  store ptr null, ptr %t628, align 8
  store ptr null, ptr %t629, align 8
  br label %zr_aot_fn_0_ins_46
zr_aot_stack_copy_weak_check_622:
  %t610 = icmp eq i32 %t602, 3
  br i1 %t610, label %zr_aot_stack_copy_weak_622, label %zr_aot_stack_copy_fast_check_622
zr_aot_stack_copy_weak_622:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t597, ptr %t600)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t600)
  br label %zr_aot_fn_0_ins_46
zr_aot_stack_copy_fast_check_622:
  %t614 = icmp ne i8 %t613, 0
  %t615 = icmp eq i32 %t611, 18
  %t616 = and i1 %t614, %t615
  %t617 = icmp eq i32 %t602, 0
  %t618 = icmp eq i32 %t604, 0
  %t619 = and i1 %t617, %t618
  %t620 = xor i1 %t616, true
  %t621 = and i1 %t619, %t620
  br i1 %t621, label %zr_aot_stack_copy_fast_622, label %zr_aot_stack_copy_slow_622
zr_aot_stack_copy_fast_622:
  %t630 = load %SZrTypeValue, ptr %t600, align 32
  store %SZrTypeValue %t630, ptr %t597, align 32
  br label %zr_aot_fn_0_ins_46
zr_aot_stack_copy_slow_622:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t597, ptr %t600)
  br label %zr_aot_fn_0_ins_46

zr_aot_fn_0_ins_46:
  %t631 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 46, i32 0)
  br i1 %t631, label %zr_aot_fn_0_ins_46_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_46_body:
  %t632 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t633 = load ptr, ptr %t632, align 8
  %t634 = getelementptr i8, ptr %t633, i64 832
  %t635 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t636 = load ptr, ptr %t635, align 8
  %t637 = getelementptr i8, ptr %t636, i64 20
  %t638 = load i32, ptr %t637, align 4
  %t639 = getelementptr i8, ptr %t634, i64 20
  %t640 = load i32, ptr %t639, align 4
  %t647 = load i32, ptr %t636, align 4
  %t648 = getelementptr i8, ptr %t636, i64 16
  %t649 = load i8, ptr %t648, align 1
  %t641 = icmp eq i32 %t638, 2
  %t642 = icmp eq i32 %t638, 1
  %t643 = icmp eq i32 %t638, 5
  %t644 = or i1 %t642, %t643
  %t645 = or i1 %t644, %t641
  br i1 %t645, label %zr_aot_stack_copy_transfer_658, label %zr_aot_stack_copy_weak_check_658
zr_aot_stack_copy_transfer_658:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t634)
  %t659 = load %SZrTypeValue, ptr %t636, align 32
  store %SZrTypeValue %t659, ptr %t634, align 32
  %t660 = getelementptr i8, ptr %t636, i64 8
  %t661 = getelementptr i8, ptr %t636, i64 16
  %t662 = getelementptr i8, ptr %t636, i64 17
  %t663 = getelementptr i8, ptr %t636, i64 20
  %t664 = getelementptr i8, ptr %t636, i64 24
  %t665 = getelementptr i8, ptr %t636, i64 32
  store i32 0, ptr %t636, align 4
  store i64 0, ptr %t660, align 8
  store i8 0, ptr %t661, align 1
  store i8 1, ptr %t662, align 1
  store i32 0, ptr %t663, align 4
  store ptr null, ptr %t664, align 8
  store ptr null, ptr %t665, align 8
  br label %zr_aot_fn_0_ins_47
zr_aot_stack_copy_weak_check_658:
  %t646 = icmp eq i32 %t638, 3
  br i1 %t646, label %zr_aot_stack_copy_weak_658, label %zr_aot_stack_copy_fast_check_658
zr_aot_stack_copy_weak_658:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t634, ptr %t636)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t636)
  br label %zr_aot_fn_0_ins_47
zr_aot_stack_copy_fast_check_658:
  %t650 = icmp ne i8 %t649, 0
  %t651 = icmp eq i32 %t647, 18
  %t652 = and i1 %t650, %t651
  %t653 = icmp eq i32 %t638, 0
  %t654 = icmp eq i32 %t640, 0
  %t655 = and i1 %t653, %t654
  %t656 = xor i1 %t652, true
  %t657 = and i1 %t655, %t656
  br i1 %t657, label %zr_aot_stack_copy_fast_658, label %zr_aot_stack_copy_slow_658
zr_aot_stack_copy_fast_658:
  %t666 = load %SZrTypeValue, ptr %t636, align 32
  store %SZrTypeValue %t666, ptr %t634, align 32
  br label %zr_aot_fn_0_ins_47
zr_aot_stack_copy_slow_658:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t634, ptr %t636)
  br label %zr_aot_fn_0_ins_47

zr_aot_fn_0_ins_47:
  %t667 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 47, i32 0)
  br i1 %t667, label %zr_aot_fn_0_ins_47_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_47_body:
  %t668 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t669 = load ptr, ptr %t668, align 8
  %t670 = getelementptr i8, ptr %t669, i64 768
  %t671 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t672 = load ptr, ptr %t671, align 8
  %t673 = getelementptr i8, ptr %t672, i64 832
  %t674 = getelementptr i8, ptr %t673, i64 20
  %t675 = load i32, ptr %t674, align 4
  %t676 = getelementptr i8, ptr %t670, i64 20
  %t677 = load i32, ptr %t676, align 4
  %t684 = load i32, ptr %t673, align 4
  %t685 = getelementptr i8, ptr %t673, i64 16
  %t686 = load i8, ptr %t685, align 1
  %t678 = icmp eq i32 %t675, 2
  %t679 = icmp eq i32 %t675, 1
  %t680 = icmp eq i32 %t675, 5
  %t681 = or i1 %t679, %t680
  %t682 = or i1 %t681, %t678
  br i1 %t682, label %zr_aot_stack_copy_transfer_695, label %zr_aot_stack_copy_weak_check_695
zr_aot_stack_copy_transfer_695:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t670)
  %t696 = load %SZrTypeValue, ptr %t673, align 32
  store %SZrTypeValue %t696, ptr %t670, align 32
  %t697 = getelementptr i8, ptr %t673, i64 8
  %t698 = getelementptr i8, ptr %t673, i64 16
  %t699 = getelementptr i8, ptr %t673, i64 17
  %t700 = getelementptr i8, ptr %t673, i64 20
  %t701 = getelementptr i8, ptr %t673, i64 24
  %t702 = getelementptr i8, ptr %t673, i64 32
  store i32 0, ptr %t673, align 4
  store i64 0, ptr %t697, align 8
  store i8 0, ptr %t698, align 1
  store i8 1, ptr %t699, align 1
  store i32 0, ptr %t700, align 4
  store ptr null, ptr %t701, align 8
  store ptr null, ptr %t702, align 8
  br label %zr_aot_fn_0_ins_48
zr_aot_stack_copy_weak_check_695:
  %t683 = icmp eq i32 %t675, 3
  br i1 %t683, label %zr_aot_stack_copy_weak_695, label %zr_aot_stack_copy_fast_check_695
zr_aot_stack_copy_weak_695:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t670, ptr %t673)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t673)
  br label %zr_aot_fn_0_ins_48
zr_aot_stack_copy_fast_check_695:
  %t687 = icmp ne i8 %t686, 0
  %t688 = icmp eq i32 %t684, 18
  %t689 = and i1 %t687, %t688
  %t690 = icmp eq i32 %t675, 0
  %t691 = icmp eq i32 %t677, 0
  %t692 = and i1 %t690, %t691
  %t693 = xor i1 %t689, true
  %t694 = and i1 %t692, %t693
  br i1 %t694, label %zr_aot_stack_copy_fast_695, label %zr_aot_stack_copy_slow_695
zr_aot_stack_copy_fast_695:
  %t703 = load %SZrTypeValue, ptr %t673, align 32
  store %SZrTypeValue %t703, ptr %t670, align 32
  br label %zr_aot_fn_0_ins_48
zr_aot_stack_copy_slow_695:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t670, ptr %t673)
  br label %zr_aot_fn_0_ins_48

zr_aot_fn_0_ins_48:
  %t704 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 48, i32 1)
  br i1 %t704, label %zr_aot_fn_0_ins_48_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_48_body:
  %t705 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t706 = load ptr, ptr %t705, align 8
  %t707 = getelementptr i8, ptr %t706, i64 832
  %t708 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 13, i32 19)
  br i1 %t708, label %zr_aot_fn_0_ins_49, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_49:
  %t709 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 49, i32 0)
  br i1 %t709, label %zr_aot_fn_0_ins_49_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_49_body:
  %t710 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t711 = load ptr, ptr %t710, align 8
  %t712 = getelementptr i8, ptr %t711, i64 896
  %t713 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t714 = load ptr, ptr %t713, align 8
  %t715 = getelementptr i8, ptr %t714, i64 64
  %t716 = getelementptr i8, ptr %t715, i64 20
  %t717 = load i32, ptr %t716, align 4
  %t718 = getelementptr i8, ptr %t712, i64 20
  %t719 = load i32, ptr %t718, align 4
  %t726 = load i32, ptr %t715, align 4
  %t727 = getelementptr i8, ptr %t715, i64 16
  %t728 = load i8, ptr %t727, align 1
  %t720 = icmp eq i32 %t717, 2
  %t721 = icmp eq i32 %t717, 1
  %t722 = icmp eq i32 %t717, 5
  %t723 = or i1 %t721, %t722
  %t724 = or i1 %t723, %t720
  br i1 %t724, label %zr_aot_stack_copy_transfer_737, label %zr_aot_stack_copy_weak_check_737
zr_aot_stack_copy_transfer_737:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t712)
  %t738 = load %SZrTypeValue, ptr %t715, align 32
  store %SZrTypeValue %t738, ptr %t712, align 32
  %t739 = getelementptr i8, ptr %t715, i64 8
  %t740 = getelementptr i8, ptr %t715, i64 16
  %t741 = getelementptr i8, ptr %t715, i64 17
  %t742 = getelementptr i8, ptr %t715, i64 20
  %t743 = getelementptr i8, ptr %t715, i64 24
  %t744 = getelementptr i8, ptr %t715, i64 32
  store i32 0, ptr %t715, align 4
  store i64 0, ptr %t739, align 8
  store i8 0, ptr %t740, align 1
  store i8 1, ptr %t741, align 1
  store i32 0, ptr %t742, align 4
  store ptr null, ptr %t743, align 8
  store ptr null, ptr %t744, align 8
  br label %zr_aot_fn_0_ins_50
zr_aot_stack_copy_weak_check_737:
  %t725 = icmp eq i32 %t717, 3
  br i1 %t725, label %zr_aot_stack_copy_weak_737, label %zr_aot_stack_copy_fast_check_737
zr_aot_stack_copy_weak_737:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t712, ptr %t715)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t715)
  br label %zr_aot_fn_0_ins_50
zr_aot_stack_copy_fast_check_737:
  %t729 = icmp ne i8 %t728, 0
  %t730 = icmp eq i32 %t726, 18
  %t731 = and i1 %t729, %t730
  %t732 = icmp eq i32 %t717, 0
  %t733 = icmp eq i32 %t719, 0
  %t734 = and i1 %t732, %t733
  %t735 = xor i1 %t731, true
  %t736 = and i1 %t734, %t735
  br i1 %t736, label %zr_aot_stack_copy_fast_737, label %zr_aot_stack_copy_slow_737
zr_aot_stack_copy_fast_737:
  %t745 = load %SZrTypeValue, ptr %t715, align 32
  store %SZrTypeValue %t745, ptr %t712, align 32
  br label %zr_aot_fn_0_ins_50
zr_aot_stack_copy_slow_737:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t712, ptr %t715)
  br label %zr_aot_fn_0_ins_50

zr_aot_fn_0_ins_50:
  %t746 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 50, i32 0)
  br i1 %t746, label %zr_aot_fn_0_ins_50_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_50_body:
  %t747 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t748 = load ptr, ptr %t747, align 8
  %t749 = getelementptr i8, ptr %t748, i64 960
  %t750 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t751 = load ptr, ptr %t750, align 8
  %t752 = getelementptr i8, ptr %t751, i64 768
  %t753 = getelementptr i8, ptr %t752, i64 20
  %t754 = load i32, ptr %t753, align 4
  %t755 = getelementptr i8, ptr %t749, i64 20
  %t756 = load i32, ptr %t755, align 4
  %t763 = load i32, ptr %t752, align 4
  %t764 = getelementptr i8, ptr %t752, i64 16
  %t765 = load i8, ptr %t764, align 1
  %t757 = icmp eq i32 %t754, 2
  %t758 = icmp eq i32 %t754, 1
  %t759 = icmp eq i32 %t754, 5
  %t760 = or i1 %t758, %t759
  %t761 = or i1 %t760, %t757
  br i1 %t761, label %zr_aot_stack_copy_transfer_774, label %zr_aot_stack_copy_weak_check_774
zr_aot_stack_copy_transfer_774:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t749)
  %t775 = load %SZrTypeValue, ptr %t752, align 32
  store %SZrTypeValue %t775, ptr %t749, align 32
  %t776 = getelementptr i8, ptr %t752, i64 8
  %t777 = getelementptr i8, ptr %t752, i64 16
  %t778 = getelementptr i8, ptr %t752, i64 17
  %t779 = getelementptr i8, ptr %t752, i64 20
  %t780 = getelementptr i8, ptr %t752, i64 24
  %t781 = getelementptr i8, ptr %t752, i64 32
  store i32 0, ptr %t752, align 4
  store i64 0, ptr %t776, align 8
  store i8 0, ptr %t777, align 1
  store i8 1, ptr %t778, align 1
  store i32 0, ptr %t779, align 4
  store ptr null, ptr %t780, align 8
  store ptr null, ptr %t781, align 8
  br label %zr_aot_fn_0_ins_51
zr_aot_stack_copy_weak_check_774:
  %t762 = icmp eq i32 %t754, 3
  br i1 %t762, label %zr_aot_stack_copy_weak_774, label %zr_aot_stack_copy_fast_check_774
zr_aot_stack_copy_weak_774:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t749, ptr %t752)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t752)
  br label %zr_aot_fn_0_ins_51
zr_aot_stack_copy_fast_check_774:
  %t766 = icmp ne i8 %t765, 0
  %t767 = icmp eq i32 %t763, 18
  %t768 = and i1 %t766, %t767
  %t769 = icmp eq i32 %t754, 0
  %t770 = icmp eq i32 %t756, 0
  %t771 = and i1 %t769, %t770
  %t772 = xor i1 %t768, true
  %t773 = and i1 %t771, %t772
  br i1 %t773, label %zr_aot_stack_copy_fast_774, label %zr_aot_stack_copy_slow_774
zr_aot_stack_copy_fast_774:
  %t782 = load %SZrTypeValue, ptr %t752, align 32
  store %SZrTypeValue %t782, ptr %t749, align 32
  br label %zr_aot_fn_0_ins_51
zr_aot_stack_copy_slow_774:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t749, ptr %t752)
  br label %zr_aot_fn_0_ins_51

zr_aot_fn_0_ins_51:
  %t783 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 51, i32 5)
  br i1 %t783, label %zr_aot_fn_0_ins_51_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_51_body:
  %t784 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 13, i32 13, i32 2, ptr %direct_call)
  br i1 %t784, label %zr_aot_fn_0_ins_51_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_51_prepare_ok:
  %t785 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 13, i32 13, i32 2, i32 1)
  br i1 %t785, label %zr_aot_fn_0_ins_51_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_51_finish_ok:
  br label %zr_aot_fn_0_ins_52

zr_aot_fn_0_ins_52:
  %t786 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 52, i32 0)
  br i1 %t786, label %zr_aot_fn_0_ins_52_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_52_body:
  %t787 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t788 = load ptr, ptr %t787, align 8
  %t789 = getelementptr i8, ptr %t788, i64 64
  %t790 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t791 = load ptr, ptr %t790, align 8
  %t792 = getelementptr i8, ptr %t791, i64 832
  %t793 = getelementptr i8, ptr %t792, i64 20
  %t794 = load i32, ptr %t793, align 4
  %t795 = getelementptr i8, ptr %t789, i64 20
  %t796 = load i32, ptr %t795, align 4
  %t803 = load i32, ptr %t792, align 4
  %t804 = getelementptr i8, ptr %t792, i64 16
  %t805 = load i8, ptr %t804, align 1
  %t797 = icmp eq i32 %t794, 2
  %t798 = icmp eq i32 %t794, 1
  %t799 = icmp eq i32 %t794, 5
  %t800 = or i1 %t798, %t799
  %t801 = or i1 %t800, %t797
  br i1 %t801, label %zr_aot_stack_copy_transfer_814, label %zr_aot_stack_copy_weak_check_814
zr_aot_stack_copy_transfer_814:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t789)
  %t815 = load %SZrTypeValue, ptr %t792, align 32
  store %SZrTypeValue %t815, ptr %t789, align 32
  %t816 = getelementptr i8, ptr %t792, i64 8
  %t817 = getelementptr i8, ptr %t792, i64 16
  %t818 = getelementptr i8, ptr %t792, i64 17
  %t819 = getelementptr i8, ptr %t792, i64 20
  %t820 = getelementptr i8, ptr %t792, i64 24
  %t821 = getelementptr i8, ptr %t792, i64 32
  store i32 0, ptr %t792, align 4
  store i64 0, ptr %t816, align 8
  store i8 0, ptr %t817, align 1
  store i8 1, ptr %t818, align 1
  store i32 0, ptr %t819, align 4
  store ptr null, ptr %t820, align 8
  store ptr null, ptr %t821, align 8
  br label %zr_aot_fn_0_ins_53
zr_aot_stack_copy_weak_check_814:
  %t802 = icmp eq i32 %t794, 3
  br i1 %t802, label %zr_aot_stack_copy_weak_814, label %zr_aot_stack_copy_fast_check_814
zr_aot_stack_copy_weak_814:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t789, ptr %t792)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t792)
  br label %zr_aot_fn_0_ins_53
zr_aot_stack_copy_fast_check_814:
  %t806 = icmp ne i8 %t805, 0
  %t807 = icmp eq i32 %t803, 18
  %t808 = and i1 %t806, %t807
  %t809 = icmp eq i32 %t794, 0
  %t810 = icmp eq i32 %t796, 0
  %t811 = and i1 %t809, %t810
  %t812 = xor i1 %t808, true
  %t813 = and i1 %t811, %t812
  br i1 %t813, label %zr_aot_stack_copy_fast_814, label %zr_aot_stack_copy_slow_814
zr_aot_stack_copy_fast_814:
  %t822 = load %SZrTypeValue, ptr %t792, align 32
  store %SZrTypeValue %t822, ptr %t789, align 32
  br label %zr_aot_fn_0_ins_53
zr_aot_stack_copy_slow_814:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t789, ptr %t792)
  br label %zr_aot_fn_0_ins_53

zr_aot_fn_0_ins_53:
  %t823 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 53, i32 0)
  br i1 %t823, label %zr_aot_fn_0_ins_53_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_53_body:
  %t824 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t825 = load ptr, ptr %t824, align 8
  %t826 = getelementptr i8, ptr %t825, i64 832
  %t827 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t828 = load ptr, ptr %t827, align 8
  %t829 = getelementptr i8, ptr %t828, i64 64
  %t830 = getelementptr i8, ptr %t829, i64 20
  %t831 = load i32, ptr %t830, align 4
  %t832 = getelementptr i8, ptr %t826, i64 20
  %t833 = load i32, ptr %t832, align 4
  %t840 = load i32, ptr %t829, align 4
  %t841 = getelementptr i8, ptr %t829, i64 16
  %t842 = load i8, ptr %t841, align 1
  %t834 = icmp eq i32 %t831, 2
  %t835 = icmp eq i32 %t831, 1
  %t836 = icmp eq i32 %t831, 5
  %t837 = or i1 %t835, %t836
  %t838 = or i1 %t837, %t834
  br i1 %t838, label %zr_aot_stack_copy_transfer_851, label %zr_aot_stack_copy_weak_check_851
zr_aot_stack_copy_transfer_851:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t826)
  %t852 = load %SZrTypeValue, ptr %t829, align 32
  store %SZrTypeValue %t852, ptr %t826, align 32
  %t853 = getelementptr i8, ptr %t829, i64 8
  %t854 = getelementptr i8, ptr %t829, i64 16
  %t855 = getelementptr i8, ptr %t829, i64 17
  %t856 = getelementptr i8, ptr %t829, i64 20
  %t857 = getelementptr i8, ptr %t829, i64 24
  %t858 = getelementptr i8, ptr %t829, i64 32
  store i32 0, ptr %t829, align 4
  store i64 0, ptr %t853, align 8
  store i8 0, ptr %t854, align 1
  store i8 1, ptr %t855, align 1
  store i32 0, ptr %t856, align 4
  store ptr null, ptr %t857, align 8
  store ptr null, ptr %t858, align 8
  br label %zr_aot_fn_0_ins_54
zr_aot_stack_copy_weak_check_851:
  %t839 = icmp eq i32 %t831, 3
  br i1 %t839, label %zr_aot_stack_copy_weak_851, label %zr_aot_stack_copy_fast_check_851
zr_aot_stack_copy_weak_851:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t826, ptr %t829)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t829)
  br label %zr_aot_fn_0_ins_54
zr_aot_stack_copy_fast_check_851:
  %t843 = icmp ne i8 %t842, 0
  %t844 = icmp eq i32 %t840, 18
  %t845 = and i1 %t843, %t844
  %t846 = icmp eq i32 %t831, 0
  %t847 = icmp eq i32 %t833, 0
  %t848 = and i1 %t846, %t847
  %t849 = xor i1 %t845, true
  %t850 = and i1 %t848, %t849
  br i1 %t850, label %zr_aot_stack_copy_fast_851, label %zr_aot_stack_copy_slow_851
zr_aot_stack_copy_fast_851:
  %t859 = load %SZrTypeValue, ptr %t829, align 32
  store %SZrTypeValue %t859, ptr %t826, align 32
  br label %zr_aot_fn_0_ins_54
zr_aot_stack_copy_slow_851:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t826, ptr %t829)
  br label %zr_aot_fn_0_ins_54

zr_aot_fn_0_ins_54:
  %t860 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 54, i32 13)
  br i1 %t860, label %zr_aot_fn_0_ins_54_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_54_body:
  %t861 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 13, i32 13, i32 0, ptr %direct_call)
  br i1 %t861, label %zr_aot_fn_0_ins_54_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_54_prepare_ok:
  %t862 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 13, i32 13, i32 0, i32 1)
  br i1 %t862, label %zr_aot_fn_0_ins_54_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_54_finish_ok:
  %t863 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 13, i1 true)
  ret i64 %t863

zr_aot_fn_0_ins_55:
  %t864 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 55, i32 8)
  br i1 %t864, label %zr_aot_fn_0_ins_55_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_55_body:
  %t865 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 13, i1 true)
  ret i64 %t865

zr_aot_fn_0_end_unsupported:
  %t866 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 0, i32 56, i32 0)
  ret i64 %t866

zr_aot_fn_0_fail:
  %t867 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t867
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
  %t2 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t3 = load ptr, ptr %t2, align 8
  %t4 = getelementptr i8, ptr %t3, i64 128
  %t5 = getelementptr i8, ptr %t4, i64 8
  %t6 = getelementptr i8, ptr %t4, i64 16
  %t7 = getelementptr i8, ptr %t4, i64 17
  %t8 = getelementptr i8, ptr %t4, i64 20
  %t9 = getelementptr i8, ptr %t4, i64 24
  %t10 = getelementptr i8, ptr %t4, i64 32
  store i32 1, ptr %t4, align 4
  store i64 1, ptr %t5, align 8
  store i8 0, ptr %t6, align 1
  store i8 1, ptr %t7, align 1
  store i32 0, ptr %t8, align 4
  store ptr null, ptr %t9, align 8
  store ptr null, ptr %t10, align 8
  br label %zr_aot_fn_1_ins_1

zr_aot_fn_1_ins_1:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t11, label %zr_aot_fn_1_ins_1_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_1_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr %state, ptr %frame, i32 3, i32 1, i32 0)
  br i1 %t12, label %zr_aot_fn_1_ins_2, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_2:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t13, label %zr_aot_fn_1_ins_2_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_2_body:
  %t14 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t15 = load ptr, ptr %t14, align 8
  %t16 = getelementptr i8, ptr %t15, i64 256
  %t17 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t17, label %zr_aot_fn_1_ins_3, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_3:
  %t18 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t18, label %zr_aot_fn_1_ins_3_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_3_body:
  %t19 = call i1 @ZrLibrary_AotRuntime_SetByIndex(ptr %state, ptr %frame, i32 2, i32 3, i32 4)
  br i1 %t19, label %zr_aot_fn_1_ins_4, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_4:
  %t20 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t20, label %zr_aot_fn_1_ins_4_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_4_body:
  %t21 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t22 = load ptr, ptr %t21, align 8
  %t23 = getelementptr i8, ptr %t22, i64 128
  %t24 = getelementptr i8, ptr %t23, i64 8
  %t25 = getelementptr i8, ptr %t23, i64 16
  %t26 = getelementptr i8, ptr %t23, i64 17
  %t27 = getelementptr i8, ptr %t23, i64 20
  %t28 = getelementptr i8, ptr %t23, i64 24
  %t29 = getelementptr i8, ptr %t23, i64 32
  store i32 0, ptr %t23, align 4
  store i64 0, ptr %t24, align 8
  store i8 0, ptr %t25, align 1
  store i8 1, ptr %t26, align 1
  store i32 0, ptr %t27, align 4
  store ptr null, ptr %t28, align 8
  store ptr null, ptr %t29, align 8
  br label %zr_aot_fn_1_ins_5

zr_aot_fn_1_ins_5:
  %t30 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 8)
  br i1 %t30, label %zr_aot_fn_1_ins_5_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_5_body:
  %t31 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 false)
  ret i64 %t31

zr_aot_fn_1_end_unsupported:
  %t32 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 1, i32 6, i32 0)
  ret i64 %t32

zr_aot_fn_1_fail:
  %t33 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t33
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
  %t2 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t3 = load ptr, ptr %t2, align 8
  %t4 = getelementptr i8, ptr %t3, i64 64
  %t5 = getelementptr i8, ptr %t4, i64 8
  %t6 = getelementptr i8, ptr %t4, i64 16
  %t7 = getelementptr i8, ptr %t4, i64 17
  %t8 = getelementptr i8, ptr %t4, i64 20
  %t9 = getelementptr i8, ptr %t4, i64 24
  %t10 = getelementptr i8, ptr %t4, i64 32
  store i32 1, ptr %t4, align 4
  store i64 1, ptr %t5, align 8
  store i8 0, ptr %t6, align 1
  store i8 1, ptr %t7, align 1
  store i32 0, ptr %t8, align 4
  store ptr null, ptr %t9, align 8
  store ptr null, ptr %t10, align 8
  br label %zr_aot_fn_2_ins_1

zr_aot_fn_2_ins_1:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t11, label %zr_aot_fn_2_ins_1_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_1_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr %state, ptr %frame, i32 2, i32 0, i32 0)
  br i1 %t12, label %zr_aot_fn_2_ins_2, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_2:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t13, label %zr_aot_fn_2_ins_2_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_2_body:
  %t14 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t15 = load ptr, ptr %t14, align 8
  %t16 = getelementptr i8, ptr %t15, i64 192
  %t17 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t17, label %zr_aot_fn_2_ins_3, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_3:
  %t18 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t18, label %zr_aot_fn_2_ins_3_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_3_body:
  %t19 = call i1 @ZrLibrary_AotRuntime_SetByIndex(ptr %state, ptr %frame, i32 1, i32 2, i32 3)
  br i1 %t19, label %zr_aot_fn_2_ins_4, label %zr_aot_fn_2_fail

zr_aot_fn_2_ins_4:
  %t20 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t20, label %zr_aot_fn_2_ins_4_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_4_body:
  %t21 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t22 = load ptr, ptr %t21, align 8
  %t23 = getelementptr i8, ptr %t22, i64 64
  %t24 = getelementptr i8, ptr %t23, i64 8
  %t25 = getelementptr i8, ptr %t23, i64 16
  %t26 = getelementptr i8, ptr %t23, i64 17
  %t27 = getelementptr i8, ptr %t23, i64 20
  %t28 = getelementptr i8, ptr %t23, i64 24
  %t29 = getelementptr i8, ptr %t23, i64 32
  store i32 0, ptr %t23, align 4
  store i64 0, ptr %t24, align 8
  store i8 0, ptr %t25, align 1
  store i8 1, ptr %t26, align 1
  store i32 0, ptr %t27, align 4
  store ptr null, ptr %t28, align 8
  store ptr null, ptr %t29, align 8
  br label %zr_aot_fn_2_ins_5

zr_aot_fn_2_ins_5:
  %t30 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 8)
  br i1 %t30, label %zr_aot_fn_2_ins_5_body, label %zr_aot_fn_2_fail
zr_aot_fn_2_ins_5_body:
  %t31 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 1, i1 false)
  ret i64 %t31

zr_aot_fn_2_end_unsupported:
  %t32 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 2, i32 6, i32 0)
  ret i64 %t32

zr_aot_fn_2_fail:
  %t33 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t33
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
  %t2 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t3 = load ptr, ptr %t2, align 8
  %t4 = getelementptr i8, ptr %t3, i64 128
  %t5 = getelementptr i8, ptr %t4, i64 8
  %t6 = getelementptr i8, ptr %t4, i64 16
  %t7 = getelementptr i8, ptr %t4, i64 17
  %t8 = getelementptr i8, ptr %t4, i64 20
  %t9 = getelementptr i8, ptr %t4, i64 24
  %t10 = getelementptr i8, ptr %t4, i64 32
  store i32 1, ptr %t4, align 4
  store i64 1, ptr %t5, align 8
  store i8 0, ptr %t6, align 1
  store i8 1, ptr %t7, align 1
  store i32 0, ptr %t8, align 4
  store ptr null, ptr %t9, align 8
  store ptr null, ptr %t10, align 8
  br label %zr_aot_fn_3_ins_1

zr_aot_fn_3_ins_1:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t11, label %zr_aot_fn_3_ins_1_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_1_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr %state, ptr %frame, i32 3, i32 1, i32 0)
  br i1 %t12, label %zr_aot_fn_3_ins_2, label %zr_aot_fn_3_fail

zr_aot_fn_3_ins_2:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t13, label %zr_aot_fn_3_ins_2_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_2_body:
  %t14 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t15 = load ptr, ptr %t14, align 8
  %t16 = getelementptr i8, ptr %t15, i64 256
  %t17 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t17, label %zr_aot_fn_3_ins_3, label %zr_aot_fn_3_fail

zr_aot_fn_3_ins_3:
  %t18 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t18, label %zr_aot_fn_3_ins_3_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_3_body:
  %t19 = call i1 @ZrLibrary_AotRuntime_SetByIndex(ptr %state, ptr %frame, i32 2, i32 3, i32 4)
  br i1 %t19, label %zr_aot_fn_3_ins_4, label %zr_aot_fn_3_fail

zr_aot_fn_3_ins_4:
  %t20 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t20, label %zr_aot_fn_3_ins_4_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_4_body:
  %t21 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t22 = load ptr, ptr %t21, align 8
  %t23 = getelementptr i8, ptr %t22, i64 128
  %t24 = getelementptr i8, ptr %t23, i64 8
  %t25 = getelementptr i8, ptr %t23, i64 16
  %t26 = getelementptr i8, ptr %t23, i64 17
  %t27 = getelementptr i8, ptr %t23, i64 20
  %t28 = getelementptr i8, ptr %t23, i64 24
  %t29 = getelementptr i8, ptr %t23, i64 32
  store i32 0, ptr %t23, align 4
  store i64 0, ptr %t24, align 8
  store i8 0, ptr %t25, align 1
  store i8 1, ptr %t26, align 1
  store i32 0, ptr %t27, align 4
  store ptr null, ptr %t28, align 8
  store ptr null, ptr %t29, align 8
  br label %zr_aot_fn_3_ins_5

zr_aot_fn_3_ins_5:
  %t30 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 8)
  br i1 %t30, label %zr_aot_fn_3_ins_5_body, label %zr_aot_fn_3_fail
zr_aot_fn_3_ins_5_body:
  %t31 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 false)
  ret i64 %t31

zr_aot_fn_3_end_unsupported:
  %t32 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 3, i32 6, i32 0)
  ret i64 %t32

zr_aot_fn_3_fail:
  %t33 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t33
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
  %t2 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t3 = load ptr, ptr %t2, align 8
  %t4 = getelementptr i8, ptr %t3, i64 128
  %t5 = getelementptr i8, ptr %t4, i64 8
  %t6 = getelementptr i8, ptr %t4, i64 16
  %t7 = getelementptr i8, ptr %t4, i64 17
  %t8 = getelementptr i8, ptr %t4, i64 20
  %t9 = getelementptr i8, ptr %t4, i64 24
  %t10 = getelementptr i8, ptr %t4, i64 32
  store i32 1, ptr %t4, align 4
  store i64 1, ptr %t5, align 8
  store i8 0, ptr %t6, align 1
  store i8 1, ptr %t7, align 1
  store i32 0, ptr %t8, align 4
  store ptr null, ptr %t9, align 8
  store ptr null, ptr %t10, align 8
  br label %zr_aot_fn_4_ins_1

zr_aot_fn_4_ins_1:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t11, label %zr_aot_fn_4_ins_1_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_1_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr %state, ptr %frame, i32 3, i32 1, i32 0)
  br i1 %t12, label %zr_aot_fn_4_ins_2, label %zr_aot_fn_4_fail

zr_aot_fn_4_ins_2:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t13, label %zr_aot_fn_4_ins_2_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_2_body:
  %t14 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t15 = load ptr, ptr %t14, align 8
  %t16 = getelementptr i8, ptr %t15, i64 256
  %t17 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t17, label %zr_aot_fn_4_ins_3, label %zr_aot_fn_4_fail

zr_aot_fn_4_ins_3:
  %t18 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t18, label %zr_aot_fn_4_ins_3_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_3_body:
  %t19 = call i1 @ZrLibrary_AotRuntime_SetByIndex(ptr %state, ptr %frame, i32 2, i32 3, i32 4)
  br i1 %t19, label %zr_aot_fn_4_ins_4, label %zr_aot_fn_4_fail

zr_aot_fn_4_ins_4:
  %t20 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t20, label %zr_aot_fn_4_ins_4_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_4_body:
  %t21 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t22 = load ptr, ptr %t21, align 8
  %t23 = getelementptr i8, ptr %t22, i64 128
  %t24 = getelementptr i8, ptr %t23, i64 8
  %t25 = getelementptr i8, ptr %t23, i64 16
  %t26 = getelementptr i8, ptr %t23, i64 17
  %t27 = getelementptr i8, ptr %t23, i64 20
  %t28 = getelementptr i8, ptr %t23, i64 24
  %t29 = getelementptr i8, ptr %t23, i64 32
  store i32 0, ptr %t23, align 4
  store i64 0, ptr %t24, align 8
  store i8 0, ptr %t25, align 1
  store i8 1, ptr %t26, align 1
  store i32 0, ptr %t27, align 4
  store ptr null, ptr %t28, align 8
  store ptr null, ptr %t29, align 8
  br label %zr_aot_fn_4_ins_5

zr_aot_fn_4_ins_5:
  %t30 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 8)
  br i1 %t30, label %zr_aot_fn_4_ins_5_body, label %zr_aot_fn_4_fail
zr_aot_fn_4_ins_5_body:
  %t31 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 false)
  ret i64 %t31

zr_aot_fn_4_end_unsupported:
  %t32 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 4, i32 6, i32 0)
  ret i64 %t32

zr_aot_fn_4_fail:
  %t33 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t33
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
  %t2 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t3 = load ptr, ptr %t2, align 8
  %t4 = getelementptr i8, ptr %t3, i64 128
  %t5 = getelementptr i8, ptr %t4, i64 8
  %t6 = getelementptr i8, ptr %t4, i64 16
  %t7 = getelementptr i8, ptr %t4, i64 17
  %t8 = getelementptr i8, ptr %t4, i64 20
  %t9 = getelementptr i8, ptr %t4, i64 24
  %t10 = getelementptr i8, ptr %t4, i64 32
  store i32 1, ptr %t4, align 4
  store i64 1, ptr %t5, align 8
  store i8 0, ptr %t6, align 1
  store i8 1, ptr %t7, align 1
  store i32 0, ptr %t8, align 4
  store ptr null, ptr %t9, align 8
  store ptr null, ptr %t10, align 8
  br label %zr_aot_fn_5_ins_1

zr_aot_fn_5_ins_1:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t11, label %zr_aot_fn_5_ins_1_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_1_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr %state, ptr %frame, i32 3, i32 1, i32 0)
  br i1 %t12, label %zr_aot_fn_5_ins_2, label %zr_aot_fn_5_fail

zr_aot_fn_5_ins_2:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t13, label %zr_aot_fn_5_ins_2_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_2_body:
  %t14 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t15 = load ptr, ptr %t14, align 8
  %t16 = getelementptr i8, ptr %t15, i64 256
  %t17 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t17, label %zr_aot_fn_5_ins_3, label %zr_aot_fn_5_fail

zr_aot_fn_5_ins_3:
  %t18 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t18, label %zr_aot_fn_5_ins_3_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_3_body:
  %t19 = call i1 @ZrLibrary_AotRuntime_SetByIndex(ptr %state, ptr %frame, i32 2, i32 3, i32 4)
  br i1 %t19, label %zr_aot_fn_5_ins_4, label %zr_aot_fn_5_fail

zr_aot_fn_5_ins_4:
  %t20 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t20, label %zr_aot_fn_5_ins_4_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_4_body:
  %t21 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t22 = load ptr, ptr %t21, align 8
  %t23 = getelementptr i8, ptr %t22, i64 128
  %t24 = getelementptr i8, ptr %t23, i64 8
  %t25 = getelementptr i8, ptr %t23, i64 16
  %t26 = getelementptr i8, ptr %t23, i64 17
  %t27 = getelementptr i8, ptr %t23, i64 20
  %t28 = getelementptr i8, ptr %t23, i64 24
  %t29 = getelementptr i8, ptr %t23, i64 32
  store i32 0, ptr %t23, align 4
  store i64 0, ptr %t24, align 8
  store i8 0, ptr %t25, align 1
  store i8 1, ptr %t26, align 1
  store i32 0, ptr %t27, align 4
  store ptr null, ptr %t28, align 8
  store ptr null, ptr %t29, align 8
  br label %zr_aot_fn_5_ins_5

zr_aot_fn_5_ins_5:
  %t30 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 8)
  br i1 %t30, label %zr_aot_fn_5_ins_5_body, label %zr_aot_fn_5_fail
zr_aot_fn_5_ins_5_body:
  %t31 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 false)
  ret i64 %t31

zr_aot_fn_5_end_unsupported:
  %t32 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 5, i32 6, i32 0)
  ret i64 %t32

zr_aot_fn_5_fail:
  %t33 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t33
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
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 8)
  br i1 %t1, label %zr_aot_fn_6_ins_0_body, label %zr_aot_fn_6_fail
zr_aot_fn_6_ins_0_body:
  %t2 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 1, i1 false)
  ret i64 %t2

zr_aot_fn_6_end_unsupported:
  %t3 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 6, i32 1, i32 0)
  ret i64 %t3

zr_aot_fn_6_fail:
  %t4 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t4
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
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 1)
  br i1 %t1, label %zr_aot_fn_7_ins_0_body, label %zr_aot_fn_7_fail
zr_aot_fn_7_ins_0_body:
  %t2 = call i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr %state, ptr %frame, i32 1, i32 0, i32 0)
  br i1 %t2, label %zr_aot_fn_7_ins_1, label %zr_aot_fn_7_fail

zr_aot_fn_7_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 8)
  br i1 %t3, label %zr_aot_fn_7_ins_1_body, label %zr_aot_fn_7_fail
zr_aot_fn_7_ins_1_body:
  %t4 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 1, i1 false)
  ret i64 %t4

zr_aot_fn_7_end_unsupported:
  %t5 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 7, i32 2, i32 0)
  ret i64 %t5

zr_aot_fn_7_fail:
  %t6 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t6
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
  %t2 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 3)
  br i1 %t2, label %zr_aot_fn_8_ins_1, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t3, label %zr_aot_fn_8_ins_1_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 3, i32 3, i32 0)
  br i1 %t4, label %zr_aot_fn_8_ins_2, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 0)
  br i1 %t5, label %zr_aot_fn_8_ins_2_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_2_body:
  %t6 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t7 = load ptr, ptr %t6, align 8
  %t8 = getelementptr i8, ptr %t7, i64 128
  %t9 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t10 = load ptr, ptr %t9, align 8
  %t11 = getelementptr i8, ptr %t10, i64 192
  %t12 = getelementptr i8, ptr %t11, i64 20
  %t13 = load i32, ptr %t12, align 4
  %t14 = getelementptr i8, ptr %t8, i64 20
  %t15 = load i32, ptr %t14, align 4
  %t22 = load i32, ptr %t11, align 4
  %t23 = getelementptr i8, ptr %t11, i64 16
  %t24 = load i8, ptr %t23, align 1
  %t16 = icmp eq i32 %t13, 2
  %t17 = icmp eq i32 %t13, 1
  %t18 = icmp eq i32 %t13, 5
  %t19 = or i1 %t17, %t18
  %t20 = or i1 %t19, %t16
  br i1 %t20, label %zr_aot_stack_copy_transfer_33, label %zr_aot_stack_copy_weak_check_33
zr_aot_stack_copy_transfer_33:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t8)
  %t34 = load %SZrTypeValue, ptr %t11, align 32
  store %SZrTypeValue %t34, ptr %t8, align 32
  %t35 = getelementptr i8, ptr %t11, i64 8
  %t36 = getelementptr i8, ptr %t11, i64 16
  %t37 = getelementptr i8, ptr %t11, i64 17
  %t38 = getelementptr i8, ptr %t11, i64 20
  %t39 = getelementptr i8, ptr %t11, i64 24
  %t40 = getelementptr i8, ptr %t11, i64 32
  store i32 0, ptr %t11, align 4
  store i64 0, ptr %t35, align 8
  store i8 0, ptr %t36, align 1
  store i8 1, ptr %t37, align 1
  store i32 0, ptr %t38, align 4
  store ptr null, ptr %t39, align 8
  store ptr null, ptr %t40, align 8
  br label %zr_aot_fn_8_ins_3
zr_aot_stack_copy_weak_check_33:
  %t21 = icmp eq i32 %t13, 3
  br i1 %t21, label %zr_aot_stack_copy_weak_33, label %zr_aot_stack_copy_fast_check_33
zr_aot_stack_copy_weak_33:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t8, ptr %t11)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t11)
  br label %zr_aot_fn_8_ins_3
zr_aot_stack_copy_fast_check_33:
  %t25 = icmp ne i8 %t24, 0
  %t26 = icmp eq i32 %t22, 18
  %t27 = and i1 %t25, %t26
  %t28 = icmp eq i32 %t13, 0
  %t29 = icmp eq i32 %t15, 0
  %t30 = and i1 %t28, %t29
  %t31 = xor i1 %t27, true
  %t32 = and i1 %t30, %t31
  br i1 %t32, label %zr_aot_stack_copy_fast_33, label %zr_aot_stack_copy_slow_33
zr_aot_stack_copy_fast_33:
  %t41 = load %SZrTypeValue, ptr %t11, align 32
  store %SZrTypeValue %t41, ptr %t8, align 32
  br label %zr_aot_fn_8_ins_3
zr_aot_stack_copy_slow_33:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t8, ptr %t11)
  br label %zr_aot_fn_8_ins_3

zr_aot_fn_8_ins_3:
  %t42 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t42, label %zr_aot_fn_8_ins_3_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_3_body:
  %t43 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t43, label %zr_aot_fn_8_ins_4, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_4:
  %t44 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t44, label %zr_aot_fn_8_ins_4_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_4_body:
  %t45 = call i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr %state, ptr %frame, i32 2, i32 2, i32 0)
  br i1 %t45, label %zr_aot_fn_8_ins_5, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_5:
  %t46 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 0)
  br i1 %t46, label %zr_aot_fn_8_ins_5_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_5_body:
  %t47 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t48 = load ptr, ptr %t47, align 8
  %t49 = getelementptr i8, ptr %t48, i64 64
  %t50 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t51 = load ptr, ptr %t50, align 8
  %t52 = getelementptr i8, ptr %t51, i64 128
  %t53 = getelementptr i8, ptr %t52, i64 20
  %t54 = load i32, ptr %t53, align 4
  %t55 = getelementptr i8, ptr %t49, i64 20
  %t56 = load i32, ptr %t55, align 4
  %t63 = load i32, ptr %t52, align 4
  %t64 = getelementptr i8, ptr %t52, i64 16
  %t65 = load i8, ptr %t64, align 1
  %t57 = icmp eq i32 %t54, 2
  %t58 = icmp eq i32 %t54, 1
  %t59 = icmp eq i32 %t54, 5
  %t60 = or i1 %t58, %t59
  %t61 = or i1 %t60, %t57
  br i1 %t61, label %zr_aot_stack_copy_transfer_74, label %zr_aot_stack_copy_weak_check_74
zr_aot_stack_copy_transfer_74:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t49)
  %t75 = load %SZrTypeValue, ptr %t52, align 32
  store %SZrTypeValue %t75, ptr %t49, align 32
  %t76 = getelementptr i8, ptr %t52, i64 8
  %t77 = getelementptr i8, ptr %t52, i64 16
  %t78 = getelementptr i8, ptr %t52, i64 17
  %t79 = getelementptr i8, ptr %t52, i64 20
  %t80 = getelementptr i8, ptr %t52, i64 24
  %t81 = getelementptr i8, ptr %t52, i64 32
  store i32 0, ptr %t52, align 4
  store i64 0, ptr %t76, align 8
  store i8 0, ptr %t77, align 1
  store i8 1, ptr %t78, align 1
  store i32 0, ptr %t79, align 4
  store ptr null, ptr %t80, align 8
  store ptr null, ptr %t81, align 8
  br label %zr_aot_fn_8_ins_6
zr_aot_stack_copy_weak_check_74:
  %t62 = icmp eq i32 %t54, 3
  br i1 %t62, label %zr_aot_stack_copy_weak_74, label %zr_aot_stack_copy_fast_check_74
zr_aot_stack_copy_weak_74:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t49, ptr %t52)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t52)
  br label %zr_aot_fn_8_ins_6
zr_aot_stack_copy_fast_check_74:
  %t66 = icmp ne i8 %t65, 0
  %t67 = icmp eq i32 %t63, 18
  %t68 = and i1 %t66, %t67
  %t69 = icmp eq i32 %t54, 0
  %t70 = icmp eq i32 %t56, 0
  %t71 = and i1 %t69, %t70
  %t72 = xor i1 %t68, true
  %t73 = and i1 %t71, %t72
  br i1 %t73, label %zr_aot_stack_copy_fast_74, label %zr_aot_stack_copy_slow_74
zr_aot_stack_copy_fast_74:
  %t82 = load %SZrTypeValue, ptr %t52, align 32
  store %SZrTypeValue %t82, ptr %t49, align 32
  br label %zr_aot_fn_8_ins_6
zr_aot_stack_copy_slow_74:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t49, ptr %t52)
  br label %zr_aot_fn_8_ins_6

zr_aot_fn_8_ins_6:
  %t83 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 0)
  br i1 %t83, label %zr_aot_fn_8_ins_6_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_6_body:
  %t84 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 4)
  br i1 %t84, label %zr_aot_fn_8_ins_7, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_7:
  %t85 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 1)
  br i1 %t85, label %zr_aot_fn_8_ins_7_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_7_body:
  %t86 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 0)
  br i1 %t86, label %zr_aot_fn_8_ins_8, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_8:
  %t87 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 8, i32 0)
  br i1 %t87, label %zr_aot_fn_8_ins_8_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_8_body:
  %t88 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t89 = load ptr, ptr %t88, align 8
  %t90 = getelementptr i8, ptr %t89, i64 192
  %t91 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t92 = load ptr, ptr %t91, align 8
  %t93 = getelementptr i8, ptr %t92, i64 256
  %t94 = getelementptr i8, ptr %t93, i64 20
  %t95 = load i32, ptr %t94, align 4
  %t96 = getelementptr i8, ptr %t90, i64 20
  %t97 = load i32, ptr %t96, align 4
  %t104 = load i32, ptr %t93, align 4
  %t105 = getelementptr i8, ptr %t93, i64 16
  %t106 = load i8, ptr %t105, align 1
  %t98 = icmp eq i32 %t95, 2
  %t99 = icmp eq i32 %t95, 1
  %t100 = icmp eq i32 %t95, 5
  %t101 = or i1 %t99, %t100
  %t102 = or i1 %t101, %t98
  br i1 %t102, label %zr_aot_stack_copy_transfer_115, label %zr_aot_stack_copy_weak_check_115
zr_aot_stack_copy_transfer_115:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t90)
  %t116 = load %SZrTypeValue, ptr %t93, align 32
  store %SZrTypeValue %t116, ptr %t90, align 32
  %t117 = getelementptr i8, ptr %t93, i64 8
  %t118 = getelementptr i8, ptr %t93, i64 16
  %t119 = getelementptr i8, ptr %t93, i64 17
  %t120 = getelementptr i8, ptr %t93, i64 20
  %t121 = getelementptr i8, ptr %t93, i64 24
  %t122 = getelementptr i8, ptr %t93, i64 32
  store i32 0, ptr %t93, align 4
  store i64 0, ptr %t117, align 8
  store i8 0, ptr %t118, align 1
  store i8 1, ptr %t119, align 1
  store i32 0, ptr %t120, align 4
  store ptr null, ptr %t121, align 8
  store ptr null, ptr %t122, align 8
  br label %zr_aot_fn_8_ins_9
zr_aot_stack_copy_weak_check_115:
  %t103 = icmp eq i32 %t95, 3
  br i1 %t103, label %zr_aot_stack_copy_weak_115, label %zr_aot_stack_copy_fast_check_115
zr_aot_stack_copy_weak_115:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t90, ptr %t93)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t93)
  br label %zr_aot_fn_8_ins_9
zr_aot_stack_copy_fast_check_115:
  %t107 = icmp ne i8 %t106, 0
  %t108 = icmp eq i32 %t104, 18
  %t109 = and i1 %t107, %t108
  %t110 = icmp eq i32 %t95, 0
  %t111 = icmp eq i32 %t97, 0
  %t112 = and i1 %t110, %t111
  %t113 = xor i1 %t109, true
  %t114 = and i1 %t112, %t113
  br i1 %t114, label %zr_aot_stack_copy_fast_115, label %zr_aot_stack_copy_slow_115
zr_aot_stack_copy_fast_115:
  %t123 = load %SZrTypeValue, ptr %t93, align 32
  store %SZrTypeValue %t123, ptr %t90, align 32
  br label %zr_aot_fn_8_ins_9
zr_aot_stack_copy_slow_115:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t90, ptr %t93)
  br label %zr_aot_fn_8_ins_9

zr_aot_fn_8_ins_9:
  %t124 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 9, i32 1)
  br i1 %t124, label %zr_aot_fn_8_ins_9_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_9_body:
  %t125 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 3, i32 3)
  br i1 %t125, label %zr_aot_fn_8_ins_10, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_10:
  %t126 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 10, i32 1)
  br i1 %t126, label %zr_aot_fn_8_ins_10_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_10_body:
  %t127 = call i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr %state, ptr %frame, i32 3, i32 3, i32 1)
  br i1 %t127, label %zr_aot_fn_8_ins_11, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_11:
  %t128 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 11, i32 1)
  br i1 %t128, label %zr_aot_fn_8_ins_11_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_11_body:
  %t129 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 3, i32 3, i32 3)
  br i1 %t129, label %zr_aot_fn_8_ins_12, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_12:
  %t130 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 12, i32 1)
  br i1 %t130, label %zr_aot_fn_8_ins_12_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_12_body:
  %t131 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t132 = load ptr, ptr %t131, align 8
  %t133 = getelementptr i8, ptr %t132, i64 256
  %t134 = getelementptr i8, ptr %t133, i64 8
  %t135 = getelementptr i8, ptr %t133, i64 16
  %t136 = getelementptr i8, ptr %t133, i64 17
  %t137 = getelementptr i8, ptr %t133, i64 20
  %t138 = getelementptr i8, ptr %t133, i64 24
  %t139 = getelementptr i8, ptr %t133, i64 32
  store i32 5, ptr %t133, align 4
  store i64 0, ptr %t134, align 8
  store i8 0, ptr %t135, align 1
  store i8 1, ptr %t136, align 1
  store i32 0, ptr %t137, align 4
  store ptr null, ptr %t138, align 8
  store ptr null, ptr %t139, align 8
  br label %zr_aot_fn_8_ins_13

zr_aot_fn_8_ins_13:
  %t140 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 13, i32 1)
  br i1 %t140, label %zr_aot_fn_8_ins_13_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_13_body:
  %t141 = call i1 @ZrLibrary_AotRuntime_GetByIndex(ptr %state, ptr %frame, i32 3, i32 3, i32 4)
  br i1 %t141, label %zr_aot_fn_8_ins_14, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_14:
  %t142 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 14, i32 1)
  br i1 %t142, label %zr_aot_fn_8_ins_14_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_14_body:
  %t143 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 3, i32 3, i32 1)
  br i1 %t143, label %zr_aot_fn_8_ins_15, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_15:
  %t144 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 15, i32 0)
  br i1 %t144, label %zr_aot_fn_8_ins_15_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_15_body:
  %t145 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t146 = load ptr, ptr %t145, align 8
  %t147 = getelementptr i8, ptr %t146, i64 128
  %t148 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t149 = load ptr, ptr %t148, align 8
  %t150 = getelementptr i8, ptr %t149, i64 192
  %t151 = getelementptr i8, ptr %t150, i64 20
  %t152 = load i32, ptr %t151, align 4
  %t153 = getelementptr i8, ptr %t147, i64 20
  %t154 = load i32, ptr %t153, align 4
  %t161 = load i32, ptr %t150, align 4
  %t162 = getelementptr i8, ptr %t150, i64 16
  %t163 = load i8, ptr %t162, align 1
  %t155 = icmp eq i32 %t152, 2
  %t156 = icmp eq i32 %t152, 1
  %t157 = icmp eq i32 %t152, 5
  %t158 = or i1 %t156, %t157
  %t159 = or i1 %t158, %t155
  br i1 %t159, label %zr_aot_stack_copy_transfer_172, label %zr_aot_stack_copy_weak_check_172
zr_aot_stack_copy_transfer_172:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t147)
  %t173 = load %SZrTypeValue, ptr %t150, align 32
  store %SZrTypeValue %t173, ptr %t147, align 32
  %t174 = getelementptr i8, ptr %t150, i64 8
  %t175 = getelementptr i8, ptr %t150, i64 16
  %t176 = getelementptr i8, ptr %t150, i64 17
  %t177 = getelementptr i8, ptr %t150, i64 20
  %t178 = getelementptr i8, ptr %t150, i64 24
  %t179 = getelementptr i8, ptr %t150, i64 32
  store i32 0, ptr %t150, align 4
  store i64 0, ptr %t174, align 8
  store i8 0, ptr %t175, align 1
  store i8 1, ptr %t176, align 1
  store i32 0, ptr %t177, align 4
  store ptr null, ptr %t178, align 8
  store ptr null, ptr %t179, align 8
  br label %zr_aot_fn_8_ins_16
zr_aot_stack_copy_weak_check_172:
  %t160 = icmp eq i32 %t152, 3
  br i1 %t160, label %zr_aot_stack_copy_weak_172, label %zr_aot_stack_copy_fast_check_172
zr_aot_stack_copy_weak_172:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t147, ptr %t150)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t150)
  br label %zr_aot_fn_8_ins_16
zr_aot_stack_copy_fast_check_172:
  %t164 = icmp ne i8 %t163, 0
  %t165 = icmp eq i32 %t161, 18
  %t166 = and i1 %t164, %t165
  %t167 = icmp eq i32 %t152, 0
  %t168 = icmp eq i32 %t154, 0
  %t169 = and i1 %t167, %t168
  %t170 = xor i1 %t166, true
  %t171 = and i1 %t169, %t170
  br i1 %t171, label %zr_aot_stack_copy_fast_172, label %zr_aot_stack_copy_slow_172
zr_aot_stack_copy_fast_172:
  %t180 = load %SZrTypeValue, ptr %t150, align 32
  store %SZrTypeValue %t180, ptr %t147, align 32
  br label %zr_aot_fn_8_ins_16
zr_aot_stack_copy_slow_172:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t147, ptr %t150)
  br label %zr_aot_fn_8_ins_16

zr_aot_fn_8_ins_16:
  %t181 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 16, i32 0)
  br i1 %t181, label %zr_aot_fn_8_ins_16_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_16_body:
  %t182 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 5)
  br i1 %t182, label %zr_aot_fn_8_ins_17, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_17:
  %t183 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 17, i32 1)
  br i1 %t183, label %zr_aot_fn_8_ins_17_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_17_body:
  %t184 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 5, i32 5, i32 0)
  br i1 %t184, label %zr_aot_fn_8_ins_18, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_18:
  %t185 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 18, i32 0)
  br i1 %t185, label %zr_aot_fn_8_ins_18_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_18_body:
  %t186 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t187 = load ptr, ptr %t186, align 8
  %t188 = getelementptr i8, ptr %t187, i64 256
  %t189 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t190 = load ptr, ptr %t189, align 8
  %t191 = getelementptr i8, ptr %t190, i64 320
  %t192 = getelementptr i8, ptr %t191, i64 20
  %t193 = load i32, ptr %t192, align 4
  %t194 = getelementptr i8, ptr %t188, i64 20
  %t195 = load i32, ptr %t194, align 4
  %t202 = load i32, ptr %t191, align 4
  %t203 = getelementptr i8, ptr %t191, i64 16
  %t204 = load i8, ptr %t203, align 1
  %t196 = icmp eq i32 %t193, 2
  %t197 = icmp eq i32 %t193, 1
  %t198 = icmp eq i32 %t193, 5
  %t199 = or i1 %t197, %t198
  %t200 = or i1 %t199, %t196
  br i1 %t200, label %zr_aot_stack_copy_transfer_213, label %zr_aot_stack_copy_weak_check_213
zr_aot_stack_copy_transfer_213:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t188)
  %t214 = load %SZrTypeValue, ptr %t191, align 32
  store %SZrTypeValue %t214, ptr %t188, align 32
  %t215 = getelementptr i8, ptr %t191, i64 8
  %t216 = getelementptr i8, ptr %t191, i64 16
  %t217 = getelementptr i8, ptr %t191, i64 17
  %t218 = getelementptr i8, ptr %t191, i64 20
  %t219 = getelementptr i8, ptr %t191, i64 24
  %t220 = getelementptr i8, ptr %t191, i64 32
  store i32 0, ptr %t191, align 4
  store i64 0, ptr %t215, align 8
  store i8 0, ptr %t216, align 1
  store i8 1, ptr %t217, align 1
  store i32 0, ptr %t218, align 4
  store ptr null, ptr %t219, align 8
  store ptr null, ptr %t220, align 8
  br label %zr_aot_fn_8_ins_19
zr_aot_stack_copy_weak_check_213:
  %t201 = icmp eq i32 %t193, 3
  br i1 %t201, label %zr_aot_stack_copy_weak_213, label %zr_aot_stack_copy_fast_check_213
zr_aot_stack_copy_weak_213:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t188, ptr %t191)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t191)
  br label %zr_aot_fn_8_ins_19
zr_aot_stack_copy_fast_check_213:
  %t205 = icmp ne i8 %t204, 0
  %t206 = icmp eq i32 %t202, 18
  %t207 = and i1 %t205, %t206
  %t208 = icmp eq i32 %t193, 0
  %t209 = icmp eq i32 %t195, 0
  %t210 = and i1 %t208, %t209
  %t211 = xor i1 %t207, true
  %t212 = and i1 %t210, %t211
  br i1 %t212, label %zr_aot_stack_copy_fast_213, label %zr_aot_stack_copy_slow_213
zr_aot_stack_copy_fast_213:
  %t221 = load %SZrTypeValue, ptr %t191, align 32
  store %SZrTypeValue %t221, ptr %t188, align 32
  br label %zr_aot_fn_8_ins_19
zr_aot_stack_copy_slow_213:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t188, ptr %t191)
  br label %zr_aot_fn_8_ins_19

zr_aot_fn_8_ins_19:
  %t222 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 19, i32 1)
  br i1 %t222, label %zr_aot_fn_8_ins_19_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_19_body:
  %t223 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 4, i32 4)
  br i1 %t223, label %zr_aot_fn_8_ins_20, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_20:
  %t224 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 20, i32 1)
  br i1 %t224, label %zr_aot_fn_8_ins_20_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_20_body:
  %t225 = call i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr %state, ptr %frame, i32 4, i32 4, i32 2)
  br i1 %t225, label %zr_aot_fn_8_ins_21, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_21:
  %t226 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 21, i32 1)
  br i1 %t226, label %zr_aot_fn_8_ins_21_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_21_body:
  %t227 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 4)
  br i1 %t227, label %zr_aot_fn_8_ins_22, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_22:
  %t228 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 22, i32 1)
  br i1 %t228, label %zr_aot_fn_8_ins_22_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_22_body:
  %t229 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t230 = load ptr, ptr %t229, align 8
  %t231 = getelementptr i8, ptr %t230, i64 320
  %t232 = getelementptr i8, ptr %t231, i64 8
  %t233 = getelementptr i8, ptr %t231, i64 16
  %t234 = getelementptr i8, ptr %t231, i64 17
  %t235 = getelementptr i8, ptr %t231, i64 20
  %t236 = getelementptr i8, ptr %t231, i64 24
  %t237 = getelementptr i8, ptr %t231, i64 32
  store i32 5, ptr %t231, align 4
  store i64 0, ptr %t232, align 8
  store i8 0, ptr %t233, align 1
  store i8 1, ptr %t234, align 1
  store i32 0, ptr %t235, align 4
  store ptr null, ptr %t236, align 8
  store ptr null, ptr %t237, align 8
  br label %zr_aot_fn_8_ins_23

zr_aot_fn_8_ins_23:
  %t238 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 23, i32 1)
  br i1 %t238, label %zr_aot_fn_8_ins_23_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_23_body:
  %t239 = call i1 @ZrLibrary_AotRuntime_GetByIndex(ptr %state, ptr %frame, i32 4, i32 4, i32 5)
  br i1 %t239, label %zr_aot_fn_8_ins_24, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_24:
  %t240 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 24, i32 1)
  br i1 %t240, label %zr_aot_fn_8_ins_24_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_24_body:
  %t241 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 1)
  br i1 %t241, label %zr_aot_fn_8_ins_25, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_25:
  %t242 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 25, i32 0)
  br i1 %t242, label %zr_aot_fn_8_ins_25_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_25_body:
  %t243 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t244 = load ptr, ptr %t243, align 8
  %t245 = getelementptr i8, ptr %t244, i64 192
  %t246 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t247 = load ptr, ptr %t246, align 8
  %t248 = getelementptr i8, ptr %t247, i64 256
  %t249 = getelementptr i8, ptr %t248, i64 20
  %t250 = load i32, ptr %t249, align 4
  %t251 = getelementptr i8, ptr %t245, i64 20
  %t252 = load i32, ptr %t251, align 4
  %t259 = load i32, ptr %t248, align 4
  %t260 = getelementptr i8, ptr %t248, i64 16
  %t261 = load i8, ptr %t260, align 1
  %t253 = icmp eq i32 %t250, 2
  %t254 = icmp eq i32 %t250, 1
  %t255 = icmp eq i32 %t250, 5
  %t256 = or i1 %t254, %t255
  %t257 = or i1 %t256, %t253
  br i1 %t257, label %zr_aot_stack_copy_transfer_270, label %zr_aot_stack_copy_weak_check_270
zr_aot_stack_copy_transfer_270:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t245)
  %t271 = load %SZrTypeValue, ptr %t248, align 32
  store %SZrTypeValue %t271, ptr %t245, align 32
  %t272 = getelementptr i8, ptr %t248, i64 8
  %t273 = getelementptr i8, ptr %t248, i64 16
  %t274 = getelementptr i8, ptr %t248, i64 17
  %t275 = getelementptr i8, ptr %t248, i64 20
  %t276 = getelementptr i8, ptr %t248, i64 24
  %t277 = getelementptr i8, ptr %t248, i64 32
  store i32 0, ptr %t248, align 4
  store i64 0, ptr %t272, align 8
  store i8 0, ptr %t273, align 1
  store i8 1, ptr %t274, align 1
  store i32 0, ptr %t275, align 4
  store ptr null, ptr %t276, align 8
  store ptr null, ptr %t277, align 8
  br label %zr_aot_fn_8_ins_26
zr_aot_stack_copy_weak_check_270:
  %t258 = icmp eq i32 %t250, 3
  br i1 %t258, label %zr_aot_stack_copy_weak_270, label %zr_aot_stack_copy_fast_check_270
zr_aot_stack_copy_weak_270:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t245, ptr %t248)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t248)
  br label %zr_aot_fn_8_ins_26
zr_aot_stack_copy_fast_check_270:
  %t262 = icmp ne i8 %t261, 0
  %t263 = icmp eq i32 %t259, 18
  %t264 = and i1 %t262, %t263
  %t265 = icmp eq i32 %t250, 0
  %t266 = icmp eq i32 %t252, 0
  %t267 = and i1 %t265, %t266
  %t268 = xor i1 %t264, true
  %t269 = and i1 %t267, %t268
  br i1 %t269, label %zr_aot_stack_copy_fast_270, label %zr_aot_stack_copy_slow_270
zr_aot_stack_copy_fast_270:
  %t278 = load %SZrTypeValue, ptr %t248, align 32
  store %SZrTypeValue %t278, ptr %t245, align 32
  br label %zr_aot_fn_8_ins_26
zr_aot_stack_copy_slow_270:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t245, ptr %t248)
  br label %zr_aot_fn_8_ins_26

zr_aot_fn_8_ins_26:
  %t279 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 26, i32 0)
  br i1 %t279, label %zr_aot_fn_8_ins_26_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_26_body:
  %t280 = call i1 @ZrLibrary_AotRuntime_GetGlobal(ptr %state, ptr %frame, i32 6)
  br i1 %t280, label %zr_aot_fn_8_ins_27, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_27:
  %t281 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 27, i32 1)
  br i1 %t281, label %zr_aot_fn_8_ins_27_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_27_body:
  %t282 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 6, i32 6, i32 0)
  br i1 %t282, label %zr_aot_fn_8_ins_28, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_28:
  %t283 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 28, i32 0)
  br i1 %t283, label %zr_aot_fn_8_ins_28_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_28_body:
  %t284 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t285 = load ptr, ptr %t284, align 8
  %t286 = getelementptr i8, ptr %t285, i64 320
  %t287 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t288 = load ptr, ptr %t287, align 8
  %t289 = getelementptr i8, ptr %t288, i64 384
  %t290 = getelementptr i8, ptr %t289, i64 20
  %t291 = load i32, ptr %t290, align 4
  %t292 = getelementptr i8, ptr %t286, i64 20
  %t293 = load i32, ptr %t292, align 4
  %t300 = load i32, ptr %t289, align 4
  %t301 = getelementptr i8, ptr %t289, i64 16
  %t302 = load i8, ptr %t301, align 1
  %t294 = icmp eq i32 %t291, 2
  %t295 = icmp eq i32 %t291, 1
  %t296 = icmp eq i32 %t291, 5
  %t297 = or i1 %t295, %t296
  %t298 = or i1 %t297, %t294
  br i1 %t298, label %zr_aot_stack_copy_transfer_311, label %zr_aot_stack_copy_weak_check_311
zr_aot_stack_copy_transfer_311:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t286)
  %t312 = load %SZrTypeValue, ptr %t289, align 32
  store %SZrTypeValue %t312, ptr %t286, align 32
  %t313 = getelementptr i8, ptr %t289, i64 8
  %t314 = getelementptr i8, ptr %t289, i64 16
  %t315 = getelementptr i8, ptr %t289, i64 17
  %t316 = getelementptr i8, ptr %t289, i64 20
  %t317 = getelementptr i8, ptr %t289, i64 24
  %t318 = getelementptr i8, ptr %t289, i64 32
  store i32 0, ptr %t289, align 4
  store i64 0, ptr %t313, align 8
  store i8 0, ptr %t314, align 1
  store i8 1, ptr %t315, align 1
  store i32 0, ptr %t316, align 4
  store ptr null, ptr %t317, align 8
  store ptr null, ptr %t318, align 8
  br label %zr_aot_fn_8_ins_29
zr_aot_stack_copy_weak_check_311:
  %t299 = icmp eq i32 %t291, 3
  br i1 %t299, label %zr_aot_stack_copy_weak_311, label %zr_aot_stack_copy_fast_check_311
zr_aot_stack_copy_weak_311:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t286, ptr %t289)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t289)
  br label %zr_aot_fn_8_ins_29
zr_aot_stack_copy_fast_check_311:
  %t303 = icmp ne i8 %t302, 0
  %t304 = icmp eq i32 %t300, 18
  %t305 = and i1 %t303, %t304
  %t306 = icmp eq i32 %t291, 0
  %t307 = icmp eq i32 %t293, 0
  %t308 = and i1 %t306, %t307
  %t309 = xor i1 %t305, true
  %t310 = and i1 %t308, %t309
  br i1 %t310, label %zr_aot_stack_copy_fast_311, label %zr_aot_stack_copy_slow_311
zr_aot_stack_copy_fast_311:
  %t319 = load %SZrTypeValue, ptr %t289, align 32
  store %SZrTypeValue %t319, ptr %t286, align 32
  br label %zr_aot_fn_8_ins_29
zr_aot_stack_copy_slow_311:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t286, ptr %t289)
  br label %zr_aot_fn_8_ins_29

zr_aot_fn_8_ins_29:
  %t320 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 29, i32 1)
  br i1 %t320, label %zr_aot_fn_8_ins_29_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_29_body:
  %t321 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 5, i32 5)
  br i1 %t321, label %zr_aot_fn_8_ins_30, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_30:
  %t322 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 30, i32 1)
  br i1 %t322, label %zr_aot_fn_8_ins_30_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_30_body:
  %t323 = call i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr %state, ptr %frame, i32 5, i32 5, i32 3)
  br i1 %t323, label %zr_aot_fn_8_ins_31, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_31:
  %t324 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 31, i32 1)
  br i1 %t324, label %zr_aot_fn_8_ins_31_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_31_body:
  %t325 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 5, i32 5, i32 5)
  br i1 %t325, label %zr_aot_fn_8_ins_32, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_32:
  %t326 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 32, i32 1)
  br i1 %t326, label %zr_aot_fn_8_ins_32_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_32_body:
  %t327 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t328 = load ptr, ptr %t327, align 8
  %t329 = getelementptr i8, ptr %t328, i64 384
  %t330 = getelementptr i8, ptr %t329, i64 8
  %t331 = getelementptr i8, ptr %t329, i64 16
  %t332 = getelementptr i8, ptr %t329, i64 17
  %t333 = getelementptr i8, ptr %t329, i64 20
  %t334 = getelementptr i8, ptr %t329, i64 24
  %t335 = getelementptr i8, ptr %t329, i64 32
  store i32 5, ptr %t329, align 4
  store i64 0, ptr %t330, align 8
  store i8 0, ptr %t331, align 1
  store i8 1, ptr %t332, align 1
  store i32 0, ptr %t333, align 4
  store ptr null, ptr %t334, align 8
  store ptr null, ptr %t335, align 8
  br label %zr_aot_fn_8_ins_33

zr_aot_fn_8_ins_33:
  %t336 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 33, i32 1)
  br i1 %t336, label %zr_aot_fn_8_ins_33_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_33_body:
  %t337 = call i1 @ZrLibrary_AotRuntime_GetByIndex(ptr %state, ptr %frame, i32 5, i32 5, i32 6)
  br i1 %t337, label %zr_aot_fn_8_ins_34, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_34:
  %t338 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 34, i32 1)
  br i1 %t338, label %zr_aot_fn_8_ins_34_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_34_body:
  %t339 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 5, i32 5, i32 1)
  br i1 %t339, label %zr_aot_fn_8_ins_35, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_35:
  %t340 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 35, i32 0)
  br i1 %t340, label %zr_aot_fn_8_ins_35_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_35_body:
  %t341 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t342 = load ptr, ptr %t341, align 8
  %t343 = getelementptr i8, ptr %t342, i64 256
  %t344 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t345 = load ptr, ptr %t344, align 8
  %t346 = getelementptr i8, ptr %t345, i64 320
  %t347 = getelementptr i8, ptr %t346, i64 20
  %t348 = load i32, ptr %t347, align 4
  %t349 = getelementptr i8, ptr %t343, i64 20
  %t350 = load i32, ptr %t349, align 4
  %t357 = load i32, ptr %t346, align 4
  %t358 = getelementptr i8, ptr %t346, i64 16
  %t359 = load i8, ptr %t358, align 1
  %t351 = icmp eq i32 %t348, 2
  %t352 = icmp eq i32 %t348, 1
  %t353 = icmp eq i32 %t348, 5
  %t354 = or i1 %t352, %t353
  %t355 = or i1 %t354, %t351
  br i1 %t355, label %zr_aot_stack_copy_transfer_368, label %zr_aot_stack_copy_weak_check_368
zr_aot_stack_copy_transfer_368:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t343)
  %t369 = load %SZrTypeValue, ptr %t346, align 32
  store %SZrTypeValue %t369, ptr %t343, align 32
  %t370 = getelementptr i8, ptr %t346, i64 8
  %t371 = getelementptr i8, ptr %t346, i64 16
  %t372 = getelementptr i8, ptr %t346, i64 17
  %t373 = getelementptr i8, ptr %t346, i64 20
  %t374 = getelementptr i8, ptr %t346, i64 24
  %t375 = getelementptr i8, ptr %t346, i64 32
  store i32 0, ptr %t346, align 4
  store i64 0, ptr %t370, align 8
  store i8 0, ptr %t371, align 1
  store i8 1, ptr %t372, align 1
  store i32 0, ptr %t373, align 4
  store ptr null, ptr %t374, align 8
  store ptr null, ptr %t375, align 8
  br label %zr_aot_fn_8_ins_36
zr_aot_stack_copy_weak_check_368:
  %t356 = icmp eq i32 %t348, 3
  br i1 %t356, label %zr_aot_stack_copy_weak_368, label %zr_aot_stack_copy_fast_check_368
zr_aot_stack_copy_weak_368:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t343, ptr %t346)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t346)
  br label %zr_aot_fn_8_ins_36
zr_aot_stack_copy_fast_check_368:
  %t360 = icmp ne i8 %t359, 0
  %t361 = icmp eq i32 %t357, 18
  %t362 = and i1 %t360, %t361
  %t363 = icmp eq i32 %t348, 0
  %t364 = icmp eq i32 %t350, 0
  %t365 = and i1 %t363, %t364
  %t366 = xor i1 %t362, true
  %t367 = and i1 %t365, %t366
  br i1 %t367, label %zr_aot_stack_copy_fast_368, label %zr_aot_stack_copy_slow_368
zr_aot_stack_copy_fast_368:
  %t376 = load %SZrTypeValue, ptr %t346, align 32
  store %SZrTypeValue %t376, ptr %t343, align 32
  br label %zr_aot_fn_8_ins_36
zr_aot_stack_copy_slow_368:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t343, ptr %t346)
  br label %zr_aot_fn_8_ins_36

zr_aot_fn_8_ins_36:
  %t377 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 36, i32 1)
  br i1 %t377, label %zr_aot_fn_8_ins_36_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_36_body:
  %t378 = call i1 @ZrLibrary_AotRuntime_GetClosureValue(ptr %state, ptr %frame, i32 7, i32 0)
  br i1 %t378, label %zr_aot_fn_8_ins_37, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_37:
  %t379 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 37, i32 0)
  br i1 %t379, label %zr_aot_fn_8_ins_37_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_37_body:
  %t380 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t381 = load ptr, ptr %t380, align 8
  %t382 = getelementptr i8, ptr %t381, i64 384
  %t383 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t384 = load ptr, ptr %t383, align 8
  %t385 = getelementptr i8, ptr %t384, i64 448
  %t386 = getelementptr i8, ptr %t385, i64 20
  %t387 = load i32, ptr %t386, align 4
  %t388 = getelementptr i8, ptr %t382, i64 20
  %t389 = load i32, ptr %t388, align 4
  %t396 = load i32, ptr %t385, align 4
  %t397 = getelementptr i8, ptr %t385, i64 16
  %t398 = load i8, ptr %t397, align 1
  %t390 = icmp eq i32 %t387, 2
  %t391 = icmp eq i32 %t387, 1
  %t392 = icmp eq i32 %t387, 5
  %t393 = or i1 %t391, %t392
  %t394 = or i1 %t393, %t390
  br i1 %t394, label %zr_aot_stack_copy_transfer_407, label %zr_aot_stack_copy_weak_check_407
zr_aot_stack_copy_transfer_407:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t382)
  %t408 = load %SZrTypeValue, ptr %t385, align 32
  store %SZrTypeValue %t408, ptr %t382, align 32
  %t409 = getelementptr i8, ptr %t385, i64 8
  %t410 = getelementptr i8, ptr %t385, i64 16
  %t411 = getelementptr i8, ptr %t385, i64 17
  %t412 = getelementptr i8, ptr %t385, i64 20
  %t413 = getelementptr i8, ptr %t385, i64 24
  %t414 = getelementptr i8, ptr %t385, i64 32
  store i32 0, ptr %t385, align 4
  store i64 0, ptr %t409, align 8
  store i8 0, ptr %t410, align 1
  store i8 1, ptr %t411, align 1
  store i32 0, ptr %t412, align 4
  store ptr null, ptr %t413, align 8
  store ptr null, ptr %t414, align 8
  br label %zr_aot_fn_8_ins_38
zr_aot_stack_copy_weak_check_407:
  %t395 = icmp eq i32 %t387, 3
  br i1 %t395, label %zr_aot_stack_copy_weak_407, label %zr_aot_stack_copy_fast_check_407
zr_aot_stack_copy_weak_407:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t382, ptr %t385)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t385)
  br label %zr_aot_fn_8_ins_38
zr_aot_stack_copy_fast_check_407:
  %t399 = icmp ne i8 %t398, 0
  %t400 = icmp eq i32 %t396, 18
  %t401 = and i1 %t399, %t400
  %t402 = icmp eq i32 %t387, 0
  %t403 = icmp eq i32 %t389, 0
  %t404 = and i1 %t402, %t403
  %t405 = xor i1 %t401, true
  %t406 = and i1 %t404, %t405
  br i1 %t406, label %zr_aot_stack_copy_fast_407, label %zr_aot_stack_copy_slow_407
zr_aot_stack_copy_fast_407:
  %t415 = load %SZrTypeValue, ptr %t385, align 32
  store %SZrTypeValue %t415, ptr %t382, align 32
  br label %zr_aot_fn_8_ins_38
zr_aot_stack_copy_slow_407:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t382, ptr %t385)
  br label %zr_aot_fn_8_ins_38

zr_aot_fn_8_ins_38:
  %t416 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 38, i32 1)
  br i1 %t416, label %zr_aot_fn_8_ins_38_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_38_body:
  %t417 = call i1 @ZrLibrary_AotRuntime_TypeOf(ptr %state, ptr %frame, i32 6, i32 6)
  br i1 %t417, label %zr_aot_fn_8_ins_39, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_39:
  %t418 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 39, i32 1)
  br i1 %t418, label %zr_aot_fn_8_ins_39_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_39_body:
  %t419 = call i1 @ZrLibrary_AotRuntime_GetMemberSlot(ptr %state, ptr %frame, i32 6, i32 6, i32 4)
  br i1 %t419, label %zr_aot_fn_8_ins_40, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_40:
  %t420 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 40, i32 0)
  br i1 %t420, label %zr_aot_fn_8_ins_40_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_40_body:
  %t421 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t422 = load ptr, ptr %t421, align 8
  %t423 = getelementptr i8, ptr %t422, i64 320
  %t424 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t425 = load ptr, ptr %t424, align 8
  %t426 = getelementptr i8, ptr %t425, i64 384
  %t427 = getelementptr i8, ptr %t426, i64 20
  %t428 = load i32, ptr %t427, align 4
  %t429 = getelementptr i8, ptr %t423, i64 20
  %t430 = load i32, ptr %t429, align 4
  %t437 = load i32, ptr %t426, align 4
  %t438 = getelementptr i8, ptr %t426, i64 16
  %t439 = load i8, ptr %t438, align 1
  %t431 = icmp eq i32 %t428, 2
  %t432 = icmp eq i32 %t428, 1
  %t433 = icmp eq i32 %t428, 5
  %t434 = or i1 %t432, %t433
  %t435 = or i1 %t434, %t431
  br i1 %t435, label %zr_aot_stack_copy_transfer_448, label %zr_aot_stack_copy_weak_check_448
zr_aot_stack_copy_transfer_448:
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t423)
  %t449 = load %SZrTypeValue, ptr %t426, align 32
  store %SZrTypeValue %t449, ptr %t423, align 32
  %t450 = getelementptr i8, ptr %t426, i64 8
  %t451 = getelementptr i8, ptr %t426, i64 16
  %t452 = getelementptr i8, ptr %t426, i64 17
  %t453 = getelementptr i8, ptr %t426, i64 20
  %t454 = getelementptr i8, ptr %t426, i64 24
  %t455 = getelementptr i8, ptr %t426, i64 32
  store i32 0, ptr %t426, align 4
  store i64 0, ptr %t450, align 8
  store i8 0, ptr %t451, align 1
  store i8 1, ptr %t452, align 1
  store i32 0, ptr %t453, align 4
  store ptr null, ptr %t454, align 8
  store ptr null, ptr %t455, align 8
  br label %zr_aot_fn_8_ins_41
zr_aot_stack_copy_weak_check_448:
  %t436 = icmp eq i32 %t428, 3
  br i1 %t436, label %zr_aot_stack_copy_weak_448, label %zr_aot_stack_copy_fast_check_448
zr_aot_stack_copy_weak_448:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t423, ptr %t426)
  call void @ZrCore_Ownership_ReleaseValue(ptr %state, ptr %t426)
  br label %zr_aot_fn_8_ins_41
zr_aot_stack_copy_fast_check_448:
  %t440 = icmp ne i8 %t439, 0
  %t441 = icmp eq i32 %t437, 18
  %t442 = and i1 %t440, %t441
  %t443 = icmp eq i32 %t428, 0
  %t444 = icmp eq i32 %t430, 0
  %t445 = and i1 %t443, %t444
  %t446 = xor i1 %t442, true
  %t447 = and i1 %t445, %t446
  br i1 %t447, label %zr_aot_stack_copy_fast_448, label %zr_aot_stack_copy_slow_448
zr_aot_stack_copy_fast_448:
  %t456 = load %SZrTypeValue, ptr %t426, align 32
  store %SZrTypeValue %t456, ptr %t423, align 32
  br label %zr_aot_fn_8_ins_41
zr_aot_stack_copy_slow_448:
  call void @ZrCore_Value_CopySlow(ptr %state, ptr %t423, ptr %t426)
  br label %zr_aot_fn_8_ins_41

zr_aot_fn_8_ins_41:
  %t457 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 41, i32 1)
  br i1 %t457, label %zr_aot_fn_8_ins_41_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_41_body:
  %t458 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 6, i32 1, i32 6)
  br i1 %t458, label %zr_aot_fn_8_ins_42, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_42:
  %t459 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 42, i32 2)
  br i1 %t459, label %zr_aot_fn_8_ins_42_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_42_body:
  %t460 = call i1 @ZrLibrary_AotRuntime_IsTruthy(ptr %state, ptr %frame, i32 6, ptr %truthy_value)
  br i1 %t460, label %zr_aot_fn_8_ins_42_truthy, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_42_truthy:
  %t461 = load i8, ptr %truthy_value, align 1
  %t462 = icmp eq i8 %t461, 0
  br i1 %t462, label %zr_aot_fn_8_ins_45, label %zr_aot_fn_8_ins_43

zr_aot_fn_8_ins_43:
  %t463 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 43, i32 0)
  br i1 %t463, label %zr_aot_fn_8_ins_43_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_43_body:
  %t464 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t465 = load ptr, ptr %t464, align 8
  %t466 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t467 = load ptr, ptr %t466, align 8
  %t468 = load i32, ptr %t467, align 4
  %t469 = getelementptr i8, ptr %t467, i64 8
  %t470 = load i64, ptr %t469, align 8
  %t471 = icmp uge i32 %t468, 2
  %t472 = icmp ule i32 %t468, 5
  %t473 = and i1 %t471, %t472
  br i1 %t473, label %zr_aot_add_int_const_fast_474, label %zr_aot_fn_8_fail
zr_aot_add_int_const_fast_474:
  %t475 = add i64 %t470, 1
  %t476 = getelementptr i8, ptr %t465, i64 8
  %t477 = getelementptr i8, ptr %t465, i64 16
  %t478 = getelementptr i8, ptr %t465, i64 17
  %t479 = getelementptr i8, ptr %t465, i64 20
  %t480 = getelementptr i8, ptr %t465, i64 24
  %t481 = getelementptr i8, ptr %t465, i64 32
  store i32 5, ptr %t465, align 4
  store i64 %t475, ptr %t476, align 8
  store i8 0, ptr %t477, align 1
  store i8 1, ptr %t478, align 1
  store i32 0, ptr %t479, align 4
  store ptr null, ptr %t480, align 8
  store ptr null, ptr %t481, align 8
  br label %zr_aot_fn_8_ins_44

zr_aot_fn_8_ins_44:
  %t482 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 44, i32 2)
  br i1 %t482, label %zr_aot_fn_8_ins_44_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_44_body:
  br label %zr_aot_fn_8_ins_45

zr_aot_fn_8_ins_45:
  %t483 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 45, i32 1)
  br i1 %t483, label %zr_aot_fn_8_ins_45_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_45_body:
  %t484 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 7, i32 2, i32 7)
  br i1 %t484, label %zr_aot_fn_8_ins_46, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_46:
  %t485 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 46, i32 2)
  br i1 %t485, label %zr_aot_fn_8_ins_46_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_46_body:
  %t486 = call i1 @ZrLibrary_AotRuntime_IsTruthy(ptr %state, ptr %frame, i32 7, ptr %truthy_value)
  br i1 %t486, label %zr_aot_fn_8_ins_46_truthy, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_46_truthy:
  %t487 = load i8, ptr %truthy_value, align 1
  %t488 = icmp eq i8 %t487, 0
  br i1 %t488, label %zr_aot_fn_8_ins_49, label %zr_aot_fn_8_ins_47

zr_aot_fn_8_ins_47:
  %t489 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 47, i32 0)
  br i1 %t489, label %zr_aot_fn_8_ins_47_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_47_body:
  %t490 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t491 = load ptr, ptr %t490, align 8
  %t492 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t493 = load ptr, ptr %t492, align 8
  %t494 = load i32, ptr %t493, align 4
  %t495 = getelementptr i8, ptr %t493, i64 8
  %t496 = load i64, ptr %t495, align 8
  %t497 = icmp uge i32 %t494, 2
  %t498 = icmp ule i32 %t494, 5
  %t499 = and i1 %t497, %t498
  br i1 %t499, label %zr_aot_add_int_const_fast_500, label %zr_aot_fn_8_fail
zr_aot_add_int_const_fast_500:
  %t501 = add i64 %t496, 2
  %t502 = getelementptr i8, ptr %t491, i64 8
  %t503 = getelementptr i8, ptr %t491, i64 16
  %t504 = getelementptr i8, ptr %t491, i64 17
  %t505 = getelementptr i8, ptr %t491, i64 20
  %t506 = getelementptr i8, ptr %t491, i64 24
  %t507 = getelementptr i8, ptr %t491, i64 32
  store i32 5, ptr %t491, align 4
  store i64 %t501, ptr %t502, align 8
  store i8 0, ptr %t503, align 1
  store i8 1, ptr %t504, align 1
  store i32 0, ptr %t505, align 4
  store ptr null, ptr %t506, align 8
  store ptr null, ptr %t507, align 8
  br label %zr_aot_fn_8_ins_48

zr_aot_fn_8_ins_48:
  %t508 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 48, i32 2)
  br i1 %t508, label %zr_aot_fn_8_ins_48_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_48_body:
  br label %zr_aot_fn_8_ins_49

zr_aot_fn_8_ins_49:
  %t509 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 49, i32 1)
  br i1 %t509, label %zr_aot_fn_8_ins_49_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_49_body:
  %t510 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 8, i32 3, i32 8)
  br i1 %t510, label %zr_aot_fn_8_ins_50, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_50:
  %t511 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 50, i32 2)
  br i1 %t511, label %zr_aot_fn_8_ins_50_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_50_body:
  %t512 = call i1 @ZrLibrary_AotRuntime_IsTruthy(ptr %state, ptr %frame, i32 8, ptr %truthy_value)
  br i1 %t512, label %zr_aot_fn_8_ins_50_truthy, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_50_truthy:
  %t513 = load i8, ptr %truthy_value, align 1
  %t514 = icmp eq i8 %t513, 0
  br i1 %t514, label %zr_aot_fn_8_ins_53, label %zr_aot_fn_8_ins_51

zr_aot_fn_8_ins_51:
  %t515 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 51, i32 0)
  br i1 %t515, label %zr_aot_fn_8_ins_51_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_51_body:
  %t516 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t517 = load ptr, ptr %t516, align 8
  %t518 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t519 = load ptr, ptr %t518, align 8
  %t520 = load i32, ptr %t519, align 4
  %t521 = getelementptr i8, ptr %t519, i64 8
  %t522 = load i64, ptr %t521, align 8
  %t523 = icmp uge i32 %t520, 2
  %t524 = icmp ule i32 %t520, 5
  %t525 = and i1 %t523, %t524
  br i1 %t525, label %zr_aot_add_int_const_fast_526, label %zr_aot_fn_8_fail
zr_aot_add_int_const_fast_526:
  %t527 = add i64 %t522, 4
  %t528 = getelementptr i8, ptr %t517, i64 8
  %t529 = getelementptr i8, ptr %t517, i64 16
  %t530 = getelementptr i8, ptr %t517, i64 17
  %t531 = getelementptr i8, ptr %t517, i64 20
  %t532 = getelementptr i8, ptr %t517, i64 24
  %t533 = getelementptr i8, ptr %t517, i64 32
  store i32 5, ptr %t517, align 4
  store i64 %t527, ptr %t528, align 8
  store i8 0, ptr %t529, align 1
  store i8 1, ptr %t530, align 1
  store i32 0, ptr %t531, align 4
  store ptr null, ptr %t532, align 8
  store ptr null, ptr %t533, align 8
  br label %zr_aot_fn_8_ins_52

zr_aot_fn_8_ins_52:
  %t534 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 52, i32 2)
  br i1 %t534, label %zr_aot_fn_8_ins_52_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_52_body:
  br label %zr_aot_fn_8_ins_53

zr_aot_fn_8_ins_53:
  %t535 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 53, i32 1)
  br i1 %t535, label %zr_aot_fn_8_ins_53_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_53_body:
  %t536 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 9, i32 4, i32 9)
  br i1 %t536, label %zr_aot_fn_8_ins_54, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_54:
  %t537 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 54, i32 2)
  br i1 %t537, label %zr_aot_fn_8_ins_54_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_54_body:
  %t538 = call i1 @ZrLibrary_AotRuntime_IsTruthy(ptr %state, ptr %frame, i32 9, ptr %truthy_value)
  br i1 %t538, label %zr_aot_fn_8_ins_54_truthy, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_54_truthy:
  %t539 = load i8, ptr %truthy_value, align 1
  %t540 = icmp eq i8 %t539, 0
  br i1 %t540, label %zr_aot_fn_8_ins_57, label %zr_aot_fn_8_ins_55

zr_aot_fn_8_ins_55:
  %t541 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 55, i32 0)
  br i1 %t541, label %zr_aot_fn_8_ins_55_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_55_body:
  %t542 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t543 = load ptr, ptr %t542, align 8
  %t544 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t545 = load ptr, ptr %t544, align 8
  %t546 = load i32, ptr %t545, align 4
  %t547 = getelementptr i8, ptr %t545, i64 8
  %t548 = load i64, ptr %t547, align 8
  %t549 = icmp uge i32 %t546, 2
  %t550 = icmp ule i32 %t546, 5
  %t551 = and i1 %t549, %t550
  br i1 %t551, label %zr_aot_add_int_const_fast_552, label %zr_aot_fn_8_fail
zr_aot_add_int_const_fast_552:
  %t553 = add i64 %t548, 8
  %t554 = getelementptr i8, ptr %t543, i64 8
  %t555 = getelementptr i8, ptr %t543, i64 16
  %t556 = getelementptr i8, ptr %t543, i64 17
  %t557 = getelementptr i8, ptr %t543, i64 20
  %t558 = getelementptr i8, ptr %t543, i64 24
  %t559 = getelementptr i8, ptr %t543, i64 32
  store i32 5, ptr %t543, align 4
  store i64 %t553, ptr %t554, align 8
  store i8 0, ptr %t555, align 1
  store i8 1, ptr %t556, align 1
  store i32 0, ptr %t557, align 4
  store ptr null, ptr %t558, align 8
  store ptr null, ptr %t559, align 8
  br label %zr_aot_fn_8_ins_56

zr_aot_fn_8_ins_56:
  %t560 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 56, i32 2)
  br i1 %t560, label %zr_aot_fn_8_ins_56_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_56_body:
  br label %zr_aot_fn_8_ins_57

zr_aot_fn_8_ins_57:
  %t561 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 57, i32 1)
  br i1 %t561, label %zr_aot_fn_8_ins_57_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_57_body:
  %t562 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 10, i32 5, i32 10)
  br i1 %t562, label %zr_aot_fn_8_ins_58, label %zr_aot_fn_8_fail

zr_aot_fn_8_ins_58:
  %t563 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 58, i32 2)
  br i1 %t563, label %zr_aot_fn_8_ins_58_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_58_body:
  %t564 = call i1 @ZrLibrary_AotRuntime_IsTruthy(ptr %state, ptr %frame, i32 10, ptr %truthy_value)
  br i1 %t564, label %zr_aot_fn_8_ins_58_truthy, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_58_truthy:
  %t565 = load i8, ptr %truthy_value, align 1
  %t566 = icmp eq i8 %t565, 0
  br i1 %t566, label %zr_aot_fn_8_ins_61, label %zr_aot_fn_8_ins_59

zr_aot_fn_8_ins_59:
  %t567 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 59, i32 0)
  br i1 %t567, label %zr_aot_fn_8_ins_59_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_59_body:
  %t568 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t569 = load ptr, ptr %t568, align 8
  %t570 = getelementptr %ZrAotGeneratedFrame, ptr %frame, i32 0, i32 3
  %t571 = load ptr, ptr %t570, align 8
  %t572 = load i32, ptr %t571, align 4
  %t573 = getelementptr i8, ptr %t571, i64 8
  %t574 = load i64, ptr %t573, align 8
  %t575 = icmp uge i32 %t572, 2
  %t576 = icmp ule i32 %t572, 5
  %t577 = and i1 %t575, %t576
  br i1 %t577, label %zr_aot_add_int_const_fast_578, label %zr_aot_fn_8_fail
zr_aot_add_int_const_fast_578:
  %t579 = add i64 %t574, 16
  %t580 = getelementptr i8, ptr %t569, i64 8
  %t581 = getelementptr i8, ptr %t569, i64 16
  %t582 = getelementptr i8, ptr %t569, i64 17
  %t583 = getelementptr i8, ptr %t569, i64 20
  %t584 = getelementptr i8, ptr %t569, i64 24
  %t585 = getelementptr i8, ptr %t569, i64 32
  store i32 5, ptr %t569, align 4
  store i64 %t579, ptr %t580, align 8
  store i8 0, ptr %t581, align 1
  store i8 1, ptr %t582, align 1
  store i32 0, ptr %t583, align 4
  store ptr null, ptr %t584, align 8
  store ptr null, ptr %t585, align 8
  br label %zr_aot_fn_8_ins_60

zr_aot_fn_8_ins_60:
  %t586 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 60, i32 2)
  br i1 %t586, label %zr_aot_fn_8_ins_60_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_60_body:
  br label %zr_aot_fn_8_ins_61

zr_aot_fn_8_ins_61:
  %t587 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 61, i32 8)
  br i1 %t587, label %zr_aot_fn_8_ins_61_body, label %zr_aot_fn_8_fail
zr_aot_fn_8_ins_61_body:
  %t588 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 0, i1 false)
  ret i64 %t588

zr_aot_fn_8_end_unsupported:
  %t589 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 8, i32 62, i32 0)
  ret i64 %t589

zr_aot_fn_8_fail:
  %t590 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t590
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
