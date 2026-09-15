---
doc_type: acceptance-record
plan: docs/plans/ssa/11-tooling-acceptance/03-release-acceptance.md
matrix: tests/acceptance/ssa-requirement-matrix.md
manifest: tests/cmake/ssa-coverage-manifest.cmake
focused_test: tests/core/test_ssa_release_acceptance.c
status: open
---

# SSA milestone acceptance record

This record is deliberately an open gate until the complete matrix is run.
It separates a focused contract pass from a release decision and preserves
missing-tool/backend/platform evidence instead of counting it as success.

## Snapshot and focused contract

The focused test contains a synthetic complete manifest plus failure-injection
fixtures.  It does not read the host's CTest database or manufacture current
coverage numbers.  The source revision observed while authoring this record
was:

```text
HEAD: b2b726eed491f5fe358914b8ca9382c33ccf2586
```

The checkout was concurrently dirty, so the value above is an authoring
snapshot only.  Before release use, regenerate a complete dirty-tree digest
and write it into the manifest evidence.  A digest of only `git diff` or only
tracked files is insufficient when untracked generated/test files exist.

Focused commands run from the WSL-mounted checkout:

```bash
cd /mnt/e/Git/zr_vm
gcc -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes \
    -Wmissing-prototypes -Werror -Izr_vm_common/include \
    tests/core/test_ssa_release_acceptance.c \
    -o /tmp/ssa_release_acceptance_gcc
/tmp/ssa_release_acceptance_gcc

clang -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes \
      -Wmissing-prototypes -Werror -Izr_vm_common/include \
      tests/core/test_ssa_release_acceptance.c \
      -o /tmp/ssa_release_acceptance_clang
/tmp/ssa_release_acceptance_clang

cmake -P tests/cmake/ssa-coverage-manifest.cmake
```

Observed result at authoring time: both focused executables printed
`ssa release acceptance PASS` and exited 0; the CMake declaration check exited
0.  These are contract-level results only.  The parent integration still has
to register and run the named CTest target and refresh this record at the
final revision.

The focused failure fixtures prove that:

- removing one positive/boundary/negative case is a structural invalidity;
- a `focused-passed` row cannot make an `accepted` milestone;
- a sanitizer failure or missing backend/platform row keeps coverage open;
- a missing or incomparable performance sample cannot support a performance
  claim; and
- a non-zero production legacy consumer blocks the removal claim.

## Current implementation audit (not release evidence)

The repository-local implementation audit was refreshed after the SSA leaf
commits.  The code snapshot audited immediately before this record update was
`06103f644c0e02962d31db87bcbc46f17152bc29`; it reports:

- all 47 leaf plans have their declared implementation and test paths present;
- all 47 manifest CTest names are registered, and the manifest declaration
  check exits zero;
- the WSL GCC Debug/static build with `ZR_VM_ENABLE_HOST_JIT=ON` and
  `ZR_VM_JIT_USE_LLVM=OFF` last ran 51 `^ssa_` tests with 51/51 passing at
  `210aa19b` (the subsequent `06103f64` change is whitespace-only); and
- the current Windows GCC strict standalone release-acceptance fixture exits
  with `ssa release acceptance PASS`.

The checkout still contains unrelated user changes and untracked plan input,
so no dirty-tree digest is claimed here.  A fresh WSL Clang rebuild could not
be started during this audit because the host WSL service returned HCS
`0x800705aa`; the older Clang CTest database is not counted as current
evidence.  These results validate repository contracts only and do not close
the release gate below.

## Milestone state

`accepted` means every in-scope requirement is accepted, every prerequisite is
accepted, every required backend/platform/sanitizer row has actual evidence,
and the artifact/documentation/legacy checks are complete.  `focused-passed`
means only the owning focused test passed.  `unavailable` and `known-failure`
remain open states.

