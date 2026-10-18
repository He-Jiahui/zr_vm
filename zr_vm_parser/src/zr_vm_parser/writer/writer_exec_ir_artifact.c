#include "artifact/artifact_exec_ir.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/artifact_exec_ir_scalar.h"
#include "zr_vm_common/zr_aot_abi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef ZR_PLATFORM_WIN
#include <windows.h>
#endif

#ifdef ZR_ARTIFACT_TEST_IO_FAILURE
static TZrBool execIrArtifactForceWriteFailure = ZR_FALSE;
void ZrParser_ExecIrArtifact_TestInjectWriteFailure(TZrBool enabled);
void ZrParser_ExecIrArtifact_TestInjectWriteFailure(TZrBool enabled) {
    execIrArtifactForceWriteFailure = enabled;
}
#endif

static EZrArtifactStatus exec_ir_writer_fail(
        SZrArtifactDiagnostic *diagnostic, EZrArtifactStatus status,
        TZrUInt32 sectionKind) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
        diagnostic->sectionKind = sectionKind;
    }
    return status;
}

static TZrBool exec_ir_writer_metadata_is_supported(
        const SZrArtifactDocument *metadata, const SZrExecIrModule *module) {
    static const EZrArtifactSectionKind requiredSections[] = {
        ZR_ARTIFACT_SECTION_STRING_HEAP,
        ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE,
        ZR_ARTIFACT_SECTION_TYPE_REF_TABLE,
        ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE,
        ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
        ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
        ZR_ARTIFACT_SECTION_LAYOUT_TABLE
    };
    const SZrArtifactContractRow *contract = ZR_NULL;
    if (metadata->kind != ZR_ARTIFACT_KIND_ZRO || metadata->flags != 0u ||
        metadata->sectionCount != 7u || metadata->sections == ZR_NULL)
        return ZR_FALSE;
    for (TZrUInt32 required = 0u; required < 7u; ++required) {
        TZrBool found = ZR_FALSE;
        for (TZrUInt32 index = 0u; index < metadata->sectionCount; ++index) {
            const SZrArtifactSectionInput *section = &metadata->sections[index];
            if (section->kind == requiredSections[required] &&
                section->flags == ZR_ARTIFACT_SECTION_FLAG_MANDATORY) {
                if (found) return ZR_FALSE;
                found = ZR_TRUE;
                if (section->kind == ZR_ARTIFACT_SECTION_CONTRACT_TABLE) {
                    if (section->elementCount != 1u || section->data == ZR_NULL)
                        return ZR_FALSE;
                    contract = (const SZrArtifactContractRow *)section->data;
                }
            }
        }
        if (!found) return ZR_FALSE;
    }
    if (contract == ZR_NULL ||
        metadata->identity.moduleHash != module->moduleHash ||
        metadata->identity.moduleHash != module->contract.moduleHash ||
        metadata->identity.moduleHash !=
                module->functions[0].contract.moduleHash ||
        metadata->identity.signatureHash != module->functions[0].signatureHash ||
        metadata->identity.signatureHash != module->contract.signatureHash ||
        metadata->identity.signatureHash !=
                module->functions[0].contract.signatureHash ||
        metadata->identity.layoutHash != module->contract.layoutHash ||
        metadata->identity.layoutHash !=
                module->functions[0].contract.layoutHash ||
        contract->memberToken != module->functions[0].functionToken ||
        contract->signatureToken != metadata->identity.signatureToken ||
        contract->contractHash != metadata->identity.callableContractHash ||
        contract->parameterCount != 0u || contract->flags != 0u ||
        contract->receiverEffect != ZR_ARTIFACT_RECEIVER_NONE ||
        contract->refExportEffect != ZR_ARTIFACT_REF_EXPORT_NONE ||
        contract->escapeFlags != 0u ||
        contract->abiLoweringKind != ZR_ARTIFACT_ABI_LOWERING_NONE)
        return ZR_FALSE;
    return ZR_TRUE;
}

