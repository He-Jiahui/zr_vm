# SSA 01.05: oracle/ExecBC memory-provider differential

## Scope

The parser's pointer-free ExecBC projection runner now executes `LOAD` and
`STORE` through a caller-owned memory provider, preserving source/instruction
identity and emitting owned operand snapshots in execution order. The direct
ExecIR oracle uses its existing, separate provider on the same function and
an identical independent initial memory state. Neither path is the VM's
default ExecBC dispatcher; this does not enable AOT C/LLVM or runtime heap
access.

## Baseline

The oracle already supported provider-backed `LOAD/STORE`, while the ExecBC
projection copied memory tokens but set `runnable = false`. The new test
first failed to compile on missing ExecBC memory input and result event
fields. After the runner was added, `VERIFY_ALL` caught two fixture mistakes:
a missing nonzero function ID (`INVALID_ARGUMENT`) and a `STORE` missing its
required `MAY_THROW` flag (`EXCEPTION_EDGE`); both were fixed in the fixture,
along with its managed-heap version and effect chain. GCC's initial capacity
check triggered a 64-bit type-limits warning, Clang flagged test locals in a
short-circuit setup, and MSVC flagged an uninitialized instruction value;
these new warnings were addressed before final builds.

## Test Inventory

- `test_oracle_execbc_memory_differential` builds a single-block
  CONSTANT/CONSTANT/STORE/LOAD/RETURN function, validates STRUCTURE, SSA and
  EFFECT, checks `STORE` effect 1->2 and a tagged managed-heap memory version
  consumed by `LOAD`, then lowers the same input to runnable ExecBC.
- Both runners execute distinct caller-owned memory records initialized
  identically. The required backend coverage bits are oracle (0) and
  projection runner (1). Results and memory states both equal 42; one store
  and one load occur in each provider. Ordered WRITE/GET/RETURN events compare
  source IDs 503/504/505, STORE address/value 7/42, and LOAD/return values
  7/42. A separate `VERIFY_ALL` STORE-only fixture executes successfully on
  both backends without a provider and compares its write/return events.
- Corrupting the actual event kind or recorded STORE address produces
  `EVENT_MISMATCH` at index zero.
  A missing `LOAD` provider reports UNSUPPORTED at instruction 4/source 504;
  rejected STORE and LOAD callbacks report ORACLE_MEMORY_ERROR at their
  respective instruction/source; an undefined loaded value reports
  INVALID_VALUE at instruction 4/source 504. Each rejected run preserves
  earlier published slot and event allocations. A later successful rerun
  publishes a fresh result with two memory events.
- Adjacent `ssa_oracle_parallel_edges` and `ssa_differential_harness` cases
  continue to cover CFG/phi and generic event/coverage errors. No source-level
  runtime memory, AOT binary or full effect/ownership suite is claimed here.

## Tooling Evidence

GNU 11.4.0 and Clang 14.0.0 rebuilt the affected parser shared library and
the three relevant test targets; the GCC Debug registered SSA-label run only
freshly rebuilt those direct targets. GNU 11.4.0 ASan/UBSan used
`-fsanitize=address,undefined -fno-omit-frame-pointer`. MSVC 19.44.35228.0
used the local `Invoke-VsDevCommand.ps1` wrapper to rebuild the parser static
library and three test targets. Exact commands from the repository root:

```text
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/ssa-gcc-debug --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_oracle_parallel_edges_test zr_vm_ssa_differential_harness_test zr_vm_parser_shared -j 4 && ctest --test-dir build/ssa-gcc-debug -L ssa --output-on-failure --no-tests=error -j 4'
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/ssa-clang-debug --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_oracle_parallel_edges_test zr_vm_ssa_differential_harness_test zr_vm_parser_shared -j 4 && ctest --test-dir build/ssa-clang-debug -R "^(ssa_oracle_projections|ssa_oracle_parallel_edges|ssa_differential_harness)$" --output-on-failure --no-tests=error'
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/ssa-gcc-asan-phase80 --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_oracle_parallel_edges_test zr_vm_ssa_differential_harness_test -j 4 && ctest --test-dir build/ssa-gcc-asan-phase80 -R "^(ssa_oracle_projections|ssa_oracle_parallel_edges|ssa_differential_harness)$" --output-on-failure --no-tests=error'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build build/ssa-msvc-debug --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_oracle_parallel_edges_test zr_vm_ssa_differential_harness_test zr_vm_parser_static --config Debug -j 4
ctest --test-dir build/ssa-msvc-debug -C Debug -R '^(ssa_oracle_projections|ssa_oracle_parallel_edges|ssa_differential_harness)$' --output-on-failure --no-tests=error
```

The first GCC matrix attempt named a static parser target absent from its
shared-library configuration; inspecting the available targets established
`zr_vm_parser_shared` as the correct build target. The mistaken command did
not compile code or execute tests. The initial MSVC configuration warned
about existing long object paths and the project flags overriding `/W3` with
`/W4`; neither warning is caused by this memory slice.

## Results

The final GNU Debug registered SSA sweep passed 80/80. Clang Debug,
MSVC Debug and GCC ASan/UBSan each passed the focused matrix 3/3. No new
compiler diagnostic remained in the final changed-file rebuild, and the
sanitizer tests exited without reported memory/undefined-behavior errors.

## Acceptance Decision

Accepted for pointer-free provider-backed LOAD/STORE plus ordered
write/read/return differential, diagnostic identity, and transactional result
publication. External provider mutations are callback-owned and are not
rolled back after a later execution failure, as in the direct oracle. Event
allocation OOM is diagnosed and frees the candidate but has not been fault
injected here; cancellation, leases and drop balance are outside this fixture.
Calls, ownership/drop, exceptions, suspend, default production ExecBC,
AOT C/LLVM parity and the complete 01.05/M1 gates remain open.
