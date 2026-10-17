include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_rust_binding")

zr_declare_module(${zr_curr_module_name} ON)

# Rust ABI 的项目编译/执行复用 CLI 原有管线；native callback 也进入相同模块目标，避免宿主行为分叉。
set(zr_vm_rust_binding_cli_support_sources
        ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/project/project.c
        ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
        ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.c
        ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.c
        ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_rust_binding/native.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_rust_binding/native_argument_view.c
)

function(zr_vm_rust_binding_configure_target target_name)
    target_sources(${target_name} PRIVATE ${zr_vm_rust_binding_cli_support_sources})
    target_include_directories(${target_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/include
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli
    )
    target_compile_definitions(${target_name} PRIVATE
            ZR_VM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    )

    if (TARGET zr_vm_thread_shared OR TARGET zr_vm_thread_static)
        target_compile_definitions(${target_name} PRIVATE
                ZR_VM_HAS_THREAD_MODULE=1
        )
    endif ()

    if (WIN32)
        file(TO_CMAKE_PATH "${CMAKE_C_COMPILER}" zr_vm_rust_binding_host_c_compiler)
        target_compile_definitions(${target_name} PRIVATE
                ZR_VM_HOST_C_COMPILER="${zr_vm_rust_binding_host_c_compiler}"
        )
    endif ()
endfunction()

if (BUILD_STATIC_LIB)
    zr_vm_rust_binding_configure_target(zr_vm_rust_binding_static)
    target_compile_definitions(zr_vm_rust_binding_static PRIVATE
            ZR_VM_HOST_LIB_DIR="${CMAKE_BINARY_DIR}/lib"
    )
endif ()

if (BUILD_SHARED_LIB)
    zr_vm_rust_binding_configure_target(zr_vm_rust_binding_shared)
    target_compile_definitions(zr_vm_rust_binding_shared PRIVATE
            ZR_VM_HOST_LIB_DIR="$<TARGET_LINKER_FILE_DIR:zr_vm_library_shared>"
    )
endif ()

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_parser")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_math")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_system")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_network")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_container")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_ffi")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_iteration")

if (TARGET zr_vm_thread_shared OR TARGET zr_vm_thread_static)
    zr_link_library_for_module(${zr_curr_module_name} "zr_vm_thread")
endif ()

# 优先从用户 cargo 安装目录找工具；校验入口随后按当前 shared/static native target 选择。
set(zr_vm_rust_binding_cargo_candidates
        "$ENV{HOME}/.cargo/bin/cargo"
        "$ENV{USERPROFILE}/.cargo/bin/cargo.exe"
        "$ENV{USERPROFILE}/.cargo/bin/cargo"
)
set(zr_vm_rust_binding_preferred_cargo "")
foreach (zr_vm_rust_binding_cargo_candidate IN LISTS zr_vm_rust_binding_cargo_candidates)
    if (zr_vm_rust_binding_cargo_candidate AND EXISTS "${zr_vm_rust_binding_cargo_candidate}")
        set(zr_vm_rust_binding_preferred_cargo "${zr_vm_rust_binding_cargo_candidate}")
        break()
    endif ()
endforeach ()

if (zr_vm_rust_binding_preferred_cargo)
    set(ZR_VM_RUST_BINDING_CARGO_EXECUTABLE
            "${zr_vm_rust_binding_preferred_cargo}"
            CACHE FILEPATH "Cargo executable for zr_vm_rust_binding workspace"
            FORCE
    )
else ()
    unset(ZR_VM_RUST_BINDING_CARGO_EXECUTABLE CACHE)
    find_program(ZR_VM_RUST_BINDING_CARGO_EXECUTABLE NAMES cargo cargo.exe)
endif ()
# Keep Cargo products beside this native build by default. Callers may select
# an absolute output root on any platform; no machine-specific drive is required.
set(zr_vm_rust_binding_cargo_root "$ENV{ZR_VM_CARGO_TARGET_ROOT}"
        CACHE PATH "Cargo output root (defaults to the native build directory)")
if (NOT zr_vm_rust_binding_cargo_root)
    set(zr_vm_rust_binding_cargo_root "${CMAKE_CURRENT_BINARY_DIR}/cargo")
endif ()
set(zr_vm_rust_binding_cargo_output_root "$ENV{ZR_VM_CARGO_OUTPUT_ROOT}"
        CACHE PATH "Optional Cargo output directory override")
if (zr_vm_rust_binding_cargo_output_root)
    set(zr_vm_rust_binding_cargo_root "${zr_vm_rust_binding_cargo_output_root}")
endif ()
get_filename_component(zr_vm_rust_binding_cargo_root
        "${zr_vm_rust_binding_cargo_root}" ABSOLUTE BASE_DIR "${CMAKE_BINARY_DIR}")
