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
status: source-scalar-constant-functional-red-established
---

# Source scalar constant acceptance

## Prerequisite and scope

Function-owned immutable metadata is committed as
`60df9cb15cde6ac44a2ed4728416babce5c71cb8`. Its actual twelve default lifecycle
cases passed with zero failures/ignores. Its commit receipt SHA256 is
`68A7FBB525FAECD17EB0F86080272456208F91CF170828D3D51BB4D01EBAA97E`.

This gate establishes a finite constant bridge before actual returned-source
retention. The checked-in RED stage intentionally refuses extraction. The
current 24-case source and shim are frozen at SHA256
`86F7E09406712066958B531AACFB6C289B043316789DAAEE3E98B91D308031AA`
and `8EB4D173C1E5F575F2A0BDD0DC2AB12D629262449505644F919E128B3CE6B281`.

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

The actual current r3 configure/build returned0, CTest8. All24 default cases ran:
both source9/source8 prerequisites and NULL argument preservation PASS;21
feature cases FAIL/0 ignored. Actual extraction reports false/UNSUPPORTED28
after source/output preservation and zero-allocation checks. The raw map has
one bridge symbol supplied by the intended private OBJECT. The current
incremental build has0 warnings, no VM ledger or UBSan error. This establishes
the functional RED; extraction remains unimplemented.

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
fields. The current535-line test includes guarded GC-state restoration.

Historical r1 built successfully but had24 failures because the builder
transferred128 allocated bytes while Function cleanup reported16. Both
prerequisites failed, so r1 was rejected as functional RED. Receipt SHA256 is
`B84648F8BF33ED77809121BEB1338C35DCBF590C09AB5B5F6DDEA85091E86ECB`.
The [lower repair](ssa-metadata-string-heap-owner.md) is independently committed
as `5bd92350596d95c9dd99a217b8e98efd335a294a` after actual6-case GREEN. Strict
allocation sizes/flags were preserved. R1's57 build warnings are existing
unrelated Parser/Library diagnostics; it is not a whole-project warning gate.
Current r3 proves the exact reviewed source with0 incremental warnings.

## Required GREEN evidence

GREEN requires the same default24 inventory, zero failures and zero ignores,
all nine diagnostic fields checked, strict sizes/flags in VM allocation/free,
zero direct bridge allocation calls, unchanged source/output on refusal,
actual typed pool-derived constants and fresh-Build/binder/Core verification.
Whole-case cleanup must balance; helper closure review must support the
nonallocation claim. Any finite MSVC compilation covers only its listed actual
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
