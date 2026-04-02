set(ZR_VM_PROJECT_FIXTURES_DIR ${CMAKE_SOURCE_DIR}/tests/fixtures/projects)

set(HELLO_WORLD_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/hello_world/hello_world.zrp)
set(IMPORT_BASIC_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/import_basic/import_basic.zrp)
set(CLASSES_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/classes/classes.zrp)
set(CLASSES_FULL_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/classes_full/classes_full.zrp)
set(CLASSES_STATIC_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/classes_static/classes_static.zrp)
set(CLASSES_SUPER_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/classes_super/classes_super.zrp)
set(CLASSES_PROPERTIES_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/classes_properties/classes_properties.zrp)
set(IMPORT_PUB_FUNCTION_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/import_pub_function/import_pub_function.zrp)
set(IMPORT_CAPTURE_NATIVE_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/import_capture_native/import_capture_native.zrp)
set(NATIVE_CHAIN_CALLS_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/native_chain_calls/native_chain_calls.zrp)
set(NATIVE_VECTOR3_EXPORT_PROBE_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/native_vector3_export_probe/native_vector3_export_probe.zrp)
set(NATIVE_VECTOR3_CAPTURE_PROBE_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/native_vector3_capture_probe/native_vector3_capture_probe.zrp)
set(NATIVE_NUMERIC_PIPELINE_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/native_numeric_pipeline/native_numeric_pipeline.zrp)
set(CONTAINER_MATRIX_PROJECT_FILE ${ZR_VM_PROJECT_FIXTURES_DIR}/container_matrix/container_matrix.zrp)

add_executable(
    zr_vm_project_fixture_builder
    ${CMAKE_CURRENT_SOURCE_DIR}/project_fixture_builder.c
)

if (BUILD_SHARED_LIB)
    target_link_libraries(zr_vm_project_fixture_builder PRIVATE zr_vm_parser_shared zr_vm_core_shared)
else()
    target_link_libraries(zr_vm_project_fixture_builder PRIVATE zr_vm_parser_static zr_vm_core_static)
endif()

