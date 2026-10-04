if (NOT TARGET zr_vm_ssa_aot_scalar_conditional_test)
    add_executable(zr_vm_ssa_aot_scalar_conditional_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_aot_scalar_conditional.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/aot_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_arithmetic.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_conditional.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c)
    target_include_directories(zr_vm_ssa_aot_scalar_conditional_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot)
    target_compile_definitions(zr_vm_ssa_aot_scalar_conditional_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    zr_vm_apply_common_test_settings(zr_vm_ssa_aot_scalar_conditional_test)
    add_test(NAME ssa_aot_scalar_conditional COMMAND zr_vm_ssa_aot_scalar_conditional_test)
    set_tests_properties(ssa_aot_scalar_conditional PROPERTIES LABELS "ssa")
endif ()

# The public scalar emitters reference both dedicated helpers. Complete the
# already-defined scalar-text target without editing its central registration.
if (TARGET zr_vm_ssa_aot_scalar_text_test)
    get_target_property(zr_conditional_existing_sources zr_vm_ssa_aot_scalar_text_test SOURCES)
    get_target_property(zr_conditional_source_directory zr_vm_ssa_aot_scalar_text_test SOURCE_DIR)
    set(zr_conditional_normalized_sources)
    foreach (zr_conditional_existing_source IN LISTS zr_conditional_existing_sources)
        get_filename_component(zr_conditional_absolute_source "${zr_conditional_existing_source}"
                ABSOLUTE BASE_DIR "${zr_conditional_source_directory}")
        list(APPEND zr_conditional_normalized_sources "${zr_conditional_absolute_source}")
    endforeach ()
    foreach (zr_conditional_helper IN ITEMS scalar_arithmetic scalar_conditional)
        set(zr_conditional_helper_source
                "${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_${zr_conditional_helper}.c")
        list(FIND zr_conditional_normalized_sources "${zr_conditional_helper_source}" zr_conditional_helper_index)
        if (zr_conditional_helper_index EQUAL -1)
            target_sources(zr_vm_ssa_aot_scalar_text_test PRIVATE "${zr_conditional_helper_source}")
            list(APPEND zr_conditional_normalized_sources "${zr_conditional_helper_source}")
        endif ()
    endforeach ()
    unset(zr_conditional_existing_sources)
    unset(zr_conditional_source_directory)
    unset(zr_conditional_normalized_sources)
    unset(zr_conditional_existing_source)
    unset(zr_conditional_absolute_source)
    unset(zr_conditional_helper)
    unset(zr_conditional_helper_source)
    unset(zr_conditional_helper_index)
endif ()
