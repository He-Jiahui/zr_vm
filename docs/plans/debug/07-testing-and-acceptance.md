# Debug 07：测试与验收

## 测试矩阵

| 层级 | 重点 |
|---|---|
| core unit | hook顺序、pause/resume、frame/value generation、trace结构 |
| artifact | DebugMap/stack map/source checksum/module identity roundtrip |
| VM/AOT | sequence point、break/step、exception、optimized/inlined frame parity |
| `zr.debug` | exports/metadata/capability/error与裸`debug`迁移 |
| DAP | protocol lifecycle、breakpoints、variables、evaluate、modules、stale handles |
| profiling | event schema、symbolization、loss/backpressure、overhead |
| concurrency | thread/task、GC safepoint、disconnect、late event、deadlock |

## 必须场景

1. formatted与minified semicolon source产生同一sequence point semantics。
2. source、binary-first、`.zrm`、package和native module都可定位。
3. struct/class/ref/ref struct/owner/union/generic/property/PoolHandle/PoolRef均有typed inspection。
4. return/throw/finally/using/Drop与async causality产生正确trace。
5. edit/rebuild/reload后旧breakpoint重新绑定、旧frame/value稳定失效。
6. invalid UTF/position、large children paging、cancellation和target crash不导致adapter崩溃。
7. release trimming后只显示preserved metadata，debugger不通过残余symbol绕过可见性。

## Acceptance gate

- 四backend通过共享scenario/golden。
- DebugMap schema、DAP JSON和profiler output有版本兼容测试。
- 无backend-specific类型名/模块字符串推断。
- disabled hooks的性能预算达标；enabled overhead有可复现实测。
- 文档只把有证据的scope标为完成，证据放在对应plan-id目录。

## Promotion 套件

输入是syntax reference fixture、四backend artifacts、DebugMap/source checksums、DAP scenarios、capability policies和性能baseline。失败条件包括logical identity/trace/value不一致、stale handle仍可用、provider无法定位、trim泄漏metadata、协议乱序/死锁以及overhead超预算。

1. **Leaf**：core hook、DebugMap reader、frame/value/trace/event schema与generation单测。
2. **Runtime**：interp、binary-first、AOT C、AOT LLVM运行同一debug scenario，比较logical events/frames/values而非机器地址。
3. **Protocol**：DAP initialize→launch/attach→configure→stop→inspect/evaluate→resume→disconnect完整会话，含cancel/late event。
4. **Provider**：source、`.zro`、`.zrm`、`@package`、`zr.xxx` native module的breakpoint/source/module/trace。
5. **Language categories**：struct/ref struct/Span、property/ref return、owner/GC bridge、union/generic/reflection、PoolHandle/PoolRef和native extern。
6. **Optimization/trim**：inlining、tail call、register/spill/optimized-out、metadata preserve/trim与source checksum drift。
7. **Stress/security**：multithread/task、GC compaction、reload/unload、large paging、forged/stale handles、capability denial、transport loss。
8. **Performance**：off/hooks/coverage/sampling/full instrumentation分别记录吞吐、allocation、memory、event loss和p95 latency。

每个scenario的golden必须由stable Type/Symbol/Module identity组成，不保存机器地址、绝对临时路径或非确定时间。完成记录列出backend/target/build mode、命令、通过数、未运行范围和non-claims；单一DAP smoke不能晋级整个debug计划。

## 完成记录

[Profiling/tooling baseline](./06-profiling/2026-06-22-profiling-tooling-baseline.md)提供旧acceptance入口；syntax 01-10与四backend的完整promotion套件仍未完成。
