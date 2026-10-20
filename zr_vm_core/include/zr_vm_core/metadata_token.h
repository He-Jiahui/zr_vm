/**
 * ## Summary
 * 防止重复包含公开 token 声明；该 guard 只控制公开声明的重复包含。
 *
 * ## Value model
 * 本 guard 宏在首次包含中被定义；再次包含时跳过相同公开声明。
 *
 * ## Constraints and lifecycle
 * 只控制本次预处理中的重复包含；不创建运行期状态、token 或资源。#ifndef/#define 与末尾 #endif 必须配套。
 *
 * ## References
 * `function.h 的 metadata_token.h include`（zr_vm_core/include/zr_vm_core/function.h:9）：function.h 编译包含声明，触发预处理 guard；不是运行时调用。
 */
#ifndef ZR_VM_CORE_METADATA_TOKEN_H
#define ZR_VM_CORE_METADATA_TOKEN_H

#include "zr_vm_core/conf.h"

/**
 * ## Summary
 * 在公开 metadata 结构中声明 GC 字符串的不透明指针类型，使该 header 无需导入完整字符串布局；记录/heap 的持久化消费者按字符串内容工作。
 *
 * ## Value model
 * 声明不提供对象字段、分配方式或独占释放权；仅允许 struct SZrString* 出现在 record 和 heap entry。
 *
 * ## Invariants and lifecycle
 * 指针借用的对象由实际 GC 字符串体系管理。metadata 数组的 native 生命周期不能证明字符串的强根或移动转发已闭合；下一步核函数 GC 遍历和 loader 安装。
 *
 * ## References
 * `SZrMetadataTokenRecord::requestedModuleVersion`（zr_vm_core/include/zr_vm_core/metadata_token.h:345）：公开 record 仅用 struct SZrString* 参数布局，属于 compile type-use。
 * `write_metadata_token_records`（zr_vm_parser/src/zr_vm_parser/writer/writer.c:994）：writer 按字符串内容消费 record 的借用指针。
 */
struct SZrString;

/**
 * ## Summary
 * 编译和元数据消费阶段共用的 32 位值身份。生产者编码所属域中的表与 RID；记录查询、表分类和绑定据此寻找对应元数据，调用链接另建执行目标。
 *
 * ## Value model
 * 高 8 位为表号，低 24 位为 RID；零 token 表示缺席，非零编码仍可能含 RID 0。
 * MEMBER_DEF 或 SIGNATURE 只是身份命名空间；payload 在所属函数记录/签名堆中，token 不含函数地址。
 *
 * ## Invariants and lifecycle
 * 纯值无资源所有权；跨函数或模块比较必须结合元数据域。MAKE 截断输入，不验证表号、RID 范围或记录存在。
 * 当前 artifact 谓词另核要求表及非零 RID；查询返回所属数组行的借用地址。未知仓外 C 消费者须自行核域与生命周期，不能由当前搜索排除。
 *
 * ## References
 * `artifact_token_is`（zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c:22）：表号门禁与 RID 非零门禁共同验证输入，不查找执行地址。
 * `compiler_metadata_module_record_emit`（zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_record.c:121）：在所属模块 metadata 域构造 MODULE RID 1。
 */
typedef TZrUInt32 TZrMetadataToken;

/**
 * ## Summary
 * 为 compiler、source 记录、模块绑定和反射解释 token 高字节提供固定命名空间；生产者选表，消费者按表核适用记录和签名契约。
 *
 * ## Value model
 * MODULE 表示 provider 模块；TYPE_DEF 表示 union 定义或 callable owner；MEMBER_DEF 同时容纳方法、字段、child 与脚本入口。
 * ASSEMBLY_REF/TYPE_REF/MEMBER_REF 表达引用；TYPE_SPEC 表达额外类型构造；SIGNATURE 容纳普通配对或 hash-only witness。
 *
 * ## Invariants and lifecycle
 * 表号是纯值，不建立资源或可执行目标；相同 RID 跨表/域不可互换。
 * AssemblyRef 可映射为 MODULE；TypeRef 统计只处理携带 TYPE_DEF 目标的项。具体行用途还依赖 reserved0、正文和适用消费者。
 *
 * ## References
 * `compiler_metadata_module_record_emit`（zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_record.c:121）：MODULE 的实际 token 生产。
 * `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:910）：TypeRef 扫描按表号筛选适用记录。
 */
typedef enum EZrMetadataTableTag {
    /** @brief 提供方模块身份，编译器从 RID 1 建立模块与配对签名；AssemblyRef 链接可映射到此表；模块 token 不表示入口调用。 */
    ZR_METADATA_TABLE_MODULE = 1,
    /** @brief 本地类型定义或 callable prototype owner 的身份；union 定义和 owner 记录由正文及 reserved0 区分。 */
    ZR_METADATA_TABLE_TYPE_DEF = 2,
    /** @brief 已定义的 typed export、callable 或脚本入口身份；方法与字段共用表；可执行性不能只看表号。 */
    ZR_METADATA_TABLE_MEMBER_DEF = 3,
    /** @brief 导入所指程序集或模块及其版本约束身份；链接结果可为提供方 MODULE；不要求跨表 token 数值相等。 */
    ZR_METADATA_TABLE_ASSEMBLY_REF = 4,
    /** @brief 按名称或目标定义约束描述引用类型；只有携带 TYPE_DEF 目标的记录进入当前 TypeRef 绑定统计。 */
    ZR_METADATA_TABLE_TYPE_REF = 5,
    /** @brief 导入成员引用或 MethodSpec 基方法引用身份；须解析提供方成员与签名；本身不是函数指针。 */
    ZR_METADATA_TABLE_MEMBER_REF = 6,
    /** @brief 闭合泛型或组合类型的签名实体身份；匹配可跨模块 RID；签名与 union 定义及布局另核。 */
    ZR_METADATA_TABLE_TYPE_SPEC = 7,
    /** @brief 实体配对签名或独立 typed-call 签名约束身份；CALLABLE_SIGNATURE 可仅含哈希且无 blob；不能据此执行调用。 */
    ZR_METADATA_TABLE_SIGNATURE = 8
} EZrMetadataTableTag;

/* token 的高 8 位是表标记，低 24 位是 RID；MAKE 会截断越界输入。 */
/* TODO: AOT 紧缩重映射以保留行数加一生成新 RID，未见该入口检查 24 位上限；
 * 需核裁剪前后的最大行数约束，并用边界输入验证 MAKE 不产生碰撞。 */
/**
 * ## Summary
 * 三个编码宏共同固定高字节与 RID 的分界；该宏提供 token 编码表达式使用的固定纯值。
 *
 * ## Value model
 * 展开值 `24U`；固定 ABI 编码，不表示表数或容量
 *
 * ## Constraints and lifecycle
 * 这是固定编码 ABI 的纯值；被 MAKE/TABLE/RID 表达式读取，不独立验证输入。修改会影响生成与读取编码，当前值保持不变。
 *
 * ## References
 * `ZR_METADATA_TOKEN_MAKE`（zr_vm_core/include/zr_vm_core/metadata_token.h:156）：MAKE 编码表达式把表号移入高字节。
 * `ZR_METADATA_TOKEN_TABLE`（zr_vm_core/include/zr_vm_core/metadata_token.h:176）：TABLE 解码表达式还原高字节。
 */
#define ZR_METADATA_TOKEN_TABLE_SHIFT 24U
/**
 * ## Summary
 * 生成器检查 RID 容量并由 MAKE/RID 限定低位；该宏提供 token 编码表达式使用的固定纯值。
 *
 * ## Value model
 * 展开值 `((TZrUInt32)0x00FFFFFFu)`；最大可编码 RID；掩码本身不能验证存在性
 *
 * ## Constraints and lifecycle
 * 这是固定编码 ABI 的纯值；被 MAKE/TABLE/RID 表达式读取，不独立验证输入。修改会影响生成与读取编码，当前值保持不变。
 *
 * ## References
 * `metadata_token_string_heap_reserve`（zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_token.c:87）：字符串堆扩容先限制容量上界。
 * `compiler_build_function_metadata_tokens`（zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_token.c:1422）：显式模块数量及和 AssemblyRef 数的和先核 24 位上界。
 */
