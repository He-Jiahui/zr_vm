---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/function_aot_ir.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_aot_ir.c
  - zr_vm_core/src/zr_vm_core/function_aot_ir_internal.h
  - zr_vm_core/src/zr_vm_core/gc/gc_internal.h
  - zr_vm_core/src/zr_vm_core/string.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - tests/core/test_ssa_function_aot_owner.c
  - tests/core/support/ssa_function_aot_owner_fixture.h
  - tests/core/support/ssa_function_aot_owner_fixture.c
  - tests/gc/gc_test_utils.c
  - tests/parser/support/ssa_aot_owned_module_fault_shim.h
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_core/src/zr_vm_core/function_aot_ir.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_core/src/zr_vm_core/gc/gc_internal.h
plan_sources:
  - docs/plans/ssa
  - .codex/plans/20261008-ssa-function-owner-source-handoff-v2.md
tests:
  - tests/core/test_ssa_function_aot_owner.c
  - tests/acceptance/ssa-function-aot-owner.md
doc_type: milestone-detail
status: function-owner-functional-red-validated
---

# Finite Function AOT metadata ownership acceptance

## Prerequisites and current boundary

Core independent ownership is committed as
`78ce8caf737ed7d0a4f1dce3e377e1bd2b0904b8` after actual lifetime validation.
The seventeen-case allocation-failure/geometry gate is committed as
`8f7cc4b1aa3a5407b0569a30f5f83e39c1bb1ff1`. Function integration now has safe
RED scaffolding and an independent twelve-case actual Core lifecycle design.
Two actual twelve-case attempts exposed a lower shutdown allocation-size
failure. After the production size repair, a third actual twelve-case run
established functional RED; Function GREEN remains OPEN.

| Gate | Current evidence |
| --- | --- |
| Core owner lifetime and arithmetic/OOM prerequisites | Previously committed finite GREEN |
| Function safe RED source and lifecycle hooks | Frozen source; lower allocation-size repair source-reviewed and actually validated |
| Twelve-case functional RED | Actual r3: three prerequisites pass, nine feature failures at UNSUPPORTED, zero ignores |
| Actual Snapshot publication and twelve-case GREEN | OPEN |
| Finite MSVC translation-unit checks | Three new translation units compiled with exits zero and zero warnings |
| Normal CMake route and full MSVC/Linux matrices | OPEN |
| Parser Source_Compile retention and full forty-seven SSA leaves | OPEN |

Source review alone does not close any execution or acceptance row. Artifact 6
and AOT metadata ABI 17 remain unchanged; current-sized rebuilt Function records
are required. See the [module contract](../core-runtime/ssa-function-aot-owner.md)
for API, move, borrowed-shell and lifetime rules.

## Preserved actual attempts

Reports are under `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`.
Both direct attempts configured and built successfully, then ran all twelve
Unity cases with ten failures and zero ignored cases. Two Core prerequisites
passed; nine feature cases failed at the actual UNSUPPORTED status 14; the
absent-owner case failed because its strict allocator error count was 38.
No VM allocation remained live after shutdown. Failed receipts remain unchanged.

| Evidence | SHA256 |
| --- | --- |
| RED r1 receipt | `BF83ACF374B7BAFC4A053E2DF6D972B6D86BCE1AC43CB63A6F427975D49889F4` |
| RED r2 diagnostic receipt | `8461F48803B9A16291D56348EF496707D3FBC25176658AC5242D237CE1BE4BCB` |
| RED r2 raw CTest log | `3C85078A79EC650FD5A9E780B1E760D16CDB26943AD9DEA42FCA3E08AE5420FB` |
| Finite MSVC r2 receipt | `CFB2E23A4D1C227AEAF947C653A3D305EA35F3F522C2C43C5D352BAD48ABB39F` |
| Root's verified r1 warning comparison | `AFFF1536CBF47A158BE449DB7EE1AED35152A40C1A0687B5A59316B472D2D6A7` |
| Accepted functional RED r3 receipt | `781081AB25A9F4123715043D2B23D22ED11B5067573CA70411949A1280E5B2B3` |
| RED r3 raw CTest log | `D6C7F0F31FE10CA48C40FB1DCDF388491DE5A719299BB318D743A1ED2391096C` |

