# Canonical semantic value facts and their real compiler/builder boundary.
foreach(_facts_suite IN ITEMS semantic_value_facts ssa_source_value_facts)
    if (NOT TARGET zr_vm_${_facts_suite}_test)
        zr_vm_add_unity_test_target(zr_vm_${_facts_suite}_test
                ${CMAKE_SOURCE_DIR}/tests/parser/test_${_facts_suite}.c)
        target_include_directories(zr_vm_${_facts_suite}_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
        zr_vm_link_parser_core_plus_library(zr_vm_${_facts_suite}_test)
        add_test(NAME ${_facts_suite} COMMAND zr_vm_${_facts_suite}_test)
        set_tests_properties(${_facts_suite} PROPERTIES LABELS "ssa")
    endif ()
endforeach()
unset(_facts_suite)
