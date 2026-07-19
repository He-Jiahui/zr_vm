# 2026-07-19 AOT 08-S6AC / 10-S4Z50 / 11-S5H Target-Owned Reflection Module Cache

## Scope

This slice adds a target-module-owned cache for the runtime-bound `zr.reflection` service introduced by the previous
sub-milestone. Ordinary import bridging, process-global registration, replacement/unload, and generation policy remain
out of scope.

## Contract

- `ZrCore_Reflection_GetOrCreateModuleForRuntime()` accepts only a real module-owned metadata runtime.
- The target module owns the service through protected export `__zr_reflection_service_module`; the key is absent from
  public exports and the process-global module path cache.
- Repeated calls for one runtime return the same service object. Different target runtimes receive distinct services,
  and identity survives a generational full GC.
- A cache hit must validate the READY `zr.reflection` module identity, standard path hash, null prototype, ready own map,
  exactly one direct public export, module string raw types, cache/export value flags, native entry, closed owner-backed
  capture shape, GC flags, and captured target identity.
- A polluted reserved entry fails closed and remains unchanged. The implementation never silently replaces it.
- New services are validated before protected-cache installation and verified by lookup afterward. The target receives
  a persistent `NATIVE_HANDLE` pin only after successful installation.
- Capture close and module-root escape may unignore captured values. Each construction layer restores its temporary
  ignore before returning; a post-capture cache-install failure preserves caller-owned ignore and the original stack top.

## TDD And Review Evidence

- API RED: MSVC compiled the declaration and test, then linked with exactly one missing symbol:
  `ZrCore_Reflection_GetOrCreateModuleForRuntime`.
- Initial GREEN reached dynamic generic reflection 34/0 on MSVC.
- Independent review found five Important issues: owner-backed capture/export escape could remove temporary or
  caller-owned ignore; an open stack-backed capture could pass validation; non-string module fields could reach string
  payload access; corrupted cache-slot flags could be normalized; and prototype fallback could supply the trusted export.
- Review RED invalidated a fresh target's protected map after capture construction. The suite failed 34/1 because the
  failed get-or-create call did not preserve caller-owned ignore.
- GREEN restores ignore after capture close and in module-factory cleanup. Failure keeps caller ownership; successful
  construction converts the restored protection to `NATIVE_HANDLE` only after cache verification.
- Additional RED/GREEN corruption cases reject an open capture, non-string module name, GC-disabled/native cache slot,
  GC-disabled export slot, and any service prototype. The test restores each corrupted object through VM roots before
  asserting, so later GC coverage observes a valid graph.
- Follow-up independent review reports no remaining Critical or Important findings.

## Regression Evidence

- Dynamic generic reflection passes 34/0 on MSVC 19.44, GCC 11.4, and Clang 14.0.
- Changed reflection sources emit no GCC/Clang warnings in the isolated builds.
- Final MSVC metadata/reflection CTest passes 6/6.
- Final MSVC shared regression passes GC 66/0, instruction execution 31/0, and instruction table 95/0.
- Isolated GCC/Clang builds use `HEAD=2b1c46c` plus only this slice's implementation/tests and the known concurrent
  profile definition needed to close the existing HEAD profile-enum gap; profile files are not part of this commit.
- Frozen isolated builds used a fresh `HEAD=2b1c46c` tree after all review fixes; the WSL build root was transient.

## Acceptance Decision

Accepted as 08-S6AC / 10-S4Z50 / 11-S5H. Target-owned reflection service reuse, runtime isolation, protected cache
validation, GC survival, and failure ownership are closed. Ordinary `import zr.reflection`, global registration,
replacement/unload, generation policy, declaration reflection, invoke thunks, and full-AOT closure remain open.
