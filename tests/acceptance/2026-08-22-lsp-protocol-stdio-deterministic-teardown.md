# LSP Protocol Stdio Deterministic Teardown Acceptance

## Scope

- Native stdio server lifetime, reader ownership and ordered teardown.
- Same-process lifecycle repetition and startup-failure cleanup.
- Existing JSON-RPC and LSP smoke behavior after the executable begins real
  teardown rather than relying on process termination.

## Contract

- A server instance owns its global runtime, LSP context, request registry,
  input synchronization objects and caches.
- Reader ownership remains available until `join`; it is never detached or
  closed immediately after creation.
- Cleanup is ordered: stop reader, join it after protocol `exit`/EOF, drain inbound messages,
  destroy input synchronization, free caches and registry, free LSP context,
  then free global state.
- Valid `exit` and EOF end the reader. The caller retains ownership of the
  supplied input stream.
- Failures after global creation, context creation, input initialization and
  reader creation use the same cleanup implementation.

## Evidence

The WSL GCC Debug shared build at
`/home/hejiahui/.codex-builds/lsp-stdio-teardown-gcc-red` recorded:

| Gate | Result |
| --- | --- |
| Lifecycle RED | Expected missing `ZrLanguageServer_StdioServer_*` link symbols before implementation. |
| Lifecycle GREEN | Direct process exit `0`; 100 iterations, valid exit frame and four fault points. |
| `language_server_stdio*` CTest | `8/8` passed after building the CLI and descriptor fixture dependencies. |
| Protocol conformance | Passed as part of the 8-case CTest run. |

The first two CTest attempts failed only because the fresh isolated build
lacked `zr_vm_cli` and the descriptor plugin fixture needed by the smoke setup.
After those targets were built, the unchanged test set passed completely.

A subsequent isolated Clang ASan+UBSan run found and drove repairs for an
unaligned `TZrNativeString` access and global registry allocations not released
at teardown. The focused core layout test is now sanitizer-clean. However, the
100-cycle lifecycle executable still terminates with `SIGSEGV` only when
LeakSanitizer is enabled, so this record remains unaccepted.

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 00:16 +08:00 | in_progress | stdio deterministic teardown、reader join and failure cleanup implementation complete; core layout and global registry teardown support repaired. | GCC layout + lifecycle direct exit `0`; Clang core layout sanitizer-clean; LeakSanitizer lifecycle still SIGSEGV. |
| 2026-08-23 01:35 +08:00 | in_progress | Rebuilt the GCC stdio/CLI targets and completed an independent Memcheck lifecycle audit. | GCC CTest lifecycle/smoke/protocol inventory/protocol conformance `4/4` passed; Valgrind reports `54,335 allocs / 54,335 frees`, `0 errors`, and no live blocks. Clang direct lifecycle passes, but Node-spawned sanitizer protocol children remain unstable and sanitizer smoke exposes out-of-scope CLI leaks. |
| 2026-08-23 01:37 +08:00 | in_progress | Completed a reader/main-thread lifecycle race audit. | Helgrind reports `0 errors from 0 contexts` for the full lifecycle loop. The Clang Node-spawn sanitizer instability remains unaccepted. |
| 2026-08-23 03:27 +08:00 | completed | Accepted deterministic teardown and its protocol transport validation gates. | Clang and GCC ASan+UBSan each pass lifecycle/smoke/protocol inventory/protocol conformance `4/4`; uninstrumented GCC keeps the 512 MiB smoke budget and passes `4/4`; Valgrind and Helgrind report zero errors; MSVC Debug passes `4/4` and MSVC ASan lifecycle exits `0`. |

## Acceptance Decision

Task 5 and Task 6 are accepted. The final Clang and GCC sanitizer runs cover
the Node-spawned protocol child rather than only the direct lifecycle target.
The sanitizer-only 1 GiB peak-budget override accounts for ASan shadow memory;
the default 512 MiB budget remains enforced by the uninstrumented GCC and
MSVC smoke runs. No sanitizer suppression or whitelist is used.
