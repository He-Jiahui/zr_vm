# Close-proxy cleanup is a core runtime contract shared by the VM and AOT.
zr_vm_add_unity_test_target(zr_vm_close_proxy_core_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_close_proxy.c)
target_include_directories(zr_vm_close_proxy_core_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
zr_vm_link_core(zr_vm_close_proxy_core_test)
add_test(NAME close_proxy_core COMMAND $<TARGET_FILE:zr_vm_close_proxy_core_test>)
set_tests_properties(close_proxy_core PROPERTIES LABELS "ssa;core" TIMEOUT 30)