/* Persist one verified canonical scalar graph in a ZRAF ZRO. All encoders run
 * before fopen, so validation failures cannot replace an existing artifact. */
EZrArtifactStatus ZrParser_ExecIr_WriteCanonicalZroFile(
        const SZrArtifactDocument *metadata, const SZrExecIrModule *module,
        const char *filename, SZrArtifactDiagnostic *diagnostic) {
    SZrArtifactExecIrSectionInput nestedSection;
    SZrArtifactExecIrDocument nestedDocument;
    SZrArtifactExecIrDiagnostic nestedDiagnostic;
    SZrArtifactSectionInput sections[8];
    SZrArtifactDocument document;
    TZrByte *payload = ZR_NULL;
    TZrByte *nestedBytes = ZR_NULL;
    TZrByte *outerBytes = ZR_NULL;
    TZrUInt32 nestedSize = 0u;
    TZrUInt32 nestedWritten = 0u;
    TZrUInt32 payloadSize = 0u;
    TZrSize outerSize = 0u;
    TZrSize outerWritten = 0u;
    EZrArtifactStatus status;
    FILE *file = ZR_NULL;
    char *temporaryFilename = ZR_NULL;
    size_t filenameLength;
    TZrBool writeFailed = ZR_FALSE;
    if (metadata == ZR_NULL || module == ZR_NULL || filename == ZR_NULL ||
        filename[0] == '\0')
        return exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u);
    if (ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                module, &payloadSize, &nestedDiagnostic) !=
        ZR_ARTIFACT_EXEC_IR_OK)
        return exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE);
    payload = (TZrByte *)malloc((size_t)payloadSize);
    if (payload == ZR_NULL)
        return exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE);
    if (ZrCore_ArtifactExecIrScalar_Write(module, payload, payloadSize,
                                           &nestedDiagnostic) !=
        ZR_ARTIFACT_EXEC_IR_OK) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE);
        goto done;
    }
    if (!exec_ir_writer_metadata_is_supported(metadata, module)) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u);
        goto done;
    }
    memset(&nestedSection, 0, sizeof(nestedSection));
    nestedSection.kind = ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR;
    nestedSection.elementCount = payloadSize;
    nestedSection.elementSize = 1u;
    nestedSection.data = payload;
    nestedSection.byteLength = payloadSize;
    memset(&nestedDocument, 0, sizeof(nestedDocument));
    nestedDocument.abiVersion = ZR_VM_AOT_ABI_VERSION;
    nestedDocument.moduleHash = module->moduleHash;
    nestedDocument.execIrHash = ZrCore_ArtifactExecIr_HashBytes(
            payload, payloadSize);
    nestedDocument.sectionCount = 1u;
    nestedDocument.sections = &nestedSection;
    if (ZrCore_ArtifactExecIr_GetEncodedSize(&nestedDocument, &nestedSize,
                                             &nestedDiagnostic) !=
        ZR_ARTIFACT_EXEC_IR_OK || nestedSize == 0u) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE);
        goto done;
    }
    nestedBytes = (TZrByte *)malloc(nestedSize);
    if (nestedBytes == ZR_NULL) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE);
        goto done;
    }
    if (ZrCore_ArtifactExecIr_Write(&nestedDocument, nestedBytes, nestedSize,
                                    &nestedWritten, &nestedDiagnostic) !=
        ZR_ARTIFACT_EXEC_IR_OK || nestedWritten != nestedSize) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE);
        goto done;
    }
    memcpy(sections, metadata->sections,
           metadata->sectionCount * sizeof(sections[0]));
    sections[7] = (SZrArtifactSectionInput){
        ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
        ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
        nestedSize, nestedBytes};
    document = *metadata;
    document.sectionCount = 8u;
    document.sections = sections;
    status = ZrCore_Artifact_GetEncodedSize(&document, &outerSize,
                                            diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK || outerSize == 0u) goto done;
    outerBytes = (TZrByte *)malloc(outerSize);
    if (outerBytes == ZR_NULL) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL, 0u);
        goto done;
    }
    status = ZrCore_Artifact_Write(&document, outerBytes, outerSize,
                                   &outerWritten, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK || outerWritten != outerSize) {
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = exec_ir_writer_fail(diagnostic,
                    ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u);
        goto done;
    }
    filenameLength = strlen(filename);
    if (filenameLength > SIZE_MAX - 32u) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u);
        goto done;
    }
    temporaryFilename = (char *)malloc(filenameLength + 32u);
    if (temporaryFilename == ZR_NULL) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL, 0u);
        goto done;
    }
    for (unsigned attempt = 0u; attempt < 32u; ++attempt) {
        int length = snprintf(temporaryFilename, filenameLength + 32u,
                              "%s.zr-tmp-%u", filename, attempt);
        if (length < 0 || (size_t)length >= filenameLength + 32u) break;
        file = fopen(temporaryFilename, "wbx");
        if (file != ZR_NULL || errno != EEXIST) break;
    }
    if (file == ZR_NULL) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u);
        goto done;
    }
    if (fwrite(outerBytes, 1u, outerWritten, file) != outerWritten)
        writeFailed = ZR_TRUE;
