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
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding_contract_internal.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c
  - tests/cmake/ssa-tests.cmake
  - tests/cmake/ssa-builder-tests.cmake
tests:
  - tests/parser/test_ssa_static_binding_facts.c
  - tests/parser/test_ssa_core_model.c
  - tests/parser/test_ssa_oracle_call_differential.c
  - tests/parser/test_ssa_pass_manager_scalar.c
  - tests/parser/test_ssa_binding_rows_artifact.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_typed_call_binding.c
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/02-static-binding-facts.md
  - docs/plans/ssa/architecture-design.md
  - user: 2026-09-12 实现 SSA 计划目录下的阶段性目标
doc_type: module-detail
status: implemented-subset
---

# ExecIR 静态 Binding Facts

## 事实边界

`SZrExecIrBindingFacts` 是 parser 在类型推导和 compiler binding builder
之后发布的、只含稳定标识的快照。每个 `SZrExecIrBindingSegment` 表示一段
receiver/place 求值：运行时字段段保留 `PLACE_PROJECT`/`LOAD`，accessor 和
meta 段保存操作类型，最后一段关联到 `SZrExecIrBindingRow`。行中复用已有
`SZrCallBindingContract` 和 `SZrCallBindingLocation`；ExecIR 指令只保存行引用，
不保存 AST、函数指针或成员名称。producer facts 数组仍由生产者借用；Core
投影会复制完整行到 `SZrExecIrFunction.bindingRows`，函数对象不保留这些借用指针。

函数行表有明确的 schema 模式：schema 0 是历史模式，不拥有行表；schema 1
拥有 typed rows，即使表为空也保留 schema 1。schema 1 中 `bindingRow=0`
表示无行，`1..N` 表示 `bindingRows[bindingRow - 1]`。这避免把行号与
`FunctionId` 混为一谈。Core 负责表的复制、释放、函数克隆、字段级哈希和
schema/行/指令双向关联验证。

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

`ZrParser_ExecIr_ProjectBindingFacts` 是事务式入口：先完整校验 borrowed facts
并准备行表副本，再由 Core 原子替换 owned rows 并写入 1-based 指令引用。
重复投影和合法空投影会释放旧表/旧引用；失败时保留函数原有的 module identity、
行表和引用。sealed 函数、越界指令、
缺失/未知成员、重载歧义、签名或 module/layout 不一致分别返回结构化
status，并在 `SZrExecIrDiagnostic` 中提供 function token、IR instruction、
source id 和 expected/actual hash/version。失败不会生成名称查找 opcode。

Schema 1 的顶层 `flags` 当前没有已定义位，producer 必须写入零。`ValidateEx`
在 schema 检查后拒绝任意低位或高位标志，返回
`ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT`，并在诊断中保留
`expectedVersion=0`、`actualVersion=facts->flags`；`ProjectBindingFacts` 因而在
替换 owned rows 前失败，原有函数 metadata、行表和引用保持不变。该边界先由
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/binding-flags-red-receipt.json`
（SHA256 `64ed3de95b09b01fa14805e0a78dc0a515c147c80c5b49db7ce20a459efe7622`）固定，随后由
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/binding-flags-green-receipt.json`
（SHA256 `dc0033da8495cc8e08636d1a0c210ec722fcfcb9a5c3528d44d61dce338bcc43`）验证；
`ssa_typed_binding_contract` 目标的 4 个测试全部通过，低位和高位保留标志均被拒绝，
且 UBSan 没有诊断。该 fixture 只验证 parser/Core facts，不执行 network、FFI、provider
或其他安全边界；SSA 总计划仍保持 OPEN。

当前 schema 1 是行所有权和模式迁移切片，不代表跨函数调用目标解析已经完成。
`zr_oracle_validate` 在执行前调用 Core 行表验证器：schema 0 的历史 prototype
references 仍保留原有行为；未知 schema、损坏的 typed 表或不匹配的指令关联在
执行及 callback 前失败。通用 Oracle callback 没有 typed contract 参数，因此
结构有效但绑定行非零的 typed CALL/INVOKE 仍以 `UNSUPPORTED` 拒绝，且不调用旧
callback。call graph、devirtualize、
inline、LICM、fusion、AOT projection 等消费者必须识别 schema 1 并解码完整行，
或保守拒绝/报告 unresolved；绝不能把 1-based row reference 当作 `FunctionId`。
EIS1-EIS5 当前 wire format 不包含 rows schema 与 payload，所以这些 writer 拒绝
schema 1（包括 typed-empty），不静默丢失模式信息。后续 typed target resolver、
分析消费者迁移和持久化格式扩展仍未完成。

`ZrParser_ExecIr_BindingFacts_Hash` 按字段使用 FNV-1a，排除 C 结构填充和
`expectedHash` 自身；它同时纳入函数 signature hash、module hash、generation，
因此同一 token/slot/layout/operation 序列在不同主机 ABI
上有相同事实身份，重排或篡改行会在投影前被拒绝。

## 验证覆盖

`tests/parser/test_ssa_static_binding_facts.c` 覆盖 direct final call、字段
链保序、accessor setter/writeback、virtual/interface/typed contract，以及
unknown、ambiguous、signature/hash、layout、missing contract、sealed 和
失败不改目标函数等路径。新增 Core/model 断言覆盖 owned copy、clone 独立性、
typed-empty、字段完整哈希和失败替换回滚；Oracle 断言 typed CALL 不落入 legacy
callback、未知 schema 与失配 row association 在 callback 前拒绝；Core/model 也
区分已知非 CALL opcode 与真正未知 opcode 的诊断。pass-manager 断言 zero-effect
CALL 在标量清理后仍保留，并确认只更改 owned row 的 signature hash 会改变
`FunctionHash`。前一冻结版本的 root Core/parser focused gates 已通过：
Core 5/5、root 12/12、legacy capability direct 0，以及已注册测试 2/2。随后添加
了 Oracle 入口校验和以上诊断断言；这些最后改动尚未重新构建/运行，见
[`ssa-static-binding-owned-rows.md`](../../tests/acceptance/ssa-static-binding-owned-rows.md)。
既有 `test_call_binding_pipeline.c` 与
`test_typed_call_binding.c` 继续验证 compiler 生产出的历史 call-site contract；
本接口只消费这些稳定 contract，不复制 overload/member 字符串解析。
