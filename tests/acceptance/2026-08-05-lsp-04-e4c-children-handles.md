# LSP 04 E4c Generation-Checked Children Handles

## Scope

- Affected layers: debug agent lifecycle, paused-state variable snapshot lookup, and the
  `zrdbg/1` TCP protocol test.
- `variablesReference` values created before `continue` must not resolve at a later stop.

## Baseline

- Before the repair, `zr_debug_variable_handles_clear` reset the allocator to `1000`.
- The new GCC protocol case evaluated arrays at two stop generations and observed
  `firstReference == secondReference == 1000`; Unity reported 8 tests / 1 failure with
  `Expected 1000 to be not equal to 1000`.
- The failure was a lower-layer debug lifecycle defect, not a language-server fallback or a
  parser diagnostic issue.

## Test Inventory

- Two paused generations: evaluate an expandable array before resume and another after a
  breakpoint stop.
- Identity boundary: assert that the two `variablesReference` values differ.
- Negative protocol boundary: submit the first handle after the second stop and require
  JSON-RPC `-32002` / `failed to read variables`.
- Existing protocol coverage: initialize, manifest, pause, evaluate capabilities,
  disconnect, raw socket close, and reconnect/pause.

## Tooling Evidence

| Toolchain | Build directory | Command | Result |
|---|---|---|---|
| GCC 11.4 | `.codex/build-e4c-stale-handles-gcc` | `./.codex/build-e4c-stale-handles-gcc/bin/zr_vm_debug_agent_protocol_test` | 8/8, 0 failures, exit 0 |
| Clang 14 | `.codex/build-e4c-stale-handles-clang` | `./.codex/build-e4c-stale-handles-clang/bin/zr_vm_debug_agent_protocol_test` | 8/8, 0 failures, exit 0 |
| MSVC 19.44 | `.codex/build-e4c-stale-handles-msvc` | `.\\.codex\\build-e4c-stale-handles-msvc\\bin\\zr_vm_debug_agent_protocol_test.exe` | 8/8, 0 failures, exit 0 |

All three targets were configured as Debug shared-library builds and built from source before
their direct executable run. The MSVC run took about 148 seconds because its protocol cases
use real TCP timeouts; its Unity summary and process exit were both successful.

## Results

- The agent now assigns one monotonic handle sequence for its full lifetime.
- Clearing a paused state no longer reuses an old number.
- Unsigned wrap is unavailable rather than a reused capability.
- The reserved high handle range cannot be decoded as a scope when no current handle exists.

## Acceptance Decision

Accepted at 2026-08-05 13:15 +08:00. The observed stale-handle alias has a RED/GREEN
regression test and passes on all required toolchains. The change introduces no LSP name,
text, AST, or raw-pointer fallback.