set(zr_vm_rust_binding_cargo_target_dir "${zr_vm_rust_binding_cargo_root}/cargo-target")
set(zr_vm_rust_binding_cargo_home "${zr_vm_rust_binding_cargo_root}/cargo-home")
set(zr_vm_rust_binding_sccache_dir "${zr_vm_rust_binding_cargo_root}/sccache")
set(zr_vm_rust_binding_temp_dir "${zr_vm_rust_binding_cargo_root}/tmp")
file(MAKE_DIRECTORY "${zr_vm_rust_binding_cargo_target_dir}"
        "${zr_vm_rust_binding_cargo_home}" "${zr_vm_rust_binding_sccache_dir}"
        "${zr_vm_rust_binding_temp_dir}")
set(zr_vm_rust_binding_cargo_environment
        "CARGO_TARGET_DIR=${zr_vm_rust_binding_cargo_target_dir}"
        "CARGO_HOME=${zr_vm_rust_binding_cargo_home}"
        "SCCACHE_DIR=${zr_vm_rust_binding_sccache_dir}"
        "TMP=${zr_vm_rust_binding_temp_dir}"
        "TEMP=${zr_vm_rust_binding_temp_dir}"
        "TMPDIR=${zr_vm_rust_binding_temp_dir}"
)

set(zr_vm_rust_binding_cargo_target "")
set(zr_vm_rust_binding_link_kind "")
if (BUILD_SHARED_LIB AND TARGET zr_vm_rust_binding_shared)
    set(zr_vm_rust_binding_cargo_target zr_vm_rust_binding_shared)
    set(zr_vm_rust_binding_link_kind dylib)
elseif (BUILD_STATIC_LIB AND TARGET zr_vm_rust_binding_static)
    set(zr_vm_rust_binding_cargo_target zr_vm_rust_binding_static)
    set(zr_vm_rust_binding_link_kind static)
endif ()

# A static archive does not carry CMake's PRIVATE dependency closure into a
# Cargo/rustc invocation.  Export the same target order and platform system
# libraries that the native target uses, so static-only consumers never fall
# through to the shared library name or a second facade target.
function(zr_vm_rust_binding_collect_static_link_item item_name)
    if ("${item_name}" STREQUAL "Threads::Threads")
        return()
    endif ()
    if (TARGET ${item_name})
        get_property(zr_vm_rust_binding_seen_target GLOBAL
                PROPERTY ZR_VM_RUST_BINDING_SEEN_STATIC_TARGETS)
        if ("${item_name}" IN_LIST zr_vm_rust_binding_seen_target)
            return()
        endif ()
        set_property(GLOBAL APPEND PROPERTY
                ZR_VM_RUST_BINDING_SEEN_STATIC_TARGETS "${item_name}")

        get_target_property(zr_vm_rust_binding_target_type
                ${item_name} TYPE)
        if (zr_vm_rust_binding_target_type STREQUAL "STATIC_LIBRARY")
            get_target_property(zr_vm_rust_binding_output_name
                    ${item_name} OUTPUT_NAME)
            if (NOT zr_vm_rust_binding_output_name)
                set(zr_vm_rust_binding_output_name ${item_name})
            endif ()
            set_property(GLOBAL APPEND PROPERTY
                    ZR_VM_RUST_BINDING_STATIC_LINK_LIBRARIES
                    "${zr_vm_rust_binding_output_name}")
            get_target_property(zr_vm_rust_binding_target_links
                    ${item_name} LINK_LIBRARIES)
        else ()
            get_target_property(zr_vm_rust_binding_target_links
                    ${item_name} INTERFACE_LINK_LIBRARIES)
        endif ()

        if (zr_vm_rust_binding_target_links AND
                NOT zr_vm_rust_binding_target_links MATCHES "-NOTFOUND$")
            foreach (zr_vm_rust_binding_target_link IN LISTS
                    zr_vm_rust_binding_target_links)
                zr_vm_rust_binding_collect_static_link_item(
                        "${zr_vm_rust_binding_target_link}")
            endforeach ()
        endif ()
        return()
    endif ()

    if (NOT "${item_name}" STREQUAL "")
        if (IS_ABSOLUTE "${item_name}")
            if (NOT "${item_name}" MATCHES "\\.(a|lib|so(\\.[0-9]+)*|dylib)$")
                message(FATAL_ERROR
                        "static Rust binding cannot map this system library path: ${item_name}")
            endif ()
            get_filename_component(zr_vm_rust_binding_system_name
                    "${item_name}" NAME_WE)
            string(REGEX REPLACE "^lib" ""
                    zr_vm_rust_binding_system_name
                    "${zr_vm_rust_binding_system_name}")
            set_property(GLOBAL APPEND PROPERTY
                    ZR_VM_RUST_BINDING_STATIC_SYSTEM_LIBRARIES
                    "${zr_vm_rust_binding_system_name}")
        elseif ("${item_name}" MATCHES "^-.*")
            # Rustc receives system linker flags through the target's normal
            # platform contract; no raw CMake flag is guessed here.
            return()
        else ()
            set_property(GLOBAL APPEND PROPERTY
                    ZR_VM_RUST_BINDING_STATIC_SYSTEM_LIBRARIES
                    "${item_name}")
        endif ()
    endif ()
