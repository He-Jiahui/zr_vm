#include "ssa_retained_builder_cfg_fixture.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/semantic_ir.h"

#include <stdlib.h>
#include <string.h>

/* One heap allocation owns every producer view, including nested edge arrays.
 * None of this storage is copied into a fabricated ExecIR graph by the test. */
typedef struct SSemanticProducer {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock blocks[4];
    SZrParserCfgEdge edges[4];
    SZrSemanticIrInstruction instructions[12];
    SZrSemanticIrValue values[6];
    TZrValueId operands[4];
    SZrParserPlace place;
    SZrSemanticIrLocal local;
} SSemanticProducer;

static SZrArray input_array(void *storage, TZrSize count, TZrSize width) {
    SZrArray array = {0};
    array.head = (TZrBytePtr)storage;
    array.length = count;
    array.capacity = count;
    array.elementSize = width;
    array.isValid = ZR_TRUE;
    return array;
}

static void initialize_producer(SSemanticProducer *p, TZrBool loop,
                                TZrBool parameter) {
    TZrUInt32 index;
    TZrUInt32 instructionCount = loop ? 12u : 10u;
    TZrUInt32 valueCount = loop ? 6u : 4u;
    p->semantic.symbolId = loop ? 43u : 42u;
    p->semantic.callableTypeId = 12u;
    p->semantic.cfg.entryBlockId = 0u;
    p->semantic.cfg.exitBlockId = 3u;
    p->semantic.cfg.blocks = input_array(p->blocks, 4u, sizeof(p->blocks[0]));
    p->semantic.instructions = input_array(p->instructions, instructionCount,
                                            sizeof(p->instructions[0]));
    p->semantic.values = input_array(p->values, valueCount, sizeof(p->values[0]));
    p->semantic.valueOperands = input_array(p->operands, loop ? 4u : 2u,
                                             sizeof(p->operands[0]));
    p->semantic.places.places = input_array(&p->place, 1u, sizeof(p->place));
    p->semantic.locals = input_array(&p->local, 1u, sizeof(p->local));
    p->place.id = 1u;
    p->place.typeId = ZR_RETAINED_CFG_I64_TYPE;
    p->place.base.kind = parameter ? ZR_PARSER_PLACE_BASE_PARAMETER : ZR_PARSER_PLACE_BASE_LOCAL;
    p->place.base.identity = loop ? 18u : 17u;
    p->local.symbolId = p->place.base.identity;
    p->local.placeId = 1u;
    p->local.typeId = ZR_RETAINED_CFG_I64_TYPE;
    p->local.isScalar = ZR_TRUE;
    p->local.isParameter = parameter;
    for (index = 0u; index < 4u; ++index) {
        p->blocks[index].id = index;
        p->blocks[index].kind = index == 0u ? ZR_PARSER_CFG_BLOCK_ENTRY : ZR_PARSER_CFG_BLOCK_STATEMENT;
    }
    for (index = 0u; index < instructionCount; ++index) {
        SZrSemanticIrInstruction *instruction = &p->instructions[index];
        instruction->id = index + 1u;
        instruction->typeId = ZR_RETAINED_CFG_I64_TYPE;
        instruction->targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        instruction->sourceRange.start.offset = index * 4u;
        instruction->sourceRange.end.offset = index * 4u + 3u;
        instruction->sourceRange.start.line = index + 1u;
        instruction->sourceRange.end.line = index + 1u;
        instruction->sourceRange.start.column = 1u;
        instruction->sourceRange.end.column = 4u;
    }
    for (index = 0u; index < valueCount; ++index) {
        p->values[index].id = index + 1u;
        p->values[index].typeId = ZR_RETAINED_CFG_I64_TYPE;
        p->values[index].facts.typeId = ZR_RETAINED_CFG_I64_TYPE;
        p->values[index].facts.ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_UNKNOWN;
        p->values[index].facts.nullability = ZR_SEMANTIC_VALUE_NULLABILITY_UNKNOWN;
    }
    p->values[loop ? 5u : 0u].typeId = ZR_RETAINED_CFG_BOOL_TYPE;
    p->values[loop ? 5u : 0u].facts.typeId = ZR_RETAINED_CFG_BOOL_TYPE;
}

static void edge(SSemanticProducer *p, TZrUInt32 index, TZrUInt32 from,
                 TZrUInt32 to, EZrParserCfgEdgeKind kind) {
    p->edges[index].fromBlockId = from;
    p->edges[index].toBlockId = to;
    p->edges[index].kind = kind;
}

static void block(SSemanticProducer *p, TZrUInt32 index, TZrUInt32 first,
                  TZrUInt32 count, TZrUInt32 edgeStart, TZrUInt32 edgeCount) {
    p->blocks[index].firstInstructionIndex = first;
    p->blocks[index].instructionCount = count;
    p->blocks[index].terminatorKind = index == 3u ? ZR_PARSER_CFG_TERMINATOR_RETURN
                                                : ZR_PARSER_CFG_TERMINATOR_BRANCH;
    if (edgeCount != 0u)
        p->blocks[index].outgoingEdges = input_array(&p->edges[edgeStart], edgeCount,
                                                     sizeof(p->edges[0]));
}

