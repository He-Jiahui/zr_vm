# SSA 01.02: focused builder test registration boundary

`tests/cmake/ssa-tests.cmake` had reached 1078 lines after adding the
builder-specific fixtures. The four independent builder targets and their
shared real production sources now live in `tests/cmake/ssa-builder-tests.cmake`,
included at the original position by the unchanged central SSA suite.
CTest target names, labels, source lists, include directories and compiler
definitions were preserved. The other session's dirty
`tests/CMakeLists.txt` was not edited.

MSVC/VSDevCmd rebuilt the same four `zr_vm_ssa_builder_*_test` targets in
the D:-backed build; CMake regenerated successfully and Ninja reported
`no work to do`. The adjacent SSA CTest selection passed 11/11 both before
and after the move, including all four builder targets. No runtime behavior
or compiler/lowering rule changed in this refactor. GCC/Clang CMake target
registration was not run for this CMake-only boundary.
