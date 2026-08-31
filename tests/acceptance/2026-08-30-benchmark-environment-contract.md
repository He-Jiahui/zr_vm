---
title: Benchmark environment contract acceptance
date: 2026-08-30
status: complete
---

# Benchmark Environment Contract Acceptance

## Scope

This acceptance covers the independent benchmark environment-contract slice:

- schema-v2 lifecycle, validation, fingerprinting, and comparison;
- deterministic effective build and runtime identities;
- recursive Git source identity and full SHA-256 cache identities;
- Linux CPU-topology selection, affinity capture, and interrupted-run handling.

The slice remains intentionally separate from CMake benchmark orchestration,
aggregation, report publishing, and cache mutation.

## Baseline

The first implementation passed 21 focused tests but had these quality gaps:

- capture files were published as `COMPLETE` before a benchmark ran;
- `source.after` and a finalization marker were not required;
- build/runtime schema completeness was implicit;
- effective IPO/LTO, sanitizer, ABI, toolchain-file, and target compile evidence was
  absent from fingerprints and cache identities;
- the 1004-line module mixed schema, Git, capture, topology, and CLI responsibilities;
- tracked diff and untracked content could be buffered as complete byte strings;
- Git identities did not distinguish different edits under the same submodule commit;
- Git path prefixes, diff ordering/submodule rendering, and cache-directory hash
  suffixes were not fully deterministic;
- target compile evidence marked `unavailable` was accepted for complete reports;
- CLI cache identities without effective target evidence were not explicitly marked
  non-comparable;
- source identity contract version 2 was exported but absent from source/cache data.

## TDD Evidence

The reopened Windows RED run (the pre-fix baseline) executed 35 tests and reported
20 failures, 3 errors, and 4 expected platform skips. The failures directly exposed missing schema-v2
constants, lifecycle finalization, complete build capture, fingerprint mismatches,
recursive submodule hashing, streamed untracked content, Git timeouts, deterministic
prefixes, and full cache hashes.

Two additional focused red/green cycles caught:

- UBSan cache keys being omitted by the sanitizer-name pattern: 1 failed, then 2/2
  build-capture tests passed after the fix;
- clean cache directories not embedding their full source SHA-256: 1 failed, then
  1/1 passed after the key format changed;
- missing target compile evidence and Git `diff.orderFile`/`diff.submodule`
  configuration: WSL 2/2 failed before the fixes, then the focused WSL group passed
  4/4 including missing-submodule and cache regressions;
- Windows Git order-file portability: the Windows-runnable regression passed after
  replacing the rejected empty config value with a temporary empty order file.

## Test Inventory

The 41-case suite verifies:

- strict duplicate-key and non-finite JSON rejection plus canonical ASCII JSON;
- explicit schema, build, source, fingerprint, and cache contract versions;
- `IN_PROGRESS` files have no fingerprint, completion timestamp, end load, or source
  after-identity and fail comparison closed;
- finalization populates `source.after`, change status, completion fields, and the
  finalized marker before publishing a fingerprinted `COMPLETE` document;
- every required build identity, performance option group, and runtime name fails
  closed when absent;
- unavailable/missing target compile evidence prevents a comparable complete capture;
- CLI cache-key output without an effective build contract is explicitly marked
  `comparable=false`;
- effective shared-library, IPO/LTO, sanitizer, ABI/runtime-library, compile
  definition/feature/option, toolchain-file, and normalized `compile_commands`
  evidence affect fingerprints and comparisons;
- compiler executable path, toolchain-file identity, target evidence, ordered flags,
  source commit, and dirty digest affect full SHA-256 cache identities;
- deterministic Git diff prefixes/order/submodule rendering ignore user color,
  algorithm, `diff.orderFile`, and `diff.submodule` settings on Windows and WSL;
- tracked modifications/deletions/modes/symlinks, nonignored untracked files, and
  initialized submodules contribute to dirty identity;
- missing or uninitialized gitlinks fail closed;
- two edits beneath the same submodule commit produce different dirty digests;
- source identity contract versions are embedded in source snapshots and cache source
  hashes, so a contract revision changes the cache key;
- a 16 MiB sparse untracked file is streamed without `Path.read_bytes`;
- Git subprocesses time out through a real slow child command;
- all test-owned subprocess calls have explicit outer timeouts;
- Linux wrapper success, source mutation, non-isolated fallback, nonzero command
  status, child affinity, and forced interruption paths.

## Tooling Evidence

Windows tooling:

- Python 3.14.4
- Git 2.31.1.windows.1

WSL tooling:

- Python 3.10.12
- Git 2.34.1
- Bash 5.1.16
- `taskset` and `lscpu` from util-linux 2.37.2

Commands:

```text
python -B tests/benchmarks/test_benchmark_environment_contract.py -v
wsl.exe -e bash -lc 'cd /mnt/e/Git/zr_vm && python3 -B tests/benchmarks/test_benchmark_environment_contract.py -v'
python -B -m py_compile scripts/benchmark/benchmark_environment_contract.py scripts/benchmark/benchmark_environment_schema.py scripts/benchmark/benchmark_source_identity.py scripts/benchmark/benchmark_environment_capture.py tests/benchmarks/test_benchmark_environment_contract.py
wsl.exe -e bash -lc 'cd /mnt/e/Git/zr_vm && bash -n scripts/benchmark/capture_benchmark_environment.sh'
```

## Results

- Windows final full run: 41 tests succeeded, with 8 expected skips for Linux capture,
  Linux submodule integration, and Windows symlink privileges.
- WSL final full run: 41/41 tests passed.
- Final Python compilation and Bash syntax validation passed.
- ShellCheck is unavailable in the current WSL environment.
- Production responsibilities remain split across the public facade, schema,
  source/cache, and capture modules, each below the repository's large-file threshold.

## Acceptance Decision

Accepted for the independent environment-contract slice. A capture interrupted before
finalization remains atomically published as `IN_PROGRESS` and cannot be compared.
Benchmark-suite attachment, baseline comparison, keyed-cache wiring, and report
publication are covered by the parent integration tests. Isolation proves process
affinity rather than exclusive CPU ownership, and native Linux validation remains
advisable in addition to the completed WSL coverage.
