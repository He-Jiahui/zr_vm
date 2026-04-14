# Steady-State Performance Convergence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce fresh steady-state benchmark evidence for `zr_interp` and `zr_binary`, implement one shared hot-path optimization, and verify the improvement with the same benchmark filters and WSL regression checks.

**Architecture:** Reuse the existing `performance_report` harness in `tests/cmake/run_performance_suite.cmake` with case and implementation filters so the work stays inside the repository's current benchmark world. Collect a fresh timing baseline and a matched profile/Callgrind pass, identify the lowest shared runtime hotspot across `zr_interp` and `zr_binary`, then implement the smallest shared fix in `zr_vm_core` or compiler quickening and rerun the same evidence path.

**Tech Stack:** WSL `gcc` Release build, Bash, CMake/CTest, Valgrind Callgrind counting mode, `zr_vm_core` runtime C code, `zr_vm_parser` compiler quickening C code, benchmark fixtures under `tests/benchmarks`.

---

### Task 1: Rebuild The Benchmark Tree And Capture A Fresh Baseline

**Files:**
- Read: `docs/superpowers/specs/2026-04-14-steady-state-performance-convergence-design.md`
- Read: `tests/benchmarks/README.md`
- Read: `tests/benchmarks/registry.cmake`
- Read: `tests/cmake/run_performance_suite.cmake`
- Read: `scripts/benchmark/build_benchmark_release.sh`
- Read: `scripts/benchmark/run_wsl_benchmarks_report_csv.sh`
- Verify output: `build/benchmark-gcc-release/CMakeCache.txt`
- Verify output: `build/benchmark-gcc-release/tests_generated/performance/benchmark_report.json`
- Verify output: `build/benchmark-gcc-release/tests_generated/performance/comparison_report.json`
- Verify output: `build/benchmark-gcc-release/tests_generated/performance/benchmark_speed_timings.csv`

- [ ] **Step 1: Rebuild the WSL gcc Release benchmark tree**

Run:

```bash
wsl bash -lc 'cd /mnt/e/Git/zr_vm && bash scripts/benchmark/build_benchmark_release.sh gcc'
```

Expected: configure/build succeeds for `build/benchmark-gcc-release`.

- [ ] **Step 2: Verify the benchmark tree registers `performance_report`**

