---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_capability_registry.h
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/05-native-web-capability-parity.md
tests:
  - tests/language_server/test_lsp_capability_registry.c
doc_type: milestone-record
---

# Capability Registry Implementation Metadata

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证结果 |
| --- | --- | --- | --- | --- |
| 2026-09-05 18:10 +08:00 | 2026-09-05 18:48 +08:00 | completed (metadata subitem; parent pending) | Replace invented implementation names with audited core/native/WASM entries and registered test IDs; represent native adapter ownership; validate enabled and disabled runtime metadata; correct experimental 3.18 and color client-path metadata. | RED 109, ownership RED 208, review RED 6 reproduced. GCC/Clang/MSVC each focused 9/9 pass and final registry test 1/1 pass. Independent specification and quality reviews approved. |

## Contract And Outputs

The C-only registry now describes 31 current base capabilities, 19 shared
native/WASM and 12 native-only. Eight facilities owned by the native adapter
have null core entry points instead of invented core APIs. Every remaining
core entry, native handler, WASM export and CTest ID was checked against source
definitions and registration. Protocol methods are unchanged in this leaf.

`implementationLayer` distinguishes core entry-point ownership from native
adapter ownership. Native-only and WASM-only descriptors require the enabled
runtime's nonempty field and the disabled runtime's explicitly null field.
Unknown bits and conflicting ownership fail validation. Inline completion is
marked experimental 3.18. Color's client capability is
`textDocument.colorProvider`, distinct from `textDocument/documentColor`.

The registry returns immutable borrowed process-lifetime data. It stores no
semantic snapshot or compiler identity. Module documentation describes
ownership, lifetime, exactness and the difference between metadata validation
and actual client/runtime publication:
[LSP Capability Registry Metadata](../../../cli-and-tooling/lsp-capability-registry-metadata.md).

## Failure Evidence

1. New audited-name expectations against the prior registry reproduced 109
   assertion failures, exit 1. Raw log: `.codex/lsp-registry-metadata-red.log`.
2. Explicit ownership expectations against old initializers reproduced 208
   failures, exit 1. Raw log: `.codex/lsp-registry-layer-red.log`.
3. Independent specification review found missing symmetric disabled-runtime
   validation and an incorrect color client path. Six new assertions failed
   on the implementation before repair, exit 1:

```text
Fail - colorProvider client capability path: expected textDocument.colorProvider, got textDocument.documentColor
Fail - native-only capabilities must not claim a disabled WASM export
Fail - disabled WASM metadata must be explicitly null, not empty
Fail - WASM-only capabilities must not claim a disabled native adapter
Fail - disabled native metadata must be explicitly null, not empty
Fail - native adapter ownership must not claim a disabled WASM export
Fail - LSP capability registry: 6 failures
```

The resolve-negative fixture clears the now-disabled WASM field before testing
its independent resolve errors, preventing unrelated validation from masking
those assertions. Specification re-review approved both fixes. Quality review
also isolated the zero-runtime and unknown-bit fixtures from disabled-runtime
field checks, so unrelated invalid fields cannot mask those regressions.

## Verification

```text
cmake --build <build-dir> --target zr_vm_language_server_stdio zr_vm_language_server_lsp_capability_registry_test zr_vm_language_server_stdio_server_lifecycle_test --parallel 4
ctest --test-dir <build-dir> --output-on-failure -R '^language_server_(lsp_capability_registry|stdio_(protocol_inventory|protocol_conformance|resolve_capabilities_smoke|navigation_capabilities_smoke|file_operation_capabilities_smoke|document_sync_conformance|server_lifecycle|workspace_folders_smoke))$'
```

GCC 11.4 / Clang 14 Debug shared build directories are
`/home/hejiahui/.codex-builds/lsp-optimize-20260905-root/gcc` and `clang`.
MSVC 19.44 Debug static uses
`E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc`, invoked through
`Invoke-VsDevCommand.ps1`. GCC and Clang report 9/9 passes in 9.55 s and 8.36 s.
MSVC reports 9/9 passes in 25.43 s and builds with the existing `/W3` to `/W4`
command-line warning. After the final test-only fixture isolation adjustment,
all three registry targets rebuild successfully and their CTest entries pass
1/1 (GCC 0.43 s, Clang 0.15 s, MSVC 0.69 s). Production code is unchanged from
the preceding 9/9 runs. Scoped `git diff --check` passes.

Final reviewed source SHA-256 values, in header/implementation/test order:

```text
BA35A5F2626F41DE25ED91BF6A96B586E043D299E377E7864C35169D39C81C84
FD08F576F3BD860829746D0E09A1E50AA02B3A1CE34003ABB288FCC93C3FCA52
DF474A85810B04A47917CF8D929CF5EA7AFABBF1E32B93E58D40608B7278F1D3
```

These byte hashes describe the reviewed worktree/frozen-source files. Git line
ending normalization may give a different checkout-byte hash; Git blob identity
in the containing commit remains the authoritative committed source version.

## Source And Remaining Gates

The leaf starts after `31bdfa0d` on shared main. Isolated verification uses
original `c95e5387` plus exact gitlinks and this session's explicitly owned
capability/protocol corrections through `31bdfa0d`, followed by these three
registry files. Concurrent ownership commits and uncommitted semantic paths
are excluded from that frozen validation source. The containing commit
identifies this leaf; final integrated acceptance must use one committed tree.

The metadata audit is structural: client negotiation, actual handler behavior,
linked WASM exports and zero-orphan inventory are not inferred from nonempty
strings. Existing broad smoke failures remain in the frozen failure baseline.
Task 2 and Phase 00 stay unaccepted until the inventory and remaining baseline
requirements close. The original symbol-projection/type-query commits are
still required before integrated semantic validation. Phases 01-06 are not
promoted by this repair. No syntax rule changes or conflicts are introduced.
