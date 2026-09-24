# CFG ownership for logical state maps

## Scope

2026-09-24, plan 01.04 logical checkpoint state and 01.03 validation ordering.
The parser producer and core materializer now share `exec_ir_owner_state`.
Both previous scans of MOVE/DROP instructions in storage order were removed.

The analysis starts with explicit external entries, propagates possible states
over reachable CFG edges, initializes ordinary results from available operands,
and projects simultaneous phi inputs on each incoming edge. Throwing results
are available only on the normal edge. Checkpoint live values must have one
available state; mixed or consumed states reject publication/materialization.
Unreachable instructions do not acquire resumable maps. The analysis is
transient and adds no serialized pointer or runtime frame representation.

## Design evidence

- `lua/rust/compiler/rustc_mir_dataflow/src/impls/initialized.rs` describes
  maybe-initialized/uninitialized sets, branch joins and the distinction between
  definite initialization and a conditional drop flag.
- `lua/rust/tests/ui/liveness/liveness-move-in-loop.rs` rejects loop-carried
  reuse of a moved value; `lua/rust/tests/ui/borrowck/drop-in-loop.rs` shows why
  repeated assignment and destruction need ownership reasoning beyond liveness.
- `lua/cpython/Python/flowgraph.c`, `scan_block_for_locals`/`maybe_push`, propagates
  possible uninitialized states, clears them at definitions and joins them at
  successors, including exception paths.
- `lua/jdk/src/jdk.compiler/share/classes/com/sun/tools/javac/comp/Flow.java`,
  `AssignAnalyzer`, distinguishes definite assignment from conditional paths.

ExecIR's existing CFG and definition pools carry this contract. Dynamic cleanup
flags are not added here: conflicting states for a live value are rejected
instead of inventing a single state. The shared analysis is also used by the
consumer, so serialized owner state is checked against the same rules.

## Baseline and tests

The initial seven ownership tests produced **7 tests, 5 failures** on the
unchanged code. Sibling DROP/MOVE and an unreachable MOVE poisoned another
checkpoint; a forged join state and a repeated loop MOVE were accepted.

Review then added two independent failing cases: phi assignment and COPY could
incorrectly restore availability from an already moved input. Both were fixed
by propagating operand availability through definitions and edge phi inputs.

The ownership suite now has **15 cases**:

- Sibling DROP/MOVE isolation, including materialization of retained roots.
- Rejection of a forged ambiguous join with the prior target untouched.
- Rejection of a loop-carried consumed owner with the prior map untouched.
- Reinitialization at a loop definition and at an edge-defined phi result.
- MOVE before/after phases switching from source to result.
- Rejection of COPY and phi results derived from moved inputs.
- Unreachable predecessor isolation.
- Consumer rejection of malformed successor and ordinary-result IDs before
  dereferencing the corresponding pools.
- Rejection of an uninitialized deopt reconstruction value.
- Every owner-analysis allocation failing in turn during both rebuild and
  materialization, with no leaked analysis allocations and no prior-state loss.

`ssa_state_map_fixture.h` shares graph construction and consumer assertions with
the existing nine liveness cases. `ssa_owner_fault_allocator.c` compiles the
production analysis with allocation interception only in the ownership test
executable. Other suites link the normal production translation unit. Failure
ordinals advance until the first successful run; the test does not hard-code the
number of allocations. Interception is disarmed before assertions and cleanup.

The older handcrafted state-map fixture now marks its parameter as an explicit
external entry, following the existing SSA entry-value contract.

## Tooling evidence

Final source was rebuilt and checked on GCC 11.4.0 and Clang 14.0.0 under WSL,
and MSVC 19.44.35228.0 after importing the Visual Studio x64 environment.
Each compiler passed **8/8 suites**: core model, effects verifier, builder CFG,
value validation, state maps, deopt validation, liveness and ownership.

For each of `build/ssa-gcc-debug`, `build/ssa-clang-debug`, and
`build/ssa-msvc-debug`, the build selected these targets:

```text
zr_vm_ssa_core_model_test zr_vm_ssa_effects_verifier_test
zr_vm_ssa_builder_cfg_test zr_vm_ssa_value_validation_test
zr_vm_ssa_state_maps_test zr_vm_ssa_deopt_validation_test
zr_vm_ssa_state_map_liveness_test zr_vm_ssa_state_map_ownership_test
```

CTest used `--output-on-failure --no-tests=error` and the exact filter:

```text
^ssa_(core_model|effects_verifier|value_validation|builder_cfg|state_map_ownership|state_map_liveness|state_maps|deopt_validation)$
```

MSVC build and CTest additionally used `--config Debug` and `-C Debug`,
respectively. The existing `/W3` to `/W4` override warnings and unrelated
numeric-loop object-path warnings remain; the changed source emitted no new
compiler diagnostics.

GCC ASan/UBSan in `build/ssa-gcc-asan-phase80` rebuilt and passed all **4/4
state-map suites**, covering 66 Unity cases (35 existing state-map, 7 deopt,
9 liveness and 15 ownership). It used the following environment and CTest
filter, with no sanitizer findings:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
UBSAN_OPTIONS=halt_on_error=1
^ssa_(state_map_ownership|state_map_liveness|state_maps|deopt_validation)$
```

## Acceptance decision

Accepted for shared CFG checkpoint ownership. Full 01.04 remains incomplete: aggregate/inline frame
maps, conditional cleanup, physical frame preparation/commit and actual resumed
execution still need implementation. This stage does not claim global resource
balance or full ownership validation at every non-checkpoint use.