#ifdef ZR_ARTIFACT_TEST_IO_FAILURE
    if (execIrArtifactForceWriteFailure) writeFailed = ZR_TRUE;
#endif
    if (fclose(file) != 0) writeFailed = ZR_TRUE;
    if (writeFailed) {
        remove(temporaryFilename);
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u);
        goto done;
    }
#ifdef ZR_PLATFORM_WIN
    if (!MoveFileExA(temporaryFilename, filename,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
#else
    if (rename(temporaryFilename, filename) != 0) {
#endif
        remove(temporaryFilename);
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u);
        goto done;
    }
    status = exec_ir_writer_fail(diagnostic, ZR_ARTIFACT_STATUS_OK, 0u);
done:
    free(temporaryFilename);
    free(outerBytes);
    free(nestedBytes);
    free(payload);
    return status;
}

/**
 * @var execIrArtifactForceWriteFailure
 *
 * ## Summary
 * 为同一测试编译单元中的文件写入分支保存故障注入开关。 仅定义ZR_ARTIFACT_TEST_IO_FAILURE时存在，由测试setter设置，在fwrite之后、fclose之前读取。
 *
 * ## Value model
 * 只在测试宏开启的编译变体中存在的文件静态布尔值；生产变体不提供此状态。
 *
 * ## Invariants
 * 不能让其他线程并发setter与读者；本值不取得文件或缓冲区所有权。
 *
 * ## Constraints and lifecycle
 * static TZrBool初值为ZR_FALSE，setter原样赋enabled；条件分支按非零为真，不把赋值表述为0/1规范化。开关持续至下次setter或进程结束；不转交引用，不构造fixture或VM对象。全局可变状态无锁，测试调用者须串行并在注入后恢复；它只强制writeFailed，不模拟短写字节数或关闭/重命名故障。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:15; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:16; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:19; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:229; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:230; tests/library/test_ssa_exec_ir_artifact_v6.c:507; tests/library/test_ssa_exec_ir_artifact_v6.c:512
 */

/**
 * @fn void ZrParser_ExecIrArtifact_TestInjectWriteFailure(TZrBool enabled)
 *
 * ## Summary
 * 设置文件写入测试开关，使下一次经过写入点的调用走已实现的失败清理路径。 仅测试宏启用的构建变体，由普通C测试在调用文件writer前设置并在之后恢复。
 *
 * ## Parameters
 * @param[in] enabled 保存的TZrBool值；非零在注入检查处视为开启。
 *
 * ## Returns
 * void；只改变本编译单元测试状态，不返回写入结果或所有权。
 *
 * ## Constraints and lifecycle
 * 仅原样赋值静态TZrBool，无分配、异常或参数规范化。设置TRUE不会跳过实际fwrite/fclose；若writer在编码或开文件阶段先失败则不会到达该注入点。状态不会自动复位，测试须串行管理生命周期。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:15; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:18; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:19; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:230; tests/library/test_ssa_exec_ir_artifact_v6.c:507; tests/library/test_ssa_exec_ir_artifact_v6.c:512
 */

