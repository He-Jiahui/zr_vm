---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/include/zr_vm_core/exec_ir_interpreter.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
implementation_files:
  - tests/parser/test_ssa_conditional_cleanup.c
  - tests/parser/test_ssa_oracle_resume.c
  - tests/core/test_ssa_runtime_objects.c
  - tests/cmake/ssa-cleanup-tests.cmake
plan_sources:
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
tests:
  - ssa_conditional_cleanup
  - ssa_state_map_ownership
  - ssa_state_map_liveness
  - ssa_state_maps
  - ssa_oracle_resume
  - ssa_runtime_objects
doc_type: acceptance-evidence
status: accepted-stage
---

# Conditional cleanup execution and recovery

This stage implements the initialization-state part of 01.04 batch 2. The
explicit parser lowering API produces guarded cleanup; the reference oracle
executes it, pauses and resumes the existing dispatcher. Real object graph
preparation consumes the resolved state without reading inactive owner slots.
The full 01.04 milestone and the overall SSA plan remain open.

## Behavioral inventory

| Trigger | Required evidence |
| --- | --- |
| Owner defined on one branch only | Both arms resume before/after a poll and in all three cleanup phases; defined arm drops once, other arm drops zero times |
| MOVE on one branch | Moved destination drops once; original cleanup skips only the moved arm |
| UNIQUE and SHARED | Same guarded-cleanup contract; GC/plain/borrowed inputs cannot masquerade as an owning cleanup operand |
| INVOKE success/exception | Actual callback runs once; successful result cleans up once, missing exceptional result never drops |
| Loop definition | 257 cleanup checkpoints resume to completion; each fresh iteration drops once and full event traces match |
| Runtime owner witness | Active roots include the owner; inactive obligations remain represented without roots; impossible or missing witnesses do not replace targets |
| Invalid paused state | Impossible owner or initialized state with absent payload preserves the old cursor and effect history |
| Ordinary invalid use | Double DROP and ordinary use of unavailable owner remain errors |
| Invalid guarded instruction | Wrong block/ownership, invalid value ID and declared-but-never-defined owner are rejected |
| Clone lifecycle | Clone retains independent map pools; freeing original still permits clone execution and recovery |
| Preparation failures | Each intercepted resume allocation and each ownership-analysis allocation in cleanup lowering fails atomically, followed by a successful retry |
| Runtime GC integration | Consumed owner slot contains invalid bytes; each shell allocation forces GC while valid cyclic graph identity survives |
| Buffer overlap | Base/interior owner-state aliases, including function-owned map pools at an empty-live checkpoint, cannot reach resume commit; object output cannot overwrite the concrete owner witness |

The OOM loops enumerate actual intercepted allocations until the first success
without injection. They do not hard-code a count of allocations. Resume tests
intercept allocations in the resume preparation translation unit; lowering tests
intercept the shared owner analysis. These do not claim exhaustive fault
injection for every allocator used by function cloning or liveness.

## Initial failures and corrections

- Before implementation, GCC compilation rejected the missing guarded opcode,
  parser API, runtime owner arrays and witness fields. The normal CMake target
  also failed for those missing contracts.
- The first integrated run passed branch, MOVE, INVOKE, loop and runtime GC
  cases. The existing resume OOM test's fixed allocation count was obsolete
  after owner-state storage was added; it now verifies each injected failure
  without imposing an implementation-specific count.
- A direct oracle fixture with a declared UNIQUE ID but no definition was
  initially accepted. Shared guarded-instruction validation must establish a
  real definition or external-entry identity before conditional execution.
- A guarded-instruction shape error initially omitted its source location.
  Instruction-level diagnostics must retain the original source ID. Invalid
  global operand-pool IDs still use the existing pool-level diagnostic contract.
- Expanded regression exposed rejection of conservative, initialized extra
  live values in existing maps. These remain supported; missing required
  values and unrelated conditional/inactive extras remain invalid.
- Independent specification review found two alias defects. The paused owner
  buffer alias reproduced as a `free(): double free detected` process abort;
  object output aliasing the owner witness incorrectly succeeded and overwrote
  that input. Both received regression coverage before the fixes.
- Independent quality review found that the first resume alias fix did not
  exclude function-owned pools. An empty-live checkpoint accepted the map's
  owner-state pool and freed it during commit. The regression first failed
  with `resume accepted function-owned owner-state storage`; it now rejects
  both the base and an interior pointer without consuming the checkpoint,
  then retries successfully. Resume and logical materialization share the
  function/map input-span inventory rather than maintaining separate lists.

