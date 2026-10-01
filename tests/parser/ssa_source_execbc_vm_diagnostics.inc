static const char *source_execbc_place_base_kind_name(
        EZrParserPlaceBaseKind kind) {
    switch (kind) {
        case ZR_PARSER_PLACE_BASE_LOCAL: return "LOCAL";
        case ZR_PARSER_PLACE_BASE_PARAMETER: return "PARAMETER";
        case ZR_PARSER_PLACE_BASE_THIS: return "THIS";
        case ZR_PARSER_PLACE_BASE_STATIC: return "STATIC";
        case ZR_PARSER_PLACE_BASE_TEMPORARY: return "TEMPORARY";
        case ZR_PARSER_PLACE_BASE_RETURN_SLOT: return "RETURN_SLOT";
        case ZR_PARSER_PLACE_BASE_EXTERNAL_HANDLE: return "EXTERNAL_HANDLE";
        case ZR_PARSER_PLACE_BASE_ENUM_MAX:
        default: return "UNKNOWN";
    }
}

/* On a materializer RED, correlate each source place with its SemIR literal
 * initializer and the ExecIR PLACE_BASE address value. Place nodes do not
 * carry flags; the corresponding ExecIR address value does.
 */
static void source_execbc_report_semir_places(
        const SZrSemanticIrFunction *semanticFunction,
        const SZrExecIrFunction *execIr,
        const SZrExecIrConstant *constants,
        TZrUInt32 constantCount) {
    TZrSize placeIndex;

    if (semanticFunction == ZR_NULL || execIr == ZR_NULL) {
        return;
    }
    (void)printf(
            "source materializer SemIR places count=%u instructions=%u\n",
            (unsigned int)semanticFunction->places.places.length,
            (unsigned int)semanticFunction->instructions.length);
    for (placeIndex = 0u;
         placeIndex < semanticFunction->places.places.length;
         ++placeIndex) {
        TZrPlaceId placeId = (TZrPlaceId)(placeIndex + 1u);
        const SZrParserPlace *place = ZrParser_PlaceGraph_Get(
                &semanticFunction->places, placeId);
        const SZrSemanticIrInstruction *baseInstruction = ZR_NULL;
        const SZrSemanticIrInstruction *initializeInstruction = ZR_NULL;
        const SZrSemanticIrValue *initializedValue = ZR_NULL;
        const SZrSemanticIrInstruction *literalInstruction = ZR_NULL;
        const SZrExecIrValue *addressValue = ZR_NULL;
        TZrUInt32 baseCount = 0u;
        TZrUInt32 initializeCount = 0u;
        TZrSize instructionIndex;
        TZrUInt32 execInstructionIndex;

        if (place == ZR_NULL) {
            (void)printf("source materializer SemIR place index=%u invalid\n",
                         (unsigned int)placeIndex);
            continue;
        }
        for (instructionIndex = 0u;
             instructionIndex < semanticFunction->instructions.length;
             ++instructionIndex) {
            const SZrSemanticIrInstruction *instruction =
                    (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
                            (SZrArray *)&semanticFunction->instructions,
                            instructionIndex);
            if (instruction == ZR_NULL || instruction->placeId != placeId) {
                continue;
            }
            if (instruction->opcode == ZR_SEMANTIC_IR_PLACE_BASE) {
                ++baseCount;
                if (baseInstruction == ZR_NULL) {
                    baseInstruction = instruction;
                }
            } else if (instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE) {
                ++initializeCount;
                if (initializeInstruction == ZR_NULL) {
                    initializeInstruction = instruction;
                }
            }
        }
        if (initializeInstruction != ZR_NULL &&
            initializeInstruction->valueId != ZR_VALUE_ID_INVALID) {
            initializedValue = ZrParser_SemanticIr_Value(
                    semanticFunction, initializeInstruction->valueId);
            if (initializedValue != ZR_NULL &&
                initializedValue->definitionInstructionId !=
                        ZR_SEMANTIC_INSTRUCTION_ID_INVALID) {
                literalInstruction = ZrParser_SemanticIr_InstructionAt(
                        semanticFunction,
                        initializedValue->definitionInstructionId - 1u);
            }
        }
        if (baseInstruction != ZR_NULL) {
            for (execInstructionIndex = 0u;
                 execInstructionIndex < execIr->instructionCount;
                 ++execInstructionIndex) {
                const SZrExecIrInstruction *execInstruction =
                        &execIr->instructions[execInstructionIndex];
                TZrUInt32 valueIndex;
                if (execInstruction->opcode != ZR_EXEC_IR_OPCODE_PLACE_BASE ||
                    execInstruction->sourceId != baseInstruction->id) {
                    continue;
                }
                for (valueIndex = 0u; valueIndex < execIr->valueCount;
                     ++valueIndex) {
                    const SZrExecIrValue *value = &execIr->values[valueIndex];
                    if (value->definition == execInstructionIndex + 1u &&
                        (value->flags & ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS) !=
                                0u) {
                        addressValue = value;
                        break;
                    }
                }
                break;
            }
        }
        (void)printf(
                "source materializer SemIR place id=%u kind=%s(%u) "
                "identity=%u parent=%u type=%u projections=%u "
                "baseInstr=%u baseCount=%u "
                "initializeInstr=%u initializeCount=%u initValue=%u "
                "literalDef=%u literalOpcode=%u literalType=%u "
                "literalPool=%u literalHasPool=%u literalVmType=%u "
                "literalBits=%llu "
                "execAddressValue=%u execAddressFlags=%u external=%u "
                "placeAddress=%u promotable=%u\n",
                (unsigned int)place->id,
                source_execbc_place_base_kind_name(place->base.kind),
                (unsigned int)place->base.kind,
                (unsigned int)place->base.identity,
                (unsigned int)place->parentId,
                (unsigned int)place->typeId,
                (unsigned int)place->projections.length,
                baseInstruction != ZR_NULL
                        ? (unsigned int)baseInstruction->id : 0u,
                (unsigned int)baseCount,
                initializeInstruction != ZR_NULL
                        ? (unsigned int)initializeInstruction->id : 0u,
                (unsigned int)initializeCount,
                initializeInstruction != ZR_NULL
                        ? (unsigned int)initializeInstruction->valueId : 0u,
                initializedValue != ZR_NULL
                        ? (unsigned int)initializedValue->definitionInstructionId
                        : 0u,
                literalInstruction != ZR_NULL
                        ? (unsigned int)literalInstruction->opcode : 0u,
                initializedValue != ZR_NULL
                        ? (unsigned int)initializedValue->typeId : 0u,
                literalInstruction != ZR_NULL
                        ? (unsigned int)literalInstruction->constantPoolIndex : 0u,
                literalInstruction != ZR_NULL
                        ? (unsigned int)literalInstruction->hasConstantPoolIndex
                        : 0u,
                literalInstruction != ZR_NULL &&
                                literalInstruction->hasConstantPoolIndex != ZR_FALSE &&
                                constants != ZR_NULL &&
                                literalInstruction->constantPoolIndex < constantCount
                        ? (unsigned int)constants[
                                  literalInstruction->constantPoolIndex].typeToken
                        : 0u,
                (unsigned long long)(
                        literalInstruction != ZR_NULL &&
                                literalInstruction->hasConstantPoolIndex != ZR_FALSE &&
                                constants != ZR_NULL &&
                                literalInstruction->constantPoolIndex < constantCount
                        ? constants[literalInstruction->constantPoolIndex].bits
                        : 0u),
                addressValue != ZR_NULL ? (unsigned int)addressValue->id : 0u,
                addressValue != ZR_NULL ? (unsigned int)addressValue->flags : 0u,
                addressValue != ZR_NULL
                        ? (unsigned int)((addressValue->flags &
                                           ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u)
                        : 0u,
                addressValue != ZR_NULL
                        ? (unsigned int)((addressValue->flags &
                                           ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS) != 0u)
                        : 0u,
                addressValue != ZR_NULL
                        ? (unsigned int)((addressValue->flags &
                                           ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) != 0u)
                        : 0u);
    }
}

/* Print the actual compiler-produced materializer input on a RED. The VM
 * currently rejects several ExecIR side tables before instruction checking,
 * so the structured diagnostic alone cannot identify which table triggered
 * that guard.
 */
static void source_execbc_report_materializer_input(
        const SZrExecIrFunction *execIr,
        const SZrSemanticIrFunction *semanticFunction,
        const SZrExecIrConstant *constants,
        TZrUInt32 constantCount,
        const SZrExecIrOracleExecutionResult *oracle,
        const SZrExecBcProjection *projection) {
    TZrUInt32 index;
    if (execIr == ZR_NULL || projection == ZR_NULL) {
        return;
    }
    (void)printf(
            "source materializer input functionId=%u functionToken=%u "
            "signatureHash=%llu entry=%u oracleReturned=%u oracleKind=%u "
            "oracleInteger=%lld oracleBlock=%u\n",
            (unsigned int)execIr->id,
            (unsigned int)execIr->functionToken,
            (unsigned long long)execIr->signatureHash,
            (unsigned int)execIr->entryBlockId,
            oracle != ZR_NULL ? (unsigned int)oracle->returned : 0u,
            oracle != ZR_NULL ? (unsigned int)oracle->returnValue.kind : 0u,
            (long long)(oracle != ZR_NULL &&
                                oracle->returnValue.kind ==
                                        ZR_EXEC_IR_ORACLE_VALUE_SIGNED
                        ? oracle->returnValue.as.signedInteger
            : 0),
            oracle != ZR_NULL ? (unsigned int)oracle->currentBlock : 0u);
    source_execbc_report_semir_places(
            semanticFunction, execIr, constants, constantCount);
    (void)printf(
            "source materializer ExecIR counts values=%u instructions=%u "
            "blocks=%u operands=%u results=%u phis=%u phiIncomings=%u "
            "memoryTokens=%u gcMaps=%u gcRoots=%u stateMap=%u deoptStates=%u "
            "bindingRows=%u\n",
            (unsigned int)execIr->valueCount,
            (unsigned int)execIr->instructionCount,
            (unsigned int)execIr->blockCount,
            (unsigned int)execIr->operandCount,
            (unsigned int)execIr->resultCount,
            (unsigned int)execIr->phiCount,
            (unsigned int)execIr->phiIncomingCount,
            (unsigned int)execIr->memoryTokenCount,
            (unsigned int)execIr->gcMapCount,
            (unsigned int)execIr->gcRootCount,
            (unsigned int)(execIr->stateMap != ZR_NULL),
            (unsigned int)execIr->deoptStateCount,
            (unsigned int)execIr->bindingRowCount);
    if (execIr->frameLayout != ZR_NULL) {
        (void)printf(
                "source materializer ExecIR frame parameters=%u locals=%u "
                "slots=%u bytes=%u align=%u logicalSlots=%u storageSlots=%u "
                "parameterPrefix=%u returnOffset=%u hash=%llu\n",
                (unsigned int)execIr->frameLayout->parameterCount,
                (unsigned int)execIr->frameLayout->localCount,
                (unsigned int)execIr->frameLayout->slotCount,
                (unsigned int)execIr->frameLayout->frameByteSize,
                (unsigned int)execIr->frameLayout->frameByteAlign,
                (unsigned int)execIr->frameLayout->logicalSlotCount,
                (unsigned int)execIr->frameLayout->storageSlotCount,
                (unsigned int)execIr->frameLayout->parameterPrefixCount,
                (unsigned int)execIr->frameLayout->returnBufferOffset,
                (unsigned long long)execIr->frameLayout->layoutHash);
        if (execIr->frameLayout->slots != ZR_NULL) {
            for (index = 0u; index < execIr->frameLayout->slotCount; ++index) {
                const SZrExecIrFrameSlot *slot =
                        &execIr->frameLayout->slots[index];
                (void)printf(
                        "source materializer ExecIR frameSlot id=%u offset=%u "
                        "size=%u align=%u type=%u kind=%u\n",
                        (unsigned int)slot->slotId,
                        (unsigned int)slot->byteOffset,
                        (unsigned int)slot->byteSize,
                        (unsigned int)slot->byteAlign,
                        (unsigned int)slot->typeToken,
                        (unsigned int)slot->kind);
            }
        }
    } else {
        (void)printf("source materializer ExecIR frame=absent\n");
    }

    (void)printf(
            "source materializer projection functionId=%u functionToken=%u "
            "signatureHash=%llu entry=%u runnable=%u ownershipTag=%u "
            "valueSlotMappings=%u physicalSlots=%u instructions=%u "
            "blocks=%u syntheticBlocks=%u operands=%u results=%u phis=%u "
            "phiIncomings=%u phiCopies=%u phiMoves=%u constants=%u "
            "sourceMaps=%u\n",
            (unsigned int)projection->functionId,
            (unsigned int)projection->functionToken,
            (unsigned long long)projection->signatureHash,
            (unsigned int)projection->entryBlockId,
            (unsigned int)projection->runnable,
            (unsigned int)projection->ownershipTag,
            (unsigned int)projection->valueSlotCount,
            (unsigned int)projection->physicalSlotCount,
            (unsigned int)projection->instructionCount,
            (unsigned int)projection->blockCount,
            (unsigned int)projection->syntheticBlockCount,
            (unsigned int)projection->operandCount,
            (unsigned int)projection->resultCount,
            (unsigned int)projection->phiCount,
            (unsigned int)projection->phiIncomingCount,
            (unsigned int)projection->phiCopyCount,
            (unsigned int)projection->phiMoveCount,
            (unsigned int)projection->constantCount,
            (unsigned int)projection->sourceMapCount);
    (void)printf(
            "source materializer projection guard frameHash=%llu frameBytes=%u "
            "frameAlign=%u frameSlots=%u frameSlotsPtr=%u logicalSlots=%u "
            "storageSlots=%u parameterPrefix=%u returnOffset=%u layouts=%u "
            "layoutsPtr=%u gcMapPresent=%u gcMapCount=%u gcRoots=%u "
            "gcRootsPtr=%u deoptStates=%u deoptValues=%u deoptAggregates=%u "
            "deoptFields=%u stateMapPresent=%u memoryTokens=%u "
            "memoryTokensPtr=%u deoptStatesPtr=%u deoptValuesPtr=%u "
            "deoptAggregatesPtr=%u deoptFieldsPtr=%u\n",
            (unsigned long long)projection->frameLayoutHash,
            (unsigned int)projection->frameByteSize,
            (unsigned int)projection->frameByteAlign,
            (unsigned int)projection->frameSlotCount,
            (unsigned int)(projection->frameSlots != ZR_NULL),
            (unsigned int)projection->logicalSlotCount,
            (unsigned int)projection->storageSlotCount,
            (unsigned int)projection->parameterPrefixBytes,
            (unsigned int)projection->returnAreaOffset,
            (unsigned int)projection->layoutCount,
            (unsigned int)(projection->layouts != ZR_NULL),
            (unsigned int)projection->gcMapPresent,
            (unsigned int)projection->gcMapCount,
            (unsigned int)projection->gcRootCount,
            (unsigned int)(projection->gcRoots != ZR_NULL),
            (unsigned int)projection->deoptStateCount,
            (unsigned int)projection->deoptValueCount,
            (unsigned int)projection->deoptAggregateCount,
            (unsigned int)projection->deoptAggregateFieldCount,
            (unsigned int)projection->stateMapPresent,
            (unsigned int)projection->memoryTokenCount,
            (unsigned int)(projection->memoryTokens != ZR_NULL),
            (unsigned int)(projection->deoptStates != ZR_NULL),
            (unsigned int)(projection->deoptValues != ZR_NULL),
            (unsigned int)(projection->deoptAggregates != ZR_NULL),
            (unsigned int)(projection->deoptAggregateFields != ZR_NULL));

    if (projection->constants != ZR_NULL) {
        for (index = 0u; index < projection->constantCount; ++index) {
            (void)printf(
                    "source materializer constant index=%u type=%u flags=%u "
                    "bits=%llu\n",
                    (unsigned int)index,
                    (unsigned int)projection->constants[index].typeToken,
                    (unsigned int)projection->constants[index].flags,
                    (unsigned long long)projection->constants[index].bits);
        }
    }
    if (projection->slotValues != ZR_NULL) {
        for (index = 0u; index < projection->physicalSlotCount; ++index) {
            const SZrExecIrValue *value = &projection->slotValues[index];
            (void)printf(
                    "source materializer slot physical=%u value=%u def=%u "
                    "type=%u flags=%u ownership=%u nullability=%u\n",
                    (unsigned int)index,
                    (unsigned int)value->id,
                    (unsigned int)value->definition,
                    (unsigned int)value->typeToken,
                    (unsigned int)value->flags,
                    (unsigned int)value->ownership,
                    (unsigned int)value->nullability);
        }
    }
    if (projection->blocks != ZR_NULL) {
        for (index = 0u; index < projection->blockCount; ++index) {
            const SZrExecBcBlock *block = &projection->blocks[index];
            (void)printf(
                    "source materializer block id=%u flags=%u instructions=%u/%u "
                    "preds=%u/%u successors=%u/%u phis=%u/%u terminator=%u "
                    "predIds=",
                    (unsigned int)block->id,
                    (unsigned int)block->flags,
                    (unsigned int)block->instructions.start,
                    (unsigned int)block->instructions.count,
                    (unsigned int)block->predecessors.start,
                    (unsigned int)block->predecessors.count,
                    (unsigned int)block->successors.start,
                    (unsigned int)block->successors.count,
                    (unsigned int)block->phis.start,
                    (unsigned int)block->phis.count,
                    (unsigned int)block->terminatorInstructionId);
            source_execbc_print_block_range(
                    projection->predecessors, projection->predecessorCount,
                    block->predecessors);
            (void)printf(" succIds=");
            source_execbc_print_block_range(
                    projection->successors, projection->successorCount,
                    block->successors);
            (void)printf("\n");
        }
    }
    if (projection->instructions != ZR_NULL) {
        for (index = 0u; index < projection->instructionCount; ++index) {
            const SZrExecBcInstruction *instruction =
                    &projection->instructions[index];
            const TZrChar *opcodeName = ZrCore_ExecIr_OpcodeName(
                    (EZrExecIrOpcode)instruction->opcode);
            (void)printf(
                    "source materializer instruction id=%u block=%u pc=%u "
                    "opcode=%s(%u) source=%u flags=%u type=%u matchType=%u "
                    "layout=%u deopt=%u bindingRow=%u operandIds=",
                    (unsigned int)(index + 1u),
                    (unsigned int)source_execbc_projection_instruction_block(
                            projection, index),
                    (unsigned int)instruction->pc,
                    opcodeName != ZR_NULL ? opcodeName : "unknown",
                    (unsigned int)instruction->opcode,
                    (unsigned int)instruction->sourceId,
                    (unsigned int)instruction->flags,
                    (unsigned int)instruction->typeToken,
                    (unsigned int)instruction->matchTypeToken,
                    (unsigned int)instruction->layoutId,
                    (unsigned int)instruction->deoptId,
                    (unsigned int)instruction->bindingRow);
            source_execbc_print_value_range(
                    projection->operands, projection->operandCount,
                    instruction->operands);
            (void)printf(" resultIds=");
            source_execbc_print_value_range(
                    projection->results, projection->resultCount,
                    instruction->results);
            (void)printf(
                    " successorRange=%u/%u memoryIn=%u/%u memoryOut=%u/%u "
                    "effectIn=%u effectOut=%u phiRange=%u/%u\n",
                    (unsigned int)instruction->successorRange.start,
                    (unsigned int)instruction->successorRange.count,
                    (unsigned int)instruction->memoryIn.start,
                    (unsigned int)instruction->memoryIn.count,
                    (unsigned int)instruction->memoryOut.start,
                    (unsigned int)instruction->memoryOut.count,
                    (unsigned int)instruction->effectIn,
                    (unsigned int)instruction->effectOut,
                    (unsigned int)instruction->phiRange.start,
                    (unsigned int)instruction->phiRange.count);
        }
    }
}
