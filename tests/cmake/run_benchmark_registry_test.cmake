# benchmark_registry 的 CTest 包装器：在构建树的加载环境中运行 C 测试目标。
# EXE 必须是已构建目标；HOST_BINARY_DIR 由顶层注册传入并兼作其工作目录。
if (NOT DEFINED EXE OR EXE STREQUAL "")
    message(FATAL_ERROR "EXE is required.")
endif ()

include("${CMAKE_CURRENT_LIST_DIR}/zr_vm_test_host_env.cmake")

if (NOT EXISTS "${EXE}")
    message(FATAL_ERROR "Benchmark registry executable not found: ${EXE}. Build target zr_vm_benchmark_registry_test.")
endif ()

if (DEFINED HOST_BINARY_DIR AND NOT HOST_BINARY_DIR STREQUAL "")
    # 使用宿主构建目录，使测试加载当前构建的共享库且不依赖启动 ctest 的目录。
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
