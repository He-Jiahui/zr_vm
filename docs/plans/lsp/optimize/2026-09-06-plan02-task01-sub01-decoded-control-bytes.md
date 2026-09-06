---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_uri.c
  - tests/language_server/test_lsp_uri.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_uri.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
tests:
  - zr_vm_language_server_lsp_uri_test
doc_type: milestone-record
---

# Plan 02 Task 1 Sub01: Decoded Control Bytes

## 状态与产出记录

| Started | Completed | Status | Completed items | Evidence |
| --- | --- | --- | --- | --- |
| 2026-09-06 13:43 +08:00 | 2026-09-06 14:03 +08:00 | completed (subitem only) | Reject percent-encoded control bytes at the native file boundary; retain printable escapes and path round trips. | GCC 17/17, Clang ASan+UBSan 17/17, MSVC 19/19; every process exited 0. Parent Plan 02 Task 1 remains open. |

## Defect And Contract

`FileToNativePath` rejected raw control bytes and an encoded NUL, but the
post-percent-decode check allowed `%01` and `%7F`. These bytes could therefore
reach a native filename despite the published control-character rejection
contract. The decoder now applies the same control-byte boundary after decoding
and clears the caller's buffer on failure.

The change keeps strict single-pass decoding, encoded-separator rejection,
UTF-8 byte round trips and virtual-scheme rejection. It introduces no semantic
identity, provider-generation or snapshot lifetime changes. The module contract
is documented in [LSP URI/native path boundary](../../../cli-and-tooling/lsp-uri-native-path-boundary.md).

## Verification

The new regression fails on the original codec and passes after the decoder
change. The current GCC target required a 767-step dependency rebuild because
other sessions had added shared runtime/parser/library sources. The final
incremental rebuild completed 16/16 and the URI executable reported 17/17
passes. The current Clang ASan+UBSan cache rebuilt 756/756 steps and reported
17/17 passes with leak detection enabled.

```text
cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --target zr_vm_language_server_lsp_uri_test --parallel 8
LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/lib \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_lsp_uri_test
```

The Clang run used the current-source cache
`.codex/lsp-optimize-validation/clang-asan-current` with:

```text
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/lib \
    /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_lsp_uri_test
```

The MSVC Debug static cache is rooted at
`.codex/lsp-optimize-validation/source`, a frozen validation snapshot. Its
reconfigure initially encountered the unrelated active call-binding include;
that include was removed only in the private snapshot, while the URI codec and
focused test were patched there to match this slice. The wrapper build produced
the URI executable, and the wrapper invocation reported 19/19 passes:

```text
Invoke-VsDevCommand.ps1 cmake --build E:\Git\zr_vm\.codex\lsp-optimize-validation\msvc \
  --target zr_vm_language_server_lsp_uri_test --parallel 4
Invoke-VsDevCommand.ps1 E:\Git\zr_vm\.codex\lsp-optimize-validation\msvc\bin\zr_vm_language_server_lsp_uri_test.exe
```

The MSVC snapshot adds Windows UNC cases, hence its matrix has two more checks
than the POSIX GCC and Clang matrices. No sanitizer, semantic phase or parent
Task 1 acceptance is inferred from this focused subitem.

HEAD before this slice was `5fb7bcff`. The current checkout includes unrelated
uncommitted overlays; only the URI codec, its focused regression and associated
documents belong to this slice. No phase or full semantic gate is promoted by
this focused boundary repair.