endfunction()

set_property(GLOBAL PROPERTY ZR_VM_RUST_BINDING_SEEN_STATIC_TARGETS "")
set_property(GLOBAL PROPERTY ZR_VM_RUST_BINDING_STATIC_LINK_LIBRARIES "")
set_property(GLOBAL PROPERTY ZR_VM_RUST_BINDING_STATIC_SYSTEM_LIBRARIES "")
if (zr_vm_rust_binding_link_kind STREQUAL "static")
    get_target_property(zr_vm_rust_binding_static_target_links
            zr_vm_rust_binding_static LINK_LIBRARIES)
    foreach (zr_vm_rust_binding_static_target_link IN LISTS
            zr_vm_rust_binding_static_target_links)
        zr_vm_rust_binding_collect_static_link_item(
                "${zr_vm_rust_binding_static_target_link}")
    endforeach ()
endif ()
get_property(zr_vm_rust_binding_static_link_libraries GLOBAL PROPERTY
        ZR_VM_RUST_BINDING_STATIC_LINK_LIBRARIES)
get_property(zr_vm_rust_binding_static_system_libraries GLOBAL PROPERTY
        ZR_VM_RUST_BINDING_STATIC_SYSTEM_LIBRARIES)
list(REMOVE_DUPLICATES zr_vm_rust_binding_static_link_libraries)
list(REMOVE_DUPLICATES zr_vm_rust_binding_static_system_libraries)
if (zr_vm_rust_binding_link_kind STREQUAL "static")
    if (WIN32)
        # ws2_32 is collected from zr_vm_lib_network_static's target links.
    else ()
        # Threads::Threads may expose -pthread through an imported target;
        # make the platform's normal CMake thread library explicit for rustc.
        list(APPEND zr_vm_rust_binding_static_system_libraries pthread)
    endif ()
endif ()

if (zr_vm_rust_binding_static_link_libraries)
    string(JOIN "," zr_vm_rust_binding_static_link_libraries_csv
            ${zr_vm_rust_binding_static_link_libraries})
else ()
    set(zr_vm_rust_binding_static_link_libraries_csv "")
endif ()
if (zr_vm_rust_binding_static_system_libraries)
    string(JOIN "," zr_vm_rust_binding_static_system_libraries_csv
            ${zr_vm_rust_binding_static_system_libraries})
else ()
    set(zr_vm_rust_binding_static_system_libraries_csv "")
endif ()
if (zr_vm_rust_binding_link_kind STREQUAL "static")
    file(GENERATE
            OUTPUT "${CMAKE_BINARY_DIR}/zr_vm_rust_binding-static-link-contract.txt"
            CONTENT "${zr_vm_rust_binding_static_link_libraries_csv}\n${zr_vm_rust_binding_static_system_libraries_csv}\n")
endif ()
set(zr_vm_rust_binding_cargo_link_environment
        "ZR_VM_RUST_BINDING_LINK_KIND=${zr_vm_rust_binding_link_kind}")
if (zr_vm_rust_binding_link_kind STREQUAL "static")
    list(APPEND zr_vm_rust_binding_cargo_link_environment
            "ZR_VM_RUST_BINDING_STATIC_LINK_LIBRARIES=${zr_vm_rust_binding_static_link_libraries_csv}"
            "ZR_VM_RUST_BINDING_STATIC_SYSTEM_LIBRARIES=${zr_vm_rust_binding_static_system_libraries_csv}")
endif ()

