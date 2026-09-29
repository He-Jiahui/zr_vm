#include "compile_tool_binding.h"

#include <string.h>

/**
 * @brief 将 provider 或 shadow 追加到词法绑定栈；Resolve 按逆序查找以实现最近声明遮蔽。
 * @note name、provider、hash 与 artifact 都是借用指针，其存储必须覆盖绑定作用域。
 * TODO: 绑定表不复制或显式 root name；需确认 AST/调用方引用是否覆盖所有延迟查询。
 */
static TZrBool compile_tool_binding_declare(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider,
        const TZrChar *providerContentHash,
        const SZrParserCompileToolResolvedArtifact *resolvedArtifact,
        EZrCompileToolBindingKind kind) {
    SZrCompileToolBinding binding;

    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    binding.name = name;
    binding.provider = provider;
    binding.providerContentHash = providerContentHash;
    binding.resolvedArtifact = resolvedArtifact;
    binding.kind = kind;
    // BUG: Array_Push 扩容分配失败后仍会向空 head 写入；该路径由 array.h 的实现确认。
    ZrCore_Array_Push(cs->state, &cs->compileToolBindings, &binding);
    return ZR_TRUE;
}

/** @brief 编译单元开始时清空临时绑定并重置阶段；不释放 provider 所有的借用对象。 */
void ZrParser_CompileToolBinding_Reset(SZrCompilerState *cs) {
    if (cs != ZR_NULL) {
        cs->compileToolBindings.length = 0;
        cs->compilePhase = ZR_PARSER_COMPILE_PHASE_BUILD_FACTS;
    }
}

/** @brief 记录当前绑定栈长度，供 compile-time 求值和 provider 注册失败回滚。 */
TZrSize ZrParser_CompileToolBinding_Mark(const SZrCompilerState *cs) {
    return cs != ZR_NULL ? cs->compileToolBindings.length : 0;
}

/** @brief 将绑定栈截回有效 mark；忽略越过当前栈顶的 mark 以免制造虚假条目。 */
void ZrParser_CompileToolBinding_Restore(SZrCompilerState *cs, TZrSize mark) {
    if (cs != ZR_NULL && mark <= cs->compileToolBindings.length) {
        cs->compileToolBindings.length = mark;
    }
}

/** @brief 为已知内建 compile-tool 描述符登记 provider，供导入与属性绑定解析。 */
TZrBool ZrParser_CompileToolBinding_DeclareProvider(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider) {
    return ZrParser_CompileToolBinding_DeclareProviderWithContentHash(
            cs, name, provider, ZR_NULL);
}

/** @brief 登记带内容身份的 provider；哈希字符串由调用者持有并覆盖绑定生命周期。 */
TZrBool ZrParser_CompileToolBinding_DeclareProviderWithContentHash(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider,
        const TZrChar *providerContentHash) {
    return provider != ZR_NULL &&
           compile_tool_binding_declare(
                   cs,
                   name,
                   provider,
                   providerContentHash,
                   ZR_NULL,
                   ZR_COMPILE_TOOL_BINDING_PROVIDER);
}

/** @brief 仅接受合同哈希相符且已打开的项目 artifact，供 project provider 导入事务使用。 */
TZrBool ZrParser_CompileToolBinding_DeclareResolvedProvider(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider,
        const SZrParserCompileToolResolvedArtifact *resolvedArtifact) {
    if (provider == ZR_NULL ||
        !ZrParser_CompileToolArtifact_IsOpen(resolvedArtifact) ||
        provider->providerPhase != ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL ||
        resolvedArtifact->providerPhase != ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL ||
        provider->publicContractHash == ZR_NULL ||
        strcmp(provider->publicContractHash, resolvedArtifact->publicContractHash) != 0 ||
        resolvedArtifact->artifactContentHash[0] == '\0') {
        return ZR_FALSE;
    }

    return compile_tool_binding_declare(
            cs,
            name,
            provider,
            resolvedArtifact->artifactContentHash,
            resolvedArtifact,
            ZR_COMPILE_TOOL_BINDING_PROVIDER);
}

/** @brief 登记局部变量对 compile-tool alias 的遮蔽，使词法最近绑定规则一致。 */
TZrBool ZrParser_CompileToolBinding_DeclareShadow(SZrCompilerState *cs, SZrString *name) {
    return compile_tool_binding_declare(
            cs, name, ZR_NULL, ZR_NULL, ZR_NULL, ZR_COMPILE_TOOL_BINDING_SHADOW);
}

/** @brief 从最近作用域向外解析 alias；返回数组内借用地址，仅在栈未重配/截断前有效。 */
const SZrCompileToolBinding *ZrParser_CompileToolBinding_Resolve(
        const SZrCompilerState *cs,
        SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = cs->compileToolBindings.length; index > 0; index--) {
        const SZrCompileToolBinding *binding =
                (const SZrCompileToolBinding *)ZrCore_Array_Get(
                        (SZrArray *)&cs->compileToolBindings,
                        index - 1);
        if (binding != ZR_NULL && binding->name != ZR_NULL &&
            ZrCore_String_Equal(binding->name, name)) {
            return binding;
        }
    }

    return ZR_NULL;
}
