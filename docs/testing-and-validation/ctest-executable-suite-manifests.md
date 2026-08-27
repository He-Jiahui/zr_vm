---
related_code:
  - tests/CMakeLists.txt
  - tests/cmake/zr_vm_register_executable_suite.cmake
  - tests/cmake/run_executable_suite.cmake
implementation_files:
  - tests/CMakeLists.txt
  - tests/cmake/zr_vm_register_executable_suite.cmake
  - tests/cmake/run_executable_suite.cmake
plan_sources:
  - user: 2026-08-28 require full GCC, Clang, and MSVC acceptance with truthful test execution
  - docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md
tests:
  - tests/CMakeLists.txt
  - tests/cmake/run_executable_suite.cmake
  - tests/acceptance/2026-08-10-ownership-object-member-separation.md
doc_type: testing-guide
---

# CTest Executable Suite Manifests

## Purpose

`run_executable_suite.cmake` runs an ordered list of test executables and fails
the aggregate CTest as soon as a child exits unsuccessfully. Small suites pass
their executable lists directly through `cmake -D`. The `language_pipeline`
suite is large enough that repeating its default, core, stress, and smoke lists
in one CTest command exceeds the Windows process command-line limit before the
runner can start.

Large suites therefore register through
`zr_vm_add_manifest_executable_suite`. The helper moves list data into a
generated CMake manifest and leaves only bounded paths and working-directory
arguments on the CTest command line.

## Related Files

- `tests/CMakeLists.txt` defines the target membership and tier lists.
- `tests/cmake/zr_vm_register_executable_suite.cmake` generates one manifest per
  build configuration and registers the short CTest command.
- `tests/cmake/run_executable_suite.cmake` validates and includes the manifest,
  selects a tier, and executes each child.

## Behavior Model

The registration helper requires a suite name, runner script, host build
directory, run working directory, and default executable list. Smoke, core, and
stress lists are optional inputs, although `language_pipeline` supplies all
three explicitly.

`file(GENERATE)` resolves target-file generator expressions separately for each
configuration. A multi-configuration build therefore produces files such as
`language_pipeline-Debug.cmake` and `language_pipeline-Release.cmake`; a
single-configuration Debug build produces the same Debug-qualified name. The
manifest defines only these suite inputs:

- `SUITE_NAME`
- `EXECUTABLES`
- `EXECUTABLES_SMOKE`
- `EXECUTABLES_CORE`
- `EXECUTABLES_STRESS`

The runner includes the manifest before applying its existing defaults and tier
selection. `TIER` or `ZR_VM_TEST_TIER` still selects `smoke`, `core`, or
`stress`; an unset or unknown tier still runs the default list. The helper does
not change executable order, child output forwarding, host runtime-library
setup, or failure propagation.

## Design And Rationale

Passing a manifest path avoids relying on a platform-specific command-line
limit and remains stable as the aggregate suite grows. Splitting the suite into
unrelated CTests would weaken its ordered aggregate contract, while dropping
tier lists would change existing test selection. A generated manifest preserves
both behaviors without duplicating list data on the process command line.

Other small suites may continue using direct `-DEXECUTABLES` arguments. The
runner treats `SUITE_MANIFEST` as an optional transport, not as a second suite
execution model: after loading, both registrations use the same selection and
execution path.

## Edge Cases And Constraints

- A supplied manifest path must exist; a missing file is a configuration error
  and terminates the runner before any child starts.
- Suite names must be unique within one tests binary directory because they
  determine the manifest basename and CTest name.
- Executable entries are paths only. Per-child argument vectors are outside
  this runner's contract.
- A child launch error or nonzero exit remains a suite failure. The manifest
  transport must never turn a missing or failing child into a passing CTest.

## Test Coverage

The registered `language_pipeline` CTest is the end-to-end regression. On MSVC,
CTest must be able to start `run_executable_suite.cmake` instead of reporting
`BAD_COMMAND`; after the graph is built, every listed child must run and any
child failure must propagate. GCC and Clang configuration verifies the same
per-configuration manifest generation under single-configuration Ninja builds.

The ownership/member separation acceptance uses this aggregate suite as part of
its final three-toolchain replay. That acceptance remains incomplete until the
stable post-L8 build and test matrix has run.

## Plan Sources

This transport was added while closing the ownership/object-member separation
milestone. A pre-L8 full MSVC graph exposed the command-length failure before
the first `language_pipeline` child could execute; the language design itself
does not depend on the manifest format.