/**
 * @fn static EZrArtifactStatus exec_ir_writer_fail(SZrArtifactDiagnostic *diagnostic, EZrArtifactStatus status, TZrUInt32 sectionKind)
 *
 * ## Summary
 * 统一重置本适配器的可选诊断并返回指定artifact状态。 被参数、编码、资源、I/O拒绝分支及最终成功分支调用。
 *
 * ## Parameters
 * @param[out] diagnostic 可空结构存储；被完整清零后设置两项归属字段。
 * @param[in] status 需要返回及写入诊断的状态码，OK同样允许。
 * @param[in] sectionKind 适配器定位的节种类；0表示未归属特定节。
 *
 * ## Returns
 * @return 原样返回status；可选diagnostic已按上述规则重置，不代表任何资源已清理。
 *
 * ## Constraints and lifecycle
 * diagnostic可空；非空时清零整个结构，再写status和sectionKind，旧byteOffset/rowIndex等细节不保留。status原样返回，不分配资源，不关闭文件，也不执行跳转或异常传播；下层nestedDiagnostic不会自动复制到此输出。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:23; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:26; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:27; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:28; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:29; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:31; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:120; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:250
 */

/**
 * @fn static TZrBool exec_ir_writer_metadata_is_supported(const SZrArtifactDocument *metadata, const SZrExecIrModule *module)
 *
 * ## Summary
 * 判定七节ZRO元数据是否符合此标量持久化适配器的身份合同。 只在标量codec已接受模块之后、构造嵌套包之前由文件writer调用。
 *
 * ## Parameters
 * @param[in] metadata 借用ZRO文档及有效类型化节数据；不为空，尤其合同data可解引用一行。
 * @param[in] module 已被当前scalar codec接受的借用图，functions[0]及合同有效。
 *
 * ## Returns
 * @return TRUE仅表示本适配器列出的七节形状与身份限制匹配；FALSE不改输入或诊断，不能当作通用metadata验证结果。
 *
 * ## Constraints and lifecycle
 * metadata/module及其类型化数组须有效；调用者先经scalar尺寸检查保证首函数可访问。要求ZRO/flags0/恰七节，七种requiredSections各唯一且恰MANDATORY；合同节须有一行非空数据。再核module、signature、layout、首函数与callable合同身份一致，以及无参数/无receiver、ref-export、escape、ABI-lowering扩展。只读不分配，不是任意元数据内存的安全探测，也不替代随后Core外层编码器的完整行和跨节校验。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:34; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:36; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:46; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:53; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:58; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:67; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:78; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:81; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:136; zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c:549; zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c:622
 */

/**
 * @fn EZrArtifactStatus ZrParser_ExecIr_WriteCanonicalZroFile(const SZrArtifactDocument *metadata, const SZrExecIrModule *module, const char *filename, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 先编码完整受支持标量图及身份包，再通过同目录临时文件替换目标ZRO。 用于输出当前scalar codec可表示的规范ExecIR工件；不负责运行模块或登记外部符号。
 *
 * ## Parameters
 * @param[in] metadata 有效七身份节ZRO及其数据，flags0并与模块/首函数合同匹配。
 * @param[in] module 当前scalar codec支持的有效图及数组，保持稳定至返回。
 * @param[in] filename 非空NUL结尾目标路径；宿主提供可写父目录与平台路径表示。
 * @param[out] diagnostic 可空；适配器帮助函数清零旧信息，外层编码器错误可直接保留传播。
 *
 * ## Returns
 * @return OK仅在平台替换成功后返回；参数或临时文件创建失败INVALID_ARGUMENT，适配器分配失败BUFFER_TOO_SMALL，标量/嵌套/写入/关闭/替换失败INVALID_SECTION；Core外层预检/编码错误按status传播。失败前已分配内存经done释放，不能把返回码当作临时文件清理成功证据。
 *
 * ## Constraints and lifecycle
 * 输入图/类型化元数据与NUL结尾路径借用至返回且不得并发修改。先产生scalar payload、ERI1嵌套包和ZRAF外包，全部校验成功才fopen。四个自有malloc缓冲区在普通返回路径释放；已打开的FILE经fclose后才判断写入失败。目标路径旁最多尝试32个.zr-tmp-N排他文件，已有临时文件不截断；失败remove只尝试，不保证无残留。Windows用MoveFileExA(REPLACE_EXISTING|WRITE_THROUGH)，其他平台用rename；不建目录、无内部写者锁、不承诺超出平台API的崩溃持久性。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:92; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:113; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:117; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:136; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:186; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:195; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:215; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:219; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:232; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:240; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:243; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:252; tests/library/test_ssa_exec_ir_artifact_v6.c:502; tests/library/test_ssa_exec_ir_artifact_v6.c:508; tests/library/test_ssa_exec_ir_artifact_v6.c:521
 */

