include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_language_server")

zr_declare_module(${zr_curr_module_name} ON)

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_parser")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_include_third_party_for_module(${zr_curr_module_name} "zr_c_json")

zr_install_module(${zr_curr_module_name})

# WASM 编译支持（使用 Emscripten）
option(BUILD_WASM "Build WASM version of language server using Emscripten" OFF)

if(BUILD_WASM)
    # 检查是否为 Emscripten 编译器
    # Emscripten 的编译器 ID 是 "Clang"，但可以通过检查 CMAKE_C_COMPILER 路径或系统名称来判断
    set(IS_EMSCRIPTEN FALSE)
    
    if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        set(IS_EMSCRIPTEN TRUE)
    elseif(CMAKE_C_COMPILER MATCHES "emcc" OR CMAKE_C_COMPILER MATCHES "emscripten")
        set(IS_EMSCRIPTEN TRUE)
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        # 检查编译器路径是否包含 emscripten
        get_filename_component(COMPILER_DIR ${CMAKE_C_COMPILER} DIRECTORY)
        if(COMPILER_DIR MATCHES "emscripten")
            set(IS_EMSCRIPTEN TRUE)
        endif()
    endif()
    
    if(IS_EMSCRIPTEN)
        message(STATUS "Building for Emscripten/WASM using emcc")
        
        # 收集当前模块的源文件
        file(GLOB_RECURSE zr_module_src
            RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
            src/*.c
            wasm/*.cpp
        )
        
        # 收集 zr_vm_core 的源文件（直接包含到 WASM 构建中）
        file(GLOB_RECURSE zr_vm_core_src
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/*.c
        )
        
        # 收集 zr_vm_parser 的源文件（直接包含到 WASM 构建中）
        file(GLOB_RECURSE zr_vm_parser_src
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/*.c
        )
        
        # 收集第三方库的源文件
        set(zr_xx_hash_src ${CMAKE_SOURCE_DIR}/third_party/zr_xx_hash/xxHash/xxhash.c)
        
        # 排除测试文件
        list(FILTER zr_vm_core_src EXCLUDE REGEX ".*test.*")
        list(FILTER zr_vm_parser_src EXCLUDE REGEX ".*test.*")
        
        # 创建 WASM 可执行文件（包含所有依赖的源文件）
        add_executable(${zr_curr_module_name}_wasm 
            ${zr_vm_common_src} 
            ${zr_module_src}
            ${zr_vm_core_src}
            ${zr_vm_parser_src}
            ${zr_xx_hash_src}
        )
        
        # 设置包含目录
        target_include_directories(${zr_curr_module_name}_wasm PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_include_directories(${zr_curr_module_name}_wasm PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/third_party/zr_c_json
            ${CMAKE_SOURCE_DIR}/third_party/zr_xx_hash
        )
        
        # 使用 EMSCRIPTEN_KEEPALIVE + EXPORTED_FUNCTIONS 导出函数
        # 函数已在 wasm_exports.cpp 中使用 EMSCRIPTEN_KEEPALIVE 标记
        # 注意：函数名使用 wasm_ 前缀以避免与原始 LSP 函数冲突
        set(EXPORTED_FUNCTIONS_JSON "[\"_malloc\",\"_free\",\"_wasm_malloc\",\"_wasm_free\",\"_wasm_ZrLspContextNew\",\"_wasm_ZrLspContextFree\",\"_wasm_ZrLspUpdateDocument\",\"_wasm_ZrLspGetDiagnostics\",\"_wasm_ZrLspGetCompletion\",\"_wasm_ZrLspGetHover\",\"_wasm_ZrLspGetDefinition\",\"_wasm_ZrLspFindReferences\",\"_wasm_ZrLspRename\"]")
        
        # 设置 Emscripten 编译选项
        target_compile_options(${zr_curr_module_name}_wasm PRIVATE
            -O2
            -DZR_WASM_BUILD
            -DZR_CURRENT_MODULE="${zr_curr_module_name}"
            -Wno-gnu-anonymous-struct
            -fpermissive
            -Wno-error
            -Wno-c++11-narrowing
        )
        
        # 为 C++ 文件设置特殊选项（允许 C 兼容性）
        set_source_files_properties(
            wasm/wasm_exports.cpp
            PROPERTIES
            COMPILE_FLAGS "-fpermissive -Wno-error -Wno-c++11-narrowing"
        )
        
        # 设置 Emscripten 链接选项（-s 选项必须在链接时使用）
        target_link_options(${zr_curr_module_name}_wasm PRIVATE
            "SHELL:-s WASM=1"
            "SHELL:-s ALLOW_MEMORY_GROWTH=1"
            "SHELL:-s EXPORTED_FUNCTIONS=${EXPORTED_FUNCTIONS_JSON}"
            "SHELL:-s EXPORTED_RUNTIME_METHODS=[\"UTF8ToString\",\"stringToUTF8\",\"ccall\",\"cwrap\"]"
            "SHELL:-s MODULARIZE=1"
            "SHELL:-s EXPORT_NAME=createZrLanguageServerModule"
            "SHELL:--no-entry"
        )
        
        # 导出函数宏定义
        target_compile_definitions(${zr_curr_module_name}_wasm PRIVATE
            ZR_WASM_BUILD
            ZR_CURRENT_MODULE="${zr_curr_module_name}"
            __EMSCRIPTEN__
        )
        
        # 设置输出目录和文件名
        set_target_properties(${zr_curr_module_name}_wasm PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/wasm
            OUTPUT_NAME ${zr_curr_module_name}
        )
        
        # 链接依赖库（如果它们也支持 WASM 编译）
        # TODO: 需要确保依赖库也支持 WASM 编译
        # 目前依赖库需要单独编译为 WASM 或静态链接
        # 链接依赖库（WASM 构建需要静态链接所有依赖）
        # 注意：由于依赖库尚未编译为 WASM，暂时只链接 cJSON
        # TODO: 需要确保 zr_vm_parser 和 zr_vm_core 也编译为 WASM 版本
        target_link_libraries(${zr_curr_module_name}_wasm PRIVATE
            zr_c_json_static
        )
        
        message(STATUS "WASM target ${zr_curr_module_name}_wasm configured for Emscripten")
        message(STATUS "  Output directory: ${CMAKE_BINARY_DIR}/wasm")
        message(STATUS "  Functions are marked with EMSCRIPTEN_KEEPALIVE in wasm_exports.cpp")
        message(STATUS "  Exported functions: ${EXPORTED_FUNCTIONS_JSON}")
        
    else()
        message(WARNING "WASM build requires Emscripten compiler (emcc)")
        message(WARNING "  Current compiler: ${CMAKE_C_COMPILER}")
        message(WARNING "  Compiler ID: ${CMAKE_C_COMPILER_ID}")
        message(WARNING "  System name: ${CMAKE_SYSTEM_NAME}")
        message(WARNING "  To use Emscripten, set CMAKE_TOOLCHAIN_FILE to Emscripten.cmake")
        message(WARNING "  Example: cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/emscripten/cmake/Modules/Platform/Emscripten.cmake ..")
    endif()
endif()
