---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_tail_call.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dynamic_call_guard.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/exception.c
tests:
  - tests/parser/test_tail_dispatch_runtime.c
  - tests/parser/test_call_binding_pipeline.c
plan_sources:
  - docs/plans/ssa/04-frame-native/02-call-return-tail.md
doc_type: acceptance-record
status: complete
---

# SSA 04.02 dynamic tail dispatch runtime gate

## Scope

This gate compiles source into legacy VM bytecode and executes it through the
core interpreter. It covers the eligible `DYN_TAIL_CALL` reuse branch, ordinary
and tail runtime failure for a non-null scalar target, and language-level catch. The
existing `test_dynamic_tail_meta_call_preserves_last_argument_after_reuse_declines`
in `test_call_binding_pipeline.c` remains the boundary gate for an inline
`@call` parameter that declines reuse and must preserve the full fallback
window; it returns `347`.

This slice does not wire the ExecIR transfer plan into an executor, implement
return-buffer forwarding, or complete 04.02.

## Runtime evidence required

`test_eligible_dynamic_tail_call_reuses_active_runtime_frame` compiles a
scalar-only callable and performs 8,192 dynamic tail calls. Its read-only trace
observer counts the actual tail instruction, samples the active call-info and
stack offsets, and issues no VM calls. It asserts the exact result, stable
call-info/frame offset, call-info depth at most four, and stack high-water at
most 64 slots. The observer selects the interpreter's traced dispatch path but
does not participate in tail eligibility; stable frame identity across the
tail-site observations is the executable check that reuse still occurs.

`test_noncallable_dynamic_tail_failure_restores_error_and_owned_frame` keeps a
`Unique<Tracker>` alive in the source caller while `relay` performs a zero-arg
dynamic tail call to integer target `1`. The caller's cleanup scope prevents
its call to `relay` from being tail-lowered; `relay` has no owned locals, so its
inner call is eligible for `DYN_TAIL_CALL`. The protected runtime failure must
retain an Error object with the non-callable message and a traceback naming
`relay` and the test source. A read-only instruction observer must see the
compiled resource destructor exactly once during caller unwind. After capture,
the ownership-root count returns to baseline, `callInfoList` is the base native
frame, and the stack returns to the one-slot error boundary.

`test_noncallable_dynamic_call_failure_restores_error_and_owned_frame` repeats
the unhandled error through an ordinary dynamic call. Its source prevents tail
lowering and the trace must see one ordinary call and no tail call. Both error
cases retain the exact runtime status and non-callable message, execute the
compiled destructor once, and restore the ownership-root baseline.

`test_noncallable_dynamic_call_failure_is_caught_and_owner_closes` catches the
same error inside the source-language relay. It checks Error fields, a live
outer owner at catch entry, a result of one, one destructor, no final exception
and the original ownership-root count. Successful execution retains its result
slot in the harness; top is `stackBase + 2`, unlike the reset boundary used by
the two captured failures. The normal 8,192-call case checks this successful
result-slot boundary as well.

## Integration

The new target is `zr_vm_ssa_tail_dispatch_runtime_test`; its CTest name is
`ssa_tail_dispatch_runtime`. The fragment
`tests/cmake/tail-dispatch-runtime-tests.cmake` is intended to be included from
`tests/CMakeLists.txt` with:

```cmake
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/tail-dispatch-runtime-tests.cmake")
```

The fragment links the existing parser/core/library runtime test stack. The
registered run should include both the new target and the existing fallback
boundary:

```text
cmake --build D:/tmp/zr_vm/ssa-artifact-v6-msvc --target zr_vm_ssa_tail_dispatch_runtime_test zr_vm_call_binding_pipeline_test -j 2
ctest --test-dir D:/tmp/zr_vm/ssa-artifact-v6-msvc -R "^(ssa_tail_dispatch_runtime|call_binding_pipeline)$" --output-on-failure --no-tests=error
```

Keep MSVC `TEMP` and `TMP` under
`D:/tmp/zr_vm/ssa-control/compiler-tmp`. Build outputs, generated
files, and compiler temporaries stay on `D:`.

## Results and acceptance

The current-source MSVC RED build completed 571/571 steps. Its runtime test
passed the 8,192-call case and failed the other three: ordinary and tail errors
left ownership roots `0 -> 1`, the tail path executed no destructor, and the
source catch was skipped. Evidence: `tail-runtime-current-red-build.log`,
`tail-runtime-current-red.log` and `tail-runtime-current-red-last-test.log` in
`D:/tmp/zr_vm/ssa-control`.

After routing non-callable targets through the existing VM runtime-error path,
both unhandled cases passed and the catch executed, released its owner and
returned one. The first catch fixture incorrectly expected the failed-execution
stack boundary after successful execution; the harness retains its result
instead. That fixture assertion was corrected after reading the normal call
and capture/reset implementations. Evidence for this intermediate result is
`tail-runtime-guard-fixed-ctest.log` and
`tail-runtime-guard-first-last-test.log`.

Final MSVC 19.44 builds completed 11/11 production/regression steps and 6/6
fixture/tail-reset steps. Three CTests passed in 6.68s: `ssa_tail_dispatch_runtime`
(4/4 Unity), `call_binding_pipeline` (18/18) and `close_meta_exception` (4/4).
The direct `zr_vm_tail_reuse_callinfo_reset_test` passed 4/4. These 30 cases also
retain the actual meta-call fallback result `347`, required frame reuse and
exactly-once cleanup. Evidence: `tail-runtime-guard-fixed-build.log`,
`tail-runtime-final-build.log`, `tail-runtime-msvc-final-ctest.log`,
`tail-runtime-msvc-final-last-test.log` and
`tail-runtime-msvc-reset-final-direct.log`.

Clang 14 completed its initial 583/583 build, then 17/17 final fixture/support
steps after explicitly retiring the old test object. The same three CTests
passed in 13.08s, with 4/4, 18/18 and 4/4 Unity cases; the direct call-info reset
test passed 4/4. Evidence: `clang-tail-guard-first-build.log`,
`clang-tail-final-build.log`, `clang-tail-final-ctest.log`,
`clang-tail-final-last-test.log` and `clang-tail-reset-final-direct.log`.

GCC 11.4 completed its initial 583/583 build and 17/17 final fixture/support
steps after retiring the old test object. The same three CTests passed in
10.65s, again with 4/4, 18/18 and 4/4 Unity cases; the direct call-info reset
test passed 4/4. Evidence: `gcc-tail-guard-first-build.log`,
`gcc-tail-final-build.log`, `gcc-tail-final-ctest.log`,
`gcc-tail-final-last-test.log` and `gcc-tail-reset-final-direct.log`.

Each platform therefore passes 30 focused cases. Both WSL builds reuse their
existing D: caches; the final fixture correction was recompiled and the
production guard remained frozen. No performance, sanitizer or complete 04.02
acceptance is claimed.
