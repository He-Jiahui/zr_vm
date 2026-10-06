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
  - tests/acceptance/ssa-primitive-source-frame-oom.md
doc_type: acceptance-evidence
status: primitive-source-frame-oom-focused-windows-green-accepted
---

# Primitive source frame OOM evidence

## Current decision

**Focused Windows direct-source OOM gate accepted.** Root registered and built
the standalone OBJECT and executable, then ran four complete suites under
Windows x64 clang-cl 19 Debug assertions and UBSan: host 15, frame 19, storage
4 and OOM 2, totaling 40 Unity cases, zero failures and ignored cases. Configure,
build and CTest each naturally exited 0. No UBSan diagnostics or build warnings
were observed. Final receipt has `validated: true`; Root's final Git transaction
remains pending, and this record assigns no new commit or RED run.

The [existing frame acceptance](ssa-primitive-source-frame.md) and
[storage supplement](ssa-primitive-source-frame-storage.md) remain historical
dependencies. Their counts, production compile results and receipts are not
fresh evidence for this OOM executable. The existing source prerequisites
cover actual 9/8; each new OOM case prepares its own real NINE graph through
the same shared source fixture, same-context dead-place compaction and actual
host i64 row/module append. This new test performs no Oracle or native/backend
execution.

## Finite observations passed

| Case | Actual production allocation | Failure checks | Same-graph retry |
| --- | --- | --- | --- |
| `test_values_calloc_failure_same_graph_retry` | `exec_ir_source_frame.c:313`, `calloc(valueCount, sizeof(SZrExecIrPackedValue))` | One selected fault, exact geometry/OOM diagnostic, zero direct successful allocations or temporary owners, whole inputs unchanged | Fault disabled; actual Attach succeeds on the original live graph. |
| `test_header_malloc_failure_same_graph_retry` | `exec_ir_source_frame.c:337`, `malloc(sizeof(SZrExecIrFrameLayout))` | One selected fault, exact geometry/OOM diagnostic, values freed, real packed cleanup called and owners cleared, whole inputs unchanged | Fault disabled; actual Attach succeeds on the original live graph. |

Each diagnostic comparison covers the complete bytes: the test prefills the
output and expects only OOM code and actual functionToken in an otherwise zero
record. It observes complete graph/header/contract identity and all active
graph pools, original source graph, live SemIR/context and module tables.
Source maps and caller owners remain independently owned. Retry does not
rebuild, clone, restore or compact the graph again. It allows only the two
successful publications, checks actual ValueId/TypeId frame slots and geometry,
then runs Core VERIFY_ALL and the shared source-map correspondence assertions.

## Instrumentation and ownership boundary

Root compiled the unchanged attachment production C as a separate OBJECT
target with the test shim forced into that TU alone. The shim reads normal
stdlib and real packed-layout declarations before applying macros. The test
TU suppresses those macros so its wrappers call real CRT/helper functions.
Ordinary parser and fixture compilation remain uninjected. This uses the
normal parser static archive for real dependencies, with no whole-archive,
source copy, `include.c`, production test hook or process-wide CRT hook.
Actual compile commands contain exactly two production frame TU entries and
one forced-include entry belonging solely to the standalone OBJECT. The normal
parser entry has no shim. The sole Attach link-map record resolves to explicit
`exec_ir_source_frame.c.obj`, without a parser-archive member prefix.

Bookkeeping is a fixed global ledger; wrappers allocate no bookkeeping and
never Unity-assert inside the production call stack. Setup registers global
owners before its first assertion. After Attach returns, publication is
registered before any assertions, including an unexpected publication.
Successful header ownership transfers to the output graph; its slots transfer
with it. Teardown disables failure injection and calls real Core FreeFunction,
releases any direct temporary owner left by a failing assertion, then frees
fixture and state. A shallow observed graph header is never treated as an owner.

| Stage | Direct successful allocations | Wrapped non-NULL frees | Direct temporary owners | Header transferred to graph |
| --- | ---: | ---: | ---: | ---: |
| values calloc fault | 0 | 0 | 0 | 0 |
| header malloc fault | 1 | 1 | 0 | 0 |
| successful retry, fresh ledger | 2 | 1 | 0 | 1 |

The separate cleanup-call wrapper invokes original PackedFrameLayoutFree and
checks the still-live packed header after return. In the header-fault path it
observes owned slots/maps before that real cleanup; after successful publication
it observes no packed frame slots and still-owned maps. The actual helper
releases its maps, and Core FrameLayoutFree releases its slots on failure.
This is call/owner-clearing evidence, not an individual heap-free count.
Packed construction's additional ordering, map and slot allocations are not
fault-injected here. Core teardown frees of the published header/slots are
outside the wrapped direct count. No total-heap-balance or exhaustive OOM claim
is available from these two cases.

## Actual execution and immutable evidence

