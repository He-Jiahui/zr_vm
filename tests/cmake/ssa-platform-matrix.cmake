# Portable capability matrix helpers for SSA platform tests.
#
# This module deliberately separates a target's *policy* from build/runtime
# evidence.  A profile describes what a target may expose (for example,
# machine-code JIT is forbidden on mobile and WASM); a test is registered only
# when its executable/command and required capabilities are present.  Missing
# tooling is represented by a CTest skip marker, never by a passing test.
#
# The file is usable in two modes:
#
#   include(tests/cmake/ssa-platform-matrix.cmake)
#
#     Includes the frozen profile table and helper functions for a project's
#     CMake configure step.
#
#   cmake -P tests/cmake/ssa-platform-matrix.cmake
#
#     Runs a deterministic declaration self-check without probing the host or
#     claiming that an unavailable platform was executed.

if (DEFINED ZR_VM_SSA_PLATFORM_MATRIX_INCLUDED)
    return()
endif ()
set(ZR_VM_SSA_PLATFORM_MATRIX_INCLUDED TRUE)

set(ZR_VM_SSA_PLATFORM_MATRIX_SCHEMA_VERSION 1)

# These names intentionally match the platform contract's target classes and
# architecture vocabulary.  Keep the list stable: acceptance manifests use it
# as the denominator for platform rows.
set(ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_IDS
        desktop_windows_x86_64
        desktop_windows_aarch64
        desktop_linux_x86_64
        desktop_linux_aarch64
        desktop_darwin_x86_64
        desktop_darwin_aarch64
        android_aarch64
        ios_aarch64
        wasm32
        wasm64)

set(ZR_VM_SSA_PLATFORM_MATRIX_BACKENDS
        execbc
        aot_c
        aot_llvm
        host_jit)
set(ZR_VM_SSA_PLATFORM_MATRIX_FEATURES
        machine_code_jit
        threads
        concurrent_gc
        pmu
        unwind
        debug
        native_callbacks
        restricted_patch)
set(ZR_VM_SSA_PLATFORM_MATRIX_RUNNERS
        host
        cross_compile
        emulator
        real_device
        browser_runtime
        wasm_runtime)
set(ZR_VM_SSA_PLATFORM_MATRIX_TARGET_CLASSES
        desktop_windows
        desktop_linux
        desktop_darwin
        android
        ios
        wasm)
set(ZR_VM_SSA_PLATFORM_MATRIX_ARCHITECTURES
        x86_64
        aarch64
        wasm32
        wasm64)

# Define one profile.  The values are policy declarations, not observations:
#
#   ALLOWED_BACKENDS      backends that may be selected for this target
#   DEFAULT_FEATURES      capabilities enabled by the conservative profile
#   OPTIONAL_FEATURES     capabilities a runner may opt into explicitly
#   FORBIDDEN_FEATURES    capabilities that must always be rejected
#
# In particular, an empty DEFAULT_FEATURES value for a WASM threading feature
# does not mean that a future threaded runner is impossible; it means that a
# single-thread WASM build must not silently claim worker coverage.
macro(_zr_vm_ssa_platform_matrix_define_profile
        profile_id target_class architecture target_triple pointer_width
        allowed_backends default_features optional_features forbidden_features
        runner machine_code_jit_allowed)
    list(APPEND ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_IDS_INTERNAL "${profile_id}")
    set(ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${profile_id}_TARGET_CLASS
            "${target_class}")
    set(ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${profile_id}_ARCHITECTURE
            "${architecture}")
    set(ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${profile_id}_TARGET_TRIPLE
            "${target_triple}")
    set(ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${profile_id}_POINTER_WIDTH_BITS
            "${pointer_width}")
    set(ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${profile_id}_ALLOWED_BACKENDS
            "${allowed_backends}")
    set(ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${profile_id}_DEFAULT_FEATURES
            "${default_features}")
    set(ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${profile_id}_OPTIONAL_FEATURES
            "${optional_features}")
    set(ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${profile_id}_FORBIDDEN_FEATURES
            "${forbidden_features}")
    set(ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${profile_id}_RUNNER
            "${runner}")
    set(ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${profile_id}_MACHINE_CODE_JIT_ALLOWED
            "${machine_code_jit_allowed}")
