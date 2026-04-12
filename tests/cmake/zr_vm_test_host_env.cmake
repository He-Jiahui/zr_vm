#
# When CTest runs build-tree executables that link shared libraries from ${HOST_BINARY_DIR}/lib,
# the dynamic loader may not find them unless RPATH is complete. Prepend that directory to
# LD_LIBRARY_PATH for the duration of the parent cmake -P script (execute_process children inherit it).
#
# On Windows, DLLs usually live under ${HOST_BINARY_DIR}/lib/<Config> while the CLI is under
# bin/<Config>. The loader searches the executable directory first, then PATH. If an unrelated
# directory on PATH contains an older zr_vm_core.dll, that module can be loaded instead of the
# build-tree DLL, causing ABI mismatch crashes (e.g. access violation during GC slot rewrite).
# Prepend the directory that contains the zr_vm_core.dll matching the invoked CLI configuration.
#
if (DEFINED HOST_BINARY_DIR AND NOT HOST_BINARY_DIR STREQUAL "")
    file(TO_CMAKE_PATH "${HOST_BINARY_DIR}" _zr_vm_test_host_binary_dir)

    if (UNIX AND NOT APPLE)
        set(_zr_vm_test_host_lib_dir "${_zr_vm_test_host_binary_dir}/lib")
        if (EXISTS "${_zr_vm_test_host_lib_dir}")
            set(ENV{LD_LIBRARY_PATH} "${_zr_vm_test_host_lib_dir}:$ENV{LD_LIBRARY_PATH}")
        endif ()
    elseif (WIN32)
        set(_zr_vm_win_prepended FALSE)
        set(_zr_vm_anchor_exe "")
        if (DEFINED CLI_EXE AND NOT CLI_EXE STREQUAL "")
            set(_zr_vm_anchor_exe "${CLI_EXE}")
        elseif (DEFINED EXE AND NOT EXE STREQUAL "")
            set(_zr_vm_anchor_exe "${EXE}")
        endif ()

        if (NOT _zr_vm_anchor_exe STREQUAL "")
            file(TO_CMAKE_PATH "${_zr_vm_anchor_exe}" _zr_vm_anchor_exe_norm)
            get_filename_component(_zr_vm_exe_dir "${_zr_vm_anchor_exe_norm}" DIRECTORY)
            get_filename_component(_zr_vm_bin_dir "${_zr_vm_exe_dir}" DIRECTORY)
            get_filename_component(_zr_vm_exe_leaf "${_zr_vm_exe_dir}" NAME)
            get_filename_component(_zr_vm_bin_parent_name "${_zr_vm_bin_dir}" NAME)

            if (_zr_vm_bin_parent_name STREQUAL "bin" AND NOT _zr_vm_exe_leaf STREQUAL "bin")
                get_filename_component(_zr_vm_build_root "${_zr_vm_bin_dir}" DIRECTORY)
                set(_zr_vm_core_dll "${_zr_vm_build_root}/lib/${_zr_vm_exe_leaf}/zr_vm_core.dll")
                if (EXISTS "${_zr_vm_core_dll}")
                    get_filename_component(_zr_vm_core_dir "${_zr_vm_core_dll}" DIRECTORY)
                    file(TO_NATIVE_PATH "${_zr_vm_core_dir}" _zr_vm_core_dir_native)
                    set(ENV{PATH} "${_zr_vm_core_dir_native};$ENV{PATH}")
                    set(_zr_vm_win_prepended TRUE)
                endif ()
            endif ()
        endif ()

        if (NOT _zr_vm_win_prepended AND EXISTS "${_zr_vm_test_host_binary_dir}/lib/zr_vm_core.dll")
            file(TO_NATIVE_PATH "${_zr_vm_test_host_binary_dir}/lib" _zr_vm_lib_flat_native)
            set(ENV{PATH} "${_zr_vm_lib_flat_native};$ENV{PATH}")
        endif ()
    endif ()
endif ()
