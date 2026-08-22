---
related_code:
  - zr_vm_language_server/stdio/stdio_server.h
  - zr_vm_language_server/stdio/stdio_server.c
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
implementation_files:
  - zr_vm_language_server/stdio/stdio_server.c
  - zr_vm_language_server/stdio/stdio_transport.c
plan_sources:
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - tests/language_server/test_stdio_server_lifecycle.c
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_protocol_conformance.js
doc_type: milestone-detail
---

# LSP Protocol Task 5 Deterministic Teardown

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 00:16 +08:00 | in_progress | 已完成 stdio server 的确定性 reader 停止、join、输入队列/registry/cache/context/global 有序释放实现、构造和 reader-start fault injection，以及支持层 long-string 对齐、global object/container teardown。 | GCC layout + lifecycle direct 均退出 `0`；Clang ASan core layout direct 退出 `0` 且无 leak。启用 LeakSanitizer 的 100-cycle lifecycle 仍 SIGSEGV，Task 5 未验收。 |
| 2026-08-23 01:35 +08:00 | in_progress | Valgrind 已完成 100-cycle lifecycle 的真实释放审计；GCC 重新构建后重放 stdio lifecycle、smoke、protocol inventory 和 protocol conformance。 | Valgrind: `54,335 allocs / 54,335 frees`、`0 errors`、`0 bytes in use`；GCC CTest `4/4` passed。Clang direct lifecycle 现可通过 LSan，但 Node-spawned protocol child 仍不稳定，且 sanitizer smoke 的 CLI fixture 暴露范围外 native metadata/AST leak；Task 5 继续未验收。 |
| 2026-08-23 01:37 +08:00 | in_progress | 完成 reader/main-thread 并发审计。 | Helgrind lifecycle loop: `0 errors from 0 contexts`；未发现 Task 5 输入队列、join 或 teardown 共享状态的数据竞争。Clang Node-spawn sanitizer 问题仍待隔离，未标记完成。 |
| 2026-08-23 03:27 +08:00 | completed | 完成 Task 5 确定性 teardown 与 Task 6 验证门禁：100-cycle lifecycle、四个启动故障点、reader join、协议负向/stdout 隔离、GCC/Clang sanitizer、Valgrind/Helgrind、MSVC Debug/ASan。 | Clang ASan+UBSan、GCC ASan+UBSan 均通过 lifecycle/smoke/protocol inventory/protocol conformance `4/4`；GCC release 默认 512 MiB budget 同一集合 `4/4`；Valgrind `54,339 allocs / 54,339 frees`、零 live bytes、零 errors，Helgrind 零 errors；MSVC Debug 同一集合 `4/4`，MSVC ASan lifecycle 退出 `0`。 |

## Delivered Contract

- `SZrStdioServer` 是 heap-owned runtime root。`New` 初始化 global registry、LSP
  context、request registry 和 input state；`Start` 是唯一创建 reader 的入口。
- input state 保存 Windows thread handle 或 POSIX thread id、stop flag、init/start
  状态和 input stream。线程不再在创建后 `CloseHandle` 或 `pthread_detach`。
- `Free` 固定执行 stop reader、在 protocol `exit`/EOF 后 join、drain queued
  cJSON requests、destroy synchronization primitives、free registry/caches、free
  LSP context、free global state，最后释放 server。
- 有效 `exit` notification 由 reader 入队后自然终止读取；EOF 同样终止。调用方
  提供的 `FILE *` 保持调用方所有权。
- Fault injection 覆盖 global、context、input initialization 与 reader start。
  所有已初始化资源都走同一个 `ZrLanguageServer_StdioServer_Free` 路径。

## Evidence

Validation used isolated WSL GCC Debug shared build
`/home/hejiahui/.codex-builds/lsp-stdio-teardown-gcc-red`.

- RED: the new lifecycle target failed only at link time for the absent
  `ZrLanguageServer_StdioServer_*` API before implementation.
- GREEN: direct `zr_vm_language_server_stdio_server_lifecycle_test` exited 0.
  It performs 100 same-process lifecycle cycles, an exit-frame stop and four
  startup-failure paths.
- Rebuilt `zr_vm_language_server_stdio` and `zr_vm_cli`, then CTest
  `language_server_stdio_(server_lifecycle|smoke|protocol|document_sync)` passed
  `4/4`: lifecycle, main stdio smoke, protocol inventory and protocol
  conformance.
- Valgrind `--leak-check=full --errors-for-leak-kinds=definite,indirect` on the
  lifecycle target completed with 54,335 allocations/frees, zero live bytes and
  zero errors.
- Helgrind on the same lifecycle loop completed with zero data-race errors.

## Boundary

Task 5 and Task 6 are accepted. The final sanitizer smoke replay covers the
Node-spawned server, CLI metadata fixture generation, descriptor plugin
loading, imported highlights and decorator parsing. Sanitizer-only smoke uses
`ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES=1073741824` to account for ASan shadow
memory; the uninstrumented GCC and MSVC smoke runs retain the default 512 MiB
process budget. No sanitizer suppression or marker whitelist was added.
