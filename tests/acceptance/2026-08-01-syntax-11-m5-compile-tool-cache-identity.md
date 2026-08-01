# Syntax 11 M5 CompileTool cache identity acceptance

Date: 2026-08-01

Scope: the in-process comptime cache identity boundary for phase-typed
CompileTool bindings. This record does not claim external CompileTool sandbox
loading, persistent incremental cache integration, or full Gate 11 M5
promotion.

## Contract

- Cache schema `zr.comptime.cache/v2` retains the current module, function,
  comptime context, source range, and typed arguments.
- Every active lexical CompileTool binding contributes its alias, binding kind,
  provider module, provider phase, published public-contract hash, computed
  contract hash, and optional provider artifact content hash.
- Text inputs are mixed with presence and length markers, so adjacent fields
  cannot alias through concatenation.
- Shadow bindings participate without dereferencing a provider. Provider
  bindings with a changed public contract or changed content hash cannot reuse
  an earlier result.
- The content hash is borrowed immutable provider storage and must outlive the
  compiler state. The existing builtin declaration API supplies no artifact
  hash; an external CompileTool loader must use the explicit content-hash API.

## TDD evidence

1. The public-contract RED used the same function, module, source range, and
   alias with provider variants that changed only the published hash or only
   the computed hash. The old key implementation returned the same key and
   failed the new identity assertion.
2. The artifact-content RED failed to compile and link because no binding API
   could carry a provider content hash.
3. GREEN introduced the explicit content-hash binding contract and cache
   schema v2. The existing pure-function behavior remains one hit and two
   misses, while published, computed, and content identity changes each produce
   distinct nonzero keys.

## Focused validation

The same `zr_vm_comptime_runtime_contract_test` executable passed 11 tests with
zero failures under GCC 11.4, Clang 14, and MSVC 19.44.35228. A separate MSVC
`/Od /fsanitize=address` build also passed 11/11 with no sanitizer report.

## Remaining M5 work

- resolve and load manifest `buildDependencies` inside the compiler sandbox;
- pass the resolved artifact content hash through the explicit binding API;
- integrate the identity with the persistent incremental cache and prove clean
  and incremental outputs are byte-identical;
- prove runtime graphs cannot observe build dependency exports;
- complete formatter and remaining cross-consumer acceptance.
