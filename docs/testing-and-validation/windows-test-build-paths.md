---
related_code:
  - tests/cmake/parser-semantic-targets.cmake
  - tests/cmake/lsp-component-targets.cmake
  - tests/cmake/lsp-query-targets.cmake
  - tests/cmake/manifest-suite-registration.cmake
  - tests/cmake/msvc-object-paths-validation.cmake
implementation_files:
  - tests/cmake/parser-semantic-targets.cmake
  - tests/cmake/lsp-component-targets.cmake
  - tests/cmake/lsp-query-targets.cmake
  - tests/cmake/manifest-suite-registration.cmake
  - tests/cmake/msvc-object-paths-validation.cmake
plan_sources:
  - "user: 2026-10-16 bounded Windows MSVC compiler-object path follow-up"
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
tests:
  - tests/cmake/msvc-object-paths-validation.cmake
  - tests/acceptance/2026-10-16-macro-m1-token-foundation.md
doc_type: testing-guide
---

# Windows Test Build Paths

## Scope And Ownership

The focused follow-up addresses compiler-object paths in the existing physical
`E:/cargo-targets/zr_vm/build/m1-msvc-verified` Ninja tree. Parser test targets
are defined in [parser-semantic-targets.cmake](../../tests/cmake/parser-semantic-targets.cmake).
LSP targets are defined in [lsp-component-targets.cmake](../../tests/cmake/lsp-component-targets.cmake),
with shared setup and include directories in
[lsp-query-targets.cmake](../../tests/cmake/lsp-query-targets.cmake).
Aggregate executable references are maintained in
[manifest-suite-registration.cmake](../../tests/cmake/manifest-suite-registration.cmake).
Internal target identities affect generated `CMakeFiles/<target>.dir` object
directories as well as references to their executable files.

The bounded repair shortens exactly six internal target names using
the abbreviations already used by neighboring targets and update every actual
reference in those four fragments. It does not rename or copy source files,
add compatibility aliases, relocate or alias the build root, change registry
settings, compiler flags, timeouts, test assertions, inventory, or order.

## Observed Failure

Windows r8 foundation CTest completed with 52 unique PASS and no failures.
Its subsequent full build, PID 36132, naturally exited 1 after 499/1,430 actions.
The actual log reports two C1083 failures opening compiler-generated files,
with `Invalid argument`, while compiling the parser sum and unary dataflow
tests. The log is
`E:/cargo-targets/zr_vm/reports/m1-msvc-current-r8-full-build.log`, SHA-256
`0CC1720D7189255D11D6109BC5BFD8211329444762ED9858A26D4BAFD9800DFF`.

The full configuration's 6,828-entry compilation database contains exactly six
absolute object output paths of at least 260 characters. Their source files and
output parent directories exist. The lengths below exclude a terminating NUL;
they are observations of the current graph, not successful compiler results.

| Internal Target Before Repair | Object Path Length |
| --- | --- |
| `zr_vm_numeric_loop_selfdep_conditional_delta_negative_offset_zero_inclusive_sum_dataflow_test` | 269 |
| `zr_vm_numeric_loop_selfdep_conditional_delta_negative_offset_zero_inclusive_unary_dataflow_test` | 273 |
| `zr_vm_lsp_selfdep_conditional_delta_negative_offset_zero_inclusive_query_test` | 268 |
| `zr_vm_lsp_selfdep_cond_delta_neg_zero_incl_subtract_query_test` | 265 |
| `zr_vm_language_server_numeric_loop_self_dependency_symbolic_coefficient_cross_semantic_query_test` | 266 |
| `zr_vm_lsp_selfdep_coeff_cross_four_additional_scale_reader_query_test` | 261 |

Path length was the repair hypothesis, not an established cause solely
from C1083: Microsoft documents multiple causes for that error. The conservative
graph check requires fewer than 260 characters so the traditional `MAX_PATH`
budget includes its terminating NUL. Long-path support is not assumed for every
native tool merely because Windows supports opt-in extended paths.
See Microsoft's [C1083 reference](https://learn.microsoft.com/en-us/cpp/error-messages/compiler-errors-1/fatal-error-c1083?view=msvc-170)
and [maximum path length rules](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation).

## Read-Only Regression

[msvc-object-paths-validation.cmake](../../tests/cmake/msvc-object-paths-validation.cmake)
accepts `BUILD_DIR` and `NINJA_EXE`. It requires an existing `build.ninja`, runs
native Ninja `-C <build> -t targets all`, and reads actual `.obj` compiler-rule
outputs. It normalizes separators, resolves each output against the build root,
requires a positive compiler-object inventory, and fails if any absolute object
path has 260 or more characters. A failed query is an error, not an empty PASS.
The query does not configure, compile, delete, or mutate source files.

