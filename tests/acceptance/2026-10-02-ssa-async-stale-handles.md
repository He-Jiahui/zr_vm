# SSA Async Stale Handles After Single-Slot Reuse

## Scope and Baseline

This test-only slice adds two functions in
`tests/task/ssa_async_stale_handle_cases.inc` and registers them in the existing
focused executable. The preceding 12-function completion-contract slice was
committed as `2b3e87b4` after root-owned MSVC CTest and independent GCC sanitizer
validation. Current production wait operations validate token and generation;
compile record operations validate job ID while holding the queue lock.
No production defect or RED/fix claim is inferred from that inspection.

The fixture preserves a copied handle A, finishes and releases its original
record, and begins B in the same single slot at the same generation. Keeping
generation fixed requires stale rejection through token/job ID, rather than
allowing a generation mismatch to hide missing identity validation.

## Test Inventory and Boundary

The focused executable now invokes 14 functions: the previous 12 plus:

- `test_wait_stale_handle_cannot_change_reused_slot`: old Wake, Cancel, Timeout,
  and Recheck must report `WAIT_NOT_FOUND` and leave B waiting. Old Resume must
  leave a READY B unconsumed; old Release must leave a RESUMED B registered
  with resume count one. Old state/count queries must not observe B. The
  current handle then releases B normally.
- `test_compile_stale_handle_cannot_change_reused_job`: old state/warm-up
  queries must not observe B; GetSnapshot must expose no B bytes; Cancel must
  leave queued and running B uncancelled; Complete must not publish through
  running B; Release must not free completed B. Every rejected operation checks
  `WAIT_NOT_FOUND`. B's identity, immutable bytes while the worker owns them,
  snapshot ownership, active count, and result hash remain correct; the current
  handle completes and releases B normally.

The terminal-state tests matter: READY allows the current wait to resume,
RESUMED allows it to release, RUNNING allows the current compile job to
complete, and COMPLETED allows it to release. An old identity cannot pass
merely because the new record's operation would otherwise be legal.

All operations use the existing always-evaluated `TEST_CHECK`; `NDEBUG`
remains defined in optimized builds. Existing initialization and handle/worker
flags guard cleanup. The tests keep the registry/queue initialized and obey
the existing requirement that current slot accesses are finished before
release. They do not claim concurrent release/reuse, registry reinitialization,
real scheduler integration, or the full 06.05 milestone.

## Owned Scratch Cleanup

Before building this slice, the old failed Clang temporary objects were removed
from the resolved owned root
`D:\tmp\zr_vm\ssa-20261002-01a0fc3b\async-contracts`:

| Relative path | Bytes removed |
| --- | --- |
| `compiler-tmp/execution_async_wait-b51776.o` | 237008 |
| `compiler-tmp/execution_compile_queue-2e382e.o` | 132568 |
| `compiler-tmp/test_ssa_async_frame_budget-f766f9.o` | 155152 |
| Total | 524728 |

Exact-path WSL process inspection showed only the inspection grep; the previous
MSVC driver was terminal. Each object also passed an exclusive `FileShare.None`
open immediately before deletion. The temporary object directory is now empty.
All logs, scripts, receipts, the seven final binaries, and successful MSVC object
caches were retained. The cleanup receipt is
`async-stale-handles/cleanup-async-contracts-receipt.json`.

## Tooling and Commands

All new artifacts, compiler temporary files, binaries, and logs are confined to
`D:\tmp\zr_vm\ssa-20261002-01a0fc3b\async-stale-handles` (WSL
`/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/async-stale-handles`). Linux uses a fixed
Linux-only PATH, `wsl.exe -e`, and this D-drive cwd. `TMPDIR`, `TMP`, and `TEMP`
point to its `compiler-tmp`. The standalone source manifest is:

```text
tests/task/test_ssa_async_frame_budget.c
zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c
zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c
```

The test source includes both focused `.inc` files. The Linux driver runs GCC
11.4 and Clang 14 in Debug and `-O2 -DNDEBUG`, then one GCC Debug ASan/UBSan job
with leak detection. It does not retry the previous Clang static-ASan linker
that stalled in DrvFs. The exact driver invocation is:

```powershell
wsl.exe -e /bin/sh -c 'PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin; export PATH; cd /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/async-stale-handles && /bin/sh ./validate-linux.sh'
```

The driver executes the following complete source command for each ordinary
configuration, with `compiler` set to `gcc` or `clang`, `flags` to `-O0 -g` or
`-O2 -DNDEBUG`, and `artifact` to the corresponding compiler/mode name:

```sh
source_root=/mnt/e/Git/zr_vm
task_dir=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/async-stale-handles
"$compiler" -std=c11 -Wall -Wextra -Werror -pedantic $flags \
    -I "$source_root/zr_vm_core/include" -I "$source_root/zr_vm_common/include" \
    "$source_root/tests/task/test_ssa_async_frame_budget.c" \
    "$source_root/zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c" \
    "$source_root/zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c" \
    -o "$task_dir/$artifact" > "$artifact-build.log" 2>&1
"./$artifact" > "$artifact-run.log" 2>&1
```