#define ZR_METADATA_TOKEN_RID_MASK ((TZrUInt32)0x00FFFFFFu)
/**
 * ## Summary
 * MAKE/TABLE 限定表号高字节；该宏提供 token 编码表达式使用的固定纯值。
 *
 * ## Value model
 * 展开值 `((TZrUInt32)0xFFu)`；只限定编码宽度，未知表号不被拒绝
 *
 * ## Constraints and lifecycle
 * 这是固定编码 ABI 的纯值；被 MAKE/TABLE/RID 表达式读取，不独立验证输入。修改会影响生成与读取编码，当前值保持不变。
 *
 * ## References
 * `ZR_METADATA_TOKEN_MAKE`（zr_vm_core/include/zr_vm_core/metadata_token.h:156）：MAKE 对表号截取 8 位。
 * `ZR_METADATA_TOKEN_TABLE`（zr_vm_core/include/zr_vm_core/metadata_token.h:176）：TABLE 截取解码结果。
 */
#define ZR_METADATA_TOKEN_TABLE_MASK ((TZrUInt32)0xFFu)
/**
 * ## Summary
 * 为生产者构造元数据域内的表号/RID 身份；该宏接受参数并返回纯值；调用方需结合所属元数据域继续验证。
 *
 * ## Parameters
 * @param[in] TABLE 元数据命名空间输入；转 UInt32 后仅保留低 8 位，宏不拒绝未知表。
 * @param[in] RID 所属表的行号输入；转 UInt32 后仅保留低 24 位，零与越界输入不在宏内拒绝；生产者另核容量。
 *
 * ## Returns
 * @return 返回掩码组合后的 UInt32 token；未知表、RID 0 或被截断 RID 仍可编码。结果为纯值，调用者负责域、容量和记录验证。
 *
 * ## Constraints and lifecycle
 * TABLE、RID 各求值一次；转 UInt32 后截断；不拒绝 0 RID 或未知表；表达式只计算值，不取得资源；结果需由实际消费者按所属域继续核验。
 * TODO:MAKE 的一般 AOT compacted retained-count 总上界仍需核 remap 输入与 24 位边界；这不扩大现有生产者容量检查保证。
 *
 * ## References
 * `compiler_metadata_module_record_emit`（zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_record.c:121）：生成 MODULE1。
 * `binding_publish_definition`（zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c:227）：生成 callable MEMBER_DEF 身份。
 */
#define ZR_METADATA_TOKEN_MAKE(TABLE, RID)                                                                            \
    ((((TZrUInt32)(TABLE) & ZR_METADATA_TOKEN_TABLE_MASK) << ZR_METADATA_TOKEN_TABLE_SHIFT) |                         \
     ((TZrUInt32)(RID) & ZR_METADATA_TOKEN_RID_MASK))
/**
 * ## Summary
 * 读取标签供表分类和针对性校验；该宏接受参数并返回纯值；调用方需结合所属元数据域继续验证。
 *
 * ## Parameters
 * @param[in] TOKEN 待分类的 32 位可转换值，求值一次；可为零，不授予引用或目标存在性。
 *
 * ## Returns
 * @return 返回 token 高字节的表号纯值；零 token 得零，已知表号也不证明对应记录存在。
 *
 * ## Constraints and lifecycle
 * TOKEN 求值一次；零 token 得零标签；不验证实体；表达式只计算值，不取得资源；结果需由实际消费者按所属域继续核验。
 *
 * ## References
 * `artifact_token_is`（zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c:22）：检查 token 属于要求表。
 * `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:910）：TypeRef 适用项分类。
 */
#define ZR_METADATA_TOKEN_TABLE(TOKEN)                                                                                \
    (((TZrUInt32)(TOKEN) >> ZR_METADATA_TOKEN_TABLE_SHIFT) & ZR_METADATA_TOKEN_TABLE_MASK)
/**
 * ## Summary
 * 读取表内定位号及生产者当前最大 RID；该宏接受参数并返回纯值；调用方需结合所属元数据域继续验证。
 *
 * ## Parameters
 * @param[in] TOKEN 待取行号的 32 位可转换值，求值一次；读取方仍核要求表、非零与表内范围。
 *
 * ## Returns
 * @return 返回低 24 位 RID 纯值；RID 0 需由适用实体消费者拒绝，宏不读取数组或借出对象。
 *
 * ## Constraints and lifecycle
 * TOKEN 求值一次；0 RID 需由适用读取器拒绝；表达式只计算值，不取得资源；结果需由实际消费者按所属域继续核验。
 *
 * ## References
 * `artifact_token_is`（zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c:23）：拒绝 RID0 编码。
 * `binding_next_rid`（zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c:192）：生产者求现有最大 RID 并分配下一身份。
 */
#define ZR_METADATA_TOKEN_RID(TOKEN) ((TZrUInt32)(TOKEN) & ZR_METADATA_TOKEN_RID_MASK)

/**
 * ## Summary
 * 供签名 writer 与对应 reader 分派字节正文。根节点、递归类型子节点和 compiler 富正文分别遵循其实际格式，不能由节点码推定通用 payload。
 *
 * ## Value model
 * PRIMITIVE、TYPE_REF/TYPE_DEF、ARRAY、TUPLE、GENERIC_INST、OWNERSHIP、UNION、NULLABLE 描述适用的类型节点；字段/方法根使用 FIELD_SIG/METHOD_SIG。
 * INVALID 和 FUNC 被当前结构 reader 拒绝。普通 TYPE_DEF reader 期待 basic type/name；compiler union 定义从 name/arity 开始写 variants。
 * MEMBER_REF/ASSEMBLY_REF 导入富正文与普通结构节点的一 u32 payload 不同；MethodSpec 的 generic base 专门要求 MEMBER_REF。MODULE 写 name/version 引用。
 *
 * ## Invariants and lifecycle
 * 节点是纯字节标签，名称引用由所属字符串堆解释，不持有运行时对象。
 * 结构 reader 验递归边界、允许节点和完整根消费；不能据枚举存在宣称源编译器会产生 TUPLE 或预留 FUNC。
 *
 * ## References
 * `zrp_metadata_validate_signature_type_node`（zr_vm_core/src/zr_vm_core/artifact/zrp_metadata.c:265）：字节节点转枚举后按适用 payload 分支验证。
 * `metadata_token_write_type_ref_signature`（zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c:360）：类型签名 writer 输出节点字节。
 */
typedef enum EZrMetadataSignatureNode {
    /** @brief 未初始化或无有效签名节点；现有结构型校验和类型节点读取均拒绝。 */
    ZR_METADATA_SIGNATURE_NODE_INVALID = 0,
    /** @brief 携带 u32 基础值类型编号；值类型编号不是 metadata token。 */
    ZR_METADATA_SIGNATURE_NODE_PRIMITIVE = 1,
    /** @brief 携带基础类型编号与字符串堆名称索引；描述引用类型，不直接携带提供方函数地址。 */
    ZR_METADATA_SIGNATURE_NODE_TYPE_REF = 2,
    /** @brief 普通类型节点与 union 定义富正文共用的节点码；普通类型读取路径期待基础类型号和名称索引两个 u32；当前 compiler union 定义 writer 从名称索引、generic arity 开始再写 variant 契约，不附带普通节点的基础类型号；只能用对应定义读取路径解释。 */
    ZR_METADATA_SIGNATURE_NODE_TYPE_DEF = 3,
    /** @brief 携带数组秩及递归元素类型；类型子节点读取须递归验证元素，节点本身不描述数组实例。 */
    ZR_METADATA_SIGNATURE_NODE_ARRAY = 4,
    /** @brief 携带元素数量及有序类型子节点列表；结构型读取支持此节点，不据枚举存在推定当前源编译器必然生成。 */
    ZR_METADATA_SIGNATURE_NODE_TUPLE = 5,
    /** @brief 预留函数类型节点码；当前结构型校验器和类型节点读取器均拒绝，方法根使用 METHOD_SIG。 */
    ZR_METADATA_SIGNATURE_NODE_FUNC = 6,
    /** @brief 携带基节点与具体类型实参列表；类型实例基可为 TYPE_REF/TYPE_DEF，MethodSpec 专用读取要求 MEMBER_REF 基节点。 */
    ZR_METADATA_SIGNATURE_NODE_GENERIC_INST = 7,
    /** @brief 携带所有权限定值及被限定类型；签名限定信息不持有运行时对象。 */
    ZR_METADATA_SIGNATURE_NODE_OWNERSHIP = 8,
    /** @brief 携带 union 基础类型、名称和泛型实参类型列表；本地 union 定义及布局另存 TYPE_DEF record。 */
    ZR_METADATA_SIGNATURE_NODE_UNION = 9,
    /** @brief 包装一个可空类型子节点；不增加独立值或引用 token。 */
    ZR_METADATA_SIGNATURE_NODE_NULLABLE = 10,
    /** @brief 在 MethodSpec 中承载基方法 token；编译器导入成员正文还写模块名、成员名与 effect kind；专用格式读取与普通类型节点读取的 payload 不可混同。 */
    ZR_METADATA_SIGNATURE_NODE_MEMBER_REF = 11,
    /** @brief 编译器导入签名写程序集或模块名及三个版本字符串引用；普通结构型类型读取仅处理一个 u32，富导入正文须用适用路径解释。 */
    ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF = 12,
    /** @brief 方法根携带格式版本、flags、generic arity、返回类型和参数列表；可作为 blob 根；类型子节点路径拒绝此码。 */
    ZR_METADATA_SIGNATURE_NODE_METHOD_SIG = 13,
    /** @brief 字段根携带格式版本及字段类型；可作为 blob 根；不能作为参数类型子节点。 */
    ZR_METADATA_SIGNATURE_NODE_FIELD_SIG = 14,
    /** @brief 模块签名携带入口名与版本的字符串堆引用；模块 ABI 哈希另存；该节点不是函数入口地址。 */
    ZR_METADATA_SIGNATURE_NODE_MODULE = 15
} EZrMetadataSignatureNode;

