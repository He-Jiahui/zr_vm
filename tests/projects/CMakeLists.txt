set(HELLO_WORLD_PROJECT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/hello_world/hello_world.zrp)
set(IMPORT_BASIC_PROJECT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/import_basic/import_basic.zrp)
set(CLASSES_PROJECT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/classes/classes.zrp)

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

set(IMPORT_BINARY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/import_binary)
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
