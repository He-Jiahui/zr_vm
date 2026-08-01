# Syntax 11 M5 CompileTool cache identity acceptance

Date: 2026-08-01

Scope: the in-process comptime cache identity boundary for phase-typed
CompileTool bindings. This record does not claim external CompileTool sandbox
loading, persistent incremental cache integration, or full Gate 11 M5
promotion.

## Contract

- Cache schema `zr.comptime.cache/v4` retains the current module, function,
  comptime context, source path/range, and typed arguments.
- Every active lexical CompileTool binding contributes its alias, binding kind,
  provider module, provider phase, published public-contract hash, computed
  contract hash, and optional provider artifact content hash. A resolved
  external binding additionally contributes package/module identity, source
  kind, resolved version, package hash, canonical CompileTool lock-section
  hash, artifact entry, actual entry hash, and public contract.
- Text inputs use presence and big-endian length markers. Enums, positions,
  numeric typed values, counts, and booleans use explicit big-endian `u64`
  encodings rather than host `sizeof` bytes.
- The builder streams canonical fields into SHA-256. Cache entries retain and
  compare the complete 32-byte digest, so a 64-bit FNV collision cannot return
  another input's result.
- Shadow bindings participate without dereferencing a provider. Provider
  bindings with a changed public contract or changed content hash cannot reuse
  an earlier result.
- The content hash is borrowed immutable provider storage and must outlive the
  compiler state. The compiler-owned artifact resolver now supplies the actual
  selected-entry hash and the resolved identity record through the dedicated
  resolved-provider API; builtin declarations still have no artifact hash.

## TDD evidence

1. The public-contract RED used the same function, module, source range, and
   alias with provider variants that changed only the published hash or only
   the computed hash. The old key implementation returned the same key and
   failed the new identity assertion.
2. The artifact-content RED failed to compile and link because no binding API
   could carry a provider content hash.
3. GREEN introduced the explicit content-hash binding contract and cache
   schema v2. The resolved-artifact handoff first advanced the schema to v3.
   The existing pure-function behavior remains one hit and two
   misses, while published, computed, and content identity changes each produce
   distinct nonzero keys.
4. Independent review found that v3 compressed all strong identities into an
   unchecked 64-bit FNV key and mixed host enum/structure bytes. The review RED
   also showed two different transitive lock assertions reusing one key. Cache
   v4 replaces that path with canonical SHA-256 and full-digest lookup; the
   resolver contributes a sorted hash of the complete CompileTool lock section.

## Focused validation

The same `zr_vm_comptime_runtime_contract_test` executable passed 13 tests with
zero failures under isolated GCC 11.4, Clang 14, and MSVC 19.44.35228 builds.
The earlier 11-case MSVC AddressSanitizer replay predates cache v4 and is not
used as evidence for the v4 promotion.

## Remaining M5 work

- activate the resolved external provider through the ordinary compile-only
  import and transform execution path;
- integrate the identity with the persistent incremental cache and prove clean
  and incremental outputs are byte-identical;
- prove runtime graphs cannot observe build dependency exports;
- complete formatter and remaining cross-consumer acceptance.
