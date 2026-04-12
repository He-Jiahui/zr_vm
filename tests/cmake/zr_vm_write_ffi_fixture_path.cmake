# Generate tests_generated/zr_vm_ffi_fixture_path.h with a MSVC-safe path string.
# Invoked as: cmake -DIN_DLL=<abs path to zr_vm_ffi_fixture> -DOUT_FILE=<.h path> -P this_script.cmake
if (NOT DEFINED IN_DLL OR NOT DEFINED OUT_FILE)
    message(FATAL_ERROR "zr_vm_write_ffi_fixture_path.cmake requires -DIN_DLL= and -DOUT_FILE=")
endif ()

file(TO_CMAKE_PATH "${IN_DLL}" _ffi_path_cmake_style)

file(WRITE "${OUT_FILE}"
        "#ifndef ZR_VM_TESTS_FFI_FIXTURE_PATH_H\n"
        "#define ZR_VM_TESTS_FFI_FIXTURE_PATH_H\n"
        "#define ZR_VM_FFI_FIXTURE_PATH \"${_ffi_path_cmake_style}\"\n"
        "#endif\n")
