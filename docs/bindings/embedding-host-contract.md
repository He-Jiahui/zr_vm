---
related_code:
  - zr_vm_rust_binding/include/zr_vm_rust_binding.h
  - zr_vm_rust_binding/include/zr_vm_rust_binding/conf.h
  - zr_vm_rust_binding/src/zr_vm_rust_binding/internal.h
  - zr_vm_rust_binding/src/zr_vm_rust_binding/api.c
  - zr_vm_rust_binding/src/zr_vm_rust_binding/native.c
  - zr_vm_rust_binding/src/zr_vm_rust_binding/value.c
  - zr_vm_rust_binding/src/zr_vm_rust_binding/project_session_checkpoint.c
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/src/lib.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/src/native.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/src/execution_budget.rs
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_library/src/zr_vm_library/common_state.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_official_inventory.c
  - zr_vm_library/src/zr_vm_library/project/project_module_specifier.c
  - zr_vm_core/include/zr_vm_core/global.h
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_method_binding.c
implementation_files:
  - zr_vm_rust_binding/include/zr_vm_rust_binding.h
  - zr_vm_rust_binding/src/zr_vm_rust_binding/api.c
  - zr_vm_rust_binding/src/zr_vm_rust_binding/native.c
  - zr_vm_rust_binding/src/zr_vm_rust_binding/value.c
plan_sources:
  - user: 2026-11-22 plan-jit-perf-gc-bridge r2, unit BR-D1 (gap B-G1), facts-only embedding host contract inventory
tests:
  - tests/rust_binding/test_rust_binding_api.c
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/tests/native_registration.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/tests/execution_budget.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/tests/project_session_checkpoint.rs
  - tests/parser/test_aot_c_metadata_binding_loader.c
doc_type: module-detail
---

# Embedding host contract (inventory)

> 摘要：本文只记录事实，不做设计。它列出宿主（例如 Zircon Engine）今天通过 `zr_vm_rust_binding` 嵌入 ZR VM 时可以依赖的契约，以及当前**没有提供**的能力。设计取舍属于后续 RFC，本文不作选择。

This page lists what an embedding host can rely on today when it embeds the VM through `zr_vm_rust_binding`, and what is not provided. It is an inventory, not a design. Each claim cites the code or design text it comes from. Behavior and API details are in [ZR VM Rust Binding](../cli-and-tooling/zr-vm-rust-binding.md); build details are in [Rust binding builds and generic descriptors](./rust-build-and-generics.md).

