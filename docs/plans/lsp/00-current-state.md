# LSP 00：当前状态与缺口

## 已有基线

language server已有semantic analyzer/query、symbol/type/union/ownership diagnostics、stdio JSON、位置处理和部分diagnostic fix。证据见：[semantic fact/query](./01-semantic-core/2026-06-20-semantic-fact-query-baseline.md)、[numeric range microcase](./01-semantic-core/2026-07-06-numeric-range-microcase-evidence.md)、[structured diagnostic](./02-diagnostics/2026-07-19-structured-diagnostic-baseline.md)、[position robustness](./03-robustness/2026-06-20-position-robustness-baseline.md)。

## 结构性缺口

1. analyzer内仍可能重复做compiler已有的scope/type/union/ownership判断，事实来源未完全唯一。
2. Canonical TypeRef、Place/loan/region、receiver effect、property ref-return尚未形成完整query contract。
3. import仍需从raw literal迁移到ModuleSpecifier/ModuleIdentity，package/alias/.zrm导航未统一。
4. diagnostics正在快速扩展，但registry、related ranges、fix safety与snapshot validity需稳定schema。
5. 旧计划按表达式微case追加大量日志，掩盖了foundation尚未闭合。

## 重写策略

保留已验证的query/position/diagnostic基础；停止在LSP内发展平行类型系统。新功能先进入compiler fact/query，再由LSP投影。尚未由facts支持的功能明确open，而不是用启发式hover冒充完成。
