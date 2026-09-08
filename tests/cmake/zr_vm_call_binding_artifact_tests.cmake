zr_vm_add_unity_test_target(zr_vm_call_binding_artifact_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_call_binding_artifact.c)
target_include_directories(zr_vm_call_binding_artifact_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include)
zr_vm_link_parser_core_plus_library(zr_vm_call_binding_artifact_test)
add_test(NAME call_binding_artifact COMMAND $<TARGET_FILE:zr_vm_call_binding_artifact_test>)

zr_vm_add_unity_test_target(zr_vm_call_binding_aot_projection_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_call_binding_aot_projection.c)
target_include_directories(zr_vm_call_binding_aot_projection_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include)
zr_vm_link_parser_core_plus_library(zr_vm_call_binding_aot_projection_test)
add_test(NAME call_binding_aot_projection COMMAND $<TARGET_FILE:zr_vm_call_binding_aot_projection_test>)
