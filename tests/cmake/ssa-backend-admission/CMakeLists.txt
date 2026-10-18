cmake_minimum_required(VERSION 3.21)
project(zr_vm_ssa_backend_admission LANGUAGES C)

enable_testing()
include("${CMAKE_CURRENT_LIST_DIR}/../ssa-backend-tests.cmake")
zr_vm_add_ssa_backend_test(admission)
zr_vm_add_ssa_backend_test(service)
