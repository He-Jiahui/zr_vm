---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_call_transfer.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_call_transfer.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_call_transfer.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_call_transfer.c
plan_sources:
  - docs/plans/ssa/04-frame-native/02-call-return-tail.md
tests:
  - tests/core/test_ssa_call_return_tail.c
doc_type: testing-guide
---

# Call-transfer packed-layout acceptance

## Scope

This is parser representation-plan validation. Selected physical endpoints
must bind unique logical occupants with matching actual class, type token,
whole byte size and alignment. Output spans and transfer kinds are metadata,
not memory-copy or ownership-execution authorization. `forwardReturn` is
conservatively false because return descriptors and commit proof are absent.
There are no production callers. Runtime argument transfer, return forwarding,
overlap staging, tail reuse and lifecycle integration remain open. The 04.02
plan leaf is unchanged.

All artifacts are under
`D:/tmp/zr_vm/ssa-20261003-01a0fe2b/call-transfer-layout-design`.
The shared read-only driver is
`D:/tmp/zr_vm/ssa-20261003-01a0fe2b/validation-control/actual_tu_driver.py`.
No existing build cache, CMake fragment, stage or commit is owned by this work.
The root-approved bounded contract is preserved as the D `design.md` resource
(SHA256 `88e296440ee0014ab34e6df68e0e430d22a1ab47866b3168922aff1da73e085d`).

## Actual RED

`gcc/red-v2` uses the original call-transfer implementation and an existing-API
fixture built with actual `LayoutPackedFrame` constructors. Source type token
7 and target token 9 have identical slot counts/sizes. The original planner
incorrectly returns true. All six actual TUs compiled and linked with exit 0;
fixture exit 1, Unity 4 tests / 1 failure / 0 ignored. The failed assertion is
`test_parser_transfer_rejects_actual_representation_mismatch` at snapshot
test line 71 (`Expected FALSE Was TRUE`). Recorded input hashes stayed stable.
This is the pre-production-edit RED, not a synthetic implementation.

The earlier `gcc/red-v1` attempt lacked Unity's include directory and timed out
the real Core ExecIR TU at 120 seconds. It is retained as a failed validation
setup attempt and is not semantic RED. The corrected closure includes real
Unity source and headers; no test-framework or FrameLayoutFree stub was used.

Root review then found a missing selected type proof: matching zero tokens
were accepted. The actual packed producer accepts zero tokens; it must not be
changed merely to impose this consumer's narrower requirement.
`msvc/red-zero-type-v3` constructs genuine two-value layouts, mutates both
selected descriptor tokens to zero, and runs against the prior implementation.
All 19 actual TUs compiled and linked with exit 0, fixture exit 1, Unity
8 tests / 1 failure / 0 ignored. Only the new zero-token control fails
(`Expected FALSE Was TRUE`, snapshot line 307); its 71 input hashes are stable.
The repair rejects zero only in selected endpoint rows. The earlier v2 GREEN
results remain prior-epoch evidence, and do not accept the corrected contract.

## Fixture coverage

The final fixture executes eight Unity tests, including the two unchanged Core
eligibility predicate tests. Parser assertions cover:

- different caller/callee logical IDs and physical ordering distinct from the
  logical order;
- scalar and 16-byte inline spans with actual offsets/alignment/type/hash
  snapshots;
- invalid type, class, size, alignment, offset/range, map, flags and enum casts;
- selected reused storage rejection with otherwise compatible scalar endpoints,
  and unselected reused storage acceptance;
- metadata classification for Unknown, Borrowed, Unique, Shared and GC;
- repeated target rejection and repeated source rejection when Unique;
- zero values on nonempty and genuinely empty constructed layouts, and malformed
  layout rejection even when the request has zero rows;
- late second-row failure preserving the previous whole plan, including pointer
  fields, count/flags/hash snapshots and all four arrays;
- selected tokens zero on either side or both sides, a late second-row zero
  failure with complete rollback, and unselected zero-token acceptance.

Selected type tokens must be nonzero opaque IDs: comparison checks equality, without claiming a
metadata-manager lookup or proving triviality. Unknown ownership remains
unknown. No destructor/retain/borrow-lifetime or runtime copying result is
claimed. Allocation overflow is checked in implementation; allocation-failure
injection and 32-bit runtime execution are not performed.

## GREEN and replay

