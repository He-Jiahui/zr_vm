# ZR-VM Scripts Comprehensive Tests CMakeLists.txt

set(SCRIPTS_SOURCE_DIR_NORMALIZED "${CMAKE_CURRENT_SOURCE_DIR}")
set(SCRIPTS_BINARY_DIR_NORMALIZED "${CMAKE_CURRENT_BINARY_DIR}")
file(TO_CMAKE_PATH "${SCRIPTS_SOURCE_DIR_NORMALIZED}" SCRIPTS_SOURCE_DIR_NORMALIZED)
file(TO_CMAKE_PATH "${SCRIPTS_BINARY_DIR_NORMALIZED}" SCRIPTS_BINARY_DIR_NORMALIZED)

set(SCRIPTS_COMMON_SOURCES
    test_utils.c
    ${CMAKE_SOURCE_DIR}/tests/test_support.c
)

function(zr_vm_configure_scripts_target target_name)
    zr_vm_apply_common_test_settings(${target_name})

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )

    if (BUILD_SHARED_LIB)
        target_link_libraries(${target_name} PRIVATE zr_vm_parser_shared zr_vm_core_shared)
    else()
        target_link_libraries(${target_name} PRIVATE zr_vm_parser_static zr_vm_core_static)
    endif()

    zr_link_third_party_for_target(${target_name} "zr_c_json")
    zr_configure_unity_test(${target_name})

    target_compile_definitions(${target_name} PRIVATE
        ZR_VM_SCRIPTS_SOURCE_DIR="${SCRIPTS_SOURCE_DIR_NORMALIZED}"
        ZR_VM_SCRIPTS_BINARY_DIR="${SCRIPTS_BINARY_DIR_NORMALIZED}"
    )
endfunction()

# 创建测试可执行文件
add_executable(zr_vm_scripts_test
    test_comprehensive.c
    ${SCRIPTS_COMMON_SOURCES}
)
zr_vm_configure_scripts_target(zr_vm_scripts_test)

add_executable(zr_vm_artifact_golden_test
    test_artifact_golden.c
    ${SCRIPTS_COMMON_SOURCES}
)
zr_vm_configure_scripts_target(zr_vm_artifact_golden_test)

# 复制测试用例文件到构建目录
file(GLOB TEST_CASE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/test_cases/*.zr")
foreach(TEST_FILE ${TEST_CASE_FILES})
    get_filename_component(TEST_FILE_NAME ${TEST_FILE} NAME)
    # 复制到当前构建目录
    configure_file(
        ${TEST_FILE}
        ${CMAKE_CURRENT_BINARY_DIR}/${TEST_FILE_NAME}
        COPYONLY
    )
    # 复制到 bin 目录
    configure_file(
        ${TEST_FILE}
        ${CMAKE_BINARY_DIR}/bin/${TEST_FILE_NAME}
        COPYONLY
    )
    # 复制到 tests/scripts/test_cases 目录结构
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/scripts/test_cases)
    configure_file(
        ${TEST_FILE}
        ${CMAKE_BINARY_DIR}/tests/scripts/test_cases/${TEST_FILE_NAME}
        COPYONLY
    )
endforeach()

# 创建输出目录
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/scripts/output/ast)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/scripts/output/binary)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/scripts/output/intermediate)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/scripts/output/runtime)

# 添加测试目标
enable_testing()
add_test(NAME scripts_tests COMMAND zr_vm_scripts_test)
add_test(NAME scripts_artifact_golden COMMAND zr_vm_artifact_golden_test)
