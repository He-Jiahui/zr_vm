#include "compile_tool_binding.h"

#include <string.h>

/**
 * @brief 将 provider 或 shadow 作为一条新记录压入编译器的词法绑定栈。
 * @pre cs 非空时其 compileToolBindings 已初始化；name 为空时直接返回失败。
 * @note 记录只复制字段值；name、provider、hash 和 artifact 均为借用指针，必须覆盖绑定可见期。
 * TODO: 绑定表不复制或显式 root name；需核查 AST 与各调用方在后续 GC 和缓存键遍历期间是否持续保活。核查入口为编译器状态的根登记、脚本 AST 所有者及 provider 的释放顺序。
 * @return cs 或 name 为空时返回 ZR_FALSE；push 完成后返回 ZR_TRUE。扩容分配失败的行为见下方 BUG 注释。
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
    // BUG: 扩容分配失败时 Array_Push 仍向空指针复制，且不返回失败状态；OOM 会使编译进程崩溃，本函数也无法回报登记失败。
    ZrCore_Array_Push(cs->state, &cs->compileToolBindings, &binding);
    return ZR_TRUE;
}

/**
 * @brief 清空当前 CompileTool 绑定并把编译阶段设回 build-facts。
 * @note 仅将绑定长度置零并重置阶段；保留数组缓冲区，不释放记录中的借用对象。cs 为空时无操作。
 */
void ZrParser_CompileToolBinding_Reset(SZrCompilerState *cs) {
    if (cs != ZR_NULL) {
        cs->compileToolBindings.length = 0;
        cs->compilePhase = ZR_PARSER_COMPILE_PHASE_BUILD_FACTS;
    }
}

/**
 * @brief 保存当前追加栈顶，供嵌套编译期求值或 provider 导入事务设置回滚点。
 * @return cs 为空时返回 0，否则返回当前绑定数；mark 只适用于同一绑定栈的后续追加区间。
 */
TZrSize ZrParser_CompileToolBinding_Mark(const SZrCompilerState *cs) {
    return cs != ZR_NULL ? cs->compileToolBindings.length : 0;
}

/**
 * @brief 丢弃 mark 之后追加的绑定，以恢复外层词法可见范围。
 * @note 仅缩短逻辑长度，不释放借用对象；cs 为空或 mark 大于当前长度时无操作。
 */
void ZrParser_CompileToolBinding_Restore(SZrCompilerState *cs, TZrSize mark) {
    if (cs != ZR_NULL && mark <= cs->compileToolBindings.length) {
        cs->compileToolBindings.length = mark;
    }
}

/**
 * @brief 登记已知 provider 描述符，供编译期表达式、CompileTool 导入及类型和属性绑定解析。
 * @pre cs 非空时其 compileToolBindings 已初始化。
 * @pre name 与 provider 在绑定可见期间保持有效；二者不由绑定表复制或释放。
 * @return 空 compiler、名称或描述符时返回 ZR_FALSE；追加分配失败的行为见登记 helper 的 BUG 注释。
 */
TZrBool ZrParser_CompileToolBinding_DeclareProvider(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider) {
    return ZrParser_CompileToolBinding_DeclareProviderWithContentHash(
            cs, name, provider, ZR_NULL);
}

/**
 * @brief 登记已知 provider 及可选的内容身份，供编译期缓存区分相同公开合同下的不同实现。
 * @pre cs 非空时其 compileToolBindings 已初始化。
 * @pre name、provider 以及非空 hash 均由调用方持有，并在绑定可见期间保持有效。
 * @note providerContentHash 为 ZR_NULL 表示没有额外内容身份；缓存键仍会使用 provider 的公开合同身份。
 * @return compiler、名称或 provider 无效时返回 ZR_FALSE；追加分配失败的行为见登记 helper 的 BUG 注释。
 */
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

/**
 * @brief 登记与已打开项目 artifact 合同匹配的 provider，供 project-provider 导入事务绑定 alias。
 * @pre cs 非空时其 compileToolBindings 已初始化。
 * @pre provider 与 artifact 的阶段均为 CompileTool，公开合同哈希相同，artifact 仍打开且内容哈希非空；绑定可见期间不得关闭其借用存储。
 * @return 输入、artifact 状态或合同不匹配时返回 ZR_FALSE，且不会追加绑定；成功后仍由 project provider 管理 artifact 生命周期。
 */
TZrBool ZrParser_CompileToolBinding_DeclareResolvedProvider(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider,
        const SZrParserCompileToolResolvedArtifact *resolvedArtifact) {
    // 仅让相同 CompileTool 合同的打开 artifact 进入绑定，缓存键会继续读取其内容身份。
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

/**
 * @brief 将局部变量记为 shadow，使同名的 CompileTool alias 在该词法范围内被遮蔽。
 * @pre cs 非空时其 compileToolBindings 已初始化。
 * @pre name 在该绑定可见期间保持有效；记录不复制或释放它。
 * @return cs 或 name 为空时返回 ZR_FALSE；追加分配失败的行为见登记 helper 的 BUG 注释。
 */
TZrBool ZrParser_CompileToolBinding_DeclareShadow(SZrCompilerState *cs, SZrString *name) {
    return compile_tool_binding_declare(
            cs, name, ZR_NULL, ZR_NULL, ZR_NULL, ZR_COMPILE_TOOL_BINDING_SHADOW);
}

/**
 * @brief 按最近声明优先查找 alias；调用方须检查 kind，shadow 记录不含 provider。
 * @pre cs 非空时其 compileToolBindings 已初始化。
 * @return 返回绑定数组内借用地址；后续追加可能扩容，Restore 或 Reset 会截断它，因此这些操作后不得继续使用。
 */
const SZrCompileToolBinding *ZrParser_CompileToolBinding_Resolve(
        const SZrCompilerState *cs,
        SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    // 倒序扫描使最近追加的局部 shadow 或 provider 决定同名 alias 的解析结果。
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
