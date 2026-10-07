---
related_code:
  - zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot_ir.c
  - tests/parser/test_ssa_source_aot_owned_module.c
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-execbc-vm.cmake
implementation_files:
  - zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
plan_sources:
  - .codex/plans/20261007-ssa-core-owned-module.md
  - .codex/plans/20261006-ssa-core-scalar-owner-design-v2.md
tests:
  - tests/parser/test_ssa_source_aot_owned_module.c
  - tests/parser/test_ssa_source_aot_scalar_text.c
  - tests/parser/test_ssa_source_aot_descriptor.c
  - tests/acceptance/ssa-aot-owned-module.md
doc_type: acceptance
status: finite-core-owner-genuine-red
---

# Independent Core owner finite acceptance

## Current decision

**Finite genuine lifetime RED is proved by actual r1.** Public APIs, the true
Core validation stub, the 677-line lifetime test and both CMake registrations
exist. Configure/build and both prerequisite cases passed; both isolated feature
cases failed only at the Clone-success assertion. This establishes the missing
ownership feature with functioning prerequisites, without functional GREEN.
Valid Clone input currently returns UNSUPPORTED (14), with no allocation and no
destination write. Successful ownership, producer teardown retention, replacement,
error atomicity with a live owner and allocation failure remain unproved.

The previous scalar source-text GREEN commit
`f111fb610f2e12748b2df60b7c1a63431f8abf1f` is a prerequisite only: five selected
suites, 42 Unity cases and three standalone regressions within its own finite
scope. Its [acceptance record](ssa-source-aot-scalar-text.md) does not establish
independent ownership. See the [owner module contract](../core-runtime/ssa-aot-owned-module.md)
and [test guide](../../tests/acceptance/ssa-aot-owned-module.md).

## Actual genuine RED r1

Windows x64 clang-cl 19 Debug assertions/UBSan ran directly against the checkout.
Root driver PID86154 naturally terminated with observed exit 0. Configure PID38688,
build PID6876 and prerequisite PID6504 naturally exited 0; feature PID37064
naturally exited 2. Prerequisites reported 2 tests, 0 failures, 0 ignored. Features
reported 2 tests, 2 failures, 0 ignored, each at test line 415:

```text
Expected 0 Was 14:FEATURE RED: actual source module requires independent Core ownership
```

The prerequisites verify actual Core module validity, callable ABI and both full
pure C/LLVM buffers for real 9/8 sources. Build warning count was zero and no UBSan
diagnostic was observed. Source pins, compilation graph and five explicit Core
header pins were checked. The actual owner TU occurred once in Core, with assertions
and UBSan enabled. This is genuine feature RED, not a build/prerequisite failure
or unsupported-host substitute. No new test run was performed for this document.

Reports below are under `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`.
The final Root receipt has `validated=true` for the RED gate.

| Actual r1 evidence | SHA-256 |
| --- | --- |
| source-aot-owned-module-red-r1-receipt.json | 5AC6E5B9FFA2CC4304428C719F9EAE346429F096A6DF39619A6CB8FB4F969501 |
| source-aot-owned-module-red-r1-configure.log | C6E8057022A5822ACF79A163EFDB7D37B815F331E9D12324CFD02487AEDF6899 |
| source-aot-owned-module-red-r1-build.log | 384B5192AFF0D406482AFCB57804645AD4F003F90BF042C68C6C14A987D5C893 |
| source-aot-owned-module-red-r1-prerequisites.log | C5DDB2A96630D3F76689B90F5DA4F97B511B95A733C35112CCF8D7CC18251432 |
| source-aot-owned-module-red-r1-features.log | 91C364CE3209A1029AEDAFD8852E9085DEBCCD088DBB5DF79562F7E80523F896 |

| Actual source pin | SHA-256 |
| --- | --- |
| tests/parser/test_ssa_source_aot_owned_module.c | 3FB6B64DBA7CA37995132647C08A19C53A41E27B986EF53C04AD0D2947094A32 |
| zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h | 1CE282BD6726FDE921253074AB0EC0D7F6A9130F961A8C43ADB956D130C8904B |
| zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c | DF3E74B094E8AC38AEE37ED5FB03068E2DA15E113B3895243699A199F68411D0 |
| tests/cmake/ssa-source-direct-validation/CMakeLists.txt | C5C64915365A02E3B256A5235DFD321E56AAF791975D06CAFAADC22DD858EE34 |
| tests/cmake/ssa-source-execbc-vm.cmake | 892BCB75D7C4F107BA5F1569B19CB940FC662227A383BF05F7497BD5C152D38C |

The receipt also pins actual source/settings/header TSV metadata,
compile_commands.json and built binaries. Root will create and record the real
finite commit identity afterward; this document does not invent that identity.
The no-argument ten-case inventory is a later functional gate and has no current
GREEN credit. No OOM or overflow test was run or claimed in this phase.

## Actual source and registration route

Sources are compiled directly from `E:/Git/zr_vm`, without snapshots, copies or
worktrees. The existing build location is
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`;
reports belong under `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`.
Root owns execution and exact-path Git work.

The direct target `zr_vm_ssa_source_aot_owned_module_test` compiles the actual
lifetime test and shared literal SCRIPT fixture, uses the existing five-source
scalar support library and common validation settings, and receives the owner TU
from Core's actual source glob. Its normal registration uses the existing
parser/core/library link helper. The owner source is not separately added to the
test executable. Both register `ssa_source_aot_owned_module`; CTest invokes the
binary without arguments. Metadata must establish one actual owner TU in Core
and the genuine checkout paths/settings; the registration text alone does not
establish compilation or runtime results.

## Finite behavior targeted after RED

The actual real SCRIPT chain binds module contracts, compacts while retaining
source coordinates, attaches a primitive frame/layout, lowers with canonical
callable facts and builds the host descriptor. The planned success path clones
while those producers live, checks independent copied spans, then releases the
descriptor, projection, compacted function, fixture/compiler and runtime state
before validating/ABI-checking and emitting from the owner view.

The intended evidence preserves the three CONSTANT/NOP/RETURN instructions,
actual literal/function identity, contracts, target, source maps, frame/value
slots and non-NULL empty logical header with identity/generation. A value-only
witness and Core HashModule support semantic comparisons. Local pointer-bearing
observations measure refusal mutation, not ABI identity. Projection stays
non-runnable; text is parsed as a buffer and never compiled or executed.

Further cases target owner-to-owner cloning followed by first-owner release,
exact self-source replacement with distinct arena addresses, actual 9-to-8
replacement, Core-invalid instruction flags and contract signature perturbations,
NULL diagnostics/required arguments, repeat-safe Free and owned buffer boundaries.
Perturbed producer fields are restored before assertions. These are authored
expectations until a corresponding successful actual run proves them.

## Explicit OPEN scope

Production Clone, checked geometry and appending OOM enum/status naming belong
to the next production implementation phase. Actual-TU allocator/private-helper
tests belong to the following separate finite gate; no OOM proof exists now. An OBJECT
shim must compile the real owner TU once with local allocator redirection; copied
helper formulas or production .c inclusion do not provide that proof. General
multiple-function/GC/CFG/deopt retention has no acceptance from the finite scalar
inventory. Linux/full MSVC, normal returned-program ownership and the full47
matrix remain OPEN. SourceExecute, oracle, generated-native execution, loader,
network, FFI, provider and security probes provide no evidence in this gate.
