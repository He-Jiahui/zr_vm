include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_language_server")
# 由根工程启用语言服务器时，为首方源码创建与全局选项一致的静态/共享库。
zr_declare_module(${zr_curr_module_name} ON)
# 核心依赖随模块库形态链接；可选 network/thread 只在对应目标存在时开放能力宏。
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_parser")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_system")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_math")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_container")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_ffi")
if (TARGET zr_vm_lib_network_shared OR TARGET zr_vm_lib_network_static)
    zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_network")
# network 能力宏与实际链接目标同门控，供接口层决定是否暴露网络能力。
    if (TARGET ${zr_curr_module_name}_shared)
        target_compile_definitions(${zr_curr_module_name}_shared PRIVATE
            ZR_VM_LSP_HAS_NETWORK_LIB=1
        )
    endif()

    if (TARGET ${zr_curr_module_name}_static)
        target_compile_definitions(${zr_curr_module_name}_static PRIVATE
            ZR_VM_LSP_HAS_NETWORK_LIB=1
        )
    endif()
endif()
if (TARGET zr_vm_thread_shared OR TARGET zr_vm_thread_static)
    zr_link_library_for_module(${zr_curr_module_name} "zr_vm_thread")
# thread 能力宏与实际链接目标同门控，供接口层决定是否暴露线程能力。
    if (TARGET ${zr_curr_module_name}_shared)
        target_compile_definitions(${zr_curr_module_name}_shared PRIVATE
            ZR_VM_LSP_HAS_THREAD_LIB=1
        )
    endif()

    if (TARGET ${zr_curr_module_name}_static)
        target_compile_definitions(${zr_curr_module_name}_static PRIVATE
            ZR_VM_LSP_HAS_THREAD_LIB=1
        )
    endif()
endif()
zr_link_third_party_for_module(${zr_curr_module_name} "zr_c_json")
# LSP 查询借用 parser 内部设施；这些私有目录只授予本模块目标和下方 WASM 目标。
set(_zr_vm_ls_parser_private_includes
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/parser
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/writer
)
if (TARGET zr_vm_language_server_shared)
    target_include_directories(zr_vm_language_server_shared PRIVATE ${_zr_vm_ls_parser_private_includes})
endif ()
if (TARGET zr_vm_language_server_static)
    target_include_directories(zr_vm_language_server_static PRIVATE ${_zr_vm_ls_parser_private_includes})
endif ()
# 模块变体建立并链接完毕后，按统一首方规则导出安装目标。
zr_install_module(${zr_curr_module_name})
# stdio 是可关闭的原生协议入口；其 CTest 由 tests/CMakeLists.txt 按目标存在性注册。
option(BUILD_LANGUAGE_SERVER_STDIO "Build native stdio LSP server" ON)

if(BUILD_LANGUAGE_SERVER_STDIO)
    add_executable(zr_vm_language_server_stdio
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/zr_vm_language_server_stdio.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_server.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_transport.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_frame_reader.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_request_registry.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_json.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_json_rpc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_lsp_memory.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_lsp_parse.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_position_encoding.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_document_file.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_document_content.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_documents.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_completion.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_completion_json.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_editing.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_editing_json.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_editor_features.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_editor_features_json.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_diagnostics.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_diagnostic_json.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_initialize.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_initialize_capabilities.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_navigation.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_navigation_json.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_project.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_rename.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_hierarchy.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_linked_editing.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_inline_completion.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_inline_value.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_inline_value_scan.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_inline_value_semantic_text.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_moniker.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_semantic_tokens_cache.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_semantic_tokens.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_semantic_tokens_json.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_workspace_files.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_request_dispatch.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_request_progress.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_requests.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stdio/stdio_lifecycle.c
    )
