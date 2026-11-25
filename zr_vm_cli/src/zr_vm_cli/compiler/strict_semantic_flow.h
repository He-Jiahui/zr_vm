#ifndef ZR_CLI_STRICT_SEMANTIC_FLOW_H
#define ZR_CLI_STRICT_SEMANTIC_FLOW_H

#include "zr_vm_parser/location.h"
#include "zr_vm_strict/program.h"

typedef struct SZrCliStrictLocation {
    SZrFileRange range;
    uint32_t module;
} SZrCliStrictLocation;

/* Compiler-only ownership checking. The strict artifact verifier does not
 * depend on Semantic IR and must still verify the serialized instruction set. */
TZrBool ZrCli_StrictSemanticFlow_Check(SZrState *state, const ZrStrictProgram *program,
        const SZrCliStrictLocation *locations, TZrBool receiverIsLive,
        uint32_t *badPc, const char **message);

#endif
