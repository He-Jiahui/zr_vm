#ifndef ZR_VM_LANGUAGE_SERVER_LSP_METADATA_PROVIDER_H
#define ZR_VM_LANGUAGE_SERVER_LSP_METADATA_PROVIDER_H

#include "module/lsp_module_metadata.h"
#include "zr_vm_parser/semantic_query.h"

/** @brief 区分导入成员的展示与导航类别；类别不替代 parser 的完整元数据身份。 */
typedef enum EZrLspMetadataMemberKind {
    ZR_LSP_METADATA_MEMBER_NONE = 0,
    ZR_LSP_METADATA_MEMBER_MODULE = 1,
    ZR_LSP_METADATA_MEMBER_CONSTANT = 2,
    ZR_LSP_METADATA_MEMBER_FUNCTION = 3,
    ZR_LSP_METADATA_MEMBER_TYPE = 4,
    ZR_LSP_METADATA_MEMBER_FIELD = 5,
    ZR_LSP_METADATA_MEMBER_METHOD = 6,
    ZR_LSP_METADATA_MEMBER_PROPERTY = 7
} EZrLspMetadataMemberKind;

/** provider 是请求内的轻量借用视图；state 与 context 须覆盖一次解析、悬停或补全调用。 */
typedef struct SZrLspMetadataProvider {
    SZrState *state;
    SZrLspContext *context;
} SZrLspMetadataProvider;

/**
 * 已解析成员的多来源视图。module、descriptor、analyzer、symbol 指针均借用当前项目和注册表；
 * resolvedTypeText 等新字符串由 VM GC 持有。hasDeclaration 才表示 URI/range 可供导航。
 */
typedef struct SZrLspResolvedMetadataMember {
    SZrLspResolvedImportedModule module;
    SZrString *memberName;
    EZrLspMetadataMemberKind memberKind;
    const ZrLibModuleLinkDescriptor *moduleLinkDescriptor;
    const ZrLibConstantDescriptor *constantDescriptor;
    const ZrLibFunctionDescriptor *functionDescriptor;
    const ZrLibTypeDescriptor *typeDescriptor;
    const ZrLibFieldDescriptor *fieldDescriptor;
    const ZrLibMethodDescriptor *methodDescriptor;
    const ZrLibTypeDescriptor *ownerTypeDescriptor;
    const ZrLibTypeHintDescriptor *typeHintDescriptor;
    SZrString *ownerTypeName;
    SZrSemanticAnalyzer *declarationAnalyzer;
    SZrSymbol *declarationSymbol;
    SZrString *declarationUri;
    SZrFileRange declarationRange;
    SZrString *resolvedTypeText;
    const SZrTypeMemberInfo *typeMemberInfo;
    SZrParserSemanticPropertyQuery propertyContract;
    TZrBool hasPropertyContract;
    TZrBool hasDeclaration;
} SZrLspResolvedMetadataMember;

/** 模块入口保留来源对应的导航坐标；原生模块可指向虚拟声明，二进制关系可另带作用域身份。 */
typedef struct SZrLspResolvedImportedModuleEntry {
    SZrLspResolvedImportedModule module;
    SZrString *declarationUri;
    /* 仅无源码二进制关系可携带作用域身份；declarationUri 保留源码、物理 .zro 或原生虚拟声明坐标。 */
    SZrString *virtualDeclarationUri;
    SZrFileRange declarationRange;
    TZrBool hasDeclaration;
} SZrLspResolvedImportedModuleEntry;

/** @brief 绑定请求的 state/context；provider 不接管二者的所有权。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspMetadataProvider_Init(
    SZrLspMetadataProvider *provider,
    SZrState *state,
    SZrLspContext *context);
/** @brief 需要时先装载项目模块，再从 parser/项目索引与原生注册表解析导入模块。 */
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(SZrLspMetadataProvider *provider,
                                                                   SZrSemanticAnalyzer *analyzer,
                                                                   SZrLspProjectIndex *projectIndex,
                                                                   SZrString *moduleName,
                                                                   SZrLspResolvedImportedModule *outResolved);
/**
 * @brief 按模块名及成员名汇集原型、源码、二进制和原生 descriptor 信息。
 * @note 返回成功不等于有精确声明；检查 hasDeclaration。仅凭名称无法区分同名重载。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspMetadataProvider_ResolveImportedMember(
    SZrLspMetadataProvider *provider,
    SZrSemanticAnalyzer *analyzer,
    SZrLspProjectIndex *projectIndex,
    SZrString *moduleName,
    SZrString *memberName,
    SZrLspResolvedMetadataMember *outResolved);
/** @brief 为 import 导航给出当前来源入口及可选的 parser 关系身份。 */
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveImportedModuleEntry(SZrLspMetadataProvider *provider,
                                                                        SZrSemanticAnalyzer *analyzer,
                                                                        SZrLspProjectIndex *projectIndex,
                                                                        SZrString *moduleName,
                                                                        SZrLspResolvedImportedModuleEntry *outResolved);
