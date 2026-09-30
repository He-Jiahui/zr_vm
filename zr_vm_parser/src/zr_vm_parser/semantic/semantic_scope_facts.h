/* 头文件保护宏确保本接口在一个翻译单元中只展开一次。 */
#ifndef ZR_VM_PARSER_SEMANTIC_SCOPE_FACTS_H
#define ZR_VM_PARSER_SEMANTIC_SCOPE_FACTS_H

#include "zr_vm_parser/semantic.h"

/**
 * @brief 从 AST 的词法边界和已有的已解析声明事实构建源码作用域事实。
 * @param context 当前语义快照；其中的符号与引用事实必须先于本接口就绪。
 * @param root 要遍历的脚本 AST；其节点与源码范围在事实被查询期间须保持有效。
 * @return 根节点无效或事实发布失败时返回 false；失败可能留下本次已追加的部分事实。
 * @note 本接口只投影绑定结果，不负责按名称解析变量；开始新快照前由调用方重置语义上下文。
 */
/* Builds source lexical scope facts from AST boundaries and resolved declaration facts. */
TZrBool ZrParser_Semantic_BuildSourceScopeFacts(
        SZrSemanticContext *context,
        SZrAstNode *root);

#endif /* ZR_VM_PARSER_SEMANTIC_SCOPE_FACTS_H */
