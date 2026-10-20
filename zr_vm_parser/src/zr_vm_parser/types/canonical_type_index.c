#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

#include <stdint.h>

#define ZR_CANONICAL_TYPE_INDEX_INITIAL_BUCKETS ((TZrSize)64U)
#define ZR_CANONICAL_TYPE_INDEX_NONE ((TZrUInt32)UINT32_MAX)

/**
 * @brief 将结构 hash 映射到容量为二次幂的桶数组。
 * @pre bucketCount 非零且为二次幂；容量由 Index_Init/扩容路径提供。
 */
static TZrSize canonical_type_index_bucket(TZrUInt64 structuralHash, TZrSize bucketCount) {
    return (TZrSize)(structuralHash & (TZrUInt64)(bucketCount - 1U));
}

/**
 * @brief 创建空桶表并将每个 head 初始化为链尾哨兵。
 * @note Init 与扩容重建共用此步骤，桶表只存 node index，不持有节点对象。
 * @note Array_Init 分配失败会传播 MEMORY_ERROR；仅正常完成后才具备可写桶和完整哨兵。
 */
static void canonical_type_index_init_buckets(
        SZrSemanticContext *context,
        SZrArray *buckets,
        TZrSize bucketCount) {
    TZrUInt32 empty = ZR_CANONICAL_TYPE_INDEX_NONE;
    TZrSize index;

    ZrCore_Array_Init(context->state, buckets, sizeof(TZrUInt32), bucketCount);
    for (index = 0; index < bucketCount; index++) {
        ZrCore_Array_Push(context->state, buckets, &empty);
    }
}

/**
 * @brief 根据 canonicalTypes 现有节点重建所有碰撞链，再替换旧桶数组。
 * @note 必须在节点数组与 canonicalTypeHashNext 同步增长后运行；重建按节点当前顺序头插。
 * @note 新桶分配失败会传播 MEMORY_ERROR；正常填链后才释放并替换旧桶，不承诺外层上下文的异常回滚。
 */
static void canonical_type_index_rebuild(SZrSemanticContext *context, TZrSize bucketCount) {
    SZrArray buckets;
    TZrSize nodeIndex;

    canonical_type_index_init_buckets(context, &buckets, bucketCount);
    for (nodeIndex = 0; nodeIndex < context->canonicalTypes.length; nodeIndex++) {
        const SZrCanonicalTypeNode *node = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                nodeIndex);
        TZrSize bucketIndex = canonical_type_index_bucket(node->structuralHash, bucketCount);
        TZrUInt32 *bucketHead = (TZrUInt32 *)ZrCore_Array_Get(&buckets, bucketIndex);
        TZrUInt32 *next = (TZrUInt32 *)ZrCore_Array_Get(&context->canonicalTypeHashNext, nodeIndex);

        *next = *bucketHead;
        *bucketHead = (TZrUInt32)nodeIndex;
    }

    ZrCore_Array_Free(context->state, &context->canonicalTypeHashBuckets);
    context->canonicalTypeHashBuckets = buckets;
}

/**
 * @brief 为 canonical type interning 建立结构哈希桶和碰撞链数组。
 * @note semantic context 初始化时调用；Reset 保留桶容量，Free 才释放 backing storage。
 * @note 数组构造 OOM 会传播 MEMORY_ERROR；外层负责异常恢复及已初始化前缀清理，不以本函数普通返回报告。
 */
void ZrParser_CanonicalTypeIndex_Init(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return;
    }

    canonical_type_index_init_buckets(
            context,
            &context->canonicalTypeHashBuckets,
            ZR_CANONICAL_TYPE_INDEX_INITIAL_BUCKETS);
    ZrCore_Array_Init(
            context->state,
            &context->canonicalTypeHashNext,
            sizeof(TZrUInt32),
            ZR_PARSER_INITIAL_CAPACITY_SMALL);
}

/**
 * @brief 清空碰撞链索引，使当前 type 节点不再能通过结构哈希命中。
 * @note 由 canonical type reset 在释放节点子数组后调用；桶数组保留以供同一 context 重用。
 */
