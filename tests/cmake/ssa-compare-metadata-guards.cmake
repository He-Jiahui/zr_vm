if (NOT TARGET zr_vm_ssa_compare_metadata_guards_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_compare_metadata_guards_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_compare_metadata_guards.c)
    target_include_directories(zr_vm_ssa_compare_metadata_guards_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_compare_metadata_guards_test)
    if (WIN32 AND MSVC)
        # The Debug Core dispatcher frame exceeds the default Windows reserve.
        target_link_options(zr_vm_ssa_compare_metadata_guards_test PRIVATE /STACK:8388608)
    endif ()
    add_test(NAME ssa_compare_metadata_guards
            COMMAND zr_vm_ssa_compare_metadata_guards_test)
    set_tests_properties(ssa_compare_metadata_guards PROPERTIES
            LABELS "ssa;parser;core" TIMEOUT 60)
endif ()
