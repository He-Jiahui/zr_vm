---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - tests/parser/test_ssa_roots_observation.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
plan_sources:
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
tests:
  - tests/parser/test_ssa_roots_observation.c
doc_type: testing-guide
status: verified
---

# Parser observation of reused scalar slots

## Scope

This bounded fix rejects conflicting scalar writes to a reused physical slot
before changing frame bytes, writeback values, invalidation entries or the
invalidation count. It compares only bytes actually copied by the existing
observer. `INVALID_VALUE` identifies the later logical ordinal through the
adapter's existing `actualVersion` diagnostic field.

The fixture uses the real `LayoutPackedFrame`: two four-byte scalar values
have disjoint lifetimes and share storage; a third overlapping value receives
separate storage. It covers conflicting writes, preservation of all output
buffers on failure, equal copied bytes with different unused input bytes,
different payloads in separate storage, a single supplied writer and read-only
reused storage. Existing root construction, relocation and observation checks
remain enabled.

The request has no active-occupant selector. This change does not add safepoint
liveness, optimized-out debugger values, state-map production, GC integration,
new schemas or source-language observation. No whole-plan completion is claimed.

## Actual-source verification

Artifacts are exclusively under
`D:/tmp/zr_vm/ssa-20261003-01a0fe2b/parser-observation`.
The validation driver copies unchanged actual translation units and their
recursive include/`.inc`/`.def` closure into a private snapshot, records SHA256
for originals and copies, and checks the snapshot before/after compilation.
No stub, borrowed object or foreign build cache is used. Original E: sources
stay in place. MSVC reads a previously established toolchain environment as
configuration; it does not borrow compiled output.

Linux uses four actual translation units: the fixture, parser frame layout,
parser frame roots and core `exec_ir.c`. MSVC needs the genuine verifier and
state-support translation units to resolve `exec_ir.c` references, for seventeen
actual translation units. These support files are only read and copied;
foreign worktree changes are preserved.

The pre-fix MSVC run `msvc/red-msvc-full-v5` compiled all seventeen units and
linked successfully (exit 0), then exited 1 on the expected assertion at fixture
line 168: `!ZrParser_ExecIr_ObserveFrame(&observation, &d)`. The source snapshot
contained 66 stable files. This semantic RED was observed before editing the
production function.

Comparing the first RED and GREEN snapshots also found a foreign change to
`exec_ir_verify_ssa.c`. That source was preserved. A second RED,
`msvc/red-exact-green-support-v6`, recompiles the exact GREEN fixture, headers
and all support units, replacing only the parser root adapter with its retained
pre-fix source. All seventeen compiles and the link exit 0; the same fixture
line 168 exits 1. Its 67 input hashes (the GREEN closure plus the old adapter)
remain unchanged. This is the single-source differential control.

The final MSVC run `msvc/green-msvc-final-v1` compiles all seventeen units,
links and executes successfully (all exit 0; runtime 35.57 seconds).
The final GCC 11.4 run `gcc/green-linux-final-v1` compiles all four actual units,
links and executes with ASan/UBSan successfully (all exit 0; runtime 2.85
seconds). Both print `parser roots observation PASS (including reused scalar
conflicts)`. Their complete driver receipts report `inputs_changed: []`.
Native verification also checks all 66 MSVC and 43 Linux original/copy hashes
against their manifests without differences.

The Clang 14 static sanitizer link later timed out at 600 seconds despite four
successful actual-unit compiles and stable source hashes. After that linker
terminated, `clang/shared-asan-final-v2` reused those same four owned,
instrumented objects and linked with `-shared-libasan` (exit 0, 210.69 seconds).
Its executable passed (exit 0, 2.67 seconds) with ASan leak checks and UBSan
enabled. All four object hashes remain identical before and after that final
link/run. The runtime location is obtained from the actual Clang compiler:
`/usr/lib/llvm-14/lib/clang/14.0.0/lib/linux/libclang_rt.asan-x86_64.so`.
No source was recompiled or altered for this runtime-library selection.

| Gate | Actual compiles | Link | Fixture | Receipt |
| --- | --- | --- | --- | --- |
| MSVC semantic RED before production edit | 17, all 0 | 0 | 1, expected line 168 | `msvc/red-msvc-full-v5/receipt.json` |
| MSVC exact GREEN-support RED | 17, all 0 | 0 | 1, same line 168 | `msvc/red-exact-green-support-v6/receipt.json` |
| MSVC GREEN | 17, all 0 | 0 | 0 | `msvc/green-msvc-final-v1/receipt.json` |
| GCC 11.4 ASan/UBSan GREEN | 4, all 0 | 0 | 0 | `gcc/green-linux-final-v1/receipt.json` |
| Clang 14 ASan/UBSan GREEN, shared runtime | 4, all 0 in preceding run | 0 | 0 | `clang/shared-asan-final-v2/receipt.json` |

Every fixture check is always active, including when `NDEBUG` is defined. The
fixture uses no failure allocator and the fix adds no allocation; allocation
failure/rollback behavior of the frame builder is unchanged and is not a new
acceptance claim. Cancellation and suspend are outside this synchronous API.

## Replay

The complete actual compiler and linker argument arrays are saved in the
receipts above. Build snapshots and manifests remain in:

- MSVC: `snapshots/green-msvc-final-v1-1790973567605527000/`
- Linux: `snapshots/green-linux-final-v1-1790973575519550600/`
- Single-source RED: `red-exact-green-support-v6.json`

Direct final executable replay from PowerShell:

```powershell
& 'D:/tmp/zr_vm/ssa-20261003-01a0fe2b/parser-observation/msvc/green-msvc-final-v1/fixture.exe'
wsl -e /usr/bin/env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/parser-observation/gcc/green-linux-final-v1/fixture
wsl -e /usr/bin/env LD_LIBRARY_PATH=/usr/lib/llvm-14/lib/clang/14.0.0/lib/linux ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/parser-observation/clang/shared-asan-final-v2/fixture
```

For a fresh compile, clone the corresponding snapshot configuration to a new
label and run `validation-control/actual_tu_driver.py` with native Python for
MSVC or explicit `wsl -e /usr/bin/python3` for GCC/Clang. The driver refuses to
overwrite an existing run directory. The Clang shared-runtime linker replay
script is `parser-observation/clang_shared_link.py`; similarly choose a new
output directory. Original/copy verification uses native Python, the snapshot
configuration and `--toolchain msvc --verify-snapshot`.

All source snapshots, RED/GREEN binaries, logs, receipt JSON and earlier
failure records are retained for independent root verification. This worker
does not stage or commit any file.

## Earlier attempts

The initial MSVC environment quoting failure and the later 180-second
`VsDevCmd` import timeout are tool failures. The corrected cache-environment
four-unit MSVC build compiled successfully but failed to link six genuine core
dependencies; an isolated LTCG attempt confirmed the same missing dependencies.
The seventeen-unit build resolves them without stubs.

Initial Linux attempts encountered broad-header DrvFS delays and Windows path
separators in an older snapshot configuration. The corrected four-unit Clang
sanitizer attempt compiled all units successfully but its GNU linker timed out
after 180 seconds. These attempts are preserved as supplementary evidence and
are not counted as semantic RED or GREEN.
