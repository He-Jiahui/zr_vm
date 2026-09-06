---
related_code:
  - zr_vm_language_server/stdio/stdio_request_registry.c
  - zr_vm_language_server/stdio/stdio_request_registry.h
  - tests/language_server/test_stdio_server_lifecycle.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 4 Sub04: Request Registry Identity

## 状态与产出记录

- 开始时间: 2026-09-07 04:22 +08:00
- 实际完成时间: 2026-09-07 04:24 +08:00
- 状态: 已完成
- 源码版本: 基于 `596b01e3` 的当前工作树；本记录与回归测试由同一阶段提交固化
- 产出路径: `tests/language_server/test_stdio_server_lifecycle.c`、模块文档、计划索引与本记录

本子项为 request registry 增加直接的类型和值语义回归。它验证活动请求 ID 的
重复预留、数字与字符串身份隔离、精确取消和完成后复用；不宣称请求并发模型、
active query cancellation latency、ContentModified fence 或统一 progress sink 已验收。

## 覆盖边界

测试确认：

- 数字 `1` 和字符串 `"1"` 是不同 registry key，各自可独立预留；
- 同类型同值的活动 ID 第二次预留返回 `DUPLICATE`；
- `$/cancelRequest` 只标记精确匹配的活动 ID，未知 ID 和 bool/结构化 ID 不改变状态；
- `Complete` 删除活动项，随后重新预留得到全新的未取消记录；
- registry 只接受 null/number/string 三种 JSON-RPC ID 形状，其他形状返回失败。

## 验证命令及结果

```text
wsl.exe bash -lc 'cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --target zr_vm_language_server_stdio_server_lifecycle_test --parallel 8'
  passed
wsl.exe bash -lc '/mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio_server_lifecycle_test'
  Pass - stdio server lifecycle

wsl.exe bash -lc 'cmake --build /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current --target zr_vm_language_server_stdio_server_lifecycle_test --parallel 8'
  passed
wsl.exe bash -lc '/mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio_server_lifecycle_test'
  Pass - stdio server lifecycle
```

GCC 与 Clang ASan/UBSan 均通过同一生命周期目标；该目标同时保留 100 次
New/Start/Shutdown/Free 和启动 fault-injection 回归。`git diff --check` 对本子项
源文件和文档无空白错误。

## 接受决定

接受 Plan 01 Task 4 Sub04。registry 的精确 ID key 和 cancellation 状态现在有独立
C 回归证据；Task 4 的请求注册与执行线性化、活动查询取消、ContentModified 和
统一 progress sink 仍需后续验收。
