# Test Suite and CI Recovery Plan

## Objective

Reconcile the active syntax-migration test layout, remove obsolete test
registrations, and make the GitHub CMake matrix reliably configure, build, and
run its supported test set on GCC, Clang, and MSVC.

## Baseline

- The only workflow is `.github/workflows/cmake-multi-platform.yml`.
- It runs all registered CTest tests for GCC, Clang, and MSVC.
- Remote run `30209499167` failed on `503fb72`; failures have been continuous
  since May.
- `tests/CMakeLists.txt` contains active migration work that relocates several
  suites. Its staged and working-tree changes must be reconciled, not reset.

## Execution

1. Capture the latest Actions job and failed-step evidence, then reproduce the
   same configuration in isolated Linux and MSVC build directories.
2. Classify every failure as a build-registration error, obsolete test,
   platform requirement, or behavioral regression. Keep current syntax-gate
   coverage and delete only tests proven superseded.
3. Consolidate registrations around the current test directories, use CTest
   labels for supported suites, and correct missing source/link dependencies.
4. Update the workflow to expose configure/build/test failure output and run
   the supported matrix explicitly.
5. Run configure, build, and CTest for GCC, Clang, and MSVC from clean caches;
   record any intentional exclusions in the test configuration and plan.

## Acceptance Criteria

- CMake no longer references deleted or duplicate test sources/targets.
- Every retained test is registered exactly once in the correct suite.
- GCC, Clang, and MSVC configure and build from fresh caches.
- The CI-equivalent CTest invocation passes on each supported platform, or any
  platform exclusion is explicit, justified, and covered by the other matrix
  jobs.
- Workflow logs identify the failed CTest case without requiring a rerun.
