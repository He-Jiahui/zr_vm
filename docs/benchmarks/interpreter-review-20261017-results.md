# Interpreter review implementation — 2026-10-17

Branch: `perf/interpreter-20261017`. Requested by John He. Review baseline: `a149c17e`.
Runtime changes finish at `dc0459cd`; GC phase tests finish at `e6ed4a5b`.
Verification ran on 2026-10-18 UTC in `/workspace/zr_vm-perf`.

## Changes landed

| Commit | Change |
| --- | --- |
| `a61ebdeb` | Interpreter debt collection, weak string intern/API/concat caches, GC root/forwarding repairs, finalizer ordering, and sweep work counted by visited objects. Adds review probes and GC collection assertions. |
| `f529fb1c` | Automatic majors compare old-space growth with the last major's live baseline. Defers automatic collection across native reentry with unpublished C temporaries. |
| `dcf51ef4` | Reuses authoritative packed scalar call arguments and returns, caches frame metadata, and extends packed slot access. Preserves ownership cleanup and bounds checks. |
| `876a6a6d` | Direct fast-table entries for 37 common opcodes, typed float arithmetic, signed `<` branch fusion using the existing reversed `<=` encoding, strict signed relational operands, and matching AOT typed-loop recognition. |
| `dff34b1f` | Resource budget checks every 256 instructions with exact instruction admission counts; cached budget pointer; atomic GC pause publication and idle poll fast path. Adds limit/cancellation/deadline regressions and a persistent budget benchmark. |
| `dc0459cd` | Prevents idle/sweep barriers and temporary roots from leaving unconsumed gray objects; preserves transitive graphs across incremental slices. Three regressions reproduce the old failure. |
| `e6ed4a5b` | Updates legacy GC tests to check idle and marking semantics explicitly. |

The GC audit also covers expression-result roots, metadata strings and call frames,
permanent/ignored-owner young references, parser/project analysis caches, task
callable publication, and root registration before a parking safepoint.
The final core-tier run exposed a linked-list failure missed by smoke checks and
forced-full GC. The incremental phase fix resolves it; both a sliced sanitizer
build and a forced-full sanitizer build verify the resulting behavior.

## Verification

- Release: **416/416 CTests passed** (65.56 s).
- ASan + UBSan + forced-full GC: **416/416 CTests passed** (998.06 s).
- All 16 existing benchmark cases at smoke scale: **32/32 source/binary
  checksum checks passed under ASan**, including collection assertions for GC cases.
- Ordinary sliced-GC ASan/UBSan: **13/13 review probes passed**, plus the core
  container pipeline checksum `672287189`.
- The review's deterministic `.zro` mutation probe: **118 cases**, 68 rejected
  and 50 completed with checksum `4950`; no ASan/UBSan report, abort, or SEGV.
- The new idle, sweep and interpreted-chain regressions fail against the prior
  core library and pass with the fix. The 70-case legacy GC executable also
  passes in both configurations after its marking-phase setup was corrected.
- Registered matrix: smoke **21/21**, core **42/42**, stress **42/42** jobs
  passed. Core and stress cover all 16 cases. Steady has two passing ZR rows
  and two skipped C rows across two cases.
- Release also passes all **32/32** source/binary smoke checks across all 16
  registered cases. Seven cases are excluded from the registry smoke tier, so
  these were checked separately. Every captured tier has a `COMPARABLE` environment.

The main sanitizer configuration is Debug with `-fsanitize=address,undefined`,
`-fno-omit-frame-pointer`, and `-DZR_DEBUG_GARBAGE_COLLECT_MEM_TEST`.
Runs use `ASAN_OPTIONS=detect_leaks=0` and `UBSAN_OPTIONS=halt_on_error=1`;
LeakSanitizer was not part of this gate. An additional ASan/UBSan build uses
`-g -UZR_DEBUG` to retain ordinary GC slices; Debug otherwise enables forced-full
GC automatically in `zr_object_conf.h`. Its 13 smoke probes and core container
checksum pass. LSP and Rust binding builds are disabled.

The benchmark matrix uses GCC 14.2 Release, seed 0, C/ZR source/ZR binary,
and the repository's environment capture and single-CPU affinity wrapper.
Some process rows remain ineligible for comparison under the suite's policy;
numbers below come from raw per-implementation runner summaries. Process timings
include startup and loading; the budget harness and supported steady rows exclude
compilation/startup. Historical review timings do not constitute a matched
environment baseline, so a universal 5% regression gate is not claimed.

## GC and microbenchmarks

