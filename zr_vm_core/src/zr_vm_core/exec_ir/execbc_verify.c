#include "zr_vm_core/execbc_verify.h"

#include <string.h>

static EZrExecBcVerifyStatus execbc_fail(SZrExecBcVerifyDiagnostic *diagnostic,
                                          EZrExecBcVerifyStatus status,
                                          TZrUInt32 offset,
                                          TZrUInt32 instructionIndex,
                                          TZrUInt16 opcode,
                                          TZrUInt16 flags,
                                          TZrUInt32 expected,
                                          TZrUInt32 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->byteOffset = offset;
        diagnostic->instructionIndex = instructionIndex;
        diagnostic->opcode = opcode;
        diagnostic->flags = flags;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
        diagnostic->expectedHash = 0u;
        diagnostic->actualHash = 0u;
    }
    return status;
}

static TZrUInt16 execbc_get16(const TZrByte *bytes) {
    return (TZrUInt16)((TZrUInt16)bytes[0] |
                       ((TZrUInt16)bytes[1] << 8u));
}

static TZrUInt32 execbc_get32(const TZrByte *bytes) {
    return (TZrUInt32)bytes[0] |
           ((TZrUInt32)bytes[1] << 8u) |
           ((TZrUInt32)bytes[2] << 16u) |
           ((TZrUInt32)bytes[3] << 24u);
}

static const SZrArtifactExecIrSectionView *execbc_find_section(
        const SZrArtifactExecIrView *artifact,
        EZrArtifactExecIrSectionKind kind) {
    TZrUInt32 index;

    for (index = 0u; index < artifact->sectionCount; ++index) {
        if (artifact->sections[index].kind == kind) {
            return &artifact->sections[index];
        }
    }
    return ZR_NULL;
}

void ZrCore_ExecBcVerifyOptions_Init(SZrExecBcVerifyOptions *options) {
    if (options != ZR_NULL) {
        memset(options, 0, sizeof(*options));
    }
}

