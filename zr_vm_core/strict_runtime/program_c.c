#include "zr_vm_strict/program.h"

#include <inttypes.h>
#include <stdlib.h>

static int emit_instruction(FILE *file, const ZrStrictProgram *p, uint32_t pc) {
    const ZrStrictInstruction *i = &p->instructions[pc];
    switch (i->op) {
        case ZR_STRICT_OP_CONST:
            return fprintf(file, "    slots[%" PRIu32 "] = ZrStrictProgram_IntBits(UINT64_C(%" PRIu64 "));\n",
                           i->dst, i->immediate) >= 0;
        case ZR_STRICT_OP_COPY:
            return fprintf(file, "    slots[%" PRIu32 "] = slots[%" PRIu32 "];\n", i->dst, i->lhs) >= 0;
        case ZR_STRICT_OP_JUMP:
            return fprintf(file, "    goto pc_%" PRIu64 ";\n", i->immediate) >= 0;
        case ZR_STRICT_OP_JUMP_IF_FALSE:
            return fprintf(file, "    if (!slots[%" PRIu32 "]) goto pc_%" PRIu64 ";\n", i->lhs, i->immediate) >= 0;
        case ZR_STRICT_OP_RETURN:
            return fprintf(file, "    out->kind = %u; out->scalar = slots[%" PRIu32 "];\n"
                                 "    return ZR_STRICT_PROGRAM_OK;\n", (unsigned)p->kinds[i->lhs], i->lhs) >= 0;
        case ZR_STRICT_OP_HALT:
            return fputs("    return ZR_STRICT_PROGRAM_OK;\n", file) >= 0;
        default:
            if (fprintf(file, "    status = ZrStrictProgram_Scalar(%" PRIu32 ", slots[%" PRIu32 "], ", i->op, i->lhs) < 0) {
                return 0;
            }
            if (i->op == ZR_STRICT_OP_NEG || i->op == ZR_STRICT_OP_NOT) {
                if (fputs("0", file) < 0) return 0;
            } else if (fprintf(file, "slots[%" PRIu32 "]", i->rhs) < 0) {
                return 0;
            }
            return fprintf(file, ", &slots[%" PRIu32 "]);\n"
                                 "    if (status != ZR_STRICT_PROGRAM_OK) { *fault = %" PRIu32 "; return status; }\n",
                           i->dst, pc) >= 0;
    }
}

static int emit_owned_body(FILE *file, const ZrStrictProgram *p, const unsigned char *labels) {
    if (fputs("    static uint8_t kinds[] = {", file) < 0 || (!p->slotCount && fputs("0", file) < 0)) return 0;
    for (uint32_t slot = 0; slot < p->slotCount; ++slot) if (fprintf(file, "%u,", p->kinds[slot]) < 0) return 0;
    if (fputs("};\n    static uint32_t types[] = {", file) < 0 || (!p->slotCount && fputs("0", file) < 0)) return 0;
    for (uint32_t slot = 0; slot < p->slotCount; ++slot) if (fprintf(file, "%" PRIu32 ",", p->slotLayouts[slot]) < 0) return 0;
    if (fputs("};\n    static ZrStrictRecordLayout layouts[] = {\n", file) < 0) return 0;
    for (uint32_t index = 0; index < p->layoutCount; ++index) {
        const ZrStrictRecordLayout *layout = &p->layouts[index];
        if (fprintf(file, "        {%" PRIu32 ",%" PRIu32 ",{", layout->fieldCount, layout->readonlyMask) < 0) return 0;
        for (uint32_t field = 0; field < ZR_STRICT_MAX_FIELDS; ++field)
            if (fprintf(file, "%u,", layout->fields[field]) < 0) return 0;
        if (fputs("},{", file) < 0) return 0;
        for (uint32_t byte = 0; byte < ZR_STRICT_LAYOUT_NAME_SIZE; ++byte)
            if (fprintf(file, "(char)%u,", (unsigned)(unsigned char)layout->name[byte]) < 0) return 0;
        if (fputs("}},\n", file) < 0) return 0;
    }
    if (fprintf(file, "    };\n    static const ZrStrictProgram program = { .kinds = kinds, .slotCount = %" PRIu32
                      ", .slotLayouts = types, .layouts = layouts, .layoutCount = %" PRIu32 " };\n"
                      "    ZrStrictFrame frame;\n"
                      "    ZrStrictFrameSlot slots[%" PRIu32 "];\n"
                      "    ZrStrictProgramStatus status = ZrStrictFrame_Open(&frame, &program, slots, %" PRIu32 ", NULL);\n"
                      "    if (status != ZR_STRICT_PROGRAM_OK) return status;\n"
                      "    *out = (ZrStrictResult){0};\n", p->slotCount, p->layoutCount, p->slotCount ? p->slotCount : 1,
                      p->slotCount) < 0) return 0;
    for (uint32_t pc = 0; pc < p->instructionCount; ++pc) {
        const ZrStrictInstruction *i = &p->instructions[pc];
        if (labels[pc] && fprintf(file, "pc_%" PRIu32 ":;\n", pc) < 0) return 0;
        if (i->op == ZR_STRICT_OP_RETURN) {
            if (fprintf(file, "    out->kind = %u; out->scalar = ZrStrictFrame_Scalar(&frame, %" PRIu32 ");\n    goto cleanup;\n",
                        (unsigned)p->kinds[i->lhs], i->lhs) < 0) return 0;
            continue;
        }
        if (i->op == ZR_STRICT_OP_HALT) {
            if (fputs("    goto cleanup;\n", file) < 0) return 0;
            continue;
        }
        if (i->op == ZR_STRICT_OP_JUMP) {
            if (fprintf(file, "    goto pc_%" PRIu64 ";\n", i->immediate) < 0) return 0;
            continue;
        }
        if (i->op == ZR_STRICT_OP_JUMP_IF_FALSE) {
            if (fprintf(file, "    if (!ZrStrictFrame_Scalar(&frame, %" PRIu32 ")) goto pc_%" PRIu64 ";\n",
                        i->lhs, i->immediate) < 0) return 0;
            continue;
        }
        if (fprintf(file, "    status = ZrStrictFrame_Apply(&frame, &(ZrStrictInstruction){%" PRIu32 ",%" PRIu32 ",%" PRIu32
                          ",%" PRIu32 ",UINT64_C(%" PRIu64 ")});\n"
                          "    if (status != ZR_STRICT_PROGRAM_OK) { *fault = %" PRIu32 "; goto cleanup; }\n",
                    i->op, i->dst, i->lhs, i->rhs, i->immediate, pc) < 0) return 0;
    }
    return fputs("cleanup:;\n    ZrStrictProgramStatus closing = ZrStrictFrame_Close(&frame);\n"
                 "    return status == ZR_STRICT_PROGRAM_OK ? closing : status;\n", file) >= 0;
}

