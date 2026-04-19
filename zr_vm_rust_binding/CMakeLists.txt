include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_rust_binding")

zr_declare_module(${zr_curr_module_name} ON)

set(zr_vm_rust_binding_cli_support_sources
        ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/project/project.c
        ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
        ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_rust_binding/native.c
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

    if (TARGET zr_vm_lib_thread_shared OR TARGET zr_vm_lib_thread_static)
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

if (TARGET zr_vm_lib_thread_shared OR TARGET zr_vm_lib_thread_static)
    zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_thread")
endif ()

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
if (ZR_VM_RUST_BINDING_CARGO_EXECUTABLE AND TARGET zr_vm_rust_binding_shared)
    set(zr_vm_rust_binding_rust_workspace "${CMAKE_CURRENT_SOURCE_DIR}/rust")
    set(zr_vm_rust_binding_cargo_target_dir "${CMAKE_BINARY_DIR}/cargo/zr_vm_rust_binding")

    if (WIN32)
        set(zr_vm_rust_binding_runtime_env_name "PATH")
        set(zr_vm_rust_binding_runtime_env_value "$<TARGET_FILE_DIR:zr_vm_rust_binding_shared>;$ENV{PATH}")
    elseif (APPLE)
        set(zr_vm_rust_binding_runtime_env_name "DYLD_LIBRARY_PATH")
        set(zr_vm_rust_binding_runtime_env_value "$<TARGET_FILE_DIR:zr_vm_rust_binding_shared>:$ENV{DYLD_LIBRARY_PATH}")
    else ()
        set(zr_vm_rust_binding_runtime_env_name "LD_LIBRARY_PATH")
        set(zr_vm_rust_binding_runtime_env_value "$<TARGET_FILE_DIR:zr_vm_rust_binding_shared>:$ENV{LD_LIBRARY_PATH}")
    endif ()

    add_custom_target(zr_vm_rust_binding_rust_check
            COMMAND ${CMAKE_COMMAND} -E env
                    "CARGO_TARGET_DIR=${zr_vm_rust_binding_cargo_target_dir}"
                    "ZR_VM_RUST_BINDING_LIB_DIR=$<TARGET_LINKER_FILE_DIR:zr_vm_rust_binding_shared>"
                    "${zr_vm_rust_binding_runtime_env_name}=${zr_vm_rust_binding_runtime_env_value}"
                    ${ZR_VM_RUST_BINDING_CARGO_EXECUTABLE} check --workspace
            WORKING_DIRECTORY "${zr_vm_rust_binding_rust_workspace}"
            USES_TERMINAL
            COMMENT "Run cargo check for zr_vm_rust_binding workspace"
    )
    add_dependencies(zr_vm_rust_binding_rust_check zr_vm_rust_binding_shared)

    add_custom_target(zr_vm_rust_binding_rust_test
            COMMAND ${CMAKE_COMMAND} -E env
                    "CARGO_TARGET_DIR=${zr_vm_rust_binding_cargo_target_dir}"
                    "ZR_VM_RUST_BINDING_LIB_DIR=$<TARGET_LINKER_FILE_DIR:zr_vm_rust_binding_shared>"
                    "${zr_vm_rust_binding_runtime_env_name}=${zr_vm_rust_binding_runtime_env_value}"
                    ${ZR_VM_RUST_BINDING_CARGO_EXECUTABLE} test --workspace
            WORKING_DIRECTORY "${zr_vm_rust_binding_rust_workspace}"
            USES_TERMINAL
            COMMENT "Run cargo test for zr_vm_rust_binding workspace"
    )
    add_dependencies(zr_vm_rust_binding_rust_test zr_vm_rust_binding_shared)

    if (BUILD_TESTS)
        add_test(
                NAME zr_vm_rust_binding_cargo_check
                COMMAND ${CMAKE_COMMAND} -E env
                        "CARGO_TARGET_DIR=${zr_vm_rust_binding_cargo_target_dir}"
                        "ZR_VM_RUST_BINDING_LIB_DIR=$<TARGET_LINKER_FILE_DIR:zr_vm_rust_binding_shared>"
                        "${zr_vm_rust_binding_runtime_env_name}=${zr_vm_rust_binding_runtime_env_value}"
                        ${ZR_VM_RUST_BINDING_CARGO_EXECUTABLE} check --workspace
        )
        set_tests_properties(zr_vm_rust_binding_cargo_check PROPERTIES
                WORKING_DIRECTORY "${zr_vm_rust_binding_rust_workspace}"
        )

        add_test(
                NAME zr_vm_rust_binding_cargo_test
                COMMAND ${CMAKE_COMMAND} -E env
                        "CARGO_TARGET_DIR=${zr_vm_rust_binding_cargo_target_dir}"
                        "ZR_VM_RUST_BINDING_LIB_DIR=$<TARGET_LINKER_FILE_DIR:zr_vm_rust_binding_shared>"
                        "${zr_vm_rust_binding_runtime_env_name}=${zr_vm_rust_binding_runtime_env_value}"
                        ${ZR_VM_RUST_BINDING_CARGO_EXECUTABLE} test --workspace
        )
        set_tests_properties(zr_vm_rust_binding_cargo_test PROPERTIES
                WORKING_DIRECTORY "${zr_vm_rust_binding_rust_workspace}"
        )
    endif ()
endif ()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

zr_install_module(${zr_curr_module_name})
