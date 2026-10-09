---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_scalar_constant.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_scalar_constant.c
  - tests/parser/test_ssa_source_scalar_constant.c
  - tests/parser/support/ssa_source_scalar_constant_noalloc_shim.h
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_scalar_constant.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_scalar_constant.c
plan_sources:
  - docs/plans/ssa
  - .codex/plans/20261008-ssa-source-scalar-constant-source-tests-design.md
tests:
  - tests/parser/test_ssa_source_scalar_constant.c
  - tests/acceptance/ssa-source-scalar-constant.md
doc_type: acceptance-record
status: source-scalar-constant-finite-green
---

# Source scalar constant acceptance

## Prerequisite and scope

Function-owned immutable metadata is committed as
`60df9cb15cde6ac44a2ed4728416babce5c71cb8`. Its actual twelve default lifecycle
cases passed with zero failures/ignores. Its commit receipt SHA256 is
`68A7FBB525FAECD17EB0F86080272456208F91CF170828D3D51BB4D01EBAA97E`.

This gate establishes a finite constant bridge before actual returned-source
retention. The current24-case source and shim are frozen at SHA256
`936792AE7CAD43BEA9F515FEB6484C78C2FB7D73DCC2868A63D2616F8621F29D`
and `8EB4D173C1E5F575F2A0BDD0DC2AB12D629262449505644F919E128B3CE6B281`.
The implementation C296 is
`445D1658EBD92D3C767BD4D09F10E7D3F5AD41D97AA046F3BCAACDB4148FADFE`;
public H98 is `75E13CF540A27718B75FFFAAC05DB219656D5808640F1D06FB952EEC9A782989`.

The reviewed case22 correction is separately committed as
`9d8283a6542734b2137c688721eed9c736102abb`, exact test and test-record paths,
actual He-Jiahui author/committer. Its real9/8 traces each show one SemIR value,
CONSTANT result1, PLACE_BASE result0 and RETURN operand1. Case22 tests invalid
operand0; bounded different-valid-value coverage remains OPEN.

## Actual validation gate

Root runs the actual checkout through the finite direct CMake project, Windows
x64 clang-cl19 Debug with assertions and UBSan halt-on-error. No source copy or
snapshot is created. Outputs reuse
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`.
The actual 24-case selector is `^ssa_source_scalar_constant$`; no case is ignored.

The gate must rebuild selected Parser/Core/Library sources against current
headers. Source hashes, production header hashes, actual target membership,
compile commands, settings, binary and link map are checked before and after
the actual run. The map must select the forced-shim private bridge OBJECT once;
the ordinary Parser bridge is compiled without that shim. Named network/FFI
providers and legacy runtime/scalar-text fallback archives are excluded.

## Actual GREEN r1

Configure33912, build14692 and CTest34752 returned0. All24 default cases PASS,
0 failures/ignores. The extraction calls return true with diagnostic0. The
actual forced-shim OBJECT supplies the bridge once; no Parser archive fallback,
Oracle, network or FFI symbol is selected by the reviewed map. The selected
incremental build reports0 warnings and no VM ledger or UBSan error occurs.
Actual source/header/generated settings/source graph/binary/map pins remain
unchanged across the run.

| Evidence | SHA256 |
| --- | --- |
| source-scalar-constant-green-r1-receipt.json | 9BB8B83996E2FDB876A617A314D2F8860C2889DECF58633A3721963543D7DA94 |
| source-scalar-constant-green-r1-ctest.log | 57A98F8B9A87D8C0C024DFBB4A2D0FA93575D6C543339D2DD919B87E3A0C2043 |
| source-scalar-constant-green-r1-root-provenance-review.json | 03DB5197C56AF6C97027C11558A9FB379D10B9BCE2C384E608A411FE50D70A32 |
| source-scalar-constant-msvc-green-r1-receipt.json | AC8928E545AC0E7D9CC7FC46C8D135659FB2D0EEF0EF105B8558B8C15B859551 |

Finite MSVC module9664/test19976/literal-fixture25424 all exit0,0 warnings with
/W4 and assertions. This covers those three translation units only. Sol
independently reviewed the production lookup closure, ordered geometry/union
checks and candidate publication, plus the corrected test mutation restoration.
The actual instruction-invalid sentinel is0; it is excluded before id-1 lookup.

## Preserved functional RED r3

The earlier r3 configure/build returned0, CTest8. All24 default cases ran:
both source9/source8 prerequisites and NULL argument preservation PASS;21
feature cases FAIL/0 ignored. Actual extraction reports false/UNSUPPORTED28
after source/output preservation and zero-allocation checks. The raw map has
one bridge symbol supplied by the intended private OBJECT. The current
incremental build has0 warnings, no VM ledger or UBSan error. This establishes
the functional RED for the then-current refusal stub.

Reports under `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`:

| Evidence | SHA256 |
| --- | --- |
| source-scalar-constant-red-r3-receipt.json | 44B7AF97268EE6751C57BF280E5744036101DEA1DDFE7A3017F6FAAEACDF0ADA |
| source-scalar-constant-red-r3-ctest.log | 0AF2378580BD243D2A35524086E3CFE1839DC09A9EADDEF4FEF289769F354FA0 |
| source-scalar-constant-msvc-red-r2-receipt.json | 619BA3A75BA47E77D006F53332843211436666AD9CF23F11A5279BB80B9B90D5 |

The finite MSVC check compiles the actual module/test/literal fixture with /W4
and assertions: three child exits0,0 warnings. Independent Sol helper closure
review ruled out allocating canonical interning/general validator/producers;
Luna reviewed actual24-case cleanup, mutation restoration and full diagnostic
fields. That535-line test included guarded GC-state restoration; current566
adds bounded actual SemIR tracing and the truthful invalid-operand correction.

Historical r1 built successfully but had24 failures because the builder
transferred128 allocated bytes while Function cleanup reported16. Both
prerequisites failed, so r1 was rejected as functional RED. Receipt SHA256 is
`B84648F8BF33ED77809121BEB1338C35DCBF590C09AB5B5F6DDEA85091E86ECB`.
The [lower repair](ssa-metadata-string-heap-owner.md) is independently committed
as `5bd92350596d95c9dd99a217b8e98efd335a294a` after actual6-case GREEN. Strict
allocation sizes/flags were preserved. R1's57 build warnings are existing
unrelated Parser/Library diagnostics; it is not a whole-project warning gate.
Current r3 proves the exact reviewed source with0 incremental warnings.

## GREEN evidence and limits

GREEN requires the same default24 inventory, zero failures and zero ignores,
all nine diagnostic fields checked, strict sizes/flags in VM allocation/free,
zero direct bridge allocation calls, unchanged source/output on refusal,
actual typed pool-derived constants and fresh-Build/binder/Core verification.
Whole-case cleanup balances, and actual helper closure review supports the
nonallocation claim within this profile. The current gate meets this inventory;
isolated span overflow, schema and nested geometry variants are source-reviewed,
not claimed as executed tests. Any finite MSVC compilation covers only its listed actual
translation units; it does not close the full platform matrix.

## Open boundaries

The known source9/source8 tests perform metadata construction and destruction.
Oracle, SourceExecute, generated-native compilation/execution and network,
FFI, provider, security, capture, monitor or install actions are outside this
gate. Unused Oracle support in the existing fixture may be link-selected and
must be reported from the actual map; it is never invoked here.

Default canonical returned-source output, all source families/backend parity,
Linux, full MSVC, performance and all47 SSA plan leaves remain OPEN. The exact
16-case descriptor OOM work is an independent prerequisite with its own commit.