static void make_diamond(SSemanticProducer *p) {
    edge(p, 0u, 0u, 1u, ZR_PARSER_CFG_EDGE_TRUE_BRANCH);
    edge(p, 1u, 0u, 2u, ZR_PARSER_CFG_EDGE_FALSE_BRANCH);
    edge(p, 2u, 1u, 3u, ZR_PARSER_CFG_EDGE_NORMAL);
    edge(p, 3u, 2u, 3u, ZR_PARSER_CFG_EDGE_NORMAL);
    block(p, 0u, 0u, 2u, 0u, 2u);
    block(p, 1u, 2u, 3u, 2u, 1u);
    block(p, 2u, 5u, 3u, 3u, 1u);
    block(p, 3u, 8u, 2u, 0u, 0u);
    p->instructions[0].opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    p->instructions[0].placeId = 1u;
    p->instructions[1].opcode = ZR_SEMANTIC_IR_BRANCH;
    p->instructions[1].typeId = ZR_RETAINED_CFG_BOOL_TYPE;
    p->instructions[1].operandCount = 1u;
    p->instructions[2].opcode = ZR_SEMANTIC_IR_CONSTANT;
    p->instructions[2].resultValueId = 2u;
    p->instructions[3].opcode = ZR_SEMANTIC_IR_STORE;
    p->instructions[3].placeId = 1u;
    p->instructions[3].valueId = 2u;
    p->instructions[4].opcode = ZR_SEMANTIC_IR_BRANCH;
    p->instructions[5].opcode = ZR_SEMANTIC_IR_CONSTANT;
    p->instructions[5].resultValueId = 3u;
    p->instructions[6].opcode = ZR_SEMANTIC_IR_STORE;
    p->instructions[6].placeId = 1u;
    p->instructions[6].valueId = 3u;
    p->instructions[7].opcode = ZR_SEMANTIC_IR_BRANCH;
    p->instructions[8].opcode = ZR_SEMANTIC_IR_LOAD;
    p->instructions[8].placeId = 1u;
    p->instructions[8].resultValueId = 4u;
    p->instructions[9].opcode = ZR_SEMANTIC_IR_RETURN;
    p->instructions[9].operandStart = 1u;
    p->instructions[9].operandCount = 1u;
    p->values[1].definitionInstructionId = 3u;
    p->values[2].definitionInstructionId = 6u;
    p->values[3].definitionInstructionId = 9u;
    p->operands[0] = 1u;
    p->operands[1] = 4u;
}

static void make_loop(SSemanticProducer *p) {
    edge(p, 0u, 0u, 1u, ZR_PARSER_CFG_EDGE_NORMAL);
    edge(p, 1u, 1u, 2u, ZR_PARSER_CFG_EDGE_TRUE_BRANCH);
    edge(p, 2u, 1u, 3u, ZR_PARSER_CFG_EDGE_FALSE_BRANCH);
    edge(p, 3u, 2u, 1u, ZR_PARSER_CFG_EDGE_NORMAL);
    block(p, 0u, 0u, 4u, 0u, 1u);
    block(p, 1u, 4u, 2u, 1u, 2u);
    block(p, 2u, 6u, 4u, 3u, 1u);
    block(p, 3u, 10u, 2u, 0u, 0u);
    p->instructions[0].opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    p->instructions[0].placeId = 1u;
    p->instructions[1].opcode = ZR_SEMANTIC_IR_CONSTANT;
    p->instructions[1].resultValueId = 1u;
    p->instructions[2].opcode = ZR_SEMANTIC_IR_STORE;
    p->instructions[2].placeId = 1u;
    p->instructions[2].valueId = 1u;
    p->instructions[3].opcode = ZR_SEMANTIC_IR_BRANCH;
    p->instructions[4].opcode = ZR_SEMANTIC_IR_LOAD;
    p->instructions[4].placeId = 1u;
    p->instructions[4].resultValueId = 2u;
    p->instructions[5].opcode = ZR_SEMANTIC_IR_BRANCH;
    p->instructions[5].typeId = ZR_RETAINED_CFG_BOOL_TYPE;
    p->instructions[5].operandCount = 1u;
    p->instructions[6].opcode = ZR_SEMANTIC_IR_CONSTANT;
    p->instructions[6].resultValueId = 3u;
    p->instructions[7].opcode = ZR_SEMANTIC_IR_ADD;
    p->instructions[7].resultValueId = 4u;
    p->instructions[7].operandStart = 1u;
    p->instructions[7].operandCount = 2u;
    p->instructions[8].opcode = ZR_SEMANTIC_IR_STORE;
    p->instructions[8].placeId = 1u;
    p->instructions[8].valueId = 4u;
    p->instructions[9].opcode = ZR_SEMANTIC_IR_BRANCH;
    p->instructions[10].opcode = ZR_SEMANTIC_IR_LOAD;
    p->instructions[10].placeId = 1u;
    p->instructions[10].resultValueId = 5u;
    p->instructions[11].opcode = ZR_SEMANTIC_IR_RETURN;
    p->instructions[11].operandStart = 3u;
    p->instructions[11].operandCount = 1u;
    p->values[0].definitionInstructionId = 2u;
    p->values[1].definitionInstructionId = 5u;
    p->values[2].definitionInstructionId = 7u;
    p->values[3].definitionInstructionId = 8u;
    p->values[4].definitionInstructionId = 11u;
    p->operands[0] = 6u;
    p->operands[1] = 2u;
    p->operands[2] = 3u;
    p->operands[3] = 5u;
}