if (ZR_VM_RUST_BINDING_CARGO_EXECUTABLE AND zr_vm_rust_binding_cargo_target)
    set(zr_vm_rust_binding_rust_workspace "${CMAKE_CURRENT_SOURCE_DIR}/rust")
    set(zr_vm_rust_binding_runtime_env_name "")
    set(zr_vm_rust_binding_runtime_env_value
            "$<TARGET_FILE_DIR:${zr_vm_rust_binding_cargo_target}>")
    if (zr_vm_rust_binding_link_kind STREQUAL "static")
        set(zr_vm_rust_binding_runtime_env_name "")
        set(zr_vm_rust_binding_runtime_env_value "")
    elseif (WIN32)
        set(zr_vm_rust_binding_runtime_env_name "PATH")
        set(zr_vm_rust_binding_runtime_env_value
                "$<TARGET_FILE_DIR:${zr_vm_rust_binding_cargo_target}>;$ENV{PATH}")
    elseif (APPLE)
        set(zr_vm_rust_binding_runtime_env_name "DYLD_LIBRARY_PATH")
        set(zr_vm_rust_binding_runtime_env_value
                "$<TARGET_FILE_DIR:${zr_vm_rust_binding_cargo_target}>:$ENV{DYLD_LIBRARY_PATH}")
    else ()
        set(zr_vm_rust_binding_runtime_env_name "LD_LIBRARY_PATH")
        set(zr_vm_rust_binding_runtime_env_value
                "$<TARGET_FILE_DIR:${zr_vm_rust_binding_cargo_target}>:$ENV{LD_LIBRARY_PATH}")
    endif ()
    set(zr_vm_rust_binding_cargo_runtime_environment "")
    if (zr_vm_rust_binding_runtime_env_name)
        list(APPEND zr_vm_rust_binding_cargo_runtime_environment
                "${zr_vm_rust_binding_runtime_env_name}=${zr_vm_rust_binding_runtime_env_value}")
    endif ()

    add_custom_target(zr_vm_rust_binding_rust_check
            COMMAND ${CMAKE_COMMAND} -E env
                    ${zr_vm_rust_binding_cargo_environment}
                    ${zr_vm_rust_binding_cargo_link_environment}
                    "ZR_VM_RUST_BINDING_LIB_DIR=$<TARGET_LINKER_FILE_DIR:${zr_vm_rust_binding_cargo_target}>"
                    ${zr_vm_rust_binding_cargo_runtime_environment}
                    ${ZR_VM_RUST_BINDING_CARGO_EXECUTABLE} check --workspace --locked
            WORKING_DIRECTORY "${zr_vm_rust_binding_rust_workspace}"
            USES_TERMINAL
            COMMENT "Run cargo check for zr_vm_rust_binding workspace"
    )
    add_dependencies(zr_vm_rust_binding_rust_check ${zr_vm_rust_binding_cargo_target})

    add_custom_target(zr_vm_rust_binding_rust_test
            COMMAND ${CMAKE_COMMAND} -E env
                    ${zr_vm_rust_binding_cargo_environment}
                    ${zr_vm_rust_binding_cargo_link_environment}
                    "ZR_VM_RUST_BINDING_LIB_DIR=$<TARGET_LINKER_FILE_DIR:${zr_vm_rust_binding_cargo_target}>"
                    ${zr_vm_rust_binding_cargo_runtime_environment}
                    ${ZR_VM_RUST_BINDING_CARGO_EXECUTABLE} test --workspace --locked
            WORKING_DIRECTORY "${zr_vm_rust_binding_rust_workspace}"
            USES_TERMINAL
            COMMENT "Run cargo test for zr_vm_rust_binding workspace"
    )
    add_dependencies(zr_vm_rust_binding_rust_test ${zr_vm_rust_binding_cargo_target})

    if (BUILD_TESTS)
        add_test(
                NAME zr_vm_rust_binding_cargo_check
                COMMAND ${CMAKE_COMMAND} -E env
                        ${zr_vm_rust_binding_cargo_environment}
                        ${zr_vm_rust_binding_cargo_link_environment}
                        "ZR_VM_RUST_BINDING_LIB_DIR=$<TARGET_LINKER_FILE_DIR:${zr_vm_rust_binding_cargo_target}>"
                        ${zr_vm_rust_binding_cargo_runtime_environment}
                        ${ZR_VM_RUST_BINDING_CARGO_EXECUTABLE} check --workspace --locked
        )
        set_tests_properties(zr_vm_rust_binding_cargo_check PROPERTIES
                WORKING_DIRECTORY "${zr_vm_rust_binding_rust_workspace}"
        )

        add_test(
                NAME zr_vm_rust_binding_cargo_test
                COMMAND ${CMAKE_COMMAND} -E env
                        ${zr_vm_rust_binding_cargo_environment}
                        ${zr_vm_rust_binding_cargo_link_environment}
                        "ZR_VM_RUST_BINDING_LIB_DIR=$<TARGET_LINKER_FILE_DIR:${zr_vm_rust_binding_cargo_target}>"
                        ${zr_vm_rust_binding_cargo_runtime_environment}
                        ${ZR_VM_RUST_BINDING_CARGO_EXECUTABLE} test --workspace --locked
        )
        set_tests_properties(zr_vm_rust_binding_cargo_test PROPERTIES
                WORKING_DIRECTORY "${zr_vm_rust_binding_rust_workspace}"
        )
    endif ()
endif ()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

zr_install_module(${zr_curr_module_name})