Build: `E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`.
Reports: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`.

| Actual phase | PID | Observed completion |
| --- | ---: | --- |
| Configure | 31472 | Actual WaitForExit, naturally terminal, exit 0 |
| Build | 37572 | Actual WaitForExit, naturally terminal, exit 0 |
| CTest | 17860 | Actual WaitForExit, naturally terminal, exit 0 |

| Complete CTest suite | Unity cases | Failures | Ignored |
| --- | ---: | ---: | ---: |
| `ssa_host_primitive_layout` | 15 | 0 | 0 |
| `ssa_primitive_source_frame` | 19 | 0 | 0 |
| `ssa_primitive_source_frame_storage` | 4 | 0 | 0 |
| `ssa_primitive_source_frame_oom` | 2 | 0 | 0 |
| Total | 40 | 0 | 0 |

The two OOM cases passed diagnostic, preservation, cleanup and retry assertions
on their original live graphs. The other three full suites passed as focused
consumer regressions. This gate performed no source Oracle, canonical
projection or native/backend execution.

### Recorded hashes

Paths are relative to the repository, report directory or build directory above,
as identified in the first column.

| Scope / file | SHA-256 |
| --- | --- |
| Repository: `tests/cmake/ssa-source-direct-validation/CMakeLists.txt` | `0AF636903118D4AAD85BF6BE9BE67210ECEBBB96E1F6428CA744B7C5C73323A3` |
| Repository: `tests/cmake/ssa-source-execbc-vm.cmake` | `65D85B31E96B63911C00B6B99B3083045304DAAB92B7074DEB23BB46755E05E2` |
| Repository: `tests/parser/support/ssa_primitive_source_frame_oom_shim.h` | `989A8CFB625EFC1079F4E1636A2A72BD55228338F9DE5B29870D2DBEB39C363E` |
| Repository: `tests/parser/test_ssa_primitive_source_frame_oom.c` | `F5288DD68494160F1E8EF4F6F7288E9D79CA781B379CC7E1BD939FE786885C7C` |
| Repository: `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c` | `B1E2FA4EFE39ECD120034FFB7DFA9F7DF550120A08CA04FA7C010287CF367AD3` |
| Reports: `primitive-frame-oom-configure.log` | `7FCF9D63607050EFE787BA1397F17F9E5D1B6608F0C19E4E811A04164065B61A` |
| Reports: `primitive-frame-oom-build.log` | `3AED5A5DA07EA0EDE2179E6B03B59D0E2DFF73DBABE00418D205724BD81FBD06` |
| Reports: `primitive-frame-oom-ctest.log` | `D95AEAF5CAD69D473BA28A8483AE92A14C171165B18DCF69460C329B0D8DAE28` |
| Build: `source-direct-target-sources.tsv` | `D19FF5B5A09E2C506B340CF9489AEC44AA49D48E3703F2830F33093E283881BB` |
| Build: `source-direct-target-settings.tsv` | `32813EE8B1A69F3A02828B9DC5ACDECE064DD0E4EB90E34FFA84C347F33069E0` |
| Build: `source-direct-header-hashes.tsv` | `441D2FA5DF1BA5EB20E7D53E94FA4DFAF204F76FDF723E7C47A10778A0DA549B` |
| Build: `compile_commands.json` | `AB5923ABB7C8D1707C8747B49AF4DD3379B36A1E3B4E5F8178FDB1FDC2761905` |
| Build: `ssa-primitive-source-frame-oom-link.map` | `8892AE773F5B9C8F4303F6DF4DD65373455052B249CB442E3F4F95174F1DFDC1` |
| Build: `bin/zr_vm_ssa_primitive_source_frame_oom_test.exe` | `5EE579CC1AEBA5235C133BC54BAA712BD510274F1B8524ED5D4531199E3C648E` |
| Build: `bin/zr_vm_ssa_primitive_source_frame_test.exe` | `1D8933B1CAEB2AB1BCA62A98BA6E3D0ECB2C1B09A32947ECB56D0D1C2E0BFA7E` |
| Build: `bin/zr_vm_ssa_primitive_source_frame_storage_test.exe` | `3C8E94249F4FF25D790C7CEA15EDBF6589598EC815B62DB08C257F549F36D323` |
| Build: `bin/zr_vm_ssa_host_primitive_layout_test.exe` | `391E7B8CD1FF4D8F5AA31FF4E7B1BFE0AD0ED3029E80E336F44D346CEA7F6DE8` |

Final receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/primitive-frame-oom-r2-receipt.json`,
SHA-256 `522FA6E2E2CD2FF8776EAE86BA682DF1EB1E3511478D6B26F217350E79DEEC87`.
All 28 source pins, five metadata files, four executables and three immutable
logs were freshly checked against the receipt and matched. Historical receipt:
`primitive-frame-oom-receipt.json`, SHA-256
`A0AF1B6825597C3F68941C0B4EE19286D8DCC59126253A948F02E88A97D329E0`.
Earlier preflight record: `primitive-frame-oom-preflight-failure.json`.
Both historical records remain in the same report directory.

### Retained failures and evidence reconciliation

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

## Acceptance limits

The unchanged production attachment module passed this finite gate. Acceptance
covers only the two direct Attach allocation sites, preservation and recovery
on the same live graph. Packed-helper allocations remain uninstrumented; no total heap
balance or exhaustive allocator OOM claim is made. Ordinary static/shared route
execution, full MSVC/Linux matrices, descriptor/native/retention acceptance,
sensitive boundaries and full SSA47 remain OPEN. Root owns the final Git
transaction; no new commit is assigned here.

See the [module successor](../parser-and-semantics/ssa-primitive-source-frame.md)
and [test guide](../../tests/acceptance/ssa-primitive-source-frame-oom.md).