Diagnostic r2 recorded exactly 380 size mismatches across ten real shutdowns:
all allocated 335 bytes, freed with original-size arguments 212 through 226,
and used matching OBJECT flags. The fixed short-string constructor capacity
and the GC logical-length-based free size disagree. This lower failure is
repaired before Snapshot implementation; the strict ledger stays unchanged.

The r1 Core rebuild emitted 174 warnings. Independent review and Root's raw-log
comparison found 173 byte-identical messages from the preceding build; one
existing size-limit warning changed its constant because Function grew from
1192 to 1200 bytes. No warnings arose from the new module or test/fixture.
The r2 incremental build emitted zero warnings; this does not erase the r1
diagnostics or establish whole-project warning acceptance. A failed earlier
warning derivative contains JSON null and is not acceptance evidence; the
verified `source-function-aot-owner-red-r1-root-warning-provenance.json` is
the separate valid comparison record.

## Accepted functional RED and support repair

Actual r3 driver PID 36516 ran configure/build/CTest children
29012/6412/35596 to natural exits 0/0/8. CTest exit 8 is the genuine expected
feature failure, not an inverted CTest pass. All twelve default cases ran:
the raw module, independent owner and absent-Function shutdown cases passed;
the remaining nine each failed at the actual Snapshot UNSUPPORTED value 14.
The raw log contains no allocator diagnostics and no UBSan diagnostics.
The exact zero-live and zero-error assertions in the absent-owner shutdown
case passed without relaxation.

The production fix changes only the short-string base-size expression to match
fixed allocation capacity; long-string sizing remains unchanged. Independent
source review checked constructors, concat/intern routes, Parser budgeting,
GC free and the common clone-size consumer. Four existing synthetic GC STRING
callers also needed their test helper's minimum allocation reconciled with
the same layout. That helper repair is source-reviewed; the broad GC runtime
suite, which links Library/System and has no case selector, remains unrun.

R3 rebuilt affected Core translation units and emitted 154 warnings, all
byte-identical to the r1 log. Source, header, settings and selected link
provenance were pinned before and unchanged after the run. The raw map selects
the actual private owner OBJECT and static Core providers required below.
This accepted RED permits Snapshot implementation; it is not Function GREEN.

## Required actual proof

The no-argument target must register exactly twelve default Unity cases:
two actual Core prerequisites followed by ten Function cases. The absent-owner
lifecycle case also passes under the safe stub. The other nine feature cases
must first fail at the actual UNSUPPORTED Snapshot assertion; missing symbols,
bad fixtures, invalid metadata, loader failures or a crash are infrastructure
failures and cannot establish functional RED.

GREEN requires all twelve cases passing with zero failures, zero ignores and
no UBSan diagnostic. Arena and VM allocation ledgers must show correct lifetime,
zero live records after actual GlobalState shutdown and no bookkeeping errors.
Replacement and exact self-source preserve pointer-free semantic witnesses;
validation and one-site OOM preserve the live owner and actual diagnostics;
move/borrowed/owned descendant cleanup uses real Core records and collector
dispatch. A default target run must not omit any case to obtain acceptance.

The link map must select owner Clone/View/Free from the separately compiled
actual fault OBJECT, while Function/GlobalState routines come from actual static
Core. Parser, Library, runtime_support, scalar emitter archives and provider
implementations must be absent. The allocator shim is forced on one production
owner OBJECT only. No copied source or replacement Function implementation is
accepted. Fresh source/header/graph/command hashes and actual child PIDs,
WaitForExit codes and immutable raw logs are recorded under the approved E roots.

Compiler warnings are recorded and reviewed before commit; no whole-project
zero-warning claim follows from a finite target result. Finite MSVC compilation
does not imply the full MSVC or Linux matrices passed. The Parser alias-clear
hook is source-reviewed; the Core test does not claim actual Parser execution.

All operations use the current checkout directly. The user reconfirmed
`E:/cargo-targets/zr_vm` for outputs. No source snapshot, D output, cross-drive
output move, source execution, Oracle, native generated program, provider,
network, FFI, capture, monitor or security probe is included. Existing foreign
source/index/jobs/leases are preserved; commits contain only reviewed exact paths
and use He-Jiahui <814111601@qq.com> as both author and committer.
