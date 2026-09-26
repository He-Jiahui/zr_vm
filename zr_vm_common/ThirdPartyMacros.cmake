# 第三方目标采用全仓唯一名称，避免不同模块重复编译同一个依赖。
# 输出写入调用方作用域，供注册者和链接者使用相同目标名。
function(zr_get_third_party_target_name out_var library_name)
    set(${out_var} "zr_third_party_${library_name}_static" PARENT_SCOPE)
endfunction()

# 由依赖源码所在模块注册一次静态目标；多处 include/调用不应重复创建目标。
# owner 属性标明维护归属，PIC 和隐藏符号让该静态库可安全嵌入共享模块。
function(zr_register_owned_third_party owner_module library_name)
    zr_get_third_party_target_name(zr_third_party_target ${library_name})

    if (TARGET ${zr_third_party_target})
        return()
    endif ()

    add_library(${zr_third_party_target} STATIC ${ARGN})
    target_include_directories(${zr_third_party_target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    set_target_properties(${zr_third_party_target} PROPERTIES
            OUTPUT_NAME ${library_name}
            POSITION_INDEPENDENT_CODE ON
            C_VISIBILITY_PRESET "hidden"
            VISIBILITY_INLINES_HIDDEN ON
    )

    set_property(TARGET ${zr_third_party_target} PROPERTY ZR_THIRD_PARTY_OWNER ${owner_module})
endfunction()

# 将已注册的第三方目标接入模块的所有启用变体；缺失注册时在配置期失败。
# 被依赖目标必须先经 zr_register_owned_third_party 创建。
function(zr_link_third_party_for_module module_name library_name)
    zr_get_third_party_target_name(zr_third_party_target ${library_name})

    if (NOT TARGET ${zr_third_party_target})
        message(FATAL_ERROR "Third-party target ${zr_third_party_target} is not registered before linking ${module_name}.")
    endif ()

    if (BUILD_STATIC_LIB)
        target_link_libraries(${module_name}_static PRIVATE ${zr_third_party_target})
    endif ()

    if (BUILD_SHARED_LIB)
        target_link_libraries(${module_name}_shared PRIVATE ${zr_third_party_target})
    endif ()
endfunction()

# 测试、LSP 独立目标和 WASM 目标通过此入口复用同一第三方注册目标。
# 与模块入口相同，调用时目标应已存在且依赖必须预先注册。
function(zr_link_third_party_for_target target_name library_name)
    zr_get_third_party_target_name(zr_third_party_target ${library_name})

    if (NOT TARGET ${zr_third_party_target})
        message(FATAL_ERROR "Third-party target ${zr_third_party_target} is not registered before linking ${target_name}.")
    endif ()

    target_link_libraries(${target_name} PRIVATE ${zr_third_party_target})
endfunction()
