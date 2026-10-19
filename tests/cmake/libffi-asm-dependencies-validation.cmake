cmake_minimum_required(VERSION 3.20)

foreach (required SOURCE_DIR BUILD_DIR NINJA_EXE)
    if (NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif ()
endforeach ()

get_filename_component(SOURCE_DIR "${SOURCE_DIR}" ABSOLUTE)
get_filename_component(BUILD_DIR "${BUILD_DIR}" ABSOLUTE)
execute_process(
        COMMAND "${NINJA_EXE}" -C "${BUILD_DIR}" -t inputs
                zr_vm_lib_ffi/generated/libffi/x86_win64/win64_intel.asm
        RESULT_VARIABLE query_result
        OUTPUT_VARIABLE query_output
        ERROR_VARIABLE query_error
)
if (NOT query_result STREQUAL "0")
    message(FATAL_ERROR "Ninja input query failed (${query_result}):\n${query_output}${query_error}")
endif ()

string(REPLACE "\r\n" "\n" query_output "${query_output}")
string(REPLACE "\r" "\n" query_output "${query_output}")
string(REPLACE "\\" "/" query_output "${query_output}")
string(REPLACE "\n" ";" input_lines "${query_output}")
set(actual_inputs)
foreach (input IN LISTS input_lines)
    if (NOT input STREQUAL "")
        get_filename_component(input "${input}" ABSOLUTE BASE_DIR "${BUILD_DIR}")
        list(APPEND actual_inputs "${input}")
    endif ()
endforeach ()

set(vendor "${SOURCE_DIR}/zr_vm_lib_ffi/third_party/zr_libffi/libffi")
set(required_inputs
        "${vendor}/src/x86/win64_intel.S"
        "${vendor}/include/ffi.h.in"
        "${SOURCE_DIR}/zr_vm_lib_ffi/cmake/fficonfig_x86_win64.h.in"
        "${SOURCE_DIR}/zr_vm_lib_ffi/cmake/preprocess_libffi_asm.cmake"
        "${vendor}/include/ffi_cfi.h"
        "${vendor}/src/x86/asmnames.h"
        "${vendor}/src/x86/ffitarget.h"
        "${BUILD_DIR}/zr_vm_lib_ffi/generated/libffi/x86_win64/ffi.h"
        "${BUILD_DIR}/zr_vm_lib_ffi/generated/libffi/x86_win64/fficonfig.h"
)
set(missing_inputs)
foreach (input IN LISTS required_inputs)
    if (NOT input IN_LIST actual_inputs)
        list(APPEND missing_inputs "${input}")
    endif ()
endforeach ()
if (missing_inputs)
    list(JOIN missing_inputs "\n  " missing_report)
    message(FATAL_ERROR "Win64 libffi assembly dependency closure is missing:\n  ${missing_report}")
endif ()
message(STATUS "Win64 libffi assembly dependency closure contains all nine required inputs")
