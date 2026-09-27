---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_ranges.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_ranges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_gvn.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_ranges.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
tests:
  - tests/parser/test_ssa_gvn_range.c
doc_type: acceptance-record
status: partial
---

# SSA 02.02: fail-closed bounds proof for mutable lengths

## Scope and baseline

`ZrParser_ExecIr_RangeProvesBounds` accepts two caller-supplied range facts.
It already rejected mutable length metadata attached to the index fact and
rejected malformed intervals on insertion into a fact container, but the
direct query failed to check `length.lengthMutable` or inverted bounds on
either operand. A stale mutable length or `lower > upper` therefore could be
used as a positive bounds proof even though a check must remain in place.

## Test inventory and RED evidence

- `test_range_facts_require_both_bounds` now sets `length.lengthMutable`
  after a valid proof. Before the fix, the direct proof still returned true:
  GCC's standalone `zr_vm_ssa_gvn_range_test` exited 1 at line 93.
- `test_bounds_check_api_is_conservative` verifies the public proof-only
  wrapper refuses mutable length and reports no execution error (no proof is
  a valid conservative result).
- `test_direct_bounds_proof_rejects_inverted_intervals` passes index and
  length facts directly with `lower > upper`, bypassing container insertion;
  before the second fix, the first invalid index interval returned true and
  the GCC standalone test exited 1 at line 173. Both invalid intervals now
  fail closed through the direct proof and bounds-check API.
- Existing valid nonnegative index/length, missing lower bound, arithmetic
  overflow, stale-generation fact-insertion rejection, and later same-block
  GVN tests remain in the same focused executable.

## Validation and boundary (2026-09-28)

The changed standalone range test and analysis object were rebuilt in three
independent Debug caches under `D:\tmp\zr_vm`: `ssa-gcc-debug` (WSL GCC
11.4), `ssa-clang-debug` (WSL Clang 14), and `ssa-msvc-debug` (MSVC 19.44).
The exact focused configure/build/test commands were:

```text
WSL: cmake -S /mnt/e/Git/zr_vm -B /mnt/d/tmp/zr_vm/ssa-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DBUILD_TESTS=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
WSL: cmake -S /mnt/e/Git/zr_vm -B /mnt/d/tmp/zr_vm/ssa-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBUILD_TESTS=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
WSL: cmake --build /mnt/d/tmp/zr_vm/ssa-gcc-debug --target zr_vm_ssa_gvn_range_test zr_vm_ssa_pass_manager_scalar_test -j 4
WSL: ctest --test-dir /mnt/d/tmp/zr_vm/ssa-gcc-debug -R '^(ssa_gvn_range|ssa_pass_manager_scalar)$' --output-on-failure --no-tests=error
WSL: cmake --build /mnt/d/tmp/zr_vm/ssa-clang-debug --target zr_vm_ssa_gvn_range_test zr_vm_ssa_pass_manager_scalar_test -j 4
WSL: ctest --test-dir /mnt/d/tmp/zr_vm/ssa-clang-debug -R '^(ssa_gvn_range|ssa_pass_manager_scalar)$' --output-on-failure --no-tests=error
MSVC: powershell -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cmake -S E:\Git\zr_vm -B D:\tmp\zr_vm\ssa-msvc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
MSVC: powershell -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cmake --build D:\tmp\zr_vm\ssa-msvc-debug --target zr_vm_ssa_gvn_range_test zr_vm_ssa_pass_manager_scalar_test --parallel 4
MSVC: ctest --test-dir D:\tmp\zr_vm\ssa-msvc-debug -R '^(ssa_gvn_range|ssa_pass_manager_scalar)$' --output-on-failure --no-tests=error
```

Each toolchain passed 2/2. `python scripts/validate_wiki.py` passed (116
Markdown files, 115 manifest pages, 646 local links). An independent read-only
review found and corrected a
documentation overclaim about detecting two equally stale generations; it
found no other issue in the scoped diff.

The query does not allocate, own, mutate, or delete ExecIR instructions; no
lease/cleanup state changes are introduced.
The no-proof path leaves guards to callers. This is not proof of actual
check elimination, edge-sensitive propagation, widening, or the complete
02.02 exit gate; no production backend differential is claimed.
The two-fact direct query has no active-generation argument. Callers must
check both facts against their current analysis generation before treating a
positive result as usable; two equally stale facts cannot be detected here.
