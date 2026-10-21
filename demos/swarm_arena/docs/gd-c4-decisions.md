# GD-C4 host decisions

GD-C4 adds only the source-driven `zr_swarm` host for smoke/DOD, the null
platform/backend, bulk submission, three GC policies and file metrics. The
engine includes the focused import correction `e1d3ab19` on `ed10137f`.
Full gameplay, other scenarios/lanes/renderers,
snapshots, performance matrices and all H/M milestones remain deferred.

## Topology and script composition

The host follows design §6.1: `host/main.c`, `options.c`, `host_vm.c`,
`host_loop.c`, `platform_null.c`, and a private header. It links the existing
shared runtime and static `zrgame.host`/`zrgame.gfx` descriptors. There is no
kernel module yet. The provider list follows the CLI's
`project.c::ZrCli_Project_RegisterStandardModulesWithBootstrap`: math, system,
optional network, container (which registers iteration), FFI, optional thread,
then task builtins. It uses CommonState, the parser and the library C API.

Before importing entry, C sets the runtime's host-drive flag. Calling
`main.hostEntry` imports and executes `main` once; the host branch returns
before CLI argument parsing or simulation construction. The canonical `game`
module exports `boot(scenario, lane, seed, frames)`, `tick(frame)`,
`render(frame)`, `fullHash()` and idempotent `shutdown()` (summary string).
Small scalar exports provide entity maxima and GC controls. No simulation
runs on importing `game`.

The GD-C3 `Session` implementation moves into `game_session.zr`; `Buffer`
moves into `render_buffer.zr`. Both callers import those classes, using import
variable names that match the module names. This is a demo composition
workaround: constructing/calling a class from its declaring module through
the host wrappers produced wrong values or render call-layout errors.
Importing the same classes from separate modules fixes those calls. A renamed
module used with the old alias also lost private fields in binary execution;
the canonical import names preserve source-removed binary loading. The
simulation arithmetic, dense order and hashing are unchanged. There is one
implementation of each class, and RNG/hash code remains in `$swarmcore` /
`import("&swarmcore")`. No reflection or v2 package alias is used.

`game.render` builds/submits the same bulk batch as CLI drive. Host drive
leaves it active; C calls `zrg_end_frame` to submit to the existing null backend
and then the null platform's no-op present. CLI drive continues to end the
frame inside the script. The backend consumes each frame once. Host shutdown
reads the completed native content hash into the shared summary.

## Supported command line

| Option | Default / accepted values |
| --- | --- |
| `--project FILE` | Required legacy game project; source-first parser-enabled execution (`exec=interp`) |
| `--scenario` / `--lane` | `smoke` / `dod` only |
| `--frames` | 120; 1–120 |
| `--seed` | 12345; 1–2147483646 |
| `--render` / `--submit` | `null` / `bulk` only (mode C) |
| `--gc-policy` | `auto`; `auto`, `slack`, `manual` |
| `--gc-budget-us` | 500; 0–1000000 |
| `--gc-minor-interval` | 30; 1–120, used only by manual GC |
| `--warmup` | 0 only; every requested frame is measured |
| `--out DIR` | `out/swarm`; create parents, replace per-run artifacts |
| `--expect-hash` | Optional decimal 0–1000000006; mismatch exits 2 |
| `--help` | Print usage, exit 0 without a project |

Unsupported settings, unknown/duplicate options, invalid bounded decimal
values and missing values fail with a field-naming `SWARM_ERROR` and exit 2.
Other design knobs are rejected, not silently enabled. There is no host binary
execution switch in GD-C4; stock CLI interpreter and binary gates remain.

## GC policy and lifetime

All policies set the pause and remark budgets through the same core call as
`system.gc.set_budget`; `auto` keeps the engine's default mode and scheduling.
`slack` adds owner-thread `GcStep` calls after backend submit/present. The
extra work window is `min(max(16667 - elapsed_us, 0), gc_budget_us)`. Each step
receives the remaining window as its pause/remark budget; no-work ends the
loop, and the configured budget is restored before the next tick. A zero cap
performs no extra steps. Engine steps are cooperative, so this is no hard
wall-clock pause guarantee.

`manual` calls `system.gc.disable()` after boot and requests
`system.gc.collect("minor")` after every K completed frames (default 30).
That existing system API schedules a minor and advances one step; a request
does not promise completion in that call. `manual_requests` records requests
separately from measured collection counts. Debug/forced-GC allocation paths
can still collect despite disable; manual does not claim zero automatic or
full collections. No engine scheduling changes are needed.

