include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_lib_ffi")

zr_declare_module(${zr_curr_module_name} ON)

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")

set(ZR_VM_LIBFFI_FOUND OFF)
set(ZR_VM_LIBFFI_BUNDLED_TARGET "")

find_path(ZR_VM_LIBFFI_INCLUDE_DIR
        NAMES ffi.h
        PATH_SUFFIXES
        ""
        x86_64-linux-gnu
        libffi
        libffi/include
        include
)
find_library(ZR_VM_LIBFFI_LIBRARY
        NAMES ffi libffi
)

if (ZR_VM_LIBFFI_INCLUDE_DIR AND ZR_VM_LIBFFI_LIBRARY)
    set(ZR_VM_LIBFFI_FOUND ON)
endif ()

if (NOT ZR_VM_LIBFFI_FOUND AND WIN32 AND MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ZR_VM_LIBFFI_VENDOR_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zr_libffi/libffi)
    set(ZR_VM_LIBFFI_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/libffi/x86_win64)
    set(ZR_VM_LIBFFI_ASM_SOURCE ${ZR_VM_LIBFFI_VENDOR_ROOT}/src/x86/win64_intel.S)
    set(ZR_VM_LIBFFI_ASM_OUTPUT ${ZR_VM_LIBFFI_GEN_DIR}/win64_intel.asm)

    set(TARGET X86_WIN64)
    set(HAVE_LONG_DOUBLE 1)
    set(FFI_EXEC_TRAMPOLINE_TABLE 0)
    set(VERSION 3.5.1)
    set(FFI_VERSION_STRING 3.5.1)
    set(FFI_VERSION_NUMBER 30501)

    file(MAKE_DIRECTORY ${ZR_VM_LIBFFI_GEN_DIR})
    configure_file(${ZR_VM_LIBFFI_VENDOR_ROOT}/include/ffi.h.in ${ZR_VM_LIBFFI_GEN_DIR}/ffi.h @ONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fficonfig_x86_win64.h.in
            ${ZR_VM_LIBFFI_GEN_DIR}/fficonfig.h
            @ONLY)

    enable_language(ASM_MASM)

    add_custom_command(
            OUTPUT ${ZR_VM_LIBFFI_ASM_OUTPUT}
            COMMAND ${CMAKE_COMMAND}
            -DC_COMPILER=${CMAKE_C_COMPILER}
            -DASM_SOURCE=${ZR_VM_LIBFFI_ASM_SOURCE}
            -DASM_OUTPUT=${ZR_VM_LIBFFI_ASM_OUTPUT}
            -DGEN_DIR=${ZR_VM_LIBFFI_GEN_DIR}
            -DVENDOR_ROOT=${ZR_VM_LIBFFI_VENDOR_ROOT}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/preprocess_libffi_asm.cmake
            DEPENDS
            ${ZR_VM_LIBFFI_ASM_SOURCE}
            ${ZR_VM_LIBFFI_VENDOR_ROOT}/include/ffi.h.in
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fficonfig_x86_win64.h.in
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/preprocess_libffi_asm.cmake
            ${ZR_VM_LIBFFI_VENDOR_ROOT}/include/ffi_cfi.h
            ${ZR_VM_LIBFFI_VENDOR_ROOT}/src/x86/asmnames.h
            VERBATIM
    )

    add_library(zr_vm_libffi_vendor STATIC
            ${ZR_VM_LIBFFI_VENDOR_ROOT}/src/prep_cif.c
            ${ZR_VM_LIBFFI_VENDOR_ROOT}/src/types.c
            ${ZR_VM_LIBFFI_VENDOR_ROOT}/src/closures.c
            ${ZR_VM_LIBFFI_VENDOR_ROOT}/src/tramp.c
            ${ZR_VM_LIBFFI_VENDOR_ROOT}/src/x86/ffiw64.c
            ${ZR_VM_LIBFFI_ASM_OUTPUT}
    )
    set_source_files_properties(${ZR_VM_LIBFFI_ASM_OUTPUT} PROPERTIES GENERATED TRUE LANGUAGE ASM_MASM)
    target_include_directories(zr_vm_libffi_vendor
            PUBLIC
            ${ZR_VM_LIBFFI_GEN_DIR}
            ${ZR_VM_LIBFFI_VENDOR_ROOT}/include
            ${ZR_VM_LIBFFI_VENDOR_ROOT}/src
            ${ZR_VM_LIBFFI_VENDOR_ROOT}/src/x86
    )
    target_compile_definitions(zr_vm_libffi_vendor
            PUBLIC
            FFI_STATIC_BUILD=1
            PRIVATE
            FFI_BUILDING=1
            X86_WIN64=1
    )
    set_target_properties(zr_vm_libffi_vendor PROPERTIES OUTPUT_NAME zr_vm_libffi_vendor)

    set(ZR_VM_LIBFFI_FOUND ON)
    set(ZR_VM_LIBFFI_BUNDLED_TARGET zr_vm_libffi_vendor)
endif ()

function(zr_vm_attach_libffi target_name)
    if (NOT TARGET ${target_name})
        return()
    endif ()

    if (ZR_VM_LIBFFI_BUNDLED_TARGET)
        target_link_libraries(${target_name} PRIVATE ${ZR_VM_LIBFFI_BUNDLED_TARGET})
        target_compile_definitions(${target_name} PRIVATE ZR_VM_HAS_LIBFFI=1)
    elseif (ZR_VM_LIBFFI_FOUND)
        target_include_directories(${target_name} PRIVATE ${ZR_VM_LIBFFI_INCLUDE_DIR})
        target_link_libraries(${target_name} PRIVATE ${ZR_VM_LIBFFI_LIBRARY})
        target_compile_definitions(${target_name} PRIVATE ZR_VM_HAS_LIBFFI=1)
    else ()
        target_compile_definitions(${target_name} PRIVATE ZR_VM_HAS_LIBFFI=0)
    endif ()
endfunction()

if (BUILD_STATIC_LIB)
    zr_vm_attach_libffi(${zr_curr_module_name}_static)
endif ()

if (BUILD_SHARED_LIB)
    zr_vm_attach_libffi(${zr_curr_module_name}_shared)
endif ()

if (UNIX AND NOT WIN32)
    find_package(Threads REQUIRED)
    if (BUILD_STATIC_LIB)
        target_link_libraries(${zr_curr_module_name}_static PRIVATE Threads::Threads)
    endif ()
    if (BUILD_SHARED_LIB)
        target_link_libraries(${zr_curr_module_name}_shared PRIVATE Threads::Threads)
    endif ()
endif ()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

zr_install_module(${zr_curr_module_name})