/* TODO: AOT 修剪器按 native sizeof 复制 tokenRecords，包括三个字符串指针；
 * 需核发布产物是否将非空宿主地址写入 blob，以及跨进程读取的字段约束。 */
/**
 * ## Summary
 * 函数/模块元数据生产、持久化和绑定阶段的行记录。普通 pair 关联实体与签名；用途 marker 行为调用链接、局部 child alias 与受限脚本入口提供不同身份契约。
 *
 * ## Fields
 * | 成员 | 对调用方的用途 | 实际生产或消费引用 |
 * | --- | --- | --- |
 * | `token` | 在函数元数据域内定位该实体或签名记录 | `metadata_query_find_token_record`（zr_vm_core/src/zr_vm_core/function/function_metadata_query.c:13）：按完整 token 找首项，借出所属记录数组中的行。 |
 * | `relatedToken` | 普通实体与 SIGNATURE 记录的双向配对身份 | `metadata_find_signature_record`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:91）：验证签名行 relatedToken 回指当前实体。 |
 * | `ownerToken` | 记录实体归属或签名行回指的实体身份 | `metadata_find_signature_record`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:92）：核签名行 owner 回指实体而非只核表号。 |
 * | `ownerIndex` | 按记录用途定位 export/effect、常量、prototype、child 或模块重定位槽 | `ZrCore_Function_FindConstantChildAlias`（zr_vm_core/src/zr_vm_core/function/function_identity.c:87）：显式 child 别名读取前核索引属于该函数 child 数组。 |
 * | `signatureBlobOffset` | 定位所属 function.signatureBlobHeap 中的签名切片起点 | `metadata_record_blob_is_valid`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:19）：确认偏移在所属函数 heap 内，随后核长度余量。 |
 * | `signatureBlobLength` | 声明签名切片的 UInt32 字节长度 | `metadata_record_blob_is_valid`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:18）：拒绝缺失 blob 并核非溢出切片长度。 |
 * | `signatureHash` | 保存本记录所属协议的签名或 owner guard 哈希 | `function_find_matching_type_spec_record`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:132）：用同域 hash 筛选 TypeSpec，再由完整 blob 比较确认。 |
 * | `layoutVersion` | 保存类型定义或调用 owner 的布局协议版本 | `function_find_targeted_type_ref_provider_type_def`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:419）：核 TypeRef caller/provider 布局版本与哈希。 |
 * | `reserved0` | 区别普通 token pair 与六类 callable/script 元数据用途 | `link_function`（zr_vm_core/src/zr_vm_core/call/call_binding_link.c:177）：依重定位种类核本地常量或模块定义标记。 |
 * | `layoutHash` | 记录 union 或 callable owner 的物理布局身份 | `function_find_targeted_type_ref_provider_type_def`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:420）：布局参与时核双方 hash，而非复用签名 hash。 |
 * | `targetMetadataToken` | 保存提供方目标约束或局部常量到 child 的显式边 | `ZrCore_Function_FindConstantChildAlias`（zr_vm_core/src/zr_vm_core/function/function_identity.c:85）：按常量记录的显式目标找到局部 child 行。 |
 * | `targetSignatureToken` | 保存引用所要求的提供方配对签名身份 | `function_find_targeted_type_ref_provider_type_def`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:403）：目标 signature token 非零才核 provider 配对。 |
 * | `targetSignatureHash` | 保存导入目标定义的预期 hash | `function_find_targeted_type_ref_provider_type_def`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:396）：TypeRef 稳定绑定要求非零目标签名约束且匹配。 |
 * | `targetModuleSignatureHash` | 保存提供方模块 ABI 约束 | `function_find_targeted_type_ref_provider_type_def`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:410）：TypeRef 声明模块约束时核 provider ABI hash。 |
 * | `requestedModuleVersion` | 提供 AssemblyRef 的可选请求版本给导入 effect 回退 | `module_import_signature_effect_with_assembly_version`（zr_vm_core/src/zr_vm_core/module/module_import_signature.c:781）：仅为缺请求版本的 effect 补齐 AssemblyRef 借用值。 |
 * | `minModuleVersionInclusive` | 提供可选版本区间的含端点下界 | `metadata_runtime_version_range_matches`（zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_binding_compatibility.c:154）：完整可解析区间比较实际版本与包含下界。 |
 * | `maxModuleVersionExclusive` | 提供可选版本区间的不含端点上界 | `metadata_runtime_version_range_matches`（zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_binding_compatibility.c:155）：完整可解析区间比较实际版本与排除上界。 |
 *
 * ## Invariants and lifecycle
 * 普通实体/SIGNATURE pair 互指 relatedToken，签名 ownerToken 回指实体，SIGNATURE 继承对应 export/effect ownerIndex；hash-only CALLABLE_SIGNATURE 才单独使用无槽且无 blob。
 * ownerIndex 由 reserved0 与所属函数选择 export/effect、constant、prototype、child 或 module relocation 数组域；owner/target 不保证同域。signatureHash 和 layoutHash 属于各自协议。
 * 结构 blob 要求 heap 存在、长度非零、offset < heapLength 且 length <= heapLength-offset；hash 相等不替代需要的完整 blob 比较。
 * 数组由函数 native 存储持有；三个版本指针借用 GC 字符串。source 按内容重建，runtime/native ZRP 可复制指针。TODO:核 native ZRP 跨进程发布及 gc_mark/gc_cycle 与 loader 安装的版本根/移动转发；不能由 native 数组释放推断根保持。
 *
 * ## References
 * `metadata_token_write_record_pair`（zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_token.c:1207）：普通 pair 实体行字段实际生产。
 * `metadata_token_write_record_pair`（zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_token.c:1226）：配对签名行继承 export/effect ownerIndex。
 */