void ZrParser_CanonicalTypeIndex_Reset(SZrSemanticContext *context) {
    TZrUInt32 empty = ZR_CANONICAL_TYPE_INDEX_NONE;
    TZrSize index;

    if (context == ZR_NULL || !context->canonicalTypeHashBuckets.isValid) {
        return;
    }

    for (index = 0; index < context->canonicalTypeHashBuckets.length; index++) {
        ZrCore_Array_Set(&context->canonicalTypeHashBuckets, index, &empty);
    }
    ZrCore_Array_Empty(&context->canonicalTypeHashNext);
}

/**
 * @brief 释放哈希桶与节点碰撞链的 backing storage。
 * @note 生命周期由 canonical type free 管理；释放后须重新 Init 才能插入或查桶。
 */
void ZrParser_CanonicalTypeIndex_Free(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(context->state, &context->canonicalTypeHashBuckets);
    ZrCore_Array_Free(context->state, &context->canonicalTypeHashNext);
}

/**
 * @brief 将新追加的 canonical type node 接入结构哈希的碰撞链。
 * @pre nodeIndex 指向刚进入 canonicalTypes 的节点；正常生产路径由 canonical_type_store 每节点调用一次。
 * @note 负载过高时重建并扩容全部链；First/Next 再沿链枚举同 hash 候选，结构相等性仍由 interner 判定。
 * TODO: 未触发重建时重复插入已有 nodeIndex 可形成碰撞链环；canonical_type_store 当前只追加后插入一次，是否拒绝或幂等仍待明确。
 * @note 链数组扩容或桶重建 OOM 会传播 MEMORY_ERROR，不转为 false；本接口不回滚调用前已追加的节点。
 */
TZrBool ZrParser_CanonicalTypeIndex_Insert(SZrSemanticContext *context, TZrSize nodeIndex) {
    const SZrCanonicalTypeNode *node;
    TZrUInt32 empty = ZR_CANONICAL_TYPE_INDEX_NONE;
    TZrSize bucketIndex;
    TZrUInt32 *bucketHead;
    TZrUInt32 *next;

    if (context == ZR_NULL ||
        nodeIndex >= context->canonicalTypes.length ||
        nodeIndex >= (TZrSize)UINT32_MAX ||
        !context->canonicalTypeHashBuckets.isValid) {
        return ZR_FALSE;
    }

    while (context->canonicalTypeHashNext.length <= nodeIndex) {
        ZrCore_Array_Push(context->state, &context->canonicalTypeHashNext, &empty);
    }

    if (context->canonicalTypes.length >
        context->canonicalTypeHashBuckets.length - context->canonicalTypeHashBuckets.length / 4U) {
        canonical_type_index_rebuild(context, context->canonicalTypeHashBuckets.length * 2U);
        return ZR_TRUE;
    }

    node = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(&context->canonicalTypes, nodeIndex);
    bucketIndex = canonical_type_index_bucket(
            node->structuralHash,
            context->canonicalTypeHashBuckets.length);
    bucketHead = (TZrUInt32 *)ZrCore_Array_Get(&context->canonicalTypeHashBuckets, bucketIndex);
    next = (TZrUInt32 *)ZrCore_Array_Get(&context->canonicalTypeHashNext, nodeIndex);
    *next = *bucketHead;
    *bucketHead = (TZrUInt32)nodeIndex;
    return ZR_TRUE;
}

/**
 * @brief 返回给定 structural hash 对应碰撞链的首节点索引。
 * @pre 索引已初始化且未与 canonicalTypes 分离；返回 ZR_MAX_SIZE 表示该桶为空或不可用。
 * @note 此接口只定位候选；调用者必须再比较完整类型结构，不能把 hash 当作类型身份。
 */
