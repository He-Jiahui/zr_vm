# 模块 CMakeLists 与 tests 都会 include 此文件；第三方目标命名与链接函数由此传递。
include(${CMAKE_SOURCE_DIR}/zr_vm_common/ThirdPartyMacros.cmake)

# 为产品模块创建静态/共享变体，按 use_common_lib 决定是否将 common 源码并入各变体。
# 调用方先设置 BUILD_STATIC_LIB / BUILD_SHARED_LIB；链接和安装函数沿用同一目标命名约定。
function(zr_declare_module module_name use_common_lib)
    set(zr_module_name ${module_name})
    get_filename_component(zr_module_src_dir_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    file(GLOB_RECURSE zr_module_src CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.c"
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
        set_target_properties(${zr_module_shared} PROPERTIES OUTPUT_NAME ${zr_module_name})

    endif ()

endfunction()

# CLI 消费此入口；可执行目标只创建一个变体，后续链接函数按构建开关选库。
# 传入 ON 时 common 源码与 CLI 源码同编译单元集合，避免引入单独的 common 链接目标。
function(zr_declare_executable module_name use_common_lib)
    set(zr_module_name ${module_name})
    get_filename_component(zr_module_src_dir_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    file(GLOB_RECURSE zr_module_src CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.c"
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

# 将首方模块同类型变体相连，避免共享库依赖静态变体或相反。
# 调用方应先通过 zr_declare_module 建立目标，且被依赖模块须已由顶层加入构建。
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

# 将线程库、系统数学库等现成目标同时接入已创建的模块变体。
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

    # BUG: 此处使用 plain target_link_libraries；若同一目标随后经
    # zr_link_third_party_for_target 使用 PRIVATE 签名，CMake 配置报签名混用错误。
    # 复现：依次对 app 调用本函数、对 app_executable 调用第三方链接函数。
    if (BUILD_STATIC_LIB)
        target_include_directories(${zr_module_executable} PRIVATE ${CMAKE_SOURCE_DIR}/${library_name}/include)
        target_link_libraries(${zr_module_executable} ${library_name}_static)
    elseif (BUILD_SHARED_LIB)
        target_include_directories(${zr_module_executable} PRIVATE ${CMAKE_SOURCE_DIR}/${library_name}/include)
        target_link_libraries(${zr_module_executable} ${library_name}_shared)
    endif ()
endfunction()

# 安装已声明的模块变体，并向其公共使用者传播模块名编译定义。
# 模块各自的 CMakeLists 在依赖配置完成后调用此入口。
# TODO: 全仓尚未发现按模块名宏条件编译的消费方；复核 PUBLIC 传播是否仍属必要安装契约。
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
