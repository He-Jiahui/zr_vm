# CTest 入口将正例与会 FATAL_ERROR 的反例分进子进程，并核验反例的诊断契约。
# REGISTRY_FILE 和 FIXTURE_SCRIPT 由 tests/CMakeLists.txt 指向源码树中的配对文件。
foreach (required_variable IN ITEMS REGISTRY_FILE FIXTURE_SCRIPT)
    if (NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif ()
endforeach ()

execute_process(
        COMMAND "${CMAKE_COMMAND}"
                "-DREGISTRY_FILE=${REGISTRY_FILE}"
                -DMODE=valid
                -P "${FIXTURE_SCRIPT}"
        RESULT_VARIABLE valid_result
        OUTPUT_VARIABLE valid_stdout
        ERROR_VARIABLE valid_stderr)
if (NOT valid_result EQUAL 0)
    message(FATAL_ERROR "valid benchmark registry contract failed:\n${valid_stdout}${valid_stderr}")
endif ()

foreach (invalid_mode IN ITEMS zero negative non_integer)
    # 退出非零本身不足以证明 MIN_SAMPLE_MS 校验生效，还需匹配注册接口的错误信息。
    execute_process(
            COMMAND "${CMAKE_COMMAND}"
                    "-DREGISTRY_FILE=${REGISTRY_FILE}"
                    "-DMODE=${invalid_mode}"
                    -P "${FIXTURE_SCRIPT}"
            RESULT_VARIABLE invalid_result
            OUTPUT_VARIABLE invalid_stdout
            ERROR_VARIABLE invalid_stderr)
    if (invalid_result EQUAL 0)
        message(FATAL_ERROR "${invalid_mode} MIN_SAMPLE_MS unexpectedly succeeded")
    endif ()
    if (NOT "${invalid_stdout}${invalid_stderr}" MATCHES
            "MIN_SAMPLE_MS to be a positive integer")
        message(FATAL_ERROR
                "${invalid_mode} MIN_SAMPLE_MS failed without the contract diagnostic:\n${invalid_stdout}${invalid_stderr}")
    endif ()
endforeach ()

message(STATUS "Benchmark registry MIN_SAMPLE_MS contract PASS")
