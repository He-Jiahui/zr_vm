# Using 04：Union 与 Pattern，不再借用 `using`

## 目标表层

```zr
if let Result.Ok(value) = result {
    consume(value);
}

switch (result) {
    (Result.Ok(value)) { consume(value); }
    (Result.Error(message)) { report(message); }
}
```

`if let`用于单分支解构，`switch`用于多分支/exhaustiveness。pattern binding是CFG refinement，不是resource guard。

## Semantic facts

- scrutinee只求值一次。
- variant test产生narrowed union state和payload Place projection。
- binding的mutability、borrow与move按`let/var`和payload TypeRef计算。
- branch join合并availability、initialization、loans和reachability。
- move-only payload只能在覆盖路径中消费一次。
- exhaustiveness/redundancy由union declaration token/closed variant set决定。

## Layout/metadata

union layout保存tag representation、payload layouts、GC/ownership maps和variant tokens。AOT/VM/reflect/LSP共享这些数据，不按variant名字字符串识别。

## 完成记录

[2026-06-18 union layout/metadata baseline](./04-unions/2026-06-18-union-layout-metadata-baseline.md) 可复用布局与metadata基础；`if let/switch` CFG facts和新diagnostics仍按syntax计划实施。

## Pattern 实施与验收

1. parser为variant/payload/wildcard/binding生成独立pattern AST，不复用using/resource节点。
2. binder把variant token、generic substitution和payload projections绑定到Canonical Type/Place。
3. CFG在success/failure edge记录narrowing、reachability、binding initialization和owner move；join不泄漏分支binding。
4. exhaustiveness/redundancy使用closed variant set与guard可达性；open/dynamic union必须有default policy。
5. VM/AOT按tag/layout contract读取payload，reflection/debug/LSP使用同一variant token。

测试入口：`tests/parser/test_union.c`、`tests/parser/test_cfg_union_exhaustiveness.c`、`tests/language_server/test_union_pattern_diagnostics.c`、`tests/acceptance/2026-06-17-union-types.md`。必须增加move-only payload、nested generic/nullable/ownership union、unreachable arm、missing arm和malformed tag artifact。

退出条件：`if let`与`switch`不依赖using语义；scrutinee只求值一次；payload Place/Drop在所有edge正确；source/binary/AOT/LSP variant identity一致。
