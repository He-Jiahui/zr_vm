---
related_code:
  - zr_vm_core/include/zr_vm_core/native_call_contract.h
  - zr_vm_core/src/zr_vm_core/native_call_contract.c
  - zr_vm_core/src/zr_vm_core/native_call_lease.c
  - zr_vm_core/src/zr_vm_core/native_call_marshalling.c
  - zr_vm_core/src/zr_vm_core/native_call_callback.c
  - zr_vm_library/include/zr_vm_library/native_call_plan.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_call_plan.c
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/runtime.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_contract.c
  - zr_vm_common/include/zr_vm_common/zr_ffi_contract.h
tests:
  - tests/ffi/test_ssa_native_abi.c
  - tests/ffi/test_ffi_module.c
  - tests/library/test_call_binding_native_registry.c
  - tests/core/test_native_inline_span_dispatch.c
doc_type: module-detail
last_verified: 2026-09-13
---

# Native and FFI call contract

`SZrNativeCallPlan` is the one runtime description consumed by the native,
FFI, VM and AOT call lanes.  It is produced from a validated
`SZrNativeImportContract` and contains only fixed-width values and bounded
arrays.  In particular, it never stores a function pointer, a module address,
or a managed-heap pointer.  A module reload therefore cannot leave a stale
address in a cached plan: the caller resolves an address or callback identity
for each process and passes it to `ZrCore_NativeCall_InvokeResolved` (or the
library wrapper with the same name).

The plan records the target calling convention and charset alongside its
signature/layout hashes, so a consumer does not need to consult a mutable
source contract when choosing a C/libffi frame or string conversion rule.

The plan also carries the fixed-width `sourceId` copied from the import symbol
(or declaring module when no symbol id is assigned).  Validation, marshal, and
lease diagnostics preserve that identity so a failure after load remains
attributable to the originating import.

## Prepare and validate

Use `ZrLibrary_NativeCall_Prepare` (or
`ZrCore_NativeCall_Prepare`) at load time.  The library/provider entry points
also enforce the runtime capability mask (a core-only caller must perform its
own capability check).  Preparation first runs
`ZrCommon_NativeImportContract_Validate`, then checks the optional expected
signature/layout hashes and target ABI environment.  A layout hash can be
obtained with `ZrCore_NativeCall_ComputeLayoutHash`.  Mismatches are reported
as `SIGNATURE_MISMATCH`, `LAYOUT_MISMATCH`, or `ABI_MISMATCH`; they are never
silently downgraded to a copy that has a different calling convention.

The parameter class is inferred when `parameterClasses[i]` is `AUTO`, or can
be supplied explicitly.  Explicit classes must agree with the contract:

| Class | Contract type and lane |
| --- | --- |
| `SCALAR` | integer, float, bool, or enum; direct only when stable and compatible |
| `INLINE_STRUCT` | blittable or copied struct/union bytes |
| `CONTIGUOUS_VIEW` | pointer-shaped input; pin for a stable direct lane, otherwise copy |
| `TYPED_ARRAY` | typed pointer input; same conservative pin/copy rule |
| `REF_OUT` | `ref`/`out`; temporary copy plus explicit write-back and root |
| `VARARGS` | variadic bridge; no direct fast lane |
| `CALLBACK` | callback registry/bridge; callback ID is resolved at invocation |

`DIRECT` is selected only when the caller allows it, the native address is
known stable for the call, the ownership is borrowed/pinned, and the contract
layout is compatible.  `PIN`, `COPY`, and `REGISTERED` marshalling requests
are retained in the plan.  Transfer/shared ownership on identity-bearing
pointer/view values and variadic or callback parameters force the bridge;
transfer/shared on a scalar or inline aggregate is rejected because there is
no lifetime witness to retain.  Unknown classes, marshalling kinds, flags, or
callback IDs fail preparation with a structured diagnostic.

Packed or otherwise non-natural parameter alignment does not make the call
invalid by itself: the direct lane is disabled and copy/bridge temporaries are
required to use the target ABI's safe alignment.  Return values have no
temporary in this compact plan, so an incompatible aggregate return is
rejected instead of being presented as a potentially misaligned direct value.
Callback returns are rejected for the same reason: callback IDs and roots are
parameter-side witnesses, and a return callback would otherwise have no
resolver-owned lifetime slot.

## Lease, roots, and thread state

Before invoking a plan, begin a `SZrNativeCallLease` with
`ZrCore_NativeCall_LeaseBegin`.  The lease enters the domain's native
safepoint mode and records every resource acquired during the call:

1. Attach an external mutator with `ZrCore_NativeCall_Attach` before entering
   the domain.  Detach it only after the lease has ended.
2. Register each managed value used by a pinned-direct operation with
   `ZrCore_NativeCall_LeasePinValue` (or pin a raw object with
   `ZrCore_NativeCall_LeasePinObject`).
3. Register retained callback contexts or `ref`/`out` objects with
   `ZrCore_NativeCall_LeaseRootObject`.
4. Marshal each argument.  Copy/bridge operations use caller-owned temporary
   storage and report `requiresWriteback` for `ref`/`out`.
