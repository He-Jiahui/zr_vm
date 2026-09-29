#ifndef ZR_VM_PARSER_COMPILE_TOOL_BINDING_H
#define ZR_VM_PARSER_COMPILE_TOOL_BINDING_H

#include "compiler_internal.h"

/** @brief 清空 compiler 的 compile-tool 临时绑定并重置其阶段状态。 */
ZR_PARSER_API void ZrParser_CompileToolBinding_Reset(SZrCompilerState *cs);
/** @brief 保存绑定栈深度，供嵌套求值和失败回滚。 */
TZrSize ZrParser_CompileToolBinding_Mark(const SZrCompilerState *cs);
/** @brief 恢复到不大于当前深度的有效 mark。 */
void ZrParser_CompileToolBinding_Restore(SZrCompilerState *cs, TZrSize mark);
/** @brief 登记静态 provider；name 和 provider 描述符均借用，须长于 compiler state。
 *  TODO: name 是否由 AST/调用方稳定 root 覆盖所有延迟查询仍待确认。
 */
ZR_PARSER_API TZrBool ZrParser_CompileToolBinding_DeclareProvider(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider);
/** @brief 登记带内容 hash 的静态 provider；name、hash 和 provider 均由调用方借出并保证生命周期。
 *  TODO: name 是否由 AST/调用方稳定 root 覆盖所有延迟查询仍待确认。
 */
ZR_PARSER_API TZrBool ZrParser_CompileToolBinding_DeclareProviderWithContentHash(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider,
        const TZrChar *providerContentHash);
/** @brief 登记已打开项目 artifact provider；name/provider/artifact 均借用，artifact 关闭前不得继续查询。
 *  TODO: name 是否由 AST/调用方稳定 root 覆盖所有延迟查询仍待确认。
 */
ZR_PARSER_API TZrBool ZrParser_CompileToolBinding_DeclareResolvedProvider(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider,
        const SZrParserCompileToolResolvedArtifact *resolvedArtifact);
/** @brief 登记遮蔽 provider alias 的局部绑定；name 不复制并须覆盖当前绑定作用域。
 *  TODO: AST 对 name 的保活是否覆盖所有延迟查询仍待确认。
 */
TZrBool ZrParser_CompileToolBinding_DeclareShadow(SZrCompilerState *cs, SZrString *name);
/**
 * @brief 按最近声明优先解析 compile-tool alias。
 * @return 返回绑定数组内借用指针；后续 Push/Restore/Reset 后不得继续使用。
 */
const SZrCompileToolBinding *ZrParser_CompileToolBinding_Resolve(
        const SZrCompilerState *cs,
        SZrString *name);

#endif
