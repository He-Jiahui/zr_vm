---
related_code:
  - tests/parser/test_ssa_aot_owned_module_faults.c
  - tests/parser/support/ssa_aot_owned_module_fault_shim.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
  - zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot_ir.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-execbc-vm.cmake
implementation_files:
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
  - tests/parser/support/ssa_aot_owned_module_fault_shim.h
plan_sources:
  - .codex/plans/20261007-ssa-owner-fault-gate.md
tests:
  - tests/parser/test_ssa_aot_owned_module_faults.c
doc_type: testing-guide
status: finite-core-owner-fault-green
---

# Core owner fault and arithmetic test guide

## Current state and exact inventory

The 588-line suite has seventeen RUN_TEST entries and main(void), using the actual
scalar 9/8 fixture chain. Direct r2 configure PID38808, build PID12372 and CTest
PID29268 naturally exited 0: one suite, 17 tests, zero failures/ignored, no UBSan
diagnostic. Its incremental build emitted zero warnings. MSVC r1 separately
compiled the real shimmed owner/test TUs with exit 0 and zero warnings; that is
two-TU compile evidence only. Previous lifetime commit
`78ce8caf737ed7d0a4f1dce3e377e1bd2b0904b8` proved its own finite lifetime profile.

| Case | Actual assertion distinction |
| --- | --- |
| Initial OOM/retry 9 | NULL destination preserved; one failed request then one same-byte successful candidate |
| Replacement OOM/retry 9 to 8 | Old owner and independent source preserved; retry frees old once and retains actual 8 |
| Exact self-source OOM/retry | Old View survives refusal; value-only witness validates new owner after publication |
| Invalid flags before allocation | Actual NOP unknown flag; direct Core diagnostic equals Clone; allocator remains untouched |
| Invalid contract before allocation | Actual signature mismatch; compare actual direct Core refusal, restore before assertions |
| Exact SIZE_MAX fit | SIZE_MAX-8 plus 8 fits; alignment 2 admits cursor 1 -> offset 2/end 3 |
| Multiplication overflow | SIZE_MAX count times width 2 fails, outputs unchanged |
| Padding overflow | SIZE_MAX cursor/nonzero count/alignment 2 fails, outputs unchanged |
| Extent-add overflow | SIZE_MAX-1 plus 2 fails independently, outputs unchanged |
| Zero count | SIZE_MAX and cursor 3 consume no alignment padding |
| Invalid width | Width zero rejected with nonzero and zero counts |
| Zero alignment | Rejected with nonzero and zero counts |
| Non-power alignment | Alignment 3 rejected with nonzero and zero counts |
| NULL cursor | False; offset sentinel unchanged |
| NULL offset | False; cursor sentinel unchanged |
| Status stability | Every enum/name 0..14 retained, OOM=15/out_of_memory, 999/unknown |
| Actual bound B | B positive power-of-two, B<=SIZE_MAX/2; B admitted, 2*B rejected for zero/nonzero counts |

Every listed case passed in the actual r2 CTest log. Numeric
boundary cases call the actual private helper via its conditional forwarding
wrapper; they allocate no fake enormous source arrays or huge arenas. Actual
AlignmentBound removes any guessed platform alignment constant.

## Ledger phases and atomicity

The eight-record fixed ledger observes only malloc/free sites in the shimmed
owner TU. One armed call returns NULL and disarms, recording attempts/failures/
positive requested bytes. Ordinary wrapper success calls CRT and records live
arena/bytes. Free clears its live pointer, records one free and decrements live;
unknown/duplicate free increments errors. Wrapper bookkeeping allocates no memory.

Each OOM case records baseline counters after setup, arms once, submits the real
valid source and disarms before assertions. Failure adds exactly one attempt and
one failure, zero successful allocations/free; old slot/View/source bounded
observations remain identical and validate. The nonzero-pattern diagnostic must
equal a wholly zeroed expected struct containing only OUT_OF_MEMORY and expected
request bytes. The unchanged-source retry adds one attempt/success, requests the
same bytes and leaves one live candidate. Replacement/self retry frees old once.
Explicit final Free requires live/errors zero, frees=successes and per-record
frees=1; repeat empty Free must leave counters unchanged before teardown fallback.

