---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_dead_source_places.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - tests/parser/test_ssa_dead_source_places.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/parser/ssa_dead_source_places_edges.inc
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_dead_source_places.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
plan_sources:
  - .codex/plans/20261005-ssa-dead-source-places.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
tests:
  - tests/parser/test_ssa_dead_source_places.c
  - tests/parser/ssa_dead_source_places_edges.inc
  - tests/acceptance/ssa-dead-source-places.md
doc_type: acceptance
status: dead-source-places-focused-windows-green-accepted
fixture_extraction_status: focused-windows-green-accepted
---

# Dead Source Places Acceptance

## Finite scope

The [module contract](../parser-and-semantics/ssa-dead-source-places.md) covers
shared graph compaction of the actual literal i64 SCRIPT source graphs.
The focused Windows producer and selected consumers are **GREEN accepted**.
The whole 47-item SSA plan remains **OPEN**.
This does not inherit the independent VM emitter's dead-place acceptance.

## Frozen behavioral RED

RED commit: `5b9d7d829624eaee52e9f7ec5766d17e968a5293`.
Receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/dead-source-places-red-receipt.json`.
Receipt SHA-256: `6966F221CE509D303D8B8608C8D946BF962F38594F2E8FDBC29A0BCCB71D2A28`.

Windows x64 clang-cl 19 Debug with assertions and UBSan built the callable
UNSUPPORTED stub successfully (exit 0). The two actual-source prerequisite
cases passed, with Core `VERIFY_ALL` and Oracle returns 9 and 8; no normalization
or fabricated identity supplied those prerequisites. Full CTest then exited 8:
11 tests, 6 failures, 0 ignored. Four feature cases returned UNSUPPORTED. The
sealed boundary expected SEALED(23) and the invalid range expected
INVALID_RANGE(11), but both received UNSUPPORTED(28). Actual address-use refusal,
non-builder provenance refusal and input/output alias refusal already passed.
This establishes behavior failure after successful compilation and source proof.

| RED evidence | SHA-256 |
| --- | --- |
| `dead-source-places-red-build.log` | `E62F4636BBB7234DA448E7652F6EA13ED6CEDBB8831B1D54B03A3BDF2D8E2672` |
| `dead-source-places-prerequisites.log` | `9E622573AED70AD6698412C4CB831392B26AE359764FA9F0D0FFCAB1D8483DD6` |
| `dead-source-places-red.log` | `52A9180B78A2CABFF146FEF30003626EF9C85875009DC47F620E1E5DF22C49DC` |
| Original fixture | `3A9442BA0358E5C45F6C750A66B99AF360B029EAAE4DC2B116B1FFD017565872` |
| Original unsupported TU | `50136268591DA300EF27856F4684DF4C7629352925D90CD4D124F530C2DD462D` |

## First focused GREEN

Receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/dead-source-places-green-receipt.json`.
Receipt SHA-256: `DD09B4BF8AAE1253C282E2AADF84D1EC4F8F1EFD0E3ACE3555FE09850D1FF92C`.
Base HEAD in this receipt is the RED commit above; the receipt pins current
directly edited source bytes before the GREEN commit.

Build exited 0. The independent prerequisite run passed 2/2, with actual Oracle
returns 9/8. Full focused CTest exited 0: **30 tests, 0 failures, 0 ignored**
(2 prerequisites, 8 features, 20 guards). No UBSan diagnostic was observed.
The production entry now proves real identities and unused builder provenance,
clones and compacts the candidate, verifies it, and transactionally publishes.
The SEALED and INVALID_RANGE diagnostic failures from RED now pass.

Additional guards cover interior capacity-span overlap for values, operands and
state values, and missing exact SemIR symbol/callable identity. Positive cases
cover output replacement, repeated original and compressed inputs, null
diagnostics and preserved empty state headers. Rich state/deopt graphs are
VERIFY_ALL-valid before their explicit UNSUPPORTED admission refusal.