| Milestone | Scope | Required release evidence | Current state | Blocking evidence / next action |
| --- | --- | --- | --- | --- |
| M0 | measurement, contract freeze, differential harness, build/cache profile | paired samples, current fingerprint, explicit CTest denominator | open | run full measurement and tool matrix |
| M1 | canonical ExecIR, verifier, state maps, oracle projections | CFG/effect/ownership/exception differential across ExecBC/AOT | open | run all 01.* rows and backend parity |
| M2 | frame/native, layout, GC, domain and async contracts | root/lease/GC sanitizer and worker evidence | open | run frame-safe, sanitizer, and domain rows |
| M3 | interpreter boundaries, binding facts, guards, generated fusion | static-site and fallback/deopt evidence | open | run every 03.* row and dispatch regression |
| M4 | AOTIR, C/LLVM lowering, generic policy, AOT runner | actual native coverage and fallback counts; no missing backend | open | run AOT runner representative set |
| M5 | artifact schema, capabilities, generation, rollback | pointer scan, capability escalation, stale generation, rollback | open | run all artifact/hotpatch attack cases |
| M6 | scalar/range/escape/inlining/loop optimization and remarks | verifier/differential evidence plus CLI/LSP source identity | open | finish 02.* and 11.02 adapter rows |
| M7 | SIMD/batch, backend service/JIT/platform matrix, release closeout | strict numeric parity, host JIT and actual mobile/WASM provenance | open | run 09.*, 10.*, and full release matrix |

No row is marked accepted merely because a sibling focused document exists.
The matrix's `Current state` column is intentionally `open` until the parent
integration writes actual command output, revision, and environment for each
row.

## Required command matrix

The following commands are recipes for the release run.  Their output must be
captured with exit status and test count; a command matching zero tests is a
failure, not an empty pass.

### Build and CTest

```bash
cmake -S . -B build/ssa-gcc-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc \
  -DBUILD_STATIC_LIB=ON -DBUILD_SHARED_LIB=OFF
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_release_acceptance_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_release_acceptance$' \
      --output-on-failure --no-tests=error

cmake -S . -B build/ssa-clang-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
  -DBUILD_STATIC_LIB=ON -DBUILD_SHARED_LIB=OFF
cmake --build build/ssa-clang-debug --target zr_vm_ssa_release_acceptance_test -j 4
ctest --test-dir build/ssa-clang-debug -R '^ssa_release_acceptance$' \
      --output-on-failure --no-tests=error

# Existing SSA and legacy regression denominator; preserve the complete list.
ctest --test-dir build/ssa-gcc-debug -N -L ssa
ctest --test-dir build/ssa-gcc-debug -L ssa --output-on-failure --no-tests=error
```

The target/CTest registration is owned by the parent integration task.  This
leaf does not edit the shared CMake file.

### Toolchains and sanitizers

Record separate runs for WSL GCC, WSL Clang, and Windows MSVC (`--config
Debug` for the multi-configuration generator).  Build ASan+UBSan, LSan, and
TSan in separate build directories; run Valgrind Memcheck and Helgrind on a
non-sanitized Debug binary.  Each record must contain compiler version,
configuration, selected tests, total/passed/failed/unavailable counts, and
the first failure's diagnostic.  A missing tool is `unavailable`, never
`passed`.

### Platforms and backends

For ExecBC, AOT-C, AOT-LLVM, and host JIT, record both requested and actual
backend.  For Android, iOS, and WASM, a cross-compile or static inspection is
not runtime execution; the row remains open until a device/emulator/browser
runner supplies execution and semantic evidence.  Mobile/WASM machine-code
JIT is forbidden by the platform contract.

## Performance and legacy gates

No performance claim is made in this record.  Release performance evidence
must use the independent Release directory and paired same-environment
samples.  A candidate sample with a different checksum/environment, a zero
sample count, missing PMU/RSS data, a crash, or an interpreter fallback is
invalid for a pure-AOT claim.  A relative improvement is only reported after
the quality/variance gate and the 3% threshold from the measurement contract;
wall-clock values from Debug or sanitizer builds cannot be substituted.

Before declaring old-path removal, run a source/build consumer scan for the
legacy SemIR projection and AOT opcode decoder.  The scan must report zero
production consumers and preserve any test/compatibility-only consumers in
the inventory.  The focused validator rejects a non-zero count.

## Decision

Current decision: **open**.  The release gate has a deterministic schema,
denominator, and focused negative tests, but the complete CTest, sanitizer,
platform, performance, and legacy-consumer evidence is intentionally not
claimed here.  Update this record and the matrix only from a fresh manifest
bound to the final commit and environment.
