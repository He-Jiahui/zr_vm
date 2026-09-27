#ifndef ZR_VM_CORE_ZRP_METADATA_H
#define ZR_VM_CORE_ZRP_METADATA_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/metadata_token.h"

/* 固定小端头部标识、版本及 13 个 section 描述符的编码长度。 */
#define ZR_ZRP_METADATA_MAGIC ((TZrUInt32)0x4D50525Au)
#define ZR_ZRP_METADATA_VERSION ((TZrUInt16)4u)
#define ZR_ZRP_METADATA_SECTION_COUNT 13u
#define ZR_ZRP_METADATA_HEADER_SIZE 224u

/** @brief 头部 section 的固定顺序；池按字节寻址，定义表按行寻址。 */
typedef enum EZrZrpMetadataSectionKind {
    ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS = 0,
    ZR_ZRP_METADATA_SECTION_TYPE_DEFS = 1,
    ZR_ZRP_METADATA_SECTION_METHOD_DEFS = 2,
    ZR_ZRP_METADATA_SECTION_FIELD_DEFS = 3,
    ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS = 4,
    ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS = 5,
    ZR_ZRP_METADATA_SECTION_TYPE_SPECS = 6,
    ZR_ZRP_METADATA_SECTION_METHOD_SPECS = 7,
    ZR_ZRP_METADATA_SECTION_MODULE_REFS = 8,
    ZR_ZRP_METADATA_SECTION_STRING_POOL = 9,
    ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL = 10,
    ZR_ZRP_METADATA_SECTION_CONSTANT_POOL = 11,
    ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS = 12
} EZrZrpMetadataSectionKind;

/** @brief 单个 section 在 blob 内的字节范围及行数/行宽。
 * InitHeader 和 AOT 排版将空节四字段归零；验证器也接受合法非零 offset 的零长度节。
 */
/* BUG: 即使 blob 基址按 8 字节对齐，验证仍允许 TypeSpec 行从偏移 228 开始；
 * AttachZrpMetadata 的定义表校验将其强转为行指针并读取，产生未对齐访问。 */
typedef struct SZrZrpMetadataSection {
    TZrUInt32 offset;
    TZrUInt32 byteLength;
    TZrUInt32 count;
    TZrUInt32 elementSize;
} SZrZrpMetadataSection;

/** @brief 借用已校验 section 的原始字节；data 与 section 依附调用方 buffer/header。 */
typedef struct SZrZrpMetadataSectionView {
    const SZrZrpMetadataSection *section;
    const TZrByte *data;
    TZrSize byteLength;
    TZrUInt32 count;
    TZrUInt32 elementSize;
} SZrZrpMetadataSectionView;

/** @brief 借用池内连续字节，调用方须在底层 blob 有效期间使用。 */
typedef struct SZrZrpMetadataPoolSliceView {
    const TZrByte *data;
    TZrSize byteLength;
} SZrZrpMetadataPoolSliceView;

/** @brief 借用字符串池中以 NUL 终止的字节串；长度不含终止符。 */
typedef struct SZrZrpMetadataStringView {
    const char *data;
    TZrSize byteLength;
} SZrZrpMetadataStringView;

/** @brief 类型定义及其方法、字段和泛型参数的连续行区间。 */
typedef struct SZrZrpMetadataTypeDefRow {
    TZrMetadataToken token;
    TZrUInt32 nameStringOffset;
    TZrUInt32 namespaceStringOffset;
    TZrUInt32 firstMethodDefIndex;
    TZrUInt32 methodDefCount;
    TZrUInt32 firstFieldDefIndex;
    TZrUInt32 fieldDefCount;
    TZrUInt32 firstGenericParamIndex;
    TZrUInt32 genericParamCount;
    TZrUInt32 flags;
    TZrUInt32 typeLayoutId;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
} SZrZrpMetadataTypeDefRow;

/** @brief 方法定义的拥有者、签名位置和模块函数表索引。 */
typedef struct SZrZrpMetadataMethodDefRow {
    TZrMetadataToken token;
    TZrMetadataToken ownerTypeToken;
    TZrUInt32 nameStringOffset;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
    TZrUInt32 functionIndex;
    TZrUInt32 firstGenericParamIndex;
    TZrUInt32 genericParamCount;
    TZrUInt32 flags;
} SZrZrpMetadataMethodDefRow;

