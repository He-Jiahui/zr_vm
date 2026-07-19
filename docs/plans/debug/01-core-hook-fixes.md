# Debug 01：Core Execution Hooks

## 事件模型

```text
DebugLocation(moduleIdentity, functionSymbol, instructionId, sourceRange)
ExecutionEvent(kind, thread/task, frameId, location, generation)
PauseState(reason, stoppedEntities, snapshotGeneration)
```

hook点至少覆盖function enter/exit、statement/sequence point、throw/catch、safepoint、task/thread transition和module load/unload。VM/AOT发出相同逻辑事件；backend-specific PC只通过DebugMap映射。

## Step/breakpoint

- breakpoint先绑定source checksum + Symbol/sequence point，再解析backend address。
- line、function、conditional、hit-count、exception breakpoint共享稳定identity。
- step in/over/out基于logical frame与sequence point，不按C行号或bytecode offset猜。
- optimized/inlined frame必须通过inline map重建；无法精确时明确标记而非返回错误scope。
- hot reload/module reload改变generation，旧breakpoint重新绑定，旧frame/value reference失效。

## 并发

pause policy明确all-stop或single-thread/task；事件排序、resume token和cancellation可验证。GC safepoint与debug pause协调，不能在不可枚举root的native临界区强停。

## 完成记录

[2026-06-20 core hook baseline](./01-core-hooks/2026-06-20-core-hook-baseline.md) 证明现有hook基础；AOT parity、generation和新DebugMap仍需收敛。

## 实施阶段与验证

输入是VM/AOT logical instruction events、DebugMap、ModuleIdentity/source checksum、thread/task state和GC safepoint capability；机器PC本身不能作为公开identity。

| 阶段 | 交付 | 关键失败 |
|---|---|---|
| H1 event schema | sequence point/function/throw/safepoint/task/module事件与generation | unknown event/version、乱序resume token |
| H2 DebugMap binding | source checksum、ModuleIdentity、SymbolId、instruction/PC、inline chain | source/artifact mismatch、trimmed map |
| H3 breakpoint binding | line/function/exception/conditional/hit-count与pending状态 | invalid location、module未加载/重载 |
| H4 stepping | logical frame与sequence-point step in/over/out | inline/tail/exception/async边界 |
| H5 concurrency | all-stop/single-thread policy、GC safepoint协调 | native critical region、late event、deadlock |

测试从`tests/debug/test_debug_hook_core.c`、`test_debug_step_edges.c`、`test_debug_threads.c`和`tests/acceptance/2026-06-20-debug-phase1-core-hooks.md`开始。目标新增同一scenario的interp、binary-first、AOT C、AOT LLVM地址映射与optimized inline版本。

退出条件：相同source sequence point在四backend拥有同一logical identity；reload后旧breakpoint重绑、旧pause token失效；GC/native临界区停止策略可证明无死锁；事件关闭时hot path不开字符串/对象分配。
