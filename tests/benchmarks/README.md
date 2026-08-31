# ZR VM Benchmarks

`tests/benchmarks` is the single source of truth for the cross-language
benchmark suite driven by `tests/cmake/run_performance_suite.cmake`.

**CTest:** `performance_report` is registered only when
`ZR_VM_REGISTER_PERFORMANCE_CTEST=ON` (default `OFF` so default `ctest` stays
fast). **Without that flag**, run the same suite via the build target
`run_performance_suite` or invoke `cmake -P tests/cmake/run_performance_suite.cmake`
with the same `-DCLI_EXE=...` arguments as in `tests/CMakeLists.txt`.

**Light checks:** `zr_vm_benchmark_registry_test` / CTest `benchmark_registry`
validates the registry layout. `zr_vm_benchmark_support_test` / CTest
`benchmark_support` executes the real performance runner, validates its JSON
with CMake `string(JSON)`, and checks CSV/aggregate contract preservation.
Building only `zr_vm_benchmark_support_test` also builds its required
`zr_vm_perf_runner` dependency. CTest registers `benchmark_support` only when a
Python interpreter is available for the structured exporter checks.
`benchmark_statistics_python`, `benchmark_execution_plan_python`,
`benchmark_task3_suite_contract`, and
`benchmark_task3_report_consumers_python` cover the Task 3 statistical and
orchestration contracts without running the full cross-language matrix.

## Layout

- `registry.cmake`
  Registers case metadata, tier membership, supported implementations, pass
  banners, tier scales, and per-tier checksum contracts.
- `../cmake/benchmark_measurement_contract.cmake`
  Owns implementation-to-scope mapping, contract validation, and the
  same-nonempty-scope gate used for cross-runtime ratios.
- `cases/<case>/zr/`
  ZR project fixture: `benchmark_<case>.zrp` plus `src/main.zr`.
- `cases/<case>/python/`
  Python wrapper that dispatches into the shared Python benchmark runner.
- `cases/<case>/node/`
  Node.js wrapper that dispatches into the shared Node benchmark runner.
- `cases/<case>/qjs/`
  QuickJS wrapper that dispatches into the shared QuickJS benchmark runner.
- `cases/<case>/lua/`
  Lua wrapper that dispatches into the shared Lua benchmark runner.
- `cases/<case>/c/`
  Native C descriptor consumed by `zr_vm_native_benchmark_runner`.
- `cases/<case>/rust/`
  Rust module wrapper consumed by `rust_runner`.
- `cases/<case>/dotnet/`
  C# wrapper consumed by `dotnet_runner`.
- `cases/<case>/java/`
  Java wrapper consumed by `java_runner`.
- `native_runner/`
  Prebuilt native C baseline executable registered from `tests/CMakeLists.txt`.
- `rust_runner/`
  Cargo-based shared Rust benchmark runner.
- `dotnet_runner/`
  Shared .NET benchmark runner.
- `java_runner/`
  Shared Java benchmark runner.
- `common/python/`
  Shared Python benchmark algorithms.
- `common/node/`
  Shared Node.js benchmark algorithms.
- `common/qjs/`
  Shared QuickJS benchmark algorithms.
- `common/lua/`
  Shared Lua benchmark algorithms.

## Contract

Every benchmark implementation must satisfy the same correctness contract for a
given case and tier:

- success exit code
- stdout contains exactly two lines
- line 1 is the registered `BENCH_<CASE>_PASS` banner
- line 2 is the deterministic checksum for that tier

Non-ZR implementations accept `--tier smoke|core|stress|profile` directly.

ZR fixtures are copied into `build/.../tests_generated/performance_suite/`
before execution, and suite preparation rewrites `src/bench_config.zr` to the
requested scale. This keeps the repo fixture immutable during the run while
avoiding runtime dependencies on generated CLI argument parsing support.

## Tier Policy

- `smoke`
  Runs `numeric_loops`, `sort_array`, `prime_trial_division`
- `core`
  Runs all registered core-tier benchmark cases
- `stress`
  Runs all registered stress-tier benchmark cases at the largest scale
- `profile`
  Runs profile-enabled cases with smaller per-case scales for opcode counters,
  hotspot capture, and slow-path analysis.

GC pressure work uses a paired benchmark shape:

- `gc_fragment_baseline`
  Same allocation and survivor workload without explicit `system.gc.collect(...)`
  forcing, used as the control.
- `gc_fragment_stress`
  Same logical workload with explicit minor/full collections in the ZR fixture,
  used to estimate high-pressure GC overhead against the baseline.

Scale is fixed in `registry.cmake`:

- `smoke = 1`
- `core = 4`
- `stress = 16`
- `profile = registry controlled per case`

## Sampling And Execution Order

