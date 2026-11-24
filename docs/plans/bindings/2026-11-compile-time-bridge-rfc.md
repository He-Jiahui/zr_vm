---
related_code:
  - zr_vm_rust_binding/include/zr_vm_rust_binding.h
  - zr_vm_rust_binding/src/zr_vm_rust_binding/native.c
  - zr_vm_rust_binding/src/zr_vm_rust_binding/api.c
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/src/native.rs
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c
  - zr_vm_library/src/zr_vm_library/common_state.c
  - zr_vm_library/src/zr_vm_library/project/project_module_specifier.c
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/include/zr_vm_core/global.h
implementation_files: []
plan_sources:
  - "user: 2026-11-22 plan-jit-perf-gc-bridge r2, unit BR-R1 (compile-time bridge RFC)"
  - "user: 2026-11-23 t727u 「拍板交给opus，他有能力拍板」 — decisions Q-B1..Q-B7 recorded by the JGB decision register"
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/rust_binding/test_rust_binding_api.c
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/tests/native_registration.rs
doc_type: plan-record
status: planned
---

# BR-R1: compile-time host bridge RFC (decided)

> 摘要：本文记录 zr_vm 侧宿主桥接（Rust binding 嵌入，例如 Zircon Engine）的设计决定 Q-B1–Q-B7，并给出 BR-U1..BR-U4 的实施顺序与范围。这些问题原先等待 John 拍板；按 t727u 授权已由 Opus 决定，每条都记录了问题、答案、理由和被拒绝的选项。本文只规划，不代表任何一项已经实现。ZE 侧工作不在本仓库。

This page records the zr_vm-side design decisions for the host bridge (embedding through `zr_vm_rust_binding`) and the implementation sequence BR-U1..BR-U4. The questions were previously waiting on the owner; under t727u they are decided here, each with its answer, rationale and rejected options. **Nothing on this page is implemented yet.** It supersedes the r2 plan wording "compare without choosing": the comparison is done and the choices are below.

