# SSA source to ExecBC Core VM integration

## Coverage

`test_ssa_source_execbc_vm.c` parses source with `ZrParser_Parse`, compiles it
to source-owned SemIR, validates the SemIR CFG, builds and verifies a published
ExecIR module function with `ZrParser_ExecIr_BuildModule`, runs the ExecIR
Oracle, lowers constants into an ExecBC projection, materializes a Core
`SZrFunction`, and invokes it through the Core runtime dispatcher.

The two cases execute both outcomes of a real source conditional:

- `test_true_source_branch_runs_through_core_dispatcher`: returns 9.
- `test_false_source_branch_runs_through_core_dispatcher`: returns 8.

The Oracle fixture resolves Builder-emitted `PLACE_BASE` values by following
their source IDs back to SemIR and returning the SemIR `placeId` as a stable
signed token. It seeds only external ExecIR values that are used as
`PLACE_BASE` provenance operands. Other undefined ExecIR inputs fail fixture
setup instead of being silently modeled.

Each case compares the signed Oracle return with the Core VM return. It
validates every observed VM PC against the materializer PC map and the
corresponding ExecIR instruction source ID. The collapsed VM block path must
start at the ExecIR entry, follow declared CFG successor edges, and end at the
Oracle's reported final block. The Oracle exposes the final block but not a
complete block history, so this does not claim full path-by-path Oracle trace
equality.

These branch bodies return independently and do not merge a value. The test
records `execIr.phiCount` for diagnosis but does not claim source value-phi or
loop-phi coverage. A real-source loop-carried phi remains an open follow-up.

The source fixture covers bool and signed i64 constants, conditional
branches, and signed i64 returns. Other source operations remain outside
this focused materializer slice.

The producer change supports only direct bool/i64 constant initialization of a
root `TEMPORARY` Place with a compiler-authored proof. The Place must be unique
by root identity, disjoint from every other Place, projection/loan/escape/local
free, and have exactly one `PLACE_BASE` plus one `INITIALIZE` with no other
place operation. This does not promote arbitrary temporary Places or relax
the materializer's memory/effect metadata checks. The compiler classifier
checks the live constant pool; Builder can only recheck the SemIR `CONSTANT`,
`hasConstantPoolIndex`, and type/proof consistency because it receives no
constant pool. The separate Core `VerifyModule` owned-pool validation is
documented in `ssa-module-constant-pool-verifier.md`; this source module uses
the compiler pool explicitly when constructing its projection and Oracle
input.

Builder value/instruction types are canonical IDs. The fixture copies the
actual compiler constants into a projection pool using IDs interned in the
same semantic context, then calls
`ZrParser_ExecBcProjection_MaterializeVmFunctionWithCanonicalTypes`. The
adapter resolves those IDs to runtime bool/i64 tokens in copied arrays; the
original graph's types, source metadata, CFG and effect guards remain intact.

## Focused validation

The CMake registration is `ssa_source_execbc_vm`, target
`zr_vm_ssa_source_execbc_vm_test`, included through
`tests/cmake/ssa-source-execbc-vm.cmake`. Root uses the native wrapper with
compiler temporary storage and all build/generated outputs under
`D:/tmp/zr_vm`:

```powershell
python D:/tmp/zr_vm/ssa-control/run_native.py source-build build zr_vm_ssa_source_execbc_vm_test
python D:/tmp/zr_vm/ssa-control/run_native.py source-ctest ctest -R '^ssa_source_execbc_vm$' --no-tests=error
```

## Current validation status

The 2026-09-30 root MSVC build linked the source target and standalone builder
consumers. `current-scalar-shape-fixtures-ctest.log` passed the two source
branches (returns 9 and 8), `ssa_source_execbc_vm` in 0.54 seconds, and
`exec_ir_scalar_scratch_eligibility` in 0.78 seconds. This same selection
passed the 18-case scalar/canonical VM target and 13-case dead-place target.
It was 7/8 overall because an independent conditional-cleanup fixture failed.
Current GCC/Clang validation is pending; no complete semantic matrix is claimed.

The previously recorded source run reached Oracle and projection for both
cases, then failed materialization with diagnostic `UNSUPPORTED` (code 28),
function 20741, block 0, instruction 0, source 0, expected version 0, and
actual version 1. The structured source dump showed three temporary scalar
`PLACE_BASE`/`INITIALIZE` pairs and three memory tokens. The new proof side
table/classifier/eligibility slice makes only the directly
proven bool/i64 literal pairs promotable; it does not relax the materializer's
memory/effect guard or delete arbitrary stores. The old RED is retained as
historical evidence alongside the passing focused run above.

The independent negative target is `exec_ir_scalar_scratch_eligibility`,
executable `zr_vm_exec_ir_scalar_scratch_eligibility_test`, registered by
`tests/cmake/exec-ir-scalar-scratch-eligibility.cmake`. The focused command is:

```powershell
python D:/tmp/zr_vm/ssa-control/run_native.py scratch-build build zr_vm_exec_ir_scalar_scratch_eligibility_test
python D:/tmp/zr_vm/ssa-control/run_native.py scratch-ctest ctest -R '^exec_ir_scalar_scratch_eligibility$' --no-tests=error
```

The source loop-phi claim remains open. This source branch has no value merge;
the acceptance does not count its observed branch as loop-phi coverage.
