#ifndef ZR_VM_PARSER_ARTIFACT_PROJECTION_H
#define ZR_VM_PARSER_ARTIFACT_PROJECTION_H

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_core/artifact_schema.h"

typedef struct SZrParserArtifactPublicContract {
    TZrMetadataToken typeRefToken;
    TZrMetadataToken typeSpecToken;
    TZrMetadataToken signatureToken;
    TZrUInt32 layoutVersion;
    TZrUInt64 layoutHash;
    TZrUInt64 callableContractHash;
    TZrUInt64 moduleHash;
} SZrParserArtifactPublicContract;

struct ZrLibTypeDescriptor;
struct SZrReflectionTypeIdentity;
struct SZrFunction;

ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactCallBinding_BuildRows(
        const struct SZrFunction *function,
        TZrUInt32 functionIndex,
        SZrArtifactCallBindingRow *rows,
        TZrUInt32 rowCapacity,
        TZrUInt32 *outRowCount,
        SZrArtifactDiagnostic *diagnostic);

ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactLayout_ApplyNativeCapabilities(
        const struct ZrLibTypeDescriptor *typeDescriptor,
        TZrUInt64 stableSlotContractHash,
        SZrArtifactLayoutRow *layout,
        SZrArtifactDiagnostic *diagnostic);

ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactType_WriteSignature(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrByte *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrArtifactDiagnostic *diagnostic);

ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactType_InternSignature(
        struct SZrSemanticContext *context,
        SZrString *moduleIdentity,
        const TZrByte *signature,
        TZrSize signatureLength,
        TZrTypeId *outTypeId,
        SZrArtifactDiagnostic *diagnostic);

ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactType_BuildPublicIdentity(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrParserArtifactPublicContract *contract,
        TZrByte *signatureBuffer,
        TZrSize signatureBufferCapacity,
        TZrSize *outSignatureLength,
        SZrArtifactPublicIdentity *outIdentity,
        SZrArtifactDiagnostic *diagnostic);

ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactMetadata_BuildState(
        const struct SZrReflectionTypeIdentity *identity,
        const struct ZrLibTypeDescriptor *nativeTypeDescriptor,
        EZrArtifactMetadataPreservationState preservationState,
        TZrUInt32 retainedMemberCount,
        TZrUInt32 retainedPropertyCount,
        TZrUInt32 retainedMetaRecordCount,
        TZrUInt64 layoutHash,
        TZrUInt64 callableContractHash,
        SZrArtifactMetadataStateRow *outState,
        SZrArtifactDiagnostic *diagnostic);

#endif // ZR_VM_PARSER_ARTIFACT_PROJECTION_H