/**
 * @par PARSER_EXECIR_PREPARE_BEFORE_IO
 *
 * ## Summary
 * 把图校验、身份核对和三层编码置于文件创建之前，避免验证失败替换已有工件。 文件writer首先执行的普通状态返回阶段。
 *
 * ## Constraints and lifecycle
 * 依次取得scalar尺寸并编码payload，核七身份节，再构造单EXEC_IR节ERI1与带第八mandatory bundle的外层ZRAF。借用原metadata只在栈副本中追加节；原输入不改。各分配失败和编码拒绝在打开文件前进入普通清理；内存有效性仍由调用者保证，不把格式校验视为任意指针安全检查。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:113; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:123; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:136; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:141; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:177; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:183; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:186; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:195; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:203
 */

/**
 * @par PARSER_EXECIR_EXCLUSIVE_TEMPORARY_OPEN
 *
 * ## Summary
 * 在目标路径旁以有界后缀和排他创建模式取得唯一的临时文件。 全部内存编码成功后、实际fwrite之前。
 *
 * ## Constraints and lifecycle
 * 先检查strlen+32上界并分配名称；依次尝试0..31的.zr-tmp-N，fopen("wbx")拒绝已存在候选，只有EEXIST继续。其他错误、后缀格式失败或候选耗尽统一拒绝；不删或覆盖他人的已有临时文件，不创建目录。调用者负责路径表示、父目录权限和并发目标发布的协调。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:203; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:204; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:209; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:215; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:216; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:219; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:220; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:222
 */

/**
 * @par PARSER_EXECIR_CLOSE_THEN_REPLACE
 *
 * ## Summary
 * 把完整写入与关闭作为发布前置条件，再用平台替换操作发布临时文件。 排他临时文件已打开且全部outerBytes就绪之后。
 *
 * ## Constraints and lifecycle
 * 无论fwrite是否短写都继续fclose，测试注入在两者之间只置失败标志；任一失败不进入替换。写/关/替换失败只尝试remove临时文件后走done，remove返回值未检查，不能承诺无残留。成功分支清诊断并返回OK；不回滚已成功的替换，也不排除其他写者随后替换目标。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:227; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:229; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:230; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:232; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:233; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:234; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:239; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:240; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:243; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:245; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:250
 */

/**
 * @par PARSER_EXECIR_ORDINARY_CLEANUP
 *
 * ## Summary
 * 在普通状态控制流汇合点释放本调用的四项堆内存。 分配后任一goto done或成功发布之后到达。
 *
 * ## Constraints and lifecycle
 * free依次处理临时名称、外层、嵌套与payload缓冲；NULL可安全传入，输入metadata/module/filename不释放。打开后的FILE在上游fclose，此处不再关流或重试remove。仅说明普通返回清理，不承诺进程终止、异步中断或外部非局部跳转的析构。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:109; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:110; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:232; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:251; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:252; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:253; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:254; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:255; zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:256
 */
