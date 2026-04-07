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
- `cases/<case>/c/`
  Native C descriptor consumed by `zr_vm_native_benchmark_runner`.
- `cases/<case>/rust/`
  Rust module wrapper consumed by `rust_runner`.
- `cases/<case>/dotnet/`
  C# wrapper consumed by `dotnet_runner`.
- `native_runner/`
  Prebuilt native C baseline executable registered from `tests/CMakeLists.txt`.
- `rust_runner/`
  Cargo-based shared Rust benchmark runner.
- `dotnet_runner/`
  Shared .NET benchmark runner.
- `common/python/`
  Shared Python benchmark algorithms.
- `common/node/`
  Shared Node.js benchmark algorithms.

## Contract

Every benchmark implementation must satisfy the same correctness contract for a
given case and tier:

- success exit code
- stdout contains exactly two lines
- line 1 is the registered `BENCH_<CASE>_PASS` banner
- line 2 is the deterministic checksum for that tier

Non-ZR implementations accept `--tier smoke|core|stress` directly.

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

Scale is fixed in `registry.cmake`:

- `smoke = 1`
- `core = 4`
- `stress = 16`

## Report Shape

`tests/cmake/run_performance_suite.cmake` emits:

- Markdown: `case | implementation | language | status | mean/median/min/max/stddev wall ms | mean/max peak MiB | relative_to_c`
- JSON: `suite -> cases[] -> implementations[]`

Unavailable toolchains are reported as `SKIP`, not silently dropped.

## Adding A New Benchmark

1. Add `cases/<case>/` with implementations for:
   `zr`, `python`, `node`, `c`, `rust`, `dotnet`.
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
