# 2026-07-19 AOT 08-S6AB / 10-S4Z49 / 11-S5G Runtime-Bound Reflection Module

## Scope

This slice creates a runtime-bound `zr.reflection` module object and installs the previously accepted trusted generic
method native entry as its sole public `MakeGenericMethod` export. Global registry/cache and unload policy remain out of
scope.

## Contract

- `ZrCore_Reflection_CreateModuleForRuntime()` accepts only a real module-owned metadata runtime.
- The returned module has `moduleName == fullPath == "zr.reflection"`, the standard path hash, READY initialization
  state, no attached metadata runtime, and exactly one public export.
- `MakeGenericMethod` is an owner-backed native closure bound to the target metadata module. Script-visible arguments
  remain the method-definition object and generic argument array only.
- The target module is temporarily ignored while the service module and names are allocated. The internal closure path
  does not persistently pin it before export installation.
- After `ZrCore_Module_AddPubExport()`, all managed pointers are reloaded from VM stack roots. Only successful export
  lookup and capture verification permit the target module's `NATIVE_HANDLE` pin and the service module's READY state.
- Every failure restores the original stack top and temporary ignore state. The factory does not register or cache the
  returned service module.

## TDD And Review Evidence

- RED: MSVC compiled the expected API and test, then linked with exactly one missing symbol:
  `ZrCore_Reflection_CreateModuleForRuntime`.
- Initial GREEN reached dynamic generic reflection 33/0 on MSVC.
- Independent review found three Important issues: managed locals reused after allocation-capable export installation,
  persistent target pin before fallible module construction, and test root installation after a managed allocation.
- The implementation now reloads roots after export installation, defers persistent pin until all checks pass, and
  performs one cleanup path. The test roots the returned module immediately and reloads it after allocations.
- Follow-up review found no remaining Critical or Important issue.

## Regression Evidence

- Dynamic generic reflection passes 33/0 on MSVC 19.44, GCC 11.4, and Clang 14.0.
- The new `reflection_module.c` and changed generic-method native source emit no GCC/Clang diagnostics.
- Final MSVC metadata/reflection CTest passes 6/6.
- Final MSVC shared regression passes GC 66/0, instruction execution 31/0, and instruction table 95/0.
- Isolated GCC/Clang builds use `HEAD=189262b` plus only this slice's implementation/tests and the known concurrent
  profile definition needed to close the existing HEAD profile-enum gap; profile files are not part of this commit.

## Acceptance Decision

Accepted as 08-S6AB / 10-S4Z49 / 11-S5G. The runtime-bound `zr.reflection` module object and trusted
`MakeGenericMethod` export are closed. Global import registration, per-runtime cache/replacement/unload policy,
`zr.reflection.declaration`, invoke thunks, cross-module method binding, and full-AOT reflection closure remain open.
