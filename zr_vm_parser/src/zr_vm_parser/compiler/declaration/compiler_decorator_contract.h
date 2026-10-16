#ifndef ZR_VM_PARSER_COMPILER_DECORATOR_CONTRACT_H
#define ZR_VM_PARSER_COMPILER_DECORATOR_CONTRACT_H

#include "zr_vm_parser/compiler.h"

/**
 * @brief 识别内建 FFI wrapper decorator 路径并返回静态叶名称。
 * @pre decoratorNode 为存活的 AST 节点；outHasCall 可为空。
 * @return 命中返回借用的静态字符串，未命中返回 NULL；该结果不验证参数合同。
 */
const TZrChar *ZrParser_DecoratorContract_BuiltinFfiWrapperLeafName(
        SZrAstNode *decoratorNode,
        TZrBool *outHasCall);

/**
 * @brief 快速判断 decorator 是否属于内建 FFI wrapper 名称集合。
 * @note 仅用于分类；参数和上下文约束由 wrapper binder 验证。
 */
TZrBool ZrParser_DecoratorContract_IsBuiltinFfiWrapper(
        SZrAstNode *decoratorNode);

/**
 * @brief 拒绝未被属性元数据或 comptime transform 接管的 runtime decorator。
 * @pre cs 已初始化，decorators 来自当前声明；allowBuiltinFfiWrapper 仅应由 class wrapper 路径启用。
 * @return true 表示可继续静态编译处理；false 表示发现错误或不支持的 runtime decorator。
 */
TZrBool ZrParser_DecoratorContract_ValidateNoRuntimeDecorators(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        TZrBool allowBuiltinFfiWrapper);

#endif // ZR_VM_PARSER_COMPILER_DECORATOR_CONTRACT_H