ZrStrictProgramStatus ZrStrictProgram_EmitC(FILE *file, const ZrStrictProgram *p) {
    unsigned char *labels;
    int scalar = 0;
    ZrStrictProgramStatus status = ZrStrictProgram_Verify(p, NULL);
    if (status != ZR_STRICT_PROGRAM_OK) return status;
    if (!file) return ZR_STRICT_PROGRAM_IO_ERROR;
    labels = calloc(p->instructionCount, 1);
    if (!labels) return ZR_STRICT_PROGRAM_OUT_OF_MEMORY;
    for (uint32_t pc = 0; pc < p->instructionCount; ++pc) {
        switch (p->instructions[pc].op) {
            case ZR_STRICT_OP_JUMP:
            case ZR_STRICT_OP_JUMP_IF_FALSE: labels[p->instructions[pc].immediate] = 1; break;
            case ZR_STRICT_OP_CONST: case ZR_STRICT_OP_COPY:
            case ZR_STRICT_OP_RETURN: case ZR_STRICT_OP_HALT: break;
            default: scalar = 1; break;
        }
    }
    status = ZR_STRICT_PROGRAM_IO_ERROR;
    if (fprintf(file, "/* Generated StrictNoGC C; link only zr_vm_strict_runtime. */\n"
                "#include <inttypes.h>\n#include \"zr_vm_strict/program.h\"\n"
                "#if ZR_STRICT_PROGRAM_ABI != %u\n#error StrictNoGC ABI mismatch\n#endif\n\n"
                "static ZrStrictProgramStatus run(ZrStrictResult *out, uint32_t *fault) {\n",
                ZR_STRICT_PROGRAM_ABI) < 0) goto done;
    if (p->layoutCount) {
        if (!emit_owned_body(file, p, labels)) goto done;
    } else {
    if (p->slotCount && fprintf(file, "    int64_t slots[%" PRIu32 "] = {0};\n", p->slotCount) < 0) goto done;
    if (fputs(scalar ? "    ZrStrictProgramStatus status;\n" : "    (void)fault;\n", file) < 0 ||
        fputs("    *out = (ZrStrictResult){0};\n", file) < 0) goto done;
    for (uint32_t pc = 0; pc < p->instructionCount; ++pc) {
        if ((labels[pc] && fprintf(file, "pc_%" PRIu32 ":\n", pc) < 0) || !emit_instruction(file, p, pc)) goto done;
    }
    }
    if (fputs(
            "}\nint main(void) {\n"
            "    ZrStrictResult result;\n"
            "    uint32_t fault = UINT32_MAX;\n"
            "    ZrStrictProgramStatus status = run(&result, &fault);\n"
            "    if (status != ZR_STRICT_PROGRAM_OK) {\n"
            "        fprintf(stderr, \"StrictNoGC: %s at instruction %\" PRIu32 \"\\n\", ZrStrictProgram_StatusName(status), fault);\n"
            "        return 1;\n"
            "    }\n"
            "    if (result.kind == ZR_STRICT_VALUE_I64) printf(\"%\" PRId64 \"\\n\", result.scalar);\n"
            "    if (result.kind == ZR_STRICT_VALUE_BOOL) puts(result.scalar ? \"true\" : \"false\");\n"
            "    return 0;\n"
            "}\n", file) >= 0) status = ZR_STRICT_PROGRAM_OK;
done:
    free(labels);
    return status;
}
