# SSA 01.05: parallel-edge oracle/ExecBC differential

## Scope

The parser's projected ExecBC runner now returns its actual block,
instruction ID and source ID on `RETURN`. The `ssa_oracle_parallel_edges`
fixture uses the existing differential harness to compare those observations
against the direct ExecIR oracle on the same verified CFG. The test executes
the projection, not the VM's default interpreter. AOT/native execution is not
part of this slice.

## Baseline

The scalar/control projection runner and separate oracle assertions existed,
but no fixture fed their actual observations through the shared comparison
and backend-coverage API. The new test failed to compile until the projection
result exposed the executed return location (`returnInstructionId`,
`returnSourceId`, `currentBlock`).

## Test Inventory

- One `STRUCTURE | SSA` verified function with two parallel edges, different
  phi incoming values, and a terminator successor range separate from the
  block's range.
- Both conditional-branch choices and both switch choices independently
  execute the direct oracle and projected runner; the returned integer and
  return event source/bit pattern must match. The four selected incoming
  values are also asserted independently of the comparison.
- Coverage requires both actual backend identities; the fixture does not
  claim production/default ExecBC or native AOT coverage.
- A test-only corruption of the projected return source must produce
  `EVENT_MISMATCH` at index zero, with source IDs 401 and 402. No runtime
  effects, ownership leases, allocation or callbacks occur in this fixture.

## Tooling Evidence

All four builds recompiled `zr_vm_ssa_oracle_parallel_edges_test`. GCC also
rebuilt the adjacent projection and differential-harness targets; the other
builds reused those targets after their earlier build in this stage. CTest
ran the exact three-name selection (no zero-test match):

```text
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_oracle_parallel_edges_test zr_vm_ssa_oracle_projections_test zr_vm_ssa_differential_harness_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^(ssa_oracle_parallel_edges|ssa_oracle_projections|ssa_differential_harness)$' --output-on-failure --no-tests=error
cmake --build build/ssa-clang-debug --target zr_vm_ssa_oracle_parallel_edges_test zr_vm_ssa_oracle_projections_test zr_vm_ssa_differential_harness_test -j 4
ctest --test-dir build/ssa-clang-debug -R '^(ssa_oracle_parallel_edges|ssa_oracle_projections|ssa_differential_harness)$' --output-on-failure --no-tests=error
cmake --build build/ssa-gcc-asan-phase80 --target zr_vm_ssa_oracle_parallel_edges_test zr_vm_ssa_oracle_projections_test zr_vm_ssa_differential_harness_test -j 4
ctest --test-dir build/ssa-gcc-asan-phase80 -R '^(ssa_oracle_parallel_edges|ssa_oracle_projections|ssa_differential_harness)$' --output-on-failure --no-tests=error
cmake --build build/ssa-msvc-debug --target zr_vm_ssa_oracle_parallel_edges_test zr_vm_ssa_oracle_projections_test zr_vm_ssa_differential_harness_test zr_vm_parser_static --config Debug -j 4
ctest --test-dir build/ssa-msvc-debug -R '^(ssa_oracle_parallel_edges|ssa_oracle_projections|ssa_differential_harness)$' --output-on-failure --no-tests=error
ctest --test-dir build/ssa-gcc-debug -L ssa --output-on-failure --no-tests=error -j 4
```

The MSVC build uses `Invoke-VsDevCommand.ps1` to load VS 19.44.35228.0.
The GCC and Clang CMake configurations report GNU 11.4.0 and Clang 14.0.0.
The GCC ASan/UBSan build checks the same three focused executables for
memory and undefined-behavior diagnostics.

## Results

GCC Debug first rejected the test at the three missing projection result
fields. After recording the executed `RETURN` location, its focused test
passed 1/1. All four toolchains passed the focused matrix 3/3; the sanitizer
run produced no findings. The MSVC build initially reported C4701 for a
short-circuit test assertion; initializing both observation records removed
that new warning on rebuild. The existing C4389 signed/unsigned comparison
warning in `exec_ir_interpreter_validate.c` was not introduced or changed
here. GCC's registered SSA-label sweep passed 80/80; only the three direct
targets were freshly rebuilt for that wider run.

## Acceptance Decision

Accepted for the CFG/phi scalar-return slice: both backends actually execute
the same verifier-valid input, selected-edge return values match, and a
negative event-source injection is detected. This is not the full
semantic-event matrix. The 01.05 effect, exception, ownership, suspend,
production cutover and AOT parity gates remain open. This fixture does not
exercise callbacks, leases, cancellation, or allocation fault injection;
allocation failure remains a later failure-path gate. The projection runner's
existing owned-slot failure tests remain in `ssa_oracle_projections`.
