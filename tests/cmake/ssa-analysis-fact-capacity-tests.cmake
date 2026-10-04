get_filename_component(ZR_SSA_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if (NOT TARGET zr_vm_ssa_analysis_fact_capacity_test)
    add_executable(zr_vm_ssa_analysis_fact_capacity_test
            ${ZR_SSA_SOURCE_ROOT}/tests/parser/test_ssa_analysis_fact_capacity.c)
    target_include_directories(zr_vm_ssa_analysis_fact_capacity_test PRIVATE
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/include
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/include
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_analysis_fact_capacity_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_analysis_fact_capacity COMMAND zr_vm_ssa_analysis_fact_capacity_test)
    set_tests_properties(ssa_analysis_fact_capacity PROPERTIES LABELS "ssa")
endif ()
