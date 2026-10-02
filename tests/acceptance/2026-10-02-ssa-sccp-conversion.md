---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc.c
  - tests/parser/test_ssa_sccp_conversion.c
  - tests/cmake/ssa-sccp-conversion-tests.cmake
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
  - "user: 2026-10-02 继续 SSA 计划，验证后按子任务提交"
tests:
  - tests/parser/test_ssa_sccp_conversion.c
  - tests/cmake/ssa-sccp-conversion-tests.cmake
doc_type: acceptance
status: scoped-accepted
---

# SCCP conversion preservation

## Scope and observed defect

This is a bounded 02.01 correctness slice, not acceptance of the entire scalar
optimizer. The only production change is the `CONVERT` lattice transfer in
`exec_ir_sccp.c`. Existing dirty scalar tests and `ssa-tests.cmake` were not
modified. The new test has a separate registration module and one include in
the previously clean `tests/CMakeLists.txt`.

The parser producer maps semantic conversion to `CONVERT` and can carry an
explicit `scalarConversionTypeToken` separately from its result type identity.
Oracle and ExecBC both implement numeric conversion and prefer that explicit
target when present. SCCP formerly grouped `CONVERT` with `COPY` and `MOVE`,
copied constant bits unchanged, and retagged them with the result type. For
an immediate signed integer `7` converted to double, SCCP then emitted a
`CONSTANT 7`: the optimized Oracle returned a signed value, whereas the
unoptimized Oracle and ExecBC returned floating `7.0`.

The corrected transfer adds a token-level eligibility filter: constant bits
pass through only if source and result tokens are the same nonzero identity
and any explicit instruction target agrees with the result token. This is not
a proof that every matching token has the same runtime representation.
Unknown sources remain unknown. Cross-type constants,
unspecified types, and explicit target mismatches become overdefined and retain
their executable conversion. No host numeric cast or new constant encoding is
introduced. Same-type integer immediate folding and pool bit patterns remain
supported for the representations covered below.

## Actual test coverage

`ssa_sccp_conversion` / `zr_vm_ssa_sccp_conversion_test` uses always-active
`CHECK` calls, including production calls, and a 120-second CTest timeout. The
fixture uses published identity `id=1`, `functionToken=7`, and signature hash
`99`, matching the existing scalar test convention. Pipeline failures print
their diagnostic code, function, block, instruction, source, and expected/actual
fields before exiting. Verifier checks are never disabled.

| Case | Assertions |
| --- | --- |
| INT64 immediate to DOUBLE, explicit target | Before/after Oracle and ExecBC return floating `7.0`; conversion remains, lattice overdefined |
| Same conversion, implicit target | Result type fallback preserves the same behavior |
| DOUBLE pool `-7.75` to INT64 | Both runners truncate to signed `-7`; lattice overdefined |
| INT64 pool `7` to BOOL | Both runners return boolean `true`; lattice overdefined |
| DOUBLE pool identity | Both runners preserve `-7.75`; constant bits/type preserved |
| INT64 pool identity | Both runners preserve signed `7`; constant bits/type preserved |
| INT64 immediate identity | Conversion folds to a constant and both runners return signed `7` |
| INT64 → DOUBLE → INT64 | `9007199254740993` rounds to `9007199254740992` in both runners; both converted values overdefined |
| Unspecified source/result type | Both pipeline verifier boundaries accept the fixture; conversion retained, lattice overdefined; no unsupported backend execution is claimed |
| Explicit DOUBLE target with INT64 result annotation | Explicit target takes precedence in both runners; conversion retained, lattice overdefined |

Every numeric fixture runs both the reference Oracle and ExecBC before and
after the scalar SCCP pass. The unspecified-type case checks the verified
function API and lattice only.

## Environment, commands, and results

Repository: `E:\Git\zr_vm`, checked-out `main`; no worktree or branch was
created. All agent build outputs, compiler temporaries, scripts, and logs are
under `D:\tmp\zr_vm\ssa-20261002-01a0fc3b\sccp-convert` (WSL
`/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/sccp-convert`). Root's authorized formal
MSVC build is under the sibling `matrix/msvc`; its captured command logs are
under `control`.

