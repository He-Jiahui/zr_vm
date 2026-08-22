# Debug 06：Profiling、Coverage 与 Tooling

## 统一事件，不统一指标

execution sequence point、allocation、GC、call、native boundary、task/thread与ownership/pool事件进入versioned event stream；coverage、sampling、instrumentation profiler分别消费。不要把所有数据塞入debug callback或单一counter。

## 指标

- CPU：sample/instrumented call、inclusive/exclusive time、inline chain。
- memory：allocation count/bytes、live/peak、GC root/scan bytes、barrier/card、pause。
- ownership：Unique construct/move/drop、Shared retain/release、Weak wake。
- pooling：deliver/validate/reject/retire/deferred reuse、active guard、slab locality。
- codegen：boxing、value construction、thunk、spill、bounds-check elimination。
- module/native：load/resolve/cache、FFI call/marshal/callback与failure。

## Symbolization与输出

事件保存ModuleIdentity、function SymbolId、instruction/sequence point和artifact generation，离线工具用DebugMap symbolicate。输出schema versioned，支持stream/chunk/backpressure；丢事件必须有计数，不能悄悄产生精确外观的错误报告。

## Overhead

off、hooks-only、sampling、coverage、full instrumentation分别有预算。关闭功能时不应保留每语句分配或字符串格式化。benchmark同时测interp/AOT和debug/release。

## 完成记录

[2026-06-22 profiling/tooling baseline](./06-profiling/2026-06-22-profiling-tooling-baseline.md) 记录已有profile/coverage能力；新owner/pool/module事件与schema仍需接入。

## Event Schema 与预算

输入来自versioned execution/GC/owner/pool/module/native事件和DebugMap；profiler不得扫描runtime私有对象来猜事件语义。

每个event包含schema version、timestamp/order domain、thread/task、ModuleIdentity、SymbolId/instruction、artifact generation和typed payload；未知required payload拒绝，optional field可跳过。sampling和instrumentation时间语义分开，不能合并为同一精度声明。

第一版overhead gate：debug/profile全关时reference benchmark吞吐回退不超过2%、每事件零heap分配；hooks-only不超过5%；coverage不超过20%；full instrumentation单独报告不设成业务默认。事件buffer有固定上限与backpressure策略，丢失时递增loss counter并在输出中标记incomplete。

验证入口：`tests/acceptance/2026-06-22-debug-phase6-coverage.md`、profile/disassembly/heap-summary acceptance与core profile tests。新增owner/pool/GC/module/native事件的schema roundtrip、offline symbolization、buffer overflow、clock/order、module reload和trimmed DebugMap case。

退出条件：四backend事件能由同一工具symbolicate；指标按allocation/GC/owner/pool/codegen/module/native分类；关闭功能达到预算；输出schema有version/corruption/partial-stream测试。
