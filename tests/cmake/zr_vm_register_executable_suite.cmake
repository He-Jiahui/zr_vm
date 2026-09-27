include_guard(GLOBAL)

# 顶层 tests/CMake 为 language_pipeline 建立一个跨配置的 CTest 入口。
# 可执行文件路径含生成表达式，必须在 generate 阶段写 manifest，供 cmake -P runner 消费。
function(zr_vm_add_manifest_executable_suite)
    cmake_parse_arguments(ZR_SUITE ""
            "NAME;RUNNER_SCRIPT;HOST_BINARY_DIR;RUN_WORKING_DIRECTORY"
            "EXECUTABLES;EXECUTABLES_SMOKE;EXECUTABLES_CORE;EXECUTABLES_STRESS"
            ${ARGN})

    foreach (required_argument IN ITEMS NAME RUNNER_SCRIPT HOST_BINARY_DIR RUN_WORKING_DIRECTORY EXECUTABLES)
        if (NOT DEFINED ZR_SUITE_${required_argument} OR ZR_SUITE_${required_argument} STREQUAL "")
            message(FATAL_ERROR "zr_vm_add_manifest_executable_suite requires ${required_argument}.")
        endif ()
    endforeach ()

    # manifest 由构建树持有；每个配置独立展开，避免多配置生成器读取错误的可执行路径。
    set(manifest_directory "${CMAKE_CURRENT_BINARY_DIR}/suite_manifests")
    file(MAKE_DIRECTORY "${manifest_directory}")
    set(manifest_path "${manifest_directory}/${ZR_SUITE_NAME}-$<CONFIG>.cmake")
    string(CONCAT manifest_content
            "set(SUITE_NAME [==[${ZR_SUITE_NAME}]==])\n"
            "set(EXECUTABLES [==[${ZR_SUITE_EXECUTABLES}]==])\n"
            "set(EXECUTABLES_SMOKE [==[${ZR_SUITE_EXECUTABLES_SMOKE}]==])\n"
            "set(EXECUTABLES_CORE [==[${ZR_SUITE_EXECUTABLES_CORE}]==])\n"
            "set(EXECUTABLES_STRESS [==[${ZR_SUITE_EXECUTABLES_STRESS}]==])\n")
    file(GENERATE OUTPUT "${manifest_path}" CONTENT "${manifest_content}")

    add_test(
            NAME ${ZR_SUITE_NAME}
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_MANIFEST=${manifest_path}"
            "-DHOST_BINARY_DIR=${ZR_SUITE_HOST_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${ZR_SUITE_RUN_WORKING_DIRECTORY}"
            -P "${ZR_SUITE_RUNNER_SCRIPT}"
    )
endfunction()
