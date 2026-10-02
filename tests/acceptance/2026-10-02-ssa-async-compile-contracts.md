# SSA Async Compilation Contract Completion and Slot Recovery

## Scope

This test-only change adds
`tests/task/ssa_async_compile_contract_cases.inc` to the existing focused
`test_ssa_async_frame_budget.c` executable. Production frame, wait, compile
queue, and scheduler code is unchanged. The added function checks four
same-generation completion failures followed by a successful completion in
one queue slot. All operations and expectations use the existing
always-evaluated `TEST_CHECK`, including builds that define `NDEBUG`.

## Baseline

`ZrCore_CompileQueue_Complete` already rejects a changed module, signature, or
layout hash and a zero result hash. The existing focused tests cover generation
changes, queued/running cancellation, malformed snapshots, and request identity;
they do not exercise these four completion contract failures or subsequent
successful reuse of their slot. This is coverage of existing production
behavior, not a production RED/fix claim. The agent's focused validation does
not change a repository test registration file.

## Test Inventory

The executable runs its 11 existing functions plus
`test_compile_queue_rejects_contract_changes_and_reuses_slot`. That function
keeps the requested generation fixed at 21 and completes five jobs in a
single-slot queue:

| Completion input | Expected completion |
| --- | --- |
| Changed module hash only | `CONTRACT_MISMATCH`, `DISCARDED`, result hash zero |
| Changed signature hash only | Same rejection |
| Changed layout hash only | Same rejection |
| Zero result hash only | Same rejection |
| All identities match; result hash 404 | `NONE`, `COMPLETED`, result hash 404 |

Each job is claimed by the worker before completion. Rejected results retain
the immutable snapshot and one active record until `Release`; the worker lease
ends at completion. Every release must restore zero active jobs, `FREE` state,
and a null snapshot with zero length and result hash. Later jobs must reuse
slot zero with increasing job IDs. The final valid completion demonstrates
that rejection and release do not prevent subsequent publication.

The existing cleanup helper remains responsible for acknowledging any active
worker and releasing a terminal handle after a failed check. Deinitialization
is guarded by successful initialization and no remaining handle or worker.

## Tooling Evidence

All scripts, compiler temporary files, object files, binaries, PDBs, and logs
are under
`D:\tmp\zr_vm\ssa-20261002-01a0fc3b\async-contracts` (WSL
`/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/async-contracts`). The drivers compile
the existing focused target's standalone source set:

```text
tests/task/test_ssa_async_frame_budget.c
zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c
zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c
```

Both include directories are `zr_vm_core/include` and `zr_vm_common/include`.
The test source includes the new `.inc` directly. GCC/Clang builds use
`-std=c11 -Wall -Wextra -Werror -pedantic`; Debug uses `-O0 -g`, and the
optimized configuration uses `-O2 -DNDEBUG` without undefining `NDEBUG`.
The GCC AddressSanitizer and UndefinedBehaviorSanitizer build additionally uses
`-fsanitize=address,undefined -fno-omit-frame-pointer`, with leak detection and
halt-on-error enabled. MSVC builds use `/std:c11 /utf-8 /W4 /WX /TC`, with
`/Od /Zi` or `/O2 /DNDEBUG`.

Tool versions from the retained version logs are Ubuntu GCC 11.4.0, Ubuntu
Clang 14.0.0, and MSVC 19.44.35228 for x64. The MSVC version-only `cl /Bv`
invocation also reports D8003 because it has no source input; it is not one of
the compilation checks below.

Exact Linux driver invocation from PowerShell, with the process working
directory set to the D-drive evidence directory:

```powershell
wsl.exe -e /bin/sh -c 'PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin; export PATH; cd /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/async-contracts && /bin/sh ./validate-linux.sh'
```

The saved driver sets `TMPDIR`, `TMP`, and `TEMP` to `compiler-tmp` below its
D-drive working directory, and runs the following complete source command
for each compiler/configuration:

```sh
source_root=/mnt/e/Git/zr_vm
task_dir=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/async-contracts
# compiler: gcc or clang; mode_flags: -O0 -g or -O2 -DNDEBUG
# artifact: gcc-debug, gcc-ndebug, clang-debug, or clang-ndebug
"$compiler" -std=c11 -Wall -Wextra -Werror -pedantic $mode_flags \
    -I "$source_root/zr_vm_core/include" \
    -I "$source_root/zr_vm_common/include" \
    "$source_root/tests/task/test_ssa_async_frame_budget.c" \
    "$source_root/zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c" \
    "$source_root/zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c" \
    -o "$task_dir/$artifact" > "$artifact-build.log" 2>&1
"./$artifact" > "$artifact-run.log" 2>&1
# The original driver also attempted a Clang Debug sanitizer build, adding:
# -fsanitize=address,undefined -fno-omit-frame-pointer
# Its link was stopped as described below, so this sanitizer run was not reached:
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    "./$artifact" > "$artifact-run.log" 2>&1
```

The Clang static-ASan link waited in WSL DrvFs `p9_client_rpc`; the retained
stop snapshot shows linker elapsed time 645 seconds. Before stopping only the
owned processes, the cleanup driver
verified each PID, process start ticks, parent PID, and the exact D-drive working
directory: linker 54112 (start 6781312, parent 54047), compiler 54047 (start
6778701, parent 53292), and this Linux driver 53292 (start 6734380, parent
53291). The evidence is retained in `clang-sanitizer-stopped.log`. TERM was
sent only to those three verified identities; the original driver ended with
exit 1 and `Terminated`. Clang sanitizer Debug is not counted as passed, and
its planned sanitizer NDEBUG iteration was never reached.

