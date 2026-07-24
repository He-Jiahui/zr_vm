# Syntax 07A M1 Reference Fixture Manifest

## Scope

- Added the discovery-only `syntax_reference_v1` project fixture, its coverage manifest, source slots,
  negative inputs, provider/package/artifact placeholders, and path-neutral golden files.
- Added `zr_vm_syntax_reference_v1_test` for fixture discovery, feature-id stability, collection gates,
  provider identity, and formatted/minified writer evidence.
- Affected layers are the parser test harness, test CMake wiring, project fixture inventory, and testing
  documentation. No parser, runtime, AOT, CLI, or language-server production behavior changed.

## Baseline

- The new focused target first failed while `golden/coverage.json` was absent, as intended by the discovery
  RED step.
- It then failed while the `src/engine/render.zr` provider slot was absent.
- With a deliberate `return 8` change in `host.min.zr`, the generated fingerprint comparison failed.
- A review-driven RED then found the missing feature-to-collection mapping and absent current-source syntax
  evidence. The fixture now records each feature's collection, rejects overlapping file lists, and parses
  every current source after checking its advertised syntax marker.
- The same review showed that expression-bodied anonymous functions are not in the current grammar surface.
  The exact `fn(...): T => expression` form is now a 06B design-pending source, rather than a block-bodied
  lambda mislabeled as current evidence.
- After restoring `return 7`, `.zrs` bytes matched, while `.zri` differed only in `START_LINE` and
  `END_LINE` source-map metadata caused by formatting. The semantic fingerprint therefore excludes exactly
  those two metadata fields and retains all remaining intermediate output.
- Existing GCC warnings in unrelated parser/library files and the MSVC `/W3` to `/W4` override warning were
  observed during builds; neither caused a target failure.

## Test Inventory

- `test_syntax_reference_v1_manifest_enumerates_stable_feature_ids`: exactly one instance of each of the 29
  stable feature identifiers.
- `test_syntax_reference_v1_separates_current_negative_and_pending_collections`: current, negative, and
  design-pending collection boundaries; every pending feature requires owner plan, gate, and promotion
  expectation; required skeleton files exist.
- `test_syntax_reference_v1_feature_mappings_match_disjoint_source_collections`: each feature's status,
  collection, and source are bound in one manifest entry, and the source belongs to exactly one collection.
- `test_syntax_reference_v1_current_feature_sources_contain_syntax_evidence_and_parse`: every current slot
  contains the advertised syntax marker and parses before it may serve as current fixture evidence.
- `test_syntax_reference_v1_pending_expression_arrow_preserves_its_target_syntax`: the 06B-only anonymous
  expression form remains exact fixture data without being promoted into the current parser collection.
- `test_syntax_reference_v1_preserves_provider_identity_without_host_paths`: Workspace `engine.render` and
  `native:engine.render` stay distinct, the project manifest contains the native provider declaration, and
  the locator golden contains only the URI placeholder rather than a host path. The focused target also
  derives a real local canonical `file:` URI, including the UNC `file://host/share/...` authority form, and
  rejects a placeholder or backslash path at runtime.
- `test_syntax_reference_v1_formatted_and_minified_current_source_have_identical_ast_and_semantic_hashes`:
  both source forms parse and compile, `.zrs` hashes match byte-for-byte, and `.zri` hashes match after only
  source-line mapping fields are excluded.
- Manifest static audit: 29 features total, 13 current, 1 negative, 15 design-pending, and zero pending
  entries missing a promotion gate.

## Tooling Evidence

- GCC 11.4.0, isolated build `.codex/build-s07a-m1-gcc`:
  `wsl.exe -d Ubuntu-22.04 -- bash -lc 'cmake --build .codex/build-s07a-m1-gcc --target zr_vm_syntax_reference_v1_test -j2 && ./.codex/build-s07a-m1-gcc/bin/zr_vm_syntax_reference_v1_test'`
  completed with 7 tests, 0 failures; `ctest --test-dir .codex/build-s07a-m1-gcc -R "^syntax_reference_v1$" --output-on-failure`
  passed 1/1.
- Clang 14.0.0, isolated build `.codex/build-syntax06a-m2-clang`:
  `wsl.exe -d Ubuntu-22.04 -- bash -lc 'cmake --build .codex/build-syntax06a-m2-clang --target zr_vm_syntax_reference_v1_test -j2 && ./.codex/build-syntax06a-m2-clang/bin/zr_vm_syntax_reference_v1_test'`
  completed with 7 tests, 0 failures; its focused CTest entry passed 1/1.
- MSVC 19.44.35228, isolated build `.codex/build-syntax06a-m2-msvc`:
  `cmake --build .codex/build-syntax06a-m2-msvc --target zr_vm_syntax_reference_v1_test -j2`, followed by
  `./.codex/build-syntax06a-m2-msvc/bin/zr_vm_syntax_reference_v1_test.exe`, completed with 7 tests,
  0 failures; the focused CTest entry passed 1/1.
- PowerShell `ConvertFrom-Json` audit reported `feature_count=29`, `current=13`, `negative=1`,
  `design_pending=15`, `pending_missing_gate=0`, and `mapping_mismatch=0`.

## Results

- The final source is covered by the same seven focused assertions and one focused CTest entry on all three
  toolchains.
- The fixture has no host-specific path in its provider locator golden and does not promote any
  design-pending feature to current evidence. Its local URI check proves canonical runtime provenance while
  preserving the checked-in placeholder boundary.
- The expected RED cases were resolved by adding the missing skeleton paths and restoring semantically
  equivalent source. The writer check was narrowed only to remove source-line map metadata.
- Independent read-only review found and drove the mapping, evidence, pending-expression, and CTest fixes;
  its final re-review reported no remaining actionable issue.

## Acceptance Decision

- Accepted for Syntax 07A Task 1.
- The fixture is intentionally a discovery and audit skeleton. `08-14` and `10R/10F/10C` slots remain
  `design-pending`; a real native provider registration and project/runtime execution evidence remain
  outside this milestone.
