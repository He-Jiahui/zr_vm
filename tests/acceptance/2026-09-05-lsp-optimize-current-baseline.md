# LSP Optimize Current Baseline

## Scope

Collect every configured `language_server` aggregate executable after the
committed native/Web resolve and navigation capability corrections. The
collector and this evidence do not change semantic production behavior.

## Baseline

Source `670e3cd0bba9b2cae2a88fa6ea1f5e6be0e7160f`, GCC 11.4 Debug shared,
WSL Node 12.22.9, exact original gitlinks, independent exported source/build.
The first aggregate stops at semantic_analyzer; project_features emits 14
failure blocks while returning 0. Concurrent semantic/runtime overlays are not
part of this source version.

## Test Inventory

The collector reads CTest's structured executable inventory and runs all 83
aggregate members. It retains full failure blocks and exits nonzero for failed
processes or printed failure markers. Current results are 73 passing and 10
failing executables. The separate reaching-definition binary fails 3 cases;
stdio generic-detail and diagnostic-fix failures are also tracked.

## Tooling Evidence

Commands, individual failures, source identity, ownership and tooling are in the
[Plan 00 record](../../docs/plans/lsp/optimize/2026-09-05-plan00-task01-sub02-gcc-baseline.md)
and its [JSON output](../../docs/plans/lsp/optimize/2026-09-05-plan00-task01-sub02-gcc-failures.json).
The collector is [collect_lsp_baseline.js](../language_server/collect_lsp_baseline.js).
It runs against native executable outputs; failure results are expected evidence,
not a passing semantic suite.

## Results

Resolve and navigation capability leaves have separate three-toolchain focused
evidence and independent review records. Extension unit tests pass 39/39.
Current semantic, protocol preparation-timeout and worker strict-type failures
remain explicitly assigned in the baseline record.

## Acceptance Decision

Baseline collection is reproducible; overall semantic/LSP acceptance remains
blocked by the recorded failures and the pending integrated source version.
No later-stage performance, sanitizer, WASM or real-editor gate is claimed here.