The replacement uses one GCC Debug sanitizer job with the same source set,
includes, D-drive cwd, and temporary-directory settings:

```powershell
wsl.exe -e /bin/sh -c 'PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin; export PATH; cd /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/async-contracts && /bin/sh ./validate-gcc-sanitizer.sh'
```

```sh
gcc -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$source_root/zr_vm_core/include" \
    -I "$source_root/zr_vm_common/include" \
    "$source_root/tests/task/test_ssa_async_frame_budget.c" \
    "$source_root/zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c" \
    "$source_root/zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c" \
    -o "$task_dir/gcc-sanitized-debug" > gcc-sanitized-debug-build.log 2>&1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    ./gcc-sanitized-debug > gcc-sanitized-debug-run.log 2>&1
```

Exact MSVC driver invocation from the same D-drive working directory:

```powershell
cmd.exe /d /c D:\tmp\zr_vm\ssa-20261002-01a0fc3b\async-contracts\validate-msvc.cmd
```

The saved driver sets `TEMP` and `TMP` before and after calling
`E:\Visual Studio\Common7\Tools\VsDevCmd.bat -no_logo -arch=x64 -host_arch=x64`.
Its two full compiler and execution commands are:

```bat
set "source_root=E:\Git\zr_vm"
cl /nologo /std:c11 /utf-8 /W4 /WX /Od /Zi /TC /I"%source_root%\zr_vm_core\include" /I"%source_root%\zr_vm_common\include" "%source_root%\tests\task\test_ssa_async_frame_budget.c" "%source_root%\zr_vm_core\src\zr_vm_core\execution\execution_async_wait.c" "%source_root%\zr_vm_core\src\zr_vm_core\execution\execution_compile_queue.c" /Fe:msvc-debug.exe /Fd:msvc-debug.pdb /link /PDB:msvc-debug-link.pdb > msvc-debug-build.log 2>&1
msvc-debug.exe > msvc-debug-run.log 2>&1
cl /nologo /std:c11 /utf-8 /W4 /WX /O2 /DNDEBUG /TC /I"%source_root%\zr_vm_core\include" /I"%source_root%\zr_vm_common\include" "%source_root%\tests\task\test_ssa_async_frame_budget.c" "%source_root%\zr_vm_core\src\zr_vm_core\execution\execution_async_wait.c" "%source_root%\zr_vm_core\src\zr_vm_core\execution\execution_compile_queue.c" /Fe:msvc-ndebug.exe /link /PDB:msvc-ndebug-link.pdb > msvc-ndebug-build.log 2>&1
msvc-ndebug.exe > msvc-ndebug-run.log 2>&1
```

The first MSVC Debug attempt omitted `/utf-8`; default code page 936 produced
C4819 for existing UTF-8 Chinese comments and `/WX` made this a build failure
(exit 1). Its output is retained in `msvc-codepage-build.log`. The driver was
corrected to specify the repository source encoding; no source was changed to
silence warnings.

## Results

| Configuration | Compiler exit | Executable exit |
| --- | --- | --- |
| GCC 11.4 Debug | 0 | 0 |
| GCC 11.4 `-O2 -DNDEBUG` | 0 | 0 |
| Clang 14 Debug | 0 | 0 |
| Clang 14 `-O2 -DNDEBUG` | 0 | 0 |
| MSVC 19.44 Debug, `/utf-8 /W4 /WX` | 0 | 0 |
| MSVC 19.44 `/O2 /DNDEBUG`, `/utf-8 /W4 /WX` | 0 | 0 |
| GCC 11.4 Debug ASan + UBSan + leak detection | 0 | 0 |

All seven final executables ran the 12-function focused test source and
returned zero. Their run logs are empty; the GCC sanitizer run reports no
address, undefined-behavior, or leak diagnostic. GCC/Clang successful build
logs are empty, and MSVC successful build logs contain only source names and
code-generation status. Every completed ordinary GCC/Clang build and direct
run returned zero; the corrected MSVC matrix driver and the replacement GCC
sanitizer driver also returned zero. The original combined Linux driver was subsequently
stopped with exit 1 at the Clang static-ASan link as recorded above; that
incomplete check is not presented as a passing sanitizer result.

`git diff --check` passed for the tracked test and runtime documentation.
A direct trailing-whitespace scan of all four changed/new files found none.
No production fix or test registration change was performed.

## Root independent validation

The root agent reviewed the four-file change and built the existing formal
repository target in a fresh MSVC Debug CMake tree at
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc`. The target build exited zero;
the registered `ssa_async_frame_budget` CTest passed in 7.90 seconds. Logs are
`control/independent-targets-build-v2.log` and
`control/independent-targets-ctest.log` under the same artifact root. This
five-test run passed four tests and timed out in the separate generation test;
it is not reported as a passing whole suite.

The root also independently executed the final GCC ASan/UBSan binary with leak
detection enabled, using:

```powershell
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/run_linux_artifact.py D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/async-root-gcc-sanitizer.log D:/tmp/zr_vm/ssa-20261002-01a0fc3b/async-contracts/gcc-sanitized-debug
```

That run exited zero with no address, undefined-behavior, or leak diagnostic.
Root compilation, execution, logs and temporary directories all remained on D.

## Acceptance Decision

Focused validation and root independent review passed for this test-only
completion-contract and slot recovery slice. The three compiler families
pass in both Debug and `NDEBUG`
configurations, with memory/UB tooling supplied by the successful GCC sanitizer
run. Clang static-ASan on this WSL DrvFs environment remains an explicitly
incomplete alternative tool run. This record does not claim the full 06.05
milestone, real scheduler integration, full CMake Release, or a repository-wide
matrix.
