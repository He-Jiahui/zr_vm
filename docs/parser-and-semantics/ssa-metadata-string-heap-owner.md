---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_string_heap.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_string_heap.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_core/src/zr_vm_core/function.c
  - tests/parser/test_ssa_metadata_string_heap_owner.c
  - tests/cmake/ssa-metadata-string-heap-owner.cmake
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_string_heap.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_string_heap.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
plan_sources:
  - docs/plans/ssa
  - .codex/plans/20261008-ssa-function-retention-next-finite-v4.md
  - .codex/plans/20261008-ssa-source-scalar-constant-source-tests-design.md
tests:
  - tests/parser/test_ssa_metadata_string_heap_owner.c
  - tests/acceptance/ssa-metadata-string-heap-owner.md
doc_type: module-detail
status: metadata-string-heap-owner-finite-green
---

# Exact ownership of the metadata string heap

## Observed failure

The actual source9/source8 compiler fixture exposed a lower-layer allocator
contract violation while preparing the source-constant gate. Its string-heap
builder reserved eight 16-byte rows, then transferred the whole allocation to
Function while publishing only one live row. Capacity was lost. Both compiler
metadata cleanup and Function destruction freed `length * sizeof(entry)`, so
the strict VM allocator observed allocated128/original16/FUNCTION11.

The first 24-case constant run therefore had24 failures/0 ignores, including
both prerequisite cases. It was rejected as a functional RED. The allocator
was kept strict. This repair addresses actual allocation ownership before
source retention can rely on successful compiler/Function cleanup.

## Module boundary

The private `compiler_metadata_string_heap` module contains the builder struct,
capacity management, stable string indexing, collection, sorting, free and
exact-size compaction. Token/signature/module orchestration remains in
`compiler_metadata_token.c`, reduced from over2100 to1760 lines. The new C/H
are411/43 lines. No public Function field, schema or metadata hash changes.

Existing collection/sort helper bodies were compared against actual Git HEAD:
after only CRLF, private `static` and surrounding blank-line normalization,
all13 functions have equal SHA256
`7202F20FEAF27C5A16A4E1D79D587C4B853101317511FE10CD3936D21451121D`.
No source snapshot was written. Collection still borrows the compiler's live
string owners and may use its existing generic-name parsing support.

## Compaction and publication

`metadata_token_string_heap_compact` accepts a live compiler/state/global and
independently owned builder storage. It checks length <= capacity, pointer/count
agreement and the capacity byte product before reading or freeing storage.

| State | Operation |
| --- | --- |
| length == capacity | Return true without allocation or mutation |
| length0, capacity>0 | Free original capacity bytes and reset entries/capacity |
| 0<length<capacity | Allocate exact live bytes, copy rows, free old capacity bytes, publish candidate |
| Invalid geometry or candidate OOM | Return false with builder/storage unchanged |

It never reallocates, generates a hash, changes an entry or destroys a string.
An OOM candidate cannot lose the old allocation. Empty rows cause no zero-byte
allocation. Successful publication has capacity == logical live length.

The token builder invokes compaction before its first Function pointer
publication. On refusal, it frees uncommitted token records, signature blob,
explicit-module list and the original capacity-aware builder. On success it
transfers compacted storage and length, clears builder ownership and computes
the existing module hash. Both existing consumers can then free exactly the
published live length. The previous behavior of clearing existing Function
metadata before the overall build is unchanged; this repair does not promise
preservation of an older published Function across every metadata-build OOM.

## Verification and remaining work

Six actual default cases verify real finalized source9/source8 allocation
sizes and complete lifecycle cleanup, compact OOM preservation/retry, repeated
no-op compaction, empty builders and zero-length reserved storage. Current
clang-cl19 Debug/assertions/UBSan and selected four-unit MSVC compilation pass
with no warnings. See [acceptance](../acceptance/ssa-metadata-string-heap-owner.md).

General builder reserve arithmetic and unrelated metadata replacement behavior
were not broadened by this finite fix. The constant API, returned-source
retention, normal-route migration, native parity, Linux, full MSVC and all47 SSA
leaves remain independently OPEN. No source program, Oracle or network/FFI/
provider/security path is executed by these tests.
