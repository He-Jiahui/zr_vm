# Native Client Command Withdrawal

Plan: `optimize/00-baseline-and-contract.md`, Task 4 Sub06.

## Reproduction

On the frozen GCC binary before the repair, direct execution of
`stdio_client_commands_smoke.js` returns exit 1 with six of ten checks failed.
The actual responses for `zr.runCurrentProject`, `zr.showReferences`, and
`zr.unknown` contain `result: null` for both client profiles. Expected responses
contain only `jsonrpc`, the same string id, and error
`{code: -32601, message: "Method not found"}`. The four capability/lifecycle
checks pass. The captured RED body is in
`.codex-builds/lsp-optimize-20260905-root/logs/client-commands-red.log` under
the WSL home directory; a subsequent direct run independently confirmed exit 1.

## Acceptance

Completed 2026-09-05 21:31 +08:00. GCC 11.4, Clang 14 and MSVC 19.44
rebuilds return exit 0. Focused CTest is 14/14 on each (8.40/10.33/17.28 s),
including all 10 command fixture assertions and the retained real CodeLens
payload. The separate inventory test is exploratory Task 2 Sub02 evidence;
the other 13 tests verify this repair and existing committed protocol paths.

Full stdio smoke was rerun on all three toolchains after its old null-success
assertion was replaced with an exact MethodNotFound envelope. Each reaches
the same later historical failure: `generic completion detail should include
the normalized closed instantiation`, exit 1, line 1973. This does not invalidate
the passed command assertion and is not counted as GREEN.

The seven code/test files agree in workspace and both frozen sources across
14 SHA-256 comparisons. The removed handler is absent everywhere. Build
locations and source provenance are in the linked milestone record. Read-only
review found the obsolete comprehensive smoke assertion; the corrected diff
passed re-review with no remaining issues. No obsolete handler or constant
references remain in production.

The fixed method has no registered provider, native handler or dispatcher
branch. Client command production remains covered by the real test CodeLens
assertions in `stdio_resolve_capabilities_smoke.js`. No provider or semantic
facts are altered. Existing whole-suite semantic failures remain in the
Plan 00 baseline ledger.
