---
related_code:
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_code_lens.c
implementation_files:
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/CMakeLists.txt
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
tests:
  - tests/language_server/stdio_client_commands_smoke.js
  - tests/language_server/stdio_resolve_capabilities_smoke.js
  - tests/language_server/stdio_smoke.js
  - tests/acceptance/2026-09-05-lsp-client-commands.md
doc_type: milestone-record
---

# Client Command Dispatch

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证结果 |
| --- | --- | --- | --- | --- |
| 2026-09-05 21:18 +08:00 | 2026-09-05 21:31 +08:00 | completed (subitem; parent pending) | Remove the unregistered executeCommand no-op and retain client CodeLens commands. | GCC RED 6/10, exit 1; GCC/Clang/MSVC focused 14/14, command fixture 10/10; independent re-review approved. |

## Reproduction And Change

The compiled native inventory found `workspace/executeCommand` in the actual
feature dispatcher with no capability owner. Its handler returned JSON null
for both recognized client command IDs and every unknown command. The new
dedicated protocol test failed exactly six request checks across empty and
command-aware clients; both initialize and lifecycle pairs passed. Direct WSL
Node execution returned exit 1. The prior source-marker inventory passed and
is explicitly not evidence for the new compiled inventory.

The fix removes the no-op handler file, CMake source entry, dispatcher branch,
prototype and four unused constants. `ZR_LSP_COMMAND_RUN_CURRENT_PROJECT`
remains in real test CodeLens production. The extension continues to execute
the commands locally. The unsupported method now follows ordinary
MethodNotFound handling without an identity resolver or compatibility shim.
Independent review identified the old comprehensive smoke's legacy null-success
assertion. It now reads the full response through the shared protocol client
and requires the exact unsupported-method envelope.

The change introduces no language syntax or compiler/query semantics. The
returned CodeLens command remains client-owned data; no document snapshot,
provider identity or edit plan is created by the withdrawn request. See
[Stdio Validation](../../../cli-and-tooling/lsp-stdio-validation.md).

## Validation

```text
node tests/language_server/stdio_client_commands_smoke.js <stdio-server>
ctest --test-dir <build-dir> --output-on-failure -R '^language_server_stdio_(client_commands_smoke|resolve_capabilities_smoke)$'
git diff --check -- <owned-paths>
```

GCC and Clang use Debug shared builds at
`/home/hejiahui/.codex-builds/lsp-optimize-20260905-root/{gcc,clang}`.
MSVC uses Debug static at `E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc`
under VsDevCmd. These contain the `c95e5387` committed source export, exact
gitlinks and explicitly owned protocol corrections through `d1b28e59`, then
this subitem's files. Concurrent analyzer, parser, ownership and benchmark
overlays are excluded. Full acceptance of one integrated committed source
tree remains a later gate.

The containing commit owns only this repair, its protocol registration,
module documentation and evidence. The separate compiled inventory remains
Task 2 Sub02 and must not be included in this commit's completion claim.

| Toolchain | Focused CTest | Comprehensive smoke |
| --- | --- | --- |
| GCC 11.4 | 14/14, exit 0, 8.40 s | exit 1 at existing generic completion detail, 0.67 s |
| Clang 14 | 14/14, exit 0, 10.33 s | same existing failure, 1.81 s |
| MSVC 19.44 | 14/14, exit 0, 17.28 s | same existing failure, 2.14 s |

Thirteen focused tests belong to the already committed protocol surface plus
this repair; the four-profile inventory is the fourteenth, exploratory test
and remains a separate uncommitted subitem. The focused suite covers registry,
optional allocations/negotiation, lifecycle, protocol conformance, complete
resolve payloads, color/save/client-command/navigation/file-operation behavior,
document synchronization and workspace folders. All command fixtures pass
10/10. Comprehensive smoke passes the updated command assertion before the
same already recorded generic completion failure at its later line 1973. No
whole-suite GREEN is claimed.

Seven relevant code/test files match both frozen sources byte-for-byte
(14 SHA-256 comparisons, zero mismatches). The deleted handler is absent in
all three sources. The shared CMake file also contains the separate inventory
registration in validation; staging selects only this subitem's six added
registration lines. Independent read-only review caught the stale legacy
assertion; re-review after correction found no remaining issue.
