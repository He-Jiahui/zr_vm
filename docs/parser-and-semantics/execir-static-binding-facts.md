---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_binding_facts.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_binding_facts.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_binding_facts.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_binding_facts.c
tests:
  - tests/parser/test_ssa_static_binding_facts.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_typed_call_binding.c
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/02-static-binding-facts.md
  - docs/plans/ssa/architecture-design.md
  - user: 2026-09-12 实现 SSA 计划目录下的阶段性目标
doc_type: module-detail
status: implemented
---

# ExecIR 静态 Binding Facts

## 事实边界

`SZrExecIrBindingFacts` 是 parser 在类型推导和 compiler binding builder
之后发布的、只含稳定标识的快照。每个 `SZrExecIrBindingSegment` 表示一段
receiver/place 求值：运行时字段段保留 `PLACE_PROJECT`/`LOAD`，accessor 和
meta 段保存操作类型，最后一段通过 `bindingRow` 指向
`SZrExecIrBindingRow`。行中复用已有 `SZrCallBindingContract` 和
`SZrCallBindingLocation`；ExecIR 指令只保存行号，不保存 AST、函数指针或成员
名称。事实数组由生产者借用，投影函数不会把这些指针写入函数对象。

这一区分很重要：`obj.a.b.invoke()` 的 `a`、`b` 读取仍是独立的字段段，
可选链的 receiver/ownership 检查不能因为最终调用已经有 token 就被删除；
`module.Type.method()` 则可以直接把最终调用绑定到 member/signature token。
dynamic 语义必须由上游显式产生，静态投影没有按名称查找的回退路径。

## 校验与投影

调用 `ZrParser_ExecIr_BindingFacts_ValidateEx` 会先检查 schema、数组边界、
连续的 segment/row index、重复 instruction/row、最终调用唯一性和 ExecIR
指令范围，然后逐行调用 `ZrCore_CallBinding_CheckContract`。module hash、
generation、owner layout/version/hash、virtual/interface dispatch slot 以及
typed callable 的 signature token/hash 都在同一阶段检查；typed callable 的
relocation 固定为 `NONE`，不会把上一次 closure witness 带入事实。accessor setter
必须声明 `SET` operation 和 `WRITEBACK` 标志；getter/meta 不能伪装成普通
field load。

`ZrParser_ExecIr_ProjectBindingFacts` 是事务式入口：完整校验成功后才把行号
写入目标指令的 `bindingRow`，失败时不修改函数。sealed 函数、越界指令、
缺失/未知成员、重载歧义、签名或 module/layout 不一致分别返回结构化
status，并在 `SZrExecIrDiagnostic` 中提供 function token、IR instruction、
source id 和 expected/actual hash/version。失败不会生成名称查找 opcode。

`ZrParser_ExecIr_BindingFacts_Hash` 按字段使用 FNV-1a，排除 C 结构填充和
`expectedHash` 自身；它同时纳入函数 signature hash、module hash、generation，
因此同一 token/slot/layout/operation 序列在不同主机 ABI
上有相同事实身份，重排或篡改行会在投影前被拒绝。

## 验证覆盖

`tests/parser/test_ssa_static_binding_facts.c` 覆盖 direct final call、字段
链保序、accessor setter/writeback、virtual/interface/typed contract，以及
unknown、ambiguous、signature/hash、layout、missing contract、sealed 和
失败不改目标函数等路径。既有 `test_call_binding_pipeline.c` 与
`test_typed_call_binding.c` 继续验证 compiler 生产出的历史 call-site contract；
本接口只消费这些稳定 contract，不复制 overload/member 字符串解析。