Initial mode has no old arena. Replacement creates an independent actual 8
producer alongside original 9. Self retry uses the old owner's exact View and
never dereferences that old view after success. Value-only witness/candidate
checks retain semantic/content hashes, actual literal, contracts/target/frame,
identity, ABI and empty logical-map identity/capacity normalization. Independent
live sources additionally permit byte-content/address copy assertions.

Invalid flags/contract cases keep allocation failure armed, call direct Core
validation and Clone, disarm and restore actual bounded fields before assertions.
They demand identical complete Core diagnostics and zero attempts/failures/free
delta. Refusal is not silently classified from the case label or counted as OOM.

## Actual-source object and registration

Direct OBJECT: `ssa_source_aot_owned_module_fault_object`; normal OBJECT:
`zr_vm_ssa_aot_owned_module_fault_object`. Both compile only the real owner C.
The private define enables CheckedSpan/AlignmentBound; forced include is /FI on
MSVC frontend or -include on the other normal route. The 24-line shim reads CRT
declarations before allocator macros. Test TU defines NO_REDIRECT; fixture,
Parser/Core support and all other TUs retain normal allocation. The installed
owner header has no fault symbols. Do not include production .c or duplicate
the helper formula. The fault executable links no scalar emitter support.

Executable: `zr_vm_ssa_aot_owned_module_faults_test`; CTest:
`ssa_aot_owned_module_faults`, no arguments. Exact selected regex is
`^ssa_aot_owned_module_faults$`. Normal registration labels are
`ssa;aot;allocator-fault`, timeout 120. Registration is present; normal route
execution remains unverified. Root owns builds, runs and finite Git work.

Sources remain direct `E:/Git/zr_vm`; existing build is
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`.
Reports use `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`, with temporary
drivers under `E:/cargo-targets/zr_vm/tmp`. No copied sources or alternate build
tree is required. Actual compile_commands/settings/source/header metadata identify
the shim solely on the independent object. Root read the pinned raw map: all three
owner APIs and both private wrappers resolve from that object. Abbreviated receipt
symbol excerpts do not replace these complete raw provider lines.

MSVC fault r1 receipt SHA-256 is
`FAC6B16062AA3474B41E0BF35DF67A67F871DD055EAF4F46CD13DA6A68C79951`:
owner PID15088/test PID13064 naturally exited 0, warnings 0. The
[acceptance record](../../docs/acceptance/ssa-aot-owned-module-faults.md) pins
current C272/shim24/test588 separately from the historical lifetime source.
No MSVC runtime or guessed fault commit follows from compile proof. The separate
runtime receipt `source-aot-owned-module-faults-r2-receipt.json` has SHA-256
`6E153DAC65CAE4AB75EDA9D5569AEF5CAD3F4853F85598E26279A4E175A71700`;
its CTest log has SHA-256
`8E3933655FEC849BFBF7F8BFF9C80A40F5F310F90FCA77B68792DDD960420748`.

The failed r1 receipt remains preserved: configure/build exited 0, but 231
Core/Library/Parser warnings stopped its driver before CTest, and six foreign
Parser inputs changed during that build. It receives no runtime credit. R2
reconfigured current inputs and incrementally rebuilt in the same directory.
Zero new warnings do not certify the whole project warning-free.

## Acceptance limits

Actual runtime proof is one no-argument CTest, seventeen Unity passes, zero
failures/ignored, no UBSan diagnostic and actual owner-symbol provenance.
Source/header/metadata/map/binary hashes remained unchanged through execution
and Root's subsequent verification. Whole-project diagnostics remain outside
this finite acceptance.
OS memory exhaustion, other linked allocators, corrupt-candidate validation cleanup,
general nonempty GC/state/deopt/CFG/multiple-function retention, Linux/full MSVC,
normal registration execution, default returned retention and full47 remain OPEN.
No generated text/native execution, SourceExecute/oracle/loader/network/FFI/
provider/security probe is part of this finite suite.
