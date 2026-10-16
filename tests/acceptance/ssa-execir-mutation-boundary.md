---
doc_type: acceptance
status: verified
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_mutation_internal.h
  - tests/parser/test_ssa_exec_ir_header_boundary.c
  - tests/cmake/ssa-execir-mutation-boundary.cmake
  - tests/cmake/ssa-execir-mutation-boundary-check.cmake
  - tests/unity_config.h
  - tests/cmake/ssa-module-constant-pool-verifier.cmake
  - tests/cmake/ssa-primitive-frame-validation.cmake
plan_sources:
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
  - .codex/plans/20261016-ssa-execir-mutation-boundary.md
---

# ExecIR mutation declaration boundary

This finite declaration-boundary delivery is verified and ready for publication.
Publication and cleanup are established by their actual external receipts. The
complete 01.01 plan and the full 47-leaf SSA goal remain open.

## Interface and caller migration

Thirty construction declarations, including all ten Reserve declarations,
the two mutable accessors and FunctionSetBindingRows, now reside in Core-private
`exec_ir/exec_ir_mutation_internal.h` rather than public `exec_ir.h`.
Three original declaration spans retain their exact bytes, signatures and
`ZR_CORE_API` annotations. Implementations, layouts, allocation order and
diagnostics are unchanged. Public model records, lifecycle/clone functions,
const queries, validation and hashing remain public.

Legitimate consumers include the private header explicitly through target
`PRIVATE` include roots. The returned-owner and integer-fault producer shims
include the real declarations before defining their ModuleAppendLayout
observer redirects. No public reverse include or compatibility alias is added.
Public C structs remain writable; this delivery does not hide DLL symbols.

Shared test support has three narrow fixes: the constant-pool verifier target
receives its Core `src` PRIVATE include root; project-owned `tests/unity_config.h`
includes standard `<stdlib.h>` before Unity's C11 `stdnoreturn` handling; and the
primitive-frame fixture adds the repository root to test EXE PRIVATE includes.
The primitive-frame addition does not expand production OBJECT include roots.
The standard-header ordering preserves the fixture lifecycle comment and uses
no macro suppression, third-party edit or system-header edit.

The static audit covers 78 root C translation units, including the consumers
of 25 support includes. Current graph selection is Source 20, Lower 3 and
normal checkout configuration 55. Caller r5 actually checks every selected
translation unit using its configured command, preserving definitions, include
roots and forced-header order while replacing object output with syntax mode.
All 78 consume the private mutation header.

## Actual evidence as of 2026-10-16

Reports are under `E:/cargo-targets/zr_vm/reports/ssa-execir-mutation-boundary-01a0fe2b/`.
Validation uses the actual checkout with no source copies. The native Windows
tools are clang-cl 19.1.5 and MSVC 19.44; Linux execution is not credited.

