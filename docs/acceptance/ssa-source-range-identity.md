---
related_code:
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.c
  - tests/parser/test_ssa_source_range_identity.c
  - tests/parser/test_ssa_source_script_entry_identity.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
plan_sources:
  - .codex/plans/20261004-source-aot-handoff-design-2005.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/parser-and-semantics/ssa-source-script-entry-identity.md
tests:
  - tests/parser/test_ssa_source_range_identity.c
  - tests/parser/test_ssa_source_script_entry_identity.c
  - docs/parser-and-semantics/ssa-source-range-identity.md
doc_type: acceptance
status: source-range-direct-green
---

# Source Range Identity Acceptance

## Finite decision

Status is **source-range-direct-green** for the actual parser SCRIPT/RETURN
spans and shared CRLF token end. The [range contract](../parser-and-semantics/ssa-source-range-identity.md)
defines source-byte offsets, exclusive ends, fixed CRLF coordinates, and the
direct missing-semicolon recovery boundary. The full 47-item SSA plan remains
**OPEN**.

## Successor gate

This document freezes the historical 13-case baseline and 85-case consumers
regression. The [multiline token successor](ssa-multiline-token-coordinates.md)
retains those 13 assertions and runs 17 range cases by default. Its independent
RED/GREEN record owns the new fixture, production and executable pins. The
historical hashes below do not claim to match the current files.

## Fixture inventory

The historical frozen `test_ssa_source_range_identity.c` reported 13/13 cases passing:

| Case | Observed contract |
| --- | --- |
| `test_script_root_starts_at_first_token` | SCRIPT begins at actual first token and reaches EOS |
| `test_return_literal_includes_semicolon` | RETURN spans `return 9;`; literal retains its own range |
| `test_literal_at_eof_without_newline` | Complete range at immediate EOF |
| `test_leading_and_trailing_trivia` | SCRIPT excludes leading trivia, includes trailing trivia through EOS; RETURN excludes trailing trivia |
| `test_return_expression_on_later_line` | RETURN contains its later-line expression |
| `test_crlf_return_and_root_ranges` | Independent byte/line/column assertions for keyword, literal, semicolon, RETURN, SCRIPT, EOS |
| `test_semicolon_token_before_crlf_excludes_both_newline_bytes` | Shared token end excludes raw CR and LF despite the lexer newline cursor |
| `test_missing_return_semicolon_preserves_following_declaration` | One `var` diagnostic, accepted-expression end, unchanged current `var`, then direct declaration parse to EOS |
| `test_return_excludes_following_statement` | RETURN ends before gap trivia and later declaration |
| `test_empty_script_is_eos_point` | Empty source yields EOS point SCRIPT |
| `test_trivia_only_script_is_eos_point` | Trivia-only source yields EOS point SCRIPT |
| `test_operandless_return_includes_semicolon` | `return;` is a complete statement span |
| `test_reference_return_preserves_ref_token_range` | RETURN, `ref`, and expression retain distinct ranges |

Missing-semicolon recovery calls the statement parsers directly. It proves
their retained token and partial range, without certifying every malformed
SCRIPT recovery route. The range fixture inspects parser AST and token state;
it does not execute generated code.

## Frozen RED

The immutable RED test commit is
`d2551f5953786cc5b7ef34ba852b376792f49e3d`. Its receipt records the preceding
base HEAD `4b12dcd86cfe953f184126a0d73162428b499ad4`; the receipt was created
before the RED test commit and is preserved as written.

- Receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/source-range-red-receipt.json`
- Receipt SHA-256: `53460499DE74033DEAA8764B2F334D64297B25BCC56CF6BB9E507635A156BFEF`
- RED fixture SHA-256: `4F6AA765778FD9BCA5E44A4481AC7871A4601984EC81052908B2426B79374A0C`
- Build log: `source-range-red-build.log`, SHA-256 `BF54D7F77B4A63F9892ABE5F6BBFD63402086B0527243FD1F20CA5D11976F889`
- CTest log: `source-range-red.log`, SHA-256 `277C03E69B6B968E6F15526A39B671CA255589362A252A954383BA082F10A28A`

Build exit was 0; focused CTest exit was 8. Eleven Unity cases produced nine
expected old-parser failures and two passes, with no ignored cases. RED is
diagnostic evidence of the producer gap, not passing acceptance.

## Historical frozen GREEN environment and evidence

The route is `01a0fe2b-2f6b-7063-9a67-ac0e08c0d82d`. The existing direct build
root is `E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`.
Windows clang-cl 19 x64 Debug UBSan compiled the then-current checkout directly;
`source_copy_created` is false. The GREEN receipt's base HEAD is the frozen
RED commit above, and the production hashes identify those historical tested files.

Target `zr_vm_ssa_source_range_identity_test` built with exit 0. Focused CTest
`ssa_source_range_identity` exited 0, and its real executable reported
`13 Tests 0 Failures 0 Ignored`. The log records
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; no UBSan diagnostic was
observed. Both ordinary source-test CMake and the direct driver register the
fixture; this result belongs to the direct driver.

- Receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/source-range-green-receipt.json`
- Receipt SHA-256: `96F1BEB193833930D235E23EEC3F8BEDAB5FD2AD6594E517550847502F2ABEB8`
- Historical frozen fixture SHA-256: `A7483E3E37BC224B13DDB6942F9FBAD0CF8F11FE02E0265C4FA499C699C66623`
- Build log: `source-range-green-build.log`, SHA-256 `AEC91D18E9A481C3AC2B00B00399F3B7869F9B43114C64967970CA28C2F0EC36`
- CTest log: `source-range-green.log`, SHA-256 `32EE5754002E0A492697E58A7FD7CC2125879C2AE4807A2E61AFDAA3EC71BF9A`

| Tested production file | SHA-256 |
| --- | --- |
| `zr_vm_parser/src/zr_vm_parser/parser.c` | `7004D7DCB7D68DAD2C94D6FCFE04601C40E24575B8DCA6F3977AA0552A9B9806` |
| `zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c` | `ECCE14E30CC6784D2FEFAFB8D73B9B2204D8987D5BA2FB2D17AD7F5CC91CC6D9` |
| `zr_vm_parser/src/zr_vm_parser/parser/parser_state.c` | `43C9BE6D6FC4D61D1DF74F6E1ADDD53B17163874D151F773EFE219C1FA7F2ECB` |

The RED/GREEN receipt hashes, then-current fixture/production hashes, and GREEN
CTest log hash were read and verified while preparing this record. The
13-case GREEN strengthens the 11-case RED inventory with fixed CRLF coordinate
checks and two additional cases; it does not rewrite the frozen RED fixture.

## Consumers regression

The separate direct consumers regression also built with exit 0 and finished
CTest with exit 0, passing 8/8 entries in the same environment. Its seven
Unity suites report 85 cases, 0 failures and 0 ignored; dominator CFG is a
standalone test and is not counted as a Unity case.

| CTest entry | Result |
| --- | --- |
| `ssa_source_execbc_vm_loops` | 6 Unity cases PASS |
| `ssa_source_straight_line_cfg` | 35 Unity cases PASS |
| `ssa_source_callable_return` | 8 Unity cases PASS |
| `ssa_source_script_entry_tokens` | 8 Unity cases PASS |
| `ssa_source_callable_identity` | 11 Unity cases PASS |
| `ssa_source_script_entry_identity` | 13 Unity cases PASS |
| `ssa_typed_binding_contract` | 4 Unity cases PASS |
| `ssa_dominator_cfg` | Standalone CFG PASS |

- Receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/source-range-regression-receipt.json`
- Receipt SHA-256: `9050C7B07FD924A73BE37A39DDEA5B450F40B89A4863E8B74659FE0948061AC1`
- Build log: `source-range-regression-build.log`, SHA-256 `DD04E7E72AEAEE2B42C7210126D26007EBD216A373C63C396CE0C99A7142DB27`
- CTest log: `source-range-regression.log`, SHA-256 `3E2711C90B4DAE20BBCE01B24CB5EE3F47C03BD2E90B0724188F1AFAD2EE2CB0`

The regression receipt and log hashes were verified, and the log's actual
suite totals and 8/8 summary were inspected. No UBSan diagnostic was observed.
The source-range fixture's 13/13 result is separate from these 8/8 CTest entries.

## Limits

This finite result does not accept full top-level CMake, WSL/Linux, native
32-bit, ASan, full parser coverage, retained canonical graph ownership,
frame/artifact publication, serialization, native or AOT consumers, token
execution, exhaustive OOM, or module-summary/cache rollback. Successful
compiler consumers regression does not establish the full SSA pipeline.
The 47-item SSA plan remains **OPEN**.

No network, FFI, provider, external-service, or security-boundary execution
contributes evidence to this gate. Historical SCRIPT identity receipts remain
unchanged; their earlier point-range boundary is superseded only by this
separate range contract and its historical evidence.
