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
HEAD: aef6fb571132cf291269f497e6897f3f8a487225
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
0.  These are contract-level results only.  The focused target is registered
as `ssa_release_acceptance`; this record still needs a fresh, final-revision
release matrix before any milestone can be accepted.

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
`aef6fb57`; it reports:

- all 47 leaf plans have their declared implementation and test paths present;
- all 47 manifest CTest names are registered, and the manifest declaration
  check exits zero;
- fresh WSL GCC 11.4 and Clang 14 Debug/static builds with
  `ZR_VM_ENABLE_HOST_JIT=ON` and `ZR_VM_JIT_USE_LLVM=OFF` each run the current
  55-test `-L ssa` matrix with 55/55 passing at `aef6fb57`;
- the current Windows GCC strict standalone release-acceptance fixture exits
  with `ssa release acceptance PASS`; and
- the Windows MSVC Debug/Ninja `-L ssa` run (MSVC 19.44.35228,
  `ZR_VM_ENABLE_HOST_JIT=ON`, `ZR_VM_JIT_USE_LLVM=OFF`) passes 55/55,
  including `ssa_release_acceptance`, `ssa_host_jit_optional`, and the two
  parser tests whose executables were built during the audit; and
- a native MinGW GCC 4.8 CMake subset (`ssa_baseline_metrics`,
  `ssa_contract_freeze`, `ssa_differential_harness`, `ssa_core_model`,
  `ssa_effects_verifier`, and `ssa_release_acceptance`) passes 6/6 after
  `802c3ab8` centralizes legacy GCC thread-local storage.

The checkout still contains unrelated user changes and untracked plan input,
so no dirty-tree digest is claimed here.  An earlier WSL start attempt returned
HCS `0x800705aa`, but the service recovered and the fresh GCC/Clang runs above
were completed in isolated D: build directories.  A whole-tree default build
was not used as release evidence because its unrelated performance runner
stopped on a missing `zr_vm_common` include path.  A wider MSVC-focused build
attempt stopped in the linker with PDB
resource errors (`LNK1140`, `LNK1318`, then `LNK1285`); that historical attempt
is not counted as evidence, while the current 55-test Debug CTest run is
recorded above.  The native MinGW full-core build is also unavailable
because GCC 4.8 has no `stdatomic.h`; this is not substituted for the required
WSL/MSVC matrix.  These results validate repository contracts only and do not
close the release gate below.

## M1 oracle memory stage

The current M1 development slice adds a caller-owned memory provider to the
direct ExecIR oracle.  LOAD now fails closed as
`ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED` when no provider is supplied; provider
failures preserve the instruction/source identity as
`ZR_EXEC_IR_DIAGNOSTIC_ORACLE_MEMORY_ERROR`.  With a provider, STORE then
LOAD replay updates a deterministic semantic fixture and emits ordered
STORE/LOAD events.  This keeps memory state out of host pointers and leaves the
legacy event-only STORE mode available for parser-only observations.

The focused test was first run RED against the pre-stage oracle (LOAD was
unsupported), then GREEN after the provider boundary was implemented:

```text
WSL GCC 11.4   ssa_oracle_projections       1/1 passed
WSL Clang 14   ssa_oracle_projections       1/1 passed
MSVC 19.44     ssa_oracle_projections       1/1 passed
```

These are reference-oracle contract results.  They do not claim production
ExecBC heap migration or close the M1 cross-backend differential gate.

The follow-on projection slice keeps the same memory fixture in the no-
optimization ExecBC/AOT seam.  It verifies that LOAD is projected and that
the copied memory-token pool remains owned by the destination view after the
ExecBC-to-AOT move.  The focused target remained green on WSL GCC 11.4, WSL
Clang 14, and MSVC 19.44; the AOT projection is intentionally still marked
metadata-only until a backend emitter produces runnable code.

The next M1 oracle slice adds a caller-owned, pointer-free allocation
provider. `ALLOC` remains fail-closed without a provider, while a successful
provider returns a deterministic token/value and emits an ordered
`ZR_EXEC_IR_ORACLE_EVENT_ALLOCATE` observation. Provider rejection uses
`ZR_EXEC_IR_DIAGNOSTIC_ORACLE_ALLOCATION_ERROR`; an undefined callback result
is rejected as `ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE`. The callback receives
only bounded oracle operands and must not encode a host pointer. The focused
fixture was first observed RED against the unsupported-ALLOC oracle and then
GREEN in the focused matrix:

```text
WSL GCC 11.4   ssa_oracle_projections       1/1 passed
WSL Clang 14   ssa_oracle_projections       1/1 passed
MSVC 19.44     ssa_oracle_projections       1/1 passed
WSL GCC 11.4   ASan+UBSan ssa_oracle_projections 1/1 passed
```

This remains reference-mode evidence and does not claim production heap
migration.

The projection follow-up now carries `ALLOC` through both no-optimization
views, preserving the opcode, operand/result ranges, and source identity. The
focused fixture asserts ExecBC transport and a non-runnable AOT view; it does
not count metadata transport as executable allocation or GC coverage.

The M0 metrics follow-up also keeps paired conclusions conservative: samples
with different measurement phases or different `availableMetrics` masks are
classified as `INCOMPARABLE` before bootstrap statistics or gate promotion.
The `ssa_baseline_metrics` target passed after the change on WSL GCC 11.4,
WSL Clang 14, and MSVC 19.44.  This remains a focused contract result; the
persistent runner still needs to supply the full phase/counter matrix.

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

The target/CTest registration lives in `tests/cmake/ssa-tests.cmake`.  The
commands remain release-run recipes; their output must be refreshed at the
final revision.

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
