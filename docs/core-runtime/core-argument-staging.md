---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_call_transfer.h
  - zr_vm_core/src/zr_vm_core/execution/execution_call_transfer.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_argument_staging.h
  - zr_vm_core/src/zr_vm_core/function_argument_staging.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_argument_staging.c
plan_sources:
  - docs/plans/ssa/04-frame-native/02-call-return-tail.md
tests:
  - tests/core/test_argument_staging.c
  - tests/core/test_argument_staging_vm.c
  - tests/cmake/ssa-argument-staging-tests.cmake
  - tests/acceptance/ssa-core-argument-staging.md
doc_type: module-detail
---

# Finite VALUE argument staging

This D-only candidate changes `ZrCore_Function_CopyValueFrameParametersFromFrame` for complete selected sets of normalized nonowning native scalar VALUE parameters. It snapshots all selected source bytes before changing packed destinations or their distinct, nonoverlapping dense mirrors. Exact self-copy and permutations preserve original inputs; a late invalid selected place, conflicting output bytes or insufficient workspace leaves the entire frame backing unchanged.

## Selection and representation

The existing direct-summary or full inventory scan, parameter ordinal, count limit, individual start-slot overflow skip, nonparameter skip, non-VALUE/short-slot skip and direct-summary termination remain. A zero count or no selected VALUE rows succeeds without obtaining workspace or resolving frame ranges. Unused argument indices cannot cause aggregate range overflow rejection. Missing source descriptors use dense storage; packed NULL prefers a non-NULL dense value. Runtime `SZrFunction` descriptors and actual stack allocation prove ranges and alignment; the dense extent includes `stackSize` even for a function with zero `frameByteSize`. Selected descriptor keys must be unique; multiple logical sources sharing a physical range are permitted.

Admitted values have `isNative=true`, no GC flag, NONE ownership and null ownership pointers, with NULL, BOOL, integer or FLOAT/DOUBLE tags. The only admitted descriptor flag is the existing DIRECT_VALUE cache flag. Receiver, constructor bitmap, alias, indirect alias, borrowed alias and unknown bits select UNSUPPORTED. Owning/boxed/ref representations also select UNSUPPORTED for the whole selected set before any frame writes, then the original legacy implementation runs once. Selected invalid storage and conflicting output bytes are terminal failures; they never select sequential legacy fallback. Identical actual output bytes can overlap. The original suppression of a dense mirror partially overlapping its packed destination remains.

## Workspace and lifetime

Eight rows use a local aligned workspace; larger inventories use ordinary CRT `malloc/free`, with checked sizing before resolving any borrowed frame place. No VM allocator, GC, stack growth, semantic copy/drop, host callback or suspension occurs within the finite transaction. CRT allocation is temporary native storage and does not increment VM allocation metrics. A caller keeps functions, descriptor arrays, state, global and profile objects alive throughout this synchronous call. Frame offsets do not themselves retain metadata. CRT replacements that reenter the VM are outside this internal leaf contract.

External workspace is aligned and disjoint from the full stack backing, functions/layout arrays, state/global, state profile, current TLS profile and diagnostic storage. Diagnostics must be valid independently writable storage. A diagnostic alias into frame or control storage is rejected without writing through it. Failure may change disjoint workspace and diagnostic/profile observations; frame payloads remain unchanged. Internal statuses distinguish UNSUPPORTED, INVALID_ARGUMENT, ALIAS_CONFLICT, NO_MEMORY and SCRATCH_TOO_SMALL; the existing public bool ABI is unchanged. New private helper symbols follow the existing `ZR_CORE_API` export convention so focused tests can link either Core variant.

## Profiling and registration

Eligibility inspection has no speculative counters. Legacy records its original counters once. Finite success preserves layout visits, DIRECT/CHECKED per row, two STACK_GET_VALUE observations per row, and VALUE_RESET_NULL/CONSTRUCT/COPY plus copied-byte metrics for each actual packed or nonsuppressed mirror write. Zero arguments records EMPTY. Failed resolved finite validation records scan visits and no committed-copy counts. Disabled profile switches remain effective.

The new fragment uses existing `zr_vm_add_unity_test_target` and `zr_vm_link_core`; that link helper selects shared Core when enabled, otherwise static Core. `zr_declare_module` discovers the new C file through its existing CONFIGURE_DEPENDS glob. The fragment has two genuine test registrations; parent inclusion and final registered execution require a separate coordinated lease.

## V5 compatibility controls

The registered `test_argument_staging.c` includes the previously supplemental
controls: an out-of-count ordinal is ignored before its invalid unused layout
is resolved; an individual `startSlot + ordinal` overflow is skipped; and a
packed destination that partly overlaps its dense mirror suppresses that mirror
write. The last control compares the entire backing against exactly one packed
write and checks one VALUE_COPY and VALUE_RESET_NULL event.

A distinct FLOAT tag has a separate normalized representation control, alongside
the DOUBLE constructor control. An actual State_Create/Function_New and
SharePlainValue producer exercises public FromFrame's unchanged owning-value
legacy branch: both destinations retain the real shared control, the strong
count becomes three, and the layout/direct/stack/value-copy counters occur once.
This does not add ownership staging to finite A.

## Remaining entry and lifecycle gates

Actual nonoverlapping allVALUE VM entry is covered using `PreCallKnownVmValueWithArgumentSource`, real state/function construction and a packed-only scalar. Existing `function.c` clears VM frame padding before it calls FromFrame and falls back to dense copying when the helper returns false. Thus whole pre-call overlap and failure atomicity require a separate C entry design. This candidate does not modify `function.c`, inline-span semantics, ownership paths, tail reuse, return forwarding, parser layouts or runtime/parser identity.

## V6 candidate validation status

The exact production five files, two tests and registration fragment remain byte
equal to V5. The D candidate passed actual GCC and Clang ASan/UBSan matrix and
nonoverlapping allVALUE VM consumer gates. Each toolchain compiled nine actual
matrix/provider/harness/Unity TUs and one actual VM TU; the VM link reused that
toolchain's eight terminal provider/harness objects. Three fresh production TUs
provide frame placement, call transfer and staging. The retained current GNU
Core archive is plain, so this is not a whole instrumented Core claim.

E activation and current-E registration/epoch validation remain pending. The
independent Linux manifest is `argument-staging-design/linux-v5/manifest.json`,
SHA `e8b9f13eb8619afc7cf6346582db63d68b7d513f12248741ac99a17158e30ae0`.

## V7 independent Linux temporary-directory proof

Root independently repeated all four accepted ELF binaries using explicit Linux
TMPDIR, TMP and TEMP under its own D directory, alongside ASan/UBSan options.
All four runs returned exit0 without timeout; their logs equal the original
results byte for byte, and all 190 before/after pins remain equal. The root
receipt is `validation-control/argument-linux-root-v5/receipt.json`, SHA
`da855841788327b7503d2d0925cf2b99db5c3c3608e23087f7dedcea2c7d18fe`.

The original four runtime receipts establish real successful execution but do
not establish Linux temporary-directory propagation from the Windows wrapper.
The Linux compiler driver's current code explicitly sets its child environment,
and recorded run paths and GCC temporary artifacts support that provenance;
the exact executed driver source bytes were not presealed. This limitation is
retained rather than presenting a current driver hash as an execution pre-hash.
GCC VM's first 55-dependency proof has literal valid=true but incomplete union
coverage; its separate final complete-deps proof supersedes that coverage.
The original V4/V5/V6 and Linux seals remain immutable. E activation and
current-E registered/provider acceptance remain pending.
