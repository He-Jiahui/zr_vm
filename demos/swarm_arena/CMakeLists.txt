# These tests are registered after the engine's test inventory, labelled only
# "demo", and have no effect when ZR_VM_BUILD_DEMOS is OFF.
if (BUILD_TESTS)
    if (NOT TARGET zr_vm_cli_executable)
        message(FATAL_ERROR "Swarm Arena feature probes require BUILD_CLI=ON")
    endif ()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(swarm_probe_runner "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_probe.py")
    foreach(mode interp binary)
        foreach(variant main nested control)
            set(probe_name "demo_probe_p01_${variant}_${mode}")
            set(probe_args
                --cli $<TARGET_FILE:zr_vm_cli_executable>
                --mode ${mode} --variant ${variant}
                --out "${CMAKE_CURRENT_BINARY_DIR}/probe-results/${variant}-${mode}")
            if (variant STREQUAL "main")
                list(APPEND probe_args --expect-failure
                    "GET_MEMBER: receiver must be an object, array, or string")
            elseif (variant STREQUAL "nested")
                list(APPEND probe_args --expect-failure
                    "KNOWN_VM_CALL: invalid known VM callable")
            endif ()
            add_test(NAME ${probe_name}
                COMMAND "${Python3_EXECUTABLE}" -I "${swarm_probe_runner}" ${probe_args})
            set_tests_properties(${probe_name} PROPERTIES LABELS "demo;probe" TIMEOUT 45)
            if (NOT variant STREQUAL "control")
                # Only the exact recorded runtime diagnostic inverts to pass.
                # The separate, non-inverted gate below still blocks GD-C2+.
                set_tests_properties(${probe_name} PROPERTIES WILL_FAIL TRUE)
            endif ()
        endforeach()
    endforeach()
    add_test(NAME demo_feature_gate
        COMMAND "${Python3_EXECUTABLE}" -I "${swarm_probe_runner}" --gate)
    set_tests_properties(demo_feature_gate PROPERTIES LABELS "demo;readiness" TIMEOUT 10)
endif ()
