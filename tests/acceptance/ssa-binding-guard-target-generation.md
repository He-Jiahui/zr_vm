---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_binding_guard.h
  - zr_vm_core/src/zr_vm_core/execution/execution_binding_guard.c
  - zr_vm_core/src/zr_vm_core/call_binding.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_binding_guard.c
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/03-guarded-caches.md
tests:
  - tests/core/test_ssa_guarded_caches.c
doc_type: testing-guide
---

# VM binding guard target generation

## Scope

The local public guard now rejects an expired nonzero resolved VM target
generation before receiver shape checks can select a slot fallback. Persistent
contract checks, binding/frame generation and optional module/signature/layout
expectations keep their existing priority. Target-stale diagnostic expected
and actual fields now contain recorded and observed generations, matching
`ZrCore_CallBinding_Validate`. Other target availability checks retain their
position. No ABI, parser, shared CMake or plan changes.

This is a finite guard correction under SSA 03.03, not completion of that
milestone. The current guard API consumers are unit tests; interpreter dispatch,
PIC budget, reload/lease and GC integration are outside this acceptance.

## Baseline and RED

The old guard jumped from either receiver shape mismatch directly to
`shape_miss`, skipping the VM target generation comparison. The fixture uses a
valid virtual contract, binding/frame generation 4, recorded target generation
7, actual VM function generation 8, shape ID 8 versus receiver 9 and declared
slot 2 within table length 4. The required stale result was incorrectly
`SLOT_FALLBACK`.

Actual original-source RED:

- GCC 11.4.0, four real TUs with ASan/UBSan: compile/link 0, fixture 1;
  `7 Tests 1 Failures 0 Ignored`, `Expected 1 Was 5` at the new stale-route
  test. All six other tests passed. Snapshot inputs unchanged.
- MSVC 19.44.35228, five real TUs: compile/link 0, fixture 1, same seven-test
  result and specific enum mismatch. Snapshot inputs unchanged.

Two earlier four-TU MSVC attempts compiled all TUs but failed to link with
LNK2019/LNK1120 for the real graph visitor referenced by unused
`AdvanceGeneration`. Defining static linkage did not resolve this. These are
build failures, not RED runtime evidence. Adding the real
`call_binding_graph.c` support TU made the actual dependency closure link; no
stub or fake implementation was introduced. Tests do not call the graph.

## Test inventory

Seven always-active Unity test groups include the original four and three
new groups. The new groups cover:

- Stale VM target with shape ID mismatch, fallback allowed and disabled;
  shape generation mismatch; repeated stale result with exact
  result/status/target kind/slot/expected 7/actual 8 diagnostics.
- One miss increment per result, unchanged hit count, saturated miss count,
  and unchanged binding and receiver byte representations.
- Signature, layout and module expectation failures retaining their specific
  classes before target stale, plus binding/frame mismatch retaining its
  priority and diagnostic values.
- Fresh VM target permitting legal slot fallback or shape miss; matching
  shape producing a hit; hit saturation; an older caller/frame generation 2
  independently matching callee generation 7; zero target generation and null
  VM function compatibility controls.

All fixtures use valid in-memory runtime structures and the real public API.
No target function/callback invocation, plugin or artifact loading, native FFI,
networking, or authorization/security boundary tests are performed. Allocation
failure and cancellation injection are not applicable to this allocation-free
guard. This correction does not alter target ownership or pointer lifetime.

## Tooling and reproduction

All products, compiler temporaries and evidence are under
`D:/tmp/zr_vm/ssa-20261003-01a0fe2b/guard-generation` (`R` below). Exact actual
source/header inputs were copied to immutable D snapshots with SHA256 values.
The final five-TU GREEN snapshot is
`R/snapshots/green-v1-1790980196144750300`, containing 65 resolved source and
header inputs. Native verification compares both original E inputs and D
copies. Actual drivers record commands, PIDs/start times, exits/timeouts and
pre/post input hashes; they do not reuse compiled products from another task.

The five TUs are the test, guard, `call_binding.c`, real Unity `unity.c`, and
`call_binding_graph.c`. Includes are common/core/Unity. Linux uses C11, O0,
debug info, function/data sections, no PIE and ASan/UBSan; MSVC uses C11,
Od/Z7/MTd/Gy/Gw and OPT:REF. Compiler identities observed: Ubuntu GCC 11.4.0,
Ubuntu Clang 14.0.0, MSVC 19.44.35228. MSVC `/Bv` metadata query printed its
version then exited 2 because it had no source file; this is not a build pass.

Source snapshot command (native Python executable is
`C:/Users/HeJiahui/AppData/Local/Python/bin/python.exe`):

```text
python D:/tmp/zr_vm/ssa-20261003-01a0fe2b/validation-control/actual_tu_driver.py R/green-config.json --toolchain msvc --snapshot
python <driver> R/snapshots/green-v1-1790980196144750300/snapshot-config.json --toolchain msvc
wsl -e /usr/bin/python3 /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/validation-control/actual_tu_driver.py /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/guard-generation/snapshots/green-v1-1790980196144750300/snapshot-config.json --toolchain gcc
```

