# Oracle checkpoint execution and resume

## Scope and design

Plan 01.04 batch 1 requires the non-optimized logical frame to resume in the
oracle with the same effects and values. A logical-ID-only materializer and an
oracle that always starts at entry do not establish that contract.

This stage connects the existing state-map consumer to the existing oracle
instruction dispatcher. An optional checkpoint selects source ID, resume ID,
and before/after/cleanup phase. A paused result carries pointer-free function
identity and control-flow metadata, including the selected successor ordinal
for parallel edges. The map remains the sole description of required values.

Resume validates identity, the map, and the saved cursor, then prepares a new
value environment containing only mapped live values and a copy of the event
history. Preparation failure preserves the paused state. Successful preparation
replaces that state and consumes its pause token before execution starts. A
later execution failure keeps the partial state and cannot restart from the old
checkpoint. Callbacks and function metadata must remain stable during a call.

The oracle still stores scalar values and opaque caller-owned tokens. It does
not allocate runtime GC objects or switch a native/ExecBC frame. Aggregate,
inline-frame and runtime scheduler reconstruction remain later 01.04 work.

## Reference evidence

- JDK `lua/jdk/src/hotspot/share/runtime/deoptimization.cpp` around
  `should_reexecute`: interpreter reconstruction distinguishes reexecution from
  advancement beyond a committed instruction.
- Lua `lua/src/ldo.c`, `unroll`, and `lua/src/lvm.c`, `luaV_finishOp`: continuation
  resumes at saved execution state and finishes an interrupted operation before
  proceeding. Zr's oracle pauses at explicit whole-instruction phases.
- CPython `lua/cpython/Objects/genobject.c`, `gen_close`: state transitions
  distinguish suspended, executing and finished frames before continuing;
  `lua/cpython/Lib/test/test_generators.py` covers partially constructed frames,
  invalid initial sends and repeated/finished continuation operations.

## Validation

The initial five tests built against the old entry-only execution loop (with
the new selector declared but ignored) failed all five pause assertions. After
connecting the execution loop, seven of nine tests passed. The remaining two
exposed diagnostic contract choices: saved identities now compare exactly
before generic map validation, and a nonexistent next checkpoint preserves the
existing consumer's `RESUME_NOT_FOUND` diagnostic.

Independent specification review also identified that exceptional INVOKE
continuations must exclude normal-only result values from the union live map,
and that zero-valued generation/signature identities must not act as wildcards.
Both receive direct regression cases. The expanded suite produced 17 passes
and one expected exceptional-INVOKE failure before the edge projection fix.
That fix now clears normal-only results on exceptional edges and excludes them
from checkpoint capture/restore. The fixture seeds stale result storage and
poisons it again after capture, checking both clearing and restoration.

Final quality review found one additional state-lifetime issue: consuming a
SUSPEND checkpoint must clear its payload snapshot so a later void return or
payloadless suspension cannot expose that old value. Both dedicated regressions
failed before the fix while the existing 18 cases passed. The fix clears only
the consumed suspension snapshot; completed RETURN checkpoints retain their
result. Independent quality re-review confirmed the fix and reported no
remaining actionable findings in this slice.

The suite contains 20 cases: getter before/after, store/cleanup trace, stale
generation, missing live values, consumed callback failure, parallel edges,
in-block phi, invalid next checkpoint, SUSPEND continuation, 70 repeated loop
resumes, empty live set, malformed successor ordinal, both resume preparation
allocation failures, normal/exceptional INVOKE, and exact zero generation and
signature, void return after SUSPEND, and payloadless SUSPEND after a prior
payload-bearing SUSPEND. Failure injection compiles the exact resume
implementation in a test translation unit with malloc/calloc interception;
ordinary oracle suites link the production translation unit.

## Tooling evidence

2026-09-24 final implementation, including stale-result and SUSPEND payload
regressions:

| Environment | Build directory | Result |
| --- | --- | --- |
| WSL GCC 11.4.0 Debug | `build/ssa-gcc-debug` | 7/7 suites |
| WSL Clang 14.0.0 Debug | `build/ssa-clang-debug` | 7/7 suites |
| Windows MSVC 19.44.35228.0 Debug | `build/ssa-msvc-debug` | 7/7 suites |
| WSL GCC ASan + UBSan | `build/ssa-gcc-asan-phase80` | 7/7 suites, no findings |

Each directory was rebuilt with these targets:

```text
cmake --build <build-directory> --target
  zr_vm_ssa_oracle_resume_test zr_vm_ssa_oracle_projections_test
  zr_vm_ssa_oracle_parallel_edges_test zr_vm_ssa_state_map_ownership_test
  zr_vm_ssa_state_map_liveness_test zr_vm_ssa_state_maps_test
  zr_vm_ssa_deopt_validation_test -j 4
```

After the final suspension-snapshot fix, the three affected oracle targets
were rebuilt and the complete seven-suite filter below was rerun on all four
environments.

CTest used the following exact filter, `--output-on-failure`, and
`--no-tests=error`:

```text
^ssa_(oracle_resume|oracle_projections|oracle_parallel_edges|state_map_ownership|state_map_liveness|state_maps|deopt_validation)$
```

Windows imported `Import-VsDevCmdEnvironment.ps1` from the `using-vsdevcmd`
skill, built with `--config Debug`, and ran CTest with `-C Debug`. Existing
`/W3` to `/W4` overrides and unrelated numeric-loop object-path CMake warnings
remain. The changed source produced no new compiler warnings.

Sanitizer CTest used `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. The 20 new Unity cases and 66 existing state-map
cases passed, alongside both assertion-based oracle projection/parallel-edge
suites. This includes failed preparation cleanup and 70 successive checkpoint
transactions; no leak, invalid access or undefined behavior was reported.

## Acceptance decision

Accepted for oracle logical-frame checkpoint execution and resume. The original
instruction dispatcher remains shared, while validation, execution control and
resume preparation now occupy separate modules below the repository's large-file
threshold. Specification review identified and verified the fixes above.

This does not complete 01.04. Runtime aggregate identity reconstruction, nested
inline frames, conditional cleanup flags, physical frame switching and actual
GC/scheduler integration remain. No source syntax, CLI or artifact format was
changed; the API uses existing verified ExecIR and state maps. Cancellation is
not a new API here: callers retain their existing provider/step-limit controls.
API-owned result records cannot be independently resumed through shallow copies.
After execution begins, a provider failure consumes the prior checkpoint; only
preparation failure permits retry of that checkpoint.
