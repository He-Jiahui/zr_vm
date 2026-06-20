---
doc_type: plan-phase
phase: 5
title: DAP 调试代理增强与修复
related_code:
  - zr_vm_lib_debug/src/debug.c
  - zr_vm_lib_debug/src/debug_protocol.c
  - zr_vm_lib_debug/src/debug_snapshot.c
  - zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h
  - zr_vm_language_server_extension/src/debug/dapSession.ts
  - zr_vm_language_server_extension/src/debug/zrdbgClient.ts
---

# Phase 5 — DAP 调试代理增强与修复

在内核自省 API（P1/P2）与 traceback（P3）就绪后，提升 DAP 代理的健壮性与覆盖面。

## 5.1 复用内核 API（去重）

把 `debug_snapshot.c` 的栈/局部/upvalue 遍历替换为 Phase 2 的内核 API（详见 02 文档 §2.4）。
这是本阶段的「正确性基线」改动，应最先做。

## 5.2 修 step 语义边界

现有 step in/out/over 基于帧深度。需核查并加固以下边界：
- **尾调用**：尾调用复用栈帧（`callStatus & ZR_CALL_STATUS_TAIL_CALL`），step over 不应把被
  尾调用的函数误判为「同帧继续」。以 callInfo 身份而非纯深度判定目标帧。
- **native 帧穿越**：step in 进入 native 函数时无行信息，应表现为 step over（Lua 同此）。
- **异常 unwind 期间 step**：step out 的目标帧若被异常提前弹出，需有「目标帧消失→回退为
  下一个停靠点或 continue」的兜底，避免永久挂起。
- **递归同行**：同一行递归调用时 step over 必须真正越过整个子调用（用「目标帧深度 + 行变化」
  双条件）。

每条边界配独立用例（见 §5.7）。

## 5.3 缓冲区截断治理

`ZR_DEBUG_TEXT_CAPACITY=256` / `ZR_DEBUG_NAME_CAPACITY=128`（`debug.h`）静默截断。方案：
- 对超长值/路径：截断时追加省略标记（`…` 或 `...[+N]`），让客户端能识别「被截断」。
- 对 `evaluate`/变量值这类可能很长的文本：协议层改为**按需分页/惰性展开**（DAP 已支持
  `supportsVariablePaging`，扩大到长字符串值的分段拉取）。
- 路径与符号名优先保留尾部（文件名/末段更有信息量）。

> 不盲目放大固定数组（会撑大每帧快照内存）。优先「标记截断 + 惰性拉全」。

## 5.4 task / 协程多执行体调试

现状只暴露 thread id=1。zr 有 task/thread 库。目标：
- DAP `threads` 请求枚举所有活跃 zr 执行体（每个 `SZrState`），稳定映射 state→threadId。
- stackTrace/scopes/variables 带 threadId 路由到对应 state。
- 停止事件标注是哪个 thread 命中断点。
- **MVP 边界**：先支持「枚举 + 各自栈检查」，跨 task 同步单步（如 step 切换执行体）列为后续。

## 5.5 data breakpoint / watchpoint（新增）

借助 Phase 2 的 `GetUpvalueId` 与变量地址：
- DAP `dataBreakpointInfo` + `setDataBreakpoints`：对某个变量/字段设「值变化即停」。
- 实现：在 LINE/COUNT hook 路径上比对被 watch 槽的当前值与上次快照，变化则触发 stop。
  （软件 watchpoint，成本随 watch 数线性增长，文档标注。）
- 初版限定**局部变量与 upvalue**；对象字段 watch 留后续。

## 5.6 远程调试鉴权（可选）

`auth_token`（`debug.h:45 ZrDebugAgentConfig`）已留字段但未启用。若放开非 loopback：
- 握手阶段校验 token（常数时间比较）。
- 文档明确「默认仍 loopback；远程需显式配置 + token」。
- `zrdbgClient.ts` 的 loopback 强制改为「非 loopback 时要求 token」。
- **范围控制**：本项为可选增强，无需求可不做，但字段与握手位先预留。

## 5.7 验收

- `tests/debug/test_debug_step_edges.c`：尾调用、native 穿越、异常 unwind、递归同行 四类用例。
- `tests/debug/test_debug_truncation.c`：超长值被标记截断且可分页拉全。
- `tests/debug/test_debug_threads.c`：多 task 场景 `threads` 枚举与按 thread 栈检查。
- `tests/debug/test_debug_data_breakpoint.c`：局部变量 watch 命中。
- 回归：`test_cli_debug_e2e.c`、`dapSession.ts` 现有行为不破坏。

## 5.8 风险

- §5.1 的内核 API 替换可能改变某些边角值的渲染（如 inline struct）；需对照旧快照做差异审查。
- 软件 watchpoint 在 hook 路径增加 per-instruction 比对，务必仅在存在 data breakpoint 时启用，
  避免拖慢无 watch 的会话。
