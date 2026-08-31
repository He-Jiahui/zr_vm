# Benchmark Measurement Scope Acceptance

## Scope

- Plan source: `docs/plans/benchmark/optimize/01-measurement-and-gates.md`, M1 Task 1
- Runner: `tests/performance/perf_runner.c`
- Scope contract: `tests/cmake/benchmark_measurement_contract.cmake`
- Suite integration: `tests/cmake/run_performance_suite.cmake`
- Report exporters: `scripts/benchmark/benchmark_reports_to_csv.py` and `scripts/benchmark/aggregate_benchmark_summary.py`
- Focused fixture and wrapper: `tests/benchmarks/test_benchmark_support.c` and `tests/cmake/run_benchmark_support_test.cmake`

This task documents and gates the existing one-process-per-sample runner. The
persistent child-process protocol and steady-state runtime/JIT reuse belong to
M1 Task 2 and are not implemented here.

## RED

The test fixture, structured CMake wrapper, and CTest registration were added
before the runner or reporting implementation. The initial Windows MSVC Debug
run used:

```powershell
cmake --build build/codex-msvc-benchmark-p0-debug --config Debug `
  --target zr_vm_benchmark_support_test zr_vm_perf_runner --parallel 8
ctest --test-dir build/codex-msvc-benchmark-p0-debug -C Debug `
  -R '^benchmark_support$' --output-on-failure
```

CTest failed `0/1` and the wrapper reported ten contract issues. The first two
prove the runner boundary directly:

```text
Unknown or incomplete option: --measurement-scope
perf runner accepted a request with all measurement contract options missing
```

The same RED also reported the missing measurement-contract module, missing CSV
fields, aggregate schema version 1, absent contract validity/issues, and an
ungated cross-runtime ratio for a record with no scope.

## Contract

Every runner invocation must explicitly provide non-empty
`measurement_scope` and `prepare_scope`, plus canonical `true` or `false` values
for `runtime_reused`, `compiler_reused`, and `jit_state_reused`. Missing fields,
empty scope strings, and alternate boolean spellings are rejected.

All current suite mappings use `measurement_scope=process_end_to_end` and set
the three reuse fields to `false`. Prepare boundaries are:

| implementations | prepare scope |
| --- | --- |
| C, Rust | `none` |
| ZR interp | `source_load_compile_in_measurement` |
| ZR binary | `bytecode_compile_before_measurement` |
| Python, Node.js, QuickJS, Lua | `script_load_in_measurement` |
| .NET, Java | `runtime_start_jit_in_measurement` |

The CMake contract module owns this mapping, validates its values, and returns
`null` for a ratio unless both measurement scopes are non-empty and identical.
The suite uses that gate for each `relative_to_c` and comparison-report ratio.

## Report Preservation

- `PASS`, `SKIP`, and `FAIL` implementation JSON records carry all five fields.
- Markdown rows show both scopes and each reuse state.
- Timing CSV preserves all five fields; comparison CSV includes
  `measurement_scope`.
- Timing CSV clears `speed_ratio_vs_c_baseline` unless the implementation and C
  records are legal, `PASS`, and same-scope. Comparison CSV rechecks the
  ZR-interp and target benchmark records and rejects an empty or inconsistent
  comparison-case scope.
- Aggregate schema version 2 validates every implementation record and writes
  `measurement_contract.valid`, `issues`, and `ratios_removed`.
- Aggregation applies the same gate to both implementation `relative_to_c` and
  comparison-report ratios, records every cleared JSON path, and never
  recalculates or reconstructs a ratio.
- A comparison record's own `measurement_scope` must be a non-empty string equal
  to the ZR-interp scope. Aggregate and CSV exporters clear the ratio when this
  condition fails; aggregation preserves the declared field instead of
  overwriting it from benchmark data.

The focused wrapper uses CMake `string(JSON)` for runner/aggregate assertions
and Python's `csv`/`json` modules for the CSV contract; it does not parse JSON
with regular expressions or substring matching.

## Review RED

The specification review found that the first implementation gated only the
aggregate comparison report. Before repairing it, the negative fixture was
expanded with both a missing-contract case and a fully legal but scope-mismatched
case. The next focused run failed `0/1` with four discriminating issues:

```text
CSV retained a ratio without compatible measurement contracts
aggregate did not record all removed implementation/comparison ratios
aggregate retained implementation.relative_to_c with a missing scope
aggregate retained implementation.relative_to_c with mismatched scopes
```

The repaired exporters passed the same fixture while retaining the legal C
self-ratio and clearing both timing/comparison ratios for the two negative cases.
The final adjacent focused run reported:

```text
1/2 Test #136: benchmark_registry ... Passed 0.34 sec
2/2 Test #137: benchmark_support  ... Passed 6.01 sec
100% tests passed, 0 tests failed out of 2
```

## Comparison Scope Isolation RED

A second specification review isolated the comparison record itself from the
otherwise valid benchmark records. The added case gives ZR interp and C complete
`PASS` contracts with `process_end_to_end`, but declares
`comparison_report.measurement_scope=persistent_runtime` and ratio `9.999`.
Before the aggregate fix, focused CTest failed `0/1` with exactly these issues:

```text
aggregate did not record all removed implementation/comparison ratios
aggregate retained ratio when only the comparison record scope mismatched
aggregate rewrote the comparison record measurement_scope
```

The repaired aggregate keeps `persistent_runtime` unchanged, replaces only the
ratio with JSON `null`, and appends that exact ratio path to `ratios_removed`.
The final focused pair passed: `benchmark_registry` in 0.34 seconds and
`benchmark_support` in 11.58 seconds (`2/2`, zero failures).

## Duplicate Identity RED

The final quality review added a fourth synthetic phase with two benchmark
cases sharing one name, two implementations sharing one name, and two
comparison cases sharing one name. A separate uniquely named case remains the
positive control. Before identity checks were implemented, focused CTest failed
`0/1`; the leading structural failures were:

```text
CSV did not fail closed for duplicate identities
aggregate accepted duplicate benchmark/comparison identities
aggregate did not report useful duplicate identity issues
aggregate did not record all duplicate-identity ratio removals
```

The final wrapper structurally asserts JSON `null` for every ambiguous
implementation/comparison ratio, blank CSV cells for the same records, and
retention of the unique control ratios. Aggregate contract issues identify
duplicate benchmark case, implementation, and comparison case names.

An independent clean MSVC build invoked only
`--target zr_vm_benchmark_support_test`. The dependency graph built
`zr_vm_perf_runner.exe` first, then the fixture, and its registered
`benchmark_support` CTest passed. Registration remains conditional on the Python
interpreter required by the wrapper.

The final combined focused run passed `benchmark_registry` in 0.27 seconds and
`benchmark_support` in 10.74 seconds (`2/2`, zero failures).

## Malformed Identity RED

The last identity audit added missing, empty, whitespace-only, and non-string
case/implementation names to both report types. A separate unique benchmark and
comparison case provides a complete legal positive control. Before strict name
validation, focused CTest failed `0/1` with two structured contract failures:

```text
CSV did not fail closed for malformed identities
aggregate did not fail closed for malformed identities
```

Names are now valid only when they are strings with a non-empty `strip()`
result. The exporters use that check only for identity validation and lookup;
they preserve every original declared value without trimming or self-healing.
All ratios in an invalid-identity case fail closed, while the unique complete
timing and comparison control ratios remain non-null/nonblank.

The final focused pair passed `benchmark_registry` in 1.01 seconds and
`benchmark_support` in 44.20 seconds (`2/2`, zero failures).

## GREEN

The focused support and adjacent registry checks passed on Windows MSVC 19.44:

```text
1/2 Test #136: benchmark_registry ... Passed 0.25 sec
2/2 Test #137: benchmark_support  ... Passed 14.42 sec
100% tests passed, 0 tests failed out of 2
```

A real smoke suite then ran only `numeric_loops` / C through
`run_performance_suite.cmake`. Optional external runtimes were excluded from
`PATH` so the check exercised suite wiring without unrelated toolchain setup.
The generated implementation record parsed as:

```text
schema_version: 2
status: PASS
measurement_scope: process_end_to_end
prepare_scope: none
runtime_reused: false
compiler_reused: false
jit_state_reused: false
relative_to_c: 1.000
```

CSV conversion and aggregation of this real suite output succeeded. The timing
CSV retained the five values, and the aggregate reported schema version 2,
`measurement_contract.valid=true`, and zero issues.

## Three-Toolchain Validation

The final runner and fixture sources were compiled directly in WSL with both
GCC and Clang using `-Wall -Wextra -Wpedantic`. Each binary pair then executed
the same structured CMake/Python wrapper against the final exporters:

```text
GCC:   Benchmark measurement contract PASS
Clang: Benchmark measurement contract PASS
```

The final Windows MSVC CMake/CTest run passed `benchmark_registry` and
`benchmark_support` as shown above. The direct WSL builds intentionally avoid
recompiling unrelated VM core sources while exercising the same runner, JSON,
CSV, aggregate, and ratio-gate behavior.
