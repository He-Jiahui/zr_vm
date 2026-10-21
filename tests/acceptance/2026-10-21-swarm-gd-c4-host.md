# Swarm Arena GD-C4 host acceptance — 2026-10-21

GD-C4 is complete: `zr_swarm` owns poll → tick → render → null backend
submit/present → GC policy → metrics. It supports only smoke/DOD, null render,
bulk submit and the documented bounded inputs. Unsupported settings name the
field in `SWARM_ERROR` and exit 2. `--out` writes one CSV row per frame and a
schema-valid JSON summary; CLI output remains textual.

The preserved baseline is `9885cb15`. Engine correction `e1d3ab19` is merged as
`268b6864`; demo implementation commits are `857e4456` and `d321437a`, with
usage documentation in `df8c84ba`, stress deadlines in `944f81be`, unit
discovery deadlines in `58a5f1f7`, worker deadlines in `d3e4c1e7`, and probe
deadlines in `c7c6b741`. The initial unchanged-engine host parity had a
memory-error-detector timeout. The supplied moved-function regression fails on
`ed10137f` and passes with the focused import correction.

| Gate | Release | memory-error-detector | forced-GC |
| --- | --- | --- | --- |
| All demo CTests (71 retained + 2 host), final outcomes | 73/73 | 73/73 | 73/73 |
| Host smoke and CLI/host parity | 2/2 | 2/2 | 2/2 |
| Native C tests | 8/8 | 8/8 | 8/8 |
| Required GD-C1 mode checks | 38/38 | 38/38 | 38/38 |
| Successful GD-C1 mode checks | 58/62 | 58/62 | 58/62 |
| P22 required parity pairs | 18/18 | 18/18 | 18/18 |
| CLI smoke repetitions, interp + source-removed binary | 3 + 3 | 3 + 3 | 3 + 3 |
| ZR unit tests | 10/10 | 10/10 | 10/10 |
| Host failure cases | 25/25 | 25/25 | 25/25 |
| Core CTest inventory | 515 unchanged | 515 unchanged | 515 unchanged |
| Engine core CTests | 515/515 | 515/515 | 129/129 selected |
| Detector findings | — | 0 | 0 |
| Demo validation wall seconds | 87.87 | 3699.53 | 2654.13 |
| Engine wall seconds | 70.93 | 1982.22 | 270.17 |

The counts represent successful final outcomes for all 73 distinct tests.
Release passes a complete 73-test run and a final 1/1 unit recheck.
Memory-error-detector combines a 72/73 full run, 3/3 focused checks and a final
1/1 unit recheck. Forced-GC combines a 69/73 full run with 4/4 focused checks:
units, P01 binary, P22 and the feature gate. Wall times sum those components.
Host binaries were built at `58a5f1f7`; later unit worker deadline validation
uses those same executables.

Host/CLI golden parity is **224663030**, seed 12345, 120 frames; checkpoint 60
is **227689432**. Final counters and the null render hash also match the
retained GD-C3 golden. The independent Draft 2020-12 validator accepts all
**21** retained final summaries. Host CTests validate policy frame hashes,
manual cadence, zero slack work, artifact aggregates, 25 failure cases and
reused-output cleanup. Native C tests remain 8/8, required probes 38/38 and P22
18/18. Demo registration adds exactly two CTests; all six core inventories
retain the same 515 names.

The original temporary forced-GC engine selection was unavailable; its 129-test
replacement covers the documented families, all 28 Swarm checks and the
moved-function regression. The engine's local detector caches use the system
compiler's shared runtime link mode for generated-library checks. Demo
memory-error-detector and forced-GC gates run sequentially with `-j1`.
Intermediate checks timed out during binary compilation, unit discovery and
workers, and P01 binary startup. The exact binary-compilation replay passed in
24.93 seconds. Unit workers now allow 600 seconds, their enclosing process 1800
seconds and CTest 2400 seconds. Probe processes allow 600 seconds and their
CTests 1800 seconds. The final focused checks above pass. Earlier
failed/interrupted attempts remain separate from the final results.

The detector gates retain the existing policy: leak detection disabled.
Allocation values are positive managed-memory delta estimates; exact pause p99
is null and per-system zones are unmeasured. Timing establishes no performance
budget. P19 reflection, P14 v2 package integration, GD-C5 and all later work
remain deferred. Simulation rules and the golden are unchanged.

See [verification](../../demos/swarm_arena/docs/verification.md) for commands,
selection and timings,
[decisions](../../demos/swarm_arena/docs/gd-c4-decisions.md) for API/measurement
choices, and [usage](../../demos/swarm_arena/README.md). Structured evidence is
`/tmp/zr-swarm-gd-c4-20261021/final-evidence.json`; every raw build/test/runtime
log is under `/tmp/zr-swarm-gd-c4-*`.