TZrSize ZrParser_CanonicalTypeIndex_First(
        const SZrSemanticContext *context,
        TZrUInt64 structuralHash) {
    const TZrUInt32 *bucketHead;
    TZrSize bucketIndex;

    if (context == ZR_NULL ||
        !context->canonicalTypeHashBuckets.isValid ||
        context->canonicalTypeHashBuckets.length == 0) {
        return ZR_MAX_SIZE;
    }

    bucketIndex = canonical_type_index_bucket(
            structuralHash,
            context->canonicalTypeHashBuckets.length);
    bucketHead = (const TZrUInt32 *)ZrCore_Array_Get(
            (SZrArray *)&context->canonicalTypeHashBuckets,
            bucketIndex);
    return *bucketHead == ZR_CANONICAL_TYPE_INDEX_NONE ? ZR_MAX_SIZE : (TZrSize)*bucketHead;
}

/**
 * @brief 返回同一哈希碰撞链中指定节点之后的索引。
 * @pre nodeIndex 是 First/Next 返回的有效节点索引；ZR_MAX_SIZE 代表遍历结束。
 * @note 链索引绑定当前 canonicalTypes 布局，Reset/Free 后不得继续使用旧索引。
 */
TZrSize ZrParser_CanonicalTypeIndex_Next(
        const SZrSemanticContext *context,
        TZrSize nodeIndex) {
    const TZrUInt32 *next;

    if (context == ZR_NULL || nodeIndex >= context->canonicalTypeHashNext.length) {
        return ZR_MAX_SIZE;
    }

    next = (const TZrUInt32 *)ZrCore_Array_Get(
            (SZrArray *)&context->canonicalTypeHashNext,
            nodeIndex);
    return *next == ZR_CANONICAL_TYPE_INDEX_NONE ? ZR_MAX_SIZE : (TZrSize)*next;
}

/**
 * @fn static TZrSize canonical_type_index_bucket(TZrUInt64 structuralHash, TZrSize bucketCount)
 * ## Summary
 * 为查询、插入和重建提供同一低位哈希分桶规则；结果只定位候选桶。
 *
 * ## Parameters
 * @param[in] structuralHash 节点或查询键的完整结构哈希值。
 * @param[in] bucketCount 当前或新建桶表的桶数，须为非零二次幂。
 *
 * ## Returns
 * @return 位于 [0, bucketCount) 的桶下标；共享桶不表示完整哈希或类型结构相等。
 *
 * ## Constraints and lifecycle
 * 纯值运算，不取得或借出资源。容量不变量由初始化和倍增重建维持，本函数不检查非法容量。
 *
 * ## References
 * canonical_type_index_rebuild 重建全部链；ZrParser_CanonicalTypeIndex_Insert 和 ZrParser_CanonicalTypeIndex_First 使用相同规则发布与查询。
 */

/**
 * @fn static void canonical_type_index_init_buckets(SZrSemanticContext *context, SZrArray *buckets, TZrSize bucketCount)
 * ## Summary
 * 为初建或重建取得桶头存储，并将每个逻辑桶置为 NONE，使后续连链不会读取未初始化元素。
 *
 * ## Parameters
 * @param[in] context 借用分配所用上下文，其 state/global 在构造期间有效。
 * @param[out] buckets 接收新数组的描述符，调用前不得拥有未释放的旧缓冲区。
 * @param[in] bucketCount 要建立的非零二次幂桶数，所需字节须可表示。
 *
 * ## Returns
 * 无返回值；正常完成后数组长度等于 bucketCount，全部桶头为 NONE。分配失败由 Array_Init 传播 MEMORY_ERROR，经恢复点跳转或 panic 终止，不以普通状态返回。
 *
 * ## Constraints and lifecycle
 * 成功缓冲区交给调用者；初建由 context 持有，重建先由局部数组持有。不释放旧资源，也不替调用者建立异常恢复或清理其他初始化前缀。先分配完整容量再填桶，容量内的 Push 不再申请扩容。
 *
 * ## References
 * ZrCore_Array_Init 和 ZrCore_Array_Push 提供数组与异常契约；ZrParser_CanonicalTypeIndex_Init 和 canonical_type_index_rebuild 是两个实际入口。
 */

