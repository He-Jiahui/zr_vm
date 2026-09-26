include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_lib_network")

# CLI、LSP 与调试代理共用此提供者；模块注册器另将 TCP/UDP 叶模块挂到 zr.network。
zr_declare_module(${zr_curr_module_name} ON)

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")

if (WIN32)
    # 底层传输直接使用 Winsock；静态和共享变体都需要系统套接字库。
    if (BUILD_STATIC_LIB)
        target_link_libraries(${zr_curr_module_name}_static PRIVATE ws2_32)
    endif ()

    if (BUILD_SHARED_LIB)
        target_link_libraries(${zr_curr_module_name}_shared PRIVATE ws2_32)
    endif ()
endif ()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

zr_install_module(${zr_curr_module_name})
