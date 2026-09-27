# 多个 cmake -P 测试入口共用此环境桥接；仅修改当前 CMake 进程的环境，子进程继承库搜索路径。
# Linux 构建树库目录须优先于宿主路径；Windows 还需用被测 exe 的配置定位同配置 DLL，
# 否则 PATH 中较旧的 zr_vm_core.dll 可能造成 ABI 不匹配。
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
            # 多配置布局为 bin/<Config> 对 lib/<Config>；CLI_EXE/EXE 是本次测试的配置锚点。
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

        # 未找到同配置 DLL 时回退到平铺的构建树 lib；多配置路径缺失也会走此分支。
        if (NOT _zr_vm_win_prepended AND EXISTS "${_zr_vm_test_host_binary_dir}/lib/zr_vm_core.dll")
            file(TO_NATIVE_PATH "${_zr_vm_test_host_binary_dir}/lib" _zr_vm_lib_flat_native)
            set(ENV{PATH} "${_zr_vm_lib_flat_native};$ENV{PATH}")
        endif ()
    endif ()
endif ()