Run:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && grep '^ZR_VM_REGISTER_PERFORMANCE_CTEST:BOOL=ON' build/benchmark-gcc-release/CMakeCache.txt"
```

Expected: one matching line proving the current benchmark tree can run `ctest -R '^performance_report$'`.

- [ ] **Step 3: Run a fresh timing + profile benchmark pass with the fixed case set**

Run:

```bash
wsl bash -lc 'cd /mnt/e/Git/zr_vm && export ZR_VM_TEST_TIER=core && export ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop,gc_fragment_baseline,gc_fragment_stress && export ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary,python,node,qjs,lua,rust,dotnet,java && bash scripts/benchmark/run_wsl_benchmarks_report_csv.sh build/benchmark-gcc-release'
```

Expected: the script completes successfully and refreshes both:

- `build/benchmark-gcc-release/tests_generated/performance/`
- `build/benchmark-gcc-release/tests_generated/performance_profile_callgrind/`

- [ ] **Step 4: Save the baseline evidence locations for later before/after comparison**

Record these files for the final comparison:

```text
build/benchmark-gcc-release/tests_generated/performance/benchmark_report.json
build/benchmark-gcc-release/tests_generated/performance/comparison_report.json
build/benchmark-gcc-release/tests_generated/performance/instruction_report.json
build/benchmark-gcc-release/tests_generated/performance/hotspot_report.json
build/benchmark-gcc-release/tests_generated/performance/benchmark_speed_timings.csv
build/benchmark-gcc-release/tests_generated/performance_profile_callgrind/hotspot_report.json
```

- [ ] **Step 5: Commit the plan and baseline-only documentation changes if any docs were edited**

Run:

```bash
git status --short
```

Expected: no production code changes yet; benchmark outputs may be regenerated under `build/`.

### Task 2: Diagnose The Lowest Shared Runtime Hotspot

**Files:**
- Read: `build/benchmark-gcc-release/tests_generated/performance/benchmark_report.md`
- Read: `build/benchmark-gcc-release/tests_generated/performance/comparison_report.md`
- Read: `build/benchmark-gcc-release/tests_generated/performance/instruction_report.md`
- Read: `build/benchmark-gcc-release/tests_generated/performance/hotspot_report.md`
- Read: `build/benchmark-gcc-release/tests_generated/performance_profile_callgrind/hotspot_report.md`
- Read candidate implementations:
  - `zr_vm_core/src/zr_vm_core/execution_dispatch.c`
  - `zr_vm_core/src/zr_vm_core/function.c`
  - `zr_vm_core/src/zr_vm_core/io_runtime.c`
  - `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`

- [ ] **Step 1: Summarize the worst `zr_interp` and `zr_binary` gaps from the fresh timing reports**

Run:

```bash
wsl bash -lc 'cd /mnt/e/Git/zr_vm && python3 - <<\"PY\"
import json, pathlib
report = pathlib.Path(\"build/benchmark-gcc-release/tests_generated/performance/benchmark_report.json\")
data = json.loads(report.read_text(encoding=\"utf-8\"))
rows = [r for r in data.get(\"results\", []) if r.get(\"implementation\") in {\"zr_interp\", \"zr_binary\"} and r.get(\"status\") == \"PASS\"]
rows.sort(key=lambda r: (r.get(\"case\", \"\"), r.get(\"implementation\", \"\")))
for row in rows:
    print(f\"{row['case']} {row['implementation']} mean_ms={row.get('mean_wall_ms')} rel_c={row.get('relative_to_c')}\")
PY'
```

Expected: one line per tracked case / implementation with the current mean wall time and `relative_to_c`.

- [ ] **Step 2: Extract the hottest shared helper families from the profile artifacts**

Run:

```bash
wsl bash -lc 'cd /mnt/e/Git/zr_vm && python3 - <<\"PY\"
from pathlib import Path
for path in [
    Path(\"build/benchmark-gcc-release/tests_generated/performance/instruction_report.md\"),
    Path(\"build/benchmark-gcc-release/tests_generated/performance_profile_callgrind/hotspot_report.md\"),
]:
    print(f\"=== {path} ===\")
    text = path.read_text(encoding=\"utf-8\")
    for line in text.splitlines()[:160]:
        print(line)
PY'
```

Expected: enough profile output to map the hottest cases onto one or more shared runtime layers.

- [ ] **Step 3: Enumerate lower-layer candidates before editing code**

Write down the candidate layers that could explain the hotspot:

```text
dispatch loop / opcode dispatch
VM call preparation / frame setup
stack or value copy helpers
object or indexed access helpers
container runtime helpers
string runtime helpers
compiler quickening feeding the same shared runtime helper
```

Expected: exactly one primary hotspot hypothesis selected for the first optimization loop.

- [ ] **Step 4: Reproduce the hotspot with the smallest useful benchmark subset**

Run the same harness with only the cases that expose the chosen hotspot. Example command shape:

```bash
wsl bash -lc 'cd /mnt/e/Git/zr_vm && export ZR_VM_TEST_TIER=core && export ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops && export ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary,python,node,qjs,lua,rust,dotnet,java && ctest -R "^performance_report$" --test-dir build/benchmark-gcc-release --output-on-failure'
```

Expected: a focused reproduction command that will be reused after the fix.

### Task 3: Implement The First Shared Hot-Path Fix

**Files:**
- Modify one or more shared hotspot files chosen from:
  - `zr_vm_core/src/zr_vm_core/execution_dispatch.c`
  - `zr_vm_core/src/zr_vm_core/function.c`
  - `zr_vm_core/src/zr_vm_core/io_runtime.c`
  - `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`
- Verify with:
  - `build/benchmark-gcc-release/tests_generated/performance/benchmark_report.json`
  - `build/benchmark-gcc-release/tests_generated/performance_profile_callgrind/hotspot_report.json`

- [ ] **Step 1: Read the exact hotspot implementation and compare it against nearby fast paths**

Run:

```bash
git grep -n "ZrCore_" -- zr_vm_core/src/zr_vm_core/execution_dispatch.c zr_vm_core/src/zr_vm_core/function.c zr_vm_core/src/zr_vm_core/io_runtime.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
```

Expected: enough context to isolate the smallest shared change instead of broad refactoring.

- [ ] **Step 2: Apply the minimal shared fix with `apply_patch`**

Edit only the hotspot path identified in Task 2. The change must remove unnecessary steady-state overhead in the shared path and must not add benchmark-case special handling.

- [ ] **Step 3: Rebuild the benchmark tree after the code change**

Run:

```bash
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/benchmark-gcc-release --parallel 8'
```

Expected: the Release benchmark build succeeds.

- [ ] **Step 4: Rerun the focused benchmark subset first**

Reuse the focused command from Task 2 Step 4.

Expected: the targeted hotspot case or cases improve, or the hypothesis is rejected and the next hypothesis must be chosen before further edits.

- [ ] **Step 5: Rerun the full tracked timing + profile benchmark pass**

Reuse the full command from Task 1 Step 3.

Expected: fresh reports that can be compared against the baseline with the same filters.

### Task 4: Validate The Result And Protect Against Regressions

**Files:**
- Verify: `build/benchmark-gcc-release/tests_generated/performance/benchmark_report.json`
- Verify: `build/benchmark-gcc-release/tests_generated/performance/comparison_report.json`
- Verify: `build/benchmark-gcc-release/tests_generated/performance/benchmark_speed_timings.csv`
- Verify touched code with:
  - `build/codex-wsl-gcc-debug`
  - `build/codex-wsl-clang-debug`

- [ ] **Step 1: Compare the before/after benchmark rows for `zr_interp` and `zr_binary`**

Run:

```bash
wsl bash -lc 'cd /mnt/e/Git/zr_vm && python3 - <<\"PY\"
import json, pathlib
report = pathlib.Path(\"build/benchmark-gcc-release/tests_generated/performance/benchmark_report.json\")
data = json.loads(report.read_text(encoding=\"utf-8\"))
for row in data.get(\"results\", []):
    if row.get(\"implementation\") in {\"zr_interp\", \"zr_binary\"}:
        print(f\"{row['case']} {row['implementation']} mean_ms={row.get('mean_wall_ms')} rel_c={row.get('relative_to_c')}\")
PY'
```

Expected: a final row set ready to compare to the Task 1 baseline snapshot.

- [ ] **Step 2: Re-run WSL regression validation for touched shared code**

Run:

```bash
powershell -ExecutionPolicy Bypass -File .\\.codex\\skills\\zr-vm-dev\\scripts\\validate-matrix.ps1 -Jobs 8 -SkipWindows
```

Expected: WSL gcc/clang configure, build, `ctest`, and hello-world smoke all succeed.

- [ ] **Step 3: Write the acceptance summary with hotspot, fix, and before/after numbers**

Create or update an acceptance note under:

```text
tests/acceptance/2026-04-14-steady-state-performance-convergence.md
```

Expected contents:

- benchmark command lines
- case and implementation filters
- primary hotspot summary
- changed files
- before/after numbers for `zr_interp` and `zr_binary`
- any residual regressions or skipped areas

- [ ] **Step 4: Commit only the plan, code, and acceptance files for this performance round**

Run:

```bash
git status --short
```

Expected: the final diff is limited to the performance work you intentionally changed; unrelated dirty files remain untouched.