endmacro()

_zr_vm_ssa_platform_matrix_define_profile(
        desktop_windows_x86_64 desktop_windows x86_64
        "x86_64-pc-windows-msvc" 64
        "execbc;aot_c;aot_llvm;host_jit"
        "unwind;debug;native_callbacks"
        "threads;concurrent_gc;pmu"
        ""
        host TRUE)
_zr_vm_ssa_platform_matrix_define_profile(
        desktop_windows_aarch64 desktop_windows aarch64
        "aarch64-pc-windows-msvc" 64
        "execbc;aot_c;aot_llvm;host_jit"
        "unwind;debug;native_callbacks"
        "threads;concurrent_gc;pmu"
        ""
        host TRUE)
_zr_vm_ssa_platform_matrix_define_profile(
        desktop_linux_x86_64 desktop_linux x86_64
        "x86_64-unknown-linux-gnu" 64
        "execbc;aot_c;aot_llvm;host_jit"
        "unwind;debug;native_callbacks"
        "threads;concurrent_gc;pmu"
        ""
        host TRUE)
_zr_vm_ssa_platform_matrix_define_profile(
        desktop_linux_aarch64 desktop_linux aarch64
        "aarch64-unknown-linux-gnu" 64
        "execbc;aot_c;aot_llvm;host_jit"
        "unwind;debug;native_callbacks"
        "threads;concurrent_gc;pmu"
        ""
        host TRUE)
_zr_vm_ssa_platform_matrix_define_profile(
        desktop_darwin_x86_64 desktop_darwin x86_64
        "x86_64-apple-darwin" 64
        "execbc;aot_c;aot_llvm;host_jit"
        "unwind;debug;native_callbacks"
        "threads;concurrent_gc;pmu"
        ""
        host TRUE)
_zr_vm_ssa_platform_matrix_define_profile(
        desktop_darwin_aarch64 desktop_darwin aarch64
        "arm64-apple-darwin" 64
        "execbc;aot_c;aot_llvm;host_jit"
        "unwind;debug;native_callbacks"
        "threads;concurrent_gc;pmu"
        ""
        host TRUE)

_zr_vm_ssa_platform_matrix_define_profile(
        android_aarch64 android aarch64
        "aarch64-linux-android" 64
        "execbc;aot_c;aot_llvm"
        "unwind;debug;native_callbacks;restricted_patch"
        "threads;concurrent_gc;pmu"
        "machine_code_jit"
        real_device FALSE)
_zr_vm_ssa_platform_matrix_define_profile(
        ios_aarch64 ios aarch64
        "arm64-apple-ios" 64
        "execbc;aot_c;aot_llvm"
        "unwind;debug;native_callbacks;restricted_patch"
        "threads;concurrent_gc"
        "machine_code_jit"
        real_device FALSE)
_zr_vm_ssa_platform_matrix_define_profile(
        wasm32 wasm wasm32
        "wasm32-unknown-unknown" 32
        "execbc;aot_c;aot_llvm"
        "unwind;debug"
        "threads;concurrent_gc"
        "machine_code_jit;pmu"
        wasm_runtime FALSE)
_zr_vm_ssa_platform_matrix_define_profile(
        wasm64 wasm wasm64
        "wasm64-unknown-unknown" 64
        "execbc;aot_c;aot_llvm"
        "unwind;debug"
        "threads;concurrent_gc"
        "machine_code_jit;pmu"
        wasm_runtime FALSE)

# Keep an internal copy for diagnostics.  The public list above is the stable
# ordering used by reports; the internal list catches accidental duplicate or
# missing macro calls if this file is edited.
# The macro above populated the internal list while defining rows.  Do not
# overwrite it with the public list: comparing the two catches an accidental
# duplicate/missing row in this declaration file.

