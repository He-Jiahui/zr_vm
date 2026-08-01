---
plan_id: aot-07-codegen
record_id: 2026-08-01-parameter-default-declaration-projection
status: completed
completed_at: 2026-08-01 11:36:53 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# AOT 07 Parameter Default-Declaration Projection

## 状态与产出记录

- 完成时间：2026-08-01 11:36:53 +08:00
- 状态：A7.2J parameter default-declaration projection 子里程碑完成；A7.2、AOT 07、AOT 12 与
  AOT 07~12 总目标继续进行。
- 完成项目：内部 `SZrAotExecIrParameterLayout` 增加 canonical
  `defaultDeclarationKnown`/`hasDeclaredDefault` 对；仅 metadata 明确声明 default 的可靠正事实投影为
  `(true, true)`，并验证 `hasDeclaredDefault` 必须蕴含 `defaultDeclarationKnown`。
- 完成项目：投影仅接受 metadata count 与 runtime parameter count 精确相等且不含 receiver 的表；metadata
  false、partial metadata 与 receiver-bearing layout 均保持 `(false, false)` unknown，不把缺失的 producer
  完整性误写成 known-no-default。
- 完成项目：`SZrFunctionMetadataParameter.hasDefaultValue` 非 canonical bool 在全函数裁剪前验证失败，即使
  owner 不可达也不能被 code stripping 隐藏；GC-bearing `defaultValue` 不复制到 ExecIR。
- 完成项目：generic/shared inline-struct `CALL_TYPED` 在既有 exact-arity、complete argument-window route 内
  消费 defaultable declaration，并写入 `zr_aot_generic_call_typed_callee_defaultable_parameter_full_arity`
  审计 marker；省略参数与显式传入同值产生相同 marker，AOT 不补参、不求值，也不声明 callsite origin。
- 完成项目：invalid internal sidecar invariant fail closed 到普通 inline-struct route；runtime、dictionary、
  public function、artifact、manifest 与 reachability schema 均未改变。
- 计划映射：完成 AOT 07 A7.2J 的 default-declaration 正事实投影与消费；AOT 12 复用既有全树 prefilter 和
  retained frame owner，不新增图节点，S1/S3/S6 与 AOT 12 保持部分完成。

## 代码与文档产出

- `backend_aot_exec_ir.h` 定义内部事实与 canonical invariant；`backend_aot_exec_ir_frame.c` 校验原始 metadata
  bool，并在精确、receiver-free 的索引映射上投影可靠正事实。
- `backend_aot_c_value_semir_calls.c` 在 shared exact-arity selector 中消费该事实，invalid sidecar 不启用
  shared route，unknown declaration 不改变既有 route。
- `test_semir_pipeline.c` 锁定内部 schema 与投影源码锚点；generic typed-call suite 覆盖 omitted/explicit
  同标记、字段级 positive/false/partial/receiver 投影、完整 invariant 状态空间、invalid consumer sidecar 和
  不可达 malformed metadata 的全树 pre-strip 拒绝。
- 模块文档、AOT 07/12 计划回链与 acceptance evidence 同步更新。
- 大文件决策：生产文件分别为 555/493 行，低于约 1000 行拆分阈值；422 行的 focused case header 隔离
  fixture 与 Unix private-consumer harness，主测试文件只保留注册职责，未改动共享 `tests/CMakeLists.txt`。

## 验证结果

- 初始 RED 在未实现 schema/consumer 时为 WSL GCC generic typed-call 16 项中 1 项失败、SemIR 10 项中
  1 项失败；unknown 负例保持通过。
- 首版实现错误地把 callee metadata 命名为 default origin，并用 metadata false 表示 known-no-default。
  独立设计审查指出当前 frontend 已把 omitted 与 explicit argument 都物化为同一 exact window，且 producer
  false 可能表示 metadata 未携带默认值。实现据此收窄为可靠正声明，并删除 origin/materialization 声明。
- 复审提出 invalid `(known=false, declared=true)` sidecar 与不可达 malformed metadata 的直接覆盖缺口；新增
  private-consumer/invariant 负例和 code-stripping 前不可达 owner 负例后关闭。
- 最终 WSL GCC 与 Clang 的 generic typed-call 均为 19/0、SemIR 均为 10/0；Windows MSVC generic typed-call
  为 19 项、0 失败、4 个预期 Unix-only ignore，SemIR 为 10/0。
- GCC、Clang 与 MSVC 邻接矩阵均通过：MethodInfo 3/0、code stripping 37/0、SemIR 10/0、generic sharing
  9/0、debug metadata 6/0、value-SemIR 8/0、typed-call contracts 4/0。
- 最终验证树基于当前 `HEAD=e710a3b87ef61f82d059ca54fda45fe29e58bb92` 加精确 A7.2J overlay，覆盖
  Syntax typed-metadata/strict-legacy cutover 后的集成状态；六个实现/测试文件在 main、冻结 WSL 与冻结
  Windows 源树的 SHA-256 完全一致。MSVC 仅保留冻结 `%TEMP%` 路径既有 MSB8029 warning。

## 未完成边界

- 当前 callee metadata 不能区分 omitted argument 与 explicit argument；callsite default origin 需要 producer
  提供按参数索引对齐的 argument-origin bitset，仍为后续 A7.2 工作。
- metadata false 尚无 defaults-complete 契约，因此 known-no-default 事实仍开放；receiver-to-metadata index
  contract 未建立前，receiver-bearing layout 的 default declaration 保持 unknown。
- `in/ref/out`/readonly/direction、TypeId/TypeRef/CallableContract 等价性、return/destination、spill、
  address-taken slot、GC/ref provenance 与 safepoint/debug map 仍开放。
- 不完成 A7.2、AOT 07、AOT 12 或 AOT 07~12 总目标。
