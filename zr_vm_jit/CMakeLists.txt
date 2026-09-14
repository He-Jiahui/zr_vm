# Optional C++ host-JIT adapter.  The core remains C11 and does not include
# LLVM headers; this target is only configured when ZR_VM_ENABLE_HOST_JIT is ON.

option(ZR_VM_JIT_USE_LLVM
        "Probe for LLVM ORC/JITLink for the optional host-JIT provider"
        OFF)

set(ZR_VM_JIT_HAS_ORC OFF)
if (ZR_VM_JIT_USE_LLVM)
    # The contract adapter intentionally does not hard-code an LLVM API
    # version.  A provider can opt in once the host toolchain supplies a
    # verified ORC/JITLink implementation; absent LLVM remains a visible
    # contract-only build rather than a configure failure.
    find_package(LLVM CONFIG QUIET)
    if (LLVM_FOUND)
        set(ZR_VM_JIT_HAS_ORC ON)
        message(STATUS "LLVM found for zr_vm_jit (${LLVM_PACKAGE_VERSION}); provider hooks remain contract-gated")
    else ()
        message(STATUS "LLVM not found: zr_vm_jit will report BACKEND_UNAVAILABLE")
    endif ()
endif ()

add_library(zr_vm_jit STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/orc_backend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/jit_state_maps.cpp)
add_library(zr_vm_jit::zr_vm_jit ALIAS zr_vm_jit)

target_compile_features(zr_vm_jit PUBLIC cxx_std_17)
target_compile_definitions(zr_vm_jit PRIVATE
        ZR_VM_JIT_HAS_ORC=$<BOOL:${ZR_VM_JIT_HAS_ORC}>)
set_target_properties(zr_vm_jit PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME zr_vm_jit)

target_include_directories(zr_vm_jit
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include)

# Several legacy C headers use C designated initializers and mutable string
# aliases.  They are valid when consumed by the C++ adapter, but GCC's
# -Wpedantic/-Wwrite-strings diagnostics would otherwise obscure adapter
# diagnostics.  The adapter itself remains warning-clean under -Wall -Wextra.
if (MSVC)
    target_compile_options(zr_vm_jit PRIVATE /W4 /EHsc)
else ()
    target_compile_options(zr_vm_jit PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-pedantic>
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-write-strings>)
endif ()

if (TARGET zr_vm_parser_aot_ir)
    target_link_libraries(zr_vm_jit PRIVATE zr_vm_parser_aot_ir)
endif ()
if (TARGET zr_vm_core_static)
    target_link_libraries(zr_vm_jit PRIVATE zr_vm_core_static)
elseif (TARGET zr_vm_core_shared)
    target_link_libraries(zr_vm_jit PRIVATE zr_vm_core_shared)
else ()
    message(FATAL_ERROR "ZR_VM_ENABLE_HOST_JIT requires a core library target")
endif ()

install(TARGETS zr_vm_jit
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)
