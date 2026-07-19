# Debug 04：`zr.debug` Script Facade

## Module identity

标准脚本调试库的Canonical ModuleId是`zr.debug`。当前native descriptor与type-hint中的裸`debug`迁移为`zr.debug`；兼容期只提供明确诊断/manifest migration，不永久双注册。

## Surface categories

- breakpoint/tracepoint与pause/resume请求。
- current thread/task/frame/scope snapshot。
- structured traceback与exception hook。
- coverage/profile session控制和结果读取。
- debug logging/assertion与可选evaluate bridge。

API返回typed result/descriptor，不以任意object map作为唯一contract。涉及外部状态、权限或暂停态的调用必须返回显式错误。

## 能力与安全

- release构建可按capability禁用mutation/evaluate/coverage，不改变module identity。
- script不能伪造FrameId/ValueHandle/ModuleIdentity；opaque handle带generation。
- evaluate使用compiler+LSP共享语义路径，遵守readonly/ref/resource/native policy。
- coverage/profile instrumentation与debug hooks共享sequence point，但分别计数，关闭时hot path成本可测。
- `zr.debug`通过registered native module descriptor导出，metadata schema与runtime exports必须一致。

## 完成记录

[2026-06-21 script debug library baseline](./04-zr-debug/2026-06-21-script-debug-library-baseline.md) 记录裸`debug`现有能力；ModuleId迁移和typed contract是新工作。

## Module/API 迁移阶段

| 阶段 | 交付 | Gate |
|---|---|---|
| D1 descriptor convergence | descriptor/hint/lookup统一`zr.debug` ModuleId | bare `debug`只触发migration diagnostic |
| D2 typed exports | snapshot/frame/trace/coverage/profile/result descriptors | runtime exports与metadata hints hash一致 |
| D3 capability policy | inspect/evaluate/mutate/coverage/profile分权 | sandbox/release拒绝高权限调用 |
| D4 service lifecycle | per-runtime/module generation、reload/unload与cache | stale/forged handle fail closed |
| D5 consumers | DAP/CLI/script共享core service | 不复制debug语义或格式化字符串解析 |

现有bare module代码是迁移输入。目标测试必须验证`let debug = import("zr.debug");`、old spelling诊断、source/binary/native module identity、exports/reflection metadata一致、capability denied和runtime replacement。

退出条件：仓库current fixture无裸`debug` import；module loader不保留永久双注册；opaque handles不能由脚本伪造；关闭debug capability时业务模块不依赖`zr.debug`且无额外hook分配。
