---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
  - tests/parser/test_ssa_invoke_result_availability.c
  - tests/cmake/ssa-invoke-result-tests.cmake
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/instruction-generation/invoke-result-edge-availability.md
tests:
  - tests/parser/test_ssa_invoke_result_availability.c
  - tests/cmake/ssa-invoke-result-tests.cmake
  - tests/acceptance/ssa-invoke-result-edge-availability.md
doc_type: testing-guide
---

# SSA 01.03 INVOKE result edge availability

## Scope and regression

The core SSA verifier now checks the destination of a PHI incoming from the
defining throwing terminator, and stops earlier exceptional-result propagation
when control re-enters that definition. This rejects an INVOKE result on its
direct exceptional PHI edge while allowing normal results after handler retry.
A mixed-predecessor PHI also distinguishes the normal result edge from a handler
edge carrying an external value.

`test_ssa_invoke_result_availability.c` calls the actual public core verifier.
Its twelve cases include six positive CFGs and six negative CFGs. Each negative
case checks `EXCEPTION_EDGE`, function token 7101, consumer block/instruction/
source, defining instruction/value 1/2 (or 3/3 for the self-exception retry). Positive cases
require `NONE`. Exceptional direct and merge PHIs first prove their identical
CFG valid with an external incoming value.

## Current twelve-case self-exception boundary

Quality review found another structurally valid CFG: entry INVOKE has a normal
exit and exception block B; B contains a result-producing INVOKE whose normal
successor is N and whose exceptional successor is B itself. A PHI in N consuming
B's result on its normal edge is legal. The old initial exceptional-successor
shortcut matched B as the PHI predecessor before applying the definition stop,
so it incorrectly rejected that normal PHI. The initial successor filter now
stops at the definition before matching a use, consistently with the DFS stop.
The explicit PHI destination check still rejects B's result on B's exceptional
self edge.

Three new fixtures check the normal PHI, ordinary normal operand control, and
illegal exceptional self-edge PHI. The negative fixture first validates both
incoming values as external, then changes only the self-edge incoming result.
It checks diagnostic `EXCEPTION_EDGE`, token 7101, consumer block/instruction
3, source 103, defining instruction/value 3/3. The full twelve-case suite has
six positive and six negative cases.

Actual MSVC rebuilt the changed test and SSA translation units against thirteen
unchanged own support objects. All other 151-closure source/header inputs match
the previous complete epoch, and reused object hashes are recorded before/after.
The immutable `self-red-source` build compiled and linked successfully; its
fixture exited 1 with exactly the normal self-retry PHI failure (11 passes).
After the seed fix, `self-green-source` compiled and linked successfully and
its fixture exited 0: **12 cases, 0 failures**. Evidence:
`msvc/self-red-manual-receipt/receipt.json` and `fixture.log`,
`msvc/self-green-manual/receipt.json` and `fixture.log`. An earlier first RED
run had the same semantic result but a post-run receipt serializer exception;
its logs are retained and the corrected independent RED run supplies the full
receipt.

The actual unchanged CMake fragment was included in a fresh `self-cmake-source`
151-input snapshot. Configure, all fifteen TU target compilation/link, and
registered CTest each exited 0: **1/1 tests passed**. Current evidence is
`self-cmake-gate/receipt.json` and its configure/build/ctest logs. The parent
include and fragment hashes remain unchanged. `final-self-source-seal.json`
compares each current E input and immutable D input for both self-GREEN and
self-CMake closures: **151 + 151 files, zero mismatches**.

Current production SSA SHA-256:

```text
9E4C0B7389B180A9B61591CB9ED4D22D89CCA92C76D613FE6CB0E61D1FEF2F54
```

Current Clang 14 and GCC 11 rebuilt both changed actual translation units with
ASan/UBSan; the thirteen support objects remain their respective own instrumented
objects from the complete earlier build. The unchanged source/header closure
was compared before reuse. Each new test compilation exited 0. Initial SSA
compilations hit the 300-second outer timeout; exact own Linux output-marker
process scans subsequently returned empty before retry. Those logs and
`self-green-incremental/timeout-disposition.json` remain preserved. Each SSA-only
retry used an inner Linux process group and 600-second timeout, wrote its actual
PID/argv/cwd and waited for terminal exit. Clang exited 0 without timeout in
281.49 seconds; GCC exited 0 without timeout in 138.13 seconds. Native receipts
record reused object hashes before/after; they match. No compiled object was
moved or copied and no other task's object was adopted.

Clang link-only native ELF LLD recovery used the actual same-task Clang 14
recorded plan and current own fifteen objects, with unchanged static sanitizer
inputs, no-PIE and section flags. Link exited 0 without timeout and object/SDK
hash changes. Its actual WSL sanitizer fixture exited 0: **12 cases, 0
failures**, no sanitizer report. Current binary SHA-256:

```text
2e8b96b0313b3860d0fa05f4fb506cbf712bc04aad1986172e438c35af463378
```

Current Linux compile evidence for each toolchain:
`self-green-incremental/compile-0.log`,
`self-green-retry/ssa-retry-receipt.json`, and
`self-green-retry/native-receipt.json`. Current Clang link/runtime evidence:
`clang/self-green-native-link/link-receipt.json`,
`clang/self-green-native-link/fixture-run-receipt.json`, and `fixture-run.log`.
Current GCC link-only native ELF LLD recovery used its actual same-task GCC 11
recorded collect2 plan and current own fifteen objects, retaining the same GCC
ASan/UBSan runtimes and program flags. Link exited 0 without timeout and with
zero object/SDK input changes. Its actual WSL leak/abort/halt sanitizer run
exited 0: **12 cases, 0 failures**, no sanitizer report. Binary SHA-256 matched
before/after:

```text
274d50c8ee2819ee30ef6db781ada881f61a9dc0243de47266eb167c96f764cd
```

Current GCC link/runtime evidence:
`gcc/self-green-native-link/link-receipt.json`,
`gcc/self-green-native-link/fixture-run-receipt.json`, and `fixture-run.log`.
`current-object-provenance.json` records all fifteen input object paths for
both Linux toolchains and the unchanged support-object hash checks.

Re-run the current preserved twelve-case executables and registered CTest:

```powershell
& 'D:/tmp/zr_vm/ssa-20261003-01a0fe2b/invoke-edge/msvc/self-green-manual/fixture.exe'
& 'C:/Users/HeJiahui/AppData/Local/Python/bin/python.exe' 'D:/tmp/zr_vm/ssa-20261003-01a0fe2b/invoke-edge/run-recovered.py' clang self-green-native-link
& 'C:/Users/HeJiahui/AppData/Local/Python/bin/python.exe' 'D:/tmp/zr_vm/ssa-20261003-01a0fe2b/invoke-edge/run-recovered.py' gcc self-green-native-link
& 'E:/Visual Studio/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe' --test-dir 'D:/tmp/zr_vm/ssa-20261003-01a0fe2b/invoke-edge/self-cmake-gate/build' -R '^ssa_invoke_result_availability$' --output-on-failure --no-tests=error
```
The nine-case evidence below is historical and remains preserved as the first
repair's RED/GREEN record. It does not substitute for the twelve-case gate.
## Historical nine-case RED and GREEN

MSVC 19.44 built all 15 actual translation units and linked successfully before
the implementation changed. The nine-case RED fixture exited 1 with exactly
four expected failures:

- The direct exceptional PHI incorrectly accepted the INVOKE result.
- A normal operand after handler retry was incorrectly rejected.
- A normal PHI after handler retry was incorrectly rejected.
- A PHI with a valid INVOKE-normal incoming and an external handler incoming
  was incorrectly rejected.

After the repair, a fresh MSVC build of all 15 translation units, link, and
fixture each exited 0: **9 cases, 0 failures**. The build used actual MSVC
14.44.35207 tools and the installed Windows SDK 10.0.26100.0 include/lib paths,
with `/std:c11 /Od /Z7 /MTd /experimental:c11atomics`.

Clang 14 compiled all 15 actual translation units with ASan/UBSan successfully.
Its initial GNU linker timed out after 180 seconds on mounted filesystem input.
A finite link-only recovery used native ELF LLD with the actual Clang 14
`-###` plan, the same static sanitizer runtimes, `-no-pie`, section collection,
and the original `-lm -pthread -ldl` flags. The plan query used one existing
ext4 object to avoid fifteen slow mounted-file stats; its unique input slot was
then replaced by the fifteen freshly compiled D objects. Both the queried and
expanded plans, that substitution, and object/SDK pre/post hashes are recorded.
No compiled objects were copied or substituted from another build.

The recovered Clang executable ran in WSL with `detect_leaks=1`,
`abort_on_error=1`, and UBSan `halt_on_error=1`: **9 cases, 0 failures**, exit 0,
without a sanitizer report. Its binary SHA-256 was unchanged before/after:

```text
629b63907ac5a03e7b7293e6e68ab4dbd00f991d71c38c9d8424ec5c0ec2fe30
```

GCC 11.4 also compiled all 15 actual translation units with ASan/UBSan. Its
initial GNU link timed out after 180 seconds. Link-only recovery used native
ELF LLD with the recorded actual GCC 11 `-###` collect2 plan and the same GCC
`libasan.so.6.0.0`/`libubsan.so.1.0.0` inputs. Only non-LTO collect2 plugin
dispatch controls were removed; program link flags were retained. The fifteen
own D objects and SDK inputs had matching pre/post hashes. The resulting ELF
ran in WSL with the same leak/abort/halt sanitizer options: **9 cases, 0
failures**, exit 0, without a sanitizer report. Binary SHA-256:

```text
86fc6a9633c245874f156c99b59b4a64ef385fa8aa7e82e67974fee65e5ebe00
```

GCC evidence: `gcc/green-snapshot/receipt.json`,
`gcc/native-link-recovery/link-receipt.json`,
`gcc/native-link-recovery/fixture-run-receipt.json`, and
`gcc/native-link-recovery/fixture-run.log`. The recovery receipt records the
same-task recorded-plan path and hash; no old compiled object is borrowed.
## Source and artifact boundary

All new build products, scripts, snapshots, temporary files, logs, and receipts
originate under:

```text
D:/tmp/zr_vm/ssa-20261003-01a0fe2b/invoke-edge
```

`red2-source-manifest.tsv` and `green-source-manifest.tsv` each list 151 exact
source/header inputs with original E source pre-copy SHA-256, D snapshot
SHA-256, and original E source post-copy SHA-256. All three match. The closure
includes private headers and `exec_ir_opcode.def`. The snapshots are immutable
during compilation. Native source sealing compares all 151 current originals
and snapshot files to the manifest; no borrowed objects or old build cache is
used.

Production source SHA-256:

```text
RED exec_ir_verify_ssa.c:
0F8029629E6F1DEDA85B114A3FBB9FB8A13CAE8D2BDED2BB68C5508D7438B27B
GREEN exec_ir_verify_ssa.c:
3BA730B8FD68E66DB54960EF950C43E2B626D1D5DACD208B607222C0E85D2A88
test_ssa_invoke_result_availability.c:
11239B07B1F6D80FA7C2F17B9FC27F48E57818957688581C501D64F865F67663
```

MSVC evidence: `msvc/red2-manual/receipt.json`, `msvc/red2-manual/fixture.log`,
`msvc/green-manual/receipt.json`, and `msvc/green-manual/fixture.log`. Each receipt
contains the exact compiler/link/execution argv and exit codes; input hashes are
preserved before and after the build.

Clang evidence: `clang/green-snapshot/receipt.json`,
`clang/native-link-probe-recovery/link-receipt.json`,
`clang/native-link-probe-recovery/fixture-run-receipt.json`, and
`clang/native-link-probe-recovery/fixture-run.log`. The original timeout is
preserved alongside the successful recovery.

Re-run the preserved MSVC executable:

```powershell
& 'D:/tmp/zr_vm/ssa-20261003-01a0fe2b/invoke-edge/msvc/green-manual/fixture.exe'
```

## Historical nine-case CMake registration gate

The repository parent `tests/CMakeLists.txt` contains the exact new fragment
include once. `cmake-parent-include.txt` records that parent hash and matching
original/snapshot fragment hashes. A fresh D snapshot configures a minimal C11
project that includes the unchanged real fragment, then builds its actual
15-TU target with MSVC/Ninja and runs the registered CTest. Configure, target
build, and `ctest -R ^ssa_invoke_result_availability$ --output-on-failure
--no-tests=error` each exited 0: **1/1 tests passed**. This checks the target and
its registration without configuring the full legacy parent project.

Evidence: `cmake-gate-sdk/receipt.json`, `configure.log`, `build.log`, and
`ctest.log`. The initial `cmake-gate` configuration failure (SDK `rc`/`mt` absent
from PATH) remains recorded; the successful gate adds the installed SDK bin
path, with source and fragment unchanged. `final-source-seal.json` verifies
both GREEN and CMake 151-file original/snapshot closures after validation,
with zero mismatches.

Re-run the registered CTest using its preserved build:

```powershell
& 'E:/Visual Studio/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe' --test-dir 'D:/tmp/zr_vm/ssa-20261003-01a0fe2b/invoke-edge/cmake-gate-sdk/build' -R '^ssa_invoke_result_availability$' --output-on-failure --no-tests=error
```
## Limits and recovery

This acceptance covers core structural/SSA validation, rather than callee
execution, source-language lowering, backend parity, or the full SSA milestone.
The repair adds no allocations or mutable shared state. Existing verifier OOM
handling remains responsible for its dominance/exception traversal allocations;
failure injection was not expanded by this slice. Cancellation, network access,
and authorization state are not inputs to this pure IR check.

Early E-read and incomplete-snapshot Linux compiler attempts were stopped only
after matching their own PID/start/argv/cwd, with termination receipts retained.
The initial native bootstrap timed out before compiling; the first native
snapshot build diagnosed one missing private header. The complete 151-input
epochs supersede those setup attempts. Foreign processes, dirty files, and
their artifacts were preserved.
