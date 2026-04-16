# ZR VM Benchmarks

`tests/benchmarks` is the single source of truth for the cross-language
benchmark suite driven by `tests/cmake/run_performance_suite.cmake`.

**CTest:** `performance_report` is registered only when
`ZR_VM_REGISTER_PERFORMANCE_CTEST=ON` (default `OFF` so default `ctest` stays
fast). **Without that flag**, run the same suite via the build target
`run_performance_suite` or invoke `cmake -P tests/cmake/run_performance_suite.cmake`
with the same `-DCLI_EXE=...` arguments as in `tests/CMakeLists.txt`.

**Light check:** `zr_vm_benchmark_registry_test` / CTest `benchmark_registry` remains in the default test set and validates `registry.cmake` layout only.

## Layout

- `registry.cmake`
  Registers case metadata, tier membership, supported implementations, pass
  banners, tier scales, and per-tier checksum contracts.
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

`tests/cmake/run_performance_suite.cmake` emits:

- `benchmark_report.md/json`
  `case | implementation | language | status | mean/median/min/max/stddev wall ms | mean/max peak MiB | relative_to_c`
- `instruction_report.md/json`
  Per-case opcode execution counts, helper counts, slow-path hits, meta fallback hits, and call cache hit/miss data.
- `hotspot_report.md/json`
  WSL callgrind summaries, top hot functions, helper hotspots, and dispatch hotspot sections.
- `comparison_report.md/json`
  `ZR interp` relative-to-language ratios across `C`, `Lua`, `QuickJS`, `Node`, `Python`, `.NET`, `Java`, and `Rust`.
- `gc_overhead_report.md/json`
  Paired `gc_fragment_baseline` vs `gc_fragment_stress` deltas per implementation:
  baseline/stress mean wall ms, stress-to-baseline ratio, wall-time delta,
  overhead percent, and mean-peak-MiB delta.

Benchmark Release build (before CSV): `scripts/benchmark/build_benchmark_release.sh gcc|clang` writes to `build/benchmark-gcc-release` or `build/benchmark-clang-release`. On Windows, `pwsh ./scripts/benchmark/build_benchmark_release.ps1 -Toolchain gcc|clang|msvc` uses WSL for gcc/clang and `build/benchmark-msvc-release` for MSVC.

CSV export (WSL): `scripts/benchmark/run_wsl_benchmarks_report_csv.sh` runs `ctest -R performance_report` and writes `benchmark_speed_timings.csv` / `zr_interp_vs_languages.csv` under `<build>/tests_generated/performance/`. The column `one_shot_compile_excluded_from_wall_ms` is `true` for ZR `binary`: wall ms come from the perf-runner phase only; the suite runs `zr_vm_cli --compile` in a separate prepare step (see `tests/cmake/run_performance_suite.cmake`). MSVC: `ctest --test-dir build/benchmark-msvc-release -C Release -R '^performance_report$'`, then `python3 scripts/benchmark/benchmark_reports_to_csv.py --report-dir build/benchmark-msvc-release/tests_generated/performance`.

**Consolidated JSON + HTML viewer:** `python3 scripts/benchmark/aggregate_benchmark_summary.py --tests-generated <build>/tests_generated` writes `<build>/tests_generated/benchmark_suite_summary.json` and a copy `<build>/tests_generated/benchmark_html_viewer.json` for the file picker in `benchmark_compare_viewer.html`. With `--bundle-html <path>`, writes a self-contained page (embedded base64). `run_wsl_benchmarks_report_csv.sh` runs aggregate with `--bundle-html <build>/tests_generated/benchmark_compare_embedded.html` so you can double-click the embedded HTML or open `benchmark_compare_viewer.html` and choose `benchmark_html_viewer.json`.

**`run_wsl_benchmarks_report_csv.sh` (default):** runs `ctest` **twice**: (1) `ZR_VM_TEST_TIER=profile` with `ZR_VM_PERF_CALLGRIND_COUNTING=1`, then renames `tests_generated/performance/` to `tests_generated/performance_profile_callgrind/`; (2) timing pass with `ZR_VM_TEST_TIER` restored (default `core`) and Callgrind counting off, writing fresh `tests_generated/performance/`. CSV and `benchmark_suite_summary.json` use pass (2). Pass (1) is summarized as `benchmark_suite_summary_callgrind.json`. Set `BENCHMARK_DUAL_CTEST=0` for a single `ctest` using your current environment.

**ZR binary (what the numbers mean):** The prepare step runs `zr_vm_cli --compile`, which emits `.zro` artifacts under the generated project tree. **Prepare is executed once by the suite via CMake (`execute_process`) and is not included in reported `mean_wall_ms` / `perf_runner` samples.** The timed command is only `zr_vm_cli ... --execution-mode binary`. Reported wall ms still include full **CLI process** startup, `.zro` loading, runtime setup, and program execution. CSV `one_shot_compile_excluded_from_wall_ms` is `true` for `binary` to document that split; see `benchmark_report.json` fields `reported_wall_ms_includes_prepare_compile` and `reported_wall_ms_scope`.

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
