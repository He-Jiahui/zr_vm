# Plan 03 Task 7.22 Canonical Super Constructor Signature

## 目标

- 让 source class `super(...)` 发布完整 canonical call facts。
- 让 `super(...)` signature help 在 compiler/symbol state 脱离后只消费
  `CallAt/FormatCall`。
- 删除 LSP 内第二套 base-constructor resolver；canonical payload 缺失时 fail closed。

## 完成项目

- AST 保存精确 `super(...)` call range，parser 在 class meta-function 上发布 CALL 与
  REFERENCE_CALL facts。
- Producer 从 resolved base constructor contract 驻留 receiver `NONE` 的 closed callable
  TypeId，并发布稳定 SymbolId、完整 declaration range 和 `@constructor(...)` display。
- LSP lightweight prototype 尚未携带 constructor member 时，semantic analyzer 复用 parser
  source constructor materializer 补齐 snapshot-local structured member；不按 constructor 名称
  或源码文本配对。
- Signature consumer 在 compiler-state 门禁前调用 `CallAt/FormatCall`，并删除 request-time
  base prototype/constructor 搜索、参数闭合和本地签名格式化。
- Canonical payload 移除、base declaration unresolved 或 identity 无效时保持 unavailable，
  不回退到 AST/name inference。

## 验证

- 固定源为 `da114f9 + 10 exact code/test overlays`；WSL 与 Windows snapshot overlay 均通过
  SHA-256 `10/10` 审计，七个 submodule revision 完整。
- GCC 11.4、Clang 14、MSVC 19.44 canonical consumers 均 `21/21`，真实 exit 0；source
  contracts 均真实 exit 0。
- 三工具链 interface 均 `111 Pass / 2 Fail`、真实 exit 1；固定失败仍为 class-member
  navigation 与 reference-call diagnostic。super signature/definition/references/highlights
  目标用例全部 PASS，marker delta 0，不把完整 interface 计为 GREEN。
- 三工具链 full stdio 均在既有 generic fixture `short_circuit_unreachable` 缺失处前置停止，
  真实 exit 1，不计 GREEN；三套 CLI `--version` 均真实 exit 0。

## 状态与产出记录

- 完成时间：2026-08-29 20:59 +08:00。
- 状态：已完成。
- 完成项目：super constructor canonical producer、exact call range、resolved target/declaration
  identity、snapshot-only signature consumer、legacy resolver 删除、fail-closed 边界与三工具链
  fixed snapshot 验收；super producer 已拆入独立 cohesive module，call-facts 文件回落到
  约 918 行。