| Gate | Observed result | Scope |
| --- | --- | --- |
| Header RED r2 | 64 compiler processes; the 60 isolated public mutation probes still compiled | Both compilers, W0; 195 stable input pins; two discovery processes receive no boundary-check credit |
| Header GREEN r1 | 66 compiler processes; public/private positives pass and all 60 public negatives fail with exactly one selected-symbol undeclared error | Both compilers, W0; 196 stable input pins; syntax only, with two discovery processes excluded from boundary credit |
| Permanent CTest r1 | CTest exit 8, zero of 32 modes accepted | All 32 compiler processes naturally exit 1 on duplicate `/TC` promoted to an error by `/WX`; this failed driver attempt is preserved |
| Lower regression r2 | Four CTests pass; 58 actual compilations plus one unchanged cached object; zero build warnings | Historical 59-TU, eight-role run with 261 actual dependencies and 344 consumed inputs |
| Source regression r2 | Five metadata CTests pass; 658 actual compilations plus 162 cached objects and five links | Historical 820-TU run with 824 dependencies, 1,638 consumed inputs and 58 warnings |
| Permanent CTest r2 | CTest exit 8; two positives pass but all 30 negative modes are rejected by the checker | Each negative has its correct single undeclared primary error; loss of a source-line semicolon in context-list parsing causes `unknown compiler output`, so the attempt remains failed |
| Permanent CTest r3 / r4 | Both pass all 32 modes: two positives and 30 isolated selected-symbol undeclared rejections | r4 independently verifies 129 outputs, 176 actual headers and 206 before/after pins; clang-cl syntax-only W0 |
| Source regression r3 | Five CTests pass; 550 actual compilations plus 270 exact-cache objects and five links | 24 roles, 820 TUs, 824 dependencies, 1,638 consumed inputs and 55 warnings; actual metadata object/link/test credit, not W0 |
| Lower regression r3 | Four CTests pass; 55 actual compilations plus four cached objects and four links | Eight roles, 59 TUs, 261 dependencies and 344 consumed inputs; incremental W0 metadata regression |
| Caller r5 | All 78 configured syntax checks pass; all 79 phases including configuration end naturally with exit 0 | Source 20, Lower 3 and root 55; 158 raw logs, 10,049 candidates, 873 actual input pins and 565 actual headers |

Header GREEN uses the same permanent probe, public-only include roots for
public modes, and one explicit private include root for the private mode. No
header probe or caller syntax check is linked or executed. The manual gate uses
both compilers; the permanent CTest and caller checks use clang-cl.

Source r3's strictly reviewed five-test payload covers AOT 25, literal 26,
integer-expression 22, fault 2 and proof 4 cases. Its 3,891 protocol payload
lines match r2, including 423 accepted fault sites, 424 healthy lifecycles and
1,353 records freed exactly once. Caller r5's 873 actual before/after pin pairs
are exact within that run. Its 150 warnings comprise 55 unused C11-atomics flag
warnings and 95 source warnings, with no error, implicit declaration, missing
input or raw/input drift. Caller syntax success is not W0.

Lower covers owned-module, Core model, module-copy and Source-finalized
metadata fixtures. The observed ownership fault sweeps include 61/61 twice,
88/88 and 89/89, with 16 finalized lifetime cycles. The x64 runs do not execute
the two checks that require 32-bit `size_t`. Lower results do not establish
full-project warning freedom or another platform's behavior.

## Run identities, publication and preserved failures

The final actual evidence is independently accepted:

| Run | Receipt SHA256 | Independent review SHA256 |
| --- | --- | --- |
| Source r3 | `1DF6FCD055D205F3CA7FA78F8C2239D01F9F7BA3BFC65BB46BA3E55ABE0E96C8` | `7B35E010F42D064A6AEBD82006E4AB6B5E3133367ECE6F57432604E7D4F59324` |
| Lower r3 | `AF33B48B3D33E84D3E7B8C4DBAD7F89E3F507707977313BFC5ABF7A81F8DBE69` | `446B0A0E3F0DFC6D6FC11FCD6CE4A8B8BD937F558402EB232121BE33AF4C990D` |
| Permanent r4 | `316D7AC61D22D6D4A85ABC855C19CA1E3AF460C14E7CAEBAE43476893754D6A0` | `879E740FEC03A860FE0A41250101F67143140631D7B6F8296BA10B160CB3CBEB` |
| Caller r5 | `216C16CB8F925E25468519884A69FBCFDAF950925916D13339F223A6230B7FFC` | `740F2B5303EC9836C86445307D47546F47DA05673DB8920D11BCC35464FC10B9` |

These are `source-regression-r3-receipt.json`, `lower-regression-r3-receipt.json`,
`permanent-header-ctest-r4-receipt.json` and `caller-compile-r5/receipt.json`,
with their respective review files under the report root. The manual RED/GREEN
receipts and `header-validation-review.json` preserve the separate two-compiler
boundary evidence. Raw logs and before/after identities describe each recorded
run; Source r3, Lower r3 and permanent r4 are not asserted to have today's raw
bytes after the following foreign EOF-only changes.

