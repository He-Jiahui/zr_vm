# Debug 05：DAP Adapter

## Adapter职责

DAP层只做协议/session映射：initialize/capabilities、launch/attach、breakpoint、continue/step、stackTrace/scopes/variables、evaluate、modules、loadedSources、exceptionInfo、disassemble/memory（按capability）。执行语义由core debug service提供。

## Identity映射

- DAP threadId/frameId/variablesReference是session-scoped opaque id，映射到带generation的core handle。
- source使用URI/path + checksum + ModuleIdentity；`.zrm`/package/native metadata可提供virtual source。
- breakpoint response区分verified、pending module load、source mismatch与invalid location。
- module event显示Canonical ModuleId、package/version、provider、artifact generation和symbol availability。

## Evaluate

watch/hover/clipboard/REPL context传给共享DebugEvaluationContext。默认hover不产生side effect；用户显式REPL可以按policy执行。编译/运行错误映射为structured response，不把target exception当adapter崩溃。

## 稳定性

覆盖multi-session、disconnect/terminate、process exit、late events、request cancellation、large variable paging、resume后stale handle、module reload和network partial frame。adapter不得在锁内等待target回调造成死锁。

## 完成记录

[2026-06-22 DAP agent baseline](./05-dap/2026-06-22-dap-agent-baseline.md) 证明已有协议路径；ModuleIdentity、typed values和四backend parity仍需扩充。

## Request/State 验收矩阵

| 请求组 | Core输入 | 必测状态 |
|---|---|---|
| initialize/launch/attach | capabilities、target/provider policy | unsupported feature、launch failure、attach race |
| setBreakpoints/configurationDone | source checksum、DebugMap binding | pending module、reload、invalid line |
| continue/next/stepIn/stepOut/pause | pause/resume generation与step plan | late event、thread exit、exception edge |
| stackTrace/scopes/variables | Frame/Value descriptors、paging | stale reference、large children、optimized out |
| evaluate/setVariable | DebugEvaluationContext/effect policy | readonly/owner/ref/native capability denial |
| modules/loadedSources | ModuleIdentity/provider/artifact generation | package/`.zrm`/native/trimmed source |
| exceptionInfo/disassemble/memory | Traceback/instruction map/memory capability | invalid range、hidden address、artifact mismatch |

验证入口：`tests/debug/test_debug_agent.c`、`test_debug_agent_protocol.c`、`test_debug_data_breakpoint.c`、`test_debug_truncation.c`和debug phase5 acceptance。协议测试必须断言request/response/event顺序、cancel、disconnect/terminate、target crash、partial transport frame和multi-session隔离。

退出条件：所有DAP id均session-scoped且generation-safe；resume后variablesReference稳定失败；adapter不在持锁状态等待target callback；source/binary/AOT provider返回一致logical frame与module信息。
