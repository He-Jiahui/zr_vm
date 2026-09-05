---
related_code:
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_protocol_client.js
  - tests/language_server/stdio_workspace_folders_smoke.js
  - tests/language_server/stdio_save_capabilities_smoke.js
implementation_files: []
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_workspace_folders_smoke
  - language_server_stdio_save_capabilities_smoke
doc_type: milestone-record
---

# Current Protocol Conformance Replay

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证结果 |
| --- | --- | --- | --- | --- |
| 2026-09-06 05:20 +08:00 | 2026-09-06 05:24 +08:00 | completed (replay evidence; parent pending) | 在当前隔离 MSVC 二进制上重放协议负向、工作区 folder、保存通知及相邻 capability smoke，并补充 GCC/Clang 协议重放。 | GCC/Clang/MSVC 的 protocol conformance 各 30/30；MSVC workspace folders 12/12、save 6/6、optional 25/25、resolve、file-operation、client-command smoke 均通过。 |

## Replay Scope

本子项只冻结当前已提交能力契约的运行时证据，不修改协议实现。协议
driver 的 30 个案例覆盖初始化顺序、重复 initialize、shutdown/exit、JSON-RPC
信封和参数、typed request id、重复 id、known/unknown cancellation、trace
隔离、work-done/partial results、oversize frame 和 malformed frame 分类。

工作区 smoke 额外验证 multi-root 优先级、folder add/remove、opened overlay
保留、watched-file 过滤以及项目 rename；save smoke 验证普通与 save-aware
client capability、willSaveWaitUntil、didSave 磁盘刷新和版本递增。

## Verification

```text
wsl.exe --exec node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js /home/hejiahui/.codex-builds/lsp-optimize-20260905-root/gcc/bin/zr_vm_language_server_stdio
wsl.exe --exec node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js /home/hejiahui/.codex-builds/lsp-optimize-20260905-root/clang/bin/zr_vm_language_server_stdio
node tests/language_server/stdio_protocol_conformance.js E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc/bin/zr_vm_language_server_stdio.exe
node tests/language_server/stdio_workspace_folders_smoke.js E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc/bin/zr_vm_language_server_stdio.exe
node tests/language_server/stdio_save_capabilities_smoke.js E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc/bin/zr_vm_language_server_stdio.exe
node tests/language_server/stdio_optional_capabilities_smoke.js E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc/bin/zr_vm_language_server_stdio.exe
node tests/language_server/stdio_resolve_capabilities_smoke.js E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc/bin/zr_vm_language_server_stdio.exe
node tests/language_server/stdio_file_operation_capabilities_smoke.js E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc/bin/zr_vm_language_server_stdio.exe
node tests/language_server/stdio_client_commands_smoke.js E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc/bin/zr_vm_language_server_stdio.exe
node --check tests/language_server/stdio_protocol_conformance.js
ctest.exe --test-dir E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc --output-on-failure -R "^language_server_(wasm_capability_inventory|stdio_protocol_inventory|stdio_protocol_conformance|stdio_save_capabilities_smoke|stdio_workspace_folders_smoke)$"
git diff --check -- docs/plans/lsp/optimize/2026-09-06-plan00-task03-sub03-protocol-replay.md
```

The GCC and Clang binaries are the frozen WSL builds used by the preceding
cancellation leaf. The MSVC binary is the current isolated Debug static build;
its CMake cache records `D:/Tools/development/cmake/bin/ctest.exe` and the
Visual Studio compiler. All direct runners exit zero and protocol stdout stays
free of trace or diagnostic text. The native Windows CTest replay passes all five
selected tests, including both inventory runners.

## Acceptance Boundary

This replay supplies current runtime evidence for the protocol corpus and the
existing workspace/save capability leaves. It does not claim the Plan 00 Task 3
parent complete: lifecycle fault injection, sanitizer/Valgrind replay, active
query cancellation latency, linked WASM exports, native/Web golden parity and
the integrated semantic baseline remain separate gates.