target_include_directories(zr_vm_project_fixture_builder PRIVATE
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

set(IMPORT_BINARY_SOURCE_DIR ${ZR_VM_PROJECT_FIXTURES_DIR}/import_binary)
set(IMPORT_BINARY_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/import_binary)
file(MAKE_DIRECTORY ${IMPORT_BINARY_BUILD_DIR})
file(MAKE_DIRECTORY ${IMPORT_BINARY_BUILD_DIR}/src)
file(MAKE_DIRECTORY ${IMPORT_BINARY_BUILD_DIR}/bin)
file(MAKE_DIRECTORY ${IMPORT_BINARY_BUILD_DIR}/fixtures)

configure_file(
    ${IMPORT_BINARY_SOURCE_DIR}/import_binary.zrp
    ${IMPORT_BINARY_BUILD_DIR}/import_binary.zrp
    COPYONLY
)
configure_file(
    ${IMPORT_BINARY_SOURCE_DIR}/src/main.zr
    ${IMPORT_BINARY_BUILD_DIR}/src/main.zr
    COPYONLY
)
configure_file(
    ${IMPORT_BINARY_SOURCE_DIR}/fixtures/greet_binary_source.zr
    ${IMPORT_BINARY_BUILD_DIR}/fixtures/greet_binary_source.zr
    COPYONLY
)

set(IMPORT_BINARY_PROJECT_FILE ${IMPORT_BINARY_BUILD_DIR}/import_binary.zrp)
set(IMPORT_BINARY_FIXTURE_SOURCE_FILE ${IMPORT_BINARY_BUILD_DIR}/fixtures/greet_binary_source.zr)
set(IMPORT_BINARY_FIXTURE_OUTPUT_FILE ${IMPORT_BINARY_BUILD_DIR}/bin/greet.zro)

add_test(
    NAME zr_project_hello_world
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${HELLO_WORLD_PROJECT_FILE}
)

set_tests_properties(zr_project_hello_world PROPERTIES
    PASS_REGULAR_EXPRESSION "hello world"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result"
)

add_test(
    NAME zr_project_import_basic
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${IMPORT_BASIC_PROJECT_FILE}
)

set_tests_properties(zr_project_import_basic PROPERTIES
    PASS_REGULAR_EXPRESSION "hello from import"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error"
)

add_test(
    NAME zr_project_classes
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${CLASSES_PROJECT_FILE}
)

set_tests_properties(zr_project_classes PROPERTIES
    PASS_REGULAR_EXPRESSION "61"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error"
)

add_test(
    NAME zr_project_classes_full
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${CLASSES_FULL_PROJECT_FILE}
)

set_tests_properties(zr_project_classes_full PROPERTIES
    PASS_REGULAR_EXPRESSION "127"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error"
)

add_test(
    NAME zr_project_classes_static
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${CLASSES_STATIC_PROJECT_FILE}
)

set_tests_properties(zr_project_classes_static PROPERTIES
    PASS_REGULAR_EXPRESSION "82"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error"
)

add_test(
    NAME zr_project_classes_super
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${CLASSES_SUPER_PROJECT_FILE}
)

set_tests_properties(zr_project_classes_super PROPERTIES
    PASS_REGULAR_EXPRESSION "42"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error"
)

add_test(
    NAME zr_project_classes_properties
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${CLASSES_PROPERTIES_PROJECT_FILE}
)

set_tests_properties(zr_project_classes_properties PROPERTIES
    PASS_REGULAR_EXPRESSION "40"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error"
)

add_test(
    NAME zr_project_import_binary_prepare
    COMMAND
        $<TARGET_FILE:zr_vm_project_fixture_builder>
        ${IMPORT_BINARY_FIXTURE_SOURCE_FILE}
        ${IMPORT_BINARY_FIXTURE_OUTPUT_FILE}
)

set_tests_properties(zr_project_import_binary_prepare PROPERTIES
    FIXTURES_SETUP zr_project_import_binary_artifacts
    PASS_REGULAR_EXPRESSION "generated"
)

add_test(
    NAME zr_project_import_binary
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${IMPORT_BINARY_PROJECT_FILE}
)

set_tests_properties(zr_project_import_binary PROPERTIES
    FIXTURES_REQUIRED zr_project_import_binary_artifacts
    PASS_REGULAR_EXPRESSION "hello from import"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error"
)

add_test(
    NAME zr_project_import_pub_function
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${IMPORT_PUB_FUNCTION_PROJECT_FILE}
)

set_tests_properties(zr_project_import_pub_function PROPERTIES
    PASS_REGULAR_EXPRESSION "7123"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error|null"
)

add_test(
    NAME zr_project_import_capture_native
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${IMPORT_CAPTURE_NATIVE_PROJECT_FILE}
)

set_tests_properties(zr_project_import_capture_native PROPERTIES
    PASS_REGULAR_EXPRESSION "7005"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error|null|Function value is NULL"
)

add_test(
    NAME zr_project_native_chain_calls
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${NATIVE_CHAIN_CALLS_PROJECT_FILE}
)

set_tests_properties(zr_project_native_chain_calls PROPERTIES
    PASS_REGULAR_EXPRESSION "5008"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error|null"
)

add_test(
    NAME zr_project_native_vector3_export_probe
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${NATIVE_VECTOR3_EXPORT_PROBE_PROJECT_FILE}
)

set_tests_properties(zr_project_native_vector3_export_probe PROPERTIES
    PASS_REGULAR_EXPRESSION "1"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error|null|Function value is NULL"
)

add_test(
    NAME zr_project_native_vector3_capture_probe
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${NATIVE_VECTOR3_CAPTURE_PROBE_PROJECT_FILE}
)

set_tests_properties(zr_project_native_vector3_capture_probe PROPERTIES
    PASS_REGULAR_EXPRESSION "6"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error|null|Function value is NULL"
)

add_test(
    NAME zr_project_native_numeric_pipeline
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${NATIVE_NUMERIC_PIPELINE_PROJECT_FILE}
)

set_tests_properties(zr_project_native_numeric_pipeline PROPERTIES
    PASS_REGULAR_EXPRESSION "NATIVE_NUMERIC_PIPELINE_PASS"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error|\\[closure\\]"
)

add_test(
    NAME zr_project_native_numeric_pipeline_result
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${NATIVE_NUMERIC_PIPELINE_PROJECT_FILE}
)

set_tests_properties(zr_project_native_numeric_pipeline_result PROPERTIES
    PASS_REGULAR_EXPRESSION "214"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error|\\[closure\\]|null"
)

add_test(
    NAME zr_project_container_matrix
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${CONTAINER_MATRIX_PROJECT_FILE}
)

set_tests_properties(zr_project_container_matrix PROPERTIES
    PASS_REGULAR_EXPRESSION "CONTAINER_MATRIX_PASS"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error|null|Function value is NULL"
)

add_test(
    NAME zr_project_container_matrix_result
    COMMAND $<TARGET_FILE:zr_vm_cli_executable> ${CONTAINER_MATRIX_PROJECT_FILE}
)

set_tests_properties(zr_project_container_matrix_result PROPERTIES
    PASS_REGULAR_EXPRESSION "635"
    FAIL_REGULAR_EXPRESSION "failed to load project|project execution failed|failed to stringify project result|Compiler Error|Run Error|null|Function value is NULL"
)
