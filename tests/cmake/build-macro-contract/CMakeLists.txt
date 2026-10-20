cmake_minimum_required(VERSION 3.20)
project(ZrBuildMacroContract LANGUAGES C)

get_filename_component(ZR_VM_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
# Production helpers intentionally resolve first-party includes from the repo root.
set(CMAKE_SOURCE_DIR "${ZR_VM_SOURCE_ROOT}")
include("${ZR_VM_SOURCE_ROOT}/zr_vm_common/CommonMacros.cmake")
set(BUILD_STATIC_LIB ON)
set(BUILD_SHARED_LIB OFF)
add_library(contract_dependency_static INTERFACE)
add_executable(contract_executable link_probe.c)
zr_link_library_for_executable(contract contract_dependency)
# This is the same keyword signature used by the third-party linker helper.
target_link_libraries(contract_executable PRIVATE contract_dependency_static)
