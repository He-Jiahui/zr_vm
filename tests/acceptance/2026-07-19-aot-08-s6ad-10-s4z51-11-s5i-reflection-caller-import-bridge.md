# 2026-07-19 AOT 08-S6AD / 10-S4Z51 / 11-S5I Reflection Caller Import Bridge

## Scope

This sub-milestone connects ordinary and guard imports of the exact `zr.reflection` path to the runtime-bound service
owned by the actual caller's loaded metadata module. Process-global native registration, replacement/unload, module
generation, declaration reflection, and additional reflection APIs remain out of scope.

## Contract

- Exact byte-length path matching rejects prefixes, suffixes, and embedded-NUL variants.
- Caller functions are normalized through a forwarding-aware owner chain with a finite depth limit.
- Loaded registry entries must have canonical string/module value shapes. Malformed pairs, chain/count inconsistencies,
  owner cycles, and multiple distinct modules matching one caller root fail closed.
- Multiple path aliases for the same module are accepted without creating ambiguity.
- A matching module must be READY and own a metadata runtime with code registration. The bridge delegates service
  identity and lifetime to `ZrCore_Reflection_GetOrCreateModuleForRuntime()`.
- The service never enters the process-global path cache. Same-runtime imports reuse one service; different runtimes
  receive distinct services.
- After bridge resolution, the loader restores its stack anchor and reacquires path/caller before diagnostics and the
  existing import signature verifier.

## TDD And Review Evidence

- Initial RED: the new caller import contract failed 35/1 because ordinary import had no global `zr.reflection` module.
- Review RED/GREEN added embedded-NUL rejection, malformed registry values, pair-count overflow, registry self-cycle,
  owner self-cycle/two-node cycle, distinct-module ambiguity, and native string-key flag corruption.
- Loader-refresh mutation RED removed the post-bridge reacquisition and failed 35/1 because the stale caller bypassed a
  post-GC signature effect. Key-validation mutation RED removed both native-string checks and failed at the corruption
  assertion.
- OOM injection forces a full GC during service construction. Metadata roots are GC-managed; the replacement caller is
  held in a VM stack slot and recovered through `SZrFunctionStackAnchor` from the current main-thread state.
- Independent review first found four Important production issues and two fixture/key-shape issues. All were closed;
  final static review reports no Critical or Important findings.

## Regression Evidence

- Frozen source: `HEAD=a3edc21` plus this sub-milestone's exact code/test overlays and the known concurrent profile
  definition overlay required by that HEAD.
- Dynamic generic reflection passes 35/0 on MSVC 19.44, GCC 11.4, and Clang 14.0.
- Changed reflection bridge sources/tests emit no GCC or Clang warnings.
- MSVC metadata/reflection CTest passes 6/6.
- MSVC shared regression passes GC 66/0, instruction execution 31/0, and instruction table 95/0.

## Acceptance Decision

Accepted as 08-S6AD / 10-S4Z51 / 11-S5I. Exact caller-context `zr.reflection` import, service reuse/isolation,
registry fail-closed behavior, post-GC caller refresh, signature verification, and global-cache isolation are closed.
Global registration, replacement/unload, generation policy, declaration reflection, create/invoke APIs, and full-AOT
closure remain open.
