#include "zr_vm_core/artifact_exec_ir.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/execbc_verify.h"
#include "zr_vm_parser/artifact_exec_ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Keep the fixture active in Release builds where NDEBUG removes assert(). */
#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static int failures = 0;
#define EXPECT(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (0)

/* relocation 校验回调把稳定 token、hash 与 contract 映射为本地目标 99。 */
static TZrBool resolve_ok(TZrUInt32 token, TZrUInt32 kind, TZrUInt64 hash,
                           TZrUInt64 contract, TZrUInt32 *out, TZrPtr user) {
    (void)kind;
    if (user != ZR_NULL) ++*(TZrUInt32 *)user;
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
    TZrUInt32 irOffset;
    TZrUInt32 relocationOffset;

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
    /* Raw ERI1 codec test: the envelope accepts a supplied ABI value;
     * executable ABI enforcement belongs to OpenExecIrArtifact. */
    document.abiVersion = ZR_VM_AOT_ABI_VERSION;
    document.flags = 0u;
    document.moduleHash = 21u;
    document.execIrHash = ZrCore_ArtifactExecIr_HashBytes(ir, sizeof(ir));
    document.execBcHash = 0u;
    document.sectionCount = 2u;
    document.sections = sections;

    writer.bytes = bytes;
    writer.capacity = sizeof(bytes);
    writer.written = 0u;
    CHECK(ZrParser_ExecIr_Write(&document, &writer, &diagnostic));
    CHECK(writer.written > ZR_ARTIFACT_EXEC_IR_HEADER_SIZE);
    reader.bytes = bytes;
    reader.length = writer.written;
    CHECK(ZrParser_ExecIr_Read(&reader, &view, &diagnostic));
    CHECK(view.abiVersion == ZR_VM_AOT_ABI_VERSION && view.sectionCount == 2u);
    irOffset = view.sections[0].byteOffset;
    relocationOffset = view.sections[1].byteOffset;
    CHECK(ZrCore_ArtifactExecIr_ValidateRelocations(
                   &view, resolve_ok, resolved, 1u, ZR_NULL, &diagnostic) ==
           ZR_ARTIFACT_EXEC_IR_OK);
    CHECK(resolved[0] == 99u);

    /* codeOffset is relative to EXEC_IR: offset 5 is outside its 4 bytes. */
    {
        TZrByte corrupt[512];
        SZrArtifactExecIrView corruptView;
        TZrUInt32 resolverCalls = 0u;
        memcpy(corrupt, bytes, writer.written);
        corrupt[relocationOffset + 8u] = 5u;
        CHECK(ZrCore_ArtifactExecIr_Read(corrupt, writer.written,
                                         &corruptView, &diagnostic) ==
              ZR_ARTIFACT_EXEC_IR_OK);
        resolved[0] = 123u;
        EXPECT(ZrCore_ArtifactExecIr_ValidateRelocations(
                       &corruptView, resolve_ok, resolved, 1u,
                       &resolverCalls, &diagnostic) ==
               ZR_ARTIFACT_EXEC_IR_INVALID_RELOCATION);
        EXPECT(resolverCalls == 0u);
        EXPECT(resolved[0] == 123u);
        EXPECT(diagnostic.sectionKind == ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS);
        EXPECT(diagnostic.rowIndex == 0u && diagnostic.token == 7u);
    }

    /* Validate every row before invoking a resolver with process-local state. */
    {
        TZrByte relocationPair[2u * ZR_ARTIFACT_EXEC_IR_RELOCATION_SIZE];
        SZrArtifactExecIrSectionInput pairSections[2];
        SZrArtifactExecIrDocument pairDocument = document;
        SZrExecIrWriter pairWriter;
        SZrArtifactExecIrView pairView;
        TZrByte pairBytes[512];
        TZrUInt32 pairResolved[2] = {123u, 456u};
        TZrUInt32 resolverCalls = 0u;
        memcpy(relocationPair, relocation, sizeof(relocation));
        memcpy(relocationPair + sizeof(relocation), relocation,
               sizeof(relocation));
        relocationPair[sizeof(relocation) + 8u] = 5u;
        memcpy(pairSections, sections, sizeof(sections));
        pairSections[1].elementCount = 2u;
        pairSections[1].data = relocationPair;
        pairSections[1].byteLength = sizeof(relocationPair);
        pairDocument.sections = pairSections;
        pairWriter.bytes = pairBytes;
        pairWriter.capacity = sizeof(pairBytes);
        pairWriter.written = 0u;
        CHECK(ZrParser_ExecIr_Write(&pairDocument, &pairWriter, &diagnostic));
        CHECK(ZrCore_ArtifactExecIr_Read(pairBytes, pairWriter.written,
                                         &pairView, &diagnostic) ==
              ZR_ARTIFACT_EXEC_IR_OK);
        EXPECT(ZrCore_ArtifactExecIr_ValidateRelocations(
                       &pairView, resolve_ok, pairResolved, 2u,
                       &resolverCalls, &diagnostic) ==
               ZR_ARTIFACT_EXEC_IR_INVALID_RELOCATION);
        EXPECT(resolverCalls == 0u);
        EXPECT(pairResolved[0] == 123u && pairResolved[1] == 456u);
        EXPECT(diagnostic.rowIndex == 1u && diagnostic.token == 7u);
    }

    resolved[0] = 123u;
    CHECK(ZrCore_ArtifactExecIr_ValidateRelocations(
                   &view, resolve_fail, resolved, 1u, ZR_NULL, &diagnostic) ==
           ZR_ARTIFACT_EXEC_IR_RESOLUTION_FAILED);
    CHECK(resolved[0] == 123u);
    EXPECT(diagnostic.token == 7u);

    /* 损坏总长度应在 reader 发布 view 前被拒绝。 */
    {
        TZrByte corrupt[512];
        memcpy(corrupt, bytes, writer.written);
        corrupt[44] = (TZrByte)(writer.written - 1u);
        CHECK(ZrCore_ArtifactExecIr_Read(corrupt, writer.written, &view,
                                         &diagnostic) !=
              ZR_ARTIFACT_EXEC_IR_OK);
        EXPECT(view.buffer == ZR_NULL && view.sectionCount == 0u);
    }
    /* A late directory failure must not expose a partially decoded view. */
    {
        TZrByte corrupt[512];
        memcpy(corrupt, bytes, writer.written);
        /* Relocation directory entry offset overlaps the first section. */
        corrupt[ZR_ARTIFACT_EXEC_IR_HEADER_SIZE + ZR_ARTIFACT_EXEC_IR_SECTION_SIZE + 16u] =
                corrupt[ZR_ARTIFACT_EXEC_IR_HEADER_SIZE + 16u];
        CHECK(ZrCore_ArtifactExecIr_Read(corrupt, writer.written, &view,
                                         &diagnostic) ==
              ZR_ARTIFACT_EXEC_IR_OVERLAP);
        EXPECT(view.buffer == ZR_NULL && view.sectionCount == 0u);
    }

    /* Writer rejects count>0 with size=0; reader must reject the same row. */
    {
        TZrByte corrupt[512];
        TZrUInt32 sizeOffset = ZR_ARTIFACT_EXEC_IR_HEADER_SIZE +
                ZR_ARTIFACT_EXEC_IR_SECTION_SIZE + 12u;
        memcpy(corrupt, bytes, writer.written);
        memset(corrupt + sizeOffset, 0, 4u);
        memset(&view, 0xa5, sizeof(view));
        EXPECT(ZrCore_ArtifactExecIr_Read(corrupt, writer.written, &view,
                                          &diagnostic) ==
               ZR_ARTIFACT_EXEC_IR_INVALID_SECTION);
        EXPECT(view.buffer == ZR_NULL && view.sectionCount == 0u);
    }

    /* The hash check happens after the directory and must still publish none. */
    {
        TZrByte corrupt[512];
        memcpy(corrupt, bytes, writer.written);
        corrupt[irOffset] ^= 1u;
        memset(&view, 0xa5, sizeof(view));
        CHECK(ZrCore_ArtifactExecIr_Read(corrupt, writer.written, &view,
                                         &diagnostic) ==
              ZR_ARTIFACT_EXEC_IR_HASH_MISMATCH);
        EXPECT(view.buffer == ZR_NULL && view.sectionCount == 0u);
    }

    /* Invalid input also clears a caller's previously populated view. */
    memset(&view, 0xa5, sizeof(view));
    EXPECT(ZrCore_ArtifactExecIr_Read(ZR_NULL, writer.written, &view,
                                      &diagnostic) ==
           ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT);
    EXPECT(view.buffer == ZR_NULL && view.sectionCount == 0u);

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
        CHECK(ZrCore_ExecBc_VerifyArtifact(
                       &execBcView, &options, ZR_NULL, ZR_NULL,
                       &verifyDiagnostic) == ZR_EXEC_BC_VERIFY_OK);
        execBc[0] = 0xffu;
        execBc[1] = 0xffu;
        execBcView.execBcHash = ZrCore_ArtifactExecIr_HashBytes(
                execBc, sizeof(execBc));
        CHECK(ZrCore_ExecBc_VerifyArtifact(
                       &execBcView, &options, ZR_NULL, ZR_NULL,
                       &verifyDiagnostic) == ZR_EXEC_BC_VERIFY_INVALID_OPCODE);
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
