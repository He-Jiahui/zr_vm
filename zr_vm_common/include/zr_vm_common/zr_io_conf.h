//
// Created by HeJiahui on 2025/7/6.
//

#ifndef ZR_IO_CONF_H
#define ZR_IO_CONF_H

#define ZR_IO_EOF EOF
#include "zr_vm_common/zr_common_conf.h"

static const union {
 TUInt64 dummy;
 TBool littleEndian;
} CZrIoEndian = {.dummy = 1};

#define ZR_IO_IS_LITTLE_ENDIAN (CZrIoEndian.littleEndian)

/* .SOURCE:
 *  0 SIGNATURE 4
 *  4 VERSION_MAJOR 4
 *  8 VERSION_MINOR 4
 *  12 VERSION_PATCH 4
 *  16 FORMAT 8
 *  24 NATIVE_INT_SIZE 1
 *  25 SIZE_T_SIZE 1
 *  26 INSTRUCTION_SIZE 1
 *  27 ENDIAN 1
 *  28 DEBUG 1
 *  29 OPT 3
 *  32 .MODULE
 */
#define ZR_IO_SOURCE_SIGNATURE "\x1ZR\x2"

/* .MODULE:
 * NAME [string]
 * MD5 [string]
 * IMPORTS_LENGTH [8]
 * IMPORTS [.IMPORT]
 * DECLARES_LENGTH [8]
 * DECLARES [.DECLARE]
 * ENTRY [.FUNCTION]
 */


/* .IMPORT:
 * NAME [string]
 * MD5 [string]
 *
 */

/* .DECLARE:
 * TYPE [1]
 * BODY [.CLASS|.STRUCT|.INTERFACE|.FUNCTION]
 */

/* .CLASS:
 * NAME [string]
 * SUPER_CLASS_LENGTH[8]
 * SUPER_CLASSES [.REFERENCE]
 * GENERIC_LENGTH [8]
 * DECLARES_LENGTH [8]
 * DECLARES [.CLASS_DECLARE]
 */

/* .META:
 * todo
 */

/* .STRUCT:
 * todo
 *
 */

/* .INTERFACE:
 * todo
 *
 */

/* .FUNCTION:
 * NAME [string]
 * START_LINE [8]
 * END_LINE [8]
 * PARAMETERS_LENGTH [8]
 * HAS_VAR_ARGS [8]
 * INSTRUCTIONS_LENGTH [8]
 * INSTRUCTIONS [.INSTRUCTION]
 * LOCAL_LENGTH [8]
 * LOCALS [.LOCAL]
 * CONSTANTS_LENGTH [8]
 * CONSTANTS [.CONSTANT]
 * CLOSURES_LENGTH [8]
 * CLOSURES [.CLOSURE]
 * DEBUG_INFO_LENGTH [8]
 * DEBUG_INFO [.DEBUG_INFO] (DEBUG)
 */

/* .INSTRUCTION:
 * RAW [8]
 */

/* .LOCAL:
 * // TYPE [4]
 * // DATA [string|uint|int|float|...]
 * INSTRUCTION_START [8]
 * INSTRUCTION_END [8]
 * START_LINE [8] (DEBUG)
 * END_LINE [8] (DEBUG)
 */

/* .CONSTANT:
 * TYPE [4]
 * DATA [string|uint|int|float|...]
 * START_LINE [8] (DEBUG)
 * END_LINE [8] (DEBUG)
 */

/* .CLOSURE:
 * SUB_FUNCTION [.FUNCTION]
 * todo
 */

/* .DEBUG_INFO:
 * todo
 */


#endif //ZR_IO_CONF_H
