#ifndef ZR_VM_LANGUAGE_SERVER_LSP_VIRTUAL_DOCUMENTS_H
#define ZR_VM_LANGUAGE_SERVER_LSP_VIRTUAL_DOCUMENTS_H

#include "zr_vm_language_server/conf.h"
#include "module/lsp_module_metadata.h"

/** @brief 标记虚拟声明页中可导航的声明类别；投影记录和查询结果必须使用同一类别。 */
typedef enum EZrLspVirtualDeclarationKind {
    ZR_LSP_VIRTUAL_DECLARATION_MODULE = 0,
    ZR_LSP_VIRTUAL_DECLARATION_MODULE_LINK = 1,
    ZR_LSP_VIRTUAL_DECLARATION_CONSTANT = 2,
    ZR_LSP_VIRTUAL_DECLARATION_FUNCTION = 3,
    ZR_LSP_VIRTUAL_DECLARATION_TYPE = 4,
    ZR_LSP_VIRTUAL_DECLARATION_FIELD = 5,
    ZR_LSP_VIRTUAL_DECLARATION_METHOD = 6,
    ZR_LSP_VIRTUAL_DECLARATION_META_METHOD = 7
} EZrLspVirtualDeclarationKind;

/**
 * @brief 将光标命中的展示范围映射回原生描述符身份，供元数据查询和定义跳转复用。
 * @note 名称与身份指针借用调用方传入的 descriptor 树；插件重载或描述符失效后需重新解析 URI。
 * range.source 借用查询 URI，不保存临时投影记录的地址。
 */
typedef struct SZrLspVirtualDeclarationMatch {
    EZrLspVirtualDeclarationKind kind;
    const ZrLibModuleDescriptor *descriptor;
    const void *declarationIdentity;
    const ZrLibTypeDescriptor *ownerTypeDescriptor;
    const TZrChar *moduleName;
    const TZrChar *ownerName;
    const TZrChar *name;
    const TZrChar *targetModuleName;
    SZrFileRange range;
} SZrLspVirtualDeclarationMatch;

/** @brief 识别虚拟声明 URI 的前缀，供普通源码分析和虚拟文档请求分流；并不验证完整身份。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(SZrString *uri);
/**
 * @brief 为内建原生模块构造旧式声明 URI，供导入导航和文档请求共用。
 * @return VM GC 持有的字符串；失败返回空指针。
 * @note BUG: 模块名长到超出固定路径缓冲时 snprintf 静默截断，仍返回看似有效的错误 URI；
 * 调用方会将该 URI 用于导航，后续按截断名解析可能失败或误指向另一模块。
 */
ZR_LANGUAGE_SERVER_API SZrString *ZrLanguageServer_LspVirtualDocuments_CreateDeclarationUri(
        SZrState *state,
        const TZrChar *moduleName);
/**
 * @brief 从旧式声明 URI 提取模块名，供描述符查找使用。
 * @note 只做前缀、非空及缓冲区边界检查；作用域 URI 需由身份解析器另行处理。
 * 失败时非空缓冲的首字节会清零，不应把返回的名字作为已授权身份。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspVirtualDocuments_ParseDeclarationUri(
        SZrString *uri,
        TZrChar *moduleNameBuffer,
        TZrSize bufferSize);
/**
 * @brief 为虚拟文档和插件来源文件重新绑定当前原生描述符，供展示及成员导航共用。
 * @pre 作用域 URI 必须携带当前项目和 provider 代数；旧式 URI 仅可解析内建模块，插件须绑定来源。
 * @note 返回的描述符借用当前注册表，outSourceKind 为查询瞬间的来源；模块名写入调用方缓冲。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspVirtualDocuments_ResolveDescriptorForUri(
        SZrState *state,
        SZrLspContext *context,
        SZrLspProjectIndex *projectIndex,
        SZrString *uri,
        const ZrLibModuleDescriptor **outDescriptor,
        EZrLspImportedModuleSourceKind *outSourceKind,
        TZrChar *moduleNameBuffer,
        TZrSize bufferSize);
/**
 * @brief 用声明投影生成客户端可打开的文本，确保展示内容和导航范围采用同一模板。
 * @return 成功时 outText 指向 VM GC 持有的字符串；调用者需检查指针是否非空。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspVirtualDocuments_RenderDeclarationText(
        SZrState *state,
        const ZrLibModuleDescriptor *descriptor,
        SZrString *uri,
        SZrString **outText);
/** @brief 给只有模块摘要而没有具体成员位置的导航结果提供首行入口范围；source 借用 uri。 */
ZR_LANGUAGE_SERVER_API SZrFileRange ZrLanguageServer_LspVirtualDocuments_ModuleEntryRange(SZrString *uri);
/**
 * @brief 从描述符身份查找字段或方法在虚拟投影中的唯一声明范围。
 * @pre declarationIdentity 必须来自当前 descriptor 树；只接受字段和方法类别。
 * @note 作用域声明页沿用文本投影，物理插件文件沿用兼容性紧凑记录；失败时 outRange 清零。
 */
TZrBool ZrLanguageServer_LspVirtualDocuments_FindTypeMemberDeclaration(SZrState *state,
                                                                       const ZrLibModuleDescriptor *descriptor,
                                                                       SZrString *uri,
                                                                       const void *declarationIdentity,
                                                                       TZrInt32 memberKind,
                                                                       SZrFileRange *outRange);
/**
 * @brief 把声明页光标映射回描述符身份，供模块链接跳转及原生成员语义查询使用。
 * @note 返回的匹配只借用传入的 descriptor 树、URI 与描述符字段；不能在插件注册表变更后继续使用。
 * 对可渲染的虚拟页，LSP UTF-16 光标按刚生成的文本快照换算到内部范围。
 */
TZrBool ZrLanguageServer_LspVirtualDocuments_FindDeclarationAtPosition(SZrState *state,
                                                                       const ZrLibModuleDescriptor *descriptor,
                                                                       SZrString *uri,
                                                                       SZrLspPosition position,
                                                                       SZrLspVirtualDeclarationMatch *outMatch);

#endif
