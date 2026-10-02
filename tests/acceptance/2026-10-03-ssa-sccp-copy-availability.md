# SCCP COPY value availability

## Scope

This finite parser pass repair prevents COPY alias propagation from changing
which SSA value MOVE, DROP, or DROP_IF_INITIALIZED consumes, and prevents reads
from bypassing an independently available COPY result. It changes only the
alias construction region of `exec_ir_sccp.c`; constant-cache work and foreign
pass-manager/state-map changes are outside the edit scope.

## Baseline

Before this repair, `exec_ir_sccp.c` had 941 lines and SHA256
`1ED1F4F7A6718E0AAD8B0B6763B522E7699002335B7F83A5621011D1FD20DCB7`.
The dedicated test uses the actual public verifier, Oracle, SCCP implementation,
and owner analysis. The MSVC baseline compiled and linked all 34 real translation
units with exit 0. Its fixture returned exit 1, printing the original Oracle's
signed return 7 followed by `INVALID_VALUE` (code 13), instruction 4, source 104
after SCCP rewrote the final operand. This was a semantic RED, not a compiler or
startup failure. All 123 source/header inputs matched their original files and
snapshot when the RED gate completed.

## Test inventory

- UNKNOWN, UNIQUE, SHARED source/result pairs.
- Each pair consumed through MOVE and DROP, separately consuming source and COPY result.
- UNIQUE and SHARED conditional cleanup through DROP_IF_INITIALIZED, in a cleanup block.
- Ordinary pure COPY control for each ownership mode; UNKNOWN propagates.
- Each of 19 groups runs three SCCP iterations, checking valid Oracle return 7.
- DROP event kind, instruction/source identity, and signed payload 7.
- Every instruction/value before/after owner state remains equal to the original.
- The original fixture passes the public verifier with diagnostic NONE before transformation.
- `test_consumption_scan_budget` measures actual analysis work with rewrite
  disabled, then sets the real work budget to that work plus prefixes 0, 1, and 2.
  Each stopping point in the three-instruction eligibility scan reports budget
  exhaustion without changing IR; RETURN still references COPY's result, and
  the public verifier and Oracle both succeed.

No allocation is introduced by the repair. Existing SCCP scratch allocation and
error handling remain unchanged; whole-milestone OOM/cancellation coverage is
not claimed. No source-language lowering change is included.

## Tooling evidence

The following table and its `green-v2`/`green-native-v1` binaries describe the
previous verifier epoch (`3BA730...`). They remain historical evidence after the
separate self-exception verifier repair below; their 123-input seal does not
cover the later verifier or parent CMake changes.

All generated inputs, compiler intermediates, binaries, and logs belong under
`D:/tmp/zr_vm/ssa-20261003-01a0fe2b/sccp-copy-availability`.

The actual-TU driver lives in the parent task's `validation-control` directory.
It hashes the include closure before and after compilation, records every
compiler/linker/runtime command and exit, and performs a final native Windows
comparison of original E files against the D snapshot. Its MSVC path uses
MSVC toolset 14.44.35207 with the existing toolchain environment metadata read
only; no existing object or binary was borrowed.

