---
related_code:
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
  - zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot_ir.c
  - tests/parser/test_ssa_aot_owned_module_faults.c
  - tests/parser/support/ssa_aot_owned_module_fault_shim.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-execbc-vm.cmake
implementation_files:
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
  - tests/parser/support/ssa_aot_owned_module_fault_shim.h
plan_sources:
  - .codex/plans/20261007-ssa-owner-fault-gate.md
  - .codex/plans/20261006-ssa-core-scalar-owner-design-v2.md
tests:
  - tests/parser/test_ssa_aot_owned_module_faults.c
  - tests/acceptance/ssa-aot-owned-module-faults.md
doc_type: acceptance
status: finite-core-owner-fault-green
---

# Core owner allocation failure and private geometry acceptance

## Current decision

**Finite 17-case runtime validation is GREEN.** The actual owner C/shim/test have
272/24/588 lines. Direct r2 configure PID38808, build PID12372 and CTest PID29268
all naturally exited 0. One no-argument suite passed seventeen Unity tests,
with zero failures/ignored and no UBSan diagnostic. Its incremental build emitted
zero warnings. Source/header/metadata/binary/map hashes were unchanged through
execution and Root's subsequent read-only verification. MSVC fault r1 separately
compiled the shimmed owner and test TUs with zero warnings; that is compile-only.

The r1 failed receipt remains preserved. Configure PID8948 and build PID36504
exited 0, but 231 Core/Library/Parser warnings stopped its strict driver before
CTest; six foreign Parser inputs also changed during that build. It receives no
runtime credit. R2 freshly configured and incrementally rebuilt current inputs
in the same approved directory. Zero new incremental warnings do not resolve
those 231 diagnostics or establish a warning-free whole project.

Prior actual scalar lifetime acceptance is committed at
`78ce8caf737ed7d0a4f1dce3e377e1bd2b0904b8`. Its three selected suites passed
52 Unity cases, within the [historical lifetime acceptance](ssa-aot-owned-module.md).
It did not inject allocation failure or directly test private arithmetic.
The [Core module contract](../core-runtime/ssa-aot-owned-module.md) preserves that
proof and distinguishes the new private gate. The [fault guide](../../tests/acceptance/ssa-aot-owned-module-faults.md)
details the seventeen current expectations.

## Actual source and narrow interception

The test uses the actual literal SCRIPT 9/8 producer chain: contract binding,
compaction retaining source maps, primitive layout/frame, canonical callable
projection, host target and descriptor. Two independent source bundles support
replacement without mutating one source to impersonate another. Source validity,
ABI, semantic hash and bounded live observations are taken before injection.

An independent OBJECT compiles the real checkout owner TU with
`ZR_TEST_SSA_AOT_OWNED_MODULE_FAULTS=1` and its private forced shim. The shim loads
CRT and owner declarations before redirecting only that TU's malloc/free sites.
The test defines NO_REDIRECT, retaining real CRT calls in its wrapper bodies.
The conditional CheckedSpan wrapper calls the production helper itself;
AlignmentBound reports its actual ordinary-alignment upper bound. Both symbols
are absent from public installed headers and ordinary production objects.
No production .c inclusion, duplicate geometry, source copy, snapshot or global
allocator redirection supplies this evidence.

Direct registration uses `ssa_source_aot_owned_module_fault_object` and
`zr_vm_ssa_aot_owned_module_faults_test`; one no-argument CTest is
`ssa_aot_owned_module_faults`. It links the shimmed object explicitly before
ordinary Parser/Core/library support and does not link scalar emitter support.
Actual compile commands identify two owner routes: ordinary Core and independently
shimmed OBJECT. Only the latter receives the forced header/private test define.
Source metadata records one test, one actual fixture and one actual owner source
in its OBJECT. Root read the raw r2 map: all three public owner APIs and both
private wrappers resolve from the direct owner object, without a Core archive
prefix. The driver's stored symbol excerpts were abbreviated by PowerShell's
automatic Matches variable; the pinned raw map supplies complete provider lines.
Normal registration is present but has not been actually run.

## OOM and refusal assertions passed

The three OOM cases each isolate one failed allocation attempt from old-owner
setup. Initial failure leaves NULL; replacement and self-source failure preserve
old slot/view, all bounded live spans, semantic observations and source validity.
Each requires one attempt/failure hit, zero successful allocation/free delta and
positive captured request bytes. The whole poisoned diagnostic must become a
zero-initialized expected record with status OUT_OF_MEMORY=15 and expected=request
bytes: actual, IDs and index remain zero. Retry must request the same measured
bytes, succeed with one candidate, and free the old owner exactly once when present.