function(_zr_vm_ssa_platform_matrix_profile_is_known profile output_variable)
    list(FIND ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_IDS "${profile}" _index)
    if (_index GREATER -1)
        set(${output_variable} TRUE PARENT_SCOPE)
    else ()
        set(${output_variable} FALSE PARENT_SCOPE)
    endif ()
endfunction()

function(_zr_vm_ssa_platform_matrix_list_contains list_value item output_variable)
    set(_values ${list_value})
    list(FIND _values "${item}" _index)
    if (_index GREATER -1)
        set(${output_variable} TRUE PARENT_SCOPE)
    else ()
        set(${output_variable} FALSE PARENT_SCOPE)
    endif ()
endfunction()

function(_zr_vm_ssa_platform_matrix_validate_profile profile output_valid output_reason)
    set(_valid TRUE)
    set(_reason "")
    _zr_vm_ssa_platform_matrix_profile_is_known("${profile}" _known)
    if (NOT _known)
        set(_valid FALSE)
        set(_reason "unknown-profile")
    else ()
        set(_prefix "ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${profile}")
        foreach (_required_field IN ITEMS
                TARGET_CLASS ARCHITECTURE TARGET_TRIPLE POINTER_WIDTH_BITS
                ALLOWED_BACKENDS DEFAULT_FEATURES OPTIONAL_FEATURES
                FORBIDDEN_FEATURES RUNNER MACHINE_CODE_JIT_ALLOWED)
            if (NOT DEFINED ${_prefix}_${_required_field})
                set(_valid FALSE)
                set(_reason "missing-${_required_field}")
                break()
            endif ()
        endforeach ()

        if (_valid)
            set(_target_class "${${_prefix}_TARGET_CLASS}")
            set(_architecture "${${_prefix}_ARCHITECTURE}")
            set(_pointer_width "${${_prefix}_POINTER_WIDTH_BITS}")
            set(_allowed_backends ${${_prefix}_ALLOWED_BACKENDS})
            set(_default_features ${${_prefix}_DEFAULT_FEATURES})
            set(_optional_features ${${_prefix}_OPTIONAL_FEATURES})
            set(_forbidden_features ${${_prefix}_FORBIDDEN_FEATURES})
            set(_runner "${${_prefix}_RUNNER}")
            set(_jit_allowed "${${_prefix}_MACHINE_CODE_JIT_ALLOWED}")

            list(FIND ZR_VM_SSA_PLATFORM_MATRIX_TARGET_CLASSES
                    "${_target_class}" _target_class_index)
            list(FIND ZR_VM_SSA_PLATFORM_MATRIX_ARCHITECTURES
                    "${_architecture}" _architecture_index)
            if (_target_class_index EQUAL -1)
                set(_valid FALSE)
                set(_reason "unknown-target-class-${_target_class}")
            elseif (_architecture_index EQUAL -1)
                set(_valid FALSE)
                set(_reason "unknown-architecture-${_architecture}")
            elseif ("${${_prefix}_TARGET_TRIPLE}" STREQUAL "")
                set(_valid FALSE)
                set(_reason "missing-target-triple")
            elseif (NOT _pointer_width STREQUAL "32" AND
                    NOT _pointer_width STREQUAL "64")
                set(_valid FALSE)
                set(_reason "invalid-pointer-width")
            elseif (_target_class STREQUAL "wasm" AND
                    ((_architecture STREQUAL "wasm32" AND
                      NOT _pointer_width STREQUAL "32") OR
                     (_architecture STREQUAL "wasm64" AND
                      NOT _pointer_width STREQUAL "64")))
                set(_valid FALSE)
                set(_reason "wasm-pointer-width-mismatch")
            elseif (_target_class STREQUAL "ios" AND
                    NOT _architecture STREQUAL "aarch64")
                set(_valid FALSE)
                set(_reason "ios-architecture-mismatch")
            elseif (_target_class STREQUAL "wasm" AND _jit_allowed)
                set(_valid FALSE)
                set(_reason "wasm-jit-must-be-forbidden")
            elseif ((_target_class STREQUAL "android" OR
                     _target_class STREQUAL "ios") AND _jit_allowed)
                set(_valid FALSE)
                set(_reason "mobile-jit-must-be-forbidden")
            elseif (NOT _target_class STREQUAL "desktop_windows" AND
                    NOT _target_class STREQUAL "desktop_linux" AND
                    NOT _target_class STREQUAL "desktop_darwin" AND
                    _jit_allowed AND
                    NOT _target_class STREQUAL "desktop")
                # A future non-desktop profile must opt in explicitly; this
                # guard prevents a typo from granting executable JIT memory.
                set(_valid FALSE)
                set(_reason "non-desktop-jit-policy-invalid")
            endif ()

            foreach (_backend IN LISTS _allowed_backends)
                _zr_vm_ssa_platform_matrix_list_contains(
                        "${ZR_VM_SSA_PLATFORM_MATRIX_BACKENDS}"
                        "${_backend}" _backend_known)
                if (NOT _backend_known)
                    set(_valid FALSE)
                    set(_reason "unknown-backend-${_backend}")
                    break()
                endif ()
            endforeach ()
            if (_valid)
                foreach (_feature IN LISTS _default_features _optional_features _forbidden_features)
                    _zr_vm_ssa_platform_matrix_list_contains(
                            "${ZR_VM_SSA_PLATFORM_MATRIX_FEATURES}"
                            "${_feature}" _feature_known)
                    if (NOT _feature_known)
                        set(_valid FALSE)
                        set(_reason "unknown-feature-${_feature}")
                        break()
                    endif ()
                endforeach ()
            endif ()
            if (_valid)
                foreach (_feature IN LISTS _default_features)
                    list(FIND _forbidden_features "${_feature}" _forbidden_index)
                    if (_forbidden_index GREATER -1)
                        set(_valid FALSE)
                        set(_reason "default-feature-forbidden-${_feature}")
                        break()
                    endif ()
                endforeach ()
            endif ()
            if (_valid)
                foreach (_feature IN LISTS _optional_features)
                    list(FIND _forbidden_features "${_feature}" _forbidden_index)
                    if (_forbidden_index GREATER -1)
                        set(_valid FALSE)
                        set(_reason "optional-feature-forbidden-${_feature}")
                        break()
                    endif ()
                endforeach ()
            endif ()
            if (_valid)
                list(FIND _allowed_backends host_jit _jit_index)
                if (_jit_allowed AND _jit_index EQUAL -1)
                    set(_valid FALSE)
                    set(_reason "jit-allowed-backend-missing")
                elseif (NOT _jit_allowed AND _jit_index GREATER -1)
                    set(_valid FALSE)
                    set(_reason "jit-forbidden-backend-present")
                endif ()
            endif ()
            if (_valid)
                list(FIND _forbidden_features machine_code_jit _forbidden_jit_index)
                if (_forbidden_jit_index GREATER -1 AND _jit_allowed)
                    set(_valid FALSE)
                    set(_reason "jit-policy-conflicts-with-forbidden-feature")
                endif ()
            endif ()
            if (_valid)
                list(FIND _default_features threads _threads_index)
                list(FIND _default_features concurrent_gc _concurrent_index)
                if (_concurrent_index GREATER -1 AND _threads_index EQUAL -1)
                    set(_valid FALSE)
                    set(_reason "concurrent-gc-requires-threads")
                endif ()
            endif ()
            if (_valid)
                list(FIND ZR_VM_SSA_PLATFORM_MATRIX_RUNNERS
                        "${_runner}" _runner_index)
                if (_runner_index EQUAL -1)
                    set(_valid FALSE)
                    set(_reason "unknown-runner-${_runner}")
                endif ()
            endif ()
        endif ()
    endif ()
    set(${output_valid} "${_valid}" PARENT_SCOPE)
    set(${output_reason} "${_reason}" PARENT_SCOPE)
