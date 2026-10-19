cmake_minimum_required(VERSION 3.22)
project(zr_vm_audit_review_validation C)

# Compile focused regressions against an existing build of this checkout. This
# avoids copying sources or rebuilding independent modules during review.
enable_testing()
get_filename_component(ZR_VM_AUDIT_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." REALPATH)
set(ZR_VM_AUDIT_REUSE_BUILD "" CACHE PATH "Existing zr_vm build from this checkout")
set(ZR_VM_AUDIT_CASES "identity_protocol_unicode;io_validation" CACHE STRING
    "Semicolon-separated focused regression source stems")
set(ZR_VM_AUDIT_FIXTURE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/fixtures" CACHE PATH
    "Generated inputs for focused tests")
if (NOT EXISTS "${ZR_VM_AUDIT_REUSE_BUILD}/CMakeCache.txt")
    message(FATAL_ERROR "ZR_VM_AUDIT_REUSE_BUILD must name an existing zr_vm build")
endif ()
load_cache("${ZR_VM_AUDIT_REUSE_BUILD}" READ_WITH_PREFIX _zr_audit_cached_
    CMAKE_HOME_DIRECTORY)
get_filename_component(_zr_audit_cached_source
    "${_zr_audit_cached_CMAKE_HOME_DIRECTORY}" REALPATH)
if (NOT _zr_audit_cached_source STREQUAL ZR_VM_AUDIT_SOURCE_ROOT)
    message(FATAL_ERROR "The reused libraries must come from the actual checkout")
endif ()
file(MAKE_DIRECTORY "${ZR_VM_AUDIT_FIXTURE_ROOT}")

foreach (_module IN ITEMS zr_vm_core zr_vm_parser zr_vm_library zr_unity)
    find_library(_zr_audit_${_module}_library NAMES ${_module}
        PATHS "${ZR_VM_AUDIT_REUSE_BUILD}/lib"
              "${ZR_VM_AUDIT_REUSE_BUILD}/lib/Debug" NO_DEFAULT_PATH REQUIRED)
    add_library(audit_${_module} UNKNOWN IMPORTED)
    set_target_properties(audit_${_module} PROPERTIES
        IMPORTED_LOCATION "${_zr_audit_${_module}_library}")
endforeach ()

foreach (_case IN LISTS ZR_VM_AUDIT_CASES)
    if (NOT _case MATCHES "^[a-z][a-z0-9_]*$")
        message(FATAL_ERROR "Invalid focused regression stem: ${_case}")
    endif ()
    set(_source "${ZR_VM_AUDIT_SOURCE_ROOT}/tests/core/test_${_case}.c")
    if (NOT EXISTS "${_source}")
        set(_source "${ZR_VM_AUDIT_SOURCE_ROOT}/tests/language_server/test_${_case}.c")
    endif ()
    if (NOT EXISTS "${_source}")
        set(_source "${ZR_VM_AUDIT_SOURCE_ROOT}/tests/debug/test_${_case}.c")
    endif ()
    if (NOT EXISTS "${_source}")
        message(FATAL_ERROR "No focused regression source exists for ${_case}")
    endif ()
    add_executable(audit_${_case} "${_source}"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/tests/harness/runtime_support.c"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/tests/harness/path_support.c"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/tests/harness/unity_crash_guard.c"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/tests/harness/reference_support.c")
    target_include_directories(audit_${_case} PRIVATE
        "${ZR_VM_AUDIT_SOURCE_ROOT}"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/tests"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/tests/harness"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/tests/third_party/zr_unity"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/tests/third_party/zr_unity/Unity/src"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/zr_vm_common/include"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/zr_vm_core/include"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/zr_vm_core/src"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/zr_vm_parser/include"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/zr_vm_parser/src"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/zr_vm_library/include"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/zr_vm_lib_network/include"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/zr_vm_language_server/include"
        "${ZR_VM_AUDIT_SOURCE_ROOT}/zr_vm_language_server/src")
    target_compile_features(audit_${_case} PRIVATE c_std_11)
    target_compile_definitions(audit_${_case} PRIVATE
        UNITY_INCLUDE_CONFIG_H UNITY_INCLUDE_DOUBLE UNITY_DOUBLE_PRECISION=1e-6
        UNITY_USE_FLUSH_STDOUT ZR_DEBUG zr_vm_core zr_vm_parser
        "ZR_VM_TESTS_SOURCE_DIR=\"${ZR_VM_AUDIT_SOURCE_ROOT}/tests\""
        "ZR_VM_TESTS_BINARY_DIR=\"${ZR_VM_AUDIT_FIXTURE_ROOT}\"")
    if (MSVC)
        set_property(TARGET audit_${_case} PROPERTY MSVC_RUNTIME_LIBRARY
            "MultiThreadedDebugDLL")
        target_compile_options(audit_${_case} PRIVATE /utf-8 /experimental:c11atomics)
        target_compile_definitions(audit_${_case} PRIVATE
            ZR_PLATFORM_WIN ZR_PLATFORM_WIN_USE_MSVC _CRT_SECURE_NO_WARNINGS)
    endif ()
    target_link_libraries(audit_${_case} PRIVATE audit_zr_vm_parser audit_zr_vm_core
        audit_zr_vm_library audit_zr_unity)
    if (_case MATCHES "^lsp_")
        find_library(_zr_audit_lsp_library NAMES zr_vm_language_server
            PATHS "${ZR_VM_AUDIT_REUSE_BUILD}/lib"
                  "${ZR_VM_AUDIT_REUSE_BUILD}/lib/Debug" NO_DEFAULT_PATH REQUIRED)
        target_link_libraries(audit_${_case} PRIVATE "${_zr_audit_lsp_library}")
    endif ()
    if (_case MATCHES "^debug_token")
        find_library(_zr_audit_network_library NAMES zr_vm_lib_network
            PATHS "${ZR_VM_AUDIT_REUSE_BUILD}/lib"
                  "${ZR_VM_AUDIT_REUSE_BUILD}/lib/Debug" NO_DEFAULT_PATH REQUIRED)
        target_link_libraries(audit_${_case} PRIVATE "${_zr_audit_network_library}")
    endif ()
    add_test(NAME audit_${_case} COMMAND audit_${_case})
    set_tests_properties(audit_${_case} PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${ZR_VM_AUDIT_REUSE_BUILD}/bin"
        TIMEOUT 60 WORKING_DIRECTORY "${ZR_VM_AUDIT_FIXTURE_ROOT}")
endforeach ()