/** @brief 字段定义的签名、默认常量片段及运行时布局位置。 */
typedef struct SZrZrpMetadataFieldDefRow {
    TZrMetadataToken token;
    TZrMetadataToken ownerTypeToken;
    TZrUInt32 nameStringOffset;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
    TZrUInt32 defaultValueConstantPoolOffset;
    TZrUInt32 defaultValueConstantPoolLength;
    TZrUInt32 byteOffset;
    TZrUInt32 typeLayoutId;
    TZrUInt32 flags;
} SZrZrpMetadataFieldDefRow;

/** @brief 泛型参数所属定义及其约束行区间。 */
typedef struct SZrZrpMetadataGenericParamRow {
    TZrMetadataToken ownerToken;
    TZrUInt32 nameStringOffset;
    TZrUInt32 parameterIndex;
    TZrUInt32 firstConstraintIndex;
    TZrUInt32 constraintCount;
    TZrUInt32 flags;
} SZrZrpMetadataGenericParamRow;

/** @brief 单条泛型约束的目标类型及可选签名片段。 */
typedef struct SZrZrpMetadataGenericParamConstraintRow {
    TZrUInt32 genericParamIndex;
    TZrMetadataToken constraintTypeToken;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
} SZrZrpMetadataGenericParamConstraintRow;

/** @brief TypeSpec 签名及布局身份；signatureHash 用于跨模块匹配。 */
typedef struct SZrZrpMetadataTypeSpecRow {
    TZrMetadataToken token;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
    TZrUInt32 typeLayoutId;
    TZrUInt64 signatureHash;
} SZrZrpMetadataTypeSpecRow;

/** @brief 方法实例化与原始方法 token、实参签名片段的关联。 */
typedef struct SZrZrpMetadataMethodSpecRow {
    TZrMetadataToken token;
    TZrMetadataToken methodToken;
    TZrUInt32 instantiationBlobOffset;
    TZrUInt32 instantiationBlobLength;
    TZrUInt64 instantiationHash;
} SZrZrpMetadataMethodSpecRow;

/** @brief 外部模块名称、版本字符串与模块签名哈希。 */
typedef struct SZrZrpMetadataModuleRefRow {
    TZrMetadataToken token;
    TZrUInt32 nameStringOffset;
    TZrUInt32 versionStringOffset;
    TZrUInt32 flags;
    TZrUInt64 moduleSignatureHash;
} SZrZrpMetadataModuleRefRow;

/** @brief AOT 清单导出的目标名及类型或成员 token。 */
typedef struct SZrZrpMetadataManifestExportRow {
    TZrUInt32 kind;
    TZrUInt32 flags;
    TZrUInt32 targetStringOffset;
    TZrMetadataToken typeToken;
    TZrMetadataToken memberToken;
} SZrZrpMetadataManifestExportRow;

/** @brief 内存中的头部投影；WriteHeader/ReadHeader 按固定小端字段编码，非原样复制。
 * 13 个 section 与枚举顺序一一对应，ValidateHeader 负责单段边界和行宽检查。
 */
typedef struct SZrZrpMetadataHeader {
    TZrUInt32 magic;
    TZrUInt16 version;
    TZrUInt16 headerSize;
    TZrUInt32 flags;
    TZrUInt32 sectionCount;
    SZrZrpMetadataSection tokenRecords;
    SZrZrpMetadataSection typeDefs;
    SZrZrpMetadataSection methodDefs;
    SZrZrpMetadataSection fieldDefs;
    SZrZrpMetadataSection genericParams;
    SZrZrpMetadataSection genericParamConstraints;
    SZrZrpMetadataSection typeSpecs;
    SZrZrpMetadataSection methodSpecs;
    SZrZrpMetadataSection moduleRefs;
    SZrZrpMetadataSection stringPool;
    SZrZrpMetadataSection signatureBlobPool;
    SZrZrpMetadataSection constantPool;
    SZrZrpMetadataSection manifestExports;
} SZrZrpMetadataHeader;

/** @brief 初始化版本、魔数和 section 数量，留下全零空 section。 */
ZR_CORE_API void ZrCore_ZrpMetadata_InitHeader(SZrZrpMetadataHeader *header);

/** @brief 检查头部身份及每段在给定 buffer 长度内的范围和行宽。
 * @note 不解析行内容；定义关系另由 ValidateDefinitionTables 校验。
 */
ZR_CORE_API TZrBool ZrCore_ZrpMetadata_ValidateHeader(const SZrZrpMetadataHeader *header, TZrSize bufferLength);

