---
related_code:
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
implementation_files:
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
plan_sources:
  - docs/plans/ssa/05-data-layout/02-arrays-slices.md
tests:
  - tests/parser/test_buffer_pool_ffi.c
doc_type: acceptance-record
status: partial
---

# SSA 05.02: closed BufferHandle owner with a live pin

## Scope

`BufferHandle.close()` logically closes the owner immediately. A previously
issued pointer retains the native allocation until its last pin is released,
but the closed owner must reject `read`, `write`, and `slice`. This slice adds
those three owner checks and keeps the existing pointer path independent.
It does not add the core storage adapter, array generation checks, resize
exclusion, or native batch legalization required by the full 05.02 plan.

## Test-first evidence

The initial GCC Debug build used the unmodified FFI runtime and added three
separate Unity cases in `tests/parser/test_buffer_pool_ffi.c`. Each case
allocated and filled a buffer, pinned it, closed the owner, checked that the
pin still read a byte, then tried exactly one owner operation. The focused
RED binary reported `3 Tests 3 Failures`: read, write, and slice each reached
the assertion requiring a `[NativeCallError]` diagnostic because the owner
operation succeeded. This was an assertion failure, not a build or parser
failure.

The first full target build completed 854/854 Ninja steps. Its unfiltered
test run stopped before the new cases at an existing pool lease path with
`ZrCore_Closure_ToBeClosedValueClosureNew` assertion failure; that run is not
counted as a focused result. The follow-up focused entry isolated the three
new cases. The runtime guard was then added to `Buffer_Read`, `Buffer_Write`,
and `Buffer_Slice`, after receiver validation and before argument or storage
work.

An initial run after the guard showed all three expected
`[NativeCallError] buffer handle is closed` messages. The original script
`try/catch` did not catch these native errors in this harness, so the tests
were revised to use `ZrTests_Runtime_Function_ExecuteCaptureFailure` and
inspect the current exception object's `message`, matching other repository
tests. The revised source also forces a full GC after owner close, then reads
and writes through the existing pin before triggering each owner error.

The revised focused GCC build completed 4/4 incremental Ninja steps. Direct
execution of the focused binary reported `4 Tests 0 Failures`: the existing
live-view pointer-close compiler rejection and all three new closed-owner
diagnostic cases passed. Each new case ran the full GC probe, wrote byte 1
through the preexisting pin, and read both pinned bytes before the owner call
raised the checked exception. The compiler diagnostic printed by the
live-view case is its expected negative result. The final test source exposes
the same four tests through `--closed-owner`, while its default entry retains
the complete existing list plus the three new cases. Its final GCC target
rebuild completed 31/31 steps after concurrent parser edits; the final binary
repeated `--closed-owner` at `4 Tests 0 Failures`, exit zero. An independent
second invocation of that final binary reported the same 4/4 result.

## Remaining acceptance boundary

The final binary's default entry failed before the three new cases: it printed
the `pool_lease_reuse.zr` compiler diagnostic `If condition lacks a semantic
value`, printed the expected live-view close rejection, then stopped at
`ZrCore_Closure_ToBeClosedValueClosureNew` in `closure.c:551`. It cannot be
reported as a passing full suite. In an earlier isolated run, the existing
pinned Span positive case also returned
`ZR_VALUE_TYPE_NULL` without a current exception where it expected integer
43. GDB confirmed that its post-close path never enters the new owner guard;
the first closed-owner guard hit was a later new read case. The new focused
cases exercise pinned pointer byte access across close and full GC before
checking owner errors, while the older Span-return issue remains separate.

The full 05.02 lifecycle, resize, generation, and native pin gates remain
open. This record accepts only the closed owner operation boundary verified
by the final focused GREEN run, with the full test target's separate failures
still open.

## Tooling

The focused target uses WSL Ubuntu GCC 11.4.0 and the following Debug Ninja
cache. `zr_vm_buffer_pool_ffi_test` is built by CMake but has no CTest
registration, so the executable is run directly.

```bash
cmake -S /mnt/e/Git/zr_vm -B /mnt/d/tmp/zr_vm/ssa-buffer-pin-gcc -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DBUILD_TESTS=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build /mnt/d/tmp/zr_vm/ssa-buffer-pin-gcc \
  --target zr_vm_buffer_pool_ffi_test -j 4
/mnt/d/tmp/zr_vm/ssa-buffer-pin-gcc/bin/zr_vm_buffer_pool_ffi_test --closed-owner
/mnt/d/tmp/zr_vm/ssa-buffer-pin-gcc/bin/zr_vm_buffer_pool_ffi_test
```
