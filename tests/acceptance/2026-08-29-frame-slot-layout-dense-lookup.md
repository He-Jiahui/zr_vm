# Dense Frame-Slot Layout Lookup Acceptance

## Scope

- P0 overall production change: the existing workspace diff in `zr_vm_core/src/zr_vm_core/function.c`
- Focused regression/characterization test: `tests/core/test_frame_slot_layout_lookup.c`
- Test registration: `tests/CMakeLists.txt`
- Module documentation: `docs/core-runtime/inline-type-layout-and-byte-stack.md`
- Plan source: user request on 2026-08-29 to audit and optimize VM performance against Lua and C#
- The follow-up also adds the validated direct VALUE-slot address slice in `function.h`, `function.c`, and `execution_inline_frame.c`; its performance gate is intentionally separate from the accepted dense-lookup result.

## Defect

`ZrCore_Function_FindFrameSlotLayout` linearly scanned `frameSlotLayouts` for every logical frame-slot access. Compiled functions emit a dense, stack-slot-ordered layout whose length matches `stackSize`, so integer and branch instructions repeatedly scanned metadata that was already indexed by the requested slot.

The pre-change scale-1 `numeric_loops` Callgrind run recorded:

- 774,972,233 total instruction reads;
- 348,952,794 instruction reads, or 45.03%, exclusively in `ZrCore_Function_FindFrameSlotLayout`;
- about 2,058,173 calls from the execution path into the layout lookup.

## Baseline And RED

The historical test-first ordering cannot be proven from the current dirty workspace because the original RED source tree and build artifacts were not retained. A fresh behavior-discriminating RED was therefore reconstructed on 2026-08-29 from an immutable `git archive HEAD` snapshot. The current focused test was overlaid on that snapshot while `function.c` remained at `HEAD`, so both dense and sparse requests used the old linear scan.

The RED binary was compiled with WSL Clang 14 in a native `/tmp` snapshot. Both timing iteration counts were overridden to 20,000 so the old O(n) implementation remained practical to execute; the shipped defaults were not changed. Five functional boundaries passed and only the complexity distinction failed:

```text
frame-slot lookup median: dense=114319 ticks/20000 iterations, sparse=114539 ticks/20000 iterations
test_dense_frame_slot_lookup_is_constant_time:FAIL:dense frame-slot lookup must avoid the linear sparse-layout scan
6 Tests 1 Failures 0 Ignored
```

The snapshot compiled only `function.c`, the Unity test, and Unity itself with `-ffunction-sections` plus `--gc-sections`. This exercises the real public implementation while discarding unrelated core functions, avoiding a rebuild of the very large interpreter dispatch translation unit. The effective RED compiler inputs were:

```bash
clang -std=c11 -g -ffunction-sections -fdata-sections \
  -DZR_DEBUG -DZR_PLATFORM_UNIX -DZR_LIBRARY_TYPE_SHARED -DUTF8PROC_STATIC -Dzr_vm_core \
  -DZR_TEST_DENSE_LOOKUP_ITERATIONS=20000u -DZR_TEST_SPARSE_LOOKUP_ITERATIONS=20000u \
  -I. -Izr_vm_core/include -Izr_vm_core/src/zr_vm_core -Izr_vm_common/include \
  -Izr_vm_library/include -Itests/third_party/zr_unity/Unity/src \
  zr_vm_core/src/zr_vm_core/function.c tests/core/test_frame_slot_layout_lookup.c \
  tests/third_party/zr_unity/Unity/src/unity.c -Wl,--gc-sections -o frame-slot-red-clang
```

This fresh RED proves that the current test distinguishes the old and new implementations. It does not retroactively prove the historical ordering of the original uncommitted optimization.

## GREEN

`ZrCore_Function_FindFrameSlotLayout` now checks the canonical indexed entry first and returns it only when `frameSlotLayouts[stackSlot].stackSlot == stackSlot`. Sparse, reordered, and out-of-range layouts retain the original linear fallback.

