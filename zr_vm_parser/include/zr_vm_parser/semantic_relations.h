#ifndef ZR_VM_PARSER_SEMANTIC_RELATIONS_H
#define ZR_VM_PARSER_SEMANTIC_RELATIONS_H

#include "zr_vm_parser/conf.h"

/**
 * @brief 描述 semantic snapshot 中一条有方向的导航关系。
 *
 * 关系端点按 source → target 解释；UNKNOWN 仅作未设置 sentinel，不能发布。
 */
typedef enum EZrSemanticRelationKind {
    /** 未设置的 sentinel，不是可查询关系。 */
    ZR_SEMANTIC_RELATION_UNKNOWN = 0,
    /** 同一符号从声明位置指向已解析写入位置。 */
    ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION,
    /** 派生成员指向其覆盖的基类成员。 */
    ZR_SEMANTIC_RELATION_OVERRIDE,
    /** 实现方类型或成员指向被实现的接口或契约。 */
    ZR_SEMANTIC_RELATION_IMPLEMENTATION,
    /** 派生类型指向直接基类型。 */
    ZR_SEMANTIC_RELATION_BASE_TYPE,
    /** 类型指向其构造函数符号。 */
    ZR_SEMANTIC_RELATION_CONSTRUCTOR,
    /** property 指向 getter、setter 或 initializer。 */
    ZR_SEMANTIC_RELATION_PROPERTY_ACCESSOR,
    /** 可见 alias 符号指向其解析出的类型。 */
    ZR_SEMANTIC_RELATION_ALIAS_TARGET,
    /** import 符号指向其 external origin。 */
    ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN,
} EZrSemanticRelationKind;

/**
 * @brief 可追加到 semantic context 的关系事实输入记录。
 *
 * Append 按值保存记录；每个端点至少提供一个有效 SymbolId 或 TypeId。
 * 两个 range 的 source 字符串仍由原语义快照借用，URI 输入则由 Append
 * 克隆到 context state。has*Range 决定对应位置是否存在；无位置的 external
 * 关系必须同时提供 externalOriginUri 与 virtualDeclarationUri。
 */
typedef struct SZrSemanticRelationFact {
    /** 有方向的关系种类；UNKNOWN 不能追加。 */
    EZrSemanticRelationKind kind;
    /** source 端的可选稳定符号身份。 */
    TZrSymbolId sourceSymbolId;
    /** target 端的可选稳定符号身份。 */
    TZrSymbolId targetSymbolId;
    /** source 端的可选类型身份。 */
    TZrTypeId sourceTypeId;
    /** target 端的可选类型身份。 */
    TZrTypeId targetTypeId;
    /** 零表示 provider 代际未知；非零代际参与关系身份比较。 */
    TZrUInt64 sourceProviderGeneration;
    /** 零表示 provider 代际未知；非零代际参与关系身份比较。 */
    TZrUInt64 targetProviderGeneration;
    /** source 位置按值复制，但其中 source 指针仍借用原快照。 */
    SZrFileRange sourceRange;
    /** target 位置按值复制，但其中 source 指针仍借用原快照。 */
    SZrFileRange targetRange;
    /** external origin 输入字符串；Append 成功后记录持有 state 中的副本。 */
    SZrString *externalOriginUri;
    /** 虚拟声明 URI 输入字符串；Append 成功后记录持有 state 中的副本。 */
    SZrString *virtualDeclarationUri;
    /** sourceRange 是否是可用位置；同时控制范围参与关系身份比较。 */
    TZrBool hasSourceRange;
    /** targetRange 是否是可用位置；同时控制范围参与关系身份比较。 */
    TZrBool hasTargetRange;
    /** 此关系是否指向外部提供者。 */
    TZrBool isExternal;
} SZrSemanticRelationFact;

