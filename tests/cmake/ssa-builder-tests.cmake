# Focused SemIR-to-ExecIR builder fixtures. Included from ssa-tests.cmake.

set(_zr_vm_ssa_builder_sources
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_control_edges.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_normalize_cfg.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_place_eligibility.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c)

if (NOT TARGET zr_vm_ssa_builder_cfg_test)
    add_executable(zr_vm_ssa_builder_cfg_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_builder_cfg.c
            ${_zr_vm_ssa_builder_sources})
    target_include_directories(zr_vm_ssa_builder_cfg_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_builder_cfg_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_builder_cfg COMMAND zr_vm_ssa_builder_cfg_test)
    set_tests_properties(ssa_builder_cfg PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_linear_effects_builder_test)
    add_executable(zr_vm_ssa_linear_effects_builder_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_linear_effects_builder.c
            ${_zr_vm_ssa_builder_sources})
    target_include_directories(zr_vm_ssa_linear_effects_builder_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_linear_effects_builder_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_linear_effects_builder
            COMMAND zr_vm_ssa_linear_effects_builder_test)
    set_tests_properties(ssa_linear_effects_builder PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_builder_dominance_test)
    add_executable(zr_vm_ssa_builder_dominance_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_builder_dominance.c
            ${_zr_vm_ssa_builder_sources})
    target_include_directories(zr_vm_ssa_builder_dominance_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_builder_dominance_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_builder_dominance COMMAND zr_vm_ssa_builder_dominance_test)
    set_tests_properties(ssa_builder_dominance PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_builder_control_edges_test)
    add_executable(zr_vm_ssa_builder_control_edges_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_builder_control_edges.c
            ${_zr_vm_ssa_builder_sources})
    target_include_directories(zr_vm_ssa_builder_control_edges_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_builder_control_edges_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_builder_control_edges COMMAND zr_vm_ssa_builder_control_edges_test)
    set_tests_properties(ssa_builder_control_edges PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_builder_cleanup_dispatch_test)
    add_executable(zr_vm_ssa_builder_cleanup_dispatch_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_builder_cleanup_dispatch.c
            ${_zr_vm_ssa_builder_sources})
    target_include_directories(zr_vm_ssa_builder_cleanup_dispatch_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_builder_cleanup_dispatch_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_builder_cleanup_dispatch
            COMMAND zr_vm_ssa_builder_cleanup_dispatch_test)
    set_tests_properties(ssa_builder_cleanup_dispatch PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_cleanup_exception_state_test)
    add_executable(zr_vm_ssa_cleanup_exception_state_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_cleanup_exception_state.c
            ${_zr_vm_ssa_builder_sources})
    target_include_directories(zr_vm_ssa_cleanup_exception_state_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_cleanup_exception_state_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_cleanup_exception_state
            COMMAND zr_vm_ssa_cleanup_exception_state_test)
    set_tests_properties(ssa_cleanup_exception_state PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_builder_fact_identity_test)
    add_executable(zr_vm_ssa_builder_fact_identity_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_builder_fact_identity.c
            ${_zr_vm_ssa_builder_sources})
    target_include_directories(zr_vm_ssa_builder_fact_identity_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_builder_fact_identity_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_builder_fact_identity COMMAND zr_vm_ssa_builder_fact_identity_test)
    set_tests_properties(ssa_builder_fact_identity PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_builder_iterator_invokes_test)
    add_executable(zr_vm_ssa_builder_iterator_invokes_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_builder_iterator_invokes.c
            ${_zr_vm_ssa_builder_sources})
    target_include_directories(zr_vm_ssa_builder_iterator_invokes_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_builder_iterator_invokes_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_builder_iterator_invokes COMMAND zr_vm_ssa_builder_iterator_invokes_test)
    set_tests_properties(ssa_builder_iterator_invokes PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_place_eligibility_test)
    add_executable(zr_vm_ssa_place_eligibility_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_place_eligibility.c
            ${_zr_vm_ssa_builder_sources})
    target_include_directories(zr_vm_ssa_place_eligibility_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_place_eligibility_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_place_eligibility COMMAND zr_vm_ssa_place_eligibility_test)
    set_tests_properties(ssa_place_eligibility PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_place_promotion_test)
    add_executable(zr_vm_ssa_place_promotion_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_place_promotion.c
            ${_zr_vm_ssa_builder_sources})
    target_include_directories(zr_vm_ssa_place_promotion_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_place_promotion_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_place_promotion COMMAND zr_vm_ssa_place_promotion_test)
    set_tests_properties(ssa_place_promotion PROPERTIES LABELS "ssa")
endif ()