The same WSL command with `--toolchain clang` compiled all five TUs but its
ordinary WSL link timed out at 180 seconds, exit -15. Inputs were unchanged;
this attempt is explicitly not a runtime pass. After the driver terminated,
the native LLD helper linked those same five own D objects, using the recorded
successful current-task Clang 14 link plan. Only object paths/count and output
path were substituted; Linux SDK canonical paths/libc script provided Windows
path compatibility. SDK and object hashes remained unchanged. The plan source
and full arguments are in `R/clang/native-link-v1/plan-source.json` and
`link-receipt.json`; no compiled object was copied or borrowed.

The actual ELF checks use:

```text
wsl -e /usr/bin/env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 <actual-own-ELF>
wsl -e /usr/bin/readelf -h -l -d <actual-own-ELF>
wsl -e /usr/bin/nm <actual-own-ELF>
```

## Results and evidence

| Gate | Actual result | Receipt under R |
| --- | --- | --- |
| Original GCC RED, four TUs | compile/link 0, runtime 1, exact enum failure | `gcc/red-v1/receipt.json` |
| Original MSVC RED, five TUs | compile/link 0, runtime 1, exact enum failure | `msvc/red-graph-v3/receipt.json` |
| GREEN MSVC, five TUs | compile/link/run 0, 7/7 tests | `msvc/green-v1/receipt.json` |
| GREEN MSVC repeat | run 0, 7/7 tests | `repeat-msvc/repeat-receipt.json` |
| GREEN GCC ASan/UBSan | compile/link/run 0, 7/7 tests | `gcc/green-v1/receipt.json` |
| GREEN GCC repeat | run 0, 7/7 tests | `repeat-gcc/repeat-receipt.json` |
| GCC actual ELF proof | run/readelf/nm 0, ELF64/Linux interpreter/ASan/UBSan symbols | `gcc/elf-proof/receipt.json` |
| Clang ordinary WSL link | timeout, exit -15; no runtime claimed | `clang/green-v1/receipt.json` |
| Clang native LLD recovery | link 0, own objects/SDK stable | `clang/native-link-v1/link-receipt.json` |
| Clang actual ELF proof | run/readelf/nm 0, 7/7 tests, ELF64/Linux interpreter/ASan/UBSan symbols | `clang/elf-proof/receipt.json` |
| Clang actual ELF repeat | run 0, 7/7 tests | `repeat-clang/repeat-receipt.json` |
| Final original/snapshot check | exit 0, 65 inputs unchanged | `green-v1-snapshot-verification.json` |
| Standalone registered CMake/CTest | configure/build/CTest 0, 1/1 registered test passed | `formal-evidence/configure-receipt.json`, `build-receipt.json`, `ctest-receipt.json` |

Final GCC binary is `R/gcc/green-v1/fixture`, SHA256
`f9ffd021f9417f61b769815236207c4f2b71391aa16f0dd57d74f288319f5c5b`.
Final Clang binary is `R/clang/native-link-v1/fixture`, SHA256
`a28c0c3b71c0f709836b7a28af654809702e1763c64efcfacaf34184156e7c97`.
Final MSVC binary is `R/msvc/green-v1/fixture.exe`.

The standalone project `R/formal-project/CMakeLists.txt` registers the existing
target name `zr_vm_ssa_guarded_caches_test`, CTest name `ssa_guarded_caches` and
label `ssa`, and builds the same five actual snapshot TUs. Its exact configure,
build and CTest commands are recorded in the three formal receipts. The final
CTest command was:

```text
D:/Tools/development/cmake/bin/ctest.exe --test-dir D:/tmp/zr_vm/ssa-20261003-01a0fe2b/guard-generation/formal-msvc -R ^ssa_guarded_caches$ --output-on-failure --no-tests=error
```

It ran one test, passed, exit 0. This is a fresh standalone registered project,
not the full parent build. Compared with the existing declaration at
`tests/cmake/ssa-tests.cmake:756`, it expands the real core/Unity dependencies
into five explicit TUs and uses Unity's default local configuration rather
than the parent `UNITY_INCLUDE_CONFIG_H`/full core linkage. Target, test and
label identities match; parent harness/whole-library compatibility is not
claimed. No parent registry edit was required. The standalone executable is
`R/formal-msvc/zr_vm_ssa_guarded_caches_test.exe`.

`R/final-manifest.json` binds the four formal files, snapshots, actual final
binaries and evidence hashes. Reproduction config paths refer to immutable
snapshots; a new run label/output is required because drivers refuse to
overwrite prior compilation runs. Separate cleanup receipts, if authorized
after commit, preserve the frozen formal manifest and accepted evidence.

## Acceptance decision

The local runtime correction has passing real GCC/Clang sanitizer and MSVC
unit gates, a passing registered standalone CTest, original-code RED evidence
and precise error/counter controls. The implementation and evidence are frozen
for independent specification/quality review and root reruns before commit.
No whole-parent build or complete SSA03.03 acceptance is inferred from this
focused suite.
