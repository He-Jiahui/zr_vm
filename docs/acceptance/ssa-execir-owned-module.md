---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir_owned_module.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c
  - tests/core/test_ssa_exec_ir_owned_module.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/cmake/ssa-execir-owned-module.cmake
implementation_files:
  - zr_vm_core/include/zr_vm_core/exec_ir_owned_module.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c
plan_sources:
  - .codex/plans/20261009-ssa-execir-owned-module-design-v1.md
  - .codex/plans/20261009-ssa-execir-owned-module-tests-v1.md
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
tests:
  - tests/core/test_ssa_exec_ir_owned_module.c
  - tests/parser/test_ssa_core_model.c
doc_type: acceptance-record
status: finite-green-verified-commit-pending
---

# Core ExecIR owner finite acceptance

## Historical RED source

| Input | SHA256 |
| --- | --- |
| Public header,45 lines | `D62DD03F9BB5A9E2AB7C2C71265B2E6BD571097D01006955829E8D4144DED181` |
| RED implementation,37 lines | `16B2FDACD9536AE86AD7A8DB1838A5DBB031054AA92D742748DBFCDAB62FEC87` |
| Frozen test,248 lines | `02C8DA09434462EEF45CDDDF1E06ED788C6DB7E6D51D1C8877BC6F493F8BFA7F` |
| Fixture,245 lines | `D51026CEBD994BEDE78F7317C106F41F8A65643D18DB882BC84AF311879EA835` |
| Strict allocator,107 lines | `46281B00B3B0BB7E72C77A5189E5A4E7A41F6B4CA1DC50FACBBF0938C7D3FD1F` |

Read-only source review confirms complete diagnostic reset, NULL-argument
INVALID_ARGUMENT and non-NULL UNSUPPORTED refusal with all payload fields zero.
The constructor allocates nothing and does not publish an owner. View and Free
are real borrowed-view and slot-clearing APIs, but RED creates no live owner.
At RED, the public snapshot/OOM contract described future GREEN behavior rather
than construction success. The implemented contract is recorded below.

## Historical RED evidence

RED r5 configure18708/build39720/support25844/owner6612 exited0/0/0/8.
The existing core-model support passed, including its original28 Function and60
Module allocation fault loops. New default14 cases observed1PASS/13featureFAIL/
0ignored, with build0warnings and no strict ledger or UBSan diagnostic. Case1
proves the real full/present-empty/absent-header lower clone prerequisite.
The13 feature failures reach the linked constructor's UNSUPPORTED refusal.
Cases12/13 do not establish the new owner allocation denominator or fault coverage;
metadata/owner success, rollback/retry and replacement were uncredited until
the GREEN run recorded below.

Registration currently selects15 private Core TUs and4 owner test TUs, plus the
existing16-TU core-model test:35 compile units. Root's separate read-only review
checks current pins, assertions/UBSan and seven actual provider symbols against
the raw output and maps. The original driver receipt stays validatedfalse:
its parser expected `Expected TRUE` instead of Unity's custom assertion message.
That r4 receipt and separate raw adoption review remain historical. The current
r5 driver accepts the exact custom message and is validatedtrue; Root separately
verified current raw output, providers and pins. The tests were not weakened.

Finite MSVC RED checks seven units: public-header probe, owner, existing clone,
StateMap storage, test, fixture and allocator. All exited0 with0warnings. This
proves only that named compile scope, not full MSVC or Linux validation.

Reports are under E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b:

| Receipt | SHA256 |
| --- | --- |
| execir-owned-module-red-r4-receipt.json (validatedfalse retained) | `95A1AD25999498DFEB662551B5266EF7096A3729AF99B15318BAA4AB00C31BE2` |
| execir-owned-module-red-r4-owner-ctest.log | `ADB50F0565D58C28D3EE3A2667286B1B10B14E66F1B5C4B64AFE210D91061BFC` |
| execir-owned-module-red-r4-root-review.json | `EF25A76F060E31C700565B52F6E12811413A204063B857844A77A227DF707F9E` |
| execir-owned-module-msvc-red-r1-receipt.json | `B47AD69870730ECA6A6A682CA77473C0A9949AF87DCA4109843AF557FFF812A6` |
| execir-owned-module-red-r5-receipt.json (current validatedtrue) | `6F771FB5C60CF2C5F5BC7E67255CB7C5307BF7B2162F0FB6CA96B95B0BC60DDE` |
| execir-owned-module-red-r5-owner-ctest.log | `4DBD554861C6EDFF8A3DE89555FC31B979C20FD9A0CE900224BC16306BAF5519` |
| execir-owned-module-red-r5-root-review.json | `1ED42B408AEAD55567981AFD949E927AAA51AC14CBB594F113E92C6822F3884A` |

