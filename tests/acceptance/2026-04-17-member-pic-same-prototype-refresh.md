# 2026-04-17 Member PIC Same-Prototype Receiver Refresh

## Scope And Owning Layers

This acceptance note covers the instruction/runtime correctness slice that follows the earlier same-prototype
pair-reuse fix.

Owning layers:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `tests/instructions/test_instructions.c`

The slice is limited to cached instance-field member access at the interpreter instruction layer:

- `GET_MEMBER_SLOT`
- `SET_MEMBER_SLOT`
- PIC receiver/object/pair refresh
- generational remembered-set convergence after the hot receiver changes

## Baseline Before Change

Before this slice:

- exact `cachedReceiverPair` reuse across different receivers with the same prototype had already been fixed
- same-prototype different-receiver accesses were now safe because they fell back to object lookup
- but successful safe object-lookup hits did **not** refresh the PIC slot to the current receiver

That left one correctness/performance boundary open:

1. cache young receiver `A`
2. later hit permanent receiver `B` with the same prototype
3. return the correct value through safe object lookup
4. still keep `cachedReceiverObject` / `cachedReceiverPair` pinned to `A`

Consequences:

- the next steady-state hit on `B` could not converge onto the exact pair-hit lane
- the old function/callsite could keep remembered metadata for `A` even though the hot receiver had already switched to `B`

## Red-State Repro

Two new instruction regressions were added first:

- `test_get_member_slot_instruction_same_prototype_safe_hit_refreshes_cached_receiver_before_minor_gc_prune`
- `test_set_member_slot_instruction_same_prototype_safe_hit_refreshes_cached_receiver_before_minor_gc_prune`

Both tests build the same boundary:

1. a shared prototype with field `value`
2. young instance `A`
3. permanent instance `B`
4. old/movable function object holding the PIC
5. first execution populates the PIC from `A`
6. later executions hit `B`
7. expected behavior:
   - slot refreshes from `A` to `B`
   - `cachedReceiverPair` matches `B->cachedStringLookupPair`
   - after minor GC restart, the remembered entry for the function is pruned

Observed red state on WSL gcc before the runtime fix:

- GET test failed because `cachedReceiverObject` stayed on `A`
- SET test failed because `cachedReceiverObject` stayed on `A`

## Runtime Change

`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c` now adds:

- `execution_member_refresh_cached_instance_field_slot(...)`

The helper is invoked after successful cached instance-field object-lookup hits in four places:

1. single-slot GET fallback hit on the same receiver object
2. prototype-matched GET fallback hit on a different receiver object
3. single-slot SET fallback hit on the same receiver object
4. prototype-matched SET fallback hit on a different receiver object

The refresh is intentionally routed back through `execution_member_store_pic_slot(...)` so the PIC rewrites:

- `cachedReceiverObject`
- `cachedReceiverPair`
- the existing barrier/remembered bookkeeping

This keeps the refresh path aligned with the normal PIC storage contract instead of inventing a partial side update.

## Boundary Coverage

The accepted slice now locks these boundaries together:

- same-prototype different-receiver accesses still do **not** reuse a foreign pair
- successful safe object-lookup hits refresh the slot to the current hot receiver
- `cachedReceiverPair` follows the refreshed receiver instead of staying attached to the previous object
- repeated hits on permanent `B` can converge onto the exact pair-hit lane
- minor GC can prune the stale remembered entry once the slot no longer points at young `A`

## Test Inventory

### Red/Green Proof

WSL gcc red-state proof before the runtime fix:

```powershell
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_instructions_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_instructions_test'
```

Observed:

- `90 Tests 2 Failures 0 Ignored`
- the two new failures were exactly the new GET/SET same-prototype refresh regressions

### Accepted Targeted Matrix

WSL gcc:

```powershell
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_instructions_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_instructions_test'
```

Result:

- `90 Tests 0 Failures 0 Ignored`

WSL clang:

```powershell
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_instructions_test -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_instructions_test'
```

Result:

- `90 Tests 0 Failures 0 Ignored`

Windows MSVC:

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake --build build\codex-msvc-debug --config Debug --target zr_vm_instructions_test --parallel 8
.\build\codex-msvc-debug\bin\Debug\zr_vm_instructions_test.exe
```

Result:

- `90 Tests 0 Failures 0 Ignored`

## Full-Repo Validation Snapshot

For context, full WSL `ctest` was also re-run in the current dirty checkout:

```powershell
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-gcc-debug --output-on-failure --parallel 4'
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-clang-debug --output-on-failure --parallel 4'
```

Observed on both gcc and clang:

- `42% tests passed, 11 tests failed out of 19`
- same failing suite set across both toolchains:
  - `core_runtime`
  - `language_pipeline`
  - `containers`
  - `language_server`
  - `projects`
  - `escape_pipeline`
  - `cli_debug_e2e`
  - `debug_metadata`
  - `system_fs`
  - `debug_agent`
  - `cli_integration`

The visible failures include the already-broken current-worktree surfaces such as:

- `decorator_import` project case
- parser/compiler integration failures in `language_pipeline`
- `native_closure_value`
- `escape_pipeline`
- `debug_metadata`

These full-suite failures are therefore recorded as the current checkout baseline and are **not** used as acceptance
evidence for this instruction-layer slice.

## Acceptance Decision

Accepted for the `GET_MEMBER_SLOT` / `SET_MEMBER_SLOT` PIC correctness slice.

The change is accepted because:

1. the targeted boundary was proven red first
2. the runtime change is minimal and local to cached instance-field slot refresh
3. the new GET/SET regressions pass on WSL gcc, WSL clang, and Windows MSVC
4. earlier same-prototype pair-reuse guards remain covered alongside the new adaptive-refresh behavior

This acceptance does **not** claim the repository is globally green.
The current checkout still has broader full-suite failures outside the accepted instruction-layer surface.
