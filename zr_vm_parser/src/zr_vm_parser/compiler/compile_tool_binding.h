#ifndef ZR_VM_PARSER_COMPILE_TOOL_BINDING_H
#define ZR_VM_PARSER_COMPILE_TOOL_BINDING_H

#include "compiler_internal.h"

/**
 * @brief 清空当前 CompileTool 临时绑定并把编译阶段设回 build-facts。
 * @note 仅清空逻辑长度并保留数组缓冲区；记录内借用的 provider、hash 和 artifact 由各自所有者管理。cs 为空时无操作。
 */
ZR_PARSER_API void ZrParser_CompileToolBinding_Reset(SZrCompilerState *cs);
/**
 * @brief 保存当前绑定栈深度，供嵌套求值和 provider 导入事务设置回滚点。
 * @pre cs 非空时必须是已初始化的 compiler state。
 * @return cs 为空时返回 0，否则返回当前绑定数；mark 适用于同一绑定栈中随后追加的记录。
 */
TZrSize ZrParser_CompileToolBinding_Mark(const SZrCompilerState *cs);
/**
 * @brief 截去 mark 之后的绑定，恢复外层词法范围。
 * @pre cs 非空时必须是已初始化的 compiler state；mark 应取自同一绑定栈的较早状态。
 * @note 只缩短逻辑长度，不释放记录中的借用对象；cs 为空或 mark 超过当前长度时无操作。
 */
void ZrParser_CompileToolBinding_Restore(SZrCompilerState *cs, TZrSize mark);
/**
 * @brief 登记已知 provider alias，供编译期表达式、CompileTool 导入及类型和属性绑定解析。
 * @pre cs 非空时其 compileToolBindings 已初始化。
 * @pre name 与 provider 描述符由调用方借出，必须在绑定可见期间保持有效。
 * @return cs、name 或 provider 为空时返回 ZR_FALSE；扩容分配失败的行为见实现中的 BUG 注释。
 */
ZR_PARSER_API TZrBool ZrParser_CompileToolBinding_DeclareProvider(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider);
/**
 * @brief 登记已知 provider 及可选的内容 hash，使缓存键区分相同公开合同下的不同内容。
 * @pre cs 非空时其 compileToolBindings 已初始化。
 * @pre name、provider 描述符和非空 hash 均由调用方借出，并覆盖绑定可见期。
 * @note hash 为 ZR_NULL 时不添加内容身份，缓存仍读取 provider 的公开合同身份。
 * @return cs、name 或 provider 为空时返回 ZR_FALSE；扩容分配失败的行为见实现中的 BUG 注释。
 */
ZR_PARSER_API TZrBool ZrParser_CompileToolBinding_DeclareProviderWithContentHash(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider,
        const TZrChar *providerContentHash);
/**
 * @brief 登记与已打开项目 artifact 的 CompileTool 合同匹配的 provider alias。
 * @pre cs 非空时其 compileToolBindings 已初始化。
 * @pre provider 与 artifact 阶段相符、公开合同 hash 一致且 artifact 内容 hash 非空；所有借用对象在绑定可查询期间保持有效。
 * @return 输入无效、artifact 已关闭或合同不匹配时返回 ZR_FALSE，且不追加记录；artifact 仍由 project provider 所有。
 */
ZR_PARSER_API TZrBool ZrParser_CompileToolBinding_DeclareResolvedProvider(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider,
        const SZrParserCompileToolResolvedArtifact *resolvedArtifact);
/**
 * @brief 登记局部名称的 shadow 绑定，使最近的局部声明遮蔽同名 provider alias。
 * @pre cs 非空时其 compileToolBindings 已初始化。
 * @pre name 由调用方借出并在绑定可见期间保持有效。
 * @return cs 或 name 为空时返回 ZR_FALSE；扩容分配失败的行为见实现中的 BUG 注释。
 */
TZrBool ZrParser_CompileToolBinding_DeclareShadow(SZrCompilerState *cs, SZrString *name);
/**
 * @brief 按最近声明优先解析 CompileTool alias；结果可能是 provider，也可能是遮蔽用的 shadow。
 * @pre cs 非空时其 compileToolBindings 已初始化。
 * @return cs 或 name 为空、或未找到名称时返回 ZR_NULL。
 * @note 返回绑定数组内的借用指针；后续追加可能扩容，Restore 或 Reset 截断记录，因此这些操作后不得继续使用。
 */
const SZrCompileToolBinding *ZrParser_CompileToolBinding_Resolve(
        const SZrCompilerState *cs,
        SZrString *name);

#endif
