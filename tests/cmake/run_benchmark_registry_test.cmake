if (NOT DEFINED EXE OR EXE STREQUAL "")
    message(FATAL_ERROR "EXE is required.")
endif ()

include("${CMAKE_CURRENT_LIST_DIR}/zr_vm_test_host_env.cmake")

if (NOT EXISTS "${EXE}")
    message(FATAL_ERROR "Benchmark registry executable not found: ${EXE}. Build target zr_vm_benchmark_registry_test.")
endif ()

if (DEFINED HOST_BINARY_DIR AND NOT HOST_BINARY_DIR STREQUAL "")
    execute_process(
            COMMAND "${EXE}"
            WORKING_DIRECTORY "${HOST_BINARY_DIR}"
            RESULT_VARIABLE _zr_vm_benchmark_registry_result
            OUTPUT_VARIABLE _zr_vm_benchmark_registry_stdout
            ERROR_VARIABLE _zr_vm_benchmark_registry_stderr
    )
else ()
    execute_process(
            COMMAND "${EXE}"
            RESULT_VARIABLE _zr_vm_benchmark_registry_result
            OUTPUT_VARIABLE _zr_vm_benchmark_registry_stdout
            ERROR_VARIABLE _zr_vm_benchmark_registry_stderr
    )
endif ()

if (NOT _zr_vm_benchmark_registry_stdout STREQUAL "")
    message("${_zr_vm_benchmark_registry_stdout}")
endif ()
if (NOT _zr_vm_benchmark_registry_stderr STREQUAL "")
    message("${_zr_vm_benchmark_registry_stderr}")
endif ()
if (NOT _zr_vm_benchmark_registry_result EQUAL 0)
    message(FATAL_ERROR "benchmark_registry failed with exit code ${_zr_vm_benchmark_registry_result}.")
endif ()
