# 模块 CMakeLists 与 tests 都会 include 此文件；第三方目标命名与链接函数由此传递。
include(${CMAKE_SOURCE_DIR}/zr_vm_common/ThirdPartyMacros.cmake)

# 为产品模块创建静态/共享变体，按 use_common_lib 决定是否将 common 源码并入各变体。
# ON 要求顶层先加入 common；库开关决定创建哪些目标，后续链接和安装须沿用同一选择。
function(zr_declare_module module_name use_common_lib)
    set(zr_module_name ${module_name})
    get_filename_component(zr_module_src_dir_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    file(GLOB_RECURSE zr_module_src CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    )
    list(REMOVE_DUPLICATES zr_module_src)
    if (${use_common_lib})
        set(zr_module_src ${zr_vm_common_src} ${zr_module_src})
    endif ()

    if (BUILD_STATIC_LIB)
        set(zr_module_static ${zr_module_name}_static)
        add_library(${zr_module_static} STATIC ${zr_module_src})
        # DEFINE MODULE NAME
        target_compile_options(${zr_module_static} PRIVATE -DZR_CURRENT_MODULE="${zr_module_name}")
        # DEFINE LIBRARY TYPE
        target_compile_definitions(${zr_module_static} PRIVATE -DZR_LIBRARY_TYPE_STATIC)

        target_include_directories(${zr_module_static} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_include_directories(${zr_module_static} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/${zr_module_src_dir_name})
        if (${use_common_lib})
            target_include_directories(${zr_module_static} PRIVATE ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
        endif ()
        set_target_properties(${zr_module_static} PROPERTIES OUTPUT_NAME ${zr_module_name})
        if (WIN32 AND BUILD_SHARED_LIB)
            # DLL 的导入库沿用 <module>.lib；静态归档另名，避免 Ninja 输出规则冲突。
            set_target_properties(${zr_module_static} PROPERTIES ARCHIVE_OUTPUT_NAME ${zr_module_name}_static)
        endif ()
    endif ()

    if (BUILD_SHARED_LIB)
        set(zr_module_shared ${zr_module_name}_shared)
        add_library(${zr_module_shared} SHARED ${zr_module_src})
        # DEFINE MODULE NAME
        target_compile_options(${zr_module_shared} PRIVATE -DZR_CURRENT_MODULE="${zr_module_name}")
        # DEFINE LIBRARY TYPE
        target_compile_definitions(${zr_module_shared} PRIVATE -DZR_LIBRARY_TYPE_SHARED)

        target_include_directories(${zr_module_shared} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_include_directories(${zr_module_shared} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/${zr_module_src_dir_name})
        if (${use_common_lib})
            target_include_directories(${zr_module_shared} PRIVATE ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
        endif ()
        set_target_properties(${zr_module_shared} PROPERTIES
                OUTPUT_NAME ${zr_module_name}
                WINDOWS_EXPORT_ALL_SYMBOLS ON)
        # ELF 默认导出全部全局符号，MSVC DLL 只导出 __declspec(dllexport)。
        # 测试会调用未标 ZR_API 的内部函数；不导出时 Windows 链接报 LNK2019。

    endif ()

endfunction()

# CLI 消费此入口；可执行目标只创建一个变体，后续链接函数按构建开关选库。
# ON 把 common 加入本目标的源码列表；本入口不创建独立的 common 链接目标。
function(zr_declare_executable module_name use_common_lib)
    set(zr_module_name ${module_name})
    get_filename_component(zr_module_src_dir_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    file(GLOB_RECURSE zr_module_src CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    )
    list(REMOVE_DUPLICATES zr_module_src)

    set(zr_module_executable ${zr_module_name}_executable)
    if (${use_common_lib})
        set(zr_module_src ${zr_vm_common_src} ${zr_module_src})
    endif ()

    add_executable(${zr_module_executable} ${zr_module_src})


    target_include_directories(${zr_module_executable} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_include_directories(${zr_module_executable} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/${zr_module_src_dir_name})
    if (${use_common_lib})
        target_include_directories(${zr_module_executable} PRIVATE ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    endif ()


    set_target_properties(${zr_module_executable} PROPERTIES OUTPUT_NAME ${zr_module_name})

    #    install(TARGETS ${zr_module_static} ${zr_module_shared}
    #            ARCHIVE DESTINATION lib
    #            LIBRARY DESTINATION lib)

endfunction()

# 将同类型首方变体连成依赖图；依赖头路径不会自动成为本模块的公共 API 契约。
# 本模块目标须已存在，依赖变体须在配置结束前加入；公开头若引用依赖类型，调用方另设 PUBLIC include。
function(zr_link_library_for_module module_name library_name)
    set(zr_module_name ${module_name})

    if (BUILD_STATIC_LIB)
        set(zr_module_static ${zr_module_name}_static)
        target_include_directories(${zr_module_static} PRIVATE ${CMAKE_SOURCE_DIR}/${library_name}/include)
        target_link_libraries(${zr_module_static} PRIVATE ${library_name}_static)
        add_dependencies(${zr_module_static} ${library_name}_static)
    endif ()

    if (BUILD_SHARED_LIB)
        set(zr_module_shared ${zr_module_name}_shared)
        target_include_directories(${zr_module_shared} PRIVATE ${CMAKE_SOURCE_DIR}/${library_name}/include)
        target_link_libraries(${zr_module_shared} PRIVATE ${library_name}_shared)
        add_dependencies(${zr_module_shared} ${library_name}_shared)
    endif ()
endfunction()

# 将已解析的线程目标或系统库名接入各模块变体；本模块须已声明，依赖不会套用 _static/_shared 命名。
function(zr_link_internal_for_module module_name library_name)
    set(zr_module_name ${module_name})
    if (BUILD_STATIC_LIB)
        set(zr_module_static ${zr_module_name}_static)
        target_link_libraries(${zr_module_static} PRIVATE ${library_name})
    endif ()

    if (BUILD_SHARED_LIB)
        set(zr_module_shared ${zr_module_name}_shared)
        target_link_libraries(${zr_module_shared} PRIVATE ${library_name})
    endif ()
endfunction()

# CLI 通过此入口选一个首方库变体；同时启用静态与共享库时优先链接静态目标。
# 调用方必须先创建 executable，并确保所选库变体在当前配置中存在。
function(zr_link_library_for_executable module_name library_name)
    set(zr_module_name ${module_name})
    set(zr_module_executable ${zr_module_name}_executable)

    # Use the same keyword signature as all other first/third-party helpers.
    if (BUILD_STATIC_LIB)
        target_include_directories(${zr_module_executable} PRIVATE ${CMAKE_SOURCE_DIR}/${library_name}/include)
        target_link_libraries(${zr_module_executable} PRIVATE ${library_name}_static)
    elseif (BUILD_SHARED_LIB)
        target_include_directories(${zr_module_executable} PRIVATE ${CMAKE_SOURCE_DIR}/${library_name}/include)
        target_link_libraries(${zr_module_executable} PRIVATE ${library_name}_shared)
    endif ()
endfunction()

# 安装已声明的模块变体，并向其公共使用者传播模块名编译定义。
# 模块各自的 CMakeLists 在依赖配置完成后调用此入口。
# TODO: 当前首方条件编译未发现消费这些模块名宏；核查外部嵌入/导出包的依赖，以确认 PUBLIC 传播目的。
function(zr_install_module module_name)
    set(zr_module_name ${module_name})
    set(zr_module_shared ${zr_module_name}_shared)

    if (BUILD_STATIC_LIB)
        set(zr_module_static ${zr_module_name}_static)
        target_compile_definitions(${zr_module_static} PUBLIC ${zr_module_name})
        install(TARGETS ${zr_module_static}
                ARCHIVE DESTINATION lib
                LIBRARY DESTINATION lib)
    endif ()

    if (BUILD_SHARED_LIB)
        set(zr_module_shared ${zr_module_name}_shared)
        target_compile_definitions(${zr_module_shared} PUBLIC ${zr_module_name})
        install(TARGETS ${zr_module_shared}
                ARCHIVE DESTINATION lib
                LIBRARY DESTINATION lib)
    endif ()
endfunction()
