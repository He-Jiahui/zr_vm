# Syntax 12 M6.3: Debug Projection and Fault Semantics

## 状态与产出记录

- 状态：`completed`
- 开始时间：2026-07-25 23:30 +08:00
- 完成时间：2026-07-26 00:15 +08:00
- 完成项目：
  - 新增 `SZrDebugAsyncSchedulerContract`，从 compiler-owned
    `SZrFunctionSchedulerSourceFact` 或 `SZrArtifactSchedulerContractRow`
    投影同一 canonical Scheduler/Task/Job contract。投影只接受 TypeDef/TypeRef
    provider token、exact schedule member/signature token、ABI、policy、requirement
    和 nonzero contract hashes；缺失或错误 token table 的事实保持 unavailable。
  - `ZrDebugFrameSnapshot` 和 `zrdbg/1` `stackTrace` 仅发布已投影的
    `asyncContract`，不以 module/function display name、source range、AST 或错误文本
    重建身份。64-bit hashes 固定为 16 位十六进制字符串，避免 JSON double 精度丢失。
  - 新增 TaskFrame terminal projection：AttachedDomain/IsolatedDomain 的 completed
    和 faulted terminal 分别保留；faulted provenance 只能由 policy rejection、transport
    prepare/decode/commit、cancellation、shutdown 或 Job throw enum 传入。普通无分类
    fault 保持 `none`，不解析 runtime error message。
  - traceback lower-layer 覆盖 source/artifact contract parity、token validation、
    attached completion 以及七个 M4/M5 fault provenance；debug-agent protocol 覆盖
    活跃帧 `asyncContract` 的稳定 JSON projection。
  - 已更新 `zrdbg/1` debugger workflow 文档，并修正实施计划中不存在的
    `docs/debugging-and-observability/` 路径到维护中的
    `docs/cli-and-tooling/zr-debugger-v1-launch-workflow.md`。
- 验收：
  - GCC 11.4、Clang 14.0、MSVC 17.14 在独立 M6.3 构建目录中均通过
    `zr_vm_debug_traceback_test` 5/5、`zr_vm_debug_agent_protocol_test` 5/5、
    `zr_vm_thread_runtime_test` 25/25；九个测试进程均真实 exit 0。
  - `zr_vm_thread_runtime_test` 日志中的 `Unique value is used after it was moved`
    是该回归主动断言的编译期拒绝；每个平台 Unity 汇总均为 25 Tests、0 Failures。
