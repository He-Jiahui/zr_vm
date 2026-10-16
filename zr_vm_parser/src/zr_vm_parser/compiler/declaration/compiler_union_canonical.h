#ifndef ZR_VM_PARSER_COMPILER_UNION_CANONICAL_H
#define ZR_VM_PARSER_COMPILER_UNION_CANONICAL_H

#include "zr_vm_parser/compiler.h"

/**
 * @brief 将 union 声明的 canonical 定义投影与语义类型符号登记到编译上下文。
 * @pre node 为 union 声明，prototype 已收集与该声明对应的名称和泛型参数元数据。
 * @return 规范 payload、定义投影和语义类型符号均成功登记时返回 true；校验或语义登记失败时返回 false。
 * @note 由 union 声明编译路径调用；AST 与 prototype 仍由调用方持有。
 * @note BUG: 合法 union 的带字段 payload、含泛型参数的 binding/kind 或非空 variant 数组若 Init OOM，底层仍置正 capacity 与有效状态而 head 为空；
 * 随后的 void Push 在断言开启时终止，关闭时通过 RawCopy 写向空地址，无法按 false 返回。可达输入见
 * tests/parser/test_canonical_type_graph_union_cases.h:48-54；底层见 zr_vm_core/include/zr_vm_core/array.h:36-42、74-88，RawCopy 实现见 zr_vm_core/include/zr_vm_core/memory.h:101；
 * 断言配置见 zr_vm_common/include/zr_vm_common/zr_common_conf.h:105-107。
 */
TZrBool compiler_union_register_canonical_type(
        SZrCompilerState *cs,
        SZrAstNode *node,
        const SZrTypePrototypeInfo *prototype);

#endif // ZR_VM_PARSER_COMPILER_UNION_CANONICAL_H
