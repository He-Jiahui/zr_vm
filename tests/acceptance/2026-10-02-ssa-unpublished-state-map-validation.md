---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - tests/parser/test_ssa_builder_cfg.c
  - tests/parser/ssa_builder_unpublished_state_maps.inc
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
tests:
  - tests/parser/test_ssa_builder_cfg.c
  - tests/parser/ssa_builder_unpublished_state_maps.inc
doc_type: testing-guide
status: scoped-msvc-accepted
---

# Unpublished state-map verification, 2026-10-02

## Failure and scope

The source ExecBC integration failed at the builder stage with diagnostic 24
(`STATE_MAP_INVALID`) in four existing positive branch cases. The function,
block, instruction and source diagnostics were `20741 / 0 / 0 / 0`. This is a
shared builder failure before module publication, independent of source MUL
admission. The division rejection remained successful.

The strict core verifier now validates a supplied map. For a source semantic
function with `symbolId == 0`, map construction restores the provisional map
token to zero. The subsequent candidate helper temporarily changes only the
function token to one. Strict map identity validation rejects that mismatch
before the `BuildModule` publication code can assign the final identity.

The correction changes only `verify_unpublished_ssa`: stack function and map
views share the original storage; only a matching pair of unpublished zero
tokens receives the private verification identity. Original identities remain
untouched, and signature, generation and checkpoint corruption still reach
strict validation. Module publication and its rollback ordering are unchanged.
The module contract is documented in
`docs/parser-and-semantics/execir-unpublished-state-map-validation.md`.

## Permanent regression coverage

The existing `ssa_builder_cfg` executable includes the new fixture file.

| Case | Required behavior |
| --- | --- |
| Raw zero-symbol `DROP -> RETURN` | Build succeeds; ID, function token and map token stay zero; three cleanup phases remain present |
| Module publication of that candidate | Final ID/token/hash/generation and map identity match; full verification passes |
| Changed published checkpoint source ID | Full verification rejects it with `STATE_MAP_INVALID`, instruction 1; restoring the source succeeds |
| Zero-symbol empty source function | Module publishes an identity-matched empty map and full verification passes |

## Direct MSVC RED/GREEN evidence

