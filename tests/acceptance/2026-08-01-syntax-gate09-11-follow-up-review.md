---
scope:
  - Syntax strict parser cutover
  - Syntax 09 callable pool/native reference contracts
  - Syntax 11 typed generation follow-up
status: accepted-in-scope
last_verified: 2026-08-01
---

# Syntax Gate 09/11 follow-up acceptance and review

## Decision

The original 55 milestone status records remain complete in their declared
scope. The separate M5 task-level support record is also complete. The strict
one-time parser cutover remains accepted: removed percent spellings are
recognized only to produce `legacy_syntax_removed` and no legacy AST.

This follow-up accepts the implemented Gate 09 callable source path and Gate 11
typed Patch/build-identity work in their documented scope. It does not promote
the root Syntax redesign, Gate 09 M2-M5, or Gate 11 M5 as a whole.

## Review fixes

- Published runtime-only/readonly native field facts and readonly/writable
  property-reference contracts, then bumped the native plugin descriptor ABI
  from v3 to v4. The runtime ABI remains v3.
- Routed native `out`/`ref` arguments through managed property references so a
  successful `tryRead`/`tryBorrow` call writes its guard view back to source.
- Preserved object identity when copying canonical `REF_LIKE` structs while
  retaining value-copy behavior for ordinary structs.
- Executed `Pool<T>` deliver, validation, recycle and guarded access through
  production native descriptors, with source-level readonly/writable property
  behavior and close/finalizer release.
- Kept Patch mutation atomic across generated fields, interfaces and attribute
  metadata, retained generated source maps, separated build dependencies in the
  v2 manifest/lock graph, and included CompileTool public/content identity in
  comptime cache keys.
- Closed a Windows-only review finding: newly tested internal parser entry
  points lacked `ZR_PARSER_API`, so MSVC could compile the DLL but not link the
  acceptance executables. The declarations now export consistently.
- Closed a sanitizer review finding in the shared member PIC write barrier:
  nullable cached receiver/callable targets were passed through a macro that
  dereferenced its operand before the barrier's null check. The barrier now
  accepts the typed pointer and performs a null-safe raw-address conversion.

No remaining P1/P2 correctness issue was found in the reviewed scope.

## Fresh matrix

The same 11 executables passed under WSL GCC, WSL Clang, and Windows MSVC:

| Executable | Tests |
|---|---:|
| `zr_vm_generational_pool_test` | 13 |
| `zr_vm_generational_pool_gc_stress_test` | 3 |
| `zr_vm_generational_pool_artifact_test` | 3 |
| `zr_vm_value_copy_fast_paths_test` | 7 |
| `zr_vm_percent_syntax_cutover_test` | 6 |
| `zr_vm_reflection_type_surface_test` | 18 |
| `zr_vm_reflection_type_stress_test` | 3 |
| `zr_vm_compile_time_test` | 69 |
| `zr_vm_comptime_contract_test` | 2 |
| `zr_vm_comptime_runtime_contract_test` | 11 |
| `zr_vm_project_manifest_v2_test` | 9 |
| **Per toolchain** | **144** |

All three toolchains completed 144/144 with real process exit 0. GCC and Clang
also rebuilt and reran the 69-test compile-time and 11-test runtime-contract
targets after the Windows export fix. `git diff --check` reports no whitespace
errors; line-ending conversion notices are repository configuration warnings.

An isolated GCC AddressSanitizer + UndefinedBehaviorSanitizer build passed the
Pool 13/13, GC stress 3/3, value-copy 7/7, compile-time 69/69, and comptime
runtime-contract 11/11 executables, for 103/103 with leak-only detection
disabled. LeakSanitizer itself reports the existing global registry/meta-method
shutdown leak rooted in `global_state_init_basic_type_object_prototypes`; that
baseline is recorded rather than misreported as green. No out-of-bounds,
use-after-free, double-free, or undefined-behavior finding remains in the
focused paths.

## Remaining boundary

Gate 09 still needs the complete ref-view legality/replacement matrix,
TypeLayout-driven production GC scan, exactly-once resource Drop and early-exit
proof, managed moving-slab compaction, dedicated LSP projection, and the final
pause/allocation promotion matrix. Gate 11 M5 still needs sandbox/content-hash
handoff, persistent incremental cache, formatter projection, and remaining
consumer acceptance. The syntax-reference manifest retains 13 `design-pending`
entries, so 07B and the root promotion remain open.