5. Invoke the resolved target and call `ZrCore_NativeCall_WriteBackArgument`
   on every successful `ref`/`out` path.  A marshal request may defer this
   operation (`writeBack = false`), but the result bit remains authoritative.
6. Always call `ZrCore_NativeCall_LeaseEnd`, including exception and partial
   initialization paths.  It releases roots and pins in reverse order and
   leaves native state symmetrically.

The lease API rejects inactive state, missing domains, null non-nullable
arguments, undersized buffers, and misaligned direct/temporary storage.  A
nullable pointer may use a null direct value; a null scalar or non-nullable
reference is rejected.  Managed pin/root lanes are also rejected for
`BLOCKING_DETACHED` and `NO_SAFEPOINT_CRITICAL` plans because those modes do
not permit a moving-GC interaction while the native frame is active.  A
successful pin/root API call acknowledges its plan obligation even when the
value is a native scalar, a null optional pointer, or another no-op; the lease
keeps those acknowledgements separate from the concrete GC records it later
releases.

The plan and lease records use bounded inline storage and do not allocate.
Failures from the underlying GC pin/root registration are reported as
`PIN_FAILED`/`ROOT_FAILED`; `LeaseEnd` can be called on every partial path and
is idempotent.  A cancelled execution budget is surfaced as `BUDGET`, while a
second lease or callback registration is rejected with `ALREADY_ACTIVE` (or
the corresponding callback state) before any existing resources are changed.
This keeps OOM, cancellation, duplicate-call, and partial-initialization paths
observable without leaving a temporary buffer, pin, root, or native-depth
entry behind.

## Callback lifetime

`SZrNativeCallbackSlot` stores only callback ID, generation, state, and an
in-flight count (plus a small atomic lock); it stores no callback address.
`Register` publishes an active identity, `Enter` increments the in-flight
lease while holding the transition lock, and `Leave` decrements it.  Unregister
transitions `ACTIVE -> CLOSING`, rejects new entries, waits for existing
entries to leave, then clears the identity and reaches `UNREGISTERED`.
`ZrCore_NativeCallback_Unregister(..., maxSpins, ...)` is non-blocking when
`maxSpins` is zero and no call is active; if calls remain it reports
`CALLBACK_IN_FLIGHT` and leaves the slot in `CLOSING` so a later finish can
complete safely.

Callback thread and exception policies remain part of the plan.  A callback
running on an unattached thread must use the attach helper, and its resolver
must translate an exception to the contract's error result before crossing
the C ABI (the invoker reports `EXCEPTION_TRANSLATION` on failure).  The C
frame is never unwound by a managed or foreign exception.  The compact lease
supports `CALL` callback lifetime; `SCOPED` and `STATIC` are rejected until a
provider supplies a persistent root registry and an explicit close protocol.

## Resolver boundary

`ZrLibrary_NativeCall_Invoke` intentionally returns `INVOKE_UNRESOLVED`: a
persistent plan has no safe place to keep a process-local address.  Resolve the
current module entry and pass a small invoker to
`ZrLibrary_NativeCall_InvokeResolved` instead.  The resolver may use the plan's
signature/layout/callback IDs, but must not bypass capability or thread-policy
checks.

The FFI provider helper `ZrVmLibFfi_PrepareNativeCallPlan` uses the same core
contract, checks the provider capability mask, and deliberately marks native
addresses unstable.  Consequently a typed FFI signature still takes the
pin/copy decision; a type annotation alone never grants a pointer across a
moving collection.  The existing libffi invoke path remains the process-local
resolver/adapter; this helper is the shared load-time contract boundary, so a
provider can adopt the plan without duplicating symbol-name lookup.

The contiguous-view ownership rules used by FFI buffers and pooling are
described in [Pooling And Pinned FFI Views](zr-pooling-and-pinned-ffi-views.md).
That document's owner-rooted view lease and this call lease are separate
lifetimes: closing one does not implicitly retain the other.

## Focused verification

The focused fixture is `tests/ffi/test_ssa_native_abi.c`.  From WSL:

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_native_abi_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_native_abi$' \
  --output-on-failure --no-tests=error
```

It covers direct versus copy lanes, explicit class/layout rejection,
alignment and uninitialized `out` handling, ref/out write-back, callback
identity/root obligations, variadic (including zero-fixed-argument) bridge
selection, in-flight rejection, and quiescent zero-spin unregister.  Sanitizer and
cross-platform build jobs should retain this test as a required ABI-boundary
check.

On 2026-09-13 the fixture was compiled against the WSL GCC core archive and
ran with exit code 0; a fresh GCC AddressSanitizer/UndefinedBehaviorSanitizer
build also exited 0.  Strict C11 GCC and Clang syntax checks covered all four
core contract/lifetime translation units plus the library and FFI adapters.
The injected failures assert ABI/signature/layout/class mismatches,
unsupported ownership/marshalling, alignment, missing temporary storage,
uninitialized `out`, callback in-flight unregister, and inactive lease paths;
the successful `ref`/`out` case verifies one explicit write-back and lease
cleanup remains bounded and allocation-free at the plan/record layer.
