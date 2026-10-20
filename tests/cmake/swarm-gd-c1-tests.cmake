if(TARGET zr_vm_cli_executable)
    add_library(zr_vm_swarm_plugin_provider MODULE "${CMAKE_CURRENT_SOURCE_DIR}/library/swarm_plugin_provider.c")
    target_link_libraries(zr_vm_swarm_plugin_provider PRIVATE zr_vm_library_shared)
    target_include_directories(zr_vm_swarm_plugin_provider PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include" "${CMAKE_SOURCE_DIR}/zr_vm_common/include")
    set_target_properties(zr_vm_swarm_plugin_provider PROPERTIES
        PREFIX "" OUTPUT_NAME "zrvm_native_zr_swarmprobe"
        WINDOWS_EXPORT_ALL_SYMBOLS ON)
    foreach(case IN ITEMS destructor_stack_growth union_call_storage callable_field class_field_initializers out_ref_arguments pool_view_out struct_operators user_iterable f32_cast)
        foreach(mode IN ITEMS interp binary)
            add_test(NAME swarm_${case}_${mode}
                COMMAND ${CMAKE_COMMAND}
                "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
                "-DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser/swarm_${case}.zr"
                "-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/swarm_${case}_${mode}"
                "-DEXECUTION_MODE=${mode}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_swarm_regression.cmake")
            set_tests_properties(swarm_${case}_${mode} PROPERTIES TIMEOUT 90)
        endforeach()
    endforeach()
    foreach(mode IN ITEMS interp binary)
        add_test(NAME swarm_binary_plugin_binding_${mode}
            COMMAND ${CMAKE_COMMAND}
            "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
            "-DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser/swarm_binary_plugin_binding.zr"
            "-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/swarm_binary_plugin_binding_${mode}"
            "-DPLUGIN_FILE=$<TARGET_FILE:zr_vm_swarm_plugin_provider>"
            "-DPLUGIN_NAME=$<TARGET_FILE_NAME:zr_vm_swarm_plugin_provider>"
            "-DEXECUTION_MODE=${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_swarm_regression.cmake")
        set_tests_properties(swarm_binary_plugin_binding_${mode} PROPERTIES TIMEOUT 90)
    endforeach()
endif()