/** @brief 将内存头部按固定小端布局写入 buffer 前 224 字节。
 * @return 参数、段布局有效且写入成功时返回真。
 */
ZR_CORE_API TZrBool ZrCore_ZrpMetadata_WriteHeader(TZrByte *buffer,
                                                   TZrSize bufferLength,
                                                   const SZrZrpMetadataHeader *header);

/** @brief 从字节流解码头部，再按整个 buffer 长度校验 section 边界。
 * @return 成功时填充 outHeader；失败时 outHeader 可能已有解码内容。
 */
ZR_CORE_API TZrBool ZrCore_ZrpMetadata_ReadHeader(const TZrByte *buffer,
                                                  TZrSize bufferLength,
                                                  SZrZrpMetadataHeader *outHeader);

/** @brief 按 sectionKind 借用已验证 section 的数据和描述符。
 * @note 空 section 的 data 为 NULL；输出视图不拥有 buffer/header。
 */
ZR_CORE_API TZrBool ZrCore_ZrpMetadata_GetSectionView(const TZrByte *buffer,
                                                      TZrSize bufferLength,
                                                      const SZrZrpMetadataHeader *header,
                                                      EZrZrpMetadataSectionKind sectionKind,
                                                      SZrZrpMetadataSectionView *outView);

/** @brief 对字符串、签名或常量池取有界字节片段。
 * @pre poolKind 必须属于三种池 section；返回指针依附输入 buffer。
 */
ZR_CORE_API TZrBool ZrCore_ZrpMetadata_GetPoolSlice(const TZrByte *buffer,
                                                    TZrSize bufferLength,
                                                    const SZrZrpMetadataHeader *header,
                                                    EZrZrpMetadataSectionKind poolKind,
                                                    TZrUInt32 offset,
                                                    TZrUInt32 byteLength,
                                                    SZrZrpMetadataPoolSliceView *outSlice);

/** @brief 从字符串池起始偏移读取一个带终止符的字符串视图。
 * @return 找到池内 NUL 终止符时返回真；byteLength 不含 NUL。
 */
ZR_CORE_API TZrBool ZrCore_ZrpMetadata_GetString(const TZrByte *buffer,
                                                 TZrSize bufferLength,
                                                 const SZrZrpMetadataHeader *header,
                                                 TZrUInt32 stringOffset,
                                                 SZrZrpMetadataStringView *outString);

/** @brief 校验完整签名 blob 的节点形状、深度及边界，不保留输入。 */
ZR_CORE_API TZrBool ZrCore_ZrpMetadata_ValidateSignatureBlob(const TZrByte *signatureBlob,
                                                             TZrSize signatureBlobLength);

/** @brief 将与头部长度一致的池 payload 拷入预留 section。
 * @pre payload 非空时须与目标范围不重叠；零长度可传 NULL。
 */
ZR_CORE_API TZrBool ZrCore_ZrpMetadata_WritePoolPayload(TZrByte *buffer,
                                                        TZrSize bufferLength,
                                                        const SZrZrpMetadataHeader *header,
                                                        EZrZrpMetadataSectionKind poolKind,
                                                        const TZrByte *payload,
                                                        TZrUInt32 payloadLength);

/** @brief 将固定行宽的定义表原始行复制到预留 section。
 * @pre rows 非空时须与目标范围不重叠，且使用本进程同 ABI 的行布局。
 */
/* TODO: 头部逐字节按小端编码，表行却用 native struct 的 sizeof 和 RawCopy；
 * 需核跨 ABI/端序产物的目标平台约束并建立兼容性测试。 */
ZR_CORE_API TZrBool ZrCore_ZrpMetadata_WriteDefinitionTablePayload(TZrByte *buffer,
                                                                   TZrSize bufferLength,
                                                                   const SZrZrpMetadataHeader *header,
                                                                   EZrZrpMetadataSectionKind tableKind,
                                                                   const void *rows,
                                                                   TZrUInt32 rowCount,
                                                                   TZrUInt32 elementSize);

/** @brief 校验各定义表 token 种类及部分拥有者、约束和行区间关系。
 * @pre buffer/header 已构成有效头部；行内容来自同 ABI 布局。
 */
ZR_CORE_API TZrBool ZrCore_ZrpMetadata_ValidateDefinitionTables(const TZrByte *buffer,
                                                                TZrSize bufferLength,
                                                                const SZrZrpMetadataHeader *header);

#endif // ZR_VM_CORE_ZRP_METADATA_H
