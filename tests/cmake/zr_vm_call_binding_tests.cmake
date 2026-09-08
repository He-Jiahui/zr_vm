zr_vm_add_unity_test_target(zr_vm_call_binding_runtime_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_call_binding_runtime.c)
target_include_directories(zr_vm_call_binding_runtime_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
zr_vm_link_core(zr_vm_call_binding_runtime_test)
add_test(NAME call_binding_runtime COMMAND $<TARGET_FILE:zr_vm_call_binding_runtime_test>)

zr_vm_add_support_target(zr_vm_call_binding_native_registry_test
        ${CMAKE_SOURCE_DIR}/tests/library/test_call_binding_native_registry.c)
target_include_directories(zr_vm_call_binding_native_registry_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include)
zr_vm_link_parser_core_plus_library(zr_vm_call_binding_native_registry_test)
add_test(NAME call_binding_native_registry COMMAND $<TARGET_FILE:zr_vm_call_binding_native_registry_test>)

foreach(suite IN ITEMS pipeline relocation)
    if(suite STREQUAL "pipeline")
        set(call_binding_test_dir parser)
    else()
        set(call_binding_test_dir library)
    endif()
    zr_vm_add_unity_test_target(zr_vm_call_binding_${suite}_test
            ${CMAKE_SOURCE_DIR}/tests/${call_binding_test_dir}/test_call_binding_${suite}.c)
    target_include_directories(zr_vm_call_binding_${suite}_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include)
    zr_vm_link_parser_core_plus_library(zr_vm_call_binding_${suite}_test)
    add_test(NAME call_binding_${suite} COMMAND $<TARGET_FILE:zr_vm_call_binding_${suite}_test>)
endforeach()

include(${CMAKE_CURRENT_LIST_DIR}/zr_vm_call_binding_artifact_tests.cmake)

zr_vm_add_unity_test_target(zr_vm_typed_call_binding_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_typed_call_binding.c)
target_include_directories(zr_vm_typed_call_binding_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include)
zr_vm_link_parser_core_plus_library(zr_vm_typed_call_binding_test)
add_test(NAME typed_call_binding COMMAND $<TARGET_FILE:zr_vm_typed_call_binding_test>)

zr_vm_add_unity_test_target(zr_vm_call_binding_module_test
        ${CMAKE_SOURCE_DIR}/tests/library/test_call_binding_module.c)
target_include_directories(zr_vm_call_binding_module_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include)
zr_vm_link_parser_core_plus_library(zr_vm_call_binding_module_test)
add_test(NAME call_binding_module COMMAND $<TARGET_FILE:zr_vm_call_binding_module_test>)

zr_vm_add_support_target(zr_vm_call_binding_measurement
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/call_binding_measurement.c)
target_include_directories(zr_vm_call_binding_measurement PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include)
target_compile_definitions(zr_vm_call_binding_measurement PRIVATE
        ZR_VM_TESTS_REPO_ROOT="${CMAKE_SOURCE_DIR}")
zr_vm_link_parser_core_plus_library(zr_vm_call_binding_measurement)
