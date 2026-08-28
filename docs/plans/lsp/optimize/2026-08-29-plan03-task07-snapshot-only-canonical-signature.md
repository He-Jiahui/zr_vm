# Plan 03 Task 7.18 Snapshot-Only Canonical Signature Help

## 目标

- 让 source direct-call signature help 在 analyzer compiler/symbol state 不可用时
  仍消费当前 parser semantic snapshot。
- 保持 `CallAt/FormatCall` 缺失时 fail closed，不进入 overload、callee name 或
  AST signature reconstruction。
- 将 legacy compiler-state 门禁限制在尚未迁移的 constructor/super/fallback
  分支之后。

## 完成项目

- `GetSignatureHelp` 不再把 `compilerState` 作为进入 canonical call query 的
  前置条件。
- Local canonical call、通用 canonical call及 external callable adapter先于
  legacy compiler-state guard执行。
- canonical query 未命中且 source fact表明该调用必须 canonical 时仍立即返回
  unavailable；只有之后的 legacy construct/super/method/function路径要求
  compiler state。
- Direct-call测试在请求前同时卸载 analyzer `compilerState` 与 `symbolTable`，
  验证签名仍严格等于 `FormatCall`；随后清除 call payload，验证仍 fail closed。
- Source contract 固定 canonical resolver 位于 legacy compiler-state guard之前。
- `lsp_signature_help.c` 已为大型单职责文件；本任务只移动既有6行门禁，未增加
  helper或新职责，因此未做与行为无关的大规模拆分。

## 验证

- GCC/Clang/MSVC semantic-query parity：`9 Pass / 0 Fail`，真实 exit 0。
- GCC/Clang/MSVC source contracts：`65 Pass / 0 Fail`，真实 exit 0。
- 三工具链 full interface 中新增 detached-state direct-call case 均 PASS；
  runner均保持 `109 Pass / 4 Fail`、真实 exit 1，既有 marker delta 0。
- 三工具链 project features 均保持 `54 Pass / 6` 个既有 marker，runner
  exit 0，marker delta 0。
- Callable-value/lambda的现有两项失败仍是 canonical call producer缺口，未在
  LSP侧补 AST/name fallback，也未计 GREEN。

## 状态与产出记录

- 完成时间：2026-08-29 03:38 +08:00。
- 状态：已完成。
- 完成项目：snapshot-only canonical signature dispatch、deferred legacy
  compiler-state gate、detached-state RED/GREEN、三工具链 focused与固定 marker
  审计。