## Platform verification

Executed on 2026-09-24 against the final production and test changes:

| Environment | Build and execution result |
| --- | --- |
| WSL x86-64, GCC 11.4.0, Debug | All 13 targets built; 13/13 CTest suites passed |
| WSL x86-64, Clang 14.0.0, Debug | All 13 targets built; 13/13 CTest suites passed |
| Windows x64, MSVC 19.44.35228, static Debug | All 13 targets built; 13/13 CTest suites passed |
| WSL GCC 11.4.0, ASan/UBSan Debug | All 13 targets built; 13/13 CTest suites passed with leak detection and halt-on-error |
| Windows x64, MSVC 19.44.35228, shared Debug | Runtime object target built and all 34 Unity cases passed through the DLL boundary |

The 13-suite selection contains 172 Unity cases in eight suites (24 new
conditional-cleanup, 15 ownership, 9 liveness, 34 runtime-object, 28 aggregate,
7 deopt-validation, 35 state-map and 20 oracle-resume cases), plus five standalone
contract executables: core model, effects verifier, oracle projections,
escape/ownership and interprocedural inlining. This is the actual selected
denominator, not a claim that the entire repository test suite ran. Runtime
object coverage adds two cases to its previous 32-case suite.

Reproduction commands from the WSL repository root (CTest 3.22.1):

```bash
suites=(ssa_conditional_cleanup ssa_state_map_ownership ssa_state_map_liveness
        ssa_oracle_resume ssa_runtime_objects ssa_core_model ssa_effects_verifier
        ssa_state_maps ssa_deopt_aggregates ssa_deopt_validation
        ssa_oracle_projections ssa_escape_ownership ssa_interprocedural_inlining)
targets=()
for suite in "${suites[@]}"; do targets+=("zr_vm_${suite}_test"); done
pattern="^($(IFS='|'; echo "${suites[*]}"))$"
for build in build/ssa-gcc-debug build/ssa-clang-debug; do
  cmake --build "$build" --target "${targets[@]}" -j 4
  ctest --test-dir "$build" -R "$pattern" --output-on-failure --no-tests=error
done
cmake --build build/ssa-gcc-asan-phase80 --target "${targets[@]}" -j 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/ssa-gcc-asan-phase80 -R "$pattern" \
  --output-on-failure --no-tests=error
```

The sanitizer directory uses `-fsanitize=address,undefined
-fno-omit-frame-pointer`. Windows used the same target list and regex in an
initialized Visual Studio developer environment, with
`build/ssa-msvc-debug`. The shared check used:

```text
cmake --build build/ssa-msvc-shared-debug --target zr_vm_ssa_runtime_objects_test -j 4
ctest --test-dir build/ssa-msvc-shared-debug -R "^ssa_runtime_objects$" --output-on-failure --no-tests=error
```

Independent specification review passed after its two alias findings were
fixed. Independent code-quality re-review passed after the function-pool alias
finding was fixed. Every final selected test passed; no test was disabled to
reach this result. Existing MSVC `/W3` to `/W4` override, unrelated object-code
unreachable-code warnings and long-path CMake warnings remain outside this
stage; none prevented the selected builds or tests.

Cleanup lowering and materialization are synchronous and do not expose a
cancellation handle. Resume consumes a successful checkpoint once; an invalid
or OOM preparation leaves it retryable. Repeated loop checkpoints are distinct
dynamic executions, with 257 iterations covered. Partially prepared ownership
analysis and resume storage are released on failure; real runtime root and
graph lifetime checks run in the object suite.

## Reference evidence and scope

Rust's `elaborate_drop.rs:1407` emits a branch on an explicit drop flag;
`issue-27401-dropflag-reinit.rs` requires a fresh flag after a loop definition.
Lua's `lfunc.c:227` pops an obligation before closing it, with exception and
exactly-once regressions in `testes/locals.lua`. QuickJS's
`quickjs.c:17825` explicitly resets locals to `JS_UNINITIALIZED`, and checked
loads reject unavailable payloads. ZR retains separate runtime owner state so
an undefined scalar payload alone cannot authorize conditional cleanup.

No new surface syntax is introduced. Source parsing, project/CLI invocation,
physical VM initialization bitmaps, destructor callbacks, ownership transfer,
nested inline-frame recovery and automatic SROA recipe production are outside
this stage. Guarded cleanup is rejected by unsupported native/AOT projections;
it is not silently lowered as unconditional DROP. The runtime GC test uses the
actual current collector, which retains ordinary object addresses; it does not
claim evidence of physical pointer relocation.