All artifacts are under
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression`.
`identity_driver.c` includes the production builder and the ordinary test
translation unit to call the private candidate helper directly. The native
driver compiles those two sources in one translation unit and links the already
built root MSVC builder CFG support objects, excluding their original builder
and test objects. It does not rebuild or alter the root matrix.

The native runner imports the existing VsDevCmd environment, uses `/std:c11`,
`/MDd`, `/Zi`, `/Od`, `/utf-8`, and writes object, PDB, executable and logs to the
driver subdirectory. Commands:

```powershell
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/run_native.py D:/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression/msvc-driver-red.log D:/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression/native_driver.py msvc-driver-red
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/run_native.py D:/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression/msvc-driver-green.log D:/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression/native_driver.py msvc-driver-green
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/run_native.py D:/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression/msvc-driver-and-suite-green.log D:/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression/native_driver.py msvc-driver-and-suite-green
```

| Run | Compile / run result | Evidence |
| --- | --- | --- |
| Original helper, RED | 0 / 1 | `zero_candidate valid=0 code=24 diagnostic_token=1 original_token=0 map_token=0 entries=3` |
| Corrected helper, GREEN | 0 / 0 | `zero_candidate valid=1 code=0 diagnostic_token=0 original_token=0 map_token=0 entries=3`; malformed-map cases PASS |
| Corrected helper plus full builder CFG main | 0 / 0 | Private candidate cases PASS and `ssa builder CFG PASS` |

The direct helper checks the original function and map headers with `memcmp`
after successful verification. It also independently corrupts map token,
signature hash, generation and checkpoint source ID; every corruption is
rejected with diagnostic 24. The bad checkpoint diagnoses instruction 1.
Restoring the map allows candidate verification again. The token corruption
case also asserts that rejection leaves the original zero function token and
bad map token intact.

## Validation limits

A separate direct GCC C11 build used exactly the builder CFG source list from
the root Ninja file, with temporary files and the output executable on D. It
spent several minutes reading source and headers through DrvFS and did not
reach execution. Its exact owned process tree was stopped after the independent
MSVC RED evidence was available. `gcc-red/build.log` preserves the command;
this run supplies no GCC success evidence. Clang and sanitizer runs were not
performed in this slice.

The root's formal CMake/CTest results are recorded below. These scoped MSVC
checks do not claim completion of all 01.04 requirements or the broader source
MUL task.

## Bounded incremental GCC follow-up

The WSL follow-up audited the existing successful
`gvn-move/gcc-green-final/receipt.json` and its originating
`gcc-red-parallel/receipt.json` before compiling. The exact builder CFG source
manifest contains 28 translation units. Fifteen support objects had matching
recorded source SHA-256 values, compiler flags (`-std=c11 -g -O0`) and the same
three include roots. The recursively resolved project dependencies were also
hashed and their last modification times preceded the original compiler
objects in `gcc-red-parallel`. The final cached copies matched those original
object hashes; the audit uses original compile timestamps, not copy timestamps.
The reusable object hashes, source hashes, dependency hashes and 13-source
difference are recorded in `map-build-regression/gcc-reuse-plan.json`.

SCCP support objects were inspected but excluded: their GNU11, section and
NDEBUG flags differed, and they lacked a historical source hash receipt. No
other task's cache was modified. The follow-up first compiled only the new
builder CFG test object, then continued sequentially through the missing
parser sources. Each compilation had a 120-second deadline. All objects,
dependency files, logs and temporary files remained under the D task directory.

```powershell
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression/plan_reuse.py
wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/python3 /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression/incremental_gcc.py --probe
wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/python3 /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression/incremental_gcc.py
wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/python3 /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/map-build-regression/audit_owned_stop.py
```

| Incremental compilation | Exit | Wall time |
| --- | --- | --- |
| `test_ssa_builder_cfg.c` | 0 | 104.57 seconds |
| `exec_ir_build.c` | 0 | 67.07 seconds |
| `exec_ir_effects.c` | 0 | 59.59 seconds |
| `exec_ir_effects_linear.c` | 0 | 60.85 seconds |
| `exec_ir_effect_loops.c` | 124, deadline | 120.02 seconds |

The timed-out step emitted no compiler diagnostic. The runner stopped at that
first deadline; it did not start the remaining difference or link and execute
the test. Four fresh objects were completed, and the 15 support objects were
identified as reusable but had not yet been linked into this test. This is
partial compile evidence, not a GCC passing test result.

`gcc-incremental-green/receipt.json` records every exact command, runner PID,
child PID/process group, elapsed time and exit status. The timed-out compiler
had owned process group 57369, created in a new session and stopped with
SIGTERM. Its parent was reaped, and the subsequent `/proc` audit found zero
remaining members of that group. The receipt records
`validation_status: bounded-compile-timeout-no-link-or-run`. The directory name
does not indicate a successful gate. Clang was not attempted after this GCC
deadline, so GCC execution and Clang/sanitizer validation remain unverified.

## Root independent validation and decision

The root reviewed the five-file change, and a separate read-only gpt-6-sol
review found no introduced issue in the private identity views or publication
transaction. The formal MSVC builder target first failed with the original
helper: `STATE_MAP_INVALID` code 24 at the legal zero-symbol cleanup fixture
(`control/builder-map-red-ctest.log`, CTest exit 8).

After the correction, the root rebuilt the formal builder, source integration
and core-root targets in its fresh repository MSVC Debug tree. Build exited
zero; the registered `ssa_builder_cfg` passed in 2.12 seconds:

```powershell
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/run_native.py D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/builder-source-roots-green-build.log D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc cmake --build D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc --target zr_vm_ssa_builder_cfg_test zr_vm_ssa_source_execbc_vm_test zr_vm_ssa_core_roots_observation_test -j 4
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/run_native.py D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/builder-source-roots-green-ctest.log D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc ctest --test-dir D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc -R "^(ssa_builder_cfg|ssa_source_execbc_vm|ssa_core_roots_observation)$" --output-on-failure -j 1
```

That three-test run passed builder and core-root tests and failed source
integration only in its two new MUL cases. The four existing positive branch
cases and division rejection recovered; MUL now reaches the independent
materializer `UNSUPPORTED` diagnostic 28. The entire three-test suite is not
claimed as green, and the source MUL consumer fix remains separate.

The unpublished-identity repair is accepted with actual MSVC behavioral
RED/GREEN, full registered builder coverage, corruption checks and partial
GCC compilation. GCC execution and Clang/sanitizer gates remain unverified
because of the bounded tool delays above. All generated root outputs and
compiler temporaries stayed under `D:/tmp/zr_vm/ssa-20261002-01a0fc3b`.
