# ZR VM Benchmarks

`tests/benchmarks` is the single source of truth for the cross-language
benchmark suite consumed by `performance_report`.

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
  Runs all 8 benchmark cases
- `stress`
  Runs all 8 benchmark cases at the largest scale
- `profile`
  Runs profile-enabled cases with smaller per-case scales for opcode counters,
  hotspot capture, and slow-path analysis.

Scale is fixed in `registry.cmake`:

- `smoke = 1`
- `core = 4`
- `stress = 16`
- `profile = registry controlled per case`

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

Unavailable toolchains are reported as `SKIP`, not silently dropped.

`ZR binary` and `ZR aot_*` remain in the report, but they are follow-up debt for
the current core-gated milestone and do not block core runtime performance work.

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
   and
   `ctest -R '^performance_report$'`
