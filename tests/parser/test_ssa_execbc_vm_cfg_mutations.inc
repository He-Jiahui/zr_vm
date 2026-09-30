/* Malformed ExecBC projection mutations for materializer rejection tests. */

static TZrBool drop_phi_predecessor_without_removing_cfg_edge(
        SZrExecBcProjection *projection) {
    SZrExecBcBlock *merge;
    SZrExecBcInstruction *phiInstruction;
    SZrExecIrPhi *phi;
    TZrUInt32 readIndex;
    TZrUInt32 writeIndex = 0u;

    if (projection == ZR_NULL || projection->blockCount < 4u ||
        projection->phiCount == 0u || projection->phiCopyCount < 2u ||
        projection->phiCopyEdges == ZR_NULL ||
        projection->phiCopyEdges[0] != 2u ||
        projection->blocks[3u].phis.count != 1u ||
        projection->blocks[3u].instructions.count == 0u ||
        projection->blocks[2u].successors.count != 1u ||
        projection->successors[
                projection->blocks[2u].successors.start] != 4u) {
        return ZR_FALSE;
    }
    merge = &projection->blocks[3u];
    phi = &projection->phis[merge->phis.start];
    phiInstruction = &projection->instructions[merge->instructions.start];
    if (merge->predecessors.count != 2u ||
        projection->predecessors[merge->predecessors.start] != 2u ||
        projection->predecessors[merge->predecessors.start + 1u] != 3u ||
        phi->incomings.count != 2u ||
        projection->phiIncomings[phi->incomings.start].predecessor != 2u ||
        projection->phiIncomings[phi->incomings.start + 1u].predecessor != 3u) {
        return ZR_FALSE;
    }

    /* Keep edge 3 -> 4 in the CFG, but remove its reciprocal predecessor and
     * phi-copy records from the target block. */
    merge->predecessors.count = 1u;
    phi->incomings.count = 1u;
    phiInstruction->operands.count = 1u;
    phiInstruction->phiRange.count = 1u;
    projection->phiCopyCount = 1u;
    for (readIndex = 0u; readIndex < projection->phiMoveCount; ++readIndex) {
        if (projection->phiMoves[readIndex].edge == 2u) {
            if (writeIndex != readIndex) {
                projection->phiMoves[writeIndex] =
                        projection->phiMoves[readIndex];
            }
            ++writeIndex;
        }
    }
    projection->phiMoveCount = writeIndex;
    return (TZrBool)(writeIndex != 0u);
}