endfunction()

function(zr_vm_ssa_platform_matrix_validate output_variable)
    set(_valid TRUE)
    set(_reason "")
    list(LENGTH ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_IDS _profile_count)
    if (NOT _profile_count EQUAL 10)
        set(_valid FALSE)
        set(_reason "profile-count-${_profile_count}")
    endif ()
    list(LENGTH ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_IDS_INTERNAL _internal_count)
    if (NOT _internal_count EQUAL _profile_count)
        set(_valid FALSE)
        set(_reason "internal-profile-count-mismatch")
    endif ()
    if (_valid)
        set(_public_ids ${ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_IDS})
        set(_internal_ids ${ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_IDS_INTERNAL})
        list(REMOVE_DUPLICATES _public_ids)
        list(REMOVE_DUPLICATES _internal_ids)
        list(LENGTH _public_ids _unique_public_count)
        list(LENGTH _internal_ids _unique_internal_count)
        if (NOT _unique_public_count EQUAL _profile_count OR
            NOT _unique_internal_count EQUAL _internal_count)
            set(_valid FALSE)
            set(_reason "duplicate-profile-id")
        endif ()
    endif ()
    if (_valid)
        foreach (_profile IN LISTS ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_IDS)
            _zr_vm_ssa_platform_matrix_validate_profile(
                    "${_profile}" _profile_valid _profile_reason)
            if (NOT _profile_valid)
                set(_valid FALSE)
                set(_reason "${_profile}:${_profile_reason}")
                break()
            endif ()
        endforeach ()
    endif ()
    set(${output_variable} "${_valid}" PARENT_SCOPE)
    set(ZR_VM_SSA_PLATFORM_MATRIX_VALID "${_valid}" PARENT_SCOPE)
    set(ZR_VM_SSA_PLATFORM_MATRIX_VALID_REASON "${_reason}" PARENT_SCOPE)