The current follow-up additionally marks complete, non-alias VALUE slots when
the compiler or loader finalizes validated dense metadata with
`ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE`. The derived bit is masked out of
serialized output and recomputed after load validation, while runtime frame
initialization remains read-only. The execution getter checks the canonical
indexed record before the generic layout lookup and calculates `frameBase +
byteOffset` through a fail-closed helper. Only a failed direct guard enters the
generic lookup and checked place resolution. Unvalidated, aliased,
inline-struct, or malformed slots retain that fallback, and relocation callers
must pass their refreshed frame base. Append-only profile helper IDs distinguish
direct from checked access without changing existing IDs.

After copying only the current `function.c` into the same immutable snapshot, the same source set and iteration overrides passed with both Clang 14 and GCC:

```text
Clang: frame-slot lookup median: dense=125 ticks/20000 iterations, sparse=150894 ticks/20000 iterations
Clang: 6 Tests 0 Failures 0 Ignored
GCC:   frame-slot lookup median: dense=154 ticks/20000 iterations, sparse=82644 ticks/20000 iterations
GCC:   6 Tests 0 Failures 0 Ignored
```

The Windows MSVC Debug CMake target used the shipped default iteration counts and also passed:

```text
frame-slot lookup median: dense=19 ticks/2000000 iterations, sparse=59 ticks/20000 iterations
test_dense_frame_slot_lookup_is_constant_time:PASS
6 Tests 0 Failures 0 Ignored
1/1 Test #2: frame_slot_layout_lookup ... Passed 0.64 sec
```

## Test Inventory

The focused test now explicitly covers:

- canonical dense hit: the indexed lookup returns the exact `frameSlotLayouts[stackSlot]` entry;
- in-range reordered layout: an indexed mismatch falls back to the linear scan and returns the exact matching entry;
- sparse out-of-range success: a `stackSlot >= frameSlotLayoutLength` request remains discoverable by the linear scan;
- out-of-range miss: no matching layout returns `NULL`;
- null inputs: both a null function and a function with null layouts return `NULL`;
- direct VALUE-slot address: validated byte offsets resolve relative to both original and relocated frame bases;
- direct VALUE frame place: the public place helper accepts a canonical direct slot while retaining null, alias, malformed, and bounds-check failures;
- direct VALUE inline-member guard: finalized direct slots skip the probe, while untrusted and inline-struct layouts retain it;
- finalization boundaries: alias, misaligned, out-of-bounds, non-canonical, non-power-of-two-aligned, and length-mismatched layouts reject or clear stale direct bits, while frame initialization preserves a prevalidated bit without mutating shared metadata;
- helper profile contract: numeric IDs 9 and 10 remain `frame_value_slot_direct` and `frame_value_slot_checked`, and a direct hit followed by an untrusted miss records one event in each counter;
- performance distinction: each sample uses at least 2,000,000 dense lookups and about 20,000 sparse lookups, prints both ticks and iteration counts, and compares median per-lookup costs by overflow-checked cross multiplication after warm-up.

The five-sample median preserves the O(1)-versus-linear distinction while reducing the chance that a single scheduler interruption causes a false failure. Separate iteration counts keep dense timing measurable under coarse `clock()` resolution, including MSVC, without making the sparse linear scan unreasonably long. It does not add a production test hook.

## Performance Evidence

The values in this section are retained summary observations from the original optimization work. The repository does not retain the per-run performance-report logs, raw timing sample files, or Callgrind output used to derive them. Consequently, these numbers cannot be rechecked from a repository artifact path and must not be treated as path-verifiable raw evidence.

All wall-time samples used the same GCC 11.4 Release build, binary execution mode, `taskset -c 2`, one process per sample, and unchanged checksums.

| Case | Before median | After median | Improvement |
|---|---:|---:|---:|
| `numeric_loops`, core, 5 measured runs | 2,941.107 ms | 1,889.751 ms | 35.75% |
| `dispatch_loops`, core, 3 measured runs after change | 17,963.016 ms | 8,798.310 ms | 51.02% |