The 2-million-object automatic-GC probe peaks at **12.95 MiB RSS**
(the review observed about 1.5 GB and zero collections), with **622
collections** in the last run. The 200,000-string churn probe finishes with
**556 live strings**, versus 600,563 in the review; its peak RSS is
**20.75 MiB**. The runner asserts collection activity, automatic-object
RSS below 64 MiB, and fewer than 5,000 live strings after full collection.

Core `gc_fragment_baseline` source execution finishes with **2.03 MiB
of managed object bytes** and **20.96 MiB median RSS**, with
2,119 minor / 15 major / 0 full collections.
The review's roughly 151 MB object heap is no longer retained. Its 300 ms
performance target remains unmet; the final measured time appears below.

Three-run process medians in milliseconds. The before column is this checkout's
`a149c17e` measurement; the closure result was strengthened from a constant to
the final captured counter. All final probe checksums passed.

| Probe | Before (ms) | Final (ms) |
| --- | ---: | ---: |
| `objauto` | — | 2149.63 |
| `strdrop` | — | 642.62 |
| `calls` | 1435.70 | 645.39 |
| `fib` | 212.21 | 167.60 |
| `methods` | 1545.51 | 1238.11 |
| `loop_lt` | 702.32 | 487.08 |
| `loop_le` | 675.21 | 566.79 |
| `floatloop` | 403.99 | 290.55 |
| `closure` | 1027.17 | 815.75 |
| `fields` | — | 419.35 |
| `arrays` | — | 264.24 |
| `strcat` | 135.57 | 116.00 |
| `intloop` | — | 315.81 |

The 100,000-call Callgrind profile fell from about **559 million** to
**274,210,513** instructions after `dcf51ef4` (51% fewer). Generic argument staging
ceased to dominate; dispatch and frame entry/return remain substantial costs.

The persistent 10-million-iteration budget probe admits **90,000,007** instructions
and returns **35,000,000** in both modes. Before the budget change, unbudgeted /
deadline-and-cancellation medians were **331.726 / 3125.078 ms**. Afterwards they
were **305.156 / 429.028 ms**: budgeted execution is 86.3% faster, with **40.59%**
overhead still above the report's 10% target. Counts remain exact at every
instruction, including exception/reentry boundaries; resource checks are sampled.

## Registered suite

Core-tier process medians in milliseconds. These are raw measurements; the
suite records one measured sample for each core process row. Unregistered
modes are shown as `—`; comparison gates can exclude rows.

| Case | C | ZR source | ZR binary |
| --- | ---: | ---: | ---: |
| `numeric_loops` | 10.23 | 94.81 | 91.52 |
| `dispatch_loops` | 3.85 | 531.40 | 525.05 |
| `container_pipeline` | 1.11 | 35.46 | 28.17 |
| `sort_array` | 1.10 | 15.31 | 7.12 |
| `prime_trial_division` | 1.30 | 14.96 | 12.31 |
| `matrix_add_2d` | 1.11 | 18.67 | 12.43 |
| `string_build` | 1.11 | 16.52 | 9.65 |
| `map_object_access` | 1.11 | 19.14 | 14.43 |
| `fib_recursive` | 1.11 | 72.59 | — |
| `call_chain_polymorphic` | 1.15 | 20.86 | — |
| `object_field_hot` | 1.53 | 39.12 | — |
| `array_index_dense` | 1.13 | 36.10 | — |
| `branch_jump_dense` | 3.26 | 28.06 | — |
| `mixed_service_loop` | 2.10 | 95.77 | — |
| `gc_fragment_baseline` | 17.66 | 4636.60 | 4691.02 |
| `gc_fragment_stress` | 17.93 | 3591.24 | 3767.41 |

Stress GC medians use two measured samples per row at scale 16.

| Case | ZR source (ms) | ZR binary (ms) |
| --- | ---: | ---: |
| `gc_fragment_baseline` | 24479.29 | 24487.97 |
| `gc_fragment_stress` | 21696.78 | 21455.55 |

Steady core medians exclude compilation/startup and use the ZR binary
persistent runtime. Both requested C rows report `SKIP`: the framework
does not provide steady scope for C. No steady C/ZR ratio is available.

| Case | ZR binary (ms) | Samples | Repetitions per sample |
| --- | ---: | ---: | ---: |
| `numeric_loops` | 74.679 | 10 | 16 |
| `dispatch_loops` | 392.782 | 10 | 2 |

## Deferred and unmet targets

