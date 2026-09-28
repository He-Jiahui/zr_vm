include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_core")
# core 的日志等实现使用线程锁；链接 core 变体前先解析可移植线程目标。
find_package(Threads REQUIRED)
# 第三方目标须在模块链接之前由其所属目录注册。
add_subdirectory(third_party)
# 同一模块名驱动静态/共享目标及后续链接、安装；ON 将 common 源码并入两种变体。
zr_declare_module(${zr_curr_module_name} ON)
# core 的对象绑定代码借用 library 的头文件；此处仅提供私有编译路径，不建立反向链接。
if (BUILD_STATIC_LIB)
    target_include_directories(zr_vm_core_static PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
endif ()
# 共享变体也需要同一私有头文件路径，不能只配置静态目标。
if (BUILD_SHARED_LIB)
    target_include_directories(zr_vm_core_shared PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
endif ()
# 哈希和 UTF-8 实现在 core 下各注册一次，两种目标变体复用对应第三方目标。
zr_link_third_party_for_module(${zr_curr_module_name} "zr_xx_hash")
zr_link_third_party_for_module(${zr_curr_module_name} "zr_utf8proc")
zr_link_internal_for_module(${zr_curr_module_name} Threads::Threads)
# 非 Windows 的浮点运算路径需要系统数学库；线程目标则在所有平台链接。
if (NOT WIN32)
    zr_link_internal_for_module(${zr_curr_module_name} m)
endif ()
zr_install_module(${zr_curr_module_name}) # 安装启用的变体，并向本构建图的链接使用者传播模块名定义。