The post-change scale-1 Callgrind run recorded 487,764,858 instruction reads, a 37.06% reduction. `ZrCore_Function_FindFrameSlotLayout` no longer appeared as the leading exclusive hotspot. The next measured hotspot is `ZrCore_Stack_MakeFramePlace` at 127,606,726 instruction reads, or 26.16%.

### Direct VALUE-Slot Follow-Up (2026-08-30)

The current work retains its raw ext4 artifacts under the keyed GCC 11.4 Release
build's `tests_generated` directory. The before profile is archived as
`performance_profile_callgrind_frame_numeric_before_direct_lookup_fix`; the
accepted deterministic profile is
`performance_profile_callgrind_frame_numeric_state_profile_final`. The stable
environment fingerprint is
`1b5197ed0de0b933c1ad8d790d23ac05331eba7da730acf9f1245d6befd8c1de`,
CPU 2 is affinity-isolated, and the source identity did not change during either
run.

| Metric | Before | Final | Decision |
|---|---:|---:|---|
| Callgrind total Ir | 820,818,823 | 722,029,136 | 12.04% reduction |
| `ZrCore_Stack_MakeFramePlace` exclusive Ir | historical gate: 26.16% | 14,352 (0.00199%) | `<5%` passed |
| `ZrCore_Function_FindFrameSlotLayout` exclusive Ir | 123,438,789 (15.04%) | 5,424 (0.00075%) | generic lookup left the numeric hot path |
| direct / checked helper count | unavailable | 10,006,568 / 1 | expected canonical/fallback split |

Two deliberately rejected intermediate profiles are retained alongside the
accepted profile. Calling the public direct helper out of line raised total Ir
to 1,026,503,332; inlining it while using the TLS profile helper still left
`__tls_get_addr` at 99,083,052 Ir. The final getter instead reads the active
profile runtime from `state->global` once.

The paired `process_end_to_end` timing archive is
`performance_frame_numeric_state_profile_final`. All 20-sample series are
`UNSTABLE` and `gate_eligible=false`: C median `21.171 ms`, CV `17.64%`; ZR
interp median `635.988 ms`, CV `22.48%`; ZR binary median `703.957 ms`, CV
`15.27%`. These samples do not satisfy the numeric `+10%` gate even though the
interpreter median moved in the favorable direction.

### Dispatch Direct-Slot Follow-Up (2026-08-30)

`object_field_hot` exposed the public VALUE-slot getter itself as the next
exclusive hotspot. The dispatch loop now calls a narrow out-of-line helper that
uses the loop-cached `profileRuntime`/`recordHelpers` values and accepts only a
canonical, self-consistent layout carrying `DIRECT_VALUE`. Every failed guard
falls back to `execution_inline_frame_get_value_slot`; the helper does not cache
an address or weaken the existing metadata validation.

With the same ext4 GCC 11.4 Release build, scale-1 profile input, and checksum
`623146080`, Callgrind total Ir fell from `205,647,828` to `159,970,049`
(`-22.21%`). The old public getter accounted for `79,975,168` exclusive Ir;
the new dispatch helper accounts for `31,418,398`. Profile counts remain
`1,686,066` direct and `1` checked access, proving that the optimization removes
dispatch/profile overhead rather than changing operand selection.

The paired process measurement remains diagnostic: ZR interp median moved from
`199.464 ms` to `160.117 ms` (`-19.73%`), but the final 17-sample CV is
`12.69%` and `gate_eligible=false`. This does not accept the object wall-time
gate.

A second guard uses the same finalized metadata before cached member-name
resolution. A canonical direct VALUE receiver cannot contain inline struct or
union fields, so get/set/initialize probes return false immediately; any slot
without the trusted bit retains the original lookup and fail-closed behavior.
Its RED test first failed to link on the missing predicate, then passed after
implementation. The comparable Callgrind total fell again from `159,970,049`
to `126,716,379 Ir` (`-20.79%` for this stage, `-38.38%` from the original
`205,647,828`), with checksum and direct/checked counts unchanged.
The raw Callgrind, stdout/stderr, and profile JSON are retained in
`tests_generated/performance_profile_callgrind_object_inline_probe_final` under
the keyed Release build.

