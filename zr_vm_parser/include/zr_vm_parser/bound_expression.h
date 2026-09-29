#ifndef ZR_VM_PARSER_BOUND_EXPRESSION_H
#define ZR_VM_PARSER_BOUND_EXPRESSION_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_core/array.h"

struct SZrSemanticContext;
struct SZrState;

/**
 * @brief 标记 bound value construction 是否持有可供编译器消费的解析结果。
 *
 * INVALID 是初始化、失败和释放后的哨兵；VALUE_CONSTRUCT 表示 constructorId 与
 * arguments 已按 canonical contract 解析，可以交给对应的 struct-init lowering。
 */
typedef enum EZrBoundExpressionKind {
    ZR_BOUND_EXPRESSION_INVALID = 0,
    ZR_BOUND_EXPRESSION_VALUE_CONSTRUCT
} EZrBoundExpressionKind;

/**
 * @brief 以源调用顺序提交给 constructor resolver 的单个实参视图。
 *
 * name 可为空以表示位置实参；类型 ID 必须属于当前语义上下文，sourceRange 用于
 * 保留调用点诊断位置。字符串由调用方持有，本结构不获取其所有权。
 */
typedef struct SZrBoundValueConstructArgumentInput {
    TZrTypeId typeId;
    SZrString *name;
    EZrCanonicalCallSiteMarker callSiteMarker;
    SZrFileRange sourceRange;
} SZrBoundValueConstructArgumentInput;

/**
 * @brief 解析成功后关联源实参与 constructor 形参的映射记录。
 *
 * sourceIndex 保留求值顺序，parameterIndex 指向 canonical contract 中的形参；
 * 两者不同正是具名实参重排时编译器需要的信息。name 仍是借用指针。
 */
typedef struct SZrBoundValueConstructArgument {
    TZrTypeId typeId;
    SZrString *name;
    EZrCanonicalCallSiteMarker callSiteMarker;
    TZrUInt32 sourceIndex;
    TZrUInt32 parameterIndex;
    SZrFileRange sourceRange;
} SZrBoundValueConstructArgument;

/**
 * @brief 编译阶段临时持有的 constructor 绑定结果。
 *
 * arguments 由 Init/Bind 管理并由 Free 释放；其中 name 指针不转移所有权，须由
 * 调用方保证底层名称仍然存活。成功绑定时 typeId/resultTypeId 描述同一目标值类型。
 */
typedef struct SZrBoundValueConstruct {
    EZrBoundExpressionKind kind;
    TZrTypeId typeId;
    TZrSymbolId constructorId;
    SZrArray arguments; /* SZrBoundValueConstructArgument */
    TZrTypeId resultTypeId;
    SZrFileRange sourceRange;
} SZrBoundValueConstruct;

/**
 * @brief 初始化绑定结果的可复用存储和 INVALID 哨兵。
 * @pre state 与 bound 均有效；不要对仍持有未释放 arguments 的对象重复初始化。
 */
ZR_PARSER_API void ZrParser_BoundValueConstruct_Init(
        struct SZrState *state,
        SZrBoundValueConstruct *bound);

/**
 * @brief 释放 Init/Bind 管理的参数映射并将结果清回零状态。
 * @note name 是借用的字符串指针，其存储生命周期不由 Free 管理。
 */
ZR_PARSER_API void ZrParser_BoundValueConstruct_Free(
        struct SZrState *state,
        SZrBoundValueConstruct *bound);

/**
 * @brief 按 canonical constructor contract 解析实参并生成编译器使用的索引映射。
 *
 * struct-init lowering 先收集各实参的 canonical 类型和调用标记，再调用本接口；
 * 成功结果区分 synthesized default constructor 与显式 constructor，后续 lowering
 * 据 constructorId 选择直接写入目标位置或调用显式构造函数。
 * @pre context 已初始化；outBound 已由 Init 初始化（后续调用可复用其有效状态）；
 *      argumentCount 大于零时 arguments 至少含有该数量的元素。
 * @return 保留 canonical resolver 的 NOT_CONSTRUCTIBLE、NO_MATCH、INACCESSIBLE、
 *         AMBIGUOUS、INVALID_ARGUMENTS 等结果；通过入口校验后只有 RESOLVED 会填充绑定。
 *         context/outBound 缺失或实参数组形态错误时，outBound 保持调用前状态。
 * @warning TODO: 当前导出接口不保留或注册 name 的 GC 根；compiler 调用路径由 AST
 *          保持名称存活，其他调用方如何跨 GC 保证借用指针有效尚无统一契约。
 */
ZR_PARSER_API EZrValueConstructorResolution ZrParser_BoundValueConstruct_Bind(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrBoundValueConstructArgumentInput *arguments,
        TZrSize argumentCount,
        SZrFileRange sourceRange,
        SZrBoundValueConstruct *outBound);

#endif // ZR_VM_PARSER_BOUND_EXPRESSION_H
