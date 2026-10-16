cmake_minimum_required(VERSION 3.20)
project(zr_native_math_validation LANGUAGES C)
enable_testing()

get_filename_component(ZR_NATIVE_MATH_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/../native-math-tests.cmake")
