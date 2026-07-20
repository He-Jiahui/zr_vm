# Syntax 02 M6 artifact and LSP consumers acceptance

对应计划：`docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md`
的 `M6 Artifact/LSP consumers`。

## Scope

- reference callable contract 的 source canonical TypeId、binary signature import 与 public identity。
- VM/AOT 对 signature summary 和 ContractRow 的同规则校验。
- resolved free/member call 的 callable TypeId、signature display、SymbolId 与 declaration range。
- LSP hover、signature help、parameter information、diagnostics 与 direct-caller invalidation。
- readonly/mutable receiver、scoped ref、ref readonly、generic clause 与 closed generic type display。

## RED evidence

- 初始 artifact reader 只验证 signature bytes，ContractRow 的 receiver/ref-export/effect/
  parameter/scoped 字段可以与 function signature 不一致；source ref return 的 export effect 也
  可被独立 header marker 覆盖。
- 初始 call query 没有稳定 resolved target identity，receiver consumer 只能按 member name
  猜测；readonly receiver display、零参数 declaration refinement 和 compiler diagnostic
  persistent fact 均存在缺口。
- source generic receiver call 的 closed TypeId 正确，但 signature display 丢失声明
  `<const N: int>`；真实 stdio smoke 因此失败，LSP AST/string fallback 被明确禁止。
- hover range 定向 RED 证明 custom interface runner 即使产生 `Fail -` 仍可能 process exit 0；
  receiver 与 free callable 都曾返回 cursor range，而不是 resolved reference fact range。
- 旧 MSVC static cache 出现 heap corruption/挂起且日志为空，该缓存证据全部作废；fresh
  short-path static cache 从零重建后未复现。

## GREEN implementation

- artifact writer 从 canonical return node 推导 ref-export；importer 先恢复 return TypeId，再拒绝
  不匹配 marker。core signature summary 与 root ContractRow 逐字段交叉验证，VM/AOT 对 stale
  receiver/ref-export/effect/parameter/scoped metadata 返回相同 `INVALID_SIGNATURE`。
- `SyntaxCallable_RefineFromDeclaration` 与 `RebindFunctionSignature` 保留 passing、escape、ref
  access、receiver 和 callable effects，只替换关闭后的参数/返回 TypeId。
- `SZrParserSemanticCallQuery` 只在 resolved reference 存在时发布
  `hasResolvedTarget + targetSymbolId + targetDeclarationRange`；member identity 按精确 declaration
  node 注册，禁止 name fallback。
- signature fact 同时消费结构化 generic parameters 与 closed callable TypeId。readonly receiver
  输出 `const fn`，mutable receiver 输出 `fn`，free/static 无 receiver 前缀。
- compiler current diagnostic 深拷贝为 persistent semantic fact；diagnostics query 重复调用稳定，
  public-contract query 在 persistent/query diagnostics 存在时保守 unavailable。
- LSP signature help、hover 和 parameter information 消费 `CallAt/FormatCall` 与 canonical
  parameter contracts；resolved receiver identity 驱动 direct-caller invalidation。call hover range
  来自同一 reference fact，并保留实参 hover 的包含边界。

## Verification

- GCC 11.4、Clang 14、MSVC 19.44 (`VSCMD_VER=17.14.36`) 均在最终同一源码基线上运行
  16 个目标，三套均为 16/16 真实 process exit 0。
- 三套 canonical consumers 均为 10/10；其中 source reference callable canonical form、binary
  signature import TypeId、VM projection 与 AOT projection 一致，ref-export/scoped mismatch
  负例在 VM/AOT 同规则拒绝。
- 三套 focused 结果还包括 semantic query 26/26、compiler query diagnostics 18/18、semantic
  facts 12/12、canonical type graph 19/19、expression facts 28/28。
- 三套 LSP interface 均为 90 Pass、0 Fail；local semantic query 32 项、semantic analyzer 46 项、
  query diagnostics 14 项无新增 failure marker。
- GCC、Clang 与 fresh MSVC `.codex/q` 的 stdio/CLI smoke 均真实 exit 0。
- 每套日志的 unexpected marker 为 0；仅保留 project binary/plugin 的 4 个既有允许 marker，
  因此本记录不宣称全仓所有 LSP provider GREEN。
- 最终跨提交与模块文档审计：Critical 0 / Important 0，`git diff --check` 为 0。

## Promotion gate

- callable contract source -> binary import roundtrip：PASS。
- signature summary 与 ContractRow cross-validation：PASS。
- VM 与 AOT 对相同 signature/negative mismatch 输出一致：PASS。
- resolved call target 使用 SymbolId/declaration range，禁止 member-name fallback：PASS。
- hover/signature/parameter information/diagnostics 使用统一 canonical facts：PASS。
- receiver effect、scoped ref、ref readonly、generic clause 与 closed type display：PASS。
- call hover canonical UTF-16 range 与 argument-hover conservative boundary：PASS。
- GCC/Clang/MSVC 16-target matrix 与 stdio/CLI smoke：PASS。

结论：M6 晋级门全部通过，Syntax 02 的 M1-M6 已完成，可以进入 Syntax 03 ref struct/Span layout。