Facts come from the BR-D1 inventory, [Embedding host contract](../../bindings/embedding-host-contract.md) (PR #64; read at its head `f80f99daf`), and were re-checked for this page at main `b3782a2c8`. If #64 has not merged when you read this, the link above resolves only on that branch; the section numbers "BR-D1 §n" refer to it.

## 1. Problem

Hosts register native modules at run time (`ZrRustBinding_Runtime_RegisterNativeModule`, `zr_vm_rust_binding/src/zr_vm_rust_binding/native.c:660`). Four gaps follow from BR-D1:

| gap | fact | BR-D1 |
|---|---|---|
| G1 offline visibility | No ABI entry, CLI flag or LSP path exports host descriptors; an offline `Project_Compile` or the language server cannot type-check `import("native:…")` against host modules. Syntax 10 §5.0.3 requires "the same descriptor catalog/contract hash before compile". | §6 |
| G2 lost callback errors | The Rust trampoline returns only `error.status` (`native.rs:1873`); the C bridge logs and returns `ZR_FALSE` (`native.c:261-268`); the dispatcher turns that into `null`. | §8 |
| G3 naming not enforced | Specifier parsing rejects `native:zr.*`, but the binding registry accepts any module name, including `zr.*` names absent from the official inventory. `import("native:<name>")` against a binding module is not covered by any test. | §7 |
| G4 no host allocator | Globals use `ZrLibrary_CommonState_BuiltinAllocator` (`common_state.c:98`) although core accepts an allocator (`ZrCore_GlobalState_New(FZrAllocator, …)`, `global.h:174-175`). | §11 |

## 2. Decisions

Format: **Q** → **Answer** → **Why** → **Rejected**.

### Q-B1 Generation direction

- **Answer:** Rust-first, with a static **descriptor catalog** as the interchange artifact. Order: (1) BR-U4 catalog export from registered `ZrLibModuleDescriptor`s; (2) later, an optional Rust proc-macro crate that generates the existing `ModuleBuilder` calls plus the catalog at build time; (3) zr-first comptime declaration generation only as a *consumer* of the catalog, never a second source.
- **Why:** Syntax 10 §4.6 makes `ZrLibModuleDescriptor` the single descriptor source and forbids a second core registry; host implementations live in Rust; the catalog is exactly the missing piece of §5.0.3 and is useful with hand-written builders.
- **Rejected:** an IDL generating both sides (third language, parallel truth); zr-first comptime as source (cannot produce host code, still needs a catalog); proc-macro first without a catalog (does not fix G1).

### Q-B2 Reflection source of truth and TypeId ownership

- **Answer:** one-way, host-authoritative projection. The host's reflection (ECS) owns layout and fields; the generator projects it into descriptors; `zr.reflection` reflects those descriptors and is never written back. Cross-boundary identity is the zr name identity `native:<module>` + type name (§5.0.2). A host numeric TypeId may ride along as opaque descriptor metadata; zr never uses it for identity or equality.
- **Why:** keeps zr independent of any engine's TypeId scheme, reuses module identity rules, avoids two-way sync.
- **Rejected:** `zr.reflection` authoritative (the engine owns its types); host TypeId as zr identity (couples artifacts to an engine-internal scheme).

### Q-B3 Host callback failure semantics

- **Answer:** throw by default; no opt-in to `null`.
  - A callback returning `Err` becomes a script-visible runtime error carrying the host message, truncated to the existing 512-byte error capacity (`ZR_RUST_BINDING_ERROR_MESSAGE_CAPACITY`).
  - A caught Rust panic (`INTERNAL_ERROR`) becomes a runtime error with fixed text, e.g. `host callback panicked in <name>`.
  - A callback that wants `null` returns `Ok(null)`.
  - **Mechanism constraint:** the raise happens in the **C bridge after the Rust callback has returned**, using `ZrCore_Debug_RunError` (`zr_vm_core/include/zr_vm_core/debug.h:418`, `ZR_NO_RETURN`) or an equivalent library raise. A `longjmp` must never cross Rust frames. The trampoline hands the message over through an additive FFI setter on the native call context or an out-param; BR-U1 chooses, provided the ABI stays additive.
- **Why:** silent `null` hides failures; script → host already copies messages (BR-D1 §8), so this makes both directions symmetric; `Result::Err` maps naturally to an exception.
- **Rejected:** keep `null` (failure becomes data); per-function opt-in (more surface, wrong default); raising from inside Rust through an FFI that longjmps (UB across Rust frames).
- **Compatibility:** behaviour change for hosts relying on failure → `null`. BR-U1 documents it in `docs/cli-and-tooling/zr-vm-rust-binding.md` and its PR body.

### Q-B4 GC ownership for engine objects reachable from scripts

- **Answer:** per type, one of two v1 modes: (1) engine-owned + weak, generation-checked identity handle (use after destroy raises a runtime error via the Q-B3 mechanism); (2) value copy for plain-data structs. GC-managed engine objects (`Gc<T>` in the script domain) are deferred. The Rust side does **not** get raw `SZrGcRootHandle` in v1; host-held script objects stay on the rooted `ZrRustBindingValue` (BR-D1 §4).
- **Why:** the ECS owns entity lifetimes (no cross-heap cycles, no finalizer ordering problems); root handles would leak core CoW/generation semantics into the ABI; the owner refcount is non-atomic (BR-D1 §4/§9).
- **Rejected:** everything GC-managed (two owners); raw root handles now (too much ABI, given the callback-scoped handle bug at `zr_vm_rust_binding.h:609-610`).
- **Prerequisite (not a decision):** that header bug gets its own fix unit before any weak-handle work ships.

### Q-B5 Host memory: allocator, heap limit, domain mapping, multimutator

- **Answer:**
  1. Host allocator injection: yes, additive (BR-U2). New `ZrRustBinding_Runtime_NewBareEx` / `NewStandardEx` taking an options struct whose **first field is `structSize`**, adding `allocator` + `allocatorUserData`. The existing `ZrRustBindingRuntimeOptions` (4 fields, `zr_vm_rust_binding.h:57-62`) and entries stay unchanged.
  2. Per-instance heap limit: already provided (`heapLimitBytes`); no change.
  3. Domain mapping: one GC domain per binding runtime (current behaviour); no domain knob in v1; hosts isolate by creating a runtime per world/instance.
  4. Multimutator (M5) is out of scope for v1: one mutator thread per runtime; `workerCount` stays GC workers only.
- **Why:** core already takes an allocator; mutating the existing struct would break callers' struct size; the binding is single-thread by construction.
- **Rejected:** adding fields to the existing struct (ABI break); a domain-per-world knob inside one runtime (no binding-level domain API, needs M5-safe internals); M5 in scope.

### Q-B6 Module organisation and naming

- **Answer:** RegisteredNative `native:engine.<subsystem>`, one module per engine subsystem, no root aggregate; zr helper libraries on top use ordinary `@package` exports. `requiredCapabilities` uses only capability names that already exist in zr_vm (this RFC defines none). The binding enforces naming at registration (BR-U3): a host module named `zr`, `zr.*` or `zr/*` that is not an official-inventory entry is rejected with `INVALID_ARGUMENT`. Whether a module registered as `engine.render` must be imported as `native:engine.render` is verified end to end in BR-U3 first (BR-D1 marks it not verified).
- **Why:** granular hashes and imports; follows Syntax 10 §5.0.1–5.0.2; closes the registration-side hole (G3).
- **Rejected:** one `native:engine` root with leaves (every change re-hashes the whole surface); `@package` for engine natives (packages are zr-source identity); inventing capability names (not authorised).

### Q-B7 When ZE moves its pin off `3c1352ec` (zr_vm-side recommendation)

- **Answer:** one hop, after BR-U1 lands on main, to a main SHA whose `interpreter-gates` and supplemental validation runs are both green. On the bump ZE rebuilds the binding, regenerates cached `.zro`/AOT artifacts and reruns its binding tests. If BR-U1 slips past about 2026-12-07, bump anyway to the then-current green main; BR-U1 then rides a second bump.
- **Why:** BR-U1 is the first binding behaviour change ZE would notice; one re-baseline instead of two; the cap stops drift.
- **Rejected:** bump now (two re-baselines); wait for BR-U2/BR-U4 (unbounded).
- **Scope:** a sequencing recommendation only; no ZE tree is touched from this repository.

## 3. RFC-level decisions for BR-U4 (new in this RFC)

BR-U4's scope "comes from BR-R1", so this RFC fixes its shape. Recorded under t727u with the same format.

### D-BR-1 Catalog format

- **Answer:** a UTF-8, LF-terminated JSON file with `"schema": "zr.native-catalog/1"`, a `modules` array and a whole-file `catalogHash`. Each module entry is a lossless projection of the fields of `ZrLibModuleDescriptor` that affect compile-time typing (module name, version, `minRuntimeAbi`, `requiredCapabilities`, `publicContractHash`, constants, functions, types, type hints, module links, attribute and canonical type roles) plus `signatureHash`, the value of the existing `ZrLibrary_NativeRegistry_ComputeModuleSignatureHash` (`zr_vm_library/include/zr_vm_library/native_registry.h:123`, implemented in `native_binding_call_binding_hash.c:188`). Callbacks, `userData` and `onMaterialize` are never serialised.
- **Why:** JSON is already the repository's descriptor interchange (`typeHintsJson`, `.zrp`, `.zrm` metadata; cJSON is vendored in `zr_vm_library`). Reusing the registry's own signature hash means a compile against the catalog and a run against the live registration agree on one number, and drift is detectable without a new hash.
- **Rejected:** a binary format (no reader tooling, harder review); reusing the `.zrm` archive (that is a zr-source package artifact with Package identity, not RegisteredNative); generated C headers (a second source of truth).

### D-BR-2 How compile and LSP find a catalog

- **Answer:** through the Syntax 10 §5.0.3 manifest section `nativeProviders`. A provider entry that names `"catalog": "<path>"` and no `library` is a **host-injected** provider: its compile-time descriptor comes from the catalog, and at run time the host must register a module whose `signatureHash` equals the catalog's. The map key must equal the catalog module name (Syntax 10 §5.0.3 names this diagnostic `native.module_name_mismatch`; it exists only in the design text today, so BR-U4 implements it). BR-U4 implements parsing for these catalog entries; the `library` plugin form is out of BR-U4 scope.
- **Why:** §5.0.3 already designates `nativeProviders` as the one place that declares RegisteredNative providers; LSP and offline compile both read the manifest, so one key serves both.
- **Rejected:** a new top-level manifest key (a second section for the same domain); a CLI-only `--native-catalog` flag (the language server would not see it).

### D-BR-3 Export entry

- **Answer:** an additive binding entry `ZrRustBinding_Runtime_ExportNativeCatalog(runtime, outputPath)` that serialises the runtime's active registrations (BR-D1 §5 step 3), plus a safe Rust wrapper. Exporting from a bare runtime is allowed (no VM global is needed; descriptors live in the runtime registry).
- **Why:** the runtime registry is the only place where all host modules are known before any global exists.
- **Rejected:** exporting from a live VM global (needs `NewStandard` and a project); a CLI subcommand (the CLI cannot see a host's in-process registrations).

### D-BR-4 Step 0 for BR-U4 (verification before code)

Before implementing, BR-U4 must verify how a callback-free catalog descriptor is installed for compile. `ZrLibModuleDescriptor.isContractOnly` is **not** that path: contract validation rejects a contract-only module that publishes constants, functions, types, module links or `onMaterialize` (`native_binding_contract_validation.c:53-62`), and the plugin path refuses contract-only descriptors (`native_binding_registry_plugin.c:798`). A catalog module does publish functions and types, so BR-U4 adds a separate compile-only install path (descriptor visible to type inference, with no callable implementation; a call at run time without a matching live registration fails with a diagnostic) instead of weakening those checks. Step 0 is to confirm where compile-time typing reads registered descriptors and whether a function descriptor without a callback is already tolerated there.

## 4. Unit sequence

| order | unit | scope | tests | depends on |
|---|---|---|---|---|
| 1 | **BR-U1** callback errors (G2, Q-B3) | `native.c` bridge: free `callbackResult`, copy the message into a bridge buffer, then raise (no-return) after the Rust callback has returned; additive message setter or out-param in `zr_vm_rust_binding.h` and `_sys`; `native.rs` forwards `Error.message`, fixed text for panic; docs `zr-vm-rust-binding.md`. | C: failing callback → script `try/catch` sees the message; panic → fixed text (`tests/rust_binding/test_rust_binding_api.c`). Rust: `tests/native_registration.rs`. `ctest -R '^(rust_binding_api|zr_vm_rust_binding_cargo_test)$'` with `BUILD_RUST_BINDING=ON`; an ASan run of the C case. | — |
| 2 | **BR-U3** naming (G3, Q-B6) | registration-time check in `Runtime_RegisterNativeModule`: reject `zr` / `zr.*` / `zr/*` unless in the official inventory; e2e `import("native:<name>")` against a binding module (verify first; fix only if broken). | new cases in both binding test files | — |
| 3 | **BR-U4** catalog (G1, Q-B1 step 1, D-BR-1..4) | export entry, catalog writer, `nativeProviders` catalog-entry reader for compile and LSP, signature-hash check at run time. | offline `Project_Compile` type-checks a `native:` import from a catalog; a mismatched `signatureHash` at run time is rejected with a diagnostic. | BR-U3 (naming rules applied to catalog names) |
| 4 | **BR-U2** allocator (G4, Q-B5) | `NewBareEx` / `NewStandardEx` with a `structSize`-first options struct + allocator; plumb into global creation (verify whether `ZrLibrary_CommonState_CommonGlobalState_New` takes an allocator; extend additively if not). | C and Rust ABI round-trip; a counting allocator sees every VM allocation; old entries unchanged. | — |
| later | weak engine handles (Q-B4) | after the `zr_vm_rust_binding.h:609-610` handle fix | — | handle fix |
| later | proc-macro crate (Q-B1 step 2) | generates builder calls + catalog | — | BR-U4 |

ABI rule for every unit: **additive only**. No existing entry point, struct field or status value changes meaning except the documented Q-B3 behaviour change.

## 5. Not decided here

- Q-J2 (ABI-18 kinds 12–14 provider checklist), capability names for engine modules, and the shared-library `library` provider form of `nativeProviders` are out of scope.
- ZE-side generator design, ECS reflection export and the ZE pin bump itself belong to ZE plans.

## 中文摘要

- **Q-B1** Rust 为源，先做描述符 catalog 导出（BR-U4），之后可选 proc-macro；zr comptime 只消费 catalog。
- **Q-B2** 宿主反射单向投影为 descriptor；跨边界身份是 `native:` 名称，宿主 TypeId 只是不透明元数据。
- **Q-B3** 回调失败默认抛出带宿主消息的脚本错误，panic 用固定文本；必须在 Rust 回调返回后由 C 桥接层抛出，绝不跨 Rust 栈帧 longjmp。
- **Q-B4** v1 每个类型二选一：引擎拥有 + 弱句柄，或值拷贝；`Gc<T>` 和裸 root handle 推迟。
- **Q-B5** 用 `structSize` 首字段的 Ex 入口注入 allocator；每个 runtime 一个 GC domain；v1 不支持 M5。
- **Q-B6** 每个子系统一个 `native:engine.<subsystem>`；只用已有 capability；注册时拒绝非官方的 `zr.*`（BR-U3）。
- **Q-B7** BR-U1 合入后 ZE 一次性升级 pin；最迟约 2026-12-07。
- **本 RFC 新增（BR-U4 范围）**：catalog 为 `zr.native-catalog/1` JSON，复用已有 `ZrLibrary_NativeRegistry_ComputeModuleSignatureHash`；通过 Syntax 10 §5.0.3 的 `nativeProviders` 条目（`catalog` 字段、无 `library`）供编译与 LSP 读取；导出入口 `ZrRustBinding_Runtime_ExportNativeCatalog`；实现前先核实 contract-only 描述符的安装路径。
- **顺序**：BR-U1 → BR-U3 → BR-U4 → BR-U2；所有 ABI 改动只做加法。
