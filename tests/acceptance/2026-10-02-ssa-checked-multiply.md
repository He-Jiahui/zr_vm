---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_checked_integer.h
  - zr_vm_core/src/zr_vm_core/execution/execution_signed_multiply.inc
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_checked_integer.h
  - zr_vm_core/src/zr_vm_core/execution/execution_signed_multiply.inc
plan_sources:
  - docs/plans/ssa
  - "user: 2026-10-02 lower checked multiply support before SSA MUL integration"
tests:
  - tests/core/test_execution_checked_multiply.c
  - tests/cmake/checked-multiply-tests.cmake
doc_type: acceptance-record
status: scoped-accepted
---

# SSA checked i64 multiply runtime support — 2026-10-02

## Scope

The lower legacy runtime now checks all seven `MUL_SIGNED` instruction forms
before C signed multiplication. The private helper preserves its output on
failure. The existing instruction macros are extracted into a cohesive include;
dispatch-local error handling preserves faulting PCs, catch/finally behavior,
and ownership cleanup. The SSA materializer and independently owned Oracle and
ExecBC interpreter files are unchanged by this slice.

## Baseline and RED

The pre-change legacy core library was read from
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc/lib/zr_vm_core.lib` and linked
without rebuilding it. Five test/harness sources compiled with exit 0, linking
returned 0, and the test returned **1**:

`test_all_signed_multiply_forms_reject_overflow: FAIL: legacy signed multiply must reject overflow`.

This is a semantic RED from actual legacy VM execution of `INT64_MAX * 2`,
before production was edited. An earlier driver launch could not locate `cl`;
that setup failure is retained but is not used as RED evidence.

The baseline artifacts are under
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/checked-multiply/red-v2/`, including
`compile-*.log`, `link.log`, `run.log`, and `receipt.json`. The receipt captures
SHA-256 of the old runtime/support libraries, test/harness source, and frozen
driver before and after execution.

## Test inventory

`tests/core/test_execution_checked_multiply.c` contains eight independent cases:

- Overflow rejection across seven opcode forms and ordinary/observer execution.
- Pure helper boundaries, null output, output aliases, and failure output preservation.
- A 26-row arithmetic catalog across every form and both execution modes.
- Both input slots as aliased destinations, for successful and failing products.
- Distinct `LOAD_STACK_CONST` source slot 0 to materialized slot 3, throughout
  the existing successful/overflow/alias matrix.
- Direct host `TryRun` checks of saved faulting PC and state reuse after failure.
- Language catch/finally consumption and finally-only rethrow.
- Existing floating fallback across all seven forms.
- Owned destination overwrite and registered ownership cleanup on overflow for
  the five ordinary store forms; plain forms retain their existing plain slot
  precondition.

The boundary catalog includes zero, both signed extrema, ±1, all four sign
combinations, operand permutations, the exact `INT64_MIN` result,
`3037000499²`, and overflowing `3037000500²`. VM cases check normalized runtime
status/exception and the absence of partial integer results. Ownership cases
observe the last strong reference reaching zero and a retained weak control
becoming inactive; overflow also checks that the registered destination is null.
The fixture retains state/function and actual owner/weak values in static
storage so Unity assertion longjmp can still clean them in `tearDown`. Cleanup
clears the observer before calling runtime code, closes registrations, releases
the tracked owned stack destination through a saved offset and the external
owner/weak values, resets the state, frees the function, destroys the state, and
clears tracked pointers. It retains no cleanup pointer to an automatic value.

## Native evidence

The frozen command is:

```text
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/checked-multiply/native.py green-v3
```

`native.py` imports the root's existing MSVC toolchain environment JSON, with all
cwd, TMP/TEMP, objects, compiler/linker PDBs, executables and logs inside the
owned checked-multiply directory. Each subprocess has a 180-second watchdog.
It compiles the changed dispatch independently and links that object ahead of
the read-only baseline static runtime and support libraries.

MSVC 14.44.35207 compiled all six source units with exit **0**. Link exit was
**0**. The executable reported **8 tests, 0 failures, 0 ignored**, exit **0**.
The receipt reports identical before/after hashes. Exact commands and outputs
are under `checked-multiply/green-v3/`.