struct SZrSemanticContext;
typedef struct SZrSemanticContext SZrSemanticContext;
struct SZrString;
typedef struct SZrString SZrString;
/**
 * @brief 为 external import origin 解析可导航的虚拟声明 URI。
 *
 * 回调同步执行；context、externalOriginUri 与 userData 均由调用方借用，
 * 返回 NULL 表示没有虚拟声明位置。非空返回值只需在当前发布调用中有效，
 * 发布器会将其交给 Append 克隆到语义 state。
 */
typedef SZrString *(*FZrSemanticVirtualDeclarationUriResolver)(
        SZrSemanticContext *context,
        SZrString *externalOriginUri,
        TZrPtr userData);
struct SZrCompilerState;
typedef struct SZrCompilerState SZrCompilerState;

/**
 * @brief 在 context state 中建立关系事实数组。
 *
 * context 为空指针或尚无 state 时不做处理；数组缓冲区由该 state 持有。
 */
ZR_PARSER_API void ZrParser_SemanticRelations_Init(SZrSemanticContext *context);
/**
 * @brief 清空当前关系事实，保留数组容量供下一轮语义快照复用。
 *
 * context 为空指针或关系数组尚未初始化时不做处理；清空不会释放 state 中已克隆的 URI。
 */
ZR_PARSER_API void ZrParser_SemanticRelations_Reset(SZrSemanticContext *context);
/**
 * @brief 释放关系事实数组存储；不销毁 context 或其共享 state。
 *
 * context 或其 state 缺失时不做处理；数组元素引用的 URI 副本仍由共享 state 管理。
 */
ZR_PARSER_API void ZrParser_SemanticRelations_Free(SZrSemanticContext *context);
/**
 * @brief 校验并将一条关系事实追加到 context；完整重复项视为成功。
 *
 * 每个端点须有有效 SymbolId 或 TypeId；UNKNOWN 无效。完整重复按 kind、两端
 * symbol/type ID、provider generation、range presence 与存在时的 range、external
 * 标志和 URI 内容比较；has*Range 为 false 时相应 range payload 不参与比较。
 * external 事实若没有 source range，必须同时有 origin URI 与 virtual declaration URI。range 按值
 * 复制但其 source 指针借用原快照；URI 则复制到 context state。
 *
 * @return 接受新事实或识别到已有相同事实时为 true；输入无效或 URI 克隆失败时为 false。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_Append(
        SZrSemanticContext *context,
        const SZrSemanticRelationFact *fact);
/**
 * @brief 从现有 canonical property contracts 发布 property→accessor 边。
 *
 * 先校验整批合同，再逐项发布已配置的 getter、setter 与 initializer；未配置项跳过。
 *
 * @return context/数组/合同校验或发布失败时为 false；否则为 true。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishPropertyContracts(
        SZrSemanticContext *context);
/**
 * @brief 从已解析的 WRITE reference facts 发布 declaration→write-position 边。
 *
 * 未解析、非写入、缺少符号或缺少可信位置的事实会跳过；同符号的不同写位置保留。
 *
 * @return context/输入数组无效或追加失败时为 false；遍历结束时为 true，即使部分输入被跳过。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishReferenceDefinitions(
        SZrSemanticContext *context);
/**
 * @brief 从可见 import facts 发布 symbol→external-origin 边。
 *
 * 可选 resolver 在发布期间同步解析虚拟声明 URI；URI 在追加时复制到 context state。
 * 缺少 import 身份、origin URI 或有效声明位置的条目会跳过。
 * visible facts 按数组顺序逐项发布；某次 Append 失败时立即返回 false，已经追加的前缀不会回滚。
 * 若需在同一输入快照上重试，先清空或重建整个 relationFacts，再重放需要保留的关系发布器：
 * 可使用 SemanticRelations_Reset，或 Free 后重新 Init。SemanticContext_Reset 会清空 visible facts，
 * 不能用于保留当前输入事实的重试准备。
 *
 * @return context/输入数组无效或某次追加失败时为 false；失败时可能留下已追加前缀；
 *         遍历完成时为 true，即使部分输入被跳过。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishImportOrigins(
        SZrSemanticContext *context);
/**
 * @brief 设置 external import 发布时使用的虚拟声明 URI resolver。
 *
 * resolver 与 userData 仅借用保存；二者须覆盖同步发布过程。传入 NULL resolver
 * 会清除回调。context 为空指针时不做处理；此 setter 本身不要求关系数组已初始化。
 * SemanticContext reset 后调用方须重新注册所需回调。
 */
