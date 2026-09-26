include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_lib_system")

# 系统叶模块由同一原生库提供，根模块负责注册与链接；回调依赖核心状态和原生描述符 API。
zr_declare_module(${zr_curr_module_name} ON)

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

zr_install_module(${zr_curr_module_name})
