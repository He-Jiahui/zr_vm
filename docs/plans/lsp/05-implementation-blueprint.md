# LSP 05：实施路线

## Milestones

1. **L1 query schema**：冻结snapshot-aware Symbol/Type/Place/Flow/Call/Property/Module query，artifact reader可提供binary facts。
2. **L2 core features**：hover/definition/references/completion/signature help只消费query。
3. **L3 diagnostics**：统一registry、related ranges、fact IDs、safe fixes和legacy migration。
4. **L4 modules**：ModuleSpecifier、`#alias`、单段`@package`、`.zrm`、native module虚拟文档。
5. **L5 ownership/property**：borrow/move/readonly/ref struct/resource/property/ref-return完整展示与fix。
6. **L6 robustness**：UTF-16、incremental invalidation、cancellation、snapshot race、workspace edits。
7. **L7 debug/REPL**：共享expression compilation与DAP/zr.debug bridge。
8. **L8 convergence**：删除LSP本地类型/ownership字符串推断与旧`%xxx`展示。

## 测试层级

- parser/semantic query unit：精确fact与range。
- language_server unit：projection、position、cancellation、JSON。
- stdio protocol smoke：真实initialize/open/change/request/close。
- project fixture：source/binary/native/package四provider。
- syntax reference golden：全部目标display/diagnostic/action。

每个上层failure先定位到底层fact还是projection；不得在LSP加特例掩盖compiler缺口。

## 完成记录规则

已有证据位于`01-semantic-core/`、`02-diagnostics/`、`03-robustness/`。以后只为独立contract写完成记录，必须注明tested snapshot/schema、命令、结果与open scope；不再把表达式逐例进度写进计划正文。

## 依赖与Promotion账本

| Milestone | 硬依赖 | 可交付 | 晋级证据 |
|---|---|---|---|
| L1 query schema | syntax 01 + compiler semantic snapshot | versioned query C/API与golden | parser/query leaf + binary reader |
| L2 core features | L1 | hover/definition/reference/completion/signature | source/binary/native parity |
| L3 diagnostics | L1 + syntax 02-06 | registry、related ranges、safe fixes | apply-edit-rebind与CLI/LSP code parity |
| L4 modules | L1 + syntax 10 | ModuleSpecifier/package/.zrm/native virtual docs | exports/version/reload/ABI negative |
| L5 ownership/property | L1/L3 + syntax 02-05/09 | borrow/owner/property/pool intelligence | causal ranges与migration actions |
| L6 robustness | L1-L5 | UTF-16/incremental/cancel/cache budgets | race/stress/perf report |
| L7 debug/REPL | L1/L2/L6 + DebugMap | context-aware evaluator | effect/capability/stale-generation matrix |
| L8 convergence | all + syntax 07 | reference fixture golden、fallback deletion | full project/protocol matrix |

每阶段完成记录必须列出query/schema generation、具体协议请求、编辑版本、leaf和stdio/project结果、p50/p95/p99与峰值内存、未覆盖provider。单个表达式microcase只能作为evidence record，不能改变milestone状态。

## 完成记录

- [Numeric range microcase evidence](./01-semantic-core/2026-07-06-numeric-range-microcase-evidence.md)
- [Structured diagnostic baseline](./02-diagnostics/2026-07-19-structured-diagnostic-baseline.md)
- [Binary export declaration identity](./03-robustness/2026-07-20-binary-export-declaration-identity.md)
- [Descriptor plugin type member parity](./03-robustness/2026-07-20-descriptor-plugin-type-member-parity.md)
- [Native descriptor function callable parity](./01-semantic-core/2026-07-20-native-descriptor-function-callable-parity.md)
- [Native receiver method callable parity](./01-semantic-core/2026-07-20-native-receiver-method-callable-parity.md)
- [Native generic receiver callable parity](./01-semantic-core/2026-07-20-native-generic-receiver-callable-parity.md)
- [Source rename workspace edit snapshot revalidation](./03-robustness/2026-07-20-source-rename-workspace-edit-snapshot-revalidation.md)
- [General rename workspace edit snapshot revalidation](./03-robustness/2026-07-21-general-rename-workspace-edit-snapshot-revalidation.md)
- [Code action workspace edit snapshot revalidation](./03-robustness/2026-07-21-code-action-workspace-edit-snapshot-revalidation.md)
- [Semicolon safe-fix convergence](./02-diagnostics/2026-07-21-semicolon-safe-fix-convergence.md)
- [Condition-close safe-fix convergence](./02-diagnostics/2026-07-21-condition-close-safe-fix-convergence.md)
- [Index-close safe-fix convergence](./02-diagnostics/2026-07-21-index-close-safe-fix-convergence.md)
- [Parameter-list-close safe-fix convergence](./02-diagnostics/2026-07-21-parameter-list-close-safe-fix-convergence.md)
- [Call-close safe-fix convergence](./02-diagnostics/2026-07-21-call-close-safe-fix-convergence.md)
- [Group-close safe-fix convergence](./02-diagnostics/2026-07-21-group-close-safe-fix-convergence.md)
- [Array-close safe-fix convergence](./02-diagnostics/2026-07-21-array-close-safe-fix-convergence.md)
- [Object-close safe-fix convergence](./02-diagnostics/2026-07-21-object-close-safe-fix-convergence.md)
- [Object computed-key close safe-fix convergence](./02-diagnostics/2026-07-21-object-computed-key-close-safe-fix-convergence.md)
- [Object property-colon safe-fix convergence](./02-diagnostics/2026-07-21-object-property-colon-safe-fix-convergence.md)
- [Object property-separator safe-fix convergence](./02-diagnostics/2026-07-21-object-property-separator-safe-fix-convergence.md)
- [Conditional-colon safe-fix convergence](./02-diagnostics/2026-07-21-conditional-colon-safe-fix-convergence.md)
- [Array element-separator safe-fix convergence](./02-diagnostics/2026-07-21-array-element-separator-safe-fix-convergence.md)
- [For/foreach header safe-fix convergence](./02-diagnostics/2026-08-08-for-foreach-header-safe-fix-convergence.md)

这些记录只对应L1/L2/L3/L6的部分历史能力；source-file rename、普通`textDocument/rename`和当前code action已共享workspace-edit snapshot重校验，code action还在resolve时拒绝stale edit；semicolon、condition-close、index-close、parameter-list-close、call-close、group-close、array-close、array-element-separator、object-close、object-computed-key-close、object-property-colon、reachable object-property-separator与conditional-colon local insertion已完成structured fact到apply-edit-rebind闭环，但其他delimiter family/replacement、其他diagnostic fix producer、并发race/cancellation、性能与峰值内存报告仍缺失，不改变L3整体、L4-L8及完整L6状态。
