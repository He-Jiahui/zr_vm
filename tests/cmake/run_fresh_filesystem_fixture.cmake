# Each executable requires a nonexistent root and leaves its fixture for inspection.
# Keep repeated and concurrent CTest invocations inside the registered parent.
foreach(required TEST_EXECUTABLE TEST_CASE FIXTURE_PARENT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
file(MAKE_DIRECTORY "${FIXTURE_PARENT}")
string(RANDOM LENGTH 24 ALPHABET 0123456789abcdef run_id)
set(fixture_root "${FIXTURE_PARENT}/${run_id}")
execute_process(COMMAND "${TEST_EXECUTABLE}" "${TEST_CASE}" "${fixture_root}"
        RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "${TEST_CASE} failed (${result}); fixture: ${fixture_root}")
endif()
