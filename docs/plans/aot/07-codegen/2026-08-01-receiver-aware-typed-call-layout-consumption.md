---
plan_id: aot-07-codegen
record_id: 2026-08-01-receiver-aware-typed-call-layout-consumption
status: completed
completed_at: 2026-08-01 09:47:07 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# AOT 07 Receiver-Aware Typed-Call Layout Consumption

## 状态与产出记录

- 完成时间：2026-08-01 09:47:07 +08:00
- 状态：A7.2I receiver-aware typed-call layout consumption 子里程碑完成；A7.2、AOT 07、AOT 12 与
  AOT 07~12 总目标继续进行。
- 完成项目：generic/shared inline-struct `CALL_TYPED` selector 现在接受 parameter sidecar index 0 上精确等于
  `ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER` 的 canonical receiver；receiver 已计入 `argumentCount`，因此 caller
  source 仍按 `instruction->operand0 + 1u + argumentIndex` 映射，不增加隐藏偏移。
- 完成项目：role-free 普通参数保持兼容；未知/组合 role 或非 index 0 receiver 继续 fail closed，receiver
  TypeRef 仍必须独立投影为 OBJECT/ARRAY，不能仅凭 role 启用 shared route。
- 完成项目：新增 receiver + 显式 reference 参数 + scalar 参数的 Unix shared-library runtime 回归，生成 C
  通过 `ZrLibrary_AotRuntime_CallInlineStruct` 执行并返回 73，证明同一参数窗口可在 AOT 模式完整传递。
- 完成项目：保留 A7.2E 的未知 role、错位/重复 receiver producer gate，并新增 unknown receiver TypeRef
  consumer 负例；不改变 runtime API、generic dictionary、public function、artifact、manifest 或 reachability schema。
- 计划映射：完成 AOT 07 A7.2I receiver-aware retained parameter-layout consumption；AOT 12 继续复用裁剪后
  callee ExecIR sidecar，不新增图节点，S1/S3/S6 与 AOT 12 保持部分完成。

## 代码与文档产出

- `backend_aot_c_value_semir_calls.c` 将 parameter role gate 从 no-role 收窄放宽为普通参数或 index 0 canonical
  receiver，同时保留 exact count、projected reference TypeRef 与 caller VALUE slot shape 校验。
- `test_aot_c_generic_call_typed_parameter_layout_cases.h` 增加可控 receiver role 投影、slot-0 正例、unknown
  receiver TypeRef 负例，以及 receiver/reference/scalar runtime 参数窗口用例。
- `test_aot_c_value_semir_contracts.c` 锁定 receiver role 常量和 index-0 gate；模块文档、AOT 07/12 链接与
  acceptance evidence 同步更新。
- 大文件决策：新增 fixture 与 runtime harness 留在既有 focused parameter-layout cases header；主 generic-call
  测试文件仅增加注册项，没有继续增长实现职责，生产文件也远低于拆分阈值。

## 验证结果

- 初始实例方法 fixture 仍产生 `DYN_CALL`，没有进入目标 consumer；该无效假设已撤销。有效 RED 在真实
  `CALL_TYPED` callee 的第 0 个 projected parameter 上注入 canonical receiver role，冻结 WSL GCC 为
  13 项中 1 项失败，且仅缺少 shared callsite marker。
- 最终复审发现原单参数 unknown receiver 负例会因没有任何其他引用参数而形成假阴性。负例改为
  receiver TypeRef unknown、第二个显式参数仍为 OBJECT 的三参数窗口后，中间实现为 14 项中 1 项失败；
  receiver branch 增加 `!isReferenceParameter` gate 后转绿。动态实例边界也直接断言生成 ExecIR 清单中的
  `DYN_CALL exec=`，不再只靠 shared marker 缺失间接推断。
- GREEN 后 WSL GCC/Clang generic typed-call 均为 14/0，value-SemIR contracts 均为 8/0；新增 Unix runtime
  case 编译生成 C 为共享库，解释器与 AOT 都返回 73。
- Windows MSVC 19.44.35228.0 x64 Debug 两个 focused target 构建退出码 0；generic typed-call 为 10 通过、
  4 个 Unix-only runtime case ignored，value-SemIR contracts 为 8/0。仅保留 `%TEMP%` 目录既有 MSB8029。
- WSL GCC 与 Clang 邻接矩阵均为 MethodInfo 3/0、code stripping 37/0、SemIR 10/0、generic sharing 9/0、
  debug metadata 6/0、typed-call contracts 4/0；MSVC 同一矩阵全部通过。
- 独立设计审查确认 receiver 已包含在 argument count、runtime 使用同一 `functionSlot + 1` 参数窗口，且
  dictionary `staticMethod` ABI 不应在本切片改变；最终复审的 P2/P3 均由上述隔离负例和直接 opcode 证据
  关闭，复审未发现其他 correctness、ABI 或测试问题。

## 未完成边界

- 当前源码实例方法调用仍编译为 `DYN_CALL`；本切片验证的是已产生 `CALL_TYPED` 时 canonical receiver
  sidecar 的 AOT consumer 契约，不声明实例方法 producer 已静态化。
- `in/ref/out`/readonly/direction、default origin、TypeId/TypeRef/CallableContract 等价性、return destination、
  spill、address-taken slot、GC/ref provenance 与 safepoint/debug map 仍开放。
- 不完成 A7.2、AOT 07、AOT 12 或 AOT 07~12 总目标。