typedef struct SZrMetadataTokenRecord {
    /** @brief 在函数元数据域内定位该实体或签名记录。
     * @note 完整表号/RID 是检索键，零表示缺席；query 返回数组内首项借用地址而非执行目标；地址依赖所属函数记录数组不被替换。
     */
    TZrMetadataToken token;
    /** @brief 普通实体与 SIGNATURE 记录的双向配对身份。
     * @note pair writer 互指实体与签名；signature 查询还检查签名 ownerToken 回指实体；CALLABLE_OWNER/CHILD/hash-only witness 不保证可查询配对。
     */
    TZrMetadataToken relatedToken;
    /** @brief 记录实体归属或签名行回指的实体身份。
     * @note export 实体 owner 可为零而签名行 owner 为实体；callable method 常量可后补 prototype TYPE_DEF owner；不能统一当模块或同一 owner。
     */
    TZrMetadataToken ownerToken;
    /** @brief 按记录用途定位 export/effect、常量、prototype、child 或模块重定位槽。
     * @note 普通 SIGNATURE 继承 export 或 flattened effect 序号；callable marker 决定数组域；hash-only CALLABLE_SIGNATURE 与 SCRIPT_ENTRY 为 NONE；读取 child 索引另验 childFunctionLength。
     */
    TZrUInt32 ownerIndex;
    /** @brief 定位所属 function.signatureBlobHeap 中的签名切片起点。
     * @note 零偏移可有效；blob 读取要求 heap 存在、offset < heapLength，长度以减法检查；hash-only witness 的零偏移不授予可解引用 blob。
     */
    TZrUInt32 signatureBlobOffset;
    /** @brief 声明签名切片的 UInt32 字节长度。
     * @note 结构 blob 消费路径要求 length > 0 且 length <= heapLength - offset；hash-only CALLABLE_SIGNATURE 可留零并被该 blob 路径拒绝；不能用长度零推断所有记录不存在。
     */
    TZrUInt32 signatureBlobLength;
    /** @brief 保存本记录所属协议的签名或 owner guard 哈希。
     * @note 普通稳定 blob hash、FunctionSignatureHash 结构 hash、CALLABLE_OWNER 的 prototype layout hash 分属协议；TypeSpec/TypeDef hash 通过后仍比较完整 blob；非零不能证明全局唯一。
     */
    TZrUInt64 signatureHash;
    /** @brief 保存类型定义或调用 owner 的布局协议版本。
     * @note 普通 pair 初始零；callable 常量关联 owner 时写 schema version，union/TypeRef 另有布局生产路径；任一侧 version/hash 非零时 TypeRef 绑定同时核两项。
     */
    TZrUInt32 layoutVersion;
    /** @brief 区别普通 token pair 与六类 callable/script 元数据用途。
     * @note 普通 pair 初始化零；该字段的 CALLABLE/SCRIPT_ENTRY 标记决定 ownerIndex 解释；SZrMetadataTokenBinding 同名保留字段不参与此分类。
     */
    TZrUInt32 reserved0;
    /** @brief 记录 union 或 callable owner 的物理布局身份。
     * @note 与 layoutVersion 组成同一布局契约；普通 pair 置零，callable 常量后补 prototype hash；TypeRef 任一侧声明布局则完整对比，不能普遍由 signatureHash 替代。
     */
    TZrUInt64 layoutHash;
    /** @brief 保存提供方目标约束或局部常量到 child 的显式边。
     * @note 导入目标通常属于 provider 域；CALLABLE_CONSTANT 指向本函数 CALLABLE_CHILD 为局部例外；TypeRef 目标 TYPE_DEF 是 provider 查找键，child 别名另核记录及签名。
     */
    TZrMetadataToken targetMetadataToken;
    /** @brief 保存引用所要求的提供方配对签名身份。
     * @note 与本记录 relatedToken 的 caller 配对不同；TypeRef 此值非零才与 provider relatedToken 比较，零只是缺少此项约束，不证明任意签名等价。
     */
    TZrMetadataToken targetSignatureToken;
    /** @brief 保存导入目标定义的预期 hash。
     * @note 引用正文 signatureHash 与目标 hash 可不同；TypeRef binder 要求目标 hash 非零并匹配，而 runtime layout-view 路径仅在非零时核此项；不能跨路径统一零值语义。
     */
    TZrUInt64 targetSignatureHash;
    /** @brief 保存提供方模块 ABI 约束。
     * @note TypeRef 非零才核 provider.moduleSignatureHash；AssemblyRef binding 优先 effect 的非零值，否则采用此字段；零表示该路径未要求模块 hash，不表示已经匹配。
     */
    TZrUInt64 targetModuleSignatureHash;
    /** @brief 提供 AssemblyRef 的可选请求版本给导入 effect 回退。
     * @note 只有 effect.requestedModuleVersion 缺失才借用此指针；source IO 按内容重建，不能据存储推断通用 exact-version 拒绝或强 GC 根；native ZRP 指针发布及版本指针 GC 根待核。
     */
    struct SZrString *requestedModuleVersion;
    /** @brief 提供可选版本区间的含端点下界。
     * @note 与 max 同时存在且 actual/min/max 可解析时要求 actual >= min；缺端点或任一版本不可解析时当前兼容路径放行；借用 GC 字符串，source 内容重建与 native ZRP 指针格式不同。
     */
    struct SZrString *minModuleVersionInclusive;
    /** @brief 提供可选版本区间的不含端点上界。
     * @note 与 min 同时存在且 actual/min/max 可解析时要求 actual < max；缺端点或任一版本不可解析时当前兼容路径放行；不能改述为闭上界，版本指针 GC 根及 ZRP 发布待核。
     */
    struct SZrString *maxModuleVersionExclusive;
} SZrMetadataTokenRecord;

/* reserved0 区分记录用途；hash-only CALLABLE_SIGNATURE 的 ownerIndex 为无槽哨兵；普通 SIGNATURE 记录可继承对应 export/effect 序号。 */
/**
 * ## Summary
 * ownerIndex 指向本函数常量槽；成员与签名记录可共享此标记；该宏提供 token 编码表达式使用的固定纯值。
 *
 * ## Value model
 * 展开值 `((TZrUInt32)1u)`；链接仍核目标 token、签名和目标函数种类
 *
 * ## Constraints and lifecycle
 * 链接仍核目标 token、签名和目标函数种类；这是 record.reserved0 的用途值，不是函数地址、GC 所有者或跨域身份。其 ownerIndex/关联字段由所属行协议核验。
 *
 * ## References
 * `binding_finalize_function`（zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c:371）：本地常量 relocation 标记生产。
 * `ZrCore_Function_FindConstantChildAlias`（zr_vm_core/src/zr_vm_core/function/function_identity.c:76）：按 constantIndex 找唯一常量 MEMBER_DEF。
 */
#define ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT ((TZrUInt32)1u)
/**
 * ## Summary
 * TYPE_DEF 记录的 ownerIndex 指向 prototype 槽，signatureHash 承载布局 guard；该宏提供 token 编码表达式使用的固定纯值。
 *
 * ## Value model
 * 展开值 `((TZrUInt32)2u)`；这是调用 owner 身份，不是目标方法
 *
 * ## Constraints and lifecycle
 * 这是调用 owner 身份，不是目标方法；这是 record.reserved0 的用途值，不是函数地址、GC 所有者或跨域身份。其 ownerIndex/关联字段由所属行协议核验。
 *
 * ## References
 * `binding_publish_owner`（zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c:306）：prototype owner 标记生产。
 * `link_owner`（zr_vm_core/src/zr_vm_core/call/call_binding_link.c:42）：owner guard 入口核 TYPE_DEF owner 标记。
 */
#define ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER ((TZrUInt32)2u)
/**
 * ## Summary
 * ownerIndex 记录模块重定位 targetIndex，供 interface/virtual dispatch guard 核对；该宏提供 token 编码表达式使用的固定纯值。
 *
 * ## Value model
 * 展开值 `((TZrUInt32)3u)`；标记本身不解析 native provider 或确定函数地址
 *
 * ## Constraints and lifecycle
 * 标记本身不解析 native provider 或确定函数地址；这是 record.reserved0 的用途值，不是函数地址、GC 所有者或跨域身份。其 ownerIndex/关联字段由所属行协议核验。
 *
 * ## References
 * `binding_finalize_function`（zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c:375）：模块 relocation targetIndex 标记生产。
 * `link_function`（zr_vm_core/src/zr_vm_core/call/call_binding_link.c:179）：模块 relocation 核对应定义标记。
 */
#define ZR_METADATA_TOKEN_RECORD_CALLABLE_MODULE ((TZrUInt32)3u)
/**
 * ## Summary
 * 仅签名约束记录，ownerIndex 使用 ZR_CALL_BINDING_SLOT_NONE；该宏提供 token 编码表达式使用的固定纯值。
 *
 * ## Value model
 * 展开值 `((TZrUInt32)4u)`；不要求 blob，也不提供目标成员 token
 *
 * ## Constraints and lifecycle
 * 不要求 blob，也不提供目标成员 token；这是 record.reserved0 的用途值，不是函数地址、GC 所有者或跨域身份。其 ownerIndex/关联字段由所属行协议核验。
 *
 * ## References
 * `binding_publish_typed_signature`（zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c:105）：typed-call hash-only 签名 witness 生产，无槽/无blob。
 * `binding_publish_typed_signature`（zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c:106）：显式写入无槽哨兵，不覆盖普通 SIGNATURE pair 规则。
 */
