---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_string_heap.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_string_heap.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - tests/parser/test_ssa_metadata_string_heap_owner.c
  - tests/cmake/ssa-metadata-string-heap-owner.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_string_heap.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_string_heap.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
plan_sources:
  - docs/plans/ssa
  - .codex/plans/20261008-ssa-function-retention-next-finite-v4.md
tests:
  - tests/parser/test_ssa_metadata_string_heap_owner.c
  - tests/acceptance/ssa-metadata-string-heap-owner.md
doc_type: acceptance-record
status: metadata-string-heap-owner-finite-green
---

# Metadata string heap ownership acceptance

## Current evidence

Actual E-drive checkout, Windows x64 clang-cl19 Debug with assertions and
UBSan halt-on-error, selector `^ssa_metadata_string_heap_owner$`:

- Configure/build/CTest all exited0.
- Default6 tests,0 failures,0 ignores; no ledger or sanitizer diagnostic.
- Current incremental build0 warnings.
- Raw link map selects the real Parser archive's private compaction module
  exactly once; no runtime/scalar-text fallback or named network/FFI providers.
- Production/source/header/selected-source/settings/binary/map pins remained
  unchanged across the run. No source copy, snapshot or worktree was created.

Immutable receipt names below are under
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`:

| Evidence | SHA256 |
| --- | --- |
| source-metadata-string-heap-owner-green-r2-receipt.json | 9E860F49CBE543A1BA18F6364598D54888B3D1E5174D3375DAFD04F220499F44 |
| source-metadata-string-heap-owner-green-r2-ctest.log | 1081853C88D6EDC5198AE98EEFA7E72959B628B687E750C88AB96AE822297231 |
| source-metadata-string-heap-owner-msvc-green-r3-receipt.json | 7D53BC3AAD5DD378D2A104AA5864691D6441A395359022E2172EE4AA6155FBDF |
| source-metadata-string-heap-owner-extraction-r1-review.json | 53E401C261989AB9E283B085A6137AC7CC5F34CEF85B83997F3556C3FE5B7F4E |

MSVC19.44 /W4/assertions compiles actual helper, test, literal fixture and token
producer: four actual child exits0,0 warnings. Its first attempt omitted an
existing private type-inference include directory; the second exposed C4702
in the test's unreachable return. Both failed receipts remain. The final test
uses a bounded search plus assertion, without suppressing warnings. Its304-line
source SHA256 is `292E84442465F42F363337708CFE79DC9C3DC332E413D086242470B9124108C0`.
These are finite compile-only checks; full MSVC and Linux remain OPEN.

## Independent review and scope

Sol independent ownership review found no issue in candidate preservation,
original capacity free, cleanup before publication or the private include
closure. Luna reviewed actual fixture registration, builder-before-fixture
cleanup, failure observations and pointer lifetime. Root separately compared
the13 moved helper bodies with the actual Git base; equality covers existing
collection/sort logic, while the new compaction has direct actual tests.

The strict allocator reports exact original bytes and memory flags. It can
reject the next exact-size FUNCTION allocation only inside the explicit compact
call interval; the fault is disarmed before assertions/cleanup. It never probes
bootstrap, providers or runtime execution. The real Global constructor-returned
owner is cleaned according to its contract; arbitrary partial bootstrap
recovery is not claimed.

The subsequent source-constant RED r2 has actual source9/source8 prerequisites
and NULL-argument case passing,21 actual stub-feature failures/0 ignores and no
ledger error. That is a separate constant implementation gate. Returned-source
retention, default canonical output, backend parity, all source families,
performance and all47 SSA leaves remain OPEN.