endfunction()

# Return one profile property.  An unknown profile or field yields an empty
# value; callers should call zr_vm_ssa_platform_matrix_validate first when a
# malformed declaration must be a configure error.
function(zr_vm_ssa_platform_matrix_get_field profile field output_variable)
    _zr_vm_ssa_platform_matrix_profile_is_known("${profile}" _known)
    if (NOT _known)
        set(${output_variable} "" PARENT_SCOPE)
        return()
    endif ()
    string(TOUPPER "${field}" _field_upper)
    set(_prefix "ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${profile}")
    if (DEFINED ${_prefix}_${_field_upper})
        set(${output_variable} "${${_prefix}_${_field_upper}}" PARENT_SCOPE)
    else ()
        set(${output_variable} "" PARENT_SCOPE)
    endif ()
endfunction()

# Return a stable key/value list suitable for a generated manifest or a
# diagnostic message.  No host pointers, executable addresses, or handles are
# placed in this representation.
function(zr_vm_ssa_platform_matrix_get_profile profile output_variable)
    _zr_vm_ssa_platform_matrix_profile_is_known("${profile}" _known)
    if (NOT _known)
        set(${output_variable} "" PARENT_SCOPE)
        return()
    endif ()
    set(_prefix "ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${profile}")
    set(_record
            "id=${profile}"
            "target_class=${${_prefix}_TARGET_CLASS}"
            "architecture=${${_prefix}_ARCHITECTURE}"
            "target_triple=${${_prefix}_TARGET_TRIPLE}"
            "pointer_width_bits=${${_prefix}_POINTER_WIDTH_BITS}"
            "allowed_backends=${${_prefix}_ALLOWED_BACKENDS}"
            "default_features=${${_prefix}_DEFAULT_FEATURES}"
            "optional_features=${${_prefix}_OPTIONAL_FEATURES}"
            "forbidden_features=${${_prefix}_FORBIDDEN_FEATURES}"
            "runner=${${_prefix}_RUNNER}"
            "machine_code_jit_allowed=${${_prefix}_MACHINE_CODE_JIT_ALLOWED}")
    set(${output_variable} "${_record}" PARENT_SCOPE)
endfunction()

