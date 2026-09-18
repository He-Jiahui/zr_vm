# SSA 01.02: module builder transaction and function identity

## RED and positive fixture

The standalone `ssa_builder_cfg` target now exercises the public module
builder with valid one-block canonical facts and caller-supplied token 77,
signature hash 99. It checks the published one-based function ID, the
caller's token/signature in both function and execution contract, and the
module's generation-1 contract. The published function also passes core
structural verification.

For a second fixture, a source block targets a nonexistent block. Before the
fix, MSVC failed at `FAIL: failed initial module build published a partial
function`: `BuildModule` appended a slot before lowering the invalid CFG.
The module must remain empty on initial failure, retain its existing entry
on a failed later build, and assign the next successful function ID without
a gap. A missing function token must return `INVALID_ARGUMENT` without a
published slot. A malformed-CFG diagnostic retains `INVALID_BLOCK` and the
caller's function token 77; before that final correction, it instead reported
the semantic fixture's token 42 and failed the new identity assertion.

## Observed validation

- MSVC (VSDevCmd, D:-backed focused build): the expanded core/effects/
  dominator/builder/value/oracle/pass-manager CTest selection passed 8/8.
- Attempted a fresh WSL GCC focused rebuild against this module-builder
  change; WSL failed to create its VM (`CreateVm`, `0x800705b4`) before a
  compiler or CTest could run. GCC and Clang validation for this slice are
  therefore unverified, not passing results. The preceding instruction-
  range slice has its own completed GCC CTest evidence.

## Acceptance boundary

This verifies transactionality and identity for an individual module-builder
call. Allocation-failure injection, cancellation, pruned phi construction,
exception-edge semantics, and end-to-end backend parity remain separate
01.02 gates. The user-modified `test_ssa_construction.c` is not part of this
fixture.
