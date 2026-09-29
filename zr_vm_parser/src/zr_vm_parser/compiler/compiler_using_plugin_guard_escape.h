#ifndef ZR_VM_PARSER_COMPILER_USING_PLUGIN_GUARD_ESCAPE_H
#define ZR_VM_PARSER_COMPILER_USING_PLUGIN_GUARD_ESCAPE_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"

/**
 * @brief 检查单标识符 import using guard 的正文是否让插件绑定逃逸作用域。
 * @pre cs 已初始化；stmt 是已通过形状校验且 AST 在同步扫描期间存活的 import guard。
 * @return 发现逃逸时写入 compiler error 并返回 false；扫描通过时返回 true。
 * @note 仅 body 是插件绑定的有效范围；moduleName 借用 import AST 的字符串，临时名称集合由本次调用释放。
 */
TZrBool ZrParser_Compiler_ValidateUsingPluginGuardEscape(SZrCompilerState *cs, SZrUsingStatement *stmt);

/**
 * @brief 用已解析的 union payload 绑定扫描 import using guard 的正文。
 * @pre cs 已初始化；stmt 与非空 bindings 已由 guard resolver 校验；moduleName 在同步扫描结束前保持有效。
 * @return 发现逃逸时写入 compiler error 并返回 false；未发现时返回 true。
 * @note 仅可识别的裸绑定名会进入扫描集合；名称、moduleName 与 AST 均不转移所有权。
 */
TZrBool ZrParser_Compiler_ValidateUsingPluginGuardEscapeBindings(SZrCompilerState *cs,
                                                                  SZrUsingStatement *stmt,
                                                                  SZrAstNodeArray *bindings,
                                                                  SZrString *moduleName);

#endif
