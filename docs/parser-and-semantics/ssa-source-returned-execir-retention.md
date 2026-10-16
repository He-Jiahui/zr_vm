---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention_internal.h
  - zr_vm_core/src/zr_vm_core/function/function_exec_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
  - tests/parser/test_ssa_source_returned_execir_retention.c
  - tests/parser/support/ssa_source_returned_execir_owner_shim.h
  - tests/parser/support/ssa_source_returned_execir_frame_shim.h
  - tests/parser/support/ssa_source_returned_execir_fixture.h
  - tests/parser/support/ssa_source_returned_execir_fixture.c
  - tests/parser/support/ssa_source_returned_retention_fixture.h
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - tests/parser/support/ssa_source_returned_execir_cleanup.h
  - tests/parser/support/ssa_source_returned_execir_cleanup.c
  - tests/parser/support/ssa_source_returned_crt_ledger.h
  - tests/parser/support/ssa_source_returned_crt_ledger.c
  - tests/core/support/ssa_exec_ir_owner_capture.h
  - tests/core/support/ssa_exec_ir_owner_capture.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/cmake/ssa-source-returned-execir-retention.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
  - tests/parser/support/ssa_source_returned_execir_cleanup.c
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - tests/core/support/ssa_exec_ir_owner_capture.c
plan_sources:
  - .codex/plans/20261009-ssa-source-returned-execir-retention-design-v1.md
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
  - .codex/plans/20261009-ssa-source-finalized-handoff-addendum-v1.md
tests:
  - tests/parser/test_ssa_source_returned_execir_retention.c
  - docs/acceptance/ssa-source-returned-execir-retention.md
doc_type: module-detail
status: implemented-validated-finite
---

# Returned source ExecIR: finalized storage handoff

## Current implementation and finite scope

The additive mode `ZR_SOURCE_RETENTION_REQUIRE_SCALAR_EXECIR=4` uses the real
`ZrParser_Source_CompileWithOptions` route. NONE0 and scalar AOT1 keep their
contracts; these are exact enum values. Unknown2, mixed1|4=5 and other unknown
modes refuse at OPTIONS/INVALID_ARGUMENT before producer preparation. NULL or
zero options remain NONE. Both REQUIRE modes reject submission context/output.

Current status is implemented-validated-finite for this handoff. After
genuine layout append, mode4 runs AttachPrimitiveSourceFrame, VerifyFunction_ALL
and the actual eleven-argument finalizer. Direct Source22, direct AOT25 and the
exact two-test CTest selection now pass with natural exit0. This supplies finite
functional evidence, with no release, fullW0 or whole-leaf acceptance. Earlier
diagnostic and generic-snapshot failures remain historical evidence in the
[acceptance record](../acceptance/ssa-source-returned-execir-retention.md).

The accepted run uses independently recorded shared-checkout inputs, including
external migrations. Finite commit scope excludes foreign changes; the run does
not establish a clean full build from the finite commit alone.

The finite inputs remain existing literal9/8 I64 SCRIPT programs, bool/float
refusals and existing invalid-input checks. No imports, captures, calls,
compile-time programs or returned Function execution are added. Existing AOT
retention has its own [contract](ssa-source-returned-retention.md).

## Genuine finalized handoff contract

The accepted design replaces the earlier preframe original-module snapshot.
The late compiler hook runs with the selected Function, CompilerState, AST,
SemIR, semantic context and both constant pools alive. The genuine bridge,
BuildModule, constant append, original Verify, module-contract bind, compaction
and layout append create an original module and a separate compacted function.
The implemented mode4 branch runs the existing AttachPrimitiveSourceFrame
and VerifyFunction_ALL on that compacted function first, then calls the private
`source_snapshot_finalized_primitive_module` once before the AOT projection path.

Its eleven typed arguments are the actual SemIR function, semantic context,
script AST, source callable identity, Compiler constant array, Function constant
pointer/count, original module, finalized compacted function, selected
`ownedExecIr` slot address and optional lower diagnostic. The test-only wrapper
passes every argument unchanged to the real helper. No Function is reconstructed
from the slot, no container-of calculation is used, and no later
FunctionSnapshotExecIr call performs a second snapshot.

The original supplies complete module constants, layouts, contract and identities.
The selected replacement supplies the genuine compacted instructions, maps and
attached frame. The helper validates original/finalized relations and source
facts, clones the complete module replacing the selected function, verifies the
candidate and frame, then creates independent owned module storage. Constant
pool indices remain the original indices; they are not renumbered through AOT
projection. Compiler and Function constant pools are separate readable inputs.
Published logical module content includes the replacement function; equality
with the whole uncompacted original function is therefore not the output contract.

The selected current Function must have no attached owner for this Source route.
The lower helper's destination transaction preserves existing output on refusal
and replaces it only on success. All temporary candidate storage is released
before returning. The Source route publishes ExecIR only, without AOT ownership.
Capacity normalization follows the existing Core copy/clone contract; spare
capacity, padding, registry ownership and executable admission are not promised.
Numeric canonical IDs retain identity without retaining the registry itself.