The GCC sanitizer build uses the same manifest and includes with
`-O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer`, producing
`gcc-sanitized-debug`; the exact run is:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    ./gcc-sanitized-debug > gcc-sanitized-debug-run.log 2>&1
```

MSVC 19.44 uses the existing VsDevCmd x64 environment and sets `TMP`/`TEMP`
before and after initialization. Debug compiles the full manifest; NDEBUG
compiles the new test source and reuses the preceding slice's successful
`/O2 /DNDEBUG` production runtime objects copied into this new scratch root.
`git diff --name-only 2b3e87b4` for the runtime source pair, async header, core
configuration header, and common include tree produced no differences before
reuse. Both copied object SHA-256 values match the originals; provenance and
hashes are retained in `msvc-cache-receipt.txt`. The exact invocation from the
D-drive cwd is:

```powershell
cmd.exe /d /c D:\tmp\zr_vm\ssa-20261002-01a0fc3b\async-stale-handles\validate-msvc.cmd
```

Its compiler and direct-run commands are:

```bat
set "source_root=E:\Git\zr_vm"
cl /nologo /std:c11 /utf-8 /W4 /WX /Od /Zi /TC /I"%source_root%\zr_vm_core\include" /I"%source_root%\zr_vm_common\include" "%source_root%\tests\task\test_ssa_async_frame_budget.c" "%source_root%\zr_vm_core\src\zr_vm_core\execution\execution_async_wait.c" "%source_root%\zr_vm_core\src\zr_vm_core\execution\execution_compile_queue.c" /Fo:msvc-debug-objects\ /Fe:msvc-debug.exe /Fd:msvc-debug.pdb /link /PDB:msvc-debug-link.pdb > msvc-debug-build.log 2>&1
msvc-debug.exe > msvc-debug-run.log 2>&1
cl /nologo /std:c11 /utf-8 /W4 /WX /O2 /DNDEBUG /TC /I"%source_root%\zr_vm_core\include" /I"%source_root%\zr_vm_common\include" "%source_root%\tests\task\test_ssa_async_frame_budget.c" /Fo:msvc-ndebug-test.obj /Fe:msvc-ndebug.exe /link msvc-ndebug-execution_async_wait.obj msvc-ndebug-execution_compile_queue.obj > msvc-ndebug-build.log 2>&1
msvc-ndebug.exe > msvc-ndebug-run.log 2>&1
```

The first NDEBUG driver attempt incorrectly placed cached `.obj` arguments
before `/link` while using `/TC`; MSVC therefore parsed them as C input and
returned exit 1. This driver error is retained in
`msvc-ndebug-driver-error.log`. Moving the object arguments after `/link`
corrects their role without changing tests or production code. Since Debug
had already built and run successfully, the retry uses only the NDEBUG branch:

```powershell
cmd.exe /d /c D:\tmp\zr_vm\ssa-20261002-01a0fc3b\async-stale-handles\validate-msvc.cmd ndebug
```

## Results and Acceptance

| Configuration | Compiler/link exit | Executable exit |
| --- | --- | --- |
| GCC 11.4 Debug | 0 | 0 |
| GCC 11.4 `-O2 -DNDEBUG` | 0 | 0 |
| Clang 14 Debug | 0 | 0 |
| Clang 14 `-O2 -DNDEBUG` | 0 | 0 |
| MSVC 19.44 Debug, `/utf-8 /W4 /WX` | 0 | 0 |
| MSVC 19.44 `/O2 /DNDEBUG`, cached runtime objects | 0 | 0 |
| GCC 11.4 Debug ASan + UBSan + leak detection | 0 | 0 |

All seven final executables run the 14-function focused source and return
zero. Their run logs are empty. The GCC sanitizer run emits no address,
undefined-behavior, or leak diagnostic. Successful GCC/Clang build logs are
empty; successful MSVC logs contain normal source/code-generation output.
The complete Linux driver exits zero. The initial MSVC driver exits one at
the object-input invocation error after a successful Debug build/run; the
corrected NDEBUG-only driver exits zero. No failed result is omitted or
presented as a production RED.

The stale operations all meet their exact rejection and state-preservation
expectations against unchanged production code, so no production fix was
needed. `git diff --check` and the four-file trailing-whitespace scan pass.
Focused validation is complete for this sequential stale-identity slice.
The root-agent review and independent validation below passed. No
full CMake Release build, repository-wide matrix, concurrent release/reuse,
or full 06.05 milestone completion is claimed.

## Root-Agent Review and Independent Validation

The root agent inspected the complete test include, harness registration,
guide and this acceptance record. An independent read-only review found no
remaining actionable findings for the sequential stale-handle contract.

The root rebuilt the registered native targets with the captured MSVC
environment in the reusable D-drive matrix. Build exit was 0. The exact
registered-test command was:

```text
ctest --test-dir D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc -R ^(ssa_core_roots_observation|ssa_async_frame_budget)$ --output-on-failure --no-tests=error -j 1
```

It returned 0 with 2/2 tests passing: `ssa_async_frame_budget` in 43.25 seconds
and the separately accepted `ssa_core_roots_observation` in 1.13 seconds.
The logs are `control/core-roots-async-stale-final-build.log` and
`control/core-roots-async-stale-final-ctest.log` under the owned D-drive root.

The root independently reran the final GCC ASan/UBSan executable through
`control/run_linux_artifact.py`, using the fixed Linux PATH,
`ASAN_OPTIONS=detect_leaks=1` and `UBSAN_OPTIONS=halt_on_error=1`. It returned
0 in 27.416 seconds without sanitizer or leak diagnostics. Its exact launcher
and result are retained in `control/async-stale-root-gcc-sanitizer.log`.