`platform-header-comment-bridge.json` (SHA256
`DF93AB6C8AFA7091B1652CDB7FEAEE0791FC6B0105372D63232E823AB7C640A6`)
proves unchanged complete prefixes and token-equivalent EOF suffixes for exactly
two byte pairs, including complete phase-1/2-safe block comments:

| File | Recorded old bytes / SHA256 | Bridged bytes / SHA256 | Added EOF block comments |
| --- | --- | --- | --- |
| `zr_vm_common/include/zr_vm_common/ssa_platform_contract.h` | 20,771 / `A57C23345E982F268924D32D6C1EA38DAB93DAEB84D37ECA2C9E2AF34E84A7AF` | 76,975 / `DE35A2BE6C9216914F700FF78C8719BDD11DEB816FCDB229BD1F37592A1DC317` | 36 |
| `zr_vm_common/src/zr_vm_common/ssa_platform_contract.c` | 52,839 / `6A2AE8FEBFCB9B60D8729DC815FD8DACA156F2643DC32C5A5703C25D3A8DF82D` | 73,806 / `F9D9F87AD1B58FD6256EF6F5A35E5D33B9503FD97CECAB92B32112ACAD74E259` | 17 |

The header was consumed by 805 Source objects, 52 Lower objects and all 32
permanent checks; the C file was a primary source for one Source common object,
with no Lower or permanent-check consumption. This bounded semantic bridge
preserves the original raw receipts and does not upgrade failed Caller r4.
At bridge creation, 3,563 of 3,565 checked paths matched and these two differed.
A subsequent bounded read found 3,562 matches and three differences: the extra
`tests/cmake/cli-fixture-targets.cmake` change is a broad candidate only, absent
from Source/Lower required, generated and actual-dependency sets and permanent
pins. These counts are observations at their recorded times, not a blanket
current-input match. Caller r5 instead has its own fresh exact 873 input pairs.

Validation used the actual configured shared checkout, including independently
recorded foreign migrations. In the three shared CMake files, the publication
candidate includes only this task's private-include and test-support insertions;
pre-existing foreign source-path migrations remain excluded. No foreign baseline
is adopted and no clean-HEAD standalone full-build result is claimed. The actual
finite commit and cleanup are separate publication records. Unknown or foreign
state and necessary original failure logs remain preserved.

Failed attempts retain their original outcomes and evidence:

- The initial manual-header controller rejected a bilingual include-trace parser
  mismatch. Source/Lower r1 retained stale graph/private-path failures; these
  controller failures receive no product passing credit.
- Permanent r1's duplicate `/TC` failure and r2's semicolon-context failure remain
  failed. The fixes preserve `/W4`, `/WX`, exact diagnostic admission and raw logs;
  r3/r4 provide fresh actual passing evidence.
- Caller r1 stopped before a caller compiler on the constant-pool private-root
  preflight; r2 likewise stopped on an unadmitted default ATLMFC include path.
  Their native configuration work is retained rather than described as no native
  activity.
- Caller r3 actually ran all 78 compilers: eight existing Unity-first C11
  `stdnoreturn`/UCRT ordering errors and one missing test repository-root include
  caused compiler failures; foreign opcode-header drift also caused refusals.
  The support fixes above address the header ordering and test include scope.
- Caller r4 had 78 compiler exit-zero results but six strict header raw-drift
  refusals, so it remains failed. Caller r5 reran the full 78 with fresh pins
  and independently accepted actual evidence.

Runtime checks in this delivery are limited to the reviewed Core and
SourceCompile metadata fixtures. VM, SourceExecute, FunctionExecute, network,
FFI and security functionality are not executed by these gates. Complete
01.01 still needs its GC-map cardinality and operation-specific local-ID
mapping evidence and remaining prerequisites.
