# 11-S7ZR / 12-S7 Range-Selected Provider Export Publication

Date: 2026-07-02 09:15 +08:00

Status: completed support slice. Full 11-S7/12-S7 remain open: complete metadata sweep/pruning, full trim analyzer,
annotation policy, compacted-token file publication, and broader ABI drift/deopt closure still need later work.

Completed:

- Provider shared-library smoke now imports the same provider through exact `$mathLocal@2.1.0/ops/sum` and
  range-selected `$mathRange@2.1.0/ops/sum` aliases.
- `mathRange` uses 2.0.5 and 2.1.0 candidates and selects the 2.1.0 provider.
- Runtime import now publishes the ordinary public `seed` export and attached manifest export metadata for the
  range-selected alias.
- `ZrLibrary_AotRuntime_PublishModuleExports()` prefers a matching `runtimeState->activeRecord` before falling back to
  frame handle / function-equivalence lookup, so equivalent generated functions do not publish exports to an earlier
  alias record.

RED:

- WSL GCC provider smoke failed for `$mathRange@2.1.0/ops/sum`: manifest export views existed, but
  `ZrCore_Module_GetPubExport(..., "seed")` returned NULL because export publication selected the earlier exact alias
  record.

GREEN:

- WSL GCC/clang provider shared-library smoke: 1/0.
- WSL GCC/clang/MSVC Debug provider version-selection 4/0, resolver 9/0, manifest normalization 28/0, provider runtime
  1/0, source contracts 24/0, frame setup contracts 1/0.
- MSVC provider shared-library smoke builds and remains 1 ignored on the Unix-only dynamic-loader branch.
- `git diff --check` clean for the focused code/test changes, except LF/CRLF warnings in pre-existing mixed-line-ending
  files.

Notes:

- This slice closes range-selected provider runtime export publication only. It does not close full provider ABI
  drift/deopt, complete metadata sweep/pruning, or the full trim analyzer.
