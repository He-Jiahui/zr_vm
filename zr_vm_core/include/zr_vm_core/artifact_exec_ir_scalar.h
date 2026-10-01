#ifndef ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_H
#define ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_H

#include "zr_vm_core/artifact_exec_ir.h"
#include "zr_vm_core/exec_ir.h"

/* EIS1 is the first canonical ExecIR graph payload inside an ERI1 EXEC_IR
 * section. It supports one no-argument i64 function with CONSTANT, RETURN.
 * The exact width is a wire width, never sizeof a runtime C record. */
#define ZR_ARTIFACT_EXEC_IR_SCALAR_MAGIC ((TZrUInt32)0x31534945u)
#define ZR_ARTIFACT_EXEC_IR_SCALAR_VERSION ((TZrUInt16)1u)
#define ZR_ARTIFACT_EXEC_IR_SCALAR_ENCODED_SIZE ((TZrUInt32)412u)
#define ZR_ARTIFACT_EXEC_IR_SCALAR_CONSTANT_OPCODE_OFFSET ((TZrUInt32)236u)

/* EIS2 adds one explicit BRANCH edge to a second CONSTANT/RETURN block.
 * ERI1 and EIS1 remain unchanged; these are independent payload versions. */
#define ZR_ARTIFACT_EXEC_IR_BRANCH_MAGIC ((TZrUInt32)0x32534945u)
#define ZR_ARTIFACT_EXEC_IR_BRANCH_VERSION ((TZrUInt16)2u)
#define ZR_ARTIFACT_EXEC_IR_BRANCH_ENCODED_SIZE ((TZrUInt32)564u)
#define ZR_ARTIFACT_EXEC_IR_BRANCH_SUCCESSOR_ID_OFFSET ((TZrUInt32)556u)

/* EIS3 adds one counted, three-block conditional CFG payload. EIS1/EIS2
 * remain byte-for-byte stable and keep their original header widths. */
#define ZR_ARTIFACT_EXEC_IR_CFG_MAGIC ((TZrUInt32)0x33534945u)
#define ZR_ARTIFACT_EXEC_IR_CFG_VERSION ((TZrUInt16)3u)
#define ZR_ARTIFACT_EXEC_IR_CFG_ENCODED_SIZE ((TZrUInt32)996u)

/* EIS4 adds one fixed scalar ADD graph. EIS1/EIS2/EIS3 retain their wire
 * bytes and continue to use their original payload widths. */
#define ZR_ARTIFACT_EXEC_IR_ADD_MAGIC ((TZrUInt32)0x34534945u)
#define ZR_ARTIFACT_EXEC_IR_ADD_VERSION ((TZrUInt16)4u)
#define ZR_ARTIFACT_EXEC_IR_ADD_ENCODED_SIZE ((TZrUInt32)716u)

/* EIS5 is a bounded counted scalar-CFG payload. Its size is determined by
 * checked record and pool counts, up to the fixed maximum below. */
#define ZR_ARTIFACT_EXEC_IR_EIS5_MAGIC ((TZrUInt32)0x35534945u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_VERSION ((TZrUInt16)5u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_HEADER_SIZE ((TZrUInt32)44u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_MAX_ENCODED_SIZE \
        ((TZrUInt32)(16u * 1024u * 1024u))

/* EIS6 is the additive one-function counted payload for Core's schema-1
 * binding rows. Legacy EIS1-EIS5 selection and bytes remain unchanged. */
#define ZR_ARTIFACT_EXEC_IR_EIS6_MAGIC ((TZrUInt32)0x36534945u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_VERSION ((TZrUInt16)6u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_HEADER_SIZE ((TZrUInt32)56u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_FIXED_PREFIX_SIZE ((TZrUInt32)224u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_BINDING_ROW_SIZE ((TZrUInt32)96u)

/* Reports the exact payload size only for a supported, verified graph.
 * Schema-zero graphs keep their EIS1-EIS5 encoding selection; one-function
 * schema-one graphs use the additive standalone EIS6 binding-row payload.
 * No output size is published on failure. */
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalar_GetEncodedSize(
        const SZrExecIrModule *module, TZrUInt32 *outSize,
        SZrArtifactExecIrDiagnostic *diagnostic);

/* Rejects every graph field or side table outside the supported EIS1-EIS6
 * standalone payload profiles. On failure destination bytes are unchanged. */
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalar_Write(
        const SZrExecIrModule *module, TZrByte *bytes, TZrUInt32 capacity,
        SZrArtifactExecIrDiagnostic *diagnostic);

/* The caller passes an initialized empty output module. This decoder builds
 * and verifies a temporary graph, then transfers ownership only on success. */
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalar_Read(
        const TZrByte *bytes, TZrUInt32 length, SZrExecIrModule *outModule,
        SZrArtifactExecIrDiagnostic *diagnostic);

#endif