The CTest name is `msvc_compile_object_paths`, registered by the parser fragment
only for MSVC with the exact `Ninja` generator and with
`ZR_VM_TESTS_MACRO_FOUNDATION_ONLY` disabled. It is an independent full-build
graph regression, not an addition to or replacement for the explicit foundation
52, lower 323, original 127, or selected full 55 inventories. The budget 11 and
original semantic-analyzer 84 cases must also remain unchanged.

The test-first sequence requires an actual old-graph RED before the six target
renames, then the unchanged regression on the normally regenerated same tree.
The actual old-graph run naturally exited 1 and reported exactly the six path
lengths above, with the expected path-budget fatal error rather than a
prerequisite error (Root session 32680, output chunk `283632`). The bounded
six-name repair is now implemented. The normal Windows r9 full build naturally
exits 0 with 956 planned actions and no `FAILED:` rows. The normal unchanged
graph regression then passes: 6,828 actual compiler-object paths, maximum 258
characters. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-build-graph-ctest-r2-receipt.json`
(SHA-256 `D563475EF1AD7F794D8D9CE41620E467E48B30B616561DDB59ECA5B1383AAB79`)
records exactly two CTest passes, including the independent nine-input FFI graph
check, and all 20 recorded inputs unchanged. This establishes the path-budget
property and a successful build, not successful runtime or platform acceptance.

## Why The Target Identity Matters

In the observed graph the long component is the internal target directory,
`tests/CMakeFiles/<target>.dir`, followed by the source-derived object name.
Setting only `OUTPUT_NAME` changes an executable or library's output basename;
it does not establish a shorter compiler-object directory. That is why the
plan changes the internal identities and their real references instead.
CMake's [OUTPUT_NAME contract](https://cmake.org/cmake/help/latest/prop_tgt/OUTPUT_NAME.html)
describes final target files, while [CMAKE_OBJECT_PATH_MAX](https://cmake.org/cmake/help/latest/variable/CMAKE_OBJECT_PATH_MAX.html)
describes its separate object-path hashing mechanism. This follow-up does not
change that variable or compiler policy to evade the observed paths.

## Acceptance Boundary

R8 did not execute lower 323, original 127, selected full 55, budget 11,
semantic-analyzer 84, or the CLI gate after the full-build failure. Its raw
receipt also remains FAIL because source-post detected foreign changes to
`tests/core/test_ssa_platform_matrix.c`; later HEAD/support gates were not
reached. This retained R8 history is not relabeled by the later R9 build.

R9 foundation CTest passes 52/52 and full build PID 33132 exits 0. The separate
typed-cache receipt `m1-msvc-cache-post-r9.json` (SHA-256
`D4F8443A9E0C672F59D1C81F9D8BD41E0A11568F7F5B34921FADAAC8A75398E5`)
verifies only its selected 119 C plus one MASM objects and their fresh r9 compile
edges. It did not cover the separately discovered zero-dependency fixture
objects with mixed PDB layouts. The original yield native target exits
`0xC0000005`, and source-post
records 12 changed inputs, so the original R9 receipt remains FAIL. Remaining
lower targets, original127, full55, budget11, LSP84 and CLI were not executed in
R9. The [current validation gate](macro-foundation-validation.md) records the
limited debugger evidence. Neither graph/cache PASS nor foundation 52 PASS
waives source or HEAD gates. Linux remains unpassed, M1 remains unaccepted and
uncommitted, and no warning-free (W0), platform, or milestone acceptance is granted.

The later cache follow-up establishes the linked stale-COMDAT ABI mismatch,
not a new GC source-code defect. Its r1 normal build succeeds for all seven
selected objects but still emits seven `VALID 0` header closures; no native
tests run and receipt0D4D43F9 remains FAIL. The two local support helpers then
normalize only native line-start showIncludes prefixes to matching ASCII and
defer the CMake prefix hook until language initialization has completed. They
retain compiler arguments, paths, flags, timeouts and inventories unchanged.

R2 rebuilds the same seven objects with positive `VALID` closures. The saved
post-dependency graph contains 2,474 positive C closures plus only the exact
84-byte tinydir placeholder at zero; absent C objects are not credited. Four
original native runs pass7/3/4/127 with unique ordered rows and zero failures or
ignored cases. Receipt558DFD25 remains raw FAIL because source4788 becomes4789
with six foreign ExecIR changes and HEAD changes. The
[validation ledger](macro-foundation-validation.md) records exact identities,
the narrow helper reviews/controls and the causal evidence.

R10-v2 naturally ended with52 PASS/3 FAIL of55 full CTest entries, despite its
foundation52/lower323/original127 passes. The AOT source, LSP ownership and CLI
incremental suites failed; one concurrent CMake source change also fails the
strict input gate. Standalone CLI/auxiliary3+4/source-final were not reached.
The later focused AOT contract repair passes26/26 with unchanged production
code and stable source/HEAD/tool/binary pins. The complete original52/55/323/
127/11/84/CLI and auxiliary3+4/source-final gates remain required; no dated
subset or graph PASS waives them. Linux and M1 remain unaccepted.