# Detect only the target profile.  Capability and runtime evidence remain
# caller supplied; in particular, this function never turns CMAKE_SYSTEM_NAME
# into a claim that a mobile/WASM runner executed.
function(zr_vm_ssa_platform_matrix_detect_current_target output_profile output_reason)
    set(_profile "")
    set(_reason "")
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" _system)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _processor)
    string(TOLOWER "${CMAKE_OSX_SYSROOT}" _sysroot)

    if (_system STREQUAL "")
        set(_reason "system-name-missing")
    else ()
        if (_system MATCHES "android")
            set(_class android)
        elseif ((_system MATCHES "ios|tvos|watchos") OR
                _sysroot MATCHES "iphone|appletv|watch|xros")
            set(_class ios)
        elseif (_system MATCHES "emscripten|wasi|wasm|webassembly")
            set(_class wasm)
        elseif (_system MATCHES "windows")
            set(_class desktop_windows)
        elseif (_system MATCHES "darwin")
            set(_class desktop_darwin)
        elseif (_system MATCHES "linux")
            set(_class desktop_linux)
        else ()
            set(_reason "system-unsupported-${CMAKE_SYSTEM_NAME}")
        endif ()

        if (_reason STREQUAL "")
            if (_processor MATCHES "^(x86_64|amd64|x64)$")
                set(_arch x86_64)
            elseif (_processor MATCHES "^(aarch64|arm64)$")
                set(_arch aarch64)
            elseif (_processor MATCHES "^(wasm32|wasm64)$")
                set(_arch "${_processor}")
            elseif (_class STREQUAL "wasm" AND
                    _system MATCHES "wasm64")
                set(_arch wasm64)
            elseif (_class STREQUAL "wasm")
                set(_arch wasm32)
            else ()
                set(_reason "processor-unsupported-${CMAKE_SYSTEM_PROCESSOR}")
            endif ()
        endif ()

        if (_reason STREQUAL "")
            if (_class STREQUAL "wasm")
                set(_profile "${_arch}")
            else ()
                set(_profile "${_class}_${_arch}")
            endif ()
            _zr_vm_ssa_platform_matrix_profile_is_known("${_profile}" _known)
            if (NOT _known)
                set(_reason "profile-unsupported-${_profile}")
                set(_profile "")
            endif ()
        endif ()
    endif ()
    set(${output_profile} "${_profile}" PARENT_SCOPE)
    set(${output_reason} "${_reason}" PARENT_SCOPE)
endfunction()

function(_zr_vm_ssa_platform_matrix_skip_test name reason profile backend)
    # A skip regular expression is visible in CTest and is not counted as a
    # semantic pass.  Keep the marker stable so CI/report parsers can preserve
    # the exact unavailable reason.
    add_test(NAME "${name}"
            COMMAND ${CMAKE_COMMAND} -E echo
            "ZR_SSA_PLATFORM_UNAVAILABLE:${reason}")
    set_tests_properties("${name}" PROPERTIES
            LABELS "ssa;platform;platform-${profile};backend-${backend}"
            SKIP_REGULAR_EXPRESSION "ZR_SSA_PLATFORM_UNAVAILABLE")
endfunction()

