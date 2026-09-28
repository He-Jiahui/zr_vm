# SSA close proxy pending-error budget cancellation

## Scope

This slice checks that the pending Error guard for an exceptional `@close`
callback preserves an execution-budget cancellation reported by the callback
call boundary. The regression is in `tests/core/test_close_proxy.c`; the runtime
path is `zr_vm_core/src/zr_vm_core/closure_close_meta_guard.c`.

It also protects the existing behavior where a normal callback restores the
pending Error and a callback-thrown replacement Error takes precedence.

## Baseline

On current source `379c7700`, the focused GCC Debug target built successfully.
The first `close_proxy_core` run reported 19 Unity cases with 1 failure: the
new regression observed the budget termination as `CANCELLED` and confirmed
that the callback body did not run, but `threadStatus` was `FINE` (0) instead
of `EXECUTION_TERMINATED` (5). The prior 18 cases passed. This reproduces the
guard restoring the saved pending Error after the budget poll cleared the
active exception state.

## Test Inventory

- `test_pending_error_close_preserves_budget_termination` asserts that a
  pre-cancelled execution budget remains `CANCELLED`, the thread remains
  `EXECUTION_TERMINATED`, no current Error is restored, the close registration
  is consumed, and the callback body is not invoked.
- Existing `test_original_error_is_rooted_across_full_gc_in_close_callback`
  checks restoration of the original pending Error after normal callback return.
- Existing `test_native_close_error_replaces_original_without_leaking_frame`
  checks that a callback-thrown Error replaces the original Error.
- Adjacent suites: `close_meta_exception`, `native_closure_value`, and
  `type_layout_inline_copy`.

## Tooling Evidence

The build used WSL Ubuntu GCC 11.4.0, CMake 3.22.1, Ninja 1.10.1, and the
`Debug` configuration in `D:/tmp/zr_vm/close-proxy-core-red`. CMake's source
directory was `/mnt/e/Git/zr_vm`; the cache stayed on D and no artifacts were
moved across drives.

RED commands:

```text
wsl.exe -e cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_close_proxy_core_test -j 8
wsl.exe -e ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -R close_proxy_core --output-on-failure --no-tests=error
```

After the guard fix, the focused and adjacent regression commands were:

```text
wsl.exe -e cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_close_proxy_core_test -j 8
wsl.exe -e ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -R close_proxy_core --output-on-failure --no-tests=error
wsl.exe -e cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_close_meta_exception_test zr_vm_native_closure_value_test zr_vm_type_layout_inline_copy_test -j 8
wsl.exe -e ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -R close_meta_exception --output-on-failure --no-tests=error
wsl.exe -e /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_native_closure_value_test
wsl.exe -e /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_type_layout_inline_copy_test
```

## Results

- RED: `close_proxy_core` failed only at the new cancellation assertion:
  expected `EXECUTION_TERMINATED` (5), observed `FINE` (0). Its preceding
  assertions proved the termination reason was `CANCELLED`, the close
  registration was consumed, and the callback body was not invoked.
- GREEN: `close_proxy_core` passed 1/1 CTest with all 19 Unity cases passing.
- GREEN: `close_meta_exception` passed 1/1 CTest.
- GREEN: `zr_vm_native_closure_value_test` passed 3/3.
- GREEN: `zr_vm_type_layout_inline_copy_test` passed 40/40.
- The focused suite also passed its existing original-Error restoration and
  replacement-Error cases after the fix.

## Acceptance Decision

Accepted for this scoped cancellation-state fix. The current-source RED exposed
the lost terminal status; the minimal guard change passes the focused test and
adjacent exception, native closure, and frame-layout suites. Broader SSA and
resource-cleanup gates remain open.