| Gate | Result and exact evidence directory |
| --- | --- |
| Initial VS environment import | 180-second timeout before compilation; not a semantic RED. `msvc/red-v1` preserved. |
| Baseline MSVC RED | Compile 34/34 and link exit 0; fixture exit 1 after 41.065 seconds. `msvc/red-native-v2` |
| First MSVC GREEN snapshot | Compile/link/fixture exit 0, 19 groups and three budget checks. `msvc/green-v1`; final original-file seal rejected a concurrently changed verifier. Historical snapshot result only. |
| Current MSVC GREEN | Fresh 34/34 compile, link and fixture exit 0; runtime 13.989 seconds. `msvc/green-v2` |
| Current input seal | 123 original/snapshot inputs, `inputs_changed=[]`, exit 0. `green-v2-snapshot-verification.json` |
| GCC full support attempt | 33/34 compile exit 0; unused ExecBC `projection_common` compile hit 120.755-second timeout (-15). No link or runtime gate claimed. `gcc/red-v1` |
| GCC minimal actual RED | Own successful 28 actual support objects linked exit 0 in 157.017 seconds, actual Oracle/SCCP fixture exit 1 in 1.587 seconds with the same code 13, instruction 4/source 104. `gcc/red-min28-v1` |
| GCC current compile/link attempt | Three changed sources compiled exit 0 against current snapshot, reusing 25 unchanged own objects; initial GNU link timed out after 181.977 seconds (-15). Posthash driver completed with `inputs_changed=[]`. `gcc/green-incremental-v1` |
| GCC current GREEN | Native LLD link exit 0 with the same 28 own objects and recorded GCC 11 runtime plan; actual WSL fixture exit 0 in 2.496 seconds, unchanged ELF64 binary. These are plain GCC objects; no GCC object sanitizer instrumentation is claimed. `gcc/green-native-v1/runtime-proof/receipt.json` |
| Clang initial link | All 34 actual sources compiled exit 0 with ASan/UBSan; initial WSL linker timed out after 120.416 seconds (-15). `clang/green-v2` |
| Clang ASan/UBSan GREEN | Native LLD link exit 0 using those same 34 own objects and the recorded Ubuntu Clang 14 runtime plan. Actual WSL fixture exit 0 in 7.512 seconds; readelf and nm exit 0, ELF64/Linux interpreter and both sanitizer symbols verified, binary SHA stable. `clang/green-native-v1/runtime-proof/receipt.json` |

The sole adopted change between the two GREEN input epochs was the separately
owned SSA verifier, SHA256
`3BA730B8FD68E66DB54960EF950C43E2B626D1D5DACD208B607222C0E85D2A88`.
`epoch-comparison.json` records this change. No verifier edits belong to this
task's manifest.

The GCC focused executable omits six ExecBC projection translation units because
the dedicated test directly calls Oracle/SCCP and does not lower or run ExecBC.
Its 28 support units are the actual repository implementations, including the
actual pass-manager budget and diagnostic helpers. No helper or Oracle behavior
is simulated. `gcc/red-min28-v1/receipt.json` binds each object to its successful
original GCC compile command and SHA256. The raw ELF evidence identifies GCC
11.4.0, x86-64 ELF64, and `/lib64/ld-linux-x86-64.so.2`.

The current GCC provider manifest identifies exactly three rebuilt sources and
25 unchanged own objects. The native recovery records object and canonical
Ubuntu SDK hashes before/after, and substitutes only program objects and output
in the successful compiler's linker plan. GCC's plain objects retain that GNU
plan's runtime libraries; the sanitizer gate comes from Clang's instrumented
34-TU build. Both original timed-out link receipts remain unchanged.

Final binaries have SHA256:

- GCC: `00227D6C334F99E84F403596DB8A0D1EBB8FA03A4F39721CF4B574BDB7D75109`.
- Clang: `35DD47691183E2B0FC40D3851EFB808F40E5D2DF13E7D04AC89CE1103B47FFDC`.

Rerun the retained historical native executable:

```powershell
& D:/tmp/zr_vm/ssa-20261003-01a0fe2b/sccp-copy-availability/msvc/green-v2/fixture.exe
wsl -e /usr/bin/env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/sccp-copy-availability/clang/green-native-v1/fixture
wsl -e /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/sccp-copy-availability/gcc/green-native-v1/fixture
```

Its original-file seal belongs to that historical verifier epoch. Comparing
the old snapshot against current repository inputs rejects the changed SSA
verifier, as expected. For a fresh rebuild, use the current formal snapshot
config and a new unique `label`, then call the driver with `--toolchain msvc`.
Existing run directories are protected against overwriting; receipts retain
the exact source, flags, include paths, and all 36 executed commands.

## Formal fragment and current verifier epoch

The separately owned verifier self-exception seed fix froze at 937 lines,
SHA256 `9E4C0B7389B180A9B61591CB9ED4D22D89CCA92C76D613FE6CB0E61D1FEF2F54`.
It is the sole changed source among the prior 123 actual-TU inputs; all original
and snapshot headers remain identical. The new immutable snapshot additionally
seals the actual conversion/COPY fragments, their sibling fixture sources, and
the parent CMake file used to obtain the original core-link helper: 129 inputs.