MSVC 19.44 uses a real 19-TU closure. The additional Core verifier/state support
TUs satisfy unresolved references exported by actual `exec_ir.c`; they are
compiled sources, not borrowed objects. `msvc/green-msvc-final-v5` compiles all
19 TUs, links and runs with exit 0, Unity 8/0/0 and stable 71 input hashes.

Linux uses six actual TUs: fixture, parser frame layout, parser call transfer,
Core call-transfer predicates, Core ExecIR and Unity. GCC 11.4 enables
ASan/UBSan; Clang 14 uses ordinary code generation. Each owns separate output
directories. Both completed all six compile stages, link and fixture with
exit 0, Unity 8/0/0 and stable 48 input hashes. GCC reports no ASan/UBSan
findings. No Clang sanitizer result is claimed. Exact binary, receipt and
formal-file hashes are recorded in the D `artifact-manifest-v5.json` and
`scope-manifest-v5.json`. The appended registration gate and current five-file
freeze are bound separately by `artifact-manifest-v6.json` and
`scope-manifest-v6.json`; v5 evidence remains unchanged.

Replay the frozen Linux closure:

```powershell
wsl -e /usr/bin/python3 /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/validation-control/actual_tu_driver.py /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/call-transfer-layout-design/snapshots/green-final-v5-1790981304811057400/snapshot-config.json --toolchain gcc
wsl -e /usr/bin/python3 /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/validation-control/actual_tu_driver.py /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/call-transfer-layout-design/green-clang-v5-snapshot-config.json --toolchain clang
```

Replay the frozen native closure:

```powershell
python D:/tmp/zr_vm/ssa-20261003-01a0fe2b/validation-control/actual_tu_driver.py D:/tmp/zr_vm/ssa-20261003-01a0fe2b/call-transfer-layout-design/snapshots/green-msvc-final-v5-1790981309592124400/snapshot-config.json --toolchain msvc
```

The driver refuses an existing output label. For rebuild replay, copy the JSON
configuration to a new D file and select a unique `label`; retain the same
frozen snapshot paths. Existing binaries can be rerun directly. Root owns
independent replay and Git; these direct TU gates do not claim the full parent
build or full SSA acceptance.

## Exact existing registration gate

The D `registration_gate_v6.py` creates a standalone native CMake project at
`registration-v6-source` and builds into `registration-v6-build`. It copies
the frozen 71-file native source closure, then extracts byte-identical code
from the current existing declaration in `tests/cmake/ssa-tests.cmake` and
the `zr_vm_apply_common_test_settings` / `zr_vm_link_core` helpers in
`tests/CMakeLists.txt`. It consumes actual `ThirdPartyMacros.cmake`, Unity's
CMake registration and `tests/unity_config.h`.

The `zr_vm_core_static` dependency is the real 14-TU verifier/state/FrameLayoutFree
closure used by the direct native gate. It is an explicitly focused Core
library, not the full production library and not an empty substitute. The
exact existing executable declaration contributes its four actual TUs; Unity
contributes its actual source, for 19 compiled TUs. No repository CMake file
was edited. CMake performed actual compiler identification and ABI checks.

Configure and public target build both exit 0. `ctest -N` finds exactly one
matching test. The exact `ssa_call_return_tail` run passes 1/1 with
`--no-tests=error`, and its captured Unity output passes 8/0/0. All copied
source, declaration and helper origin hashes remain unchanged; fresh native
snapshot verification also reports 71 unchanged inputs. This accepts the
existing declaration/helper registration with disclosed focused dependencies;
it does not claim the full parent's dependency graph.

Commands and PID/start/exit receipts are in `registration-v6-receipt.json`.
The immutable copy `registration-v6-unity-evidence.log` preserves the observed
Unity output. `registration-v6-source-manifest.json` binds the full fragment,
extracted declaration/helpers, all source files and the focused Core list.

```powershell
cmake --build D:/tmp/zr_vm/ssa-20261003-01a0fe2b/call-transfer-layout-design/registration-v6-build --target zr_vm_ssa_call_return_tail_test -j 2
ctest --test-dir D:/tmp/zr_vm/ssa-20261003-01a0fe2b/call-transfer-layout-design/registration-v6-build -R '^ssa_call_return_tail$' --output-on-failure --no-tests=error
```

For an independent binary/CTest replay, copy `CTestTestfile.cmake` to new D
metadata and run CTest there, retaining its frozen executable path. This keeps
the original gate's `LastTest.log` intact. A fresh configure/build replay may
instead copy the source project to a new D directory and use a new build
directory under the same cached native toolchain environment.
