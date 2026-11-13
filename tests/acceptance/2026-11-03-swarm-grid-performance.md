# Swarm occupied-cell grid performance

## Scope

Optimize the ZR DOD lane's per-frame spatial-grid rebuild in the actual checkout.
Remember occupied cells, cache each enemy's cell, and use `start + count` in
steering and collision. Preserve each cell's dense-slot candidate order, cell
traversal, arithmetic, RNG draws, gameplay rules, frame counts and goldens.
The independent native C simulation is unchanged.

The storage tradeoff is one capacity-sized cell-index array and an occupied-cell
array bounded by min(capacity, 4096). Both are initialized at boot; rebuilds add
no containers. The old global prefix/sentinel layout is no longer the grid's
contract. All production consumers and existing layout assertions were updated.

## Baseline

- After fetching origin, main and origin/main both point to
  `447abb1e8efb44203b353671e9a0417173e16d58`.
- Preserve the pre-existing AOT test edit and six dirty third-party submodules.
  `tests/parser/test_aot_llvm_symbol_stripping.c` has SHA256
  `8f18471b9a5b4e46b484505fe892a8873eaf40d4876fc86dd4c688317bf6e928`.
  This work makes no claim about that unrelated change or the full engine suite.
- Before changing the grid, three repeated runs of each of five scenarios
  matched every frame and complete final state, including the C reference.
  The three new grid regressions passed on the original implementation.
- The local smoke target was a 20% reduction from 4927.308333 us to at most
  3941.846667 us. Checking the old observation against that budget failed, as
  expected; this is a local observation target, not a portable timing assertion.

## Test Inventory

- New direct grid fixture: stable candidates across interleaved cells, movement
  and swap removal; repeated empty builds and reuse; every one of 4096 cells
  occupied; all 4096 enemies in one cell; zero capacity; clamped boundaries.
- Three existing game grid tests: boundaries, stable per-cell ranges/empty
  rebuild, and swap removal preserving all world columns.
- All 20 existing game cases: events (4), gameplay (10), grid (3), RNG (3).
  Gameplay includes cross-cell separation/coincident ties and piercing
  collisions across frames/removals, directly exercising both changed consumers.
- Host integration: smoke/120, arena_2k/60, swarm_10k/8, bullet_storm/60,
  particle_burst/60; three DOD samples and one C-reference run per case before
  and after. Compare all final success fields, entity maxima, and every frame's
  gameplay hash and sprite count. Enforce committed goldens at matching lengths.
- Manual and slack GC, minor interval 1: full smoke/120, comparing all success
  fields, entity maxima and every frame with the optimized automatic-GC run.
- CLI interpreter: smoke/120, execution-route marker and complete golden state.
- Negative harness checks: a synthetic failed assertion returns 1; an invalid
  test identifier returns 2. The synthetic fixture is an owned temporary input.

## Tooling Evidence

Reports are under
`E:/cargo-targets/zr_vm/reports/20261103-swarm-performance` (denoted REPORT).
Reusable build roots are under
`E:/cargo-targets/zr_vm/build/20261103-swarm-performance` (denoted BUILD).

Environment: AMD Ryzen 7 5800H, WSL Ubuntu 22.04; GCC 11.4 Release
(`-O3 -DNDEBUG`) for timing, Clang 14 Debug for independent correctness, and
MSVC 19.44 / VS 17.14.44 Debug for Windows compatibility. Timing runs use
`taskset -c 2`, seed 12345, null rendering, bulk submission and identical GCC
host/native-library hashes. No owned compilers or other owned checks ran
concurrently with the primary before/after timing series.

GCC host SHA256:
`32879250468120364dd3ce5ab4695c06ce8c398846076842ab2d3f799d0bd52d`.
Each performance JSON records actual game-source, manifest and native-library
hashes. Generated manifests reference the actual source and dependency paths;
no source trees, modules or snapshots are staged. The C unit driver reads each
actual fixture, appends one call in memory, and creates a separate VM state for
each case, matching the stock runner's isolation.

From WSL, the primary commands were:

```sh
E_ROOT=/mnt/e/cargo-targets/zr_vm
REPORT="$E_ROOT/reports/20261103-swarm-performance"
BUILD="$E_ROOT/build/20261103-swarm-performance"

ctest --test-dir "$BUILD/gcc-release" --output-on-failure \
  -R '^(demo_grid(_game_units)?|demo_native_runtime)$'
ctest --test-dir "$BUILD/clang-debug" --output-on-failure \
  -R '^(demo_grid(_game_units)?|demo_native_runtime)$'

# Baseline was captured before changing production sources.
python3 -B -I tests/benchmarks/run_swarm_performance.py \
  --host "$BUILD/gcc-release/bin/zr_swarm" --out "$REPORT/before" \
  --repeat 3 --cpu 2 --check-c-lane
python3 -B -I tests/benchmarks/run_swarm_performance.py \
  --host "$BUILD/gcc-release/bin/zr_swarm" --out "$REPORT/after" \
  --baseline "$REPORT/before/results.json" --repeat 3 --cpu 2 \
  --check-c-lane --max-smoke-tick-us 3941.846667
```

Fresh output directories are required. The helper does not reconstruct old
sources: reproducing the baseline requires the recorded original source state.
See the [module note](../../docs/benchmarks/swarm-grid-performance.md) for the
data layout and helper contract.

