---
plan_id: optimize
task: plan03-task03-sub32
status: completed
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.h
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.h
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.h
tests:
  - tests/language_server/test_lsp_native_type_member_identity_cases.h
  - tests/language_server/test_lsp_semantic_query_parity.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.32: Native Type Member Projection Identity

## Failure And Cause

Native field/method declaration projection still compared owner and member
spellings and selected the first matching row. Reverse lookup first found the
correct rendered position, then discarded that record's identity and performed
another name search. Both directions could select another same-named member.

The new metadata fixture has two distinct type rows named `Shared`, each with
two fields named `value` and two methods named `run`. The field/return types
differ. This is an adversarial metadata projection fixture, not a claim that
duplicate field declarations or return-only overloads are accepted ZR syntax.

GCC RED has four failures and exit 1. Forward lookup of row 1 returns field
`(2,16)` instead of `(3,16)`, and method `(4,12)` instead of `(5,12)`, using
zero-based LSP positions. Both reverse cases recover the wrong descriptor at
row 1. Logs are `.codex/lsp-optimize-validation/plan03-task03-sub32-red-gcc.log`
and `plan03-task03-sub32-red-bidirectional-gcc.log`.

## Change And Lifetime

`FindTypeMemberDeclaration` receives the selected descriptor identity and kind.
Virtual documents delegate to the shared native projection's exact `Find`;
the existing compact plugin coordinate records also carry descriptor identity
and require exactly one match. Missing or ambiguous identity leaves no range.
The provider clears prior `hasDeclaration` and range state before projection,
so a failed second lookup cannot retain an earlier success.

Rendered and compact records now retain their borrowed owner type descriptor.
Position lookup returns both owner and member identity. Reverse metadata lookup
uses that owner directly and validates the member address against its typed
field/method array. Names only populate display values. Checking array membership
also prevents an enum member, which shares the virtual FIELD kind, from being
cast to a field descriptor by spelling or by an unchecked pointer conversion.

Record arrays are freed within each lookup. Returned descriptor pointers are
borrowed from the supplied provider and cannot outlive it or cross a reload;
ranges are copied before the record array is released. Text remains GC-owned.
No persistent pointer identity, parser fact mutation, AST inference, URI format
change or compact-coordinate format change is introduced.

The existing forward/reverse provider functions are exported with the module's
API macro so the contract is exercised by both static and shared test builds.
The regression is a separate 122-line header with four registrations. The
virtual-document implementation is 379 lines and the native renderer 734 lines.
The oversized metadata provider receives only changes to its existing member
projection functions; broader modularization remains in Plan 06.

## Reference Evidence

Roslyn's `MetadataAsSourceHelpers` resolves a SymbolKey against generated source
to recover the declaration location, and its
`MetadataAsSourceTests.CSharp.cs::TestExtendedPartialMethod1` asserts the selected
generated declaration. rust-analyzer's
`goto_definition.rs::goto_definition_resolves_correct_name` checks that same-named
symbols in different modules do not share a target. Exact paths are recorded in
[Sub31](2026-09-09-plan03-task03-sub31-native-virtual-declaration-projection.md).
ZR's temporary descriptor-row projection stays within one live provider; cross
snapshot semantic association still requires canonical metadata identity and
provider generation.

## Verification

Source is `446c3c23` plus this milestone's eight code/test files and the existing
shared worktree overlay. GCC 11.4 Debug static uses
`/home/hejiahui/.codex-builds/l8-callable-value-gcc`; Clang 14 Debug shared with
ASan/UBSan/LSan uses `/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang`;
MSVC 19.44 Debug shared uses
`E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc-current`.

| Toolchain | Parity | Snapshot CTest | Source Contracts | Full Stdio |
| --- | --- | --- | --- | --- |
| GCC | 30/30, exit 0 | 1/1, exit 0 | exit 0 | exit 0, 36.52 MiB |
| Clang | 30/30, exit 0 | 1/1, exit 0 | exit 0 | exit 0, 706.77 MiB |
| MSVC | 30/30, exit 0 | 1/1, exit 0 | exit 0 | exit 0, 45.86 MiB |

Four new cases independently check forward and reverse projection for fields
and methods. Each visits all four rows, including duplicate member names and
duplicate owner names. Forward results must select exactly the rendered
identifier; reverse results must return the original owner and member rows.
Removing the selected descriptor afterward must fail and clear the stale range.
Clang parity, snapshot, contracts and stdio contain no sanitizer report.
Parser code is unchanged from Sub31's three-toolchain 29/29 relation validation.

All complete interface runners retain 114 PASS / 2 FAIL, exit 1:

- LSP Class Member Navigation And Completion
- LSP Hover And Completion Surface Explicit Exact Type Failures

All complete project runners retain 55 PASS / 7 FAIL, exit 1, with the exact
Sub30/Sub31 failure-name set. Clang interface retains 20144 bytes / 422
allocations and project retains 19160 bytes / 481 allocations. These whole
runner functional and memory gates remain unaccepted.

Build with `cmake --build <build> --target <targets> --parallel 16`; MSVC adds
`--config Debug` through the `using-vsdevcmd` wrapper. Targets:

```text
zr_vm_language_server_semantic_query_parity_test
zr_vm_language_server_lsp_interface_test
zr_vm_language_server_lsp_project_features_test
zr_vm_language_server_lsp_semantic_snapshot_test
zr_vm_language_server_lsp_source_contracts_test
zr_vm_language_server_stdio
```

Run the matching `<build>/bin/` executables. Snapshot uses
`ctest --test-dir <build> --output-on-failure -R '^language_server_lsp_semantic_snapshot$'`
with `-C Debug` on MSVC. Stdio uses
`node tests/language_server/stdio_smoke.js <stdio-executable> <cli-executable>`.
Clang uses `ASAN_OPTIONS=detect_leaks=1`, `UBSAN_OPTIONS=halt_on_error=1` and
the established sanitizer-only stdio budget
`ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES=1073741824`; GCC/MSVC retain 512 MiB.
Peak bytes are GCC 38293504, Clang 741105664 and MSVC 48087040.

Final logs are `.codex/lsp-optimize-validation/plan03-task03-sub32-`
`{build,parity,interface,project,snapshot,contracts,stdio}-{gcc,clang,msvc}.log`.
All builds and command sessions ended. The test fixture's initial const string
conversion warning was corrected before the final three-toolchain builds.

## 状态与产出记录

- Started: 2026-09-09 11:43 +08:00.
- Completed: 2026-09-09 11:58 +08:00.
- Status: native field/method descriptor-to-range and position-to-descriptor
  identity accepted on GCC, Clang and MSVC.
- Outputs: exact native type-member projection, owner/member range records,
  four regressions, shared-build provider API exposure, module contract and this
  record. Source version and commands are recorded above.
- Remaining: actual binary/plugin project-scoped virtual URI and parser origin
  producers, multi-definition relation matrix, canonical receiver acquisition,
  aggregate failures and leaks, and Plan 03 Tasks 3/7/8 plus full native/Web gates.