Linux tools: GCC 11.4.0, Clang 14.0.0, CMake 3.22.1, kernel
`6.18.33.2-microsoft-standard-WSL2`. WSL is invoked with `-e` and the fixed PATH
`/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin`. Native build logs
show MSVC toolset `14.44.35207` and Windows SDK `10.0.26100.0`.

Formal native commands, executed by root:

```text
cmake --build D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc --target zr_vm_ssa_sccp_conversion_test -j 4
ctest --test-dir D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc -R ^ssa_sccp_conversion$ --output-on-failure -j 1
```

| Gate | Actual result | Log |
| --- | --- | --- |
| Native original-code RED, corrected fixture | Build 0; CTest 8, one matching test failed at `oracle.returned && oracle.returnValue.kind == kind`, 1.33 seconds | `control/sccp-red-fixture-v3-ctest.log` |
| GCC original-code RED, corrected fixture | Both original build and incremental rebuild succeeded; one matching CTest failed at the same check, 0.11 seconds; WSL wrapper exit 1 (child CTest exit not separately recorded) | `sccp-convert/focus-gcc-red-build.log`, `focus-gcc-red-rebuild.log`, `focus-gcc-red-ctest.log` |
| Native final-code GREEN, ten cases | Build 0; CTest 0, 1/1 matching test passed, 1.36 seconds | `control/sccp-green-build.log`, `control/sccp-green-ctest.log` |
| GCC final-code gold validation | Latest test and SCCP compilation 0; gold link timed out after 180 seconds with exit 124; no final-code test execution | `sccp-convert/gold-gcc/results.json`, `link.log` |
| Clang shared ASan/UBSan final-code gold validation | Latest test and SCCP compilation 0; gold link timed out after 180 seconds with exit 124; no test execution or sanitizer-clean claim | `sccp-convert/gold-clangasan/results.json`, `link.log` |

Linux focused builds use the production source manifest from the new test
registration module, omitting the full core library and using function/data
sections with `--gc-sections`. Each original target compiled 34 translation
units. GCC and ordinary Clang original builds exited 0. The final bounded
drivers reuse their own 32 supporting objects, recompile only the latest test
and SCCP source, and make at most one gold link attempt per compiler. Every
compile/link has a 180-second timeout, and every execution a 120-second timeout.
The direct executable run exercises the same ten-case test source as formal
CTest. `object-receipt.json` records source/object hashes and original compiler
arguments; `results.json` records actual argv, cwd, return code, and elapsed time.

```text
wsl -e env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin python3 /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/sccp-convert/gold-validate.py gcc
wsl -e env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin python3 /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/sccp-convert/gold-validate.py clangasan
```

GCC uses Debug, non-sanitized flags `-g -UNDEBUG -ffunction-sections
-fdata-sections -std=gnu11`. Clang retains `-fsanitize=address,undefined
-shared-libasan -fno-omit-frame-pointer`, with
`LD_LIBRARY_PATH=/usr/lib/llvm-14/lib/clang/14.0.0/lib/linux`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Gold adds
`-fuse-ld=gold -Wl,--gc-sections` without changing repository build policy.

## Earlier non-acceptance attempts and remaining limits

The first native run failed inside the pipeline with an unpublished zero-ID
fixture, before any conversion assertion. It is not counted as the conversion
RED. `control/sccp-red-ctest-v2.log` preserves that exit-8 result. The fixture
identity was corrected and diagnostics added before taking the real RED.

A full-repository GCC configure was stopped while waiting on DrvFS; an early
build before its cache was ready failed with `could not load cache`. Neither is
test evidence. Both are retained in `gcc-configure.log` and `gcc-red-build.log`.
A static Clang sanitizer configure was also stopped during linking and replaced
by the shared ASan configuration; its log is `focus-sanitizer-configure.log`.
Shared-ASan's completed 34-unit build later waited in `/usr/bin/ld` at
`p9_client_rpc`. Before stopping that owned link, the agent recorded PID 55908,
start-time stat, complete argv, and exact matching build cwd in
`link-state-before-gold.json`. The original shared-ASan build exits 1 with an
explicit terminated-link diagnostic in `focus-shared-sanitizer-build.log`.
No foreign process or cache was changed. These interrupted attempts are not
treated as passing gates or semantic failures.

