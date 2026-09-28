# Existing-local `using` close proxy: focused acceptance

## Scope

`using (local) { ... }` and body-free `using local;` in a nested scope need a fresh cleanup
registration without marking the original local's stack slot twice. The parser allocates a high,
empty proxy local, emits `MARK_CLOSE_PROXY(proxy, source)`, and records one scope cleanup. A
body-free `using local;` in a scope that already owns the source cleanup keeps the existing
registration. The resource value is never copied into the proxy.

The focused fixtures cover normal close once, older outer markers, nested `using`, body-free
using, and a plain local that remains readable afterward. Throw cleanup is also covered through
an original exception and an `@close` replacement exception.

## RED and GCC evidence

- Before the parser hookup, the existing PoolLease `using(lease)` throw case and new
  `using(existingLocal)` fixtures reached `ZrCore_Closure_ToBeClosedValueClosureNew` at
  `zr_vm_core/src/zr_vm_core/closure.c:551` with the new and top marker on the same slot.
  The non-idempotent no-using control passed.
- Core close proxy API commits `3cf90695` and `a12f3259` and opcode/AOT commit `164d868b`
  precede the parser hookup. The latter appends opcode 245 with `E=proxy`, `A1=source`.
- Before the shared callback repair `fa6e6757`, the new non-idempotent throw case failed at
  runtime with `ExceptionError: null / payload: null`. The old
  `using(new CloseProbe()) { throw ...; }` path failed the same way. GDB showed a nested
  `@close` call entering the outer `CATCH` and clearing its pending exception.
- `cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_buffer_pool_ffi_test -j 4`:
  GCC Debug, final source rebuild 2/2 edges, exit 0.
- `/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_buffer_pool_ffi_test --lease-cleanup-plain`:
  2/2 pass; both `using`
  forms leave plain value `7` readable.
- `/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_buffer_pool_ffi_test --lease-cleanup-bare`:
  1/1 pass.
- `/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_buffer_pool_ffi_test --lease-cleanup`:
  10/10 pass. Normal non-idempotent close, original and replacement throw, same-scope bare
  dedupe, nested close once, older outer lease protection, and PoolLease normal/bare cases pass.
- `/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_buffer_pool_ffi_test --lease-cleanup-throw`:
  4/4 pass. Script `@close` replacement fixtures inside `fn run()` and at top level both return
  11, proving the catch saw `"replacement"` and close ran once.
- `/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_buffer_pool_ffi_test --closed-owner`:
  4/4 pass.
- `/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_buffer_pool_ffi_test`:
  default suite 23/23 pass, including the original PoolLease throw and pinned pointer/GC
  positives. Two active-view-close compilation diagnostics are expected negative cases.

## Throw failure isolation and top-level regression

The final root bytecode uses `MARK_CLOSE_PROXY` opcode 245 at instruction 16 with `E=6`,
`A1=2`; the throw literal is written to slot 7 and `THROW` reads slot 7. Thus throw does not
overwrite the proxy token. A plain `try { throw "boom"; } catch { }` returns 9. Before
`fa6e6757`, the old `using(new CloseProbe()) { throw "boom"; }` expression path also failed
with the same null exception, so that runtime failure was not proxy-specific.

GDB shows the proxy callback entering with a current OBJECT exception, runtime-error status,
and one exception handler. During nested `@close` execution, `ZrCore_Execute` reaches `CATCH`
at `execution_dispatch.c:9760` through `ZrCore_Function_CallWithoutYield` and
`closure_value_call_close_meta`; `ZrCore_Exception_ClearCurrent` then clears the outer
exception. The earlier clear inside `execution_clear_pending_control` saves and restores the
exception and was not the lost-state point. The shared core callback repair is separately
committed as `fa6e6757`.

The first top-level replacement fixture attempted to read `error.exception` inside a catch
branch's `if` and failed compilation with `Failed to lower load through pre-execution Semantic
IR`. Source controls isolated the stale type binding:

- A top-level `try/throw/catch` with the same `if (error.exception == "original")` and no
  `using` compiles, executes, and returns 10.
- Top-level old-form `using(new CloseProbe())` with a replacement throw fails at the same catch
  condition load, before execution.
- Top-level `using(existingLocal)` with an ordinary non-throwing close and an original throw
  fails at the same catch condition load, before execution.

The identical replacement check inside `fn run()` compiled and returned 11. A later no-using
control with a class declaring `@close(error)` failed at the same top-level catch read, while
renaming the catch binding passed. Class and function parameter bindings had leaked into the
enclosing `TypeEnvironment` and masked the later `catch(error)`. Commit `ffcd0e2b` isolated
callable parameter type scopes and restored their parent on success and compilation error.
The focused source CFG test passes 62/62, including old-form using, same-name and distinct-name
catch controls; the unsupported top-level source CFG remains inactive and its legacy catch path
is still used. After that support fix, the new top-level proxy fixture compiles and returns 11.

## Remaining gate

The opcode tests validate AOT C/LLVM emitted calls, C shared-library linkage, and the direct
AOT helper. Executing a generated AOT entry containing this source pattern remains a separate
gate for the broader AOT plan.
