---
plan_id: optimize
task: plan01-task06-sub12
status: completed
related_code:
  - tests/language_server/stdio_smoke.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
doc_type: plan-record
---

# Plan 01 Task 6 Sub12: Smoke Budget Teardown

## 状态与产出记录

- Started: 2026-09-08 00:44 +08:00
- Completed: 2026-09-08 00:49 +08:00
- Source: 52727f69 plus shared working-tree changes.
- Status: budget-failure finalization order accepted.
- Outputs: full-smoke finalization order and validation documentation.
- Remaining gates: Clang full-smoke 512 MiB peak-memory acceptance.

## Evidence and Scope

The full smoke samples/asserts peak RSS after shutdown returns but before it
sends exit. An over-budget result therefore prevents waitForExit and stderr
checks. Sub09's Clang run was an example; its earlier record incorrectly called
this pre-shutdown, while the actual code is post-shutdown and pre-exit.

Separate final OS sampling from budget assertion. Keep sampling while the
process exists, then send exit and verify process status/stderr before applying
the unchanged memory budget to the captured peak. This preserves budget failure
while allowing sanitizer teardown evidence to be collected.

A stricter one-byte budget on the existing GCC full smoke is the deterministic
negative case. A preload probe records actual shutdown/exit sends, process close,
exit status and stderr. Normal three-toolchain replay retains the default
512 MiB limit and existing sanitizer options.

## RED and Validation

All logs and the lifecycle probe are under .codex/lsp-optimize-validation with
the plan01-task06-sub12- prefix. The probe wraps the real protocol client's
sendPayload and onClose methods, recording shutdown/exit, close status and stderr.
It does not modify protocol requests, responses or server behavior.

| Check | Before | After |
| --- | --- | --- |
| GCC, one-byte budget | smoke exit 1 | smoke exit 1 |
| shutdown sent | true | true |
| exit sent | false | true |
| child close observed | false | true |
| child status / stderr | unavailable | 0 / empty |

The stricter limit remains a failure after the fix. The default-budget full
smoke passes on GCC with 38,825,984 bytes (37.03 MiB) and MSVC with 47,251,456
bytes (45.06 MiB), both exit 0. Clang ASan/UBSan/LSan completes every preceding
assertion and explicit exit with child status 0 and empty stderr. Its captured
peak is 701,116,416 bytes (668.64 MiB), so the smoke correctly exits 1 at the
unchanged 512 MiB assertion. This is complete teardown evidence but not complete
memory-budget acceptance.

```text
node tests/language_server/stdio_smoke.js <build>/bin/zr_vm_language_server_stdio <build>/bin/zr_vm_cli
ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES=1 ZR_LSP_LIFECYCLE_PROBE_PATH=<probe.json> node -r <validation>/plan01-task06-sub12-lifecycle-probe.js tests/language_server/stdio_smoke.js <gcc-build>/bin/zr_vm_language_server_stdio <gcc-build>/bin/zr_vm_cli
```

Toolchain paths match Sub09/Sub10. GCC's ordinary replay uses Node 22.13.1;
Clang uses the same Node plus the read-only lifecycle probe. MSVC uses the local
Node and the rebuilt Debug server/CLI. No sanitizer option, latency assertion
or default memory limit changed. All spawned build and smoke commands completed.

The existing large smoke file receives only the separation of final sampling
and assertion; no new scenario or responsibility is added. Stage its two-line
change, the module/index documentation, this record and plan/index entries,
plus the correction of Sub09's historical shutdown/exit wording.