/**
 * @fn static void canonical_type_index_rebuild(SZrSemanticContext *context, TZrSize bucketCount)
 * ## Summary
 * 负载增长后为全部当前规范类型节点重排碰撞链，并替换桶表，使驻留查询继续按桶寻找候选。
 *
 * ## Parameters
 * @param[in,out] context 独占使用的已初始化上下文；节点池与 next 数组属于同一轮分析。
 * @param[in] bucketCount 新桶数，须为可表示的非零二次幂。
 *
 * ## Returns
 * 无返回值；正常完成后全部当前节点纳入新桶，旧桶已释放。新桶分配失败传播 MEMORY_ERROR；该分配完成前本函数尚未重写旧链或释放旧桶。
 *
 * ## Constraints and lifecycle
 * hashNext 长度须覆盖全部节点，本函数直接重写已有 next 槽。局部新桶在链完成后交给 context；节点及 payload 不在此释放。不回滚调用前已追加的节点或外层状态；不得并发或重入修改同一 context，旧遍历顺序不得跨重建继续。
 *
 * ## References
 * ZrParser_CanonicalTypeIndex_Insert 先补足 next，再在超过四分之三负载时调用；canonical_type_index_init_buckets 构造新桶。
 */

/**
 * @fn void ZrParser_CanonicalTypeIndex_Init(SZrSemanticContext *context)
 * ## Summary
 * 随语义上下文首次构造规范类型驻留索引，建立初始桶表和可增长的碰撞链数组。
 *
 * ## Parameters
 * @param[in,out] context 要建立索引的上下文；NULL 时无操作。非空时 state 有效、canonicalTypes 已初始化，两个索引数组不得仍拥有未释放旧存储。
 *
 * ## Returns
 * 无返回值；正常完成后桶头全空、next 长度为零。OOM 由底层数组构造传播 MEMORY_ERROR，不通过普通返回报告。
 *
 * ## Constraints and lifecycle
 * 存储归 context，经 Reset 复用、Free 释放；本接口不是覆盖旧存储的重复初始化。第二个数组构造失败时不会自行撤销已建立桶表；异常恢复与已初始化前缀清理由外层负责，不把局部顺序称为已验证的 OOM 原子构造。
 *
 * ## References
 * semantic_context_init_arrays 在节点池建立后调用；ZrParser_CanonicalTypeIndex_Reset 与 ZrParser_CanonicalTypeIndex_Free 负责复用和关闭。
 */

/**
 * @fn void ZrParser_CanonicalTypeIndex_Reset(SZrSemanticContext *context)
 * ## Summary
 * 当前轮规范类型图清理后清空索引，避免下一轮驻留经旧桶访问已经失效的节点内容。
 *
 * ## Parameters
 * @param[in,out] context 待复用上下文；NULL 或桶无效时无操作。正常顺序先清理节点 payload 并将 canonicalTypes 长度归零。
 *
 * ## Returns
 * 无返回值；有效桶全部恢复 NONE，next 长度归零，两个数组的缓冲区与容量保留。
 *
 * ## Constraints and lifecycle
 * 不释放节点 payload、不清空节点池或重置语义 ID。旧候选索引和遍历状态不跨轮复用；调用期间不得并发访问同一索引。保留存储最终仍须由 Free 关闭。
 *
 * ## References
 * ZrParser_CanonicalType_Reset 清理节点并清空节点池后调用；graph 的大图场景包含 Reset 后重新驻留的静态断言。
 */

/**
 * @fn void ZrParser_CanonicalTypeIndex_Free(SZrSemanticContext *context)
 * ## Summary
 * 规范类型图关闭时释放桶表与碰撞链的原生存储，结束索引自身的资源生命周期。
 *
 * ## Parameters
 * @param[in,out] context 索引所属上下文；NULL 无操作。非空时分配这些数组的 state/global 在释放期间仍有效。
 *
 * ## Returns
 * 无返回值；两个数组经 Array_Free 清为未构造状态，不再拥有缓冲区。
 *
 * ## Constraints and lifecycle
 * 不遍历节点 payload，也不释放 canonicalTypes；外层先完成节点 Reset 和池释放再关闭索引。旧借用数组地址与遍历状态均失效，重新使用前须 Init；不与同 context 读写并发。
 *
 * ## References
 * ZrParser_CanonicalType_Free 负责整体关闭顺序；ZrCore_Array_Free 释放存储并清零描述符。
 */