| Historical pre-extraction GREEN source | SHA-256 |
| --- | --- |
| `exec_ir_dead_source_places.h` | `D4A0964DE327446F2893CA8DD12ED6B6CF02875915B1B450DEFE06F5594D71DF` |
| `exec_ir_dead_source_places.c` | `5EC5BC993FE65D15A00F45380AB58FF35C20E78862D592E5C783DC6BC5796E6B` |
| `test_ssa_dead_source_places.c` | `B6A5E628754B7030CA51F850D627E5764C2CEC719EF2C4964F4356AD5B76A6D9` |
| `ssa_dead_source_places_edges.inc` | `ADEA900B9D26C33B06179793C917CF2FEDBA14FA3C32A6A75FC9F5E2B40F9138` |
| Direct validation CMake | `DF52A31E370088854FC49F12546C4A77C0FACB61A331C718237516B3A9230606` |
| Ordinary source CMake fragment | `454EDB09CC5709EC99DE665D9892CA2346FF3CC0A0540EC4A31C6D7420932C1D` |

| GREEN evidence | SHA-256 |
| --- | --- |
| `dead-source-places-production-build.log` | `83878CEB0F3762A30092153C2F7948E02EA8386A227D6CAC289BB62686266CE5` |
| `dead-source-places-green-build.log` | `133CA3B6D50C3985BF73713FEBFE617F10902B1D75FB4E518763BA2D01C648B0` |
| `dead-source-places-green-prerequisites.log` | `458409EFD637141BFCB4D12CDB3EDF237367CCB829999BD9E8C90BD40130F42F` |
| `dead-source-places-green.log` | `EF9A3B14BDD07683DDC4A9EA135AF7799913A893C89D2F5C4CCC71CABB482EE5` |
| Focused binary | `5DB3CB3258AA6C2298AF1706A657C1B6943F29C982637EBBA99DB11B99905005` |

## Committed pre-extraction rebuild and consumer regression

Primary receipt for the committed pre-extraction producer gate:
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/dead-source-places-current-rebuilt-green-receipt.json`.
SHA-256: `CC31B6D32B550D0996FD6939AC262855E077C072C96FBEC08850C23EA7821865`.
The first DD09 receipt above is historical focused evidence. After selected
current Core/parser/library shared dependencies were rebuilt, fresh configure,
focused relink and focused CTest all exited 0. All 30 cases passed again, with
0 failures/ignored and no UBSan diagnostic. Source pins are unchanged from the
table above; that run's build metadata is recorded in the primary receipt.

| Final evidence | SHA-256 |
| --- | --- |
| `dead-source-places-current-configure.log` | `DC8A9280A85D07838943508181E85BCAD8B88DA3224CE4D7F830D91CE0EC2483` |
| `dead-source-places-current-rebuilt-build.log` | `133CA3B6D50C3985BF73713FEBFE617F10902B1D75FB4E518763BA2D01C648B0` |
| `dead-source-places-current-rebuilt-green.log` | `524089C67D4EB1CC1FD85D76B346C5289F7D0A3F185C62910D21B13CC7213639` |
| Pre-extraction focused binary | `1A930FC76DF42198E120F3FD67DADE8B8379F7BCF4FD0B8531462774CF5F8B93` |
| `source-direct-target-sources.tsv` | `91E3D183E06287F817546E4F5FC3F3F9C1EFFA15E09F53FA7D725F87D1A44EA4` |
| `source-direct-header-hashes.tsv` | `F53D29ABC579078195E990530B5E26BD26B49DEDBD7C92D19B6C170659142B16` |
| `source-direct-target-settings.tsv` | `0E54797AA5DE91FDC616E1B82EC68F819D885130725C0ACF8ED74E6F497C2BD5` |

Regression receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/dead-source-places-regression-receipt.json`.
SHA-256: `B5C538AD6C65EDC550B8B5F253F19666989E474684885F524F492E55D910A1EE`.
Build and CTest exited 0: **8/8 entries**, 85 Unity cases plus standalone
dominator CFG, no UBSan diagnostic. The exact suites are:

- `ssa_source_execbc_vm_loops`
- `ssa_source_straight_line_cfg`
- `ssa_source_callable_return`
- `ssa_source_script_entry_tokens`
- `ssa_source_callable_identity`
- `ssa_source_script_entry_identity`
- `ssa_typed_binding_contract`
- `ssa_dominator_cfg`

Regression build log `dead-source-places-regression-build-v2.log` SHA-256 is
`07FFA30BF546CDF3E12E792E39B8C2BA47AEB600C86A073571DF3FC81E9186E6`;
CTest log `dead-source-places-regression.log` SHA-256 is
`7F1D855E5C061413A3E24538E602D786DB9EC4CA976B2470507A161BB20D1712`.
The first build attempt exited 1 because the invented target
`zr_vm_ssa_source_execbc_vm_loops_test` does not exist; no test executed in that
attempt. It was corrected to the actual `zr_vm_ssa_source_execbc_vm_test` target
and its `--loops-only` CTest selection. Its preserved log
`dead-source-places-regression-build.log` has SHA-256
`5FF06ACE38E2DA2F1D6C38FC993EBBA4CD6EEA84D0147589E8536DC952C47720`.
Existing shared sources emitted warnings; this is not a warning-free repository
claim. All Root-owned finite native build/test jobs exited naturally; active 0,
stopped 0. The focused producer plus selected consumer gate is accepted.

## Shared literal fixture extraction

The previous finite tasks were committed as:

- Production/API/module contract: `678181d5cb93c109610a226fd2394d52dbe6c714`.
- Thirty-case fixture/edges: `e2cfd29ab9abdd95055b42183b50c64440dce391`.
- Acceptance and navigation docs: `fe4e08c5257d0522609c34d077d099eaad801a71`.

Their commit receipt SHA-256 is
`100DDC487A25580740202A2C5D674881CA4A683589E94F3DD359199A8A0D88F8`.
The new finite task moves the existing real-source preparation to
`tests/parser/support/ssa_literal_script_fixture.h/.c`; it does not duplicate
the preparation route or create a source snapshot. The main retains the
30 cases, CLI and caller-owned Oracle/output/mutated graph teardown. Shared
Prepare does no compaction/frame attachment and does not implicitly run Oracle.
Both CMake routes attach the same support TU and direct metadata maps its header.