The fixed eight-record ledger tracks only real owner-TU calls, without assertions
or dynamic bookkeeping in wrappers. Successful arenas receive one record;
free clears its live pointer before calling CRT. Duplicate/unknown free is recorded
as an error. Tests explicitly assert zero live owners/errors and one free per
successful arena, then prove repeated empty Free changes no free count. Teardown
fallback does not count as successful cleanup proof.

Self-source uses the live old View on failure, then only saved value observations
after retry. It does not compare/dereference the freed old arena. Candidate validity,
ABI, literal, contract/target/frame/source content and present-empty logical-map
identity/capacity normalization are checked. Nonempty copied spans have independent
addresses while both source/candidate remain live. Real sources have no GC header
and an empty logical header, not general nonempty maps.

Two bounded invalid-source cases arm failure but expect Core rejection before
allocation: NOP flags outside KNOWN_MASK and function-contract signature mismatch.
The original fields are restored before assertions. Clone must return the actual
direct Core refusal and whole diagnostic, with no allocator attempt/failure/free
delta and unchanged source/old view. These cases are not OOM injection proof.

## Numeric helper and status inventory

Twelve remaining cases cover exact SIZE_MAX fit (and real alignment-2 padding),
multiply overflow, padding overflow, final-add overflow, zero count without
padding, zero width, zero alignment, non-power alignment, NULL cursor, NULL offset,
status stability and actual alignment bound. Failure preserves output sentinels.
Width/alignment validation must precede zero-count handling. Actual B is positive,
power-of-two and safely doubled; B is admitted and 2*B rejected for zero/nonzero
counts. These are numeric helper calls: no huge malloc/memcpy or fake array is
used. Status assertions require exact old enum values/names 0..14, OOM=15 with
`out_of_memory`, and unknown cast 999 returning `unknown`.

## Current compile evidence and source pins

Reports are under `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`.
MSVC r1 owner-fault-object PID15088 and test PID13064 naturally exited 0, each
with zero warnings. The receipt records validated=true and compile_only=true.
Its compile arguments use actual checkout TUs, /TC /std:c11 /experimental:c11atomics
/W4 /UNDEBUG /c; only the owner receives the fault define/forced include.

| Evidence | SHA-256 |
| --- | --- |
| source-aot-owned-module-faults-r2-receipt.json | 6E153DAC65CAE4AB75EDA9D5569AEF5CAD3F4853F85598E26279A4E175A71700 |
| source-aot-owned-module-faults-r2-ctest.log | 8E3933655FEC849BFBF7F8BFF9C80A40F5F310F90FCA77B68792DDD960420748 |
| source-aot-owned-module-faults-r2-link.map | A71554F0D4845416D26E3C5D9E5CF59B1EA3CAE8B0A6E2E96065AE8022CF69C0 |
| fault executable | BF5E713CE49E8F08113AE0E35FAEB48C796C01213AD5E846C5CB0E5C4356EB69 |
| source-aot-owned-module-faults-msvc-r1-receipt.json | FAC6B16062AA3474B41E0BF35DF67A67F871DD055EAF4F46CD13DA6A68C79951 |
| owner C (272 lines) | 312E94D857FE5B74610EF4C4894B55A65E2AF0648ABB0DDCE76FD605984EC3AC |
| fault shim (24 lines) | 2B1541C8DC8D382740880D4F5EBF566EC70E62AD779F1C5F189F0A501E8A0267 |
| fault test (588 lines) | 780EC15C513136D5CFC1D30FA016EF1A07182DB65F1F74F6C259D14A181F5401 |

These are current fault pins, separate from the prior 259-line lifetime C pin.
The r2 receipt includes 739 target/source graph rows and 498 production-header
pins; its logs and recorded artifacts were verified against stored hashes.
No new finite commit identity is guessed.

## OPEN scope

The actual owner allocation point and private arithmetic profile are admitted.
OS exhausted memory, other
allocators, internal corrupt-candidate validation cleanup, concurrency, normal-route
execution, Linux/full MSVC, default returned retention and general nonempty
GC/state/deopt/CFG/multiple-function retention remain OPEN. Full47 is OPEN.
This gate neither emits nor executes C/LLVM/native code and performs no
SourceExecute/oracle/loader/network/FFI/provider/security execution.
