# 05-M4 Ref-return/Place/Region 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md` 的
`M4 Ref-return`，执行清单：
`docs/plans/syntax/05-property-unified-ast/m4-ref-return-place-region-implementation-plan.md`。

## 状态与产出记录

- 完成时间：2026-07-23 21:00 +08:00
- 状态：completed
- 完成项目：
  - 完成 `ref T` / `ref readonly T` property getter 的解析、精确范围、canonical TypeId、
    property/accessor SymbolId、receiver effect、writable-export effect 与 override/interface 不变性。
  - 完成一次 getter 调用的 reference-value lowering；value、`ref`、assignment 与 compound
    assignment 分别投影 Deref/Load、identity 或 writable Store，readonly/非 addressable 路径提前拒绝。
  - 发布 `PROPERTY_REF_GET`、`DEREFERENCE`、`PROPERTY_REF_STORE` 的 Place/LoanId/region 与
    execution SemIR；修复 reusable ValueId 令未来 loan 污染早期 instruction-use 的边界。
  - 新增 managed property-reference runtime，覆盖 class/static/inline struct/ref-struct/index Place、
    frame anchor 与 GC-safe base refresh；cold/second-run 及 source/reloaded artifact 行为一致。
  - C/LLVM AOT 均消费共享 managed-reference helper；focused test 同时编译生成的 C 与 LLVM object，
    native raw pointer 在缺少 managed/pinned descriptor 时保持 unavailable。
  - 更新 parser/loan/core/artifact/AOT 模块文档和 acceptance；冻结 50 个 exact paths，工作树与
    snapshot SHA-256 mismatch=0，三工具链 26/26 目标及三套 CLI smoke 通过。

## 当前实现边界

- M3 已提供 visible PropertySymbol、linked getter SymbolId、receiver capture、typed meta call、inline
  receiver source provenance、exception order 和 executable IO patch 36；M4 必须复用这些合同。
- 现有 reference callable、Place/Semantic IR、LoanId/NLL、reference escape、ref-struct 和 owner-borrow
  是下层事实来源；property consumer 不得按名称、display、诊断文本或 cache state 重建。
- M5 才处理 LSP hover/completion/rename/code action、最终 PropertyDef reflection 和旧属性迁移。

## 验收结论

- 冻结源：`HEAD=40f316fe78daed270d095c7b21152856ae51fed7 + 50 M4 exact paths`；
  SHA-256 mismatch=0，三份外部 Syntax 草案、LSP、build/log/generated artifact 均未进入 overlay。
- GCC 11.4.0、Clang 14.0.0、MSVC 19.44.35228.0 的 focused target 均为
  `23 Tests 0 Failures`、真实 exit 0；每套相同的 26-target property/reference/compiler/runtime/
  artifact/AOT parent matrix 均 26/26 exit 0。
- GCC/Clang/MSVC `zr_vm_cli classes_properties.zrp` 均真实 exit 0，精确输出 `40`。
- `zr_vm_aot_c_source_contracts_test` 的 `24 Tests 6 Failures` 与 clean HEAD 失败行集相同；
  `zr_vm_execbc_aot_pipeline_test` 的 23 条 FAIL/Assertion 行在 GCC/Clang 与 clean HEAD 完全相同，
  MSVC 同一位置触发既有 `0x80000003`。两项均不计为 GREEN，也未通过本里程碑放宽。
- M5 继续处理 LSP/reflection/legacy migration，必须消费 canonical SymbolId/TypeId/Place facts，
  禁止 property/member name、display text 或 cache-state fallback。
