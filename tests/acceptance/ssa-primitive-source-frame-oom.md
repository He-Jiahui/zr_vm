---
related_code:
  - tests/parser/test_ssa_primitive_source_frame_oom.c
  - tests/parser/support/ssa_primitive_source_frame_oom_shim.h
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-execbc-vm.cmake
implementation_files:
  - tests/parser/test_ssa_primitive_source_frame_oom.c
  - tests/parser/support/ssa_primitive_source_frame_oom_shim.h
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-execbc-vm.cmake
plan_sources:
  - .codex/plans/20261006-ssa-primitive-frame-oom.md
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
tests:
  - tests/parser/test_ssa_primitive_source_frame_oom.c
  - tests/parser/test_ssa_primitive_source_frame.c
  - docs/acceptance/ssa-primitive-source-frame-oom.md
doc_type: test-acceptance-guide
status: primitive-source-frame-oom-focused-windows-green-accepted
---

# Primitive source frame OOM acceptance guide

## Current gate status

**Focused Windows direct-source OOM gate accepted.** Root registered and built
the standalone OBJECT and executable, then ran four complete suites under
Windows x64 clang-cl 19 Debug assertions and UBSan: host 15, frame 19, storage
4 and OOM 2, totaling 40 Unity cases, zero failures and ignored cases. Configure,
build and CTest each naturally exited 0. No UBSan diagnostics or build warnings
were observed. Final receipt has `validated: true`; Root's final Git transaction
remains pending, and this record assigns no new commit or RED run.

The existing frame suite supplies historical real 9/8 prerequisite evidence.
Each new case uses its own NINE shared fixture, compacts the actual graph in
the same live canonical context, obtains an actual i64 host row and appends
that row to the actual module. Fixture, original source graph, compacted output,
context/source maps and module tables keep their distinct owners. The test
does not invoke Oracle, canonical projection or native/backend execution.

## Exact inventory and transaction assertions

| Default Unity case | Injected NULL result | Expected direct cleanup |
| --- | --- | --- |
| `test_values_calloc_failure_same_graph_retry` | First source-frame `calloc` at production line 313 | No successful direct allocation or packed-cleanup call. |
| `test_header_malloc_failure_same_graph_retry` | First source-frame `malloc` at production line 337 | Real packed cleanup call, packed owner clearing, values wrapped free. |

The default main runs exactly these two cases. Each executed case demonstrated:

1. Actual readable fixture/storage and VERIFY_ALL prerequisites, with one true
   surviving compacted value and its actual canonical INT64 type.
2. Exactly one selected allocator-failure hit. Compare calloc count/width with
   actual valueCount and `sizeof(SZrExecIrPackedValue)`, and malloc size with
   `sizeof(SZrExecIrFrameLayout)` when the header call is reached.
3. False return and complete-byte diagnostic equality to a zeroed record with
   only OOM code and original functionToken. No metadata perturbation substitutes
   for a real allocator NULL result.
4. Unchanged full graph header/contract/pool identities and active data, original
   source graph, source/context digest and module tables; no frame publication.
5. Zero remaining direct temporary owners. Header-fault cleanup must call the
   original packed helper and observe its live header cleared after return.
6. Disable injection, reset only the fixed ledger, and call Attach again on the
   SAME live graph and pools. No rebuild, clone, header restoration or repeated
   compaction may make this a new graph.
7. Successful retry and cleared diagnostic. Only frameLayout and
   contract.layoutHash may change; real ValueId/TypeId slot geometry, VERIFY_ALL,
   source maps and unchanged caller tables must hold.

Successful retry records two direct allocations, one wrapped values free, zero
temporary owners and one header ownership transfer. These counts deliberately
exclude normal Core teardown of published header/slots. Packed construction
allocates additional order, map and slot arrays in its own real TU; their
allocations/frees are not dynamically counted or individually fault-injected.
The helper wrapper proves real cleanup-call/owner clearing only. It does not
read freed memory or promise exhaustive OOM coverage or total heap balance.

