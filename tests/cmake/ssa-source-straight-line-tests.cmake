# Source-owned straight-line finalization; no ExecBC CFG reconstruction.
if (NOT TARGET zr_vm_ssa_source_straight_line_cfg_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_source_straight_line_cfg_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_source_straight_line_cfg.c)
    target_include_directories(zr_vm_ssa_source_straight_line_cfg_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_source_straight_line_cfg_test)
    add_test(NAME ssa_source_straight_line_cfg
            COMMAND zr_vm_ssa_source_straight_line_cfg_test)
    set_tests_properties(ssa_source_straight_line_cfg PROPERTIES LABELS "ssa")
endif ()
