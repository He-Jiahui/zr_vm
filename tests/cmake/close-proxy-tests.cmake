# Close-proxy cleanup is a core runtime contract shared by the VM and AOT.
zr_vm_add_unity_test_target(zr_vm_close_proxy_core_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_close_proxy.c)
target_include_directories(zr_vm_close_proxy_core_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
zr_vm_link_core(zr_vm_close_proxy_core_test)
add_test(NAME close_proxy_core COMMAND $<TARGET_FILE:zr_vm_close_proxy_core_test>)
set_tests_properties(close_proxy_core PROPERTIES LABELS "ssa;core" TIMEOUT 30)

zr_vm_add_unity_test_target(zr_vm_close_proxy_instruction_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_close_proxy_instruction.c)
target_include_directories(zr_vm_close_proxy_instruction_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
zr_vm_link_core(zr_vm_close_proxy_instruction_test)
add_test(NAME close_proxy_instruction COMMAND $<TARGET_FILE:zr_vm_close_proxy_instruction_test>)
set_tests_properties(close_proxy_instruction PROPERTIES LABELS "ssa;core" TIMEOUT 30)

zr_vm_add_unity_test_target(zr_vm_close_proxy_aot_runtime_test
        ${CMAKE_SOURCE_DIR}/tests/library/test_close_proxy_aot_runtime.c)
target_include_directories(zr_vm_close_proxy_aot_runtime_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include)
zr_vm_link_parser_core_plus_library(zr_vm_close_proxy_aot_runtime_test)
add_test(NAME close_proxy_aot_runtime COMMAND $<TARGET_FILE:zr_vm_close_proxy_aot_runtime_test>)
set_tests_properties(close_proxy_aot_runtime PROPERTIES LABELS "ssa;aot" TIMEOUT 30)

zr_vm_add_unity_test_target(zr_vm_close_proxy_legacy_patch_test
        ${CMAKE_SOURCE_DIR}/tests/module/test_close_proxy_legacy_patch.c)
target_include_directories(zr_vm_close_proxy_legacy_patch_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include)
zr_vm_link_parser_core(zr_vm_close_proxy_legacy_patch_test)
add_test(NAME close_proxy_legacy_patch COMMAND $<TARGET_FILE:zr_vm_close_proxy_legacy_patch_test>)
set_tests_properties(close_proxy_legacy_patch PROPERTIES LABELS "ssa;artifact" TIMEOUT 30)
