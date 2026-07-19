# Debug 03：Traceback、Exception 与 Async Causality

## Structured trace

```text
Traceback {
  exceptionTypeId;
  message;
  frames[];
  cause/suppressed[];
  asyncLinks[];
  nativeBoundary?;
}

TraceFrame {
  moduleIdentity;
  functionSymbol;
  sourceRange;
  instructionId;
  inlineChain[];
  generation;
}
```

格式化字符串是view，不是唯一数据源。DAP、CLI、`zr.debug`和crash report共享structured trace。

## 语义

- throw/catch/rethrow保留原exception identity与cause。
- finally/using Close/Drop失败按统一suppression/aggregation policy记录。
- async/await、task spawn与thread handoff通过causal link连接，不伪造成同步stack frame。
- native extern边界记录library/entry/ABI和可用native frame，但不泄漏无效pointer。
- optimized/inlined AOT frame由DebugMap恢复；artifact/source checksum mismatch明确标记。
- loader/package错误携带ModuleSpecifier、Canonical ModuleId/package/provider阶段。

## 完成记录

[2026-06-20 traceback baseline](./03-traceback/2026-06-20-traceback-baseline.md) 是现有异常/trace证据；async causality、native/module identity与cleanup组合仍需实现。

## 构建与验收

输入包括当前/异步frame chain、structured exception/cause、cleanup failure、DebugMap、ModuleIdentity与native boundary descriptor；缺失symbol不能使原始frame丢失。

1. **T1 frame capture**：从VM frame/AOT unwind/DebugMap生成结构frame，保留inline chain、module generation和source checksum。
2. **T2 exception graph**：保留cause、suppressed、rethrow identity及Drop/Close cleanup failure，不把字符串拼接当结构。
3. **T3 async causality**：task spawn/await/thread handoff记录causal link，设置深度/循环/丢失策略。
4. **T4 native/module boundary**：native extern callback、loader/package failure记录library/entry/ABI与resolver阶段，同时隐藏敏感地址。
5. **T5 formatting/transport**：CLI、DAP、`zr.debug`从同一Traceback投影；截断明确标记且原结构可分页。

验证入口：`tests/debug/test_debug_trace.c`、`test_debug_traceback.c`、`tests/acceptance/2026-06-20-debug-phase3-traceback.md`。新增nested cause/suppressed、finally/Drop/Close双异常、async cycle、optimized inline、native callback、missing `.zrm` export与source mismatch。

退出条件：四backendframe identity和异常分类一致；formatter变化不改变协议数据；无悬空module/runtime pointer；trace截断、metadata缺失与symbolization失败均显式可诊断。
