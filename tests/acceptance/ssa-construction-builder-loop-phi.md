---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
tests:
  - tests/parser/test_ssa_construction.c
  - tests/parser/test_ssa_place_promotion.c
status: partial
---

# SSA 01.02: builder loop-carried phi

## Scope

This checkpoint covers the public SemanticIR-to-ExecIR builder on a four-block
loop. A scalar local receives an entry definition and a body definition formed
by adding a separately defined constant to the header LOAD. The body backedge
targets the header; the other header edge reaches a final LOAD and RETURN.
Both arms are canonical source CFG edges, not decoded execution bytecode.

## Baseline

Before this test, `ssa_construction` covered a canonical diamond and an unread
phi but no loop through the public builder (7 Unity cases). The direct
`ssa_place_promotion` test already exercised a loop at the lower-layer API.
This is a builder-integration coverage gap, not an observed production defect.

## Test Inventory

`test_ssa_construction_builds_loop_carried_place_phi` checks exactly one header
phi, ordered `(entry, initial)` and `(body, next)` incomings, STORE-to-NOP and
LOAD-to-COPY promotion, the body ADD's use of the header value, and core
structural/SSA verification. `test_ssa_construction_loop_without_entry_definition_is_atomic`
deletes the entry STORE while retaining the loop. It requires an `INVALID_VALUE`
diagnostic at the header, unchanged previously published blocks/phis, and a
successful retry with the original source facts. The fixture's separately
defined constant has no runtime literal payload: these checks assert SSA data
flow, not arithmetic results or source-language execution. OOM and cancellation
injection remain outside this focused test.

## Tooling Evidence

GCC 11.4.0 and Clang 14.0.0 Debug builds use the WSL caches at
`/home/hejiahui/zrvm-ssa-nested-gcc.4pVemu` and
`/home/hejiahui/zrvm-ssa-nested-clang.kBIWlA`, respectively. The focused
test object and executable were rebuilt from each cache's `ninja -t commands`
recipes, then invoked as follows:

```bash
/home/hejiahui/zrvm-ssa-nested-gcc.4pVemu/bin/zr_vm_ssa_construction_test
ctest --test-dir /home/hejiahui/zrvm-ssa-nested-clang.kBIWlA --output-on-failure --no-tests=error -R '^ssa_construction$'
ctest --test-dir /home/hejiahui/zrvm-ssa-nested-gcc.4pVemu --output-on-failure --no-tests=error -R '^(ssa_construction|ssa_place_promotion|ssa_builder_cfg|ssa_dominator_cfg|ssa_source_straight_line_cfg)$'
```

MSVC 19.44.35228.0 used the existing Ninja Debug cache
`build/codex-ssa-conversion-msvc`. The Visual Studio environment was imported
through `C:/Users/HeJiahui/.codex/skills/using-vsdevcmd/scripts/Invoke-VsDevCommand.ps1`:

```powershell
cmake --build build/codex-ssa-conversion-msvc --target zr_vm_ssa_construction_test -j 4
ctest --test-dir build/codex-ssa-conversion-msvc --output-on-failure --no-tests=error -R ^ssa_construction$
```

## Results

GCC and Clang focused Unity: 9 tests, zero failures on each toolchain. Clang
focused CTest: 1/1 passed. MSVC built the test target and passed CTest 1/1.
MSVC CMake emitted path-length warnings for unrelated long-named test targets;
this test target compiled and linked successfully.
The GCC five-test adjacent selection initially had four passes and one Not Run
because the cached `ssa_dominator_cfg` executable was absent. After building
its 14 missing test-target objects and linking the executable, the same CTest
selection passed 5/5. The initial Not Run was not counted as a pass.

## Acceptance Decision

This is a partial 01.02 integration checkpoint. Source-level execution,
exception/cleanup/suspend paths, numeric literal behavior, sanitizer runs,
and the full 01.02 exit gate are not proven here.