Both authorized gold attempts timed out with exit 124. Their owned process
groups were stopped at the configured limit. The distinct native-linker
fallback below subsequently establishes a Clang sanitizer execution gate;
final-code GCC execution remains unverified. No full GCC/Clang matrix is claimed.

## Bounded native ELF linker fallback and root gates

The installed Windows `ld.lld` 19.1.5 linked the existing Linux Clang 14
instrumented objects without rebuilding or removing sanitizer instrumentation.
All generated inputs, binaries, temporary files and receipts remain on D.
Linux system libraries were read through WSL UNC paths. The first ELF64
relocatable probe returned 0. A GCC `-###` capture hit its 30-second watchdog;
the single Clang capture returned 0 and retained the exact ELF linker argv.

The first complete native link returned 1 because GNU `libc.so` contains
absolute `/lib` paths that the UNC sysroot cannot resolve through distro
symlinks. Its full failure is retained in `sccp-convert/native-lld`.
A separately authorized path-only correction copied that script to the owned
D directory and normalized its three dependency paths to the same read-only
canonical `/usr/lib` libraries. `OUTPUT_FORMAT`, `GROUP` and `AS_NEEDED` remain
unchanged. No system script or library was modified. Only `-lc` was replaced
with that private script, plus a distinct output name; the other linker inputs
and flags stayed unchanged.

That link returned 0 in 46.058 seconds. The bounded `readelf` check returned 0
and verified ELF64/x86_64, the Linux interpreter, the shared ASan dependency
and ASan/UBSan symbols. The ten-case executable actually returned 0 in
32.353 seconds with ASan leak detection and UBSan halt-on-error; its output is
`SCCP conversion preservation: 10 cases passed`, without sanitizer diagnostics.
Commands, original/private script diff, object and system-library hashes,
runtime environment and exits are in
`sccp-convert/native-lld/libc-path-only/`. The GCC capture and first link failure
are not counted as passing gates.

The reused Clang SCCP object predates a disclosed three-line comment-only
edit; the input receipt records both source hashes. Its behavior code is
unchanged. Root's native rebuild consumes the exact current source.

The root reviewed all six files; independent read-only review found no
introduced actionable findings within this cross-type scope. Root rebuilt
the current SCCP and parser-roots targets, exit 0, and ran:

```text
ctest --test-dir D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc -R "^(ssa_sccp_conversion|ssa_roots_observation)$" --output-on-failure --no-tests=error -j 1
```

It returned 0, 2/2 PASS: SCCP in 41.44 seconds and parser roots in 40.82 seconds.
Evidence is in `control/sccp-parser-inline-final-{build,ctest}.log`.
Root independently reran the final Clang sanitizer binary: exit 0 in
25.470 seconds, all ten cases passed, no sanitizer diagnostics. Its exact
launcher/result is `control/sccp-cross-root-clang-sanitizer.log`.
This accepts the finite cross-type correction on the established native and
Clang sanitizer gates; GCC final execution and broader scalar work remain open.

This slice does not implement cross-type constant conversion evaluation,
module-aware constant allocation, floating arithmetic folding, or ownership
copy fixes. It introduces no public API, allocation path, ABI, cancellation
path, or migration compatibility branch. Those broader scalar optimizer
requirements retain their existing plan status.

Root's independent review identified an existing same-token representation
gap outside these ten cases. With no pool, `CONSTANT` annotated DOUBLE and
immediate `7` produces a signed runner value. A DOUBLE-target `CONVERT` changes
it to floating `7.0`, but matching tokens still allow SCCP to rewrite that
conversion to a signed immediate constant. BOOL and UNSIGNED typed immediates
have the same representation/normalization concern. This slice does not prove
those forms safe or repair them; its positive identity cases are INT64
immediate and INT64/DOUBLE pool values with explicit matching representations.
The remaining representation guard requires its own RED/GREEN follow-up and
does not invalidate the demonstrated cross-type preservation change.
