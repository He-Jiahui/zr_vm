# 02-M6 Artifact/LSP consumers 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md`
的 `M6 Artifact/LSP consumers`。

## 状态与产出记录

- 完成时间：2026-07-20 12:00 +08:00
- 状态：已完成
- 完成项目：
  - reference callable 从 source AST/canonical TypeId 写入 structural signature，binary import
    恢复相同 TypeId 与 canonical form；ref-export 由 return ref access 推导并交叉验证。
  - core callable signature summary 发布 receiver/ref-export/effect/parameter/scoped 字段，root
    ContractRow 必须逐字段一致；VM 与 AOT 对同一 artifact projection 和 mismatch 同规则处理。
  - callable refinement/rebind 在关闭 generic 参数类型时保留 passing、escape、ref access、
    receiver 与 callable effects，不保存 interning 扩容前的内部数组指针。
  - free/member call fact 发布 closed callable TypeId、结构化 signature display 与 resolved
    `SymbolId + declarationRange`；`hasResolvedTarget` 为唯一有效性判据，禁止按 member name 推断。
  - receiver effect 由公共 declaration API 统一推导，零参数 member 也完成 refinement；readonly
    显示 `const fn`，mutable 显示 `fn`，generic clause 与 closed 参数/返回类型同时保留。
  - compiler diagnostic 深拷贝为 persistent semantic fact；diagnostics query 可重复读取，public
    contract 在 diagnostic facts 存在时保守 unavailable，不由 LSP 按 message 重建。
  - LSP hover/signature/parameter information 消费 `CallAt/FormatCall` 与 canonical parameter
    contracts；resolved receiver identity 驱动 direct-caller invalidation，hover range 投影同一
    reference fact，实参 hover 保留保守包含门禁。
  - GCC 11.4、Clang 14、MSVC 19.44 三套最终矩阵均 16/16 真实 exit 0；canonical consumers
    各 10/10、interface 各 90 Pass/0 Fail，三套 stdio/CLI smoke 均 exit 0。
  - 三工具链 unexpected marker 均为 0；仅保留 project binary/plugin 的 4 个既有允许 marker，
    未把 custom runner 的 process exit 0 单独解释为 GREEN。
- 验收证据：
  - `tests/acceptance/2026-07-20-syntax-02-m6-artifact-lsp-consumers.md`
  - `docs/parser-and-semantics/artifact-schema-and-type-projection.md`
  - `docs/parser-and-semantics/canonical-consumer-projection.md`
  - `docs/parser-and-semantics/reference-syntax-contract.md`
  - LSP consumer 记录：
    `docs/plans/lsp/03-robustness/2026-07-20-resolved-callable-consumer-convergence.md`
  - GCC/Clang/MSVC final2 日志：`.codex/logs/m6-final2-*`
- 里程碑提交：core/parser 实现进入 `0b2ead2`、`302db2e`、`95358a4`，LSP consumer 进入
  `5a20923`；验收证据与本记录随 `docs(syntax): complete artifact LSP consumers milestone`
  一并提交。

## 边界与后继

- M6 完成 Syntax 02 计划要求的 source、binary signature import、VM、AOT 与 source LSP
  canonical callable form 一致性；不把 legacy `SZrIo` 伪装为 `ZRAF`，不引入运行时 borrow
  fallback。
- imported binary/native/property/accessor/constructor/meta provider 的 LSP target identity 扩展
  仍属于对应 LSP/provider 计划；它们必须继续复用同一 query shape，不能回退到 name matching。
- project binary/plugin 的 4 个既有 marker 保留给相应 provider 修复，不阻塞本次 Syntax 02
  callable consumer 晋级门，也不被记录为全仓 GREEN。
- 下一阶段进入 Syntax 03 M1：`init TypeRef(...)` 与 ref struct/Span layout，继续消费本计划的
  canonical TypeRef、Place、region/ref facts 和 artifact identity，不另建并行类型规则。
