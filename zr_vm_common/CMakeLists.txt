# 各模块通过 zr_declare_module/ zr_declare_executable 直接编入同一份 common 源码；
# 因此这里只发布源文件清单，不创建独立公共库；目标宏负责添加 common 头文件路径。
# TODO: 此处 GLOB_RECURSE 未启用 CONFIGURE_DEPENDS；新加 common 源文件后需验证增量构建是否触发重配置。
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src")
    file(GLOB_RECURSE zr_module_src_files
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    )
else ()
    set(zr_module_src_files)
endif ()

# 上层 CMakeLists 在加入 zr_vm_common 后才加入各模块，缓存值供其目录作用域读取。
set(zr_vm_common_src
    ${zr_module_src_files}
    CACHE
    STRING
    "common source library"
    FORCE
)