The suite creates a flat `{case, implementation}` execution plan after applying
the optional case and implementation filters. It shuffles that filtered list
with the versioned Fisher-Yates/SplitMix64 planner. `ZR_VM_PERF_SEED` selects an
unsigned 64-bit seed and defaults to `0`; the exact plan is written to
`execution_plan.json` and embedded in `benchmark_report.json`.

Steady scope defaults to five warmups and ten initial samples. Process scope
keeps its existing tier defaults. Non-profile rows calibrate repetitions using
the registry's per-case `MIN_SAMPLE_MS`, may add at most ten samples while CV is
above `0.05`, and never exceed twenty total samples. An initial override of 12
therefore allows eight extras, 20 allows none, and values above 20 are rejected.

Profile scope is forced to zero warmups, one sample, one repetition, no
calibration minimum, no adaptive samples, and `--profile`. Profile and
`UNSTABLE` rows remain diagnostic records but are never ratio or gate eligible.

## Callgrind (profile tier)

Representative workloads run Callgrind after the ZR interp measurement. To use **instruction counting mode** (disable cache and branch simulation in Callgrind; faster than full simulation while keeping Ir / call-graph data):

- Set environment variable `ZR_VM_PERF_CALLGRIND_COUNTING=1` (also accepts `yes`, `on`, `true`) before running `ctest`/the performance suite.
- When enabled, the harness passes `--cache-sim=no --branch-sim=no` to `valgrind --tool=callgrind` (see `valgrind --tool=callgrind --help`).
- Reports record the mode in `benchmark_report.md/json` and `hotspot_report.md/json` (`callgrind_counting_mode`).

Example (WSL):

```bash
export ZR_VM_TEST_TIER=profile
export ZR_VM_PERF_CALLGRIND_COUNTING=1
ctest -R '^performance_report$' --test-dir build/benchmark-gcc-release --output-on-failure
```

## Isolate implementations (diagnosis)

To run only selected benchmark implementation ids (comma-separated), set **`ZR_VM_PERF_ONLY_IMPLEMENTATIONS`** before `ctest`. Matching ids are the registry keys, for example: `c`, `zr_interp`, `zr_binary`, `python`, `node`, … Cases that do not register any selected implementation are skipped entirely.

- **`relative_to_c`** and comparison columns may be empty or `null` if you omit the native **`c`** baseline; add `c` to the list when you need ratios.

Example:

```bash
export ZR_VM_TEST_TIER=core
export ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary
ctest -R '^performance_report$' --test-dir build/benchmark-gcc-release --output-on-failure
```

To run only selected benchmark case ids (comma-separated), set
**`ZR_VM_PERF_ONLY_CASES`** before `ctest`. This is useful for focused GC or
hotspot diagnosis without paying for the whole core tier.

Example:

```bash
export ZR_VM_TEST_TIER=core
export ZR_VM_PERF_ONLY_CASES=gc_fragment_baseline,gc_fragment_stress
export ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary
ctest -R '^performance_report$' --test-dir build/benchmark-gcc-release --output-on-failure
```

For the paired GC overhead cases at `stress` tier, `ctest -R '^performance_report$'`
can hit the registered `performance_report` timeout (`TIMEOUT 1800`). Use the
direct helper instead; it invokes the same `run_performance_suite.cmake`
pipeline without the CTest timeout wrapper:

```bash
bash scripts/benchmark/run_gc_overhead_stress.sh [build/benchmark-gcc-release]
```

The helper defaults to:

- `ZR_VM_TEST_TIER=stress`
- `ZR_VM_PERF_ONLY_CASES=gc_fragment_baseline,gc_fragment_stress`
- `ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary`

## Report Shape

Every implementation record, including `PASS`, `SKIP`, and `FAIL`, contains:

- `measurement_scope`: `process_end_to_end` for one-shot rows or `persistent_runtime`
  for steady rows;
- `prepare_scope`: the preparation boundary listed below;
- `runtime_reused`, `compiler_reused`, and `jit_state_reused`: explicit booleans
  describing the process/runtime boundary. Persistent rows include a
  `persistent_session` object with one PID, checksum contract, exit code, and
  session-only peak RSS; per-sample RSS is intentionally `null`.

Current `prepare_scope` values are explicit:

| implementation | prepare_scope | timed boundary |
| --- | --- | --- |
| C, Rust | `none` | native process startup and case execution |
| ZR interp | `source_load_compile_in_measurement` | CLI startup, source/project load, compile, runtime setup, and execution |
| ZR binary | `bytecode_compile_before_measurement` | bytecode compile happens once before timing; each sample includes CLI startup, bytecode load, runtime setup, and execution |
| Python, Node.js, QuickJS, Lua | `script_load_in_measurement` | runtime process startup, script load, and execution |
| .NET, Java | `runtime_start_jit_in_measurement` | managed runtime startup, load/JIT activity, and execution |

