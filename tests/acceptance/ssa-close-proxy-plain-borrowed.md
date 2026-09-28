# Close proxy plain-value and borrowed-view acceptance

## Scope

Plan source: `docs/plans/ssa`. Implementation: `zr_vm_core/src/zr_vm_core/closure.c`.
Focused test: `tests/core/test_close_proxy.c`. The proxy registration API and its
marker ordering were established in commit `3cf90695`; this follow-up changes
only the behavior when that marker is closed.

## RED evidence

Before the runtime fix, the GCC Debug target in
`D:/tmp/zr_vm/close-proxy-core-red` compiled the three new cases and ran
**15 tests, 3 failures**. The plain dense value failed at line 236 because it
became null; the distinct dense/physical plain value failed at line 268 for the
same reason; the borrowed view failed at line 313 because its object's `@close`
ran once instead of zero times. The previous twelve cases passed.

The existing source-level behavior was independently checked with the older
interpreter binary: `using(resource) { ... }` and nested bare
`using resource;` both returned the original integer value `7` afterward.
The parser's `UsingCleanupBuiltin(BORROWED)=DROP` and core
`Ownership_ReleaseValue(BORROWED)` establish the borrowed-view contract: clear
the view without closing or releasing its owner. A borrowed object source
fixture compiled and emitted `OWN_DROP`, but execution encountered the separate
`object meta method is not implemented` error before post-using observation.
It is therefore **not** counted as a source-level GREEN result here.

## GREEN evidence

In the same D drive GCC Debug build, the direct Unity binaries passed:

| Binary | Result | Protected behavior |
| --- | ---: | --- |
| `zr_vm_close_proxy_core_test` | 15/15 | Plain dense and physical values remain readable; borrowed source and mirror clear with zero `@close`; prior proxy cleanup cases remain green. |
| `zr_vm_type_layout_inline_copy_test` | 40/40 | Legacy physical frame mirror and ownership cleanup. |
| `zr_vm_native_closure_value_test` | 3/3 | Native closure metadata handling. |
| `zr_vm_object_call_known_native_fast_path_test` | 61/61 | Reentrant/native stack and value handling. |

All four binaries exited with code 0. The matching `close_proxy_core` CTest
registration also passed 1/1. Builds and test working directories stayed under
`D:/tmp/zr_vm/close-proxy-core-red`; that reusable cache was retained.
