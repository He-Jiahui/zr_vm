# Syntax 10R M1 Specifier Foundation

## Scope

- Added a library-level, domain-aware ModuleSpecifier and ModuleIdentity substrate.
- Added a focused library test and CTest registration for source spelling classification and relative identity
  resolution.
- Did not change parser syntax, project manifest loading, provider selection, artifact loading, or the 06A
  legacy import resolver.

## Baseline

- The initial RED build linked `test_project_module_specifier.c` against the existing library and failed only
  because `ZrLibrary_ModuleSpecifier_Parse`, `ZrLibrary_ModuleIdentity_Equals`, and
  `ZrLibrary_ModuleSpecifier_ResolveRelative` were absent.
- The first build also exposed the Unity target requirement for `setUp` and `tearDown`; empty fixture hooks
  were added before repeating RED, leaving only the expected missing APIs.
- Review-derived RED cases then exposed three contract gaps: multi-level `../../mesh` was rejected, two
  parsed `@math` package roots compared unequal because their default-entry segment list is empty, and an
  in-place relative-resolution output could erase a later input read. All were repaired at the structured
  parser/identity layer.

## Test Inventory

- Absolute `zr`, `native:`, and Workspace forms normalize dot/slash segment spellings and retain distinct
  domains for same-named registered-native and workspace modules.
- Relative `./`, repeated `../`, and continuous-dot spelling resolve from a canonical caller identity without
  changing its Workspace or Package domain; traversal above the root is rejected.
- `#alias` splits root and suffix; `@package` accepts a single package root with optional export path.
- A package root has a valid canonical identity and can compare equal to the same root, but cannot serve as a
  relative-resolution caller until it identifies a current module leaf.
- Relative resolution permits the output identity to alias either identity input without losing the caller
  domain, package root, or target segments.
- `file:///E:/...`, `file:///opt/...`, and `file://host/share/...` remain locators with no public identity.
- Empty segments, reserved `native:zr.*`, incomplete relative and UNC forms, invalid package names, malformed
  aliases, and bare physical paths are rejected.
- Existing project import resolver regression protects the untouched 06A migration behavior.

## Tooling Evidence

- GCC 11.4.0, isolated `.codex/build-s10r-m1-gcc`: after the final review fixes, the new target passed 5/5 and
  focused CTest passed 1/1; `zr_vm_project_import_resolver_test` also passed 9/9.
- Clang 14.0.0, isolated `.codex/build-syntax06a-m2-clang`: the new target passed 5/5 and focused CTest
  passed 1/1.
- MSVC 19.44.35228, isolated `.codex/build-syntax06a-m2-msvc`: the new target passed 5/5 and focused CTest
  passed 1/1.
- Build logs retained existing warnings from unrelated initializer tables, legacy const-qualified signatures,
  MSVC `/W3` override and long object paths; the new module emitted no compiler warning or test failure.

## Results

- The missing-API linker RED and the three subsequent behavioral RED cases became five passing behavioral tests
  after adding the narrow parser module and correcting its contracts.
- Domain, logical segments, and package root are compared structurally; provider locator and source spelling
  remain outside ModuleIdentity.
- The legacy resolver still passes its nine existing tests and is not used by the new API.

## Acceptance Decision

- Accepted: focused validation, legacy-resolver regression, code review, and exact-path audit are complete;
  the milestone is represented by one exact-path commit.
- M2 responsibilities remain deliberately unimplemented and cannot be inferred from the M1 parser result.