# Register a platform test from a target or an explicit command.  Required
# features are checked against DEFAULT_FEATURES plus DECLARED_FEATURES (an
# adapter may opt into an OPTIONAL_FEATURE).  If any input is unavailable, a
# visible skipped CTest is registered rather than silently omitting the row.
function(zr_vm_ssa_platform_matrix_register_test)
    cmake_parse_arguments(ARG
            "ALLOW_UNAVAILABLE;REQUIRE_TARGET"
            "NAME;TARGET;PROFILE;BACKEND"
            "COMMAND;REQUIRED_FEATURES;DECLARED_FEATURES"
            ${ARGN})
    if (ARG_NAME STREQUAL "")
        message(FATAL_ERROR
                "zr_vm_ssa_platform_matrix_register_test requires NAME")
    endif ()
    if (CMAKE_SCRIPT_MODE_FILE)
        message(FATAL_ERROR
                "platform tests cannot be registered in cmake -P mode")
    endif ()
    if (ARG_PROFILE STREQUAL "")
        zr_vm_ssa_platform_matrix_detect_current_target(
                ARG_PROFILE _detect_reason)
    else ()
        set(_detect_reason "")
    endif ()
    _zr_vm_ssa_platform_matrix_profile_is_known("${ARG_PROFILE}" _profile_known)
    if (NOT _profile_known)
        if (ARG_REQUIRE_TARGET)
            message(FATAL_ERROR
                    "${ARG_NAME}: unknown platform profile '${ARG_PROFILE}'")
        endif ()
        _zr_vm_ssa_platform_matrix_skip_test(
                "${ARG_NAME}" "${_detect_reason}profile-unknown" "${ARG_PROFILE}" "${ARG_BACKEND}")
        return()
    endif ()
    if (ARG_BACKEND STREQUAL "")
        message(FATAL_ERROR
                "${ARG_NAME}: BACKEND is required for platform registration")
    endif ()
    _zr_vm_ssa_platform_matrix_list_contains(
            "${ZR_VM_SSA_PLATFORM_MATRIX_BACKENDS}" "${ARG_BACKEND}"
            _backend_known)
    if (NOT _backend_known)
        message(FATAL_ERROR "${ARG_NAME}: unknown backend '${ARG_BACKEND}'")
    endif ()
    set(_prefix "ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${ARG_PROFILE}")
    set(_allowed_backends ${${_prefix}_ALLOWED_BACKENDS})
    list(FIND _allowed_backends "${ARG_BACKEND}" _backend_index)
    if (_backend_index EQUAL -1)
        if (ARG_REQUIRE_TARGET)
            message(FATAL_ERROR
                    "${ARG_NAME}: backend '${ARG_BACKEND}' is forbidden for ${ARG_PROFILE}")
        endif ()
        _zr_vm_ssa_platform_matrix_skip_test(
                "${ARG_NAME}" "backend-unsupported-${ARG_BACKEND}" "${ARG_PROFILE}" "${ARG_BACKEND}")
        return()
    endif ()

    set(_declared_features ${${_prefix}_DEFAULT_FEATURES})
    list(APPEND _declared_features ${ARG_DECLARED_FEATURES})
    list(REMOVE_DUPLICATES _declared_features)
    foreach (_required_feature IN LISTS ARG_REQUIRED_FEATURES)
        _zr_vm_ssa_platform_matrix_list_contains(
                "${ZR_VM_SSA_PLATFORM_MATRIX_FEATURES}"
                "${_required_feature}" _required_known)
        if (NOT _required_known)
            message(FATAL_ERROR
                    "${ARG_NAME}: unknown required feature '${_required_feature}'")
        endif ()
        list(FIND _declared_features "${_required_feature}" _feature_index)
        if (_feature_index EQUAL -1)
            if (ARG_REQUIRE_TARGET)
                message(FATAL_ERROR
                        "${ARG_NAME}: required feature '${_required_feature}' is unavailable on ${ARG_PROFILE}")
            endif ()
            _zr_vm_ssa_platform_matrix_skip_test(
                    "${ARG_NAME}" "feature-unavailable-${_required_feature}" "${ARG_PROFILE}" "${ARG_BACKEND}")
            return()
        endif ()
    endforeach ()

    if (ARG_TARGET STREQUAL "" AND NOT ARG_COMMAND)
        message(FATAL_ERROR
                "${ARG_NAME}: provide TARGET or COMMAND for a platform test")
    endif ()
    if (NOT ARG_TARGET STREQUAL "" AND NOT TARGET "${ARG_TARGET}")
        if (ARG_REQUIRE_TARGET)
            message(FATAL_ERROR
                    "${ARG_NAME}: target '${ARG_TARGET}' does not exist")
        endif ()
        _zr_vm_ssa_platform_matrix_skip_test(
                "${ARG_NAME}" "target-unavailable-${ARG_TARGET}" "${ARG_PROFILE}" "${ARG_BACKEND}")
        return()
    endif ()

    if (NOT ARG_TARGET STREQUAL "")
        add_test(NAME "${ARG_NAME}" COMMAND "$<TARGET_FILE:${ARG_TARGET}>")
    else ()
        add_test(NAME "${ARG_NAME}" COMMAND ${ARG_COMMAND})
    endif ()
    set_tests_properties("${ARG_NAME}" PROPERTIES
            LABELS "ssa;platform;platform-${ARG_PROFILE};backend-${ARG_BACKEND}")
