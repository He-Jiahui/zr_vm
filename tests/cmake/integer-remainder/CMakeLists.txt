cmake_minimum_required(VERSION 3.21)
project(zr_vm_integer_remainder LANGUAGES C)

enable_testing()
include("${CMAKE_CURRENT_LIST_DIR}/../integer-remainder-tests.cmake")
zr_vm_add_integer_remainder_tests()