#define ZR_METADATA_TOKEN_RECORD_CALLABLE_SIGNATURE ((TZrUInt32)4u)
/**
 * ## Summary
 * 发布本函数常量到 childFunctionList 的显式别名边；该宏提供 token 编码表达式使用的固定纯值。
 *
 * ## Value model
 * 展开值 `((TZrUInt32)5u)`；publisher 只在目标指针等于本函数 child 时生成；消费者先找唯一 constantIndex 的 MEMBER_DEF，拒绝重复，再核 target token、CHILD marker、signatureHash 和 child 索引；无显式 alias 的重绑 fallback 另做 whole-definition 比较，不能把 marker/hash 说成通用函数身份
 *
 * ## Constraints and lifecycle
 * publisher 只在目标指针等于本函数 child 时生成；消费者先找唯一 constantIndex 的 MEMBER_DEF，拒绝重复，再核 target token、CHILD marker、signatureHash 和 child 索引；无显式 alias 的重绑 fallback 另做 whole-definition 比较，不能把 marker/hash 说成通用函数身份；这是 record.reserved0 的用途值，不是函数地址、GC 所有者或跨域身份。其 ownerIndex/关联字段由所属行协议核验。
 *
 * ## References
 * `compiler_publish_module_call_bindings`（zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c:83）：target==childFunction 时发布本函数局部 edge。
 * `compiler_publish_module_call_bindings`（zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c:86）：常量 targetMetadataToken 指向该 child 行。
 */
#define ZR_METADATA_TOKEN_RECORD_CALLABLE_CHILD ((TZrUInt32)5u)
/**
 * ## Summary
 * 受限纯脚本入口及配对签名记录；ownerIndex 使用无槽哨兵；该宏提供 token 编码表达式使用的固定纯值。
 *
 * ## Value model
 * 展开值 `((TZrUInt32)6u)`；仅记录已验证无参数 i64 签名，不授予运行时 token 派发能力
 *
 * ## Constraints and lifecycle
 * 仅记录已验证无参数 i64 签名，不授予运行时 token 派发能力；这是 record.reserved0 的用途值，不是函数地址、GC 所有者或跨域身份。其 ownerIndex/关联字段由所属行协议核验。
 *
 * ## References
 * `compiler_script_entry_metadata_emit`（zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_script_entry_metadata.c:100）：已通过受限入口签名资格才生产标记。
 * `compiler_script_entry_metadata_hash`（zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_script_entry_metadata.c:115）：脚本元数据 hash 只取带入口标记的 MEMBER_DEF。
 */
#define ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY ((TZrUInt32)6u)

/**
 * ## Summary
 * 签名中的稳定字符串编号到 GC 字符串对象的条目；compiler 检查冲突、去重并排序，lookup 遍历编号后借出对象指针。
 *
 * ## Fields
 * | 成员 | 对调用方的用途 | 实际生产或消费引用 |
 * | --- | --- | --- |
 * | `stringIndex` | 把签名字节中的内容稳定编号映射到字符串对象 | `function_metadata_string_heap_lookup`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:156）：遍历所属函数字符串堆按稳定编号匹配。 |
 * | `value` | 借用 metadata 字符串对象并按稳定编号返回其指针 | `function_metadata_string_heap_lookup`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:157）：命中编号后借出该 GC 字符串指针。 |
 *
 * ## Invariants and lifecycle
 * stringIndex 由内容 stable hash 低 31 位加一产生，非空编号非零；空字符串不加入；同编号异内容在 add 拒绝。编号不是 entries 下标。
 * native entries 数组仅借用 value，lookup 不转移对象所有权。source 按内容重建，runtime 安装复制指针。TODO:核 gc_mark/gc_cycle 函数 metadata 堆根、跨代与移动转发和 loader/runtime 安装边界。
 *
 * ## References
 * `metadata_token_string_heap_add`（zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_token.c:142）：检查 stable 编号冲突/去重后发布 entries.stringIndex。
 * `function_metadata_string_heap_lookup`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:157）：lookup 只借出数组项内 GC 字符串指针。
 */
typedef struct SZrMetadataStringHeapEntry {
    /** @brief 把签名字节中的内容稳定编号映射到字符串对象。
     * @note stable hash 低 31 位加一生成非零编号；空字符串不加入；同编号异内容在 add 时拒绝；lookup 按值遍历，不按数组下标定位。
     */
    TZrUInt32 stringIndex;
    /** @brief 借用 metadata 字符串对象并按稳定编号返回其指针。
     * @note add 不拷贝对象；lookup 只借出 SZrString*，native 数组释放不释放 GC 对象；IO 按内容重建、runtime copy 借用指针；function metadata 的强根及移动转发路径尚未闭合。
     */
    struct SZrString *value;
} SZrMetadataStringHeapEntry;

/**
 * ## Summary
 * 保存 caller 引用与解析后 provider 身份的 sidecar 快照，供兼容门禁、跨模块泛型重验和诊断使用。不同 binding producer 决定 ref、expected 与 resolved 的实际域。
 *
 * ## Fields
 * | 成员 | 对调用方的用途 | 实际生产或消费引用 |
 * | --- | --- | --- |
 * | `refToken` | 以 caller 引用身份查找或复用函数 sidecar | `ZrCore_Function_FindModuleMetadataBinding`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:606）：按 caller ref 键返回 sidecar。 |
 * | `refSignatureToken` | 保留 caller 引用的配对签名身份 | `ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider`（zr_vm_core/src/zr_vm_core/reflection/generic/reflection_generic_instance.c:103）：核 requester 现有签名仍对应记录的 ref 身份。 |
 * | `refSignatureHash` | 保留 caller 引用正文的 hash | `ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider`（zr_vm_core/src/zr_vm_core/reflection/generic/reflection_generic_instance.c:104）：核 requester 正文 hash 与 ref 快照一致。 |
 * | `expectedMetadataToken` | 记录 caller 规范身份或声明的目标约束 | `metadata_runtime_check_binding_status`（zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_binding_compatibility.c:279）：在非映射例外路径应用可选 metadata token 约束。 |
 * | `expectedSignatureToken` | 记录 caller 规范签名或可选目标签名约束 | `metadata_runtime_check_binding_status`（zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_binding_compatibility.c:284）：非零且非映射例外时核目标签名 token。 |
 * | `expectedSignatureHash` | 保存适用路径的 caller 预期签名 hash | `metadata_runtime_check_binding_status`（zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_binding_compatibility.c:288）：声明非零签名约束时核 resolved hash。 |
 * | `expectedModuleSignatureHash` | 记录 caller 所要求的 provider 模块 ABI | `metadata_runtime_check_binding_status`（zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_binding_compatibility.c:273）：先于 token 比较执行可选模块 ABI 门禁。 |
 * | `expectedLayoutVersion` | 保存 caller 布局契约的版本侧 | `metadata_runtime_check_binding_status`（zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_binding_compatibility.c:293）：布局身份参与时核双方版本。 |
 * | `reserved0` | 保存预期侧 UInt32 source 布局保留值 | `io_read_module_metadata_binding_list`（zr_vm_core/src/zr_vm_core/io/io.c:711）：按 layout identity 版本读取 UInt32 保留值，旧 source 版本置零。 |
 * | `expectedLayoutHash` | 保存 caller 布局契约的 hash 侧 | `metadata_runtime_check_binding_status`（zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_binding_compatibility.c:296）：布局身份参与时核双方 hash。 |
 * | `resolvedMetadataToken` | 保存 provider 域中解析得到的实体身份 | `metadata_runtime_check_binding_status`（zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_binding_compatibility.c:280）：对非映射例外的非零期望检查提供方实体。 |
 * | `resolvedSignatureToken` | 保存 provider 签名身份的快照 | `ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider`（zr_vm_core/src/zr_vm_core/reflection/generic/reflection_generic_instance.c:125）：重新核 provider 实体指向记录的签名 token。 |
 * | `resolvedSignatureHash` | 保存 provider 当前路径产生的签名 hash | `metadata_runtime_check_binding_status`（zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_binding_compatibility.c:289）：对已声明期望核提供方签名 hash 快照。 |
 * | `resolvedModuleSignatureHash` | 保存提供方函数的 moduleSignatureHash 快照 | `ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider`（zr_vm_core/src/zr_vm_core/reflection/generic/reflection_generic_instance.c:113）：重新核提供方当前模块 ABI 与绑定快照一致。 |
 * | `resolvedLayoutVersion` | 保存 provider 布局身份的版本侧 | `metadata_runtime_check_binding_status`（zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_binding_compatibility.c:293）：核提供方布局版本与期望版本。 |
 * | `reserved1` | 保存实际侧 UInt32 source 布局保留值 | `io_read_module_metadata_binding_list`（zr_vm_core/src/zr_vm_core/io/io.c:724）：按 layout identity 版本读取 UInt32 保留值，旧 source 版本置零。 |
 * | `resolvedLayoutHash` | 保存 provider 布局身份的 hash 侧 | `metadata_runtime_check_binding_status`（zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_binding_compatibility.c:296）：核提供方布局 hash 与期望值。 |
 *
 * ## Invariants and lifecycle
 * 非零 refToken 是 caller 函数的 Upsert/Find 键。同键 Upsert 复用现有行而不重置；扩容复制旧行并释放旧 native 数组，旧借用 binding 地址失效。
 * refSignatureHash 可是引用正文，expectedSignatureHash 可是目标定义约束；AssemblyRef→MODULE 与 TypeSpec 规范映射不要求 token 数值相同，仍核适用 hash/layout。
 * 任一 expected/resolved layout version/hash 非零触发布局对比较。MemberRef producer 未写的 layout 字段不能声称在复用时清零。reserved0/reserved1 是 UInt32 source 保留值，不是 record CALLABLE marker。
 * 解析快照不保证 provider 后来仍兼容；泛型入口重新核 requester/ref 与 provider pair/hash/blob。三版本值填缺失 effect 后，区间门禁仅对完整可解析 min/max/actual 生效，缺值或解析失败当前放行。
 *
 * ## References
 * `function_record_type_spec_binding`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:480）：Upsert 借用 sidecar 后填 caller/provider 快照。
 * `metadata_runtime_check_binding_status`（zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_binding_compatibility.c:273）：实际执行模块 ABI 与后续 token/hash/layout 判定。
 */
