# ZR-VM Tests CMakeLists.txt
# 所有测试模块都在各自的子目录下

# GC 测试
add_subdirectory(gc)

# Parser 测试（如果构建了 parser 模块）
if(TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static)
    add_subdirectory(parser)
endif()