endfunction()

# Short alias used by a few test CMake fragments.
function(zr_vm_ssa_platform_matrix_register)
    zr_vm_ssa_platform_matrix_register_test(${ARGV})
endfunction()

# Write a deterministic, pointer-free table for an acceptance artifact.  This
# is intentionally opt-in so normal CMake configure never mutates the source
# tree or build directory merely by including the module.
function(zr_vm_ssa_platform_matrix_write_manifest output_path)
    if (output_path STREQUAL "")
        message(FATAL_ERROR
                "zr_vm_ssa_platform_matrix_write_manifest requires a path")
    endif ()
    zr_vm_ssa_platform_matrix_validate(_valid)
    if (NOT _valid)
        message(FATAL_ERROR
                "cannot write invalid SSA platform matrix: ${ZR_VM_SSA_PLATFORM_MATRIX_VALID_REASON}")
    endif ()
    file(WRITE "${output_path}"
            "# ZR SSA platform capability matrix v${ZR_VM_SSA_PLATFORM_MATRIX_SCHEMA_VERSION}\n"
            "# Policy rows are not runtime evidence; unavailable rows stay visible.\n"
            "profile|target_class|architecture|target_triple|pointer_width_bits|allowed_backends|default_features|optional_features|forbidden_features|runner|machine_code_jit_allowed\n")
    foreach (_profile IN LISTS ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_IDS)
        set(_prefix "ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${_profile}")
        file(APPEND "${output_path}"
                "${_profile}|${${_prefix}_TARGET_CLASS}|${${_prefix}_ARCHITECTURE}|${${_prefix}_TARGET_TRIPLE}|${${_prefix}_POINTER_WIDTH_BITS}|${${_prefix}_ALLOWED_BACKENDS}|${${_prefix}_DEFAULT_FEATURES}|${${_prefix}_OPTIONAL_FEATURES}|${${_prefix}_FORBIDDEN_FEATURES}|${${_prefix}_RUNNER}|${${_prefix}_MACHINE_CODE_JIT_ALLOWED}\n")
    endforeach ()
endfunction()

if (CMAKE_SCRIPT_MODE_FILE)
    zr_vm_ssa_platform_matrix_validate(_zr_ssa_platform_matrix_valid)
    if (NOT _zr_ssa_platform_matrix_valid)
        message(FATAL_ERROR
                "invalid SSA platform matrix declaration: ${ZR_VM_SSA_PLATFORM_MATRIX_VALID_REASON}")
    endif ()

    # Declaration-level negative checks: mobile/WASM JIT must remain forbidden,
    # and a 32-bit WASM row must not advertise a 64-bit pointer witness.
    foreach (_forbidden_profile IN ITEMS android_aarch64 ios_aarch64 wasm32 wasm64)
        set(_forbidden_prefix
                "ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_${_forbidden_profile}")
        if ("${${_forbidden_prefix}_MACHINE_CODE_JIT_ALLOWED}" STREQUAL "TRUE")
            message(FATAL_ERROR
                    "${_forbidden_profile}: machine-code JIT policy is not fail-closed")
        endif ()
    endforeach ()
    if (NOT "${ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_wasm32_POINTER_WIDTH_BITS}" STREQUAL "32")
        message(FATAL_ERROR "wasm32 pointer-width witness drifted")
    endif ()
    list(LENGTH ZR_VM_SSA_PLATFORM_MATRIX_PROFILE_IDS _self_profile_count)
    message(STATUS
            "SSA platform matrix self-check passed (${ZR_VM_SSA_PLATFORM_MATRIX_SCHEMA_VERSION}; ${_self_profile_count} profiles)")
endif ()
