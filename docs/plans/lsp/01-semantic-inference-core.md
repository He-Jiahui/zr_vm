# LSP 01：Semantic Snapshot 与统一 Query

## 架构

```text
Document text/version
  -> parser SyntaxSnapshot
  -> compiler SemanticSnapshot
     Canonical types/symbols
     Place/CFG/dataflow facts
     bound calls/properties/construction
     ModuleIdentity graph
  -> stable SemanticQuery API
  -> LSP feature projection
```

LSP可协调增量计算与缓存，但不能拥有另一套类型规则。

## Query model

```text
QueryContext(documentId, snapshotId, position/range)
SymbolInfo(SymbolId, declaration, accessibility, moduleIdentity)
TypeInfo(CanonicalTypeId, TypeUse, displayContract)
PlaceInfo(PlaceId, mutability, availability, region, activeLoans)
CallInfo(targetSymbol, callableContract, receiverEffect, argumentBindings)
PropertyInfo(propertySymbol, get/set/refGet, referentMutability)
FlowInfo(blockId, reachability, narrowing, definiteAssignment)
```

所有结果携带snapshot/schema generation；文本变更后旧result不可用于edit。

## Display rules

- definition显示`fn name(...): R`；callable type显示`fn(A) -> R`；anonymous expression body使用`=>`。
- struct construction signature help只在`init T(`显示constructor；`T(`只查询call/`@call`。
- `typeof(expr)`显示最精确reflection descriptor；`typeid(T)`显示TypeId身份。
- property只显示一个symbol，并列get/set/ref-get accessibility/effect；不合成backing field。
- PoolHandle显示weak generational identity；PoolRef显示scoped direct ref/guard。
- import显示original spelling + Canonical ModuleId + package/version/provider。

## Incremental boundary

SyntaxSnapshot可以增量；SemanticSnapshot按dependency/SymbolId/ModuleIdentity失效。局部编辑不得复用已改变scope、generic substitution、CFG或module generation的facts。cache key不能仅用文件路径+offset。

## 完成记录

- [Semantic fact/query baseline](./01-semantic-core/2026-06-20-semantic-fact-query-baseline.md)
- [Numeric range microcase evidence](./01-semantic-core/2026-07-06-numeric-range-microcase-evidence.md)
- [Canonical source public-contract hash](./03-robustness/2026-07-20-canonical-source-public-contract-hash.md)
- [Resolved callable consumer convergence](./03-robustness/2026-07-20-resolved-callable-consumer-convergence.md)

这些记录证明query机制可用，并完成Q4中source resolved callable identity/display consumer与Q5中source module public-contract hash的首个canonical query；不表示全部Canonical/Place/Module facts或binary/native provider parity已覆盖。

## Query Schema 实施阶段

失败边界：snapshot/schema/generation不匹配、poisoned fact、binary metadata缺失、ModuleIdentity漂移和query stage未完成必须返回结构化unavailable/error，禁止复用过期结果或伪造unknown-as-any。

1. **Q1 identity/context**：统一DocumentId/SnapshotId/SyntaxId/SymbolId/TypeId/PlaceId/BlockId/ModuleIdentity；每个query显式声明所需阶段和poison/recovery状态。
2. **Q2 declaration/type**：symbol、canonical/display type、generic substitution、accessibility、receiver effect和origin ranges。
3. **Q3 place/flow**：mutability、initialization、availability、region、active loans、narrowing、reachability和range facts。
4. **Q4 call/property/construction**：argument binding、passing marker、target callable、property get/set/ref-get、`init/new/own/call/createInstance`分类。
5. **Q5 module/artifact**：specifier spelling、Canonical ModuleId、package/version/provider、public contract hash和source/metadata location。
6. **Q6 binary/native parity**：`.zri/.zro/.zrm`与native descriptor提供相同query shape；缺少local-only fact时返回明确availability，不伪造源码结论。

测试从`tests/parser/test_semantic_query.c`、`tests/language_server/test_lsp_local_semantic_query.c`、`test_lsp_call_member_semantic_query.c`和`test_lsp_reachability_semantic_query.c`开始。目标用例覆盖ref/out、property ref-return、owner move、reflection TypeOf、pool guard、package alias和stale module generation。

退出条件：所有feature通过同一query API；查询结果对snapshot严格有效；source/binary/native同一公开symbol拥有同一canonical identity/display contract；LSP中删除按token/type-name重复推断路径。
