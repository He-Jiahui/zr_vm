# Execute the generated shared library and observe its non-local THROW root boundary.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET zr_vm_aot_c_shared_library_smoke_test)
    add_test(NAME ssa_aot_generated_throw_root
            COMMAND zr_vm_aot_c_shared_library_smoke_test --throw-root-only)
    set_tests_properties(ssa_aot_generated_throw_root PROPERTIES
            LABELS "ssa;aot" TIMEOUT 360)
endif ()