Earlier failures remain historical. r1 configure39028/build33640 exited0 but
stopped before tests on five existing core-model warnings. Root initialized
seven range variables in three lines without changing assertions. r2
configure39504 exited0 but stopped on the redundant cached CMAKE_TOOLCHAIN_FILE
warning. r3 stopped before native execution on an assumed FILEPATH cache type;
the actual type was UNINITIALIZED. r4's accepted build has no warnings, with no
warning-flag suppression. These earlier stops are not functional feature RED.

The seven-range initialization support fix was separately committed as
`32a789c5`, one path, with He-Jiahui author and committer; its source pins did not
change afterwards. The first owner RED transaction stopped after exact-add14
at cached validation because the new registration had one extra EOF blank.
Root removed only that blank and ran fresh r5 against the new registration pin.
The staged14-path scope was preserved for Root's resumed transaction; no test
or production behavior changed. The finite MSVC RED seven-unit receipt above
remains current.

RED was committed as `ef37e51451d0685dd6501f701ec333c20a2b6c31`, exact14 paths,
with He-Jiahui author and committer and13 Git steps exiting0. Its receipt SHA is
`3B292C69E10B0DE657606053993F0A010E700CC0A27ADF452B0149F809608024`.

## Current verified GREEN

The public header remains45 lines with the hash above. The implementation is
now61 lines, SHA `02A1FF2B00A22D58A14031D57CBE22C5C00D688B50CB3B6D4F812EE861D888CF`.
All six frozen test/support files remain unchanged. Independent source review
confirms fresh candidate rollback, complete lower diagnostic preservation,
publication before old-owner release, old-view source lifetime and full resets.

Native configure34280/build34464/support40624/owner30884 all exited0.
Core-model support passes, including its original28 Function/60 Module loops.
New default14 cases produce14PASS/0FAIL/0ignored, with build0warnings and no
strict ledger or UBSan diagnostic. Allocation/free records balance exactly.

The ordinary-source and old-view-source sweeps each measure61 owner requests,
complete61 injected owner failures and identify60 direct CloneModule requests.
There are122 injected owner failures within two of the14 Unity cases. For lower
sites the same-source direct clone supplies the complete nine-field diagnostic
witness; envelope OOM uses the actual1/0 contract. Every failure checks candidate
release before fallback, source/old-record preservation and disarmed clean retry.
These counts apply to this fixed genuine constructed profile, not arbitrary
modules or the full graph/producer denominator.

Root verifies current35 compile units, seven actual provider symbols and
source/header/settings/artifact pins. Assertions and UBSan are admitted in the
finite gate. Fresh seven-unit MSVC GREEN covers header, owner, clone, StateMap,
test, fixture and allocator, all exit0/0warnings; it is compile-only evidence.

| Current GREEN receipt under the report root above | SHA256 |
| --- | --- |
| execir-owned-module-green-r1-receipt.json | `A2E926CFBE324EAFBAA49F1144FDD029D2CCF85B0E07A39A59EB9AFFFCB3C2B5` |
| execir-owned-module-green-r1-owner-ctest.log | `B7F3FB6F41EBE85BB5F742994C59E330AD995C5354F09CCD6DCA09626FB32EE3` |
| execir-owned-module-green-r1-root-review.json | `B8CF9087A572A64D9C19AEAE230176EF497A95DE2B830729C18485534DBB8DCE` |
| execir-owned-module-msvc-green-r1-receipt.json | `76C5175716D888B7672A52E722CCCFF13075EA851BC6F7F4B60AA7B61F7C7C6F` |

Finite GREEN is verified; its separate commit remains pending. Historical RED
and failed receipts above are retained.

## Scope remains open

This is storage ownership support for leaf01.01. It adds no Verify_ALL admission,
new hash/identity/remapping, single arena, Function owner attachment, source
retention or ordinary canonical/default output. Arbitrary stale pointer safety,
concurrency, nonlocal exceptions, complete graph/producer denominators,
Linux/full MSVC, native/backend parity and all47 SSA leaf gates remain OPEN.
No source, Oracle, generated native, network/FFI/provider/security execution is
authorized or credited by this gate.

- [Module contract](../core-runtime/ssa-execir-owned-module.md)
- [Test inventory and allocator scope](../testing-and-validation/ssa-execir-owned-module.md)
