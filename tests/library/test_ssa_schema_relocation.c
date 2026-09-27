#include "zr_vm_core/artifact_exec_ir.h"
#include "zr_vm_core/execbc_verify.h"
#include "zr_vm_parser/artifact_exec_ir.h"

#include <assert.h>
#include <string.h>

/* relocation 校验回调把稳定 token、hash 与 contract 映射为本地目标 99。 */
static TZrBool resolve_ok(TZrUInt32 token, TZrUInt32 kind, TZrUInt64 hash,
                           TZrUInt64 contract, TZrUInt32 *out, TZrPtr user) {
    (void)kind; (void)user;
    if (token != 7u || hash != 11u || contract != 13u || out == ZR_NULL) {
        return ZR_FALSE;
    }
    *out = 99u;
    return ZR_TRUE;
}

/* 失败桩用于确认解析失败不会覆盖调用方现有目标数组。 */
static TZrBool resolve_fail(TZrUInt32 token, TZrUInt32 kind, TZrUInt64 hash,
                            TZrUInt64 contract, TZrUInt32 *out, TZrPtr user) {
    (void)token; (void)kind; (void)hash; (void)contract; (void)out; (void)user;
    return ZR_FALSE;
}

/* BUG: NDEBUG 会删除 writer、reader、relocation 和 ExecBC 校验调用，测试空跑成功。 */
int main(void) {
    TZrByte relocation[ZR_ARTIFACT_EXEC_IR_RELOCATION_SIZE] = {0};
    TZrByte ir[] = {1u, 2u, 3u, 4u};
    SZrArtifactExecIrSectionInput sections[2];
    SZrArtifactExecIrDocument document;
    SZrArtifactExecIrDiagnostic diagnostic;
    TZrByte bytes[512];
    SZrArtifactExecIrView view;
    TZrUInt32 resolved[1] = {123u};
    SZrExecIrReader reader;
    SZrExecIrWriter writer;

    /* BUG: 目前只用合法偏移 1；artifact_exec_ir.c 的 ValidateRelocations 用整包长度
     * 校验 codeOffset。改为 5 后虽超出 4 字节 ExecIR 段仍可解析，需补越段用例。 */
    relocation[0] = 7u;
    relocation[8] = 1u;
    relocation[16] = 11u;
    relocation[24] = 13u;
    memset(sections, 0, sizeof(sections));
    sections[0].kind = ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR;
    sections[0].elementCount = 1u;
    sections[0].elementSize = sizeof(ir);
    sections[0].data = ir;
    sections[0].byteLength = sizeof(ir);
    sections[1].kind = ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS;
    sections[1].elementCount = 1u;
    sections[1].elementSize = ZR_ARTIFACT_EXEC_IR_RELOCATION_SIZE;
    sections[1].data = relocation;
    sections[1].byteLength = sizeof(relocation);
    document.abiVersion = 16u;
    document.flags = 0u;
    document.moduleHash = 21u;
    document.execIrHash = ZrCore_ArtifactExecIr_HashBytes(ir, sizeof(ir));
    document.execBcHash = 0u;
    document.sectionCount = 2u;
    document.sections = sections;

    writer.bytes = bytes;
    writer.capacity = sizeof(bytes);
    writer.written = 0u;
    assert(ZrParser_ExecIr_Write(&document, &writer, &diagnostic));
    assert(writer.written > ZR_ARTIFACT_EXEC_IR_HEADER_SIZE);
    reader.bytes = bytes;
    reader.length = writer.written;
    assert(ZrParser_ExecIr_Read(&reader, &view, &diagnostic));
    assert(view.abiVersion == 16u && view.sectionCount == 2u);
    assert(ZrCore_ArtifactExecIr_ValidateRelocations(
                   &view, resolve_ok, resolved, 1u, ZR_NULL, &diagnostic) ==
           ZR_ARTIFACT_EXEC_IR_OK);
    assert(resolved[0] == 99u);

    /* BUG: 失败时 artifact_exec_ir.c 先记录 token 又由 fail 清零；此处只核查状态和
     * 输出未变，漏掉诊断 token。补测应断言该字段保留 7。 */
    resolved[0] = 123u;
    assert(ZrCore_ArtifactExecIr_ValidateRelocations(
                   &view, resolve_fail, resolved, 1u, ZR_NULL, &diagnostic) ==
           ZR_ARTIFACT_EXEC_IR_RESOLUTION_FAILED);
    assert(resolved[0] == 123u);

    /* 损坏总长度应在 reader 发布 view 前被拒绝。 */
    {
        TZrByte corrupt[512];
        memcpy(corrupt, bytes, writer.written);
        corrupt[44] = (TZrByte)(writer.written - 1u);
        assert(ZrCore_ArtifactExecIr_Read(corrupt, writer.written, &view,
                                          &diagnostic) !=
               ZR_ARTIFACT_EXEC_IR_OK);
    }
    /* TODO: 重叠目录被拒绝时 view 已部分填充；公开 Read 接口未约定失败时清空。
     * 需先核查失败调用者是否消费 view，再决定是否要求清空并补相应断言。 */
    {
        TZrByte corrupt[512];
        memcpy(corrupt, bytes, writer.written);
        /* Relocation directory entry offset overlaps the first section. */
        corrupt[ZR_ARTIFACT_EXEC_IR_HEADER_SIZE + ZR_ARTIFACT_EXEC_IR_SECTION_SIZE + 16u] =
                corrupt[ZR_ARTIFACT_EXEC_IR_HEADER_SIZE + 16u];
        assert(ZrCore_ArtifactExecIr_Read(corrupt, writer.written, &view,
                                          &diagnostic) !=
               ZR_ARTIFACT_EXEC_IR_OK);
    }

    /* 手工构造 ExecBC section，将有效 opcode 与非法哨兵交给核心验证器。 */
    {
        TZrByte execBc[8] = {1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
        SZrArtifactExecIrView execBcView;
        SZrExecBcVerifyOptions options;
        SZrExecBcVerifyDiagnostic verifyDiagnostic;

        memset(&execBcView, 0, sizeof(execBcView));
        execBcView.buffer = execBc;
        execBcView.bufferLength = sizeof(execBc);
        execBcView.sectionCount = 1u;
        execBcView.execBcHash = ZrCore_ArtifactExecIr_HashBytes(
                execBc, sizeof(execBc));
        execBcView.sections[0].kind = ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_BC;
        execBcView.sections[0].elementCount = 1u;
        execBcView.sections[0].elementSize = sizeof(execBc);
        execBcView.sections[0].byteOffset = 0u;
        execBcView.sections[0].byteLength = sizeof(execBc);
        execBcView.sections[0].data = execBc;
        ZrCore_ExecBcVerifyOptions_Init(&options);
        options.instructionWidth = sizeof(execBc);
        assert(ZrCore_ExecBc_VerifyArtifact(
                       &execBcView, &options, ZR_NULL, ZR_NULL,
                       &verifyDiagnostic) == ZR_EXEC_BC_VERIFY_OK);
        execBc[0] = 0xffu;
        execBc[1] = 0xffu;
        execBcView.execBcHash = ZrCore_ArtifactExecIr_HashBytes(
                execBc, sizeof(execBc));
        assert(ZrCore_ExecBc_VerifyArtifact(
                       &execBcView, &options, ZR_NULL, ZR_NULL,
                       &verifyDiagnostic) == ZR_EXEC_BC_VERIFY_INVALID_OPCODE);
    }
    return 0;
}