## Independent witnesses and ownership records

The current test records three separate pointer-free observations: original
module/full logical rows, finalized compacted function/frame, and published owned
module. Its expected logical source rows are derived from the genuine SemIR,
source identity and constant facts. Inputs are checked while their typed storage
is live, including observations before/after direct helper refusals.

Whole helper attempt records include temporary copies and final owner storage.
Successful publication classifies actual still-live records into an explicit
output-ID set; temporary records must already have one free. Live-span reads
bind genuine returned owner/view pointers to those records and check independence
from producer addresses. A numeric record or span alone is not Function/VM
provenance. After Function free or Global shutdown, only saved numerical facts
and independent witnesses are read.

`metadataTokenRecords` are checked on successful returned Function identity:
module and script-entry token relations and signature identity must agree with
the published module. The test does not snapshot every metadata record before
and after failed compilation; no universal failure-preservation claim is made.

## Failure diagnostics and cleanup

Finalizer refusals map to EXECIR_OWNER through the existing Source mapper. All
nine lower fields are preserved: code, functionToken, blockId, instructionId,
sourceId, expectedVersion, actualVersion, expectedHash and actualHash. OOM maps
to Source OUT_OF_MEMORY, Unsupported to UNSUPPORTED, and other lower errors to
PRODUCER_FAILED. Attach/Verify failures remain FRAME. A real live SemIR source
map supplies offsets and signed coordinates before cleanup; sourceId0 or an
unmapped refusal supplies no invented range. Success clears the complete Source
diagnostic; NULL diagnostic behavior uses the existing local normalization.

Producer cleanup remains descriptor, projection, compacted function, original
module, followed by caller Function/CompilerState and AST cleanup. Observers use
nonasserting TryCapture/TryArm/TryEvent/TryRecordFrees, save errors numerically
and disarm before release. Close performs real shutdown, saves VM statistics and
ledger health before recovery, then invokes accepted cleanup support. Assertions
consume the saved first report afterward; recovery cannot turn unhealthy prior
state into a passing observation, and Init checks prior health before Reset.
Existing asserting setup/Compile wrappers do not establish prior-failed Unity
Source entry or ignored-test execution.

## Prerequisites and remaining evidence

The lower [finalized-module acceptance](../acceptance/source-finalized-module.md)
is committed as `d475d47494d5b7566e258dd379466ca099b3da6f`: its declared-profile
24 cases, measured89 faults and16 lifetime cycles are finite lower evidence.
Those lower observations do not supply the genuine Source allocation denominator.
The current Source22 run preserves the existing twenty names/order and separately
measures/completes156/156 whole-helper faults for literal9 and156/156 for literal8.
It checks request traces, same-input direct refusal, candidate cleanup and a
disarmed retry in a fresh balanced Global. The exact protocol contains341 FEATURE
rows and healthy FINAL with closed=1 support=1 and zero saved VM/owner/observer
live or error counters.

Two appended cases now target the actual values calloc and frame-header malloc
inside AttachPrimitiveSourceFrame. The observer arms only that real call, saves
its lower diagnostic and pointer-free input witness, and records candidate frees
before Global shutdown. Values failure must stop before the header request;
header failure must release values and packed storage. Neither failure publishes
a frame or reaches the finalizer. A disarmed retry creates a fresh Global,
publishes the genuine frame/header/slots, calls the finalizer and proves release.
Both cases pass with exact values/header FRAME_OOM rows, each failed=1 retry=1.

The actual selected graph has eighteen roles,786 TUs and29 FI TUs. The original
private Parser10 is split into Parser8 and Frame2. Only Frame2 receives the frame
shim; the observed producer uses its typed AttachFrame wrapper. Initial775
compilations/two links are followed, after shared input changes, by an actual
27-compilation/two-link refresh with759 exact cached objects and811 unique VALID
dependency paths. Current command/provider/dependency evidence accompanies the
Source22/AOT25/CTest2 passes. The initial build emits61 warnings and refresh22;
full_current_w0 and release_accepted remain false.

The current receipt and runtime validator are under
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/source-current-direct/`;
their exact hashes and retained verifier-failure history are in the
[acceptance record](../acceptance/ssa-source-returned-execir-retention.md).
Ninja dependency and archive-member relative-path handling, and the exact CTest
environment preamble were corrected as metadata interpretation of existing raw
evidence, without rerunning those native phases.

Four child NULL/zero cases injected through Source remain open. Separate lower
AST checks at `963a81af3502d11361cab3dd64a9c0b9a5dc46a7` do not supply this coverage.
Full strict warning-free validation, Function/source-family completion, default
canonical publication, registry lifetime, Linux/full MSVC, normal-project runtime,
backend/native parity and all47 remain OPEN. No SourceExecute, Oracle, returned
Function call, generated native code, network/FFI or security probe is credited.

- [Source22 protocol and allocation closure](../testing-and-validation/ssa-source-returned-execir-retention.md)
- [Finite functional evidence and remaining gates](../acceptance/ssa-source-returned-execir-retention.md)