## Root-owned direct-source build contract

Root registered the new executable and a separate OBJECT compiling the actual
`zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c`. Only that OBJECT
receives the forced include of
`tests/parser/support/ssa_primitive_source_frame_oom_shim.h`. Normal stdlib and
real packed-layout declarations precede its macro redirects. The test TU uses
its NO_REDIRECT switch to retain original CRT/helper calls. The test also
compiles the existing shared fixture normally and links real parser/Core/library
dependencies. Ordinary production parser compilation stays uninjected.

Use the approved reusable build beneath `E:/cargo-targets/zr_vm/build` and the
current checkout directly, with the route's Debug/assertions and UBSan settings.
Keep the object aligned with normal parser definitions/includes and CRT mode;
the focused clang-cl route uses target-private `/FI` and `/MD`. Do not apply
source-global properties or whole-target `-Dmalloc=...`/`-Dcalloc=...` flags
that rename CRT declarations. No source copies, `include.c`, production test
hooks, process-wide CRT interception or WILL_FAIL acceptance are part of this
gate. Current ordinary STATIC archives do not use whole-archive; actual build
evidence confirms isolation:
Actual compile commands contain exactly two production frame TU entries and
one forced-include entry belonging solely to the standalone OBJECT. The normal
parser entry has no shim. The sole Attach link-map record resolves to explicit
`exec_ir_source_frame.c.obj`, without a parser-archive member prefix.

Wrappers use fixed global records and no Unity assertions in the production
stack. Global fixture/graph owners register before setup assertions. Immediately
after Attach returns, actual publication is recorded before assertions so Core
owns a transferred header even if an assertion fails. Teardown disables failure
injection, frees the real output graph, cleans any remaining direct temporary
owner after a failed assertion, then frees fixture and state. It never frees
a shallow observed graph as an owning graph or dereferences freed arrays.

## Recorded evidence and reconciliation

Final receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/primitive-frame-oom-r2-receipt.json`,
SHA-256 `522FA6E2E2CD2FF8776EAE86BA682DF1EB1E3511478D6B26F217350E79DEEC87`.
All 28 source pins, five metadata files, four executables and three immutable
logs were freshly checked against the receipt and matched. Historical receipt:
`primitive-frame-oom-receipt.json`, SHA-256
`A0AF1B6825597C3F68941C0B4EE19286D8DCC59126253A948F02E88A97D329E0`.
Earlier preflight record: `primitive-frame-oom-preflight-failure.json`.
Both historical records remain in the same report directory.

Initial aggregate driver native83590 was actually observed exiting 1: its
expected inventory was storage 17 / total 53 instead of actual 4 / 40. All
functional child phases had naturally exited 0. Recovery driver native7660 was
actually observed naturally terminal with exit 0 after reconciling the actual
source inventory, 28 source pins, three immutable logs, five metadata files,
four executables and compile/link isolation. It reran no tests and changed no
source or functional logs. The historical failed receipt and earlier g2:18
wrong-header-pin preflight failure remain retained; the preflight failure
preceded native configure/build/test phases. The correct frame-layout header
is `zr_vm_parser/include/zr_vm_parser/exec_ir_frame_layout.h`.

The [evidence record](../../docs/acceptance/ssa-primitive-source-frame-oom.md)
records phase PIDs, full-suite inventory and source/log/metadata/binary hashes.

The unchanged production attachment module passed this finite gate. Acceptance
covers only the two direct Attach allocation sites, preservation and recovery
on the same live graph. Packed-helper allocations remain uninstrumented; no total heap
balance or exhaustive allocator OOM claim is made. Ordinary static/shared route
execution, full MSVC/Linux matrices, descriptor/native/retention acceptance,
sensitive boundaries and full SSA47 remain OPEN. Root owns the final Git
transaction; no new commit is assigned here.

See the [module guide](../../docs/parser-and-semantics/ssa-primitive-source-frame.md)
for the implemented attachment contract and historical normal-path proofs.