typedef struct SZrMetadataTokenBinding {
    /** @brief 以 caller 引用身份查找或复用函数 sidecar。
     * @note 必须是本 caller 函数域内非零键；Upsert 同键复用，扩容使旧借用 binding 地址失效；Find 返回借用且不触发解析。
     */
    TZrMetadataToken refToken;
    /** @brief 保留 caller 引用的配对签名身份。
     * @note TypeSpec/TypeRef 取 callerSignatureRecord；MemberRef/AssemblyRef 取引用 relatedToken；跨模块泛型解析核它等于 requester 签名，不可用 provider resolved 值覆盖。
     */
    TZrMetadataToken refSignatureToken;
    /** @brief 保留 caller 引用正文的 hash。
     * @note TypeRef 保存引用 signatureHash 而 expectedSignatureHash 保存目标约束，可不同；泛型 provider 解析还核 requester view hash 与完整 blob，不能仅凭该 hash 绑定。
     */
    TZrUInt64 refSignatureHash;
    /** @brief 记录 caller 规范身份或声明的目标约束。
     * @note TypeSpec 用 caller token、TypeRef 用 targetMetadataToken、MemberRef 用 effect 目标、AssemblyRef 用 caller AssemblyRef；非零且非合法 AssemblyRef/Module 或 TypeSpec 映射才要求与 resolved 数值相等。
     */
    TZrMetadataToken expectedMetadataToken;
    /** @brief 记录 caller 规范签名或可选目标签名约束。
     * @note TypeSpec 用 caller 配对；TypeRef/MemberRef 用可为零的目标约束；AssemblyRef 用 caller relatedToken；合法映射可跳过 token 等值检查，但 hash/layout 检查继续。
     */
    TZrMetadataToken expectedSignatureToken;
    /** @brief 保存适用路径的 caller 预期签名 hash。
     * @note TypeSpec 是 caller 正文、TypeRef 是目标定义 hash、MemberRef 是 effect 目标、AssemblyRef 初始为引用 hash；compatchecker 只在非零时对比，记录存在不代表后来仍兼容。
     */
    TZrUInt64 expectedSignatureHash;
    /** @brief 记录 caller 所要求的 provider 模块 ABI。
     * @note TypeSpec/本地 union TypeDef binding 写零；TypeRef 来自 targetModuleSignatureHash；AssemblyRef 优先 effect 非零值；compatchecker 非零才核 resolved 模块 hash。
     */
    TZrUInt64 expectedModuleSignatureHash;
    /** @brief 保存 caller 布局契约的版本侧。
     * @note 普通 TypeSpec 与 AssemblyRef 映射写零；union TypeDef/TypeRef 取 caller layoutVersion；任一期望或实际 layout version/hash 非零时，版本及 hash 两项都必须相等。
     */
    TZrUInt32 expectedLayoutVersion;
    /** @brief 保存预期侧 UInt32 source 布局保留值。
     * @note 新 Upsert 存储清零且复制旧行保留原值，复用键不重置；当前 source IO 按 UInt32 读取或旧版本置零、writer 原值写出；本类型未见 CALLABLE 分类或兼容判定读取，不推断非零保留值含义。
     */
    TZrUInt32 reserved0;
    /** @brief 保存 caller 布局契约的 hash 侧。
     * @note 普通 TypeSpec 为零，union TypeDef/TypeRef 来自 caller layoutHash；布局检查由任一侧 version/hash 非零启动，不能只比较非零 hash 一项。
     */
    TZrUInt64 expectedLayoutHash;
    /** @brief 保存 provider 域中解析得到的实体身份。
     * @note TypeSpec/TypeDef 来自 provider 记录；MemberRef 来自 typed export；AssemblyRef 构造 MODULE RID 1；跨模块泛型还按此值在 provider 重新取实体和配对签名。
     */
    TZrMetadataToken resolvedMetadataToken;
    /** @brief 保存 provider 签名身份的快照。
     * @note TypeSpec/TypeDef 来自 providerSignatureRecord；MemberRef 来自 export；AssemblyRef 可在 module 配对缺失时保留零；泛型解析还核 provider 实际配对，不把表号视为存在性证明。
     */
    TZrMetadataToken resolvedSignatureToken;
    /** @brief 保存 provider 当前路径产生的签名 hash。
     * @note TypeSpec/TypeDef 来自 provider record；MemberRef 来自 symbol；AssemblyRef 可取 module signature record 或 module record fallback，缺 module 行可为零；后续重新读取可发现漂移。
     */
    TZrUInt64 resolvedSignatureHash;
    /** @brief 保存提供方函数的 moduleSignatureHash 快照。
     * @note TypeSpec、TypeDef、TypeRef、MemberRef/AssemblyRef 均来自 entry/provider 函数；compatchecker 按 expected 非零条件核，泛型 provider 解析另核当前 provider 值。
     */
    TZrUInt64 resolvedModuleSignatureHash;
    /** @brief 保存 provider 布局身份的版本侧。
     * @note 普通 TypeSpec/AssemblyRef 映射写零；union TypeDef/TypeRef 取 provider layoutVersion；任一侧声明布局时须与 expected version 以及 hash 同时一致。
     */
    TZrUInt32 resolvedLayoutVersion;
    /** @brief 保存实际侧 UInt32 source 布局保留值。
     * @note 新 Upsert 存储清零且复制旧行保留原值，复用键不重置；当前 source IO 按 UInt32 读取或旧版本置零、writer 原值写出；本类型未见 CALLABLE 分类或兼容判定读取，不推断非零保留值含义。
     */
    TZrUInt32 reserved1;
    /** @brief 保存 provider 布局身份的 hash 侧。
     * @note 普通 TypeSpec/AssemblyRef 映射写零；union TypeDef/TypeRef 取 provider layoutHash；不是 resolvedSignatureHash 替身，布局参与时核完整 version/hash 对。
     */
    TZrUInt64 resolvedLayoutHash;
} SZrMetadataTokenBinding;

