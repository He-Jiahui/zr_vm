# ZR-VM Tests CMakeLists.txt
# 所有测试模块都在各自的子目录下

# GC 测试
add_subdirectory(gc)

# Parser 测试（如果构建了 parser 模块）
if(TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static)
    add_subdirectory(parser)
endif()

# Meta 测试（如果构建了 core 模块）
if(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static)
    add_subdirectory(meta)
endif()

# Instructions 测试（如果构建了 core 模块）
if(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static)
    add_subdirectory(instructions)
endif()

# Scripts 综合测试（如果构建了 parser 和 core 模块）
if((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND 
   (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    add_subdirectory(scripts)
endif()

# Exceptions 测试（如果构建了 parser 和 core 模块）
if((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND 
   (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    add_subdirectory(exceptions)
endif()

# Module System 测试（如果构建了 parser 和 core 模块）
if((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND 
   (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    add_subdirectory(module)
endif()

# Named Arguments 测试（如果构建了 parser 和 core 模块）
if((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND 
   (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    add_subdirectory(function)
endif()

# Compile-Time Execution 测试（如果构建了 parser 和 core 模块）
if((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND 
   (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    add_subdirectory(compileTime)
endif()

# Language Server 测试（如果构建了 language_server 模块）
if(TARGET zr_vm_language_server_shared OR TARGET zr_vm_language_server_static)
    if((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND 
       (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
        add_subdirectory(language_server)
    endif()
endif()

# 测试运行器（整合所有测试）
if(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static)
    # 添加测试运行器源文件
    set(TEST_RUNNER_SOURCES
        test_runner.c
    )
    
    # 创建测试运行器可执行文件
    add_executable(zr_vm_test_runner ${TEST_RUNNER_SOURCES})
    
    # 包含头文件目录
    target_include_directories(zr_vm_test_runner PRIVATE 
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
    
    # 链接必要的库（如果需要）
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_test_runner PRIVATE zr_vm_core_shared)
    else()
        target_link_libraries(zr_vm_test_runner PRIVATE zr_vm_core_static)
    endif()
    
    # 添加测试目标
    enable_testing()
    add_test(NAME test_runner COMMAND zr_vm_test_runner)
endif()
