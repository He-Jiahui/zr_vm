---
scope:
  - Syntax 07B current-reference promotion
  - syntax_reference_v1 coverage manifest and project execution
  - owner-gate consumer evidence aggregation
status: accepted
last_verified: 2026-08-05
---

# Syntax 07B current reference acceptance

## Decision

`syntax_reference_v1` is promoted from a discovery skeleton to the current
reference catalog and runnable project. Its 29 stable feature ids contain 28
current entries and one intentional negative migration entry. The current
collection contains 25 source/support files, the negative collection contains
two files, and the design-pending collection is empty.

The previous skeleton was not actually runnable: `main.zr` called a function
from `host.zr` without importing the module, the manifest declared a library,
and the entry module had no module-level return. A direct CLI check failed on
the missing symbol. The promoted fixture is an application, imports `host`,
exports the host function, and returns checksum 7 from its entry module.

## Direct evidence

- `zr_vm_syntax_reference_v1_test`: 13/13 on GCC, Clang, and MSVC. It validates
  stable ids, disjoint collections, current syntax evidence and parsing, zero
  pending owners, provider identity, the real native descriptor, application
  entry shape, formatted/minified AST and semantic hashes, and focused compiler
  contracts.
- GCC CLI compile: two reachable modules compiled and emitted `.zro` and `.zri`
  artifacts.
- GCC interpreter execution: result `7`, `executed_via=interp`.
- GCC binary-first execution: result `7`, `executed_via=binary`.
- GCC AOT C emission: generated `main.c` and `host.c` from the same binary
  inputs.
- Complete projects CTest: 44 cases passed, including permanent
  `syntax_reference_v1_interp` and `syntax_reference_v1_binary` cases.

The public CLI intentionally rejects `aot_c` and `aot_llvm` execution modes;
07B therefore does not restore removed CLI modes. AOT C/LLVM equivalence is
validated at the frozen owner-contract layer used by the reference manifest:
reflection, pooling, native FFI, compile-time generation, task/iterator/test
artifacts, canonical callable ABI, and trimming/loader tests. Those owner gates
were independently promoted before this acceptance, and the reference fixture
only changes an id to current after that evidence exists.

## Consumer mapping

- Reflection and dynamic construction: Syntax 08 M1-M5 acceptance and commits
  `c15c746` / `bf6cef9`.
- Pooling handle/ref, compact-safe slabs, and performance: Syntax 09 promotion
  matrix.
- Native module/FFI/artifact contracts: Syntax 10F and 10C, including commit
  `17366d5`.
- Typed compile-tool generation: Syntax 11, commit `2889b2f`.
- Task/Job/Scheduler and iterator carriers: Syntax 12/13 owner matrices and
  canonical descriptors.
- TestManifest, CLI runner, LSP, and Debug consumers: Syntax 14, commit
  `8543671`.
- Repository cutover and migration-only legacy spelling: Syntax 06B acceptance
  above.

## Review conclusion

There is no remaining `design-pending` entry or unowned current source slot.
The reference project now proves real module import, artifact generation, and
interpreter/binary checksum parity; feature-specific VM/AOT/artifact/LSP
behavior remains enforced by the owner tests rather than duplicated as a
second implementation inside the fixture.
