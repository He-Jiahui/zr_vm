# ZR Language Server 重设计计划索引

> 状态：按syntax foundation原地重写。已有semantic query/diagnostic实现是baseline，不能替代统一事实层。
>
> 权威输入：[syntax 01](../syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md)、[syntax 06](../syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md)、[syntax 10](../syntax/2026-07-19-10-native-ffi-module-package-design.md)。

## 目标

Language Server成为Compiler Semantic Query的消费者：同一TypeId/SymbolId/PlaceId/CFG facts/ModuleIdentity驱动hover、definition、completion、diagnostic、rename、code action与debug expression。LSP不从token字符串重建语言语义。

## 计划

| 阶段 | 文档 | 交付 |
|---:|---|---|
| 0 | [当前状态](./00-current-state.md) | baseline与缺口 |
| 1 | [语义核心](./01-semantic-inference-core.md) | snapshot、query、display type |
| 2 | [诊断](./02-diagnostics-and-errors.md) | structured diagnostics/fixes |
| 3 | [位置健壮性](./03-lsp-robustness-and-position.md) | UTF位置、incremental snapshot |
| 4 | [debug/REPL](./04-debug-and-repl.md) | expression compile与DebugMap |
| 5 | [实施路线](./05-implementation-blueprint.md) | 分层里程碑与验收 |

## Syntax Contract 投影

| Syntax设计 | Language Server投影 | 主计划 |
|---|---|---|
| 01 Type/Symbol/Place/CFG/artifact | revision-scoped semantic query与binary facts | 01、03 |
| 02 ref/in/out/scoped/readonly | hover/signature、borrow/out/escape diagnostics | 01、02 |
| 03 struct/ref struct/Span/layout | layout/ref-like展示、bounds/range facts | 01、02 |
| 04 resource/owner/Drop/GC bridge | move/drop/bridge诊断与owner视图 | 01、02 |
| 05 property | 单一PropertySymbol、accessor navigation、ref-return effect | 01、02 |
| 06 migration | `%xxx` code action、formatter/token切换与幂等edit | 02、05 |
| 07 reference fixture | hover/diagnostic/action golden | 05 |
| 08 reflection | typeof/typeid/TypeOf/createInstance的精准display | 01、02 |
| 09 pooling | PoolHandle/PoolRef有效性、guard和逃逸提示 | 01、02 |
| 10 native/module/package | ModuleSpecifier、包exports、`.zrm`、FFI diagnostics/navigation | 01、02、03、05 |

## Feature 完成定义

一个LSP feature只有在compiler query提供稳定fact、source/binary/native provider结果一致、UTF-16 range正确、incremental snapshot失效正确、protocol JSON与取消路径通过后才完成。LSP本地启发式结果只能标为fallback debt，不能晋级。

## 共同门槛

- query结果带snapshot/schema/generation，不能跨编辑版本混用。
- source与binary/native module导航共享ModuleIdentity。
- display grammar严格使用目标`fn :`、callable`->`、`init/new/own`、`#alias/@package`。
- legacy `%xxx`只显示migration diagnostic/code action。
- 完成证据写入`plan-id/detail.md`，正文不再追加微切片日志。
