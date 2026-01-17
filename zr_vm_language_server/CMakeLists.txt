include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_language_server")

zr_declare_module(${zr_curr_module_name} ON)

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_parser")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_include_third_party_for_module(${zr_curr_module_name} "zr_c_json")

zr_install_module(${zr_curr_module_name})

# WASM 编译支持
if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    # Emscripten 环境
    message(STATUS "Building for Emscripten/WASM")
    set_target_properties(${zr_curr_module_name}_static PROPERTIES
        COMPILE_FLAGS "-target wasm32-unknown-emscripten"
        LINK_FLAGS "-target wasm32-unknown-emscripten -s EXPORTED_FUNCTIONS='[\"_malloc\",\"_free\"]' -s ALLOW_MEMORY_GROWTH=1"
    )
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    # 检查是否支持 WASM 目标
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} --print-targets
        OUTPUT_VARIABLE CLANG_TARGETS
        ERROR_QUIET
    )
    
    if(CLANG_TARGETS MATCHES "wasm32")
        message(STATUS "Clang supports WASM target")
        option(BUILD_WASM "Build WASM version" OFF)
        
        if(BUILD_WASM)
            # 创建 WASM 目标
            add_library(${zr_curr_module_name}_wasm STATIC ${zr_vm_common_src})
            file(GLOB_RECURSE zr_module_src
                RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
                src/*.c
            )
            target_sources(${zr_curr_module_name}_wasm PRIVATE ${zr_module_src})
            
            # 设置 WASM 编译选项
            target_compile_options(${zr_curr_module_name}_wasm PRIVATE
                --target=wasm32
                -nostdlib
                -fno-exceptions
                -fno-rtti
                -Wl,--no-entry
                -Wl,--export-all
                -Wl,--allow-undefined
            )
            
            target_include_directories(${zr_curr_module_name}_wasm PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}/include
            )
            target_include_directories(${zr_curr_module_name}_wasm PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_common/include
            )
            
            # 链接依赖
            target_link_libraries(${zr_curr_module_name}_wasm PRIVATE
                zr_vm_parser_static
                zr_vm_core_static
            )
            
            # 导出函数（用于 JS 调用）
            target_compile_definitions(${zr_curr_module_name}_wasm PRIVATE
                ZR_WASM_BUILD
                ZR_CURRENT_MODULE="${zr_curr_module_name}"
            )
        endif()
    endif()
endif()
