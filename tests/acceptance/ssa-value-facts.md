# Canonical SemIR value facts into ExecIR

## Scope

This stage closes the ownership/nullability producer-consumer prerequisite in
`docs/plans/ssa/01-execir-ssa/02-ssa-construction.md`. It does not complete 01.02
or 01.04. The compiler snapshots canonical types during pre-SemIR validation;
the ExecIR builder validates and consumes those snapshots for both externally
supplied and instruction-defined values. Strict DROP remains strict.

Implementation and limitations are described in
`docs/parser-and-semantics/semantic-value-facts.md`.

## Baseline and failures encountered

- Base commit: `6eb8c963`. Builder ownership/nullability was unconditionally
  unknown. New builder tests first failed GCC compilation because SemIR lacked
  the fact fields/enums; the focused resolver test failed for the missing
  `semantic_value_facts.h`. These were observed before production changes.
- Initial source fixtures compiled and validated SemIR, but builder diagnostic
  `UNSUPPORTED` reported expected edge kind 4, actual 7. Investigation traced
  this to the existing plain-line fallback CFG's synthetic RETURN edge, not to
  an ownership instruction. The final source fixtures use an actual source
  `if (true) {}` continuation to activate the existing CFG producer. A separate
  plain-line fixture verifies fact publication without claiming that legacy
  graph is lowerable. No test manually edits the compiler-produced IR.
- The source test registration initially omitted the shared Unity test helper
  and failed to include `zr_vm_common.h`; using the repository's helper fixed
  the harness. Test authoring also corrected a nonexistent instruction `id`
  field to the canonical one-based instruction index. These setup errors are
  not counted as behavioral RED evidence.
- Appending facts, instead of inserting them between existing members,
  preserves old positional initializer meaning. A four-field initializer
  syntax check failed with the intermediate layout and passed after the field
  moved to the tail. The final regression checks its definition/source fields
  and zero snapshot. Clang's expected missing-trailing-field warning remains
  visible for this intentional compatibility fixture.
- An initial nested PowerShell command expanded its exit-code variable in the
  outer shell. Each legacy executable was rerun directly and its own exit code
  captured; only those independent successful runs count below.

Unrelated pre-existing dirty source, tests and plan files were not reverted,
staged or used as grounds to claim a repository-wide clean baseline.

## Test inventory

`semantic_value_facts` adds 13 Unity cases:

- all primitive tags, GC handles versus native pointer-like payloads;
- unique/shared/atomic-shared/weak, both wrapper orders, nullable ref and
  nullable pointee distinction;
- nominal/generic/tuple/union/function/error/never and managed/inline/native
  arrays, without inferring scalar ownership from aggregate scan metadata;
- stale witness refresh and sentinel clearing;
- later-value failure leaving every earlier snapshot unchanged;
- invalid owner/ref/array/function/generic enums, IDs and nested contracts;
- self cycles and 4,096-deep wrapper chains without recursive traversal;
- unrelated malformed canonical node isolation;
- null arguments, broken array widths/capacities/storage and registry IDs;
- snapshot enum/witness checks and old positional initializer compatibility.

`ssa_source_value_facts` adds four actual-source cases:

- `own → share → ref readonly` type facts survive normal compiler and builder;
- explicit unique drop remains strict and owner analysis transitions from
  initialized before the instruction to dropped after it;
- `degrade` is not made a strong owner and `wake` retains nullable shared type;
- plain-line compiler validation publishes facts and can be repeated.

Existing standalone builder fixtures now cover all eight semantic ownership
projections for defined and external values, invalid/stale snapshots preserving
caller output, and facts surviving multi-INVOKE CFG splitting.

The selected CTest denominator is 15 suites:

```text
ssa_builder_cfg                 ssa_builder_dominance
ssa_builder_control_edges       ssa_builder_cleanup_dispatch
ssa_cleanup_exception_state     ssa_builder_fact_identity
ssa_builder_iterator_invokes    ssa_place_eligibility
ssa_place_promotion             semantic_value_facts
ssa_source_value_facts          ssa_source_cleanup_cfg
ssa_state_map_ownership         ssa_state_maps
ssa_conditional_cleanup
```