With `ZR_VM_PERF_SCOPE=steady`, only numeric/dispatch loops have persistent
servers. Lua and QuickJS load their script once, .NET starts one runtime (and
reuses JIT state after warmup or calibration), and `ZR binary` uses the dedicated
`zr_vm_zr_benchmark_server` after a fresh one-shot CLI compile. LuaJIT is never
reported as the non-JIT Lua implementation. Unsupported cases are explicit
`SKIP`; the suite fails closed when the ZR server target is missing. Steady mode
is incompatible with profile/Callgrind mode and always performs one-shot
correctness before timing.

Process mode remains the default and measures a fresh child per iteration.
Persistent mode uses the versioned line protocol and validates READY, ordered
DONE checksums, clean STOP exit, timeouts, malformed input, and clean process-group/job
cleanup. These fields are part of the report contract and must not be inferred
from wall time alone.

Case and implementation names must be strings whose `strip()` result is
non-empty. Case names must also be unique within each benchmark/comparison
report, and implementation names must be unique within a benchmark case.
Missing, non-string, empty, whitespace-only, or duplicate identities make
record lookup invalid or ambiguous: aggregate schema 2 marks the measurement
contract invalid and reports the affected paths, while aggregate JSON and both
CSV exports clear every ratio affected by that case. Validation never trims,
normalizes, supplies, or otherwise rewrites the declared identity, and the
tools do not silently select the first or last duplicate.

`tests/cmake/run_performance_suite.cmake` emits:

Process-mode raw reports are written under
`<build>/tests_generated/performance/`, with generated fixtures and toolchain
artifacts under `<build>/tests_generated/performance_suite/`. Steady-mode raw
reports use the separate `<build>/tests_generated/performance_steady/` tree,
with fixtures under `<build>/tests_generated/performance_suite_steady/`, so a
steady run cannot overwrite the default process-mode products.

- `benchmark_report.md/json`
  Schema 3 adds the execution plan, measurement policy, case minimum duration,
  sample/extra/repetition counts, calibration metadata, MAD, CV, deterministic
  median bootstrap interval, stability/comparability/gate state, and all
  measurement/prepare/reuse fields. Statistics come from runner JSON, not
  display stdout.
- `instruction_report.md/json`
  Per-case opcode execution counts, helper counts, slow-path hits, meta fallback hits, and call cache hit/miss data.
- `hotspot_report.md/json`
  WSL callgrind summaries, top hot functions, helper hotspots, and dispatch hotspot sections.
- `comparison_report.md/json`
  `ZR interp` relative-to-language ratios across `C`, `Lua`, `QuickJS`, `Node`, `Python`, `.NET`, `Java`, and `Rust`.
  Every comparison case and its required `relative_to` field must be JSON objects.
  A ratio is `null` unless both records have the same non-empty
  `measurement_scope` and are stable, comparable, and gate eligible.
- `gc_overhead_report.md/json`
  Paired `gc_fragment_baseline` vs `gc_fragment_stress` deltas per implementation:
  baseline/stress mean wall ms, stress-to-baseline ratio, wall-time delta,
  overhead percent, and mean-peak-MiB delta.

Benchmark Release build (before CSV): `scripts/benchmark/build_benchmark_release.sh gcc|clang` writes to `build/benchmark-gcc-release` or `build/benchmark-clang-release`. On Windows, `pwsh ./scripts/benchmark/build_benchmark_release.ps1 -Toolchain gcc|clang|msvc` uses WSL for gcc/clang and `build/benchmark-msvc-release` for MSVC.

CSV export (WSL): `scripts/benchmark/run_wsl_benchmarks_report_csv.sh` runs `ctest -R performance_report` and writes `benchmark_speed_timings.csv` / `zr_interp_vs_languages.csv` under `<build>/tests_generated/performance/`. Timing CSV rows preserve the five measurement-contract fields; `speed_ratio_vs_c_baseline` is empty unless that implementation and the C baseline both have legal five-field contracts, `PASS` status, and the same non-empty scope. Comparison CSV rows include `measurement_scope`, but their ratios are independently checked against the ZR-interp and target records in `benchmark_report.json`; an empty or inconsistent comparison-case scope is also rejected. Existing ratio values may be cleared but are never recalculated. The compatibility column `one_shot_compile_excluded_from_wall_ms` remains `true` for ZR `binary`. MSVC: `ctest --test-dir build/benchmark-msvc-release -C Release -R '^performance_report$'`, then `python3 scripts/benchmark/benchmark_reports_to_csv.py --report-dir build/benchmark-msvc-release/tests_generated/performance`.