Exact per-fixture commands/results for all 20 game cases are in
`REPORT/game-units-gcc.json`; GC and CLI commands/results are in
`REPORT/runtime-edges.json`. Windows imported the using-vsdevcmd environment,
built the CLI with CMake/Ninja, then compiled the same small C test driver
against that build's DLL import libraries. Compiler arguments are retained in
`REPORT/msvc-grid-arguments.txt`. Six grid cases and the CLI hello smoke passed.
The POSIX demo host itself was not claimed as a Windows host build.

## Results

Primary timing: median of three per-run mean tick times; P95 is the median of
three per-run P95 observations. All values below are milliseconds.

| Scenario / frames | Mean before | Mean after | Mean reduction | P95 before | P95 after |
| --- | ---: | ---: | ---: | ---: | ---: |
| smoke / 120 | 4.927 | 3.849 | 21.9% | 8.091 | 6.448 |
| arena_2k / 60 | 38.726 | 35.486 | 8.4% | 64.094 | 69.218 |
| swarm_10k / 8 | 467.927 | 312.171 | 33.3% | 610.299 | 365.441 |
| bullet_storm / 60 | 41.529 | 34.090 | 17.9% | 88.982 | 60.695 |
| particle_burst / 60 | 70.149 | 57.071 | 18.6% | 120.032 | 83.753 |

`before/results.json` and `after/results.json` both report `OK`. All five
scenarios preserve every checked frame and final state against both the old
implementation and the C reference. The local smoke budget passes. Smoke's
final hash remains 552988638. The short dense scenarios do not establish
1800-frame completion or a 60 FPS performance budget.

The arena P95 observation increased in the primary series despite its lower
mean. Individual sample means also vary substantially (arena before: 47.448,
34.890, 38.726 ms; after: 41.104, 31.625, 35.486 ms). An independent three-run
arena follow-up, using the same executable/libraries, CPU and baseline, measured
a 39.412 ms median mean (+1.8% from baseline) and a 55.825 ms median P95.
Every checked state/frame still matched the baseline and C reference.
`arena-followup/results.json` reports `OK`. Neither the initial P95 increase
nor a consistent mean improvement was reproduced. No stable arena latency
improvement is claimed. Preserve both series; do not generalize this local
measurement to other hardware or infer uniform latency improvements.

Passed: GCC and Clang each have three focused CTest gates (six ZR grid cases
and native runtime); MSVC has six direct grid cases and CLI hello; GCC has all
20 existing game cases, both GC policies, CLI interpreter smoke and the two
negative harness checks.

Clang Debug additionally completes smoke/120 and swarm_10k/4 against the C
reference (`clang-check/results.json`: `OK`). Its full smoke's final state,
entity maxima and every checked frame also equal GCC Release's smoke
(`cross-compiler.json`: `OK`). Debug timings are excluded from performance
claims. The independent timing follow-up command was:

```sh
python3 -B -I tests/benchmarks/run_swarm_performance.py \
  --host "$BUILD/gcc-release/bin/zr_swarm" --out "$REPORT/arena-followup" \
  --case arena_2k:60 --baseline "$REPORT/before/results.json" \
  --repeat 3 --cpu 2 --check-c-lane
```

Setup failures and limitations retained in the report:

- Early fixture setup hit dependency import/source-identity rejection and an
  ID-registration range error in `World.add` on unchanged production code.
  The final harness uses the real project and an in-memory source identity;
  the large grid fixture initializes only the grid's position/count inputs.
  Real add/swap behavior remains exercised by the small grid and gameplay cases.
- Running formerly isolated existing cases together in one VM also produced
  an ID-registration range error; each passed separately. The final runner
  isolates VM/module state per case. No engine fix for these observations is
  claimed. Earlier logs remain alongside the passing final logs.
- One Clang CTest invocation used a stale generated test command before CMake
  was refreshed; the final configuration runs all three gates successfully.
- Clang Release and MSVC Release optimizer builds were stopped by the owner
  after prolonged compilation. Successful Debug builds supply their correctness
  evidence; only GCC Release supplies performance evidence.
- An attempted GDB attachment to the owned Clang dense check was denied by
  the WSL ptrace environment (`Inappropriate ioctl for device`). No debugger
  stack, sanitizer, Valgrind or full repository-suite pass is claimed.

## Acceptance Decision

Accepted for the sparse grid and the listed bounded equivalence gates. The
primary series observes mean tick reductions of 21.9% in full smoke and 33.3%
in the eight-frame 10k case, plus reductions in bullet/particle cases. Cross-
compiler grid/host checks and all existing game cases pass. Arena timings do
not support a stable performance gain; no general tail-latency, full-scenario,
Windows-host, AOT, or full repository-suite claim is made.

Cleanup removed 2257 task-owned files (61,310,986 bytes, 58.47 MiB): the two
stopped Release build trees and the synthetic negative fixture. Empty owned
temporary/test directories were removed as well. The GCC Release, Clang Debug
and MSVC Debug builds are retained for reuse, together with reports and the
stopped builds' configuration/log evidence. Cleanup is limited to this task's
outputs; unrelated or inaccessible historical artifacts are outside its scope.

Artifact lifecycle and final source identities are recorded in
`REPORT/artifacts.json` and `REPORT/source-after.json`; cleanup receipts are
retained alongside them. Source changes remain in the actual checkout.
