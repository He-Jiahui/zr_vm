# ZR Debug、DAP 与 Profiling 重设计计划索引

> 状态：按syntax/AOT/LSP contract原地重写；现有debug能力作为baseline保留。

## 架构边界

```text
VM/AOT execution hooks + DebugMap
  -> core debug service (pause/step/frame/value)
  -> zr.debug script facade
  -> DAP adapter / CLI / profiler
```

debugger不定义类型、module或ownership语义。它消费Canonical TypeId、SymbolId、Place/region facts、ModuleIdentity、artifact generation和source checksum。

## 文档

1. [core hooks](./01-core-hook-fixes.md)
2. [introspection](./02-introspection-api.md)
3. [traceback/errors](./03-traceback-and-errors.md)
4. [`zr.debug`](./04-script-debug-library.md)
5. [DAP](./05-dap-agent-enhancements.md)
6. [profiling/tooling](./06-profiling-and-tooling.md)
7. [testing/acceptance](./07-testing-and-acceptance.md)

## Syntax/AOT Contract 投影

| 上游设计 | Debug投影 | 主计划 |
|---|---|---|
| syntax 01 Type/Place/CFG/artifact | DebugMap、scope/value location、availability | 01、02 |
| syntax 02 ref/readonly/region | ref validity、readonly与escape可视化 | 02 |
| syntax 03 struct/ref struct/Span | inline/fat-ref布局与optimized location | 02、06 |
| syntax 04 resource/owner/Drop | owner state、cleanup与异常trace | 02、03、06 |
| syntax 05 property | accessor breakpoint/navigation；默认不执行getter | 01、02 |
| syntax 06 migration | legacy source map与目标token展示 | 04、07 |
| syntax 07 reference fixture | 四backend统一debug scenario | 07 |
| syntax 08 reflection | TypeId/member metadata与trimmed visibility | 02、04 |
| syntax 09 pooling | handle generation、guarded PoolRef与事件 | 02、06 |
| syntax 10 module/package/native | ModuleIdentity、`.zrm` source、native boundary | 01、03、05 |

## 子里程碑必填证据

每个debug能力必须证明VM/AOT sequence-point parity、暂停态generation安全、resume后的stale-handle失败、trim/source checksum行为、DAP/CLI投影与关闭功能后的overhead。仅在解释器或单一未优化构建可用不能宣称完成。

## 共同约束

- interp、binary-first、AOT C、AOT LLVM使用同一DebugMap schema。
- 暂停、frame、variable reference均带environment/module generation，过期引用稳定失效。
- evaluator使用compiler semantic query，不提供绕过borrow/readonly/native policy的后门。
- `zr.debug`统一替代当前裸`debug` ModuleId。
- 完成记录写入`plan-id/detail.md`，正文不追加执行流水账。
