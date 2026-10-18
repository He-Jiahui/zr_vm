#ifndef ZR_VM_PARSER_ARTIFACT_EXEC_IR_H
#define ZR_VM_PARSER_ARTIFACT_EXEC_IR_H

#include "zr_vm_core/artifact_exec_ir.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"

typedef struct SZrExecIrReader {
    const TZrByte *bytes;
    TZrUInt32 length;
} SZrExecIrReader;

typedef struct SZrExecIrWriter {
    TZrByte *bytes;
    TZrUInt32 capacity;
    TZrUInt32 written;
} SZrExecIrWriter;

ZR_PARSER_API TZrBool ZrParser_ExecIr_Read(
        SZrExecIrReader *reader, SZrArtifactExecIrView *module,
        SZrArtifactExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_Write(
        const SZrArtifactExecIrDocument *module, SZrExecIrWriter *writer,
        SZrArtifactExecIrDiagnostic *diagnostic);

/* Persist the first canonical ExecIR scalar subset as a ZRAF v6 ZRO file.
 * metadata must contain the seven validated identity sections, and filename
 * is opened only after all graph and artifact encoding succeeds. */
ZR_PARSER_API EZrArtifactStatus ZrParser_ExecIr_WriteCanonicalZroFile(
        const SZrArtifactDocument *metadata, const SZrExecIrModule *module,
        const char *filename, SZrArtifactDiagnostic *diagnostic);

#endif

/**
 * @struct SZrExecIrReader
 *
 * ## Summary
 * 封装待解析的有界ERI1字节视图，不取得缓冲区所有权。 调用ZrParser_ExecIr_Read之前由调用者设置。
 *
 * ## Fields
 * bytes：调用者持有的只读原始字节；不是本对象分配的内存。
 * length：bytes可读范围的32位字节长度，由下层验证结构边界。
 *
 * ## Value model
 * 两个借用描述字段；无初始化器、读取位置或析构行为。
 *
 * ## Invariants
 * 不得把length当作已验证格式长度；仅在Read成功后消费其输出视图。输出视图及诊断存储应与输入字节独立。
 *
 * ## Constraints and lifecycle
 * bytes指向至少length字节可读存储，length以字节计；包装器不推进游标、不修改这两个字段。成功视图继续借用同一存储，调用者须保持字节有效且稳定至最后一次视图访问；本结构不执行释放或同步。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h:9; zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h:10; zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h:11; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:6; zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:100; zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:109; zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:158
 */

/**
 * @struct SZrExecIrWriter
 *
 * ## Summary
 * 描述调用者提供的ERI1输出缓冲区及本次成功写入长度。 调用ZrParser_ExecIr_Write前提供bytes/capacity，返回后检查布尔结果与written。
 *
 * ## Fields
 * bytes：借用可写输出存储，不转移所有权。
 * capacity：输出存储的字节容量，传递给Core编码器。
 * written：包装器记录的成功字节数；不能单独替代本次返回值。
 *
 * ## Value model
 * 输入容量和输出计数共存的描述对象，初始written由调用者管理。
 *
 * ## Invariants
 * 源文档及节数据在调用期间保持有效，并与目的缓冲区分离；下层重叠支持尚无可依赖的合同。
 *
 * ## Constraints and lifecycle
 * bytes至少容纳capacity字节可写存储；writer不分配或释放缓冲区。Core调用失败时包装器把written置0；module或writer为空而提前拒绝时不重置已有字段。成功只使[0,written)成为本次编码结果，尾部不是结果。跨线程共享此结构或缓冲区需调用者同步。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h:14; zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h:15; zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h:16; zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h:17; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:18; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:24; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:27; zr_vm_core/include/zr_vm_core/artifact_exec_ir.h:129
 */

/**
 * @fn ZR_PARSER_API TZrBool ZrParser_ExecIr_Read(SZrExecIrReader *reader, SZrArtifactExecIrView *module, SZrArtifactExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 把ERI1内存解码器暴露为Parser布尔接口。 已有字节缓冲区需要取得经验证的借用视图时调用；不是文件加载入口。
 *
 * ## Parameters
 * @param[in] reader 非空字节描述；对象字段只读，底层bytes至少可读length字节。
 * @param[out] module 非空Core产物视图存储；只在TRUE后使用，寿命受reader字节约束。
 * @param[out] diagnostic 可空Core诊断存储；包装器空参数早退时保持原值。
 *
 * ## Returns
 * @return Core返回ZR_ARTIFACT_EXEC_IR_OK时为TRUE；否则为FALSE。布尔失败不说明包装器已进入Core或写过诊断。
 *
 * ## Constraints and lifecycle
 * reader和module须非空。包装器早期空参数返回FALSE而不设置诊断或清零已有module；进入Core后，其失败路径清零视图。成功仅发布借用原始字节的视图，不构造或执行SZrExecIrModule。输入、输出与诊断不得相互覆盖；无内部并发同步。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h:20; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:3; zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:95; zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:100; zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:158; tests/library/test_ssa_schema_relocation.c:91
 */

/**
 * @fn ZR_PARSER_API TZrBool ZrParser_ExecIr_Write(const SZrArtifactExecIrDocument *module, SZrExecIrWriter *writer, SZrArtifactExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 把结构化ERI1文档编码到调用者缓冲区并发布成功长度。 调用者已准备完整SZrArtifactExecIrDocument及独立输出存储时调用；不执行文件I/O。
 *
 * ## Parameters
 * @param[in] module 借用非空ERI1文档及其所有节数据，须满足Core格式与期望哈希约束。
 * @param[in,out] writer 非空描述；读取bytes/capacity，按结果更新written。
 * @param[out] diagnostic 可空Core诊断；包装器空参数早退时不更新。
 *
 * ## Returns
 * @return TRUE表示Core编码成功且written已发布；FALSE区分为包装器参数早退或Core拒绝，不能仅凭旧written判断成功。
 *
 * ## Constraints and lifecycle
 * module与writer须非空；该早退不改writer->written或诊断。进入Core后失败会把written置0，成功才记录Core实际长度。源节数据、文档及诊断与目的缓冲区保持分离和稳定；包装器不分配缓冲区，也不回滚或清空目的字节。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h:23; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:14; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:24; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:27; zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:68; zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:85; tests/library/test_ssa_schema_relocation.c:87; tests/library/test_ssa_schema_relocation.c:143
 */

/**
 * @fn ZR_PARSER_API EZrArtifactStatus ZrParser_ExecIr_WriteCanonicalZroFile(const SZrArtifactDocument *metadata, const SZrExecIrModule *module, const char *filename, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 把当前标量codec支持的规范ExecIR图和匹配元数据封装为ZRAF v6 ZRO文件。 需要持久化该受限图与七个身份节时调用；本接口不保证任意ExecIR图都能编码。
 *
 * ## Parameters
 * @param[in] metadata 借用七身份节及其类型化数据；不接管、修改或替调用者释放。
 * @param[in] module 借用受当前scalar codec支持的规范图，调用期间不得并发修改。
 * @param[in] filename 非空NUL结尾目标路径；父目录、权限和Windows窄字符路径编码由宿主保证。
 * @param[out] diagnostic 可空状态诊断；本适配器失败帮助函数会清零后设置status/sectionKind。
 *
 * ## Returns
 * @return OK表示本次替换调用成功；参数/开临时文件失败归INVALID_ARGUMENT，适配器分配失败归BUFFER_TOO_SMALL，标量/嵌套编码及写入关闭替换失败归INVALID_SECTION；外层编码器错误可直接传播。
 *
 * ## Constraints and lifecycle
 * 输入元数据须是flags为0的ZRO，恰含七类唯一mandatory身份节，并与模块及首函数合同匹配；所有类型化行/图内存至返回前有效。先完成标量、ERI1嵌套包与外层ZRAF全部编码，再在目标同目录尝试32个排他临时文件名；写完且关闭成功才做平台替换。失败只尝试清理临时文件，remove失败可能留残余；不创建父目录，不承诺掉电持久性或并发写者隔离。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h:30; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:92; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:113; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:136; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:203; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:219; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:232; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:240; tests/library/test_ssa_exec_ir_artifact_v6.c:502; tests/cmake/ssa-tests.cmake:1016
 */