Facts were read from commit `18d902d6c` (the head of PR #59, D-U1). Statements marked **reported** come from outside this repository and were not re-checked here. Statements marked **not verified** describe a gap in the evidence, not a confirmed absence.

Path key for short file names used below:

- `zr_vm_rust_binding.h`, `conf.h`: `zr_vm_rust_binding/include/` (`conf.h` is `zr_vm_rust_binding/include/zr_vm_rust_binding/conf.h`).
- `api.c`, `native.c`, `value.c`, `internal.h`, `project_session_checkpoint.c`: `zr_vm_rust_binding/src/zr_vm_rust_binding/`.
- `lib.rs`, `native.rs`, `execution_budget.rs`: the safe crate, `zr_vm_rust_binding/rust/zr_vm_rust_binding/src/`.
- `native_binding*.c`, `native_binding_dispatch_lanes.h`: `zr_vm_library/src/zr_vm_library/native_binding/`.
- `common_state.c`: `zr_vm_library/src/zr_vm_library/`; `project_module_specifier.c`: `zr_vm_library/src/zr_vm_library/project/`.
- `gc_domain.h`: `zr_vm_core/include/zr_vm_core/`; `metadata_runtime_method_binding.c`: `zr_vm_core/src/zr_vm_core/metadata/`.
- `…-10-native-ffi-module-package-design.md`: `docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md`.

## 1. ABI surface

- The only host-facing contract is the C header `zr_vm_rust_binding/include/zr_vm_rust_binding.h`. It declares 90 `ZR_RUST_BINDING_API` entry points. All handles are opaque; no VM struct layout crosses the boundary.
- Every entry point returns `ZrRustBindingStatus` except `GetLastErrorInfo`, `ExecutionNowMicros`, the `CancellationToken_*` calls, and `Value_GetKind`/`Value_GetOwnershipKind` (`zr_vm_rust_binding.h:11-23`). The message comes from `ZrRustBinding_GetLastErrorInfo`, with capacity `ZR_RUST_BINDING_ERROR_MESSAGE_CAPACITY` = 512 bytes (`conf.h:8`, `zr_vm_rust_binding.h:52-55`).
- Rust layering: `zr_vm_rust_binding_sys` has hand-written raw declarations. Its `Cargo.toml` has an empty `[build-dependencies]` and no bindgen. The safe crate `zr_vm_rust_binding` depends only on `_sys`; its only dev-dependency is `tempfile`. The binding has no proc-macro or derive crate.
- Neither the binding header nor its option structs carry an ABI version query or a struct-size/version field. Native module descriptors carry `minRuntimeAbi`/`requiredCapabilities` through `NativeModuleBuilder_SetRuntimeRequirements` (`zr_vm_rust_binding.h:527-530`). The builder default is `ZR_VM_NATIVE_RUNTIME_ABI_VERSION`, which is `4U` (`native.c:1239`, `zr_vm_common/include/zr_vm_common/zr_abi_conf.h:11`).
- Execution modes exposed to the host are `INTERP` and `BINARY` (`.zro`) only (`zr_vm_rust_binding.h:25-28`). The binding has no AOT or JIT execution mode.
- A runtime from `Runtime_NewBare` cannot run a project, call an export, or start a session; those calls return `UNSUPPORTED` (`api.c:871`, `api.c:951`, `api.c:1098`). Execution requires `Runtime_NewStandard`.

## 2. Runtime options

`ZrRustBindingRuntimeOptions` has exactly four fields: `heapLimitBytes`, `pauseBudgetUs`, `remarkBudgetUs`, and `workerCount` (`zr_vm_rust_binding.h:57-62`; Rust `RuntimeOptions`, `lib.rs:74-79`).

- `Runtime_NewBare`/`NewStandard` only copy the struct (`api.c:383-411`). The options are applied to each VM global after it is prepared for `Project_Run`, `Project_CallModuleExport`, and `ProjectSession_Start` (`api.c:897`, `api.c:975`, `api.c:1115`). `Project_Compile` does not call the apply step (`api.c:783-812`).
- A zero field means "leave the core default" (`api.c:332-348`). Non-zero values go to `ZrCore_GarbageCollector_SetHeapLimitBytes`, `SetPauseBudgetUs`, and `SetWorkerCount`.
- `workerCount` is the GC worker count passed to the collector. It is not a script-thread or mutator count.

## 3. Session GC, budget, and checkpoint

A project session (`ProjectSession_Start`, `api.c:1084-1150`) runs the entry module once and keeps that VM global for later export calls.

- **GC step.** `ProjectSession_GcStep(session, maxPauseMicros, out)` sets the pause budget and runs one core `GcStep` (`api.c:1184-1226`). It refuses a session that is already inside an export call. `rootCount` is `ignoredObjectCount + crossBoundaryReferenceCount`, and the cross-boundary count excludes checkpoint references (`api.c:1211-1223`). The open TODO on OOM unwinding out of a void `GcStep` is recorded in `zr-vm-rust-binding.md` §"Session GC 的安全 Rust 契约".
- **Call budget.** `ProjectSession_CallModuleExportWithBudget` accepts an instruction limit, a monotonic deadline, a cancellation token, and limits on heap bytes, native calls, and GC microseconds (`zr_vm_rust_binding.h:101-114`). Its usage report has a termination reason (`zr_vm_rust_binding.h:91-99`, `116-123`). The header contract (`zr_vm_rust_binding.h:466-480`) says:
  - Checks are cooperative and run before bytecode fetch and around native callbacks.
  - Termination does not roll back globals or heap.
  - The heap limit is checked after allocation, so one allocation or a running native callback can overshoot without bound.
  - Heap usage counts only the VM allocator.
  - A session is single-threaded.
- **Checkpoint / rollback.** `ProjectSession_Checkpoint` is refused while an export call is active or while any live `Value` handle exists beyond the session and existing checkpoints (`project_session_checkpoint.c:8-41`). `Rollback` has the same condition and also requires a checkpoint from the same session owner (`project_session_checkpoint.c:43-62`). A checkpoint keeps an owner reference, so it can outlive the session handle, but rollback still needs the session.

## 4. Live-value rooting

- A VM-backed `ZrRustBindingValue` holds an execution-owner reference and a `ZrLibTempValueRoot` (`internal.h:86-102`). Creating one retains the owner and begins the root (`value.c:495-515`). `Value_Free` ends the root and then releases the owner (`value.c:384-392`).
- The session, checkpoints, and live `Value`s share one owner reference count. The last release frees the VM global, if the owner owns it, and then the captured native modules (`value.c:355-376`). The count is not atomic, so all owner operations must be serialized on one thread (`internal.h:26-37`).
- Values created inside a native callback (arguments and `self`) use an owner with `ownsGlobal = ZR_FALSE` (`native.c:185-214`). The header records a known bug: such a handle does not keep the VM global alive. If it escapes past the session's release, using or freeing it touches a freed global (`zr_vm_rust_binding.h:609-610`, `native.c:184`).
- Argument views and their string pointers are borrowed for the visitor call only (`zr_vm_rust_binding.h:580`).
- Rooting is only these temp-value roots plus owner references. The binding does not use `SZrGcRootHandle` (see §11).

## 5. Native registration lifecycle

1. `NativeModuleBuilder_New`, then `Add*`. Builder calls copy descriptor text and arrays. On success, `AddFunction` takes ownership of the callback `userData`; on failure the caller still owns it (`zr_vm_rust_binding.h:254`).
2. `NativeModuleBuilder_Build` moves the module out and leaves the builder empty (`native.c:632-643`).
3. `Runtime_RegisterNativeModule` retains the module in the runtime's registry. A second active module with the same `moduleName` on the same runtime returns `ALREADY_EXISTS` (`native.c:660-718`).
4. The descriptor is installed into a VM global only when that global is created. The bootstrap callback (`api.c:350-352`) runs after the standard modules are registered (`zr_vm_cli/src/zr_vm_cli/project/project.c:355-375`; called from `zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:876` and `zr_vm_cli/src/zr_vm_cli/compiler/compiler.c:884`). It calls `ZrLibrary_NativeRegistry_RegisterModule` for each active entry (`native.c:391-421`). This happens on each `Project_Compile` (when given a runtime), `Project_Run`, `Project_CallModuleExport`, and `ProjectSession_Start`. I found no other install path, so a module registered after a session started is **not verified** to become visible in that session.
5. The registry records the descriptor with registration kind `BUILTIN`, keyed by `moduleName` (`native_binding.c:278-284`). It rejects the removed bare name `debug`, provider-role or phase mismatches against the official inventory, and replacement of an official module by a different descriptor (`native_binding_official_inventory.c:64-130`).
6. `RuntimeNativeModuleRegistration_Free` removes the entry from the runtime registry, so future globals do not see it. An execution owner captures the active modules when it is created (`value.c:342`, `native.c:1157-1200`). Live sessions and values therefore keep the old descriptor until their last reference is released (`zr_vm_rust_binding.h:553`). Callback `destroyUserData` hooks run when the last module reference is released (`native.c:916-928`, `native.c:1124-1155`); the Rust side frees its boxed closure there (`native.rs:1878-1882`).
7. Native callbacks run between `ZrCore_GcDomain_NativeEnter` and `NativeLeave` (`native_binding_dispatch_lanes.h:262-280`). The Rust trampoline catches panics and returns `INTERNAL_ERROR` (`native.rs:1852-1876`).

## 6. Compile-time descriptor visibility

- Design rule, Syntax 10 §5.0.3 (`docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md:329`): static host injection must provide the same descriptor catalog or contract hash before compile. Registration that exists only as a runtime callback cannot satisfy a static `import` or TypeRef; it can only back dynamic `loadModule`/`loadPlugin`. §4.6 (same file, `:216-232`) names `ZrLibModuleDescriptor` as the single descriptor source and forbids a second core registry.
- Today, `Project_Compile(runtime, …)` runs the CLI compile pipeline with the same bootstrap. Host modules registered on that runtime are therefore visible to a compile **inside the same host process** (`api.c:783-812`). With `runtime == NULL`, no host modules are installed.
- At `18d902d6c` I found no ABI entry, CLI flag, or LSP path that exports a host descriptor catalog for an offline compile or the language server. (`git grep` for "descriptor catalog", "dump-native", and "native-catalog" over `zr_vm_cli`, `zr_vm_rust_binding`, and `zr_vm_language_server` returns nothing.)
- The §5.0.3 manifest key `nativeProviders` appears only in docs, the fixture `tests/fixtures/projects/syntax_reference_v1/syntax_reference_v1.zrp:26-32`, and a string assertion in `tests/parser/test_syntax_reference_v1.c:557`. No product C source parses it. The separate shared-library plugin path looks up the entry symbol `ZrVm_GetNativeModule_v1` (`native_binding_registry_plugin.c:239`).

## 7. RegisteredNative naming

- Design rules, Syntax 10 §5.0.1–§5.0.2 (`…-10-native-ffi-module-package-design.md:285-307`):
  - `zr.*` is reserved for OfficialNative.
  - A host module is imported as `native:<segments>` (RegisteredNative).
  - `native:engine.render` and the workspace module `engine.render` are distinct identities.
  - `native:zr.*` is illegal.
- Specifier parsing implements the prefix. `native:<segments>` becomes `REGISTERED_NATIVE`, and segments `zr`, `zr.*`, or `zr/*` are rejected (`project_module_specifier.c:124-126`, `project_module_specifier.c:264-276`).
- The binding does not apply these rules. `NativeModuleBuilder_New` takes the module name as a plain string, and the registry keys on that string (§5). The binding's own tests register bare names and import them as bare names, for example `import("host_demo")` (`tests/rust_binding/test_rust_binding_api.c:1346`, `zr_vm_rust_binding/rust/zr_vm_rust_binding/tests/native_registration.rs:43`).
- **Not verified:** whether `import("native:<name>")` resolves to a binding-registered module end to end. No binding test covers it. I also found no registry-side check that rejects a `zr.*` name absent from the official inventory; the source tree has no `module.reserved_root` diagnostic.

## 8. Error propagation

- **Script → host.** When an export call fails, the binding copies the current exception message into the error snapshot as `RUNTIME_ERROR` (`api.c:220-236`).
- **Host → script: the failure text is lost.** The Rust trampoline returns only `error.status` (`native.rs:1873`). The C bridge logs "native callback failed" and returns `ZR_FALSE` (`native.c:259-268`). The dispatcher treats `false` with no thread error as "return null" (`native_binding_dispatch.c:619-625`). A failing host callback is therefore observed by the script as `null`.
- **Threading of errors.** The error snapshot is one process-global variable (`api.c:17`). The header records the bug that threads calling at the same time overwrite each other's snapshot (`zr_vm_rust_binding.h:333-335`). Cancellation-token operations do not touch it and may run on other threads (`zr_vm_rust_binding.h:491-493`).

## 9. Threading (as provided)

- A session is single-threaded (`zr_vm_rust_binding.h:480`). Owner reference counts are not atomic (`internal.h:26-27`).
- In the safe Rust crate, `Runtime`, `ProjectSession`, and `Value` wrap raw pointers and have no `Send`/`Sync` impl. The only `unsafe impl Send/Sync` in the crate is the cancellation-token inner type (`execution_budget.rs:15-16`).
- The binding does not expose the core multimutator or GC-domain mutator APIs (§11).

## 10. AOT native call-binding rows (D-U1, PR #59)

`18d902d6c` changed `metadata_runtime_validate_call_binding_rows` to accept a second `MODULE`-relocation row shape: `DIRECT` binding with a `TARGET_NATIVE` witness, a non-null native function and callable object, and a matching generation (`metadata_runtime_method_binding.c:336-351`). The deferred `VIRTUAL`/`INTERFACE` + `TARGET_NONE` shape is unchanged. Before this change, an AOT module with a statically typed native-provider member call failed to load.

Scope of the evidence:

- The tests are `test_aot_c_native_provider_module_row_executes`, `test_aot_llvm_native_provider_module_row_executes`, and the control `test_aot_c_typed_int_array_add_control_has_no_native_row` (`tests/parser/test_aot_c_metadata_binding_loader.c:708-725`).
- They use the official `zr.container` `Array<Actor>.add`, not a binding-registered host module.
- The binding has no AOT execution mode (§1), so a host cannot reach this path through `zr_vm_rust_binding` today.

## 11. Not provided

These are absent at the binding ABI. The core features listed exist but are not exposed. This list records gaps; it does not choose a design.

| Area | Core fact | Binding fact |
|---|---|---|
| Host allocator | `ZrCore_GlobalState_New(FZrAllocator, userData, …)` (`zr_vm_core/include/zr_vm_core/global.h:174-175`) | Globals are created by `ZrLibrary_CommonState_CommonGlobalState_New` with `ZrLibrary_CommonState_BuiltinAllocator` (`common_state.c:98`). No allocator field or entry point exists in the binding header. |
| GC domain selection / mapping | Domain identity, share, and mutator APIs in `zr_vm_core/include/zr_vm_core/gc_domain.h` | No domain parameter, query, or mapping in the binding header or sources. |
| `Gc<T>` / root handle for host-held script objects | `SZrGcRootHandle` with `ZrCore_GcRootHandle_Create/Clone/Update/Resolve/Release` (`gc_domain.h:54-58`, `gc_domain.h:189-218`) | Not exposed. Host-held values use temp roots plus owner references (§4). |
| Script-visible error raise from a host callback | — | No error-raising entry on `NativeCallContext`. Callback failure becomes `null` (§8). |
| Offline descriptor catalog for static import / LSP | Required by Syntax 10 §5.0.3 | None found (§6). |
| Bare-runtime execution | — | `UNSUPPORTED` (§1). |
| AOT/JIT execution through the binding | — | Only `INTERP`/`BINARY` (§1). |

## 12. Downstream pin (reported)

**Reported** (official ZE R17 review, ZE `aa73011a`, 2026-11-11; not re-checked here): Zircon Engine builds the binding from zr_vm `rev = 3c1352ec`. In this repository, `3c1352ec` is an ancestor of `18d902d6c`, and `git diff --stat 3c1352ec 18d902d6c -- zr_vm_rust_binding` shows 13 files, +459/−122. Moving that pin is a downstream decision and is out of scope for this page.
