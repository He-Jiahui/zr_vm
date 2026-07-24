# Syntax 06A M1 Migration Inventory

## Scope

- Added a standard-library-only, read-only inventory command for legacy ZR syntax in current source,
  project fixtures, embedded ZR test input, and current documentation snippets.
- Added deterministic JSON and text reports, stable five-way classification, explicit exclusions, a
  synthetic lexical/classification golden, and a reviewed repository baseline.
- No parser, compiler, VM, artifact, CLI production behavior, source rewrite, or LSP code action was
  changed.

## Baseline

- The initial protocol RED failed because the inventory module did not exist.
- Repository-closure RED then failed because no repository inventory API or baseline existed.
- The first broad host-string pass incorrectly classified C printf format strings as ZR percent
  directives. The scanner now requires explicit source context plus a ZR opening shape, while retaining
  contiguous ZR source literal sequences.
- Initial Windows and WSL report hashes differed solely because Windows text output translated LF to
  CRLF. The CLI now writes UTF-8 bytes with LF explicitly; a regression test prevents recurrence.
- No unrelated repository test baseline was consumed. The acceptance scope is the standalone inventory
  test script and its checked-in report data.

## Test Inventory

- Synthetic protocol and lexical boundaries: source kinds, exact ranges, comments, strings, modulo,
  all recognized form families, and unrecognized directives.
- Classification matrix: stable target plan and reason for current forms, ownership/reference families,
  property forms, static/dynamic construction, native factories, and non-promoted targets.
- Repository closure: every selected Git-tracked candidate appears once in either `scannedFiles` or
  `exclusions`; generated and binary artifacts are never scanned as source; repeated reports match
  byte-for-byte.
- CLI bytes: `--output` writes UTF-8/LF JSON exactly matching the in-process report and repository
  golden.
- Host matrix: Windows PowerShell Python, Ubuntu 22.04 GCC-host Python, and Ubuntu 22.04 Clang-host
  Python all execute the same five tests and emit the same raw JSON SHA-256.

## Tooling Evidence

- Windows: Python 3.14, `python tests/scripts/test_syntax_migration_inventory.py --repository E:\Git\zr_vm`.
  Result: 5/5 passed in 54.199 seconds.
- WSL GCC: Ubuntu 22.04, GCC 11.4.0, Python 3.10.12,
  `CC=gcc python3 tests/scripts/test_syntax_migration_inventory.py --repository /mnt/e/Git/zr_vm`.
  Result: 5/5 passed in 105.750 seconds.
- WSL Clang: Ubuntu 22.04, Clang 14.0.0, Python 3.10.12,
  `CC=clang python3 tests/scripts/test_syntax_migration_inventory.py --repository /mnt/e/Git/zr_vm`.
  Result: 5/5 passed in 105.751 seconds.
- Each host ran `scripts/syntax_migration_inventory.py --root . --format json --output <host-file>`.
  Raw SHA-256 for Windows, WSL GCC, and WSL Clang:
  `23397b06e154a009e2db276f7f2ae8917b4c088e75066e17851e8e4a106657e2`.

## Results

- Repository baseline: 811 scanned files, 336 explicit exclusions, 770 findings, `unknownCount: 0`,
  and `blocked: 0`.
- Classification counts: 259 `machineApplicable`, 0 `maybeIncorrect`, 321 `requiresReview`, 0
  `blocked`, and 190 `targetNotPromoted`.
- Target plan counts: 02=13, 03=35, 04=23, 05=3, 06A=506, 08=79, 10=4, 11=64, 12=7, 14=36.
- The reviewed baseline remains an inventory only. `targetNotPromoted` findings are not edit candidates.

## Acceptance Decision

Accepted for Syntax 06A M1 after the final exact-path commit. M2 remains responsible for edit planning
and LSP actions; M3 remains responsible for an all-repository dry run; 06B remains gated on Plans 08,
10, 11, 12, 13, and 14.