ZR_PARSER_API void ZrParser_SemanticRelations_SetVirtualDeclarationUriResolver(
        SZrSemanticContext *context,
        FZrSemanticVirtualDeclarationUriResolver resolver,
        TZrPtr userData);
/**
 * @brief 从可见 alias facts 发布 alias-symbol→resolved-type 边。
 *
 * 不推测未解析目标；无有效符号、类型或声明位置的条目会跳过，重复边幂等。
 *
 * @return context/输入数组无效或追加失败时为 false；遍历结束时为 true，即使部分输入被跳过。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishAliasTargets(
        SZrSemanticContext *context);
/**
 * @brief 按稳定 SymbolId 发布一条 compiler 层级或成员关系。
 *
 * 仅支持 BASE_TYPE、IMPLEMENTATION、OVERRIDE 与 CONSTRUCTOR；两端 symbol、type 和
 * location 必须已解析。重复的 kind/source/target 符号组合返回成功。
 *
 * @return 类型、端点或上下文不满足发布条件时为 false；新边或重复边成功时为 true。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishSymbolRelation(
        SZrSemanticContext *context,
        EZrSemanticRelationKind kind,
        TZrSymbolId sourceSymbolId,
        TZrSymbolId targetSymbolId);
/**
 * @brief 将精确 AST declaration 映射到唯一 SymbolId 后发布成员关系。
 *
 * 不按名称或位置近似匹配；任何声明无匹配或映射到多个符号时拒绝发布。
 *
 * @return 声明映射或底层关系发布失败时为 false；发布新边或已存在边时为 true。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishSymbolDeclarationRelation(
        SZrSemanticContext *context,
        EZrSemanticRelationKind kind,
        const SZrAstNode *sourceDeclaration,
        const SZrAstNode *targetDeclaration);
/**
 * @brief 从精确类型声明身份发布直接 base-type 或 implementation 关系。
 *
 * source/target 必须对应 symbol 表中带有效 TypeId 和位置的类型声明。
 *
 * @return kind、声明映射或底层发布失败时为 false；发布新边或已存在边时为 true。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishTypeDeclarationRelation(
        SZrSemanticContext *context,
        EZrSemanticRelationKind kind,
        const SZrAstNode *sourceDeclaration,
        const SZrAstNode *targetDeclaration);
/**
 * @brief 从精确类型声明与构造函数 SymbolId 发布 type→constructor 边。
 *
 * 类型须能映射到有效声明记录；构造函数符号还须在 context 中有有效类型和位置。
 *
 * @return 任一身份未解析或底层发布失败时为 false；新边或已存在边成功时为 true。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishConstructorRelation(
        SZrSemanticContext *context,
        const SZrAstNode *sourceTypeDeclaration,
        TZrSymbolId constructorSymbolId);
/**
 * @brief 从 compiler class/struct prototypes 发布可解析的层级与成员关系。
 *
 * 返回 true 表示遍历完成，不保证每条可推导边都已发布；未解析 prototype 会跳过，
 * 当前 type-relation helper 的失败返回也未必传播到本函数结果。
 *
 * @return 当前实现仅在 compilerState 或 semantic context 不可用时返回 false；遍历完成时返回 true。
 *
 * TODO: 增加 LSP 部分符号 prototype fixture，分别覆盖目标缺失与两端存在但关系发布被拒绝；据预期边集合决定是否传播发布失败。
 */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishCompilerContracts(
        SZrCompilerState *compilerState);

#endif // ZR_VM_PARSER_SEMANTIC_RELATIONS_H