Independent gpt-6-sol specification and lifecycle review reports no blocker.
The extraction is **focused Windows GREEN accepted**. Immutable receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/literal-fixture-extraction-green-receipt-v2.json`.
SHA-256: `93DCDB420BCDB4E549405933F008CEDAAD836A792BA921DD174A474C47A65A3C`.
Windows x64 clang-cl 19 Debug with assertions and UBSan configured and built
successfully, then independently passed 2/2 source prerequisites and all 30
focused cases (0 failures, 0 ignored). All four commands exited 0 and no UBSan
diagnostic was observed. Native session 4729 ended naturally: active 0,
stopped 0. This extraction rerun does not claim a new eight-suite consumer run.

| Current extraction source | SHA-256 |
| --- | --- |
| `test_ssa_dead_source_places.c` | `E02A7F8FBA6DF8413862471C2622E3AA2014E3AA5B5504BBA8C27889ED6CDD55` |
| `ssa_literal_script_fixture.h` | `17FD2BBBFA6F625999CDFECA89693BAEB46F0F7F4F31AC7FD8A8C7FD6B5F7F39` |
| `ssa_literal_script_fixture.c` | `05FB240863B3EE4D3039836B9457D6A40E08BBF65A42711DF7EB2AA7AC8CCA23` |
| `ssa_dead_source_places_edges.inc` | `ADEA900B9D26C33B06179793C917CF2FEDBA14FA3C32A6A75FC9F5E2B40F9138` |
| Direct validation CMake | `2533DD544F199D99F5951DBF9863D152007C5963C236CD0EBF7A739E4014B03B` |
| Ordinary source CMake fragment | `8B386439E14D8CE921105E6335D6D5A86DCE9846F88EB70540FF1C0C0EF09F8A` |
| Unchanged production TU | `5EC5BC993FE65D15A00F45380AB58FF35C20E78862D592E5C783DC6BC5796E6B` |

| Extraction evidence | SHA-256 |
| --- | --- |
| `literal-fixture-extraction-configure-v2.log` | `07CDCC19C7480AF180987D5907FBAA575C0D046A075F0AA900700BA162CA7507` |
| `literal-fixture-extraction-build-v2.log` | `328F03574583B5EB1DE535861D78C46C72BC63081870570372753994D2E8CEAC` |
| `literal-fixture-extraction-prerequisites-v2.log` | `7DAE890AF9315B6CFA9822B86B7285EF1FEFB29C44230CD21299472BF6F000B0` |
| `literal-fixture-extraction-green-v2.log` | `24EBE2F288736278205E0BDD949D7E60688482B441F37D419928D81247AF5DE2` |
| Extraction focused binary | `915C9FB9DD2067296BB9ABBD293330C7C8AAD97D41E55922EB425E60FC6A52BB` |
| `source-direct-target-sources.tsv` | `0818820950E26EA009A5D8D95F83B9B86AFD37100E51E3BA4139D8D3BE25964C` |
| `source-direct-header-hashes.tsv` | `414E2DCCCE70C3829C7CEA9541574F1B5E43D1B7206636D50B18365E0ECFFA63` |
| `source-direct-target-settings.tsv` | `477CCD16082A4E4D531C32FE2E836418844FF288F2ACD8A43A58D8E05DB418C2` |

The current target-sources TSV records the shared support TU as a real
`zr_vm_ssa_dead_source_places_test` input with the 05FB hash above. The support
header is included in the refreshed header metadata. Historical B6A5 main,
DD09/CC31 binaries and earlier CMake/metadata hashes remain pre-extraction pins.

The original `literal-fixture-extraction-green-receipt.json` remains preserved
with SHA-256 `5524AE58BD9F3E8B583FAD1FEF6A90E4995CB6DD0839BA2F1D7319E51A345673`.
It records the first successful extraction run and support-TU hash
`795E61998B9DB99157374689065FA2589FB98BCACF764F32F434E3181C68F15E`.
Root's staged whitespace check then found one extra EOF blank line in the new
support TU. Exactly that blank line was removed; all other source pins stayed
unchanged. Fresh configure/build/2-prerequisite/30-full execution produced the
v2 final receipt above, without overwriting the first receipt or broadening
testing. The first native session 12896 and final session 4729 both ended
naturally. The EOF correction changes no production behavior.
The existing WSL access denial remains OPEN; the prior MSVC compile evidence
covers only the production TU, not this newly extracted support TU or MSVC
runtime. The extraction does not close SSA47 or native/frame/artifact gates.

## Toolchain boundary

MSVC production-TU compilation exited 0 using
`E:/Visual Studio/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe`.
The initial `cl.exe` name invocation failed because it was absent from PATH;
the absolute path corrected the invocation. This is compile-only evidence,
not MSVC runtime or full matrix acceptance. Log
`dead-source-places-msvc-smoke-v2.log` has SHA-256
`229ED541A393A724FBD8525A0D3C7E88EB800EE0F5C5946D95F29E466CDF39A6`.

`wsl.exe --list --quiet` exited 1 with
`Wsl/EnumerateDistros/Service/E_ACCESSDENIED`. Linux GCC and Clang runs remain
unexecuted/OPEN; the probe failure is not a test pass.

The [test acceptance guide](../../tests/acceptance/ssa-dead-source-places.md)
owns the commands, inventory and acceptance decision. Builds read the checkout
directly and place outputs on E: under the managed cargo-targets roots; no
source snapshot or cross-drive copy is part of this task. Network, FFI,
providers and security probes are not executed. Other toolchains, exhaustive
OOM injection, arbitrary expressions and retained/native artifacts are open.