/**
 * @struct SZrParserArtifactPublicContract
 *
 * ## Summary
 * 承载调用者已确定的公共类型表身份及外部稳定合同，供BuildPublicIdentity组合签名。 准备artifact TypeRef/TypeSpec/Signature行与布局/调用/模块身份时按值填写。
 *
 * ## Fields
 * typeRefToken：TypeRef表非零RID身份。
 * typeSpecToken：TypeSpec表非零RID身份。
 * signatureToken：Signature表非零RID身份。
 * layoutVersion：非零布局版本，由合同提供者确定。
 * layoutHash：非零布局合同摘要，未在此结构内复算。
 * callableContractHash：非零调用合同摘要，不是宿主函数指针。
 * moduleHash：非零模块摘要，由模块身份提供者给定。
 *
 * ## Value model
 * 可按值复制的整数合同；token、版本和摘要是调用者声明的身份数据，不构成指针、可加载工件或字段真实性证明。
 *
 * ## Invariants and lifecycle
 * 三token必须分别属于TypeRef/TypeSpec/Signature表且RID非零；layoutVersion及三个哈希均非零。结构只含纯整数值，不持有对象或缓冲；有效构造仍不证明相应表行、布局或模块真的匹配，完整artifact图另验。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.h:7; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.h:54; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:791。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactCallBinding_BuildRows( const struct SZrFunction *function, TZrUInt32 functionIndex, SZrArtifactCallBindingRow *rows, TZrUInt32 rowCapacity, TZrUInt32 *outRowCount, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 将函数的非NONE调用缓存投影为按原缓存槽排列的artifact绑定行，并支持只校验/查询数量。 AOT验证整张函数表时以rows=NULL调用；需要固定行数据的消费者提供输出数组。
 *
 * ## Parameters
 * @param[in] function 非空且已建立缓存/指令/常量计数的函数，整个两轮遍历期间稳定。
 * @param[in] functionIndex artifact函数表的稳定行索引，入口要求不为SLOT_NONE。
 * @param[out] rows 可空输出数组；为空仅允许rowCapacity=0，用于只查询数量。
 * @param[in] rowCapacity rows可写的行容量，非空时须容纳全部有效槽。
 * @param[out] outRowCount 必填数量输出；入口先清0，成功才交付完整数量。
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 *
 * ## Returns
 * @return OK时outRowCount为实际行数；非空rows仅在全量行编码校验与容量检查通过后写入。普通失败使非空outRowCount为0且不写rows；诊断可空。
 *
 * ## Constraints and lifecycle
 * function/outRowCount必填，functionIndex不能为SLOT_NONE；非零缓存长度须有缓存数组。rows=NULL仅允许rowCapacity=0。两轮间函数/缓存和输入字符串状态必须稳定，rows与缓存不得重叠；不分配。只核本函数指令和本地常量边界，祖先/外部重定位目标和目标签名由AOT等上层继续解析。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.h:21; zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_bindings.c:111; tests/parser/test_call_binding_artifact.c:191。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactLayout_ApplyNativeCapabilities( const struct ZrLibTypeDescriptor *typeDescriptor, TZrUInt64 stableSlotContractHash, SZrArtifactLayoutRow *layout, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 把原生STABLE_SLOT_SOURCE协议位与其非零契约哈希协商到artifact布局能力。 原生Pool等描述符构造布局行后调用，保持描述符协议与持久布局一致。
 *
 * ## Parameters
 * @param[in] typeDescriptor 非空原生类型描述符，protocolMask在同步投影期间稳定。
 * @param[in] stableSlotContractHash 有STABLE_SLOT_SOURCE时必须非零，无该协议时必须为0。
 * @param[in,out] layout 非空布局行，只在全部能力门禁成功后补充能力位/哈希，其余字段不变。
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 *
 * ## Returns
 * @return OK表示本能力局部一致并清零可选诊断；普通失败不修改layout，给出INVALID_ARGUMENT、ILLEGAL_TOKEN或CONTRACT_HASH_MISMATCH。
 *
 * ## Constraints and lifecycle
 * typeDescriptor/layout必填且保持独立有效。没有协议位时要求输入哈希、已有能力位与已有哈希均为零；有协议位时哈希非零，已有非零哈希必须相同。全部检查通过后才OR能力位/写哈希；其他布局字段原样，未验证完整布局。无分配和内部同步，调用方独占layout。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.h:29; tests/container/test_generational_pool_artifact.c:119; tests/container/test_generational_pool_artifact.c:288; tests/container/test_generational_pool_artifact.c:298。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactType_WriteSignature( const struct SZrSemanticContext *context, TZrTypeId typeId, TZrByte *buffer, TZrSize bufferCapacity, TZrSize *outWrittenSize, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 将已有canonical类型编码为通过Core语法验证的稳定artifact签名。 源合同、公共身份和可调用消费者在持久化或跨层比对前使用。
 *
 * ## Parameters
 * @param[in] context 已初始化且在查询期间稳定的semantic context，其state和canonical池有效。
 * @param[in] typeId 属于当前context分析代的canonical类型ID，不是跨context通用句柄。
 * @param[out] buffer 非空、bufferCapacity字节可写签名区；失败可能留下已写前缀。
 * @param[in] bufferCapacity 真实可写缓冲容量，不能用NULL缓冲执行计数查询。
 * @param[out] outWrittenSize 必填长度输出；入口先清0，完整写出并通过Core验证后发布长度。
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 *
 * ## Returns
 * @return 普通入口即把非空outWrittenSize设0并清可选诊断；只有编码与ValidateSignature均OK才发布真实长度。返回参数、容量、类型、token或语法状态。
 *
 * ## Constraints and lifecycle
 * context/buffer/outWrittenSize必填；typeId属于稳定context，bufferCapacity与真实可写区一致，输出不得破坏context/input。先递归写出再整体验证；不分配或修改context。不可用NULL buffer查询长度；失败可留下buffer部分字节，重试须忽略旧部分并提供合适容量。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.h:35; zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c:813; tests/parser/test_canonical_consumers.c:1244; zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c:241; tests/parser/test_reference_callable_consumers.c:219。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactType_InternSignature( struct SZrSemanticContext *context, SZrString *moduleIdentity, const TZrByte *signature, TZrSize signatureLength, TZrTypeId *outTypeId, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 验证完整artifact签名字节并在调用者semantic context中重新驻留canonical类型。 二进制签名导入和源/二进制合同对照时使用，不复用其他context的数值ID。
 *
 * ## Parameters
 * @param[in,out] context 非空可变semantic context；驻留可分配节点并更改其类型池，不提供回滚或内部同步。
 * @param[in] moduleIdentity 可空的名义类型域字符串；节点可能保留引用，须按context/GC根规则保持可达。
 * @param[in] signature signatureLength个只读字节；整体校验至递归导入结束期间不变。
 * @param[in] signatureLength 完整单根签名的字节数，受Core签名长度上界约束。
 * @param[out] outTypeId 有效ID输出存储；入口先置INVALID，内部helper早拒绝不统一赋值。
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 *
 * ## Returns
 * @return 先把非空outTypeId设INVALID；语法失败立即返回，语义解码失败返回具体状态。OK时恰好消费全部字节并交付context内有效ID；尾部不符再重置INVALID。无新对象由调用者单独释放，context正常Reset/Free负责驻留节点容器。
 *
 * ## Constraints and lifecycle
 * context/signature/outTypeId必填；moduleIdentity可空，名义类型节点可保留该字符串引用，调用者遵循context/GC规则保持其可达。signature仅借用至返回且验证后不得并发修改。会分配临时数组、字符串和context节点；context由调用者独占，无事务回滚，OOM可能非局部Throw；普通状态拒绝也不回滚已驻留节点。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.h:43; tests/parser/test_artifact_schema_source_roundtrip.c:117; tests/parser/test_artifact_schema.c:984; zr_vm_parser/src/zr_vm_parser/types/canonical_type_lifecycle.c:7; zr_vm_parser/src/zr_vm_parser/types/canonical_type_lifecycle.c:37; tests/parser/test_reference_callable_consumers.c:429。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactType_BuildPublicIdentity( const struct SZrSemanticContext *context, TZrTypeId typeId, const SZrParserArtifactPublicContract *contract, TZrByte *signatureBuffer, TZrSize signatureBufferCapacity, TZrSize *outSignatureLength, SZrArtifactPublicIdentity *outIdentity, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 将canonical签名与调用者提供的稳定表token、布局/调用/模块合同组合为公共artifact身份。 源与二进制消费者用同一合同重建身份并比较，供完整artifact校验使用。
 *
 * ## Parameters
 * @param[in] context 已初始化且在查询期间稳定的semantic context，其state和canonical池有效。
 * @param[in] typeId 属于当前context分析代的canonical类型ID，不是跨context通用句柄。
 * @param[in] contract 非空公共合同值，三token/版本和哈希满足本接口的非零/表约束。
 * @param[out] signatureBuffer 非空签名输出区，与输入及identity不重叠，失败可留部分内容。
 * @param[in] signatureBufferCapacity signatureBuffer的真实可写字节容量。
 * @param[out] outSignatureLength 必填长度输出；参数门禁早拒绝可保持旧值，进入WriteSignature后先清0。
 * @param[out] outIdentity 必填公共身份输出，入口先清零，签名成功后才完整填写。
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 *
 * ## Returns
 * @return OK时输出签名长度及完整identity；任何普通失败outIdentity全零。入口参数早拒绝不写outSignatureLength，进入WriteSignature后该长度先清0；signatureBuffer可留部分写出内容。
 *
 * ## Constraints and lifecycle
 * context/contract/signatureBuffer/outSignatureLength/outIdentity必填且输入输出不互相覆盖；三token须为对应TypeRef/TypeSpec/Signature表的非零RID，版本/哈希非零。先清identity再校验；只有WriteSignature成功后填身份。不验证合同哈希代表的真实布局或模块，完整图由下层另核。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.h:51; tests/parser/test_artifact_schema_source_roundtrip.c:106; tests/parser/test_artifact_schema_source_roundtrip.c:127; tests/parser/test_artifact_schema.c:897; tests/parser/test_artifact_schema.c:994。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactMetadata_BuildState( const struct SZrReflectionTypeIdentity *identity, const struct ZrLibTypeDescriptor *nativeTypeDescriptor, EZrArtifactMetadataPreservationState preservationState, TZrUInt32 retainedMemberCount, TZrUInt32 retainedPropertyCount, TZrUInt32 retainedMetaRecordCount, TZrUInt64 layoutHash, TZrUInt64 callableContractHash, SZrArtifactMetadataStateRow *outState, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 将已确定的反射身份、保留级别及稳定契约投影成metadata state行与可复算摘要。 由源/native/binary对照消费者准备局部状态；完整artifact写读端另做跨表图一致性校验。
 *
 * ## Parameters
 * @param[in] identity 非空反射身份，TypeDef token/签名摘要/metadataGeneration已设置。
 * @param[in] nativeTypeDescriptor 可空原生描述符；非空时数组和名称须可读，参与类别/计数对账。
 * @param[in] preservationState IDENTITY_ONLY至FULL的保留级别，与三个保留数及ERASED限制一致。
 * @param[in] retainedMemberCount 调用者声明保留的成员数；本函数不会实际创建成员。
 * @param[in] retainedPropertyCount 调用者声明保留的属性数；须与采用的native投影计数规则相符。
 * @param[in] retainedMetaRecordCount 保留记录数；identity-only和Members级别要求为0。
 * @param[in] layoutHash 非零稳定布局摘要，本层不从布局重新推导。
 * @param[in] callableContractHash 非零调用合同摘要，本层不从方法实现重新推导。
 * @param[out] outState 必填且与输入不重叠的状态行，入口先清零，成功最后计算摘要。
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 *
 * ## Returns
 * @return OK时所有字段齐备后计算metadataHash；普通失败使非空outState保持全零并返回具体状态。提供native时还受原型映射、形状及声明计数规则约束，不能把OK视为实际native成员已物化。
 *
 * ## Constraints and lifecycle
 * identity/outState必填且输出不与identity或native描述符重叠；可选native数组/名称借用至返回。先清输出/诊断，再核TypeDef RID、签名/代数、类别、保留级别和非零布局/调用哈希；identity-only不留表，Members不留记录，ERASED仅identity-only。无分配，不代替跨表计数证明。BUG: 仅属性数不符时仍将member数写入expected/actual，可能同为1；该既有诊断问题详见native_count_diagnostic块。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.h:61; tests/parser/test_artifact_schema_metadata_graph.c:412; tests/parser/test_artifact_schema_metadata_graph.c:425; tests/parser/test_artifact_schema_metadata_graph.c:492; zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c:95。这些位置分别为本单元定义/使用或下层合同证据。
 */