These contain 141 Unity cases in six suites and nine standalone contract
executables. Three additional legacy targets are invoked directly because
they are not registered in this selected CTest set: `pre_semantic_ir` (101),
`canonical_type_graph` (19), and `reference_loan_nll` (15). Thus each full
selected matrix covers 276 Unity cases plus nine standalone executables; this
is not the entire repository test suite.

## Tooling evidence

| Environment | Observed result |
| --- | --- |
| WSL x86-64 GCC 11.4.0 Debug | 18 targets built; 15/15 CTests and all three direct legacy executables passed |
| WSL x86-64 Clang 14.0.0 Debug | 18 targets built; 15/15 CTests and all three direct legacy executables passed |
| Windows x64 MSVC 19.44.35228 static Debug | 18 targets built; 15/15 CTests and all three direct legacy executables passed |
| Windows x64 MSVC 19.44.35228 DLL Debug | Both new suites passed, 17 Unity cases across the DLL boundary |
| WSL GCC ASan/UBSan Debug | 18 targets built; 15/15 CTests and all three direct legacy executables passed with leak detection and halt-on-error |

CTest/CMake version is 3.22.1 in WSL. Reproduction from the WSL repository root:

```bash
suites=(semantic_value_facts ssa_source_value_facts ssa_builder_fact_identity
        ssa_builder_control_edges ssa_builder_cfg ssa_builder_dominance
        ssa_builder_cleanup_dispatch ssa_cleanup_exception_state
        ssa_builder_iterator_invokes ssa_place_eligibility ssa_place_promotion
        ssa_source_cleanup_cfg ssa_conditional_cleanup ssa_state_map_ownership
        ssa_state_maps)
targets=()
for suite in "${suites[@]}"; do targets+=("zr_vm_${suite}_test"); done
targets+=(zr_vm_pre_semantic_ir_test zr_vm_canonical_type_graph_test
          zr_vm_reference_loan_nll_test)
pattern="^($(IFS='|'; echo "${suites[*]}"))$"
for build in build/ssa-gcc-debug build/ssa-clang-debug; do
  cmake --build "$build" --target "${targets[@]}" -j 4
  ctest --test-dir "$build" -R "$pattern" --output-on-failure --no-tests=error
  for suite in pre_semantic_ir canonical_type_graph reference_loan_nll; do
    "$build/bin/zr_vm_${suite}_test"
  done
done
```

Windows used the same target list/regex in the initialized Visual Studio x64
environment, `build/ssa-msvc-debug`, and direct `.exe` invocations for the three
legacy suites. The DLL check used `build/ssa-msvc-shared-debug` and regex
`^(semantic_value_facts|ssa_source_value_facts)$`.

The existing sanitizer build uses `-fsanitize=address,undefined
-fno-omit-frame-pointer`. Its completed run used:

```bash
cmake --build build/ssa-gcc-asan-phase80 --target "${targets[@]}" -j 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/ssa-gcc-asan-phase80 -R "$pattern" \
  --output-on-failure --no-tests=error
for suite in pre_semantic_ir canonical_type_graph reference_loan_nll; do
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    "build/ssa-gcc-asan-phase80/bin/zr_vm_${suite}_test"
done
```

No sanitizer or leak finding was reported in these completed runs.
Existing build warnings remain visible; no warning suppression or test skip
was added to achieve the result.

## Review and acceptance decision

Accepted on September 25, 2026 for this bounded prerequisite. Independent
specification and code-quality reviews passed without critical or important
findings. All four full selected matrices and the Windows DLL check completed
successfully. No outstanding failure is being omitted from the selected set.

Remaining larger-plan work includes normal effect-chain generation,
compiler-generated conditional cleanup, full fallback CFG lowering, weak
execution protocols, physical frame publication, inline-frame recovery and
automatic aggregate recipe production. No ownership-sensitive optimization is
enabled by this stage alone.