The standalone project loads the actual conversion fragment and this task's
actual COPY fragment, and uses the unchanged original `zr_vm_link_core` helper.
Its genuine static Core support archive compiles 19 actual Core sources from
the focused closure. It does not replace Core implementations with stubs.
The COPY executable's generated CMake source list is verified to equal the 34
actual TUs. This is a fragment registration/runtime gate, not a full production
Core library or parent product build.

| New epoch gate | Result and evidence |
| --- | --- |
| First formal attempt | Actual 34-TU executable/Core subset build and CTest passed, but initial configure crossed the 120-second timeout boundary; not accepted as the complete gate. `formal/msvc-v1` |
| Formal configure/build/registration/CTest | All exit 0 with `timeout=false`, respectively 15.572, 11.314, 5.075, and 4.617 seconds. CTest registered and ran exactly `ssa_sccp_copy_availability`, 1/1 PASS. `formal/msvc-v2/receipt.json` |
| Standalone final input seal | All 129 original/snapshot inputs unchanged, exit 0, verified at `1790979178.8269749`. `formal-v1-snapshot-verification.json` |
| GCC current support refresh | Only verifier TU rebuilt, exit 0; 27 unchanged own objects proved by SHA and source provenance. Exact recorded GNU plan link and actual WSL fixture/ELF/source seal passed. Plain GCC object instrumentation; `gcc/green-ssa-v3` |
| Clang current support refresh | Only instrumented verifier TU rebuilt, exit 0; 33 unchanged own objects proved by SHA and source provenance. Exact Clang runtime plan link, actual ASan/UBSan fixture, readelf and nm passed. `clang/green-ssa-v3` |

The new GCC binary SHA256 is
`A4FC87CD2BDC9D6C5FBCD8CEEBDE1D3BDA82484500AEB75A4D9700EDFD6DCBB8`;
Clang is `798163BDE47651EE1F36AB7D784E74E440FDFEE9E7B1C6265D3CA8F205A6E09B`.
Provider/current-source receipts bind the current 123-input SCCP source/header
closure, independently of the parent CMake integration blob.

Rerun the retained actual fragment CTest and current Linux executables:

```powershell
ctest --test-dir D:/tmp/zr_vm/ssa-20261003-01a0fe2b/sccp-copy-availability/formal/msvc-v1/build -R '^ssa_sccp_copy_availability$' --output-on-failure -V
wsl -e /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/sccp-copy-availability/gcc/green-ssa-v3/linked/fixture
wsl -e /usr/bin/env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/sccp-copy-availability/clang/green-ssa-v3/linked/fixture
```

After INVOKE commit `e74ba952`, root appended exactly one COPY fragment include
at parent `tests/CMakeLists.txt` line 8773. The original bytes, including the
core-link helper, are preserved. The explicit integration receipt is
`D:/tmp/zr_vm/ssa-20261003-01a0fe2b/validation-control/sccp-parent-integration.json`.
It binds the previous parent SHA256
`B5B690AE72C9BBDE9905775D33C1BE7516B3241A4BE17765754B0F015A447868`
to the new SHA256
`79C3A1DB47069A50CA2AD4A281BA988B17AAAEDC1131458911F3218DB37E25AE`,
with exactly one include occurrence. This explicit delta proves the next
integration epoch; the historical whole-parent seal covers its previous blob.
The real fragments, helper and 34 actual-TU inputs are unchanged, so this
integration adds no rebuild. The final six-file manifest includes the parent.

The retained CTest build directory is `formal/msvc-v1/build`; v2 receipts reuse
that exact directory. Root's attempted `formal/msvc-v2/build` invocation was a
missing-path error, not a passing CTest result.

## Results and acceptance

The finite repair passed current-source MSVC, GCC, and Clang ASan/UBSan gates.
The original and transformed Oracle, verifier, owner/event comparisons, and
three preflight budget stopping points passed. Actual standalone fragment
CTest registration passed; the root-owned parent include is integrated through
the exact one-line delta. Obsolete terminal MSVC RED/historical GREEN1
object files are removed with a task-local hash/path/process cleanup receipt;
their logs, receipts, binaries, and snapshots remain available.
The automatic-optimization plans remain planned/active.
