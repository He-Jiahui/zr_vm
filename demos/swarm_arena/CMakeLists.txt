# These tests are registered after the engine's inventory under the "demo"
# label, and have no effect when ZR_VM_BUILD_DEMOS is OFF.
add_library(zrgame_runtime SHARED native/runtime/clock.c native/runtime/sprite_batch.c
    native/runtime/metrics_sink.c native/runtime/backend_null.c)
target_include_directories(zrgame_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/runtime")
set_target_properties(zrgame_runtime PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/native")

# The same descriptors serve zr_swarm's static registration and CLI plugins.
add_library(zrgame_modules STATIC native/modules/mod_host.c native/modules/mod_gfx.c)
target_include_directories(zrgame_modules PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/modules"
    "${PROJECT_SOURCE_DIR}/zr_vm_core/include" "${PROJECT_SOURCE_DIR}/zr_vm_common/include")
target_link_libraries(zrgame_modules PUBLIC zrgame_runtime zr_vm_library_shared)
foreach(module host gfx)
    add_library(zrgame_${module}_plugin MODULE native/modules/plugin_${module}.c)
    target_link_libraries(zrgame_${module}_plugin PRIVATE zrgame_modules)
    set_target_properties(zrgame_${module}_plugin PROPERTIES PREFIX ""
        OUTPUT_NAME "zrvm_native_zrgame_${module}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/native")
endforeach()
add_custom_target(zrgame_plugins DEPENDS zrgame_host_plugin zrgame_gfx_plugin)

add_library(zrgame_ffi SHARED native/ffi/zrgame_ffi.c)
set_target_properties(zrgame_ffi PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/native")
add_library(zrgame_probe MODULE probes/native/probe_module.c)
target_link_libraries(zrgame_probe PRIVATE zr_vm_library_shared)
target_include_directories(zrgame_probe PRIVATE "${PROJECT_SOURCE_DIR}/zr_vm_core/include"
    "${PROJECT_SOURCE_DIR}/zr_vm_common/include")
set_target_properties(zrgame_probe PROPERTIES PREFIX "" OUTPUT_NAME "zrvm_native_zrgame_probe"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/native")
add_library(zrgame_inline_probe MODULE probes/native/probe_module.c)
target_compile_definitions(zrgame_inline_probe PRIVATE SWARM_INLINE_PROBE=1)
target_link_libraries(zrgame_inline_probe PRIVATE zr_vm_library_shared)
target_include_directories(zrgame_inline_probe PRIVATE "${PROJECT_SOURCE_DIR}/zr_vm_core/include"
    "${PROJECT_SOURCE_DIR}/zr_vm_common/include")
set_target_properties(zrgame_inline_probe PROPERTIES PREFIX "" OUTPUT_NAME "zrvm_native_zrgame_inlineprobe"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/native")
add_executable(zr_swarm_probe_host probes/native/host_calls.c)
target_include_directories(zr_swarm_probe_host PRIVATE "${PROJECT_SOURCE_DIR}/zr_vm_common/include"
    "${PROJECT_SOURCE_DIR}/zr_vm_core/include" "${PROJECT_SOURCE_DIR}/zr_vm_parser/include"
    "${PROJECT_SOURCE_DIR}/zr_vm_library/include" "${PROJECT_SOURCE_DIR}/zr_vm_lib_system/include"
    "${PROJECT_SOURCE_DIR}/zr_vm_lib_container/include")
target_link_libraries(zr_swarm_probe_host PRIVATE zr_vm_parser_shared zr_vm_lib_system_shared
    zr_vm_lib_container_shared zr_vm_library_shared zr_vm_core_shared)
set_target_properties(zr_swarm_probe_host PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# DOD uses the core/library path and the same runtime providers as the CLI;
# the independent C reference shares its host loop, artifacts and null batch.
add_executable(zr_swarm host/main.c host/options.c host/host_vm.c host/host_loop.c host/platform_null.c
    native/runtime/csim.c)
if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(native/runtime/csim.c PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
endif()
if (UNIX)
    target_link_libraries(zr_swarm PRIVATE m)
endif()
foreach(component zr_vm_common zr_vm_core zr_vm_library zr_vm_parser zr_vm_lib_math
        zr_vm_lib_system zr_vm_lib_container zr_vm_lib_ffi)
    target_include_directories(zr_swarm PRIVATE "${PROJECT_SOURCE_DIR}/${component}/include")
endforeach()
target_link_libraries(zr_swarm PRIVATE zrgame_modules zr_vm_parser_shared
    zr_vm_lib_math_shared zr_vm_lib_system_shared zr_vm_lib_container_shared
    zr_vm_lib_ffi_shared zr_vm_library_shared zr_vm_core_shared)
if (TARGET zr_vm_lib_network_shared)
    target_link_libraries(zr_swarm PRIVATE zr_vm_lib_network_shared)
    target_include_directories(zr_swarm PRIVATE "${PROJECT_SOURCE_DIR}/zr_vm_lib_network/include")
    target_compile_definitions(zr_swarm PRIVATE ZRG_HAS_NETWORK=1)
endif()
if (TARGET zr_vm_thread_shared)
    target_link_libraries(zr_swarm PRIVATE zr_vm_thread_shared)
    target_include_directories(zr_swarm PRIVATE "${PROJECT_SOURCE_DIR}/zr_vm_lib_thread/include")
    target_compile_definitions(zr_swarm PRIVATE ZRG_HAS_THREAD=1)
endif()
find_package(Git QUIET)
set(swarm_git "unknown")
if (GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" OUTPUT_VARIABLE swarm_git
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND "${GIT_EXECUTABLE}" diff-index --ignore-submodules=all --quiet HEAD --
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" RESULT_VARIABLE swarm_dirty)
    if (swarm_dirty EQUAL 1)
        string(APPEND swarm_git "-dirty")
    endif()
endif()
target_compile_definitions(zr_swarm PRIVATE ZR_SWARM_GIT="${swarm_git}" ZR_SWARM_BUILD="$<CONFIG>")
if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(zr_swarm PRIVATE -Wall -Wextra -Werror)
endif()
set_target_properties(zr_swarm PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if (BUILD_TESTS)
    if (NOT TARGET zr_vm_cli_executable)
        message(FATAL_ERROR "Swarm Arena feature probes require BUILD_CLI=ON")
    endif ()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(zrgame_native_tests native/tests/test_runtime.c)
    target_link_libraries(zrgame_native_tests PRIVATE zrgame_runtime)
    add_test(NAME demo_native_runtime COMMAND zrgame_native_tests)
    set_tests_properties(demo_native_runtime PROPERTIES LABELS "demo;native" TIMEOUT 10)
    foreach(mode interp binary)
        add_test(NAME demo_native_smoke_${mode}
            COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_native_smoke.py"
            --mode ${mode} --cli $<TARGET_FILE:zr_vm_cli_executable>
            --native-dir "${CMAKE_CURRENT_BINARY_DIR}/native")
        set_tests_properties(demo_native_smoke_${mode} PROPERTIES LABELS "demo;native" TIMEOUT 90)
    endforeach()
    foreach(mode interp binary)
        add_test(NAME demo_cli_smoke_${mode}_dod
            COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_game.py"
            --mode ${mode} --cli $<TARGET_FILE:zr_vm_cli_executable>
            --native-dir "${CMAKE_CURRENT_BINARY_DIR}/native")
        set_tests_properties(demo_cli_smoke_${mode}_dod PROPERTIES LABELS "demo;game" TIMEOUT 960)
    endforeach()
    add_test(NAME demo_zr_unit_tests
        COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_zr_unit_tests.py"
        --cli $<TARGET_FILE:zr_vm_cli_executable>
        --native-dir "${CMAKE_CURRENT_BINARY_DIR}/native")
    set_tests_properties(demo_zr_unit_tests PROPERTIES LABELS "demo;game" TIMEOUT 2400)
    add_test(NAME demo_bad_scenario
        COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_game.py"
        --bad --cli $<TARGET_FILE:zr_vm_cli_executable>
        --native-dir "${CMAKE_CURRENT_BINARY_DIR}/native")
    set_tests_properties(demo_bad_scenario PROPERTIES LABELS "demo;game" TIMEOUT 1800)
    foreach(check smoke parity)
        if (check STREQUAL "smoke")
            set(host_test demo_host_smoke_dod)
        else()
            set(host_test demo_cli_host_parity)
        endif()
        add_test(NAME ${host_test}
            COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_host.py"
            --check ${check} --host $<TARGET_FILE:zr_swarm>
            --cli $<TARGET_FILE:zr_vm_cli_executable>
            --native-dir "${CMAKE_CURRENT_BINARY_DIR}/native")
        set_tests_properties(${host_test} PROPERTIES LABELS "demo;host;game" TIMEOUT 1800)
    endforeach()
    # Long scenario completion belongs to Release; stress trees keep short
    # scenario parity alongside full smoke and the gameplay unit suite.
    set(swarm_scenario_frames)
    if (NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        set(swarm_scenario_frames --frames 4)
    endif()
    foreach(scenario arena_2k swarm_10k bullet_storm particle_burst)
        add_test(NAME demo_cli_host_${scenario}_dod
            COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_scenario.py"
            --scenario ${scenario} ${swarm_scenario_frames}
            --host $<TARGET_FILE:zr_swarm> --cli $<TARGET_FILE:zr_vm_cli_executable>
            --native-dir "${CMAKE_CURRENT_BINARY_DIR}/native")
        set_tests_properties(demo_cli_host_${scenario}_dod PROPERTIES
            LABELS "demo;host;game;scenario" TIMEOUT 15000)
    endforeach()
    foreach(check smoke equivalence)
        if (check STREQUAL "smoke")
            set(lane_test demo_host_smoke_c)
        else()
            set(lane_test demo_lane_equivalence)
        endif()
        add_test(NAME ${lane_test}
            COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_lanes.py"
            --check ${check} --host $<TARGET_FILE:zr_swarm>
            --native-dir "${CMAKE_CURRENT_BINARY_DIR}/native")
        set_tests_properties(${lane_test} PROPERTIES LABELS "demo;host;game;lane" TIMEOUT 1800)
    endforeach()
    set(swarm_probe_runner "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_probe.py")
    set(swarm_probe_results "${CMAKE_CURRENT_BINARY_DIR}/probe-results")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/probes/cases.json")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/probes/cases.json" swarm_cases)
    string(JSON swarm_case_count LENGTH "${swarm_cases}")
    math(EXPR swarm_case_last "${swarm_case_count} - 1")
    set(swarm_probe_tests)
    foreach(index RANGE 0 ${swarm_case_last})
        string(JSON case_name GET "${swarm_cases}" ${index} name)
        foreach(mode interp binary)
            string(REPLACE "-" "_" test_suffix "${case_name}")
            set(probe_name "demo_probe_${test_suffix}_${mode}")
            add_test(NAME ${probe_name}
                COMMAND "${Python3_EXECUTABLE}" -I "${swarm_probe_runner}"
                --case "${case_name}" --mode ${mode}
                --cli $<TARGET_FILE:zr_vm_cli_executable>
                --host-exe $<TARGET_FILE:zr_swarm_probe_host>
                --native-dir "${CMAKE_CURRENT_BINARY_DIR}/native"
                --out "${swarm_probe_results}/${case_name}-${mode}")
            set_tests_properties(${probe_name} PROPERTIES LABELS "demo;probe" TIMEOUT 1800)
            string(JSON expected_error ERROR_VARIABLE no_expectation GET "${swarm_cases}" ${index} expect ${mode})
            if (NOT no_expectation)
                # The runner returns 1 only for the exact known failure, without
                # a timeout or sanitizer diagnostic. Its readiness stays FAIL.
                set_tests_properties(${probe_name} PROPERTIES WILL_FAIL TRUE)
            endif ()
            list(APPEND swarm_probe_tests ${probe_name})
        endforeach()
    endforeach()
    add_test(NAME demo_probe_p22
        COMMAND "${Python3_EXECUTABLE}" -I "${swarm_probe_runner}" --parity --out "${swarm_probe_results}")
    set_tests_properties(demo_probe_p22 PROPERTIES LABELS "demo;probe;readiness"
        DEPENDS "${swarm_probe_tests}" TIMEOUT 10)
    add_test(NAME demo_feature_gate
        COMMAND "${Python3_EXECUTABLE}" -I "${swarm_probe_runner}" --gate --out "${swarm_probe_results}")
    set_tests_properties(demo_feature_gate PROPERTIES LABELS "demo;readiness"
        DEPENDS "${swarm_probe_tests};demo_probe_p22" TIMEOUT 10)
endif ()
