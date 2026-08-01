# 2026-08-01 AOT 07 Parameter Default-Declaration Projection

## Scope

This A7.2J sub-milestone projects a conservative positive default-declaration fact from canonical callee metadata into
the internal ExecIR parameter sidecar and lets the existing exact-arity shared inline-struct `CALL_TYPED` route emit an
audit marker for a defaultable callee parameter. It does not append or evaluate defaults and does not infer whether a
specific callsite omitted an argument.

Projection requires exact metadata/runtime parameter counts and a receiver-free layout. Metadata false, partial
metadata, and receiver-bearing layouts remain unknown because current producers do not provide defaults-complete or
receiver-index contracts. The GC-bearing metadata default value is not copied.

## RED And Review Evidence

- Before production changes, the focused GCC generic typed-call suite reported 16 tests with one missing-marker
  failure, while the unknown negative passed; SemIR contracts reported 10 tests with one missing-schema failure.
- The first implementation called the fact default origin and treated metadata false as known-no-default. Independent
  review established that frontend lowering already materializes omitted and explicit arguments into the same exact
  runtime window, and that false can also mean default metadata was not included. The accepted design therefore records
  only a reliable positive declaration and gives omitted/explicit calls the same marker.
- Follow-up review requested direct coverage for invalid internal `(known=false, declared=true)` state and for
  malformed metadata on an owner removed by stripping. The final suite exercises the consumer/invariant directly on
  Unix, checks the invariant inline on all platforms, and injects a noncanonical bool into an unreachable owner before
  trimming. Final review then expanded the invariant matrix with legal `(known=true, declared=false)` and noncanonical
  declared-bool cases, and added direct Unix ExecIR field assertions for positive, false, partial, and receiver-bearing
  projections.

## Coverage Inventory

- Projects `(defaultDeclarationKnown=true, hasDeclaredDefault=true)` only from a canonical positive metadata row.
- Leaves false, partial, and receiver-bearing metadata projections unknown.
- Directly inspects both projected fields for positive, false, partial, and receiver-bearing ExecIR rows on Unix.
- Rejects noncanonical `hasDefaultValue` before code stripping, including unreachable owners.
- Does not copy the GC-bearing metadata `defaultValue` into ExecIR.
- Emits `zr_aot_generic_call_typed_callee_defaultable_parameter_full_arity` only inside the existing exact-count shared
  route with a complete caller argument window.
- Gives omitted and explicit calls the same declaration marker and runtime argument count.
- Fails closed to the ordinary inline-struct route for an invalid internal sidecar invariant.
- Preserves runtime, dictionary, public function, artifact, manifest, and reachability schemas.

## Tooling Evidence

Frozen effective source is committed HEAD `e710a3b87ef61f82d059ca54fda45fe29e58bb92` plus the exact A7.2J six-file
production/test overlay:

- WSL source: `/home/hejiahui/codex-validation/zr_vm-aot12-20260801-a72j-e710a3b-r1`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260801-a72j-e710a3b-r1`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228.0 x64 Debug

All six implementation/test files match across main, WSL, and Windows:

- `a2ba579fdaaefc11f3dc729728765bd8fc166e53d920229c89bfca64f509f723`
- `351f27d765f0f345b8d990abaf418a79fc7e12f4280fb6f9c8bf90d020e510ce`
- `959d5a0c72a0e99e2e04755f31230539f31f57a53a1b08025b2aa81bab9cffe4`
- `5d1f4b9a2073ebf545f9992b335f5cc056e770027e213de6b49083118dc73036`
- `ed6afeafee54212d4a5c44814d28a3cc1925ac45ecba5430f8c9810251c4616f`
- `c2ae7ce08b87a428a2760d90ae95f1861fd8fed6878e8ee8d32d12dcc0352801`

## Results

- WSL GCC: generic typed-call 19/0; SemIR 10/0; MethodInfo 3/0; code stripping 37/0; generic sharing 9/0;
  debug metadata 6/0; value-SemIR 8/0; typed-call contracts 4/0.
- WSL Clang: the same eight groups pass with counts 19/0, 10/0, 3/0, 37/0, 9/0, 6/0, 8/0, and 4/0.
- Windows MSVC x64 Debug: all eight targets build and pass; generic typed-call reports 19 total, zero failures, and four
  expected Unix-only ignores. Existing temporary-directory MSB8029 warnings remain non-blocking.
- The fresh baseline includes the Syntax typed-metadata and strict-legacy cutover commit after A7.2I; all three
  toolchains were configured and built from clean current-HEAD source trees rather than reusing the earlier A7.2J cache.

## Acceptance Decision

Accepted at `2026-08-01 11:36:53 +08:00` as AOT 07 A7.2J's conservative positive parameter
default-declaration projection and exact-arity typed-call audit consumption. Callsite default origin, A7.2, AOT 07,
AOT 12, and the broader AOT 07-12 goal remain active.
