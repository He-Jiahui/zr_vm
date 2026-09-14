#ifndef ZR_VM_CORE_EXECBC_VERIFY_H
#define ZR_VM_CORE_EXECBC_VERIFY_H

#include "zr_vm_core/artifact_exec_ir.h"

/*
 * ExecBC is deliberately verified at the artifact boundary as a sequence of
 * fixed-width, little-endian rows.  The core does not depend on the parser's
 * projection structs (or on a particular opcode table); a parser/backend can
 * provide an opcode callback when it has a richer schema.  The first eight
 * bytes of every row are reserved for opcode, flags and an operand word.  Any
 * remaining bytes are opaque to this layer but remain covered by the artifact
 * hash and section bounds checks.
 */
#define ZR_EXEC_BC_VERIFY_MIN_ROW_SIZE ((TZrUInt32)8u)
#define ZR_EXEC_BC_VERIFY_MAX_ROW_SIZE ((TZrUInt32)256u)

typedef enum EZrExecBcVerifyStatus {
    ZR_EXEC_BC_VERIFY_OK = 0,
    ZR_EXEC_BC_VERIFY_INVALID_ARGUMENT,
    ZR_EXEC_BC_VERIFY_MISSING_SECTION,
    ZR_EXEC_BC_VERIFY_INVALID_WIDTH,
    ZR_EXEC_BC_VERIFY_INVALID_LENGTH,
    ZR_EXEC_BC_VERIFY_INVALID_SECTION,
    ZR_EXEC_BC_VERIFY_DUPLICATE_SECTION,
    ZR_EXEC_BC_VERIFY_INVALID_OPCODE,
    ZR_EXEC_BC_VERIFY_INVALID_FLAGS,
    ZR_EXEC_BC_VERIFY_MISSING_STATE_MAP,
    ZR_EXEC_BC_VERIFY_MISSING_BINDINGS,
    ZR_EXEC_BC_VERIFY_HASH_MISMATCH,
    ZR_EXEC_BC_VERIFY_LIMIT
} EZrExecBcVerifyStatus;

typedef struct SZrExecBcVerifyOptions {
    /* Zero means that the section's declared elementSize is accepted. */
    TZrUInt32 instructionWidth;
    /* Zero disables flag checking; otherwise all row flags must be in mask. */
    TZrUInt16 knownFlagsMask;
    TZrBool requireNonEmpty;
    TZrBool requireStateMaps;
    TZrBool requireBindings;
} SZrExecBcVerifyOptions;

typedef TZrBool (*FZrExecBcOpcodeValidator)(TZrUInt16 opcode,
                                             TZrUInt16 flags,
                                             TZrUInt32 instructionIndex,
                                             TZrPtr userData);

typedef struct SZrExecBcVerifyDiagnostic {
    EZrExecBcVerifyStatus status;
    TZrUInt32 byteOffset;
    TZrUInt32 instructionIndex;
    TZrUInt16 opcode;
    TZrUInt16 flags;
    TZrUInt32 expected;
    TZrUInt32 actual;
    TZrUInt64 expectedHash;
    TZrUInt64 actualHash;
} SZrExecBcVerifyDiagnostic;

ZR_CORE_API void ZrCore_ExecBcVerifyOptions_Init(
        SZrExecBcVerifyOptions *options);
ZR_CORE_API EZrExecBcVerifyStatus ZrCore_ExecBc_VerifyArtifact(
        const SZrArtifactExecIrView *artifact,
        const SZrExecBcVerifyOptions *options,
        FZrExecBcOpcodeValidator opcodeValidator,
        TZrPtr userData,
        SZrExecBcVerifyDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_ExecBc_VerifyStatusName(
        EZrExecBcVerifyStatus status);

/* Capitalisation used by a few backend adapters in the design documents. */
#define ZrCore_ExecBC_VerifyArtifact ZrCore_ExecBc_VerifyArtifact
#define ZrCore_ExecBc_Verify ZrCore_ExecBc_VerifyArtifact
#define ZrCore_ExecBC_Verify ZrCore_ExecBc_VerifyArtifact
#define ZR_EXEC_BC_VERIFY_MISSING_EXEC_BC ZR_EXEC_BC_VERIFY_MISSING_SECTION

#endif
