//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_AST_CONSTANTS_H
#define ZR_AST_CONSTANTS_H

#include "zr_vm_common/zr_common_conf.h"

// AST节点类型常量（用于序列化数据，避免依赖parser模块）
// 这些值必须与zr_vm_parser/ast.h中的EZrAstNodeType枚举值保持一致
#define ZR_AST_CONSTANT_STRUCT_FIELD 33
#define ZR_AST_CONSTANT_STRUCT_METHOD 34
#define ZR_AST_CONSTANT_STRUCT_META_FUNCTION 35
#define ZR_AST_CONSTANT_CLASS_FIELD 38
#define ZR_AST_CONSTANT_CLASS_METHOD 39
#define ZR_AST_CONSTANT_CLASS_PROPERTY 40
#define ZR_AST_CONSTANT_CLASS_META_FUNCTION 41

// 访问修饰符常量（用于序列化数据，避免依赖parser模块）
// 这些值必须与zr_vm_parser/ast.h中的EZrAccessModifier枚举值保持一致
#define ZR_ACCESS_CONSTANT_PUBLIC 0
#define ZR_ACCESS_CONSTANT_PRIVATE 1
#define ZR_ACCESS_CONSTANT_PROTECTED 2

#endif // ZR_AST_CONSTANTS_H