**Consolidated JSON + HTML viewer:** `python3 scripts/benchmark/aggregate_benchmark_summary.py --tests-generated <build>/tests_generated` writes `<build>/tests_generated/benchmark_suite_summary.json` and a copy `<build>/tests_generated/benchmark_html_viewer.json` for the file picker in `benchmark_compare_viewer.html`. Aggregate schema version 2 adds `measurement_contract.valid`, `measurement_contract.issues`, and `measurement_contract.ratios_removed`. It validates every implementation record and identity, then gates both per-implementation `relative_to_c` and comparison-report ratios against the corresponding unique, legal, `PASS`, same-scope benchmark records. A comparison ratio additionally requires the comparison case's own `measurement_scope` to be a non-empty string equal to the ZR-interp scope. That input field is preserved verbatim: aggregation clears an incompatible ratio rather than overwriting or healing the declared scope. Every cleared field path is recorded in `ratios_removed`; existing values are never recalculated or reconstructed. With `--bundle-html <path>`, writes a self-contained page (embedded base64). `run_wsl_benchmarks_report_csv.sh` runs aggregate with `--bundle-html <build>/tests_generated/benchmark_compare_embedded.html` so you can double-click the embedded HTML or open `benchmark_compare_viewer.html` and choose `benchmark_html_viewer.json`.

To aggregate steady-mode results, pass
`--performance-subdir performance_steady`. The default output is then
`<build>/tests_generated/benchmark_suite_summary__performance_steady.json`;
the script deliberately does not rewrite the process-mode
`benchmark_html_viewer.json` for a non-default performance subdirectory. Use
an explicit `--bundle-html` path containing `steady` when a standalone steady
viewer is required.

**`run_wsl_benchmarks_report_csv.sh` (default):** runs `ctest` **twice**: (1) `ZR_VM_TEST_TIER=profile` with `ZR_VM_PERF_CALLGRIND_COUNTING=1`, then renames `tests_generated/performance/` to `tests_generated/performance_profile_callgrind/`; (2) timing pass with `ZR_VM_TEST_TIER` restored (default `core`) and Callgrind counting off, writing fresh `tests_generated/performance/`. CSV and `benchmark_suite_summary.json` use pass (2). Pass (1) is summarized as `benchmark_suite_summary_callgrind.json`. Set `BENCHMARK_DUAL_CTEST=0` for a single `ctest` using your current environment.

**ZR binary (what the numbers mean):** The suite first runs `zr_vm_cli --compile` to
refresh `.zro` artifacts under the generated project tree. Process mode then
measures a fresh CLI process. Steady mode sends requests to the dedicated
binary-only server, which loads the generated `.zro` entry once and executes it
repeatedly without source recompilation. The boundaries are represented by
`bytecode_compile_before_measurement` (process) or
`bytecode_compile_and_load_before_measurement` (steady); compiler reuse remains
`false` because the compiler is not resident in the server.

Unavailable toolchains are reported as `SKIP`, not silently dropped.

`ZR binary` remains in the report, but it does not block core runtime performance
work outside the benchmark slice itself. Historical AOT tooling now lives only
under `zr_vm_aot/`.

## Java Toolchain Notes

- On WSL/Linux, if only a Windows JDK is available, set:
  - `ZR_VM_JAVA_EXE`
  - `ZR_VM_JAVAC_EXE`
- Example:
  - `ZR_VM_JAVA_EXE=/mnt/d/Tools/development/jdk/jdk8/bin/java.exe`
  - `ZR_VM_JAVAC_EXE=/mnt/d/Tools/development/jdk/jdk8/bin/javac.exe`
- The performance harness translates benchmark source/output/classpath arguments
  with `wslpath -w` when invoking Windows `java.exe` / `javac.exe` from WSL, so
  the Java runner can compile and execute correctly through `ctest`.
- Java runtime probing uses `java -version` for compatibility with both Java 8
  and newer releases.

## Adding A New Benchmark

1. Add `cases/<case>/` with implementations for:
   `zr`, `python`, `node`, `c`, `rust`, `dotnet`, `java`.
2. Extend the shared runner/support implementation for each language.
3. Register the case in `registry.cmake` with:
   description, tiers, pass banner, supported implementations, and
   smoke/core/stress checksums.
4. If native C support is required, add the case descriptor source to
   `zr_vm_native_benchmark_runner` in `tests/CMakeLists.txt`.
5. Re-run:
   `zr_vm_benchmark_registry_test`
   and either `cmake --build <build> --target run_performance_suite` or, if the
   tree was configured with `-DZR_VM_REGISTER_PERFORMANCE_CTEST=ON`,
   `ctest -R '^performance_report$'`