/**
 * ## Summary
 * 一次 TypeSpec metadata 绑定扫描的计数与首失败样本。模块加载诊断读取本类型的结果，以区分 unmatched、union definition 和 layout 失配。
 *
 * ## Fields
 * | 成员 | 对调用方的用途 | 实际生产或消费引用 |
 * | --- | --- | --- |
 * | `callerTypeSpecCount` | 累计本扫描器适用 caller 记录数 | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:744）：本扫描器按上述专属分支更新 callerTypeSpecCount。 |
 * | `matchedTypeSpecCount` | 累计成功留下绑定结果的适用记录数 | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:842）：本扫描器按上述专属分支更新 matchedTypeSpecCount。 |
 * | `unmatchedTypeSpecCount` | 累计没有 provider 对应记录的项数 | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:753）：本扫描器按上述专属分支更新 unmatchedTypeSpecCount。 |
 * | `definitionMismatchCount` | 累计该扫描器的定义身份失配 | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:773）：本扫描器按上述专属分支更新 definitionMismatchCount。 |
 * | `layoutMismatchCount` | 累计该扫描器的布局身份失配 | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:802）：本扫描器按上述专属分支更新 layoutMismatchCount。 |
 * | `firstUnmatchedTypeSpecToken` | 保存首个未匹配 caller 身份 | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:754）：本扫描器按上述专属分支更新 firstUnmatchedTypeSpecToken。 |
 * | `firstUnmatchedSignatureHash` | 保存首个未匹配项的签名样本 | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:756）：本扫描器按上述专属分支更新 firstUnmatchedSignatureHash。 |
 * | `firstDefinitionMismatchTypeSpecToken` | 保存首个定义失配 caller 身份 | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:774）：本扫描器按上述专属分支更新 firstDefinitionMismatchTypeSpecToken。 |
 * | `firstExpectedDefinitionSignatureHash` | 保存首个定义失配的 caller 预期样本 | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:776）：本扫描器按上述专属分支更新 firstExpectedDefinitionSignatureHash。 |
 * | `firstActualDefinitionSignatureHash` | 保存首个定义失配的 provider 样本 | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:778）：本扫描器按上述专属分支更新 firstActualDefinitionSignatureHash。 |
 * | `firstLayoutMismatchTypeSpecToken` | 保存首个布局失配 caller 身份 | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:803）：本扫描器按上述专属分支更新 firstLayoutMismatchTypeSpecToken。 |
 * | `firstExpectedLayoutVersion` | 保存首个布局失配的 caller 版本 | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:805）：本扫描器按上述专属分支更新 firstExpectedLayoutVersion。 |
 * | `firstActualLayoutVersion` | 保存首个布局失配的 provider 版本 | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:807）：本扫描器按上述专属分支更新 firstActualLayoutVersion。 |
 * | `firstExpectedLayoutHash` | 保存首个布局失配的 caller hash | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:806）：本扫描器按上述专属分支更新 firstExpectedLayoutHash。 |
 * | `firstActualLayoutHash` | 保存首个布局失配的 provider hash | `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:808）：本扫描器按上述专属分支更新 firstActualLayoutHash。 |
 *
 * ## Invariants and lifecycle
 * status 非空时入口清零；只统计 caller TYPE_SPEC。缺配对签名或 sidecar 分配失败可提前返回，计数为已处理前缀，先前成功 sidecar 不回滚。当前项先写 TypeSpec、再写适用 union TypeDef；后者失败不回滚已写 TypeSpec，且不增加 matched 计数。
 * 首样本按扫描顺序保留，零 token 是未采样哨兵。unmatched 首 hash 来自 caller TypeSpec 正文；definition 样本来自 caller/provider union TypeDef，缺少一侧可为零；layout 四样本来自对应 union 定义。
 * 无状态 wrapper 容忍单纯 unmatched；不能用计数相等或此策略推断未发生后续硬失败。status 是 caller 提供的纯标量聚合，无资源取得/转移；诊断只读对应类型。
 *
 * ## References
 * `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:744）：TYPE_SPEC 过滤后累加 caller 计数。
 * `module_import_signature_record_type_spec_diagnostic`（zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.c:155）：TypeSpec 专属诊断读取该类型的 caller 计数。
 */
typedef struct SZrMetadataTypeSpecBindStatus {
    /** @brief 累计本扫描器适用 caller 记录数。
     * @note 只在caller token 表为 TYPE_SPEC过滤后递增；非空 status 在入口清零；缺签名或 sidecar 写入失败可提前返回，值只覆盖已进入扫描的前缀。
     */
    TZrUInt32 callerTypeSpecCount;
    /** @brief 累计成功留下绑定结果的适用记录数。
     * @note 只有本项 TypeSpec sidecar 和适用 union TypeDef sidecar 均成功才增加；先前成功项不因后续失败回滚。
     */
    TZrUInt32 matchedTypeSpecCount;
    /** @brief 累计没有 provider 对应记录的项数。
     * @note provider TypeSpec 的 hash/blob 匹配未找到才增加；不包含 definition/layout 失配；无状态 wrapper 会容忍单纯 unmatched，但计数不保证未发生后续硬失败。
     */
    TZrUInt32 unmatchedTypeSpecCount;
    /** @brief 累计该扫描器的定义身份失配。
     * @note union 一侧定义缺失或双方定义 hash/完整 blob 不等 时增加；此分支继续下一项，不登记当前绑定；相同字段名在另一 status 类型不是该计数的消费者。
     */
    TZrUInt32 definitionMismatchCount;
    /** @brief 累计该扫描器的布局身份失配。
     * @note 本项定义检查通过后，caller union TypeDef 与 provider TypeDef 的 version/hash 不等才增加。
     */
    TZrUInt32 layoutMismatchCount;
    /** @brief 保存首个未匹配 caller 身份。
     * @note 仅 firstUnmatchedTypeSpecToken 为零时写 callerRecord.token；零为未观察此失败哨兵，后续失败不覆盖，不是 provider token。
     */
    TZrMetadataToken firstUnmatchedTypeSpecToken;
    /** @brief 保存首个未匹配项的签名样本。
     * @note 仅与 firstUnmatchedTypeSpecToken 首次样本同时写 callerRecord.signatureHash，即 TypeSpec 正文 hash。
     */
    TZrUInt64 firstUnmatchedSignatureHash;
    /** @brief 保存首个定义失配 caller 身份。
     * @note 仅 firstDefinitionMismatchTypeSpecToken 为零时写 callerRecord.token；定义样本 hash 同分支写入，后续定义失配保留首项。
     */
    TZrMetadataToken firstDefinitionMismatchTypeSpecToken;
    /** @brief 保存首个定义失配的 caller 预期样本。
     * @note 与 firstDefinitionMismatchTypeSpecToken 同次写 caller union TypeDef.signatureHash；caller 定义缺失时写零；完整 blob 不等也可 hash 相等。
     */
    TZrUInt64 firstExpectedDefinitionSignatureHash;
    /** @brief 保存首个定义失配的 provider 样本。
     * @note 与 firstDefinitionMismatchTypeSpecToken 同次取 provider union TypeDef.signatureHash；provider 定义缺失时为零；与预期 hash 相同仍可能 blob 不等。
     */
    TZrUInt64 firstActualDefinitionSignatureHash;
    /** @brief 保存首个布局失配 caller 身份。
     * @note 仅 firstLayoutMismatchTypeSpecToken 为零时写 callerRecord.token，并与四个 expected/actual version/hash 样本同分支成组写入。
     */
    TZrMetadataToken firstLayoutMismatchTypeSpecToken;
    /** @brief 保存首个布局失配的 caller 版本。
     * @note 与首个 TypeSpec layout token 同次取 caller union TypeDef.layoutVersion；无该类失败保持入口零值。
     */
    TZrUInt32 firstExpectedLayoutVersion;
    /** @brief 保存首个布局失配的 provider 版本。
     * @note 与首个 TypeSpec layout token 同次取 provider union TypeDef.layoutVersion；无该类失败为零。
     */
    TZrUInt32 firstActualLayoutVersion;
    /** @brief 保存首个布局失配的 caller hash。
     * @note 与首个 TypeSpec layout token 同次取 caller union TypeDef.layoutHash，不是 TypeSpec signatureHash。
     */
    TZrUInt64 firstExpectedLayoutHash;
    /** @brief 保存首个布局失配的 provider hash。
     * @note 与首个 TypeSpec layout token 同次取 provider union TypeDef.layoutHash，不是定义 signatureHash。
     */
    TZrUInt64 firstActualLayoutHash;
} SZrMetadataTypeSpecBindStatus;

