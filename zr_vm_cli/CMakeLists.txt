include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_cli")

zr_declare_executable(${zr_curr_module_name} ON)

set(zr_curr_module_exec ${zr_curr_module_name}_executable)

target_include_directories(${zr_curr_module_exec} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${zr_curr_module_exec} PRIVATE
        ZR_VM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)

if (WIN32)
    file(TO_CMAKE_PATH "${CMAKE_C_COMPILER}" zr_vm_host_c_compiler)
    target_compile_definitions(${zr_curr_module_exec} PRIVATE
            ZR_VM_HOST_C_COMPILER="${zr_vm_host_c_compiler}"
    )

    find_program(zr_vm_host_llvm_tool NAMES clang-cl clang)
    if (NOT zr_vm_host_llvm_tool AND zr_vm_host_c_compiler MATCHES "/VC/Tools/MSVC/")
        string(REGEX REPLACE
                "/VC/Tools/MSVC/[^/]+/bin/Host[^/]+/[^/]+/cl\\.exe$"
                "/VC/Tools/Llvm/x64/bin/clang-cl.exe"
                zr_vm_derived_llvm_tool
                "${zr_vm_host_c_compiler}")
        if (EXISTS "${zr_vm_derived_llvm_tool}")
            set(zr_vm_host_llvm_tool "${zr_vm_derived_llvm_tool}")
        endif ()
    endif ()

    if (zr_vm_host_llvm_tool)
        file(TO_CMAKE_PATH "${zr_vm_host_llvm_tool}" zr_vm_host_llvm_tool)
        target_compile_definitions(${zr_curr_module_exec} PRIVATE
                ZR_VM_HOST_LLVM_TOOL="${zr_vm_host_llvm_tool}"
        )
    endif ()
endif ()

zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_library")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_lib_ffi")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_lib_math")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_lib_system")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_lib_container")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_parser")

set(zr_vm_cli_has_debug_agent OFF)
if (TARGET zr_vm_lib_network_shared AND TARGET zr_vm_debug_shared)
    set(zr_vm_cli_has_debug_agent ON)
elseif (TARGET zr_vm_lib_network_static AND TARGET zr_vm_debug_static)
    set(zr_vm_cli_has_debug_agent ON)
endif ()

if (zr_vm_cli_has_debug_agent)
    zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_lib_network")
    zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_debug")
    target_compile_definitions(${zr_curr_module_exec} PRIVATE
            ZR_VM_CLI_HAS_DEBUG_AGENT=1
    )
endif ()

if (BUILD_SHARED_LIB)
    target_compile_definitions(${zr_curr_module_exec} PRIVATE
            ZR_VM_HOST_LIB_DIR="$<TARGET_LINKER_FILE_DIR:zr_vm_library_shared>"
    )
elseif (BUILD_STATIC_LIB)
    target_compile_definitions(${zr_curr_module_exec} PRIVATE
            ZR_VM_HOST_LIB_DIR="${CMAKE_BINARY_DIR}/lib"
    )
endif ()

install(TARGETS ${zr_curr_module_exec}
        ARCHIVE DESTINATION exec
        LIBRARY DESTINATION exec)