All script calls and explicit core steps check protected-call results and
exception/thread status. Shutdown closes artifacts and the game, releases the
native registry, calls `State_Exit`, then the CommonState free helper. On an
error, the host resets the exception before attempting game shutdown. No tasks,
callbacks or external game sinks are started. The detector gates retain the
existing policy: leak detection disabled.

The unchanged engine produced a host timeout in memory-error-detector CLI/host
parity and intermittent Release CLI failures. The supplied moved-function
regression fails on `ed10137f` and passes with `e1d3ab19`. That focused
correction preserves function metadata at its new address during import;
the compiler publishes moved children before detaching the original storage.
The test extends `interpreter_gc`, so the core CTest inventory stays 515.
The engine correction is committed separately and merged into the demo.

The recreated engine memory-error-detector and forced-GC test caches add
`-shared-libsan` to their existing compiler flags and put the system compiler's
runtime directory on `LD_LIBRARY_PATH`. This permits the generated C shared
library tests to link with their existing strict link checks. It changes local
test configuration only. The original temporary forced-GC selection file was
unavailable; the 129-test selection is recreated from the documented test
families, including all 28 `swarm_*` checks and `interpreter_gc`. Verification
records its exact selection expression. Demo build flags retain the GD-C3
settings, and their memory-error-detector/forced-GC CTests run sequentially with
`-j1`.

Cold source imports in the memory-error-detector tree exceeded the inherited
300-second process allowance; a host failure-path run also exceeded the
900-second CTest allowance. Game/host runners now allow 600 seconds per process,
with 1800 seconds for the bad-input and host CTests. The full gate is rerun with
the same input, hash, schema and error expectations. These are functional
deadlines, with no performance acceptance.

The retained ZR unit runner also exceeded its 480-second process allowance
while discovering the staged project. It now uses the same 600-second process
allowance as the game runners and a 960-second CTest allowance. Its ten test
assertions, staged module graph and per-test limits remain unchanged.

## Measurement contract

[perf-summary.schema.json](perf-summary.schema.json) makes the design §7.4
output shape concrete, with nullable unavailable observations. The CTests use
a standard-library validator for that schema's vocabulary and cross-check the
JSON against every CSV row. The schema is also usable by a full Draft 2020-12
validator. Artifact generation belongs to the host; CLI output stays textual.

`frames.csv` has zero-based frame indices, rolling simulation hash, sprites,
frame/tick/render/backend/GC-policy microseconds, per-frame GC count/time
deltas, the VM's cumulative maximum collection duration, managed bytes, signed
GC debt, a positive managed-memory delta estimate, RSS, explicit slack steps
and manual minor requests. Timing uses `CLOCK_MONOTONIC`. `frame_us` spans poll
through policy completion; the four phase durations add to it. Snapshot reads,
RSS, CSV/JSON I/O, boot, final full hash and shutdown are outside that timing.

All frames participate in nearest-rank p50/p95/p99, max and arithmetic mean.
Budget misses count frame times strictly greater than 16667/8333 microseconds;
they establish no performance acceptance. `entities.*_max` measures live
counts at the end of each tick. Timing, GC, allocation estimates and maxima do
not enter simulation hashing. CPU/load are sampled at summary time; runs are
unpinned. `git` identifies configure-time HEAD (with a dirty suffix when
applicable), not an allocator or content fingerprint.

GC snapshots are taken only on the VM owner thread, before tick and after the
policy step, so collection deltas include automatic work during tick/render.
`gc.max_pause_us` is the VM's cumulative collection maximum and can include
boot; **`gc.p99_pause_us` is null**, because snapshots do not supply individual
pause samples. `zones_us` is empty because per-system zones are not measured.
**`alloc.exact` is false**: positive frame deltas of `managedMemoryBytes` can
miss allocations reclaimed in the same frame. RSS uses `/proc/self/statm`
(zero if unavailable); CPU/load use null on observation failure. These are
explicit best-effort fields, not invented precision.

A complete run writes one CSV row per frame and atomically renames the JSON
summary. Hash mismatches retain measured artifacts with `status=hash_mismatch`
and no success marker. Starting a loop in a reused output directory removes
the previous summary; an incomplete loop cannot leave that successful result.
All raw build/test/runtime evidence is under `/tmp/zr-swarm-gd-c4-*`.
The retained runners now respect `TMPDIR`, keeping their historical temporary
directory names inside the current task's evidence root.