/**
 * ## Summary
 * 一次带明确 TYPE_DEF 目标的 TypeRef 绑定扫描的计数与首失败样本。TypeRef 专属诊断读取本类型，不能用同名 TypeSpec 字段替代证据。
 *
 * ## Fields
 * | 成员 | 对调用方的用途 | 实际生产或消费引用 |
 * | --- | --- | --- |
 * | `callerTypeRefCount` | 累计本扫描器适用 caller 记录数 | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:915）：本扫描器按上述专属分支更新 callerTypeRefCount。 |
 * | `matchedTypeRefCount` | 累计成功留下绑定结果的适用记录数 | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:973）：本扫描器按上述专属分支更新 matchedTypeRefCount。 |
 * | `unmatchedTypeRefCount` | 累计没有 provider 对应记录的项数 | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:926）：本扫描器按上述专属分支更新 unmatchedTypeRefCount。 |
 * | `definitionMismatchCount` | 累计该扫描器的定义身份失配 | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:936）：本扫描器按上述专属分支更新 definitionMismatchCount。 |
 * | `layoutMismatchCount` | 累计该扫描器的布局身份失配 | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:948）：本扫描器按上述专属分支更新 layoutMismatchCount。 |
 * | `firstUnmatchedTypeRefToken` | 保存首个未匹配 caller 身份 | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:927）：本扫描器按上述专属分支更新 firstUnmatchedTypeRefToken。 |
 * | `firstUnmatchedSignatureHash` | 保存首个未匹配项的签名样本 | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:929）：本扫描器按上述专属分支更新 firstUnmatchedSignatureHash。 |
 * | `firstDefinitionMismatchTypeRefToken` | 保存首个定义失配 caller 身份 | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:937）：本扫描器按上述专属分支更新 firstDefinitionMismatchTypeRefToken。 |
 * | `firstExpectedDefinitionSignatureHash` | 保存首个定义失配的 caller 预期样本 | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:939）：本扫描器按上述专属分支更新 firstExpectedDefinitionSignatureHash。 |
 * | `firstActualDefinitionSignatureHash` | 保存首个定义失配的 provider 样本 | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:940）：本扫描器按上述专属分支更新 firstActualDefinitionSignatureHash。 |
 * | `firstLayoutMismatchTypeRefToken` | 保存首个布局失配 caller 身份 | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:949）：本扫描器按上述专属分支更新 firstLayoutMismatchTypeRefToken。 |
 * | `firstExpectedLayoutVersion` | 保存首个布局失配的 caller 版本 | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:951）：本扫描器按上述专属分支更新 firstExpectedLayoutVersion。 |
 * | `firstActualLayoutVersion` | 保存首个布局失配的 provider 版本 | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:953）：本扫描器按上述专属分支更新 firstActualLayoutVersion。 |
 * | `firstExpectedLayoutHash` | 保存首个布局失配的 caller hash | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:952）：本扫描器按上述专属分支更新 firstExpectedLayoutHash。 |
 * | `firstActualLayoutHash` | 保存首个布局失配的 provider hash | `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:954）：本扫描器按上述专属分支更新 firstActualLayoutHash。 |
 *
 * ## Invariants and lifecycle
 * status 非空时入口清零；仅 TYPE_REF 且 targetMetadataToken 表为 TYPE_DEF 进入计数。目标查找按 token/hash、可选 signature/module、layout、基本名依次分类。
 * unmatched 首 hash 是 caller.targetSignatureHash；definition expected/actual 取目标约束与已找到 provider.signatureHash；layout 样本取 caller 引用与 provider 定义 version/hash，首失败 token 同分支成组保留。
 * 缺签名或 sidecar 写入失败提前返回，计数可为前缀，先前成功项不回滚；无该失败类别保持零值。纯标量聚合由调用者持有，无释放动作，不能把本类型同名字段与 TypeSpec 混用。
 *
 * ## References
 * `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus`（zr_vm_core/src/zr_vm_core/function/function_metadata_binding.c:915）：TYPE_REF+目标 TYPE_DEF 过滤后累加 caller 计数。
 * `module_import_signature_record_type_ref_diagnostic`（zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.c:195）：TypeRef 专属诊断读取该类型的 caller 计数。
 */
typedef struct SZrMetadataTypeRefBindStatus {
    /** @brief 累计本扫描器适用 caller 记录数。
     * @note 只在caller token 为 TYPE_REF 且 targetMetadataToken 表为 TYPE_DEF过滤后递增；非空 status 在入口清零；缺签名或 sidecar 写入失败可提前返回，值只覆盖已进入扫描的前缀。
     */
    TZrUInt32 callerTypeRefCount;
    /** @brief 累计成功留下绑定结果的适用记录数。
     * @note 只有配对签名存在且 TypeRef sidecar 写入成功才增加；先前成功项不因后续失败回滚。
     */
    TZrUInt32 matchedTypeRefCount;
    /** @brief 累计没有 provider 对应记录的项数。
     * @note 目标 token 对应 provider TYPE_DEF 未找到才增加；不包含 definition/layout 失配；有状态最终非零导致 false，status 空时不累计。
     */
    TZrUInt32 unmatchedTypeRefCount;
    /** @brief 累计该扫描器的定义身份失配。
     * @note 已找到目标 TYPE_DEF，但目标 hash 缺失/不同、可选 signature token 或 module hash 不等、基本名身份不兼容 时增加；此分支继续下一项，不登记当前绑定；相同字段名在另一 status 类型不是该计数的消费者。
     */
    TZrUInt32 definitionMismatchCount;
    /** @brief 累计该扫描器的布局身份失配。
     * @note TypeRef 已找到 provider 定义且目标签名/module 初步门禁通过，任一侧声明 layout 后 version/hash 不等才增加；该分支先于基本名比较。
     */
    TZrUInt32 layoutMismatchCount;
    /** @brief 保存首个未匹配 caller 身份。
     * @note 仅 firstUnmatchedTypeRefToken 为零时写 callerRecord.token；零为未观察此失败哨兵，后续失败不覆盖，不是 provider token。
     */
    TZrMetadataToken firstUnmatchedTypeRefToken;
    /** @brief 保存首个未匹配项的签名样本。
     * @note 仅与 firstUnmatchedTypeRefToken 首次样本同时写 callerRecord.targetSignatureHash，即预期 provider 定义 hash，不是引用正文 hash。
     */
    TZrUInt64 firstUnmatchedSignatureHash;
    /** @brief 保存首个定义失配 caller 身份。
     * @note 仅 firstDefinitionMismatchTypeRefToken 为零时写 callerRecord.token；定义样本 hash 同分支写入，后续定义失配保留首项。
     */
    TZrMetadataToken firstDefinitionMismatchTypeRefToken;
    /** @brief 保存首个定义失配的 caller 预期样本。
     * @note 与 firstDefinitionMismatchTypeRefToken 同次写 callerRecord.targetSignatureHash；零本身是定义匹配失败条件之一。
     */
    TZrUInt64 firstExpectedDefinitionSignatureHash;
    /** @brief 保存首个定义失配的 provider 样本。
     * @note 与 firstDefinitionMismatchTypeRefToken 同次取已找到 providerRecord.signatureHash；防御性未取得 record 则为零；模块/名称失败时 hash 也可相同。
     */
    TZrUInt64 firstActualDefinitionSignatureHash;
    /** @brief 保存首个布局失配 caller 身份。
     * @note 仅 firstLayoutMismatchTypeRefToken 为零时写 callerRecord.token，并与四个 expected/actual version/hash 样本同分支成组写入。
     */
    TZrMetadataToken firstLayoutMismatchTypeRefToken;
    /** @brief 保存首个布局失配的 caller 版本。
     * @note 与首个 TypeRef layout token 同次取 callerRecord.layoutVersion，即引用要求的定义布局版本。
     */
    TZrUInt32 firstExpectedLayoutVersion;
    /** @brief 保存首个布局失配的 provider 版本。
     * @note 与首个 TypeRef layout token 同次取 providerRecord.layoutVersion，防御性 record 空为零。
     */
    TZrUInt32 firstActualLayoutVersion;
    /** @brief 保存首个布局失配的 caller hash。
     * @note 与首个 TypeRef layout token 同次取 callerRecord.layoutHash，不是 targetSignatureHash。
     */
    TZrUInt64 firstExpectedLayoutHash;
    /** @brief 保存首个布局失配的 provider hash。
     * @note 与首个 TypeRef layout token 同次取 providerRecord.layoutHash，防御性 record 空为零。
     */
    TZrUInt64 firstActualLayoutHash;
} SZrMetadataTypeRefBindStatus;

#endif // ZR_VM_CORE_METADATA_TOKEN_H