The first GREEN attempt compiled dispatch without `/utf-8`, which misparsed
existing UTF-8 source comments and failed. Adding the repository's UTF-8
compilation setting resolved that driver issue; `green-v2` then passed the first
six tests, and `green-v3` passed all eight. No implementation check was removed
or relaxed to obtain a pass.

After independent review, the fixture cleanup and distinct stack load coverage
above were added without changing the checked helper, extracted instruction
block, or dispatch. `green-v3` remains the immutable earlier validation history;
the revised test source was validated separately with:

```text
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/checked-multiply/native.py green-v4
```

All six source units compiled with exit **0**, linking returned **0**, and the
revised executable reported **8 tests, 0 failures, 0 ignored**, exit **0**
(42.1 seconds). `green-v4/receipt.json` retains the revised source hashes and
immutable commands; helper, instruction block and dispatch hashes are unchanged.
The native mathematical oracle remains applicable because its helper source did
not change. No fake failure injection or additional worker/thread was added.

An additional independent mathematical gate used native Clang **19.1.5**, target
`x86_64-pc-windows-msvc`:

```text
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/checked-multiply/native_clang_oracle.py
```

The compiler supported `__int128`; build exit was **0**, and execution exit was
**0**, reporting **361 boundary products + 100000 random products match
independent __int128 oracle**. This is a normal native `-O2` mathematical gate,
not a WSL or sanitizer result. Commands, output, versions and stable source
hashes are in `checked-multiply/native-clang-oracle/`. Each subprocess had a
60-second watchdog; the build and run took about 47.9 and 41.2 seconds.

## WSL evidence and remaining gates

The frozen independent helper driver is
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/checked-multiply/wsl_helper.py`, with a
150-second outer watchdog and 60 seconds per compiler/run. The intended GCC and
Clang checks use `-O2 -fsanitize=undefined -fno-sanitize-recover=all`; a separate
`__int128` mathematical oracle validates 361 cross-product boundary cases and
100000 deterministic full-width random products. No wrapping flag or sanitizer
suppression is used.

The WSL launch reached its **150-second watchdog**, producing
`subprocess.TimeoutExpired`; the outer Python exited **1**. It did not create a
GCC/Clang output directory or report a compiler/test result. The exact invocation
and timeout evidence are retained in `checked-multiply/wsl-helper-timeout.log`.
Therefore no WSL helper/compiler/sanitizer pass is claimed. This is an environment
failure, not a multiplication test result.

Full GCC/Clang VM execution, sanitizer validation of VM cleanup,
and upper SSA materializer/source coverage are still parent
integration gates. The native tests exercise ordinary/observer behavior in
MSVC's switch dispatch; they do not establish GCC computed-goto execution.

The final build registration includes `tests/cmake/checked-multiply-tests.cmake`
once after the test/link helper definitions. It creates
`zr_vm_execution_checked_multiply_test` and CTest `execution_checked_multiply`,
with label `ssa` and a 120-second timeout. The parent owns the shared build
directory and ran its formal gate as recorded below; the subagent did not
write that shared directory.

## Independent root verification

Root configured the existing MSVC Debug build with the new test registration
and built `zr_vm_execution_checked_multiply_test` successfully (exit 0).
The repository's registered `execution_checked_multiply` CTest then passed in
42.03 seconds; CTest exited 0 with 1/1 tests passing. Exact commands and outputs
are retained in
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/checked-multiply-formal-build.log`
and `checked-multiply-formal-ctest.log` in the same directory.

Root and an independent reviewer checked the final production helper,
instruction extraction, dispatch-local error path and revised fixture.
The review's cleanup and distinct stack-load coverage findings were corrected
before the final direct and formal runs. No introduced defect remained in the
final eight-file scope. This formal gate validates the current native switch
runtime; it does not turn the unrun Linux, computed-goto or VM sanitizer gates
above into passes.

## Acceptance decision

The native lower runtime slice passes its current direct regression suite,
registered MSVC CTest and independent native Clang mathematical oracle.
Cross-platform and upper SSA integration acceptance remains pending the explicit
gates above. This document does not declare unrelated legacy arithmetic or the
entire repository passing.
