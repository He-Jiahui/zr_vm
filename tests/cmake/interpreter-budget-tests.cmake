if (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static)
    zr_vm_add_unity_test_target(zr_vm_execution_budget_dispatch_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_execution_budget_dispatch.c)
    if (TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static)
        zr_vm_link_parser_core(zr_vm_execution_budget_dispatch_test)
        target_compile_definitions(zr_vm_execution_budget_dispatch_test PRIVATE ZR_BUDGET_SOURCE_TESTS)
    else()
        zr_vm_link_core(zr_vm_execution_budget_dispatch_test)
    endif()
    add_test(NAME execution_budget_dispatch COMMAND zr_vm_execution_budget_dispatch_test)
    set_tests_properties(execution_budget_dispatch PROPERTIES TIMEOUT 30)
endif()

if (TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static)
    zr_vm_add_support_target(zr_vm_budgeted_loop
            ${CMAKE_SOURCE_DIR}/tests/benchmarks/budgeted_loop.c)
    zr_vm_link_parser_core_plus_library(zr_vm_budgeted_loop)
    add_test(NAME budgeted_loop_smoke COMMAND zr_vm_budgeted_loop 2000)
    set_tests_properties(budgeted_loop_smoke PROPERTIES TIMEOUT 30)
    foreach(program calls fib)
        add_test(NAME budgeted_${program}_smoke COMMAND zr_vm_budgeted_loop
            --source ${CMAKE_SOURCE_DIR}/tests/benchmarks/probes/budget/${program}.zr
            --expected 42)
        set_tests_properties(budgeted_${program}_smoke PROPERTIES TIMEOUT 30)
    endforeach()
endif()