/** @brief 将模块来源概况包装成由调用方释放的 LSP hover。 */
TZrBool ZrLanguageServer_LspMetadataProvider_CreateImportedModuleHover(SZrLspMetadataProvider *provider,
                                                                       const SZrLspResolvedImportedModule *resolvedModule,
                                                                       SZrFileRange range,
                                                                       SZrLspHover **result);
/** @brief 按来源与成员类别生成 hover；可从当前文档快照补入源码文档。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspMetadataProvider_CreateImportedMemberHover(
    SZrLspMetadataProvider *provider,
    SZrSemanticAnalyzer *analyzer,
    const SZrLspResolvedMetadataMember *resolvedMember,
    SZrFileRange range,
    SZrLspHover **result);
/** @brief 把源码、二进制和原生 descriptor 的公共导出投影到现有补全数组。 */
TZrBool ZrLanguageServer_LspMetadataProvider_AppendImportedModuleCompletions(
    SZrLspMetadataProvider *provider,
    SZrSemanticAnalyzer *analyzer,
    const SZrLspResolvedImportedModule *resolvedModule,
    SZrArray *result);
/** @brief 载入二进制元数据；成功后 outSource 由调用方用 FreeBinaryModuleSource 释放。 */
TZrBool ZrLanguageServer_LspMetadataProvider_LoadBinaryModuleSource(SZrLspMetadataProvider *provider,
                                                                    SZrLspProjectIndex *projectIndex,
                                                                    SZrString *moduleName,
                                                                    SZrIoSource **outSource);
/** @brief 取得二进制模块的物理 .zro URI，供当前坐标投影使用。 */
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveBinaryModuleUri(SZrLspMetadataProvider *provider,
                                                                    SZrLspProjectIndex *projectIndex,
                                                                    SZrString *moduleName,
                                                                    SZrString **outUri);
/** @brief 从二进制导出元数据取得声明 URI/范围；缺少精确导出范围时由调用方回退到模块入口。 */
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveBinaryExportDeclaration(
    SZrLspMetadataProvider *provider,
    SZrLspProjectIndex *projectIndex,
    SZrString *moduleName,
    SZrString *memberName,
    SZrString **outUri,
    SZrFileRange *outRange);
/** @brief 取得原生模块声明 URI；项目插件 URI 绑定 provider 代数并可能触发注册。 */
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveNativeModuleUri(SZrLspMetadataProvider *provider,
                                                                    SZrLspProjectIndex *projectIndex,
                                                                    SZrString *moduleName,
                                                                    SZrString **outUri);
/** @brief 凭当前 descriptor 指针定位原生类型字段/方法；失败时清空声明标志。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspMetadataProvider_ResolveNativeTypeMemberDeclaration(
    SZrLspMetadataProvider *provider,
    SZrLspProjectIndex *projectIndex,
    SZrLspResolvedMetadataMember *resolvedMember);
/** @brief 在项目文件的当前 AST 中寻找命名类型成员，并借出 analyzer/符号。 */
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveProjectTypeMemberDeclaration(
    SZrLspMetadataProvider *provider,
    SZrLspProjectIndex *projectIndex,
    SZrString *ownerTypeName,
    SZrString *memberName,
    SZrLspResolvedMetadataMember *outResolved);
/** @brief 二进制属性需匹配 PropertySymbol 身份后定位到所属模块入口。 */
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveBinaryTypeMemberDeclaration(
    SZrLspMetadataProvider *provider,
    SZrLspProjectIndex *projectIndex,
    SZrLspResolvedMetadataMember *resolvedMember);
/** @brief 从虚拟文档位置反查原生字段/方法 descriptor，供定义和悬停复用。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspMetadataProvider_FindNativeTypeMemberDeclaration(
    SZrLspMetadataProvider *provider,
    SZrLspProjectIndex *projectIndex,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspResolvedMetadataMember *outResolved);
/** @brief 将来源枚举交给共享模块元数据层映射为用户可读标签。 */
const TZrChar *ZrLanguageServer_LspMetadataProvider_SourceKindLabel(EZrLspImportedModuleSourceKind sourceKind);

#endif
