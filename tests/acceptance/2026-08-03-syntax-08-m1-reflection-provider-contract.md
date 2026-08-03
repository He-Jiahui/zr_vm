---
scope: Syntax 08 M1 reflection provider and canonical type identity
status: proven
date: 2026-08-03
toolchains:
  - WSL GCC 11.4
  - WSL Clang 14.0
  - MSVC 19.44
---

# Syntax 08 M1 reflection provider contract acceptance

## Reopened defects

The pre-review implementation still had two identity shortcuts: parser type
inference owned a private 20-entry reflection table, and core reflection
import/module/cache paths compared the concrete `zr.reflection` module name.

The first workspace-spoof test initially produced a false positive because its
source path did not match its explicit module. After changing the path to
`zr/reflection.zr`, the source compiled successfully. That RED proved a
workspace could occupy the official ModuleId and receive the registered surface
through its canonical name.

## Implemented contract

- Added common provider roles, canonical TypeRoles, surface flags, and
  projection categories.
- Extended native module descriptors and bumped native plugin ABI 4 to 5.
- Registered contract-only `zr.reflection` and `zr.builtin` TypeRole owners.
- Prevented contract-only descriptors from entering any native materialization
  path or delegating the reserved identity to a host loader.
- Composed and restored pre-installed host loader, provider resolver, and owner
  observer callbacks instead of clobbering embedded-state configuration.
- Validated official ownership, projection completeness/uniqueness, malformed
  storage, and acyclic parent-role graphs.
- Replaced parser-local reflection tables with native-registry role queries.
- Made non-erased reflection category selection consume registered
  `projectionKind` values rather than a second category-to-TypeRole switch.
- Replaced core name comparisons with a registered provider resolver.
- Rejected ordinary source ModuleIds under `zr.*` while retaining official
  import resolution.
- Removed targeted `zr.system.reflect.Type/CallableType` aliases; those names
  now fail through ordinary module resolution.

## Direct RED/GREEN Evidence

- Workspace `module zr.reflection` RED: compiled before reserved-root admission;
  GREEN: rejected with `reserved official module root`.
- Project resolver RED accepted reserved source ModuleId; GREEN is 10/10.
- Provider registry is 9/9, including TypeRole/projection ownership, spoof and
  malformed graph rejection, contract-only non-materialization, and host-loader
  composition/restoration.
- Reflection compile surface is 19/19, including workspace spoof rejection.
- Reflection dynamic/runtime import is 36/36, including missing-resolver failure.

## Cross-Layer Replay

Each toolchain passed the identical 12-executable matrix:

| Suite | Tests |
|---|---:|
| official provider convergence | 9 |
| project import resolver | 10 |
| reflection type surface | 19 |
| reflection dynamic generic/runtime import | 36 |
| type inference | 122 |
| module system | 78 |
| percent syntax cutover | 6 |
| parser | 74 |
| native registry invalidation | 1 |
| native direct call | 5 |
| reflection token resolve | 30 |
| reflection method invoke | 5 |
| **Total per toolchain** | **395** |

GCC, Clang, and MSVC all completed with 0 failures. The strict percent scan also
found zero old percent-keyword literals in production parser/core/library code.
Remaining percent token consumers are rejection diagnostics, `%`/`%=`, and the
internal `intermediate` closure-parameter delimiter.

## Review Result

The independent first review returned NO-GO: a contract-only descriptor could
materialize as an empty competing module, compiler registry attachment replaced
host loaders, category projection still used a private role switch, and parent
cycles were accepted. Each finding now has a direct regression and all four are
closed in the implementation above. The final identical GCC/Clang/MSVC replay
is 395/395.

Syntax 08 as a whole is not promoted: M2-M5 still require their own artifact,
construction, AOT, LSP, corruption, and stress evidence.