# stdio 直接编译协议适配层，借用本模块 API 和 parser/core 类型。
    target_include_directories(zr_vm_language_server_stdio PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_language_server
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
# stdio 入口选择已创建的模块库变体，避免跨静态/共享形态链接。
    if(TARGET zr_vm_language_server_shared)
        target_link_libraries(zr_vm_language_server_stdio PRIVATE
            zr_vm_language_server_shared
            zr_vm_parser_shared
            zr_vm_core_shared
        )
    elseif(TARGET zr_vm_language_server_static)
        target_link_libraries(zr_vm_language_server_stdio PRIVATE
            zr_vm_language_server_static
            zr_vm_parser_static
            zr_vm_core_static
        )
    endif()

    zr_link_third_party_for_target(zr_vm_language_server_stdio "zr_c_json")
endif()

# WASM 仅用于 Emscripten 构建；普通工具链启用选项时不会创建浏览器目标。
option(BUILD_WASM "Build WASM version of language server using Emscripten" OFF)

if(BUILD_WASM)
    if(NOT CMAKE_CXX_COMPILER_LOADED)
        enable_language(CXX)
    endif()

# 目标门控检查工具链身份；仅 Clang 名称不足以判定 Emscripten。
# 工具链不匹配时走末尾警告分支，上游不可无条件依赖 *_wasm 目标。
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
        
# 浏览器目标将 LSP 实现和首方依赖源码合入同一模块，以保留跨语言 ABI。
        file(GLOB_RECURSE zr_module_src
            RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
            src/*.c
            wasm/*.cpp
        )
        
# core 与下列首方库源码直接编入 WASM，不依赖宿主静态库产物。
        file(GLOB_RECURSE zr_vm_core_src
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/*.c
        )
        
# parser 源码同样进入 WASM，提供语义分析与查询层。
        file(GLOB_RECURSE zr_vm_parser_src
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/**/*.c
        )

        file(GLOB_RECURSE zr_vm_library_src
            ${CMAKE_SOURCE_DIR}/zr_vm_library/src/*.c
        )

        file(GLOB_RECURSE zr_vm_lib_system_src
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/src/*.c
        )

        file(GLOB_RECURSE zr_vm_lib_math_src
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/src/*.c
        )

        file(GLOB_RECURSE zr_vm_lib_container_src
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/src/*.c
        )

        file(GLOB_RECURSE zr_vm_lib_iteration_src
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_iteration/src/*.c
        )

        file(GLOB_RECURSE zr_vm_lib_ffi_src
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/src/*.c
        )
        
# TODO: 这些路径用任意 test 子串过滤；复核是否误排生产源码及 parser 的不同过滤策略。
        list(FILTER zr_vm_core_src EXCLUDE REGEX ".*test.*")
        list(FILTER zr_vm_library_src EXCLUDE REGEX ".*test.*")
        list(FILTER zr_vm_lib_system_src EXCLUDE REGEX ".*test.*")
        list(FILTER zr_vm_lib_math_src EXCLUDE REGEX ".*test.*")
        list(FILTER zr_vm_lib_container_src EXCLUDE REGEX ".*test.*")
        list(FILTER zr_vm_lib_iteration_src EXCLUDE REGEX ".*test.*")
        list(FILTER zr_vm_lib_ffi_src EXCLUDE REGEX ".*test.*")
        
# WASM 可执行目标承载 C/C++ 导出胶水与首方运行时源码。
        add_executable(${zr_curr_module_name}_wasm 
            ${zr_vm_common_src} 
            ${zr_module_src}
            ${CMAKE_CURRENT_SOURCE_DIR}/wasm/wasm_response.c
            ${zr_vm_core_src}
            ${zr_vm_parser_src}
            ${zr_vm_library_src}
            ${zr_vm_lib_system_src}
            ${zr_vm_lib_math_src}
            ${zr_vm_lib_container_src}
            ${zr_vm_lib_iteration_src}
            ${zr_vm_lib_ffi_src}
        )
        
# 公共头暴露 LSP ABI；私有目录仅服务此目标内部跨模块编译。
        target_include_directories(${zr_curr_module_name}_wasm PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_include_directories(${zr_curr_module_name}_wasm PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core
            ${CMAKE_SOURCE_DIR}/zr_vm_library/src/zr_vm_library
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/src/zr_vm_lib_system
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/src/zr_vm_lib_ffi
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_iteration/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
            ${_zr_vm_ls_parser_private_includes}
        )
        
# 导出列表是浏览器 bridge 的可调用 ABI，需与 wasm_exports.cpp 保持一致。
# 同时导出 libc 分配释放入口，供 bridge 管理返回的 JSON 缓冲区。
# 新增接口时同步检查 JS 绑定与列表，避免运行时找不到符号。
        set(EXPORTED_FUNCTIONS_JSON "[\"_malloc\",\"_free\",\"_wasm_malloc\",\"_wasm_free\",\"_wasm_ZrLspContextNew\",\"_wasm_ZrLspContextFree\",\"_wasm_ZrLspUpdateDocument\",\"_wasm_ZrLspCloseDocument\",\"_wasm_ZrLspGetDiagnostics\",\"_wasm_ZrLspGetDiagnosticReport\",\"_wasm_ZrLspGetWorkspaceDiagnosticReports\",\"_wasm_ZrLspGetCompletion\",\"_wasm_ZrLspGetHover\",\"_wasm_ZrLspGetRichHover\",\"_wasm_ZrLspGetDefinition\",\"_wasm_ZrLspFindReferences\",\"_wasm_ZrLspRename\",\"_wasm_ZrLspGetDocumentSymbols\",\"_wasm_ZrLspGetInlayHints\",\"_wasm_ZrLspGetWorkspaceSymbols\",\"_wasm_ZrLspGetNativeDeclarationDocument\",\"_wasm_ZrLspGetProjectModules\",\"_wasm_ZrLspGetDocumentHighlights\",\"_wasm_ZrLspGetSemanticTokens\",\"_wasm_ZrLspPrepareRename\",\"_wasm_ZrLspGetFormatting\",\"_wasm_ZrLspGetRangeFormatting\",\"_wasm_ZrLspGetCodeActions\",\"_wasm_ZrLspGetFoldingRanges\",\"_wasm_ZrLspGetSelectionRange\",\"_wasm_ZrLspGetDocumentLinks\",\"_wasm_ZrLspGetCodeLens\"]")
        
# WASM 优化与 C/C++ 兼容参数仅作用于浏览器目标。
        target_compile_options(${zr_curr_module_name}_wasm PRIVATE
            "$<IF:$<CONFIG:Debug>,-O0,-O2>"
            -DZR_WASM_BUILD
            -DZR_CURRENT_MODULE="${zr_curr_module_name}"
            -Wno-gnu-anonymous-struct
            -fpermissive
            -Wno-error
            -Wno-c++11-narrowing
        )
        
# 导出胶水单独放宽 C++ 兼容选项，供 C API 包装在同一目标内编译。
        set_source_files_properties(
            wasm/wasm_exports.cpp
            PROPERTIES
            COMPILE_FLAGS "-fpermissive -Wno-error -Wno-c++11-narrowing"
        )
        
# 浏览器加载器依赖 MODULARIZE、导出运行时方法和无 main 的链接形态。
        target_link_options(${zr_curr_module_name}_wasm PRIVATE
            "SHELL:-s WASM=1"
            "SHELL:-s ALLOW_MEMORY_GROWTH=1"
            "SHELL:-s ENVIRONMENT=web,worker"
            "SHELL:-s EXPORTED_FUNCTIONS=${EXPORTED_FUNCTIONS_JSON}"
            "SHELL:-s EXPORTED_RUNTIME_METHODS=[\"UTF8ToString\",\"stringToUTF8\",\"ccall\",\"cwrap\"]"
            "SHELL:-s MODULARIZE=1"
            "SHELL:-s EXPORT_NAME=createZrLanguageServerModule"
            "SHELL:--no-entry"
        )
        
# 显式标记 WASM 编译路径，供源码选择浏览器约束与导出行为。
        target_compile_definitions(${zr_curr_module_name}_wasm PRIVATE
            ZR_WASM_BUILD
            ZR_CURRENT_MODULE="${zr_curr_module_name}"
            __EMSCRIPTEN__
        )
        
# 扩展脚本消费 wasm 目录中的固定模块名；改名需同步打包入口。
        set_target_properties(${zr_curr_module_name}_wasm PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/wasm
            OUTPUT_NAME ${zr_curr_module_name}
        )
        
# 首方依赖已通过上方源码列表并入 WASM；此处链接第三方移植库。
# TODO: 第三方库是否全部可在 Emscripten 下链接，需用真实工具链配置验证。




        zr_link_third_party_for_target(${zr_curr_module_name}_wasm "zr_c_json")
        zr_link_third_party_for_target(${zr_curr_module_name}_wasm "zr_miniz")
        zr_link_third_party_for_target(${zr_curr_module_name}_wasm "zr_xx_hash")
        zr_link_third_party_for_target(${zr_curr_module_name}_wasm "zr_utf8proc")
        zr_link_third_party_for_target(${zr_curr_module_name}_wasm "zr_tiny_dir")
        
        message(STATUS "WASM target ${zr_curr_module_name}_wasm configured for Emscripten")
        message(STATUS "  Output directory: ${CMAKE_BINARY_DIR}/wasm")
        message(STATUS "  Functions are marked with EMSCRIPTEN_KEEPALIVE in wasm_exports.cpp")
        message(STATUS "  Exported functions: ${EXPORTED_FUNCTIONS_JSON}")
        # 非 Emscripten 工具链只发出警告，不提供 *_wasm 目标。
    else()
        message(WARNING "WASM build requires Emscripten compiler (emcc)")
        message(WARNING "  Current compiler: ${CMAKE_C_COMPILER}")
        message(WARNING "  Compiler ID: ${CMAKE_C_COMPILER_ID}")
        message(WARNING "  System name: ${CMAKE_SYSTEM_NAME}")
        message(WARNING "  To use Emscripten, set CMAKE_TOOLCHAIN_FILE to Emscripten.cmake")
        message(WARNING "  Example: cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/emscripten/cmake/Modules/Platform/Emscripten.cmake ..")
    endif()
endif()