EZrExecBcVerifyStatus ZrCore_ExecBc_VerifyArtifact(
        const SZrArtifactExecIrView *artifact,
        const SZrExecBcVerifyOptions *options,
        FZrExecBcOpcodeValidator opcodeValidator,
        TZrPtr userData,
        SZrExecBcVerifyDiagnostic *diagnostic) {
    SZrExecBcVerifyOptions defaults;
    const SZrArtifactExecIrSectionView *execBc;
    const SZrArtifactExecIrSectionView *stateMaps;
    const SZrArtifactExecIrSectionView *bindings;
    TZrUInt32 index;
    TZrUInt32 sectionIndex;

    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    ZrCore_ExecBcVerifyOptions_Init(&defaults);
    if (options == ZR_NULL) {
        options = &defaults;
    }
    if (artifact == ZR_NULL || artifact->buffer == ZR_NULL ||
        artifact->bufferLength == 0u ||
        artifact->bufferLength > ZR_ARTIFACT_EXEC_IR_MAX_BYTES ||
        artifact->sectionCount > ZR_ARTIFACT_EXEC_IR_MAX_SECTIONS) {
        return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_INVALID_ARGUMENT,
                           0u, 0u, 0u, 0u, 0u, 0u);
    }

    /* Keep this entry point safe for callers that assembled a view directly
     * instead of obtaining it from ArtifactExecIr_Read. */
    for (sectionIndex = 0u; sectionIndex < artifact->sectionCount;
         ++sectionIndex) {
        const SZrArtifactExecIrSectionView *section =
                &artifact->sections[sectionIndex];
        TZrUInt32 previous;

        if (section->kind < ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR ||
            section->kind > ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS ||
            section->byteOffset > artifact->bufferLength ||
            section->byteLength > artifact->bufferLength -
                                  section->byteOffset ||
            (section->byteLength != 0u && section->data == ZR_NULL) ||
            (section->elementCount != 0u && section->elementSize == 0u) ||
            ((TZrUInt64)section->elementCount * section->elementSize !=
             section->byteLength)) {
            return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_INVALID_SECTION,
                               section->byteOffset, 0u, 0u, 0u, 0u,
                               section->byteLength);
        }
        for (previous = 0u; previous < sectionIndex; ++previous) {
            if (artifact->sections[previous].kind == section->kind) {
                return execbc_fail(diagnostic,
                                   ZR_EXEC_BC_VERIFY_DUPLICATE_SECTION,
                                   section->byteOffset, 0u, 0u, 0u,
                                   section->kind, section->kind);
            }
        }
    }

    execBc = execbc_find_section(artifact,
                                 ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_BC);
    if (execBc == ZR_NULL) {
        return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_MISSING_SECTION,
                           0u, 0u, 0u, 0u,
                           ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_BC, 0u);
    }
    if (execBc->data == ZR_NULL && execBc->byteLength != 0u) {
        return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_INVALID_ARGUMENT,
                           execBc->byteOffset, 0u, 0u, 0u, 0u, 0u);
    }
    if (execBc->byteOffset > artifact->bufferLength ||
        execBc->byteLength > artifact->bufferLength - execBc->byteOffset) {
        return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_INVALID_LENGTH,
                           execBc->byteOffset, 0u, 0u, 0u,
                           artifact->bufferLength -
                                   (execBc->byteOffset <= artifact->bufferLength
                                            ? execBc->byteOffset
                                            : artifact->bufferLength),
                           execBc->byteLength);
    }
    if (execBc->byteLength != 0u &&
        execBc->data != artifact->buffer + execBc->byteOffset) {
        return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_INVALID_ARGUMENT,
                           execBc->byteOffset, 0u, 0u, 0u, 0u, 0u);
    }
    if (execBc->elementSize < ZR_EXEC_BC_VERIFY_MIN_ROW_SIZE ||
        execBc->elementSize > ZR_EXEC_BC_VERIFY_MAX_ROW_SIZE ||
        (options->instructionWidth != 0u &&
         options->instructionWidth != execBc->elementSize)) {
        return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_INVALID_WIDTH,
                           execBc->byteOffset, 0u, 0u, 0u,
                           options->instructionWidth,
                           execBc->elementSize);
    }
    if ((TZrUInt64)execBc->elementCount * execBc->elementSize !=
        execBc->byteLength) {
        TZrUInt64 expectedLength = (TZrUInt64)execBc->elementCount *
                                   execBc->elementSize;
        return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_INVALID_LENGTH,
                           execBc->byteOffset, 0u, 0u, 0u,
                           expectedLength > UINT32_MAX
                                   ? UINT32_MAX
                                   : (TZrUInt32)expectedLength,
                           execBc->byteLength);
    }
    if (options->requireNonEmpty && execBc->elementCount == 0u) {
        return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_LIMIT,
                           execBc->byteOffset, 0u, 0u, 0u, 1u, 0u);
    }

    stateMaps = execbc_find_section(artifact,
                                    ZR_ARTIFACT_EXEC_IR_SECTION_STATE_MAPS);
    if (options->requireStateMaps &&
        (stateMaps == ZR_NULL || stateMaps->byteLength == 0u)) {
        return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_MISSING_STATE_MAP,
                           execBc->byteOffset, 0u, 0u, 0u, 1u, 0u);
    }
    bindings = execbc_find_section(artifact,
                                  ZR_ARTIFACT_EXEC_IR_SECTION_BINDINGS);
    if (options->requireBindings &&
        (bindings == ZR_NULL || bindings->byteLength == 0u)) {
        return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_MISSING_BINDINGS,
                           execBc->byteOffset, 0u, 0u, 0u, 1u, 0u);
    }

    /* A manually assembled view may not have gone through ArtifactExecIr_Read,
     * so repeat the hash check here before any row is inspected. */
    if (artifact->execBcHash != 0u &&
        ZrCore_ArtifactExecIr_HashBytes(execBc->data, execBc->byteLength) !=
                artifact->execBcHash) {
        TZrUInt64 actualHash = ZrCore_ArtifactExecIr_HashBytes(
                execBc->data, execBc->byteLength);
        EZrExecBcVerifyStatus status = execbc_fail(
                diagnostic, ZR_EXEC_BC_VERIFY_HASH_MISMATCH,
                execBc->byteOffset, 0u, 0u, 0u,
                (TZrUInt32)artifact->execBcHash,
                (TZrUInt32)actualHash);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = artifact->execBcHash;
            diagnostic->actualHash = actualHash;
        }
        return status;
    }

    for (index = 0u; index < execBc->elementCount; ++index) {
        const TZrByte *row = execBc->data +
                             (TZrSize)index * execBc->elementSize;
        TZrUInt16 opcode = execbc_get16(row);
        TZrUInt16 flags = execbc_get16(row + 2u);

        /* UINT16_MAX is reserved as the invalid/uninitialised opcode marker;
         * extended/fused opcode ranges are intentionally left to the callback. */
        if (opcode == UINT16_MAX) {
            return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_INVALID_OPCODE,
                               execBc->byteOffset +
                                       index * execBc->elementSize,
                               index, opcode, flags, 0u, opcode);
        }
        if (options->knownFlagsMask != 0u &&
            (flags & (TZrUInt16)~options->knownFlagsMask) != 0u) {
            return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_INVALID_FLAGS,
                               execBc->byteOffset +
                                       index * execBc->elementSize,
                               index, opcode, flags,
                               options->knownFlagsMask, flags);
        }
        if (opcodeValidator != ZR_NULL &&
            !opcodeValidator(opcode, flags, index, userData)) {
            return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_INVALID_OPCODE,
                               execBc->byteOffset +
                                       index * execBc->elementSize,
                               index, opcode, flags, 1u, 0u);
        }
        /* Keep the fixed-width row's operand word readable in sanitised builds;
         * no host pointer is ever formed from it. */
        (void)execbc_get32(row + 4u);
    }
    return execbc_fail(diagnostic, ZR_EXEC_BC_VERIFY_OK, 0u, 0u, 0u, 0u,
                       0u, 0u);
}

const TZrChar *ZrCore_ExecBc_VerifyStatusName(EZrExecBcVerifyStatus status) {
    switch (status) {
        case ZR_EXEC_BC_VERIFY_OK: return "ok";
        case ZR_EXEC_BC_VERIFY_MISSING_SECTION: return "missing-section";
        case ZR_EXEC_BC_VERIFY_INVALID_WIDTH: return "invalid-width";
        case ZR_EXEC_BC_VERIFY_INVALID_LENGTH: return "invalid-length";
        case ZR_EXEC_BC_VERIFY_INVALID_SECTION: return "invalid-section";
        case ZR_EXEC_BC_VERIFY_DUPLICATE_SECTION: return "duplicate-section";
        case ZR_EXEC_BC_VERIFY_INVALID_OPCODE: return "invalid-opcode";
        case ZR_EXEC_BC_VERIFY_INVALID_FLAGS: return "invalid-flags";
        case ZR_EXEC_BC_VERIFY_MISSING_STATE_MAP: return "missing-state-map";
        case ZR_EXEC_BC_VERIFY_MISSING_BINDINGS: return "missing-bindings";
        case ZR_EXEC_BC_VERIFY_HASH_MISMATCH: return "hash-mismatch";
        case ZR_EXEC_BC_VERIFY_LIMIT: return "limit";
        default: return "invalid-argument";
    }
}
