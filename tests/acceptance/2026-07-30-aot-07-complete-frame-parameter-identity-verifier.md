# 2026-07-30 AOT 07 Complete-Frame Parameter Identity Verifier

## Scope

This sub-milestone makes parameter markers in complete materialized frame tables a verified AOT input:

- a complete table has one row for every stack slot;
- each row's `isParameter` value must match the producer's canonical parameter-slot rule;
- the final marker count must equal `parameterCount`;
- zero-row frames remain legal for fully register-carried scalar parameters;
- sparse hybrid tables may omit register-only parameters while materializing an inline local.

It reuses the existing frame-layout manifest. Parameter types/directions, receiver and return ABI, spills, address-taken
storage, and complete CallableContract frame derivation remain outside this slice.

## RED Evidence

The frozen WSL GCC baseline at commit `1e79dfc` passed code stripping 32/32. Adding the zero-frame and sparse-hybrid
positives plus an unreachable complete-table parameter undercount produced 32/33 against unchanged production: the
writer accepted the malformed owner that stripping would remove.

The first exact-count implementation passed 33/33. Independent review identified an equal-count identity gap: a
complete two-slot table with one parameter could mark local slot 1 instead of canonical parameter slot 0 and still
pass. The new swapped-marker case reproduced that first implementation as 32/33. Comparing every complete-table row
with the producer's canonical slot rule restored 33/33.

## Coverage Inventory

- A root function with `parameterCount=1` and an empty zero-byte frame remains accepted.
- A retained sparse hybrid with stack slots 0/1, register-only parameter slot 0, and one materialized local row for
  slot 1 remains accepted.
- The existing complete borrowed receiver alias remains accepted with one exact parameter marker.
- An unreachable one-slot complete table with `parameterCount=1` and no marker fails before filtering.
- An unreachable two-slot complete table with one marker on local slot 1 instead of parameter slot 0 also fails.
- Existing binary-marker, over-count, unique/in-range slot, alias, TypeLayout, alignment, and frame-span gates remain
  covered by the same suite.
- Every rejected writer attempt leaves the generated output path absent.

## Validation

Effective source is commit `1e79dfc` plus the exact production/test overlays for this sub-milestone. Validation reused:

- WSL source: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44

Results:

- WSL GCC: code stripping 33/0; adjacent generic reference sharing 9/0.
- WSL Clang: code stripping 33/0.
- Windows MSVC: code stripping 33/0.
- Main, WSL, and Windows copies of both changed code/test files have identical SHA-256 hashes.
- The malformed generated-C output is absent on WSL and Windows.
- MSVC retains only the existing MSB8029 warning caused by the isolated build residing below `%TEMP%`.
- Independent review's equal-count swapped-marker P2 was reproduced and fixed; final re-review returned no findings.

## Acceptance Decision

Accepted as AOT 07 A7.2C's complete-frame parameter identity verifier and an AOT 12 pre-filter graph-input gate.
Typed parameter contracts, receiver/return/destination ABI, sparse register mapping, spill/address-taken storage,
C/LLVM parity, and the broader AOT 07-12 goal remain active.