- **PERF-C1/H1:** Calls improve substantially but the report's absolute call,
  Fibonacci and method-call targets remain unmet. Whole-frame null filling,
  ownership cleanup, operand guards and PC end checks remain where required.
  A loader operand-range check alone does not prove slot liveness or ownership.
- **PERF-H2/M5:** Signed `<` branches fuse without adding or renumbering opcodes.
  New float/constant branch encodings and broader compiler move/constant hoisting
  are deferred. The canonical loop still has seven instructions per iteration,
  above the requested four.
- **PERF-H3/M3:** Values/stack slots remain 48/64 bytes. Reducing alignment needs
  coordinated AOT ABI and persisted frame-layout compatibility work. Next steps
  are 40/48-byte layouts with explicit compatibility handling, then ownership
  side data and smaller object/type headers with a separate lifetime design.
- **PERF-H4:** Survivor visits now consume sweep work budgets and cursors resume.
  Atomic remark, generational collection and finalization can still run
  synchronously. A stress-scale `gc_fragment_baseline` run wrapped the exported
  `GcStep` with `CLOCK_MONOTONIC` using an `LD_PRELOAD` probe: **24,709 calls**,
  **94,172.715 µs maximum**, versus the requested 800 µs bound (2 × 400).
  This is a diagnostic wall-clock observation, not an achieved pause guarantee;
  the wrapper includes collector synchronization and scheduling time.
- **CORR-M1:** The existing `.zro` loader already rejects unsupported opcodes,
  invalid operands and jumps before publishing functions. Its rejection tests
  pass. The exec-IR verifier has a separate extensible opcode domain; no unsafe
  VM-enum restriction was added there.
- **PERF-M1/M4/M6:** Deadline overhead remains above target. Weak string caches
  and direct string handlers landed; n-ary concat/string-builder lowering and
  a full new cross-language registry expansion are deferred. Appendix A shapes
  are covered by the standalone probes, with GC collection/RSS/string assertions.
- **L batch:** Strict signed relational operands and removal of the packed-base
  `noinline` attribute landed. Width/narrowing semantics, integer-helper error
  publication, CLI compile-error behavior, loop-head relocation flags, and
  debugger trap/backedge redesign are deferred.

Final calls are **55.0% faster** than the local baseline. Absolute
acceptance remains partial: the report asks for calls ≤400 ms, Fibonacci ≤60 ms,
methods ≤450 ms, floatloop ≤250 ms, strcat ≤50 ms, and the canonical loop ≤400 ms.
The table gives the observed values. The closure change is **20.6%**
against the baseline; its requested improvement is 25%. The budget, loop-size,
layout and whole-GC-step latency targets remain explicitly deferred above.

## Reproduction

```bash
export PATH="$PWD/build/local-tools/usr/bin:$PWD/build/tools/bin:$PATH"
export LD_LIBRARY_PATH="$PWD/build/local-tools/usr/lib/x86_64-linux-gnu"
cmake --build build/perf-release -j6
ZR_VM_TEST_TIER=core ctest --test-dir build/perf-release -j4 \
  -E '^performance_report$' --output-on-failure
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ZR_VM_TEST_TIER=core \
  ctest --test-dir build/gcstress -j4 -E '^performance_report$' --output-on-failure
python3 tests/benchmarks/run_interpreter_probes.py \
  --cli build/perf-release/bin/zr_vm_cli --work-dir build/probes-final --repeat 3
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  python3 tests/benchmarks/run_interpreter_probes.py --cli build/gcstress/bin/zr_vm_cli \
  --work-dir build/probes-final-asan --repeat 1 --smoke
ZR_VM_TEST_TIER=core ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary \
  ctest --test-dir build/perf-release -R '^performance_report$' --output-on-failure
bash scripts/benchmark/run_gc_overhead_stress.sh build/perf-release
ZR_VM_TEST_TIER=core ZR_VM_PERF_SCOPE=steady \
ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops \
ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_binary \
  ctest --test-dir build/perf-release -R '^performance_report$' --output-on-failure
build/perf-release/bin/zr_vm_budgeted_loop
```

The final stress matrix set `ZR_VM_PERF_ONLY_CASES` to all 16 case names above;
the stress script defaults to the two GC cases.

The final matrix, environment evidence and raw JSON/heap summaries are under
ignored `build/performance-final/` and `build/probes-final/`. Full-suite logs are
`build/test-release-final-rerun.log` and `build/test-gcstress-final-full.log`;
`build/fuzz-zro-final/results.log` records the mutation probe. The failed first
core benchmark is preserved under `build/performance-pre-barrier/`.