The new core process median is `136.382 ms`, a diagnostic `-31.62%` from the
original `199.464 ms`, but its 17-sample CV is `16.52%`; the wall-time gate
therefore remains open.

## Compatibility Boundary

- The public `SZrFunction` layout and serialized `.zro` format are unchanged.
- The optimization does not assume every layout is dense; it validates the indexed entry and retains the previous scan.
- Valid `frameSlotLayouts` metadata contains no duplicate `stackSlot`: the compiler builder creates the unique canonical `layouts[slot]` entry and IO validation rejects duplicates. Hand-built `SZrFunction` values must satisfy the same contract.
- Canonical validated VALUE reads use the direct address path; fallback VALUE reads, inline-struct storage, ownership cleanup, and all unresolved or malformed layouts retain the existing checked place-making path.
- Mainstream-runtime parity is not accepted by this change. The post-change numeric stress median remains 30,591.891 ms versus Lua 3,129.862 ms, QuickJS 4,665.153 ms, and .NET 1,579.317 ms under the current process-level harness.

## Standard Regression Evidence

The WSL GCC Debug graph was regenerated with `BUILD_TESTS=ON`, the new target was built through CMake, and its standalone CTest registration passed:

```text
1/1 Test #4: frame_slot_layout_lookup ... Passed
100% tests passed, 0 tests failed out of 1
```

Adjacent executables also passed with no failures:

- `zr_vm_type_layout_inline_copy_test`: 40/40;
- `zr_vm_execution_add_stack_relocation_test`: 20/20;
- `zr_vm_precall_frame_slot_reset_test`: 16/16;
- `zr_vm_postcall_fast_paths_test`: 3/3.

The first full regenerate of the existing `/mnt/e` Debug tree took 1,086.4 seconds; a subsequent cached regenerate took 141.3 seconds. This is recorded as benchmark-infrastructure debt rather than VM execution time.

## Current Focused Validation

- Original dense-lookup RED/Green: WSL Clang expected RED `5/6`, then Clang/GCC/MSVC GREEN `6/6`.
- Current direct-slot follow-up: ext4 WSL Clang 14 Debug and GCC 11.4 Release builds each pass `14/14`; both focused CTest entries pass `1/1`.
- Fresh Windows MSVC 19.44 CMake Debug build: current GREEN `14/14`; focused CTest `1/1`.
- Fresh ext4 Clang 14 ASan Debug: direct-slot binary `14/14` with leak
  detection enabled; the adjacent full GC binary passes `67/67` with no
  ASan/LeakSanitizer report.
- The dispatch follow-up also passes member access `102/102`, shape `3/3`,
  frame slot `14/14`, and GC `67/67` under GCC Release, Clang Debug, and MSVC
  Debug. The same set passes Clang ASan/LeakSanitizer with leak detection.
- Adjacent MSVC binaries: relocation `20/20`, precall reset `16/16`, numeric fast paths `11/11`; `zr_vm_parser_shared` also compiled and linked successfully.

The current WSL source snapshot and keyed Release artifacts are retained under
`/tmp/zr_vm-benchmark-snapshot-20260830` and
`/tmp/zr_vm-benchmark-cache-20260830`. The Clang validation uses a separate
ext4 Debug build at `/tmp/zr_vm-benchmark-clang-debug-20260830`; changed source
hashes match the workspace. The existing `/mnt/e` Clang CMake tree was not used
because regeneration there repeatedly stalls in mounted-filesystem I/O.

## Acceptance Decision

Dense-lookup acceptance requires the original six Unity cases to pass on both
WSL compilers and the MSVC CMake target, including the exact-entry and null/miss
boundaries above. The direct-slot follow-up requires all fourteen current cases to
pass. Its deterministic Callgrind `<5%` frame-place gate is accepted from the
retained raw artifact; the numeric `+10%` wall-time gate remains open because
all final timing series are unstable. The fresh snapshot RED must fail only the
complexity case against `HEAD`. Historical summaries without raw artifacts
remain narrative evidence only.