/**
 * @fn TZrBool ZrParser_CanonicalTypeIndex_Insert(SZrSemanticContext *context, TZrSize nodeIndex)
 * ## Summary
 * 将刚追加节点发布到结构哈希候选链；超过四分之三负载时重建全部链，为驻留提供候选而不代替完整结构判等。
 *
 * ## Parameters
 * @param[in,out] context 拥有当前节点池、桶表与 next 的已初始化上下文。
 * @param[in] nodeIndex 刚追加且尚未发布的节点下标，须小于节点数与 UINT32_MAX；每个节点只调用一次。
 *
 * ## Returns
 * @return true 表示节点经连链或重建纳入索引；false 仅表示 context 空、下标不可接受或桶未有效，检查失败时尚未修改索引。OOM 经 MEMORY_ERROR 离开，不转为 false。
 *
 * ## Constraints and lifecycle
 * 先补足 next 再重建或连接桶头；索引不拥有节点 payload。此前已追加的节点不由本接口异常回滚，外层恢复前不能把部分状态当完整图继续使用；遍历与修改须串行化。TODO: 未触发重建时重复插入已有节点可能形成链环；canonical_type_store 当前只追加后插入一次，是否拒绝重复或幂等仍需明确。
 *
 * ## References
 * canonical_type_store 是仓内唯一实际插入点；canonical_type_index_rebuild 处理增长，First/Next 枚举候选供 interner 判等。
 */

/**
 * @fn TZrSize ZrParser_CanonicalTypeIndex_First(const SZrSemanticContext *context, TZrUInt64 structuralHash)
 * ## Summary
 * 为驻留查询取得哈希桶首个候选，由调用者继续核对类型种类与全部结构字段。
 *
 * ## Parameters
 * @param[in] context 只读借用的当前图；NULL、桶无效或桶数零按空结果处理。
 * @param[in] structuralHash 查询的完整结构哈希。
 *
 * ## Returns
 * @return 桶首节点下标；空桶或上述不可用状态返回 ZR_MAX_SIZE。这是整数位置，不转移节点所有权，也不保证候选完整哈希或结构相等。
 *
 * ## Constraints and lifecycle
 * 不分配资源或修复索引；有效桶数须为二次幂，桶和节点池属于同 context 轮次。从 First 到最后一次 Next 之间不得插入、重建、Reset 或 Free；节点 payload 的借用另受对应容器生命周期约束。
 *
 * ## References
 * canonical_type.c 的十二组 interning 循环枚举候选后完整判等；ZrParser_CanonicalTypeIndex_Insert 建立桶链。
 */

/**
 * @fn TZrSize ZrParser_CanonicalTypeIndex_Next(const SZrSemanticContext *context, TZrSize nodeIndex)
 * ## Summary
 * 沿当前桶的碰撞链继续枚举候选，支持冲突情况下查找结构相等的既有节点。
 *
 * ## Parameters
 * @param[in] context 只读借用，须与取得链头时为同一 context 及同一轮次。
 * @param[in] nodeIndex 当前候选位置；空 context 或超出 next 长度按链尾处理。
 *
 * ## Returns
 * @return next 保存的后继下标；NONE、空 context 或越界返回 ZR_MAX_SIZE。不取得节点所有权，也不证明候选与查询类型相等。
 *
 * ## Constraints and lifecycle
 * 只读取已有链，不检测环或重建损坏状态；调用者遵守单次插入及同轮生命周期。Reset/Free 或链修改后不得沿旧遍历状态继续；本函数不提供并发同步。
 *
 * ## References
 * ZrParser_CanonicalTypeIndex_First 提供起点；十二组 interning 循环与 graph 冲突场景是当前继续遍历入口。
 */