TZrBool ZrTest_RetainedCfgProduce(const SZrRetainedCfgSpec *specs, TZrUInt32 count,
                                SZrExecIrOwnedModule **owner,
                                SZrRetainedCfgProduction *observation) {
    SSemanticProducer *producers[2] = {ZR_NULL, ZR_NULL};
    SZrExecIrModule transient;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;
    ZrCore_ExecIr_ModuleInit(&transient);
    memset(observation, 0, sizeof(*observation));
    observation->stage = 1u;
    if (specs == ZR_NULL || owner == ZR_NULL || *owner != ZR_NULL ||
        count == 0u || count > 2u) goto cleanup;
    for (index = 0u; index < count; ++index) {
        SZrExecIrBuildInput input = {0};
        SZrExecIrConstant constants[2] = {{ZR_RETAINED_CFG_I64_TYPE, 0u, 0u},
                                        {ZR_RETAINED_CFG_I64_TYPE, 0u, 0u}};
        SZrExecIrRange constantRange;
        TZrBool loop;
        TZrBool parameter;
        TZrUInt32 firstConstant;
        TZrUInt32 secondConstant;
        if ((TZrUInt32)specs[index].graph >= ZR_RETAINED_CFG_GRAPH_COUNT ||
            specs[index].token == 0u || specs[index].signature == 0u) goto cleanup;
        loop = (TZrBool)(specs[index].graph == ZR_RETAINED_CFG_PROMOTED_LOOP ||
                         specs[index].graph == ZR_RETAINED_CFG_PARAMETER_LOOP);
        parameter = (TZrBool)(specs[index].graph >= ZR_RETAINED_CFG_PARAMETER_DIAMOND);
        observation->stage = 2u;
        producers[index] = (SSemanticProducer *)calloc(1u, sizeof(*producers[index]));
        if (producers[index] == ZR_NULL) goto cleanup;
        ++observation->producerAllocated;
        initialize_producer(producers[index], loop, parameter);
        if (loop) make_loop(producers[index]); else make_diamond(producers[index]);
        constants[0].bits = loop ? 0u : 10u;
        constants[1].bits = loop ? 1u : 20u;
        observation->stage = 3u;
        if (!ZrCore_ExecIr_ModuleAppendConstant(&transient, constants, 2u,
                                               &constantRange)) goto cleanup;
        firstConstant = loop ? 1u : 2u;
        secondConstant = loop ? 6u : 5u;
        producers[index]->instructions[firstConstant].hasConstantPoolIndex = ZR_TRUE;
        producers[index]->instructions[firstConstant].constantPoolIndex = constantRange.start;
        producers[index]->instructions[secondConstant].hasConstantPoolIndex = ZR_TRUE;
        producers[index]->instructions[secondConstant].constantPoolIndex = constantRange.start + 1u;
        input.semanticFunction = &producers[index]->semantic;
        input.functionToken = specs[index].token;
        input.signatureHash = specs[index].signature;
        input.options.preserveSourceMaps = ZR_TRUE;
        input.options.preserveDeoptStates = ZR_TRUE;
        input.options.rejectMissingFacts = ZR_TRUE;
        observation->stage = 4u;
        if (!ZrParser_ExecIr_BuildModule(&input, &transient,
                                       &observation->diagnostic)) goto cleanup;
        ++observation->functionsBuilt;
    }
    observation->stage = 5u;
    if (!ZrCore_ExecIr_CloneOwnedModule(&transient, owner,
                                      &observation->diagnostic)) goto cleanup;
    observation->ownerPublished = 1u;
    observation->stage = 0u;
    success = ZR_TRUE;
cleanup:
    ZrCore_ExecIr_FreeModule(&transient);
    observation->transientReleased = 1u;
    for (index = 0u; index < 2u; ++index) {
        if (producers[index] != ZR_NULL) {
            volatile TZrByte *bytes = (volatile TZrByte *)producers[index];
            TZrSize offset;
            for (offset = 0u; offset < sizeof(*producers[index]); ++offset) bytes[offset] = 0xddu;
            free(producers[index]);
            producers[index] = ZR_NULL;
            ++observation->producerReleased;
        }
    }
    if (!success && owner != ZR_NULL && *owner != ZR_NULL && observation->ownerPublished != 0u)
        ZrCore_ExecIr_OwnedModuleFree(owner);
    return success;
}
