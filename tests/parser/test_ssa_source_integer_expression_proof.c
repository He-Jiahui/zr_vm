#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#define ZR_TEST_INTEGER_PROOF_NO_REDIRECT
#include "support/ssa_source_integer_expression_proof_shim.h"
#include "zr_vm_parser/compiler.h"
#include "exec_ir/source/exec_ir_source_integer_constants.h"
#include "support/ssa_source_returned_retention_fixture.h"
#include "tests/core/support/ssa_exec_ir_owner_capture.h"
#include "exec_ir/exec_ir_frame_layout.h"
#include "exec_ir/source/exec_ir_source_frame.h"
#include "zr_vm_core/function_exec_ir.h"
#include "exec_ir/exec_ir_mutation_internal.h"

static const char g_subject[] = "return 2 + 3 * 4;";
static const SZrSourceCompileOptions g_exec = {ZR_SOURCE_RETENTION_REQUIRE_SCALAR_EXECIR};
enum {
    SEM_OPCODE, SEM_OPERANDS, SEM_RESULT_TYPE, SEM_POOL_INDEX,
    COMPILER_BITS, RUNTIME_BITS, COMPILER_TAG, RUNTIME_TAG,
    SEM_RANGE, SEM_SOURCE_MAP, ARRAY_WIDTH, ARRAY_CAPACITY, AUXILIARY_FACT,
    AST_DIVISION, AST_NULL_LEFT, AST_CYCLE, AST_SHARED,
    ORIGINAL_OPCODE, ORIGINAL_OPERANDS, COMPACTED_OPCODE, COMPACTED_OPERANDS,
    EXEC_SOURCE_MAP, MATRIX_CASES
};
static const char *const g_names[MATRIX_CASES] = {
    "semantic-opcode", "semantic-operands", "semantic-result-type", "constant-index",
    "compiler-bits", "runtime-bits", "compiler-tag", "runtime-tag", "semantic-range",
    "semantic-source-map", "array-width", "array-capacity", "auxiliary-fact",
    "ast-division", "ast-null-left", "ast-cycle", "ast-shared-child", "original-opcode",
    "original-operands", "compacted-opcode", "compacted-operands", "exec-source-map"
};
typedef struct SInputs {
    const SZrSemanticIrFunction *semantic;
    const SZrSemanticContext *context;
    const SZrAstNode *script;
    const SZrFunctionSourceCallableIdentity *identity;
    const SZrArray *compiler;
    const SZrTypeValue *runtime;
    TZrUInt32 count;
    const SZrExecIrModule *original;
    const SZrExecIrFunction *finalized;
} SInputs;
typedef struct SMutation {
    void *pointer;
    size_t bytes;
    unsigned char before[sizeof(SZrSemanticIrInstruction) + sizeof(SZrArray)];
    unsigned char after[sizeof(SZrSemanticIrInstruction) + sizeof(SZrArray)];
} SMutation;
typedef struct SObservation {
    unsigned prepared, calls, refused, restored, sameDiagnostic, diagnosticClean;
    unsigned oldPreserved, released, retry, oldReleased, ownerComplete, inputPreserved, errors;
    SZrExecIrDiagnostic diagnostic;
} SObservation;
static SObservation g_rows[MATRIX_CASES];
static unsigned g_runMatrix, g_runDirect, g_wrapperCalls, g_actualCalls, g_errors;
static unsigned g_emptyPreserved, g_phaseRefused, g_scratchReleased, g_directClean;
static SZrExecIrOwnedModule *g_probeOwner;

/* The large case alone uses the existing logical visitor with a larger bounded
 * sink. Keep the original 4096-word capture and both original cases unchanged. */
enum { SPAN_WORDS = 16384, SPAN_LEAVES = 44, SPAN_VALUES = 2 * SPAN_LEAVES - 1,
       SPAN_SOURCE_BYTES = 4 * SPAN_LEAVES + 5, SPAN_EVENTS = 4096,
       SPAN_HEALTHY = 1, SPAN_FAULT = 2, SPAN_RETRY = 3 };
typedef struct SLargeContents {
    TZrUInt64 words[SPAN_WORDS];
    size_t count;
    unsigned overflow;
} SLargeContents;
typedef struct SSpanObservation {
    unsigned calls, errors, token, semanticValues, places, originalValues, finalizedValues;
    unsigned proofReady, frameComplete, inputPreserved, ownerComplete, returned, sourceOwner, oracle;
    unsigned eventCount, eventQueries, growCount, growOrdinal, scratchCount, scratchOrdinal;
    unsigned scratchFree1, growFree1, growNoRecord, failedCount, prefixSame, armed, windowReleased;
    unsigned wrapperDelta, actualDelta, closedReads, closedReleased, crtReleased, closedHealthy;
    size_t scratchBytes, originalWords, finalizedWords;
    TZrBool result;
    const void *ownerBefore, *ownerAfter;
    SZrOwnerHeapStats before, after, closedOwner;
    SZrOwnerHeapEvent grow;
    SZrExecIrDiagnostic lower;
    SZrSourceCompileDiagnostic source;
    SZrReturnedVmStats closedVm;
    SZrReturnedHeapStats closedCrt[2];
} SSpanObservation;
static unsigned g_runSpan, g_spanBaselineReady;
static char g_spanSource[SPAN_SOURCE_BYTES + 1];
static SSpanObservation g_span[3];
static SZrOwnerHeapEvent g_spanEvents[SPAN_EVENTS];
static SLargeContents g_spanOriginalBefore, g_spanFinalizedBefore;
static SLargeContents g_spanOriginalAfter, g_spanFinalizedAfter;

/* Diagnostic-only producer observation. Never replace the real result or its
 * diagnostic, and never inspect more than one actual refused semantic row. */
static void bridge_array(const char *name, const SZrArray *a) {
    fprintf(stderr, "INTEGER_PROOF_BRIDGE_ARRAY name=%s valid=%u width=%llu length=%llu capacity=%llu head=%u\n",
            name, (unsigned)a->isValid, (unsigned long long)a->elementSize,
            (unsigned long long)a->length, (unsigned long long)a->capacity, (unsigned)(a->head != ZR_NULL));
}
static int bridge_rows(const SZrArray *a, size_t width, size_t alignment) {
    return a->isValid && a->elementSize == width && a->length <= a->capacity && a->head &&
            (uintptr_t)a->head % alignment == 0u && a->capacity <= SIZE_MAX / width &&
            a->capacity * width <= UINTPTR_MAX - (uintptr_t)a->head;
}
static void bridge_range(const char *name, const SZrFileRange *r) {
    fprintf(stderr, "INTEGER_PROOF_BRIDGE_RANGE name=%s source=%u start=%llu:%d:%d end=%llu:%d:%d\n",
            name, (unsigned)(r->source != ZR_NULL), (unsigned long long)r->start.offset,
            (int)r->start.line, (int)r->start.column, (unsigned long long)r->end.offset,
            (int)r->end.line, (int)r->end.column);
}
TZrBool ZrTests_IntegerProof_Prepare(const struct SZrCompilerState *c,
        SSourceIntegerProof *output, SZrExecIrDiagnostic *diagnostic) {
    TZrBool result = source_integer_constants_prepare(c, output, diagnostic);
    if (!result) {
        const SZrExecIrDiagnostic zero = {0};
        const SZrExecIrDiagnostic *d = diagnostic ? diagnostic : &zero;
        fprintf(stderr, "INTEGER_PROOF_BRIDGE_REFUSED has_diag=%u code=%u token=%u block=%u instruction=%u source=%u "
                "expected_version=%u actual_version=%u expected_hash=%llu actual_hash=%llu\n",
                (unsigned)(diagnostic != ZR_NULL), (unsigned)d->code, (unsigned)d->functionToken,
                (unsigned)d->blockId, (unsigned)d->instructionId, (unsigned)d->sourceId,
                (unsigned)d->expectedVersion, (unsigned)d->actualVersion,
                (unsigned long long)d->expectedHash, (unsigned long long)d->actualHash);
        if (c) {
            const SZrSemanticIrFunction *s = &c->preSemanticIr;
            const SZrFunction *f = c->currentFunction;
            fprintf(stderr, "INTEGER_PROOF_BRIDGE_ELIGIBLE state=%u function=%u context=%u error=%u "
                    "ast_same=%u function_node=%u top_level=%u submission=%u submission_entry=%u module_key=%u "
                    "initialized=%u validated=%u cfg_active=%u cfg_terminated=%u closures=%llu\n",
                    (unsigned)(c->state != ZR_NULL), (unsigned)(f != ZR_NULL), (unsigned)(c->semanticContext != ZR_NULL),
                    (unsigned)c->hasError, (unsigned)(c->currentAst && c->currentAst == c->scriptAst),
                    (unsigned)(c->currentFunctionNode != ZR_NULL), (unsigned)(c->topLevelFunction != ZR_NULL),
                    (unsigned)(c->submissionContext != ZR_NULL), (unsigned)(c->submissionEntryFunction != ZR_NULL),
                    (unsigned)(c->currentModuleKey != ZR_NULL), (unsigned)c->preSemanticIrInitialized,
                    (unsigned)c->preSemanticIrValidated, (unsigned)c->preSemanticIrCfgActive,
                    (unsigned)c->preSemanticIrCfgTerminated, (unsigned long long)c->closureVars.length);
            if (f) {
                fprintf(stderr, "INTEGER_PROOF_BRIDGE_FUNCTION owner=%u borrowed=%u module_version=%u varargs=%u "
                        "return_present=%u return_type=%u nullable=%u array=%u type_name=%u ownership=%u "
                        "source_identity=%u name_same=%u parameters=%u imports=%u effects=%u children=%u "
                        "closures=%u typed_closures=%u exports=%u typed_exports=%u summaries=%u bindings=%u manifests=%u\n",
                        (unsigned)(f->ownerFunction != ZR_NULL), (unsigned)f->childFunctionGraphIsBorrowed,
                        (unsigned)(f->moduleVersion != ZR_NULL), (unsigned)f->hasVariableArguments,
                        (unsigned)f->hasCallableReturnType, (unsigned)f->callableReturnType.baseType,
                        (unsigned)f->callableReturnType.isNullable, (unsigned)f->callableReturnType.isArray,
                        (unsigned)(f->callableReturnType.typeName != ZR_NULL), (unsigned)f->callableReturnType.ownershipQualifier,
                        (unsigned)f->hasSourceCallableIdentity, (unsigned)(f->functionName == c->currentModuleKey),
                        (unsigned)f->parameterCount, (unsigned)f->staticImportLength, (unsigned)f->moduleEntryEffectLength,
                        (unsigned)f->childFunctionLength, (unsigned)f->closureValueLength, (unsigned)f->typedClosureBindingLength,
                        (unsigned)f->exportedVariableLength, (unsigned)f->typedExportedSymbolLength,
                        (unsigned)f->exportedCallableSummaryLength, (unsigned)f->topLevelCallableBindingLength,
                        (unsigned)f->testManifestDataLength);
            }
            fprintf(stderr, "INTEGER_PROOF_BRIDGE_GEOMETRY entry=%u exit=%u symbol=%u callable_type=%u\n",
                    (unsigned)s->cfg.entryBlockId, (unsigned)s->cfg.exitBlockId,
                    (unsigned)s->symbolId, (unsigned)s->callableTypeId);
            bridge_array("instructions", &s->instructions); bridge_array("values", &s->values);
            bridge_array("places", &s->places.places); bridge_array("blocks", &s->cfg.blocks);
            bridge_array("regions", &s->regions); bridge_array("source_map", &s->sourceMap);
            if (s->cfg.blocks.length && bridge_rows(&s->cfg.blocks, sizeof(SZrParserCfgBlock), _Alignof(SZrParserCfgBlock))) {
                const SZrParserCfgBlock *b = (const SZrParserCfgBlock *)s->cfg.blocks.head;
                fprintf(stderr, "INTEGER_PROOF_BRIDGE_BLOCK id=%u kind=%u statement=%u first=%u count=%u "
                        "successors=%u predecessors=%u terminator=%u\n", (unsigned)b->id, (unsigned)b->kind,
                        (unsigned)(b->statement != ZR_NULL), (unsigned)b->firstInstructionIndex,
                        (unsigned)b->instructionCount, (unsigned)b->successorCount, (unsigned)b->predecessorCount,
                        (unsigned)b->terminatorKind);
            }
            if (s->regions.length && bridge_rows(&s->regions, sizeof(SZrSemanticIrRegion), _Alignof(SZrSemanticIrRegion))) {
                const SZrSemanticIrRegion *r = (const SZrSemanticIrRegion *)s->regions.head;
                fprintf(stderr, "INTEGER_PROOF_BRIDGE_REGION id=%u parent=%u escape=%u\n",
                        (unsigned)r->id, (unsigned)r->parentId, (unsigned)r->escapeBound);
                bridge_range("region", &r->sourceRange);
            }
            if (c->scriptAst) {
                const SZrAstNode *script = c->scriptAst;
                fprintf(stderr, "INTEGER_PROOF_BRIDGE_AST script_type=%u\n", (unsigned)script->type);
                bridge_range("script", &script->location);
                if (script->type == ZR_AST_SCRIPT && script->data.script.statements) {
                    const SZrAstNodeArray *a = script->data.script.statements;
                    if (a->count == 1u && a->count <= a->capacity && a->nodes &&
                            (uintptr_t)a->nodes % _Alignof(SZrAstNode *) == 0u &&
                            a->capacity <= SIZE_MAX / sizeof(*a->nodes) &&
                            a->capacity * sizeof(*a->nodes) <= UINTPTR_MAX - (uintptr_t)a->nodes && a->nodes[0]) {
                        const SZrAstNode *returned = a->nodes[0];
                        fprintf(stderr, "INTEGER_PROOF_BRIDGE_RETURN type=%u\n", (unsigned)returned->type);
                        bridge_range("return", &returned->location);
                        if (returned->type == ZR_AST_RETURN_STATEMENT && returned->data.returnStatement.expr) {
                            const SZrAstNode *expr = returned->data.returnStatement.expr;
                            fprintf(stderr, "INTEGER_PROOF_BRIDGE_EXPRESSION type=%u\n", (unsigned)expr->type);
                            bridge_range("expression", &expr->location);
                        }
                    }
                }
            }
            if (d->sourceId && d->sourceId <= s->instructions.length &&
                    bridge_rows(&s->instructions, sizeof(SZrSemanticIrInstruction), _Alignof(SZrSemanticIrInstruction))) {
                const SZrSemanticIrInstruction *r = &((const SZrSemanticIrInstruction *)s->instructions.head)[d->sourceId - 1u];
                fprintf(stderr, "INTEGER_PROOF_BRIDGE_ROW id=%u opcode=%u type=%u place=%u result=%u operands=%u:%u\n",
                        (unsigned)r->id, (unsigned)r->opcode, (unsigned)r->typeId, (unsigned)r->placeId,
                        (unsigned)r->resultValueId, (unsigned)r->operandStart, (unsigned)r->operandCount);
                bridge_range("row", &r->sourceRange);
                if (r->resultValueId && r->resultValueId <= s->values.length &&
                        bridge_rows(&s->values, sizeof(SZrSemanticIrValue), _Alignof(SZrSemanticIrValue))) {
                    const SZrSemanticIrValue *v = &((const SZrSemanticIrValue *)s->values.head)[r->resultValueId - 1u];
                    fprintf(stderr, "INTEGER_PROOF_BRIDGE_VALUE id=%u type=%u definition=%u facts_type=%u ownership=%u nullability=%u\n",
                            (unsigned)v->id, (unsigned)v->typeId, (unsigned)v->definitionInstructionId,
                            (unsigned)v->facts.typeId, (unsigned)v->facts.ownership, (unsigned)v->facts.nullability);
                    bridge_range("value", &v->sourceRange);
                }
            }
        }
    }
    return result;
}

/* The private frame role is shared with the original fixture. These adapters
 * impose no single-slot observation and delegate all real heap work. */
void *ZrTests_ReturnedExec_FrameMalloc(size_t bytes) { return ZrTests_ExecOwner_Malloc(bytes); }
void *ZrTests_ReturnedExec_FrameCalloc(size_t n, size_t width) { return ZrTests_ExecOwner_Calloc(n, width); }
void ZrTests_ReturnedExec_FrameFree(void *p) { ZrTests_ExecOwner_Free(p); }
void ZrTests_ReturnedExec_PackedFree(SZrExecIrPackedFrameLayout *p) { ZrParser_ExecIr_PackedFrameLayoutFree(p); }

static TZrBool real_call(const SInputs *i, SZrExecIrOwnedModule **output, SZrExecIrDiagnostic *d) {
    return source_snapshot_finalized_primitive_module(i->semantic, i->context, i->script, i->identity,
            i->compiler, i->runtime, i->count, i->original, i->finalized, output, d);
}
static int words_same(const SZrOwnerContents *a, const SZrOwnerContents *b) {
    return a->count == b->count && memcmp(a->words, b->words, a->count * sizeof(*a->words)) == 0;
}
static int record_frees(unsigned record, unsigned expected) {
    unsigned frees = 0u;
    return ZrTests_ExecOwner_TryRecordFrees(record, &frees) && frees == expected;
}
static int released(unsigned first, unsigned last) {
    if (first > last || last > ZrTests_ExecOwner_Stats().records) return 0;
    for (unsigned n = first; n < last; ++n) if (!record_frees(n, 1u)) return 0;
    return 1;
}
static int diagnostic_same(const SZrExecIrDiagnostic *a, const SZrExecIrDiagnostic *b) {
#define EQ(field) if (a->field != b->field) return 0
    EQ(code); EQ(functionToken); EQ(blockId); EQ(instructionId); EQ(sourceId);
    EQ(expectedVersion); EQ(actualVersion); EQ(expectedHash); EQ(actualHash);
#undef EQ
    return 1;
}
static int diagnostic_clean(const SInputs *i, const SZrExecIrDiagnostic *d) {
    if (!d->code || d->functionToken != i->original->functions[0].functionToken ||
        d->blockId > 1u || d->instructionId > i->original->functions[0].instructionCount ||
        d->sourceId > i->semantic->instructions.length) return 0;
    if (d->instructionId && d->sourceId &&
        i->original->functions[0].instructions[d->instructionId - 1u].sourceId != d->sourceId) return 0;
#define CLEAN32(field) if ((TZrUInt32)d->field == UINT32_C(0xA5A5A5A5)) return 0
    CLEAN32(code); CLEAN32(functionToken); CLEAN32(blockId); CLEAN32(instructionId); CLEAN32(sourceId);
    CLEAN32(expectedVersion); CLEAN32(actualVersion);
#undef CLEAN32
    return d->expectedHash != UINT64_C(0xA5A5A5A5A5A5A5A5) &&
            d->actualHash != UINT64_C(0xA5A5A5A5A5A5A5A5);
}
static int diagnostic_zero(const SZrExecIrDiagnostic *d) {
    SZrExecIrDiagnostic zero = {0}; return diagnostic_same(d, &zero);
}
/* Read every initialized input byte again only after mutation has been restored.
 * This witness is not an allocator/readability or cryptographic authority. */
static TZrUInt64 bytes(TZrUInt64 h, const void *p, size_t n) {
    const unsigned char *s = p;
    for (size_t k = 0; k < n; ++k) h = (h ^ s[k]) * UINT64_C(1099511628211);
    return h;
}
static TZrUInt64 input_fingerprint(const SInputs *i, const SSourceIntegerProof *p) {
    TZrUInt64 h = UINT64_C(1469598103934665603);
    const SZrSemanticIrFunction *s = i->semantic;
#define OBJECT(o) h = bytes(h, o, sizeof(*(o)))
#define ARRAY(a) h = bytes(h, (a).head, (a).length * (a).elementSize)
    OBJECT(s); OBJECT(i->context); OBJECT(i->identity); OBJECT(i->compiler);
    ARRAY(s->places.places); ARRAY(s->cfg.blocks); ARRAY(s->locals); ARRAY(s->values);
    ARRAY(s->instructions); ARRAY(s->valueOperands); ARRAY(s->regions); ARRAY(s->cleanupScopes);
    ARRAY(s->sourceMap); ARRAY(s->loanFacts); ARRAY(s->escapeFacts); ARRAY(s->contiguousViewFacts);
    ARRAY(s->boundsFacts); ARRAY(s->scalarScratchProofs); ARRAY(i->context->symbols);
    ARRAY(i->context->canonicalTypes); ARRAY(*i->compiler);
    h = bytes(h, i->runtime, i->count * sizeof(*i->runtime));
    for (size_t n = 0; n < s->places.places.length; ++n)
        ARRAY(((const SZrParserPlace *)s->places.places.head)[n].projections);
    for (size_t n = 0; n < s->cfg.blocks.length; ++n)
        ARRAY(((const SZrParserCfgBlock *)s->cfg.blocks.head)[n].outgoingEdges);
    OBJECT(i->script); OBJECT(i->script->data.script.statements);
    h = bytes(h, i->script->data.script.statements->nodes,
            i->script->data.script.statements->count * sizeof(SZrAstNode *));
    OBJECT(i->script->data.script.statements->nodes[0]);
    for (TZrUInt32 n = 0; n < p->valueCount; ++n) OBJECT(p->astByValue[n]);
#undef ARRAY
#undef OBJECT
    return h;
}

static int large_emit(void *context, TZrUInt64 word) {
    SLargeContents *contents = context;
    if (contents->count == SPAN_WORDS) { contents->overflow = 1u; return 0; }
    contents->words[contents->count++] = word; return 1;
}
static int large_capture(const SZrExecIrModule *module, SLargeContents *contents) {
    SZrExecIrDiagnostic d;
    contents->count = 0u; contents->overflow = 0u;
    if (!module || module->functionCount != 1u || !module->functions) return 0;
    memset(&d, 0xA5, sizeof(d));
    if (!ZrCore_ExecIr_VerifyModule(module, &d) || !diagnostic_zero(&d)) return 0;
    memset(&d, 0xA5, sizeof(d));
    if (!ZrCore_ExecIr_VerifyFunction(&module->functions[0], ZR_EXEC_IR_VERIFY_ALL, &d) ||
            !diagnostic_zero(&d)) return 0;
    /* Checked 0 is used only for this genuine verified live graph. The same
     * visitor emits all fields; this independent sink rejects its own overflow. */
    return ZrTests_ExecOwner_VisitContents(module, 0, large_emit, contents) == 1 && !contents->overflow;
}
static int large_same(const SLargeContents *a, const SLargeContents *b) {
    return !a->overflow && !b->overflow && a->count == b->count &&
            memcmp(a->words, b->words, a->count * sizeof(*a->words)) == 0;
}
static int span_geometry_same(const SZrOwnerHeapEvent *a, const SZrOwnerHeapEvent *b) {
    return a->kind == b->kind && a->count == b->count && a->width == b->width && a->bytes == b->bytes;
}
static int span_malloc(const SZrOwnerHeapEvent *event, size_t width) {
    return event->kind == 1u && event->count == 1u && event->width == width && event->bytes == width;
}
static TZrBool span_call(const SInputs *i, SZrExecIrOwnedModule **destination, SZrExecIrDiagnostic *diagnostic) {
    SSpanObservation *o = &g_span[g_runSpan - 1u];
    SSourceIntegerProof proof = {0}; SZrExecIrDiagnostic d;
    SZrExecIrModule finalizedView = *i->original;
    TZrUInt64 fingerprint = 0u;
    const size_t growBytes = 2u * SOURCE_FINALIZED_INLINE_SPANS * sizeof(SSourceFinalizedSpan);
    ++o->calls; o->token = i->original->functions[0].functionToken;
    o->ownerBefore = *destination;
    finalizedView.functions = (SZrExecIrFunction *)i->finalized;
    finalizedView.functionCount = finalizedView.functionCapacity = 1u;
    source_integer_proof_init(&proof);
    o->proofReady = (unsigned)(source_integer_prove(i->semantic, i->context, o->token, &proof, &d) &&
            source_integer_check_ast(&proof, i->script, i->identity, &d) &&
            source_integer_check_pools(&proof, i->compiler, i->runtime, i->count, &d) &&
            source_integer_check_function(&proof, &i->original->functions[0], SOURCE_INTEGER_ORIGINAL, &d) &&
            source_integer_check_function(&proof, i->finalized, SOURCE_INTEGER_COMPACTED, &d));
    if (o->proofReady) fingerprint = input_fingerprint(i, &proof);
    else ++o->errors;
    o->scratchBytes = proof.scratchBytes; o->semanticValues = proof.valueCount; o->places = proof.placeCount;
    o->originalValues = i->original->functions[0].valueCount; o->finalizedValues = i->finalized->valueCount;
    int captured = large_capture(i->original, &g_spanOriginalBefore) &&
            large_capture(&finalizedView, &g_spanFinalizedBefore);
    if (!captured) ++o->errors;
    o->originalWords = g_spanOriginalBefore.count; o->finalizedWords = g_spanFinalizedBefore.count;
    o->frameComplete = (unsigned)ZrParser_ExecIr_ValidatePrimitiveSourceFrame(i->finalized, i->context,
            i->original->layouts, i->original->layoutCount, 0u, &d);
    if (!o->frameComplete || !diagnostic_zero(&d)) ++o->errors;
    if (diagnostic) memset(diagnostic, 0xA5, sizeof(*diagnostic));
    else ++o->errors;
    o->before = ZrTests_ExecOwner_Stats();
    if (g_runSpan == SPAN_FAULT) {
        o->armed = (unsigned)(g_spanBaselineReady && !o->errors &&
                ZrTests_ExecOwner_TryArm(g_span[0].growOrdinal));
        if (!o->armed) ++o->errors;
    }
    /* No observer work or assertion can consume the measured fault. */
    o->result = real_call(i, destination, diagnostic);
    ZrTests_ExecOwner_Disarm();
    o->after = ZrTests_ExecOwner_Stats(); o->ownerAfter = *destination;
    if (diagnostic) o->lower = *diagnostic;
    o->prefixSame = 1u;
    if (o->after.attempts < o->before.attempts) ++o->errors;
    else o->eventCount = o->after.attempts - o->before.attempts;
    if (!o->eventCount || o->eventCount > SPAN_EVENTS) ++o->errors;
    else for (unsigned n = 0u; n < o->eventCount; ++n) {
        SZrOwnerHeapEvent event = {0};
        if (!ZrTests_ExecOwner_TryEvent(o->before.attempts + n, &event)) { ++o->errors; continue; }
        ++o->eventQueries; o->failedCount += event.failed;
        if (g_runSpan == SPAN_HEALTHY) g_spanEvents[n] = event;
        else if (n >= g_span[0].eventCount || !span_geometry_same(&event, &g_spanEvents[n])) o->prefixSame = 0u;
        if (span_malloc(&event, growBytes)) {
            ++o->growCount; o->growOrdinal = n + 1u; o->grow = event;
            o->growFree1 = (unsigned)(!event.failed && record_frees(event.record, 1u));
            unsigned frees = 0u;
            o->growNoRecord = (unsigned)(event.failed && !ZrTests_ExecOwner_TryRecordFrees(event.record, &frees));
        }
        if (span_malloc(&event, o->scratchBytes)) {
            ++o->scratchCount; o->scratchOrdinal = n + 1u;
            o->scratchFree1 = (unsigned)(!event.failed && record_frees(event.record, 1u));
        }
    }
    o->windowReleased = (unsigned)(released(o->before.records, o->after.records) &&
            o->before.live == o->after.live);
    o->inputPreserved = (unsigned)(o->proofReady && captured && input_fingerprint(i, &proof) == fingerprint &&
            large_capture(i->original, &g_spanOriginalAfter) && large_same(&g_spanOriginalBefore, &g_spanOriginalAfter) &&
            large_capture(&finalizedView, &g_spanFinalizedAfter) && large_same(&g_spanFinalizedBefore, &g_spanFinalizedAfter));
    if (o->result && *destination) {
        SZrOwnerLiveSpan owner = {0};
        o->ownerComplete = (unsigned)(ZrTests_ExecOwner_TryLiveSpan(o->before.records, o->after.records,
                *destination, 1u, 1u, &owner) && record_frees(owner.record, 0u) &&
                large_capture(ZrCore_ExecIr_OwnedModuleView(*destination), &g_spanOriginalAfter) &&
                large_same(&g_spanFinalizedBefore, &g_spanOriginalAfter));
    }
    source_integer_proof_free(&proof);
    return o->result;
}
static int change(SMutation *m, void *where, const void *value, size_t n) {
    if (n > sizeof(m->before)) return 0;
    m->pointer = where; m->bytes = n;
    memcpy(m->before, where, n); memcpy(m->after, value, n); return 1;
}
#define CHANGE(field, type, value) do { type changed = (value); return change(m, &(field), &changed, sizeof(changed)); } while (0)
static int prepare(unsigned kind, const SInputs *i, const SSourceIntegerProof *p,
        SMutation *m, SZrSemanticIrLoanFact *auxiliary) {
    SZrSemanticIrFunction *s = (SZrSemanticIrFunction *)i->semantic;
    SZrSemanticIrInstruction *r = (SZrSemanticIrInstruction *)s->instructions.head, *add = NULL, *constant = NULL;
    SZrExecIrFunction *original = &((SZrExecIrModule *)i->original)->functions[0];
    SZrExecIrFunction *finalized = (SZrExecIrFunction *)i->finalized;
    SZrExecIrInstruction *originalAdd = NULL, *finalizedAdd = NULL;
    SZrAstNode *ast = NULL;
    for (TZrUInt32 n = 0; n < p->instructionCount; ++n) {
        if (r[n].opcode == ZR_SEMANTIC_IR_ADD) add = &r[n];
        if (!constant && r[n].opcode == ZR_SEMANTIC_IR_CONSTANT) constant = &r[n];
    }
    if (!add || !constant || !p->astByValue[add->resultValueId - 1u]) return 0;
    ast = (SZrAstNode *)p->astByValue[add->resultValueId - 1u];
    for (TZrUInt32 n = 0; n < original->instructionCount; ++n)
        if (original->instructions[n].sourceId == add->id) originalAdd = &original->instructions[n];
    for (TZrUInt32 n = 0; n < finalized->instructionCount; ++n)
        if (finalized->instructions[n].sourceId == add->id) finalizedAdd = &finalized->instructions[n];
    if (!originalAdd || !finalizedAdd) return 0;
    TZrUInt32 pool = constant->constantPoolIndex;
    if (pool >= i->count) return 0;
    SZrTypeValue *compiler = (SZrTypeValue *)i->compiler->head, *runtime = (SZrTypeValue *)i->runtime;
    switch (kind) {
        case SEM_OPCODE: CHANGE(add->opcode, EZrSemanticIrOpcode, ZR_SEMANTIC_IR_SUB);
        case SEM_OPERANDS: {
            TZrValueId *v = (TZrValueId *)s->valueOperands.head + add->operandStart;
            TZrValueId swap[2] = {v[1], v[0]}; return change(m, v, swap, sizeof(swap)); }
        case SEM_RESULT_TYPE: CHANGE(((SZrSemanticIrValue *)s->values.head)[add->resultValueId - 1u].typeId,
                TZrTypeId, 0u);
        case SEM_POOL_INDEX: CHANGE(constant->constantPoolIndex, TZrUInt32, i->count);
        case COMPILER_BITS: CHANGE(compiler[pool].value.nativeObject.nativeInt64, TZrInt64,
                compiler[pool].value.nativeObject.nativeInt64 + 1);
        case RUNTIME_BITS: CHANGE(runtime[pool].value.nativeObject.nativeInt64, TZrInt64,
                runtime[pool].value.nativeObject.nativeInt64 + 1);
        case COMPILER_TAG: CHANGE(compiler[pool].type, EZrValueType, ZR_VALUE_TYPE_BOOL);
        case RUNTIME_TAG: CHANGE(runtime[pool].type, EZrValueType, ZR_VALUE_TYPE_BOOL);
        case SEM_RANGE: CHANGE(add->sourceRange.start.offset, TZrSize, add->sourceRange.start.offset + 1u);
        case SEM_SOURCE_MAP: CHANGE(((SZrSemanticIrSourceMapEntry *)s->sourceMap.head)[add->id - 1u].sourceRange.start.offset,
                TZrSize, add->sourceRange.start.offset + 1u);
        case ARRAY_WIDTH: CHANGE(((SZrArray *)i->compiler)->elementSize, TZrSize, sizeof(SZrTypeValue) + 1u);
        case ARRAY_CAPACITY: CHANGE(s->instructions.capacity, TZrSize, s->instructions.length - 1u);
        case AUXILIARY_FACT: {
            SZrArray a = s->loanFacts;
            a.head = (TZrBytePtr)auxiliary; a.length = a.capacity = 1u; a.elementSize = sizeof(*auxiliary); a.isValid = ZR_TRUE;
            return change(m, &s->loanFacts, &a, sizeof(a)); }
        case AST_DIVISION: CHANGE(ast->data.binaryExpression.op.op, const TZrChar *, "/");
        case AST_NULL_LEFT: CHANGE(ast->data.binaryExpression.left, SZrAstNode *, ZR_NULL);
        case AST_CYCLE: CHANGE(ast->data.binaryExpression.left, SZrAstNode *, ast);
        case AST_SHARED: CHANGE(ast->data.binaryExpression.right, SZrAstNode *, ast->data.binaryExpression.left);
        case ORIGINAL_OPCODE: CHANGE(originalAdd->opcode, TZrUInt16, ZR_EXEC_IR_OPCODE_SUB);
        case ORIGINAL_OPERANDS: {
            TZrExecIrValueId *v = original->operandPool + originalAdd->operands.start;
            TZrExecIrValueId swap[2] = {v[1], v[0]}; return change(m, v, swap, sizeof(swap)); }
        case COMPACTED_OPCODE: CHANGE(finalizedAdd->opcode, TZrUInt16, ZR_EXEC_IR_OPCODE_SUB);
        case COMPACTED_OPERANDS: {
            TZrExecIrValueId *v = finalized->operandPool + finalizedAdd->operands.start;
            TZrExecIrValueId swap[2] = {v[1], v[0]}; return change(m, v, swap, sizeof(swap)); }
        case EXEC_SOURCE_MAP: CHANGE(original->sourceMaps[0].startOffset, TZrUInt32,
                original->sourceMaps[0].startOffset + 1u);
        default: return 0;
    }
}
#undef CHANGE
static void matrix(const SInputs *i, SSourceIntegerProof *proof) {
    SZrOwnerContents originalBefore, finalizedBefore, originalAfter, finalizedAfter, oldBefore, oldAfter;
    TZrUInt64 fingerprint = input_fingerprint(i, proof);
    int captured = ZrTests_ExecOwner_TryCapture(i->original, &originalBefore);
    SZrExecIrModule view = *i->original;
    view.functions = (SZrExecIrFunction *)i->finalized; view.functionCount = view.functionCapacity = 1u;
    captured = captured && ZrTests_ExecOwner_TryCapture(&view, &finalizedBefore);
    for (unsigned n = 0u; n < MATRIX_CASES; ++n) {
        SObservation *o = &g_rows[n];
        SMutation m = {0}; SZrSemanticIrLoanFact auxiliary = {0}; SZrExecIrDiagnostic d;
        SZrOwnerLiveSpan oldSpan = {0};
        SZrOwnerHeapStats before = ZrTests_ExecOwner_Stats();
        o->prepared = (unsigned)prepare(n, i, proof, &m, &auxiliary);
        if (!captured || !o->prepared || g_probeOwner) { ++o->errors; continue; }
        if (!ZrCore_ExecIr_CloneOwnedModule(i->original, &g_probeOwner, &d) || !g_probeOwner) {
            ++o->errors; continue;
        }
        SZrExecIrOwnedModule *old = g_probeOwner;
        unsigned oldLast = ZrTests_ExecOwner_Stats().records;
        int oldCaptured = ZrTests_ExecOwner_TryCapture(ZrCore_ExecIr_OwnedModuleView(old), &oldBefore);
        int oldRecorded = ZrTests_ExecOwner_TryLiveSpan(before.records, oldLast, old, 1u, 1u, &oldSpan);
        o->oldPreserved = (unsigned)(oldCaptured && oldRecorded && record_frees(oldSpan.record, 0u));
        unsigned failureFirst = oldLast;
        for (unsigned repeat = 0u; repeat < 2u; ++repeat) {
            memset(&d, 0xA5, sizeof(d)); memcpy(m.pointer, m.after, m.bytes);
            TZrBool result = real_call(i, &g_probeOwner, &d); ++o->calls;
            /* Restore before observation, retry, cleanup or any Unity assertion. */
            memcpy(m.pointer, m.before, m.bytes);
            o->restored += (unsigned)(memcmp(m.pointer, m.before, m.bytes) == 0);
            o->refused += (unsigned)!result;
            o->diagnosticClean += (unsigned)diagnostic_clean(i, &d);
            if (repeat == 0u) o->diagnostic = d;
            else o->sameDiagnostic = (unsigned)diagnostic_same(&o->diagnostic, &d);
            int same = g_probeOwner == old && oldCaptured && oldRecorded && record_frees(oldSpan.record, 0u);
            if (same) same = ZrTests_ExecOwner_TryCapture(ZrCore_ExecIr_OwnedModuleView(g_probeOwner), &oldAfter) &&
                    words_same(&oldBefore, &oldAfter);
            o->oldPreserved &= (unsigned)same;
        }
        o->released = (unsigned)released(failureFirst, ZrTests_ExecOwner_Stats().records);
        memset(&d, 0xA5, sizeof(d));
        o->retry = (unsigned)real_call(i, &g_probeOwner, &d);
        o->retry &= (unsigned)(g_probeOwner != ZR_NULL && diagnostic_zero(&d));
        o->oldReleased = (unsigned)(oldRecorded && record_frees(oldSpan.record, 1u));
        const SZrExecIrModule *healthy = ZrCore_ExecIr_OwnedModuleView(g_probeOwner);
        o->ownerComplete = (unsigned)(healthy && healthy->functionCount == 1u && healthy->constantCount == i->count &&
                healthy->functions[0].frameLayout && healthy->functions[0].valueCount == proof->valueCount &&
                ZrTests_ExecOwner_TryCapture(healthy, &oldAfter) && words_same(&finalizedBefore, &oldAfter));
        o->inputPreserved = (unsigned)(input_fingerprint(i, proof) == fingerprint &&
                ZrTests_ExecOwner_TryCapture(i->original, &originalAfter) && words_same(&originalBefore, &originalAfter) &&
                ZrTests_ExecOwner_TryCapture(&view, &finalizedAfter) && words_same(&finalizedBefore, &finalizedAfter));
        ZrCore_ExecIr_OwnedModuleFree(&g_probeOwner);
        if (!released(before.records, ZrTests_ExecOwner_Stats().records) ||
            ZrTests_ExecOwner_Stats().live != before.live || ZrTests_ExecOwner_Stats().errors) ++o->errors;
    }
}
static void direct_checks(const SInputs *i, SSourceIntegerProof *p) {
    SZrExecIrDiagnostic d; SSourceIntegerProof output = {0}, saved = {0};
    SZrSemanticIrFunction *s = (SZrSemanticIrFunction *)i->semantic;
    size_t width = s->instructions.elementSize;
    unsigned first = ZrTests_ExecOwner_Stats().records;
    source_integer_proof_init(&output); memcpy(&saved, &output, sizeof(saved));
    s->instructions.elementSize = 0u; memset(&d, 0xA5, sizeof(d));
    TZrBool result = source_integer_prove(s, i->context, p->functionToken, &output, &d);
    s->instructions.elementSize = width;
    g_emptyPreserved = (unsigned)(!result && memcmp(&output, &saved, sizeof(output)) == 0 && diagnostic_clean(i, &d));
    source_integer_proof_free(&output);
    memset(&d, 0xA5, sizeof(d));
    result = source_integer_check_function(p, i->finalized, (ESourceIntegerPhase)99, &d);
    g_phaseRefused = (unsigned)(!result && d.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT && diagnostic_clean(i, &d));
    g_directClean = (unsigned)released(first, ZrTests_ExecOwner_Stats().records);
}
TZrBool ZrTests_IntegerProof_Finalized(
        const SZrSemanticIrFunction *semantic, const SZrSemanticContext *context, const SZrAstNode *script,
        const SZrFunctionSourceCallableIdentity *identity, const SZrArray *compilerConstants,
        const SZrTypeValue *functionConstants, TZrUInt32 functionConstantCount, const SZrExecIrModule *original,
        const SZrExecIrFunction *finalized, SZrExecIrOwnedModule **destination, SZrExecIrDiagnostic *diagnostic) {
    const SInputs i = {semantic, context, script, identity, compilerConstants, functionConstants,
            functionConstantCount, original, finalized};
    SSourceIntegerProof proof = {0}; SZrExecIrDiagnostic d; SZrOwnerLiveSpan scratch = {0};
    SZrOwnerHeapStats before = ZrTests_ExecOwner_Stats();
    ++g_wrapperCalls;
    if (g_runMatrix || g_runDirect) {
        source_integer_proof_init(&proof);
        if (!source_integer_prove(semantic, context, original->functions[0].functionToken, &proof, &d) ||
            !source_integer_check_ast(&proof, script, identity, &d) ||
            !source_integer_check_pools(&proof, compilerConstants, functionConstants, functionConstantCount, &d) ||
            !ZrTests_ExecOwner_TryLiveSpan(before.records, ZrTests_ExecOwner_Stats().records,
                    proof.scratch, proof.scratchBytes, 1u, &scratch)) ++g_errors;
        else {
            if (g_runMatrix) matrix(&i, &proof);
            if (g_runDirect) direct_checks(&i, &proof);
        }
        source_integer_proof_free(&proof); source_integer_proof_free(&proof);
        g_scratchReleased = (unsigned)(released(before.records, ZrTests_ExecOwner_Stats().records) &&
                record_frees(scratch.record, 1u) && ZrTests_ExecOwner_Stats().live == before.live);
    }
    ++g_actualCalls;
    if (g_runSpan) return span_call(&i, destination, diagnostic);
    return real_call(&i, destination, diagnostic);
}
void setUp(void) {
    ZrTests_Returned_Init(); ZrTests_ExecOwner_Reset();
    memset(g_rows, 0, sizeof(g_rows));
    g_runMatrix = g_runDirect = g_wrapperCalls = g_actualCalls = g_errors = 0u;
    g_emptyPreserved = g_phaseRefused = g_scratchReleased = g_directClean = 0u;
    g_probeOwner = ZR_NULL;
    g_runSpan = g_spanBaselineReady = 0u; memset(g_span, 0, sizeof(g_span));
}
void tearDown(void) {
    SZrReturnedVmStats vm = {0}; SZrReturnedHeapStats crt[2] = {{0}, {0}};
    ZrCore_ExecIr_OwnedModuleFree(&g_probeOwner);
    ZrTests_Returned_FreeFunction(0u); ZrTests_Returned_FreeFunction(1u); ZrTests_Returned_Shutdown();
    int vmRead = ZrTests_Returned_TryVmStats(&vm);
    int a = ZrTests_Returned_TryStats(0u, &crt[0]), b = ZrTests_Returned_TryStats(1u, &crt[1]);
    SZrOwnerHeapStats owner = ZrTests_ExecOwner_Stats();
    unsigned allReleased = (unsigned)released(0u, owner.records);
    /* Print pre-recovery facts even when a prior Unity failure suppresses assertions. */
    fprintf(stderr, "INTEGER_PROOF_FINAL owner=%u errors=%u failures=%u vm=%u vmErrors=%u vmDiag=%u dropped=%u "
            "crt0=%u crt0Errors=%u crt1=%u crt1Errors=%u reads=%u released=%u\n",
            owner.live, owner.errors, owner.failures, vm.live, vm.errors, vm.diagnosticCount, vm.dropped,
            crt[0].live, crt[0].errors, crt[1].live, crt[1].errors, (unsigned)(vmRead == 1 && a == 1 && b == 1), allReleased);
    ZrTests_Returned_Close(); ZrTests_ExecOwner_Recover();
    TEST_ASSERT_EQUAL_INT(1, vmRead); TEST_ASSERT_EQUAL_INT(1, a); TEST_ASSERT_EQUAL_INT(1, b);
    TEST_ASSERT_EQUAL_UINT(0u, vm.live); TEST_ASSERT_EQUAL_UINT(0u, vm.errors);
    TEST_ASSERT_EQUAL_UINT(0u, vm.diagnosticCount); TEST_ASSERT_EQUAL_UINT(0u, vm.dropped);
    TEST_ASSERT_EQUAL_UINT(0u, owner.live); TEST_ASSERT_EQUAL_UINT(0u, owner.errors);
    TEST_ASSERT_EQUAL_UINT(0u, owner.failures);
    TEST_ASSERT_EQUAL_UINT(1u, allReleased);
    for (unsigned n = 0u; n < 2u; ++n) {
        TEST_ASSERT_EQUAL_UINT(0u, crt[n].live); TEST_ASSERT_EQUAL_UINT(0u, crt[n].errors);
        TEST_ASSERT_EQUAL_UINT(crt[n].successes, crt[n].frees);
    }
    ZrTests_Returned_AssertBalanced(); ZrTests_ExecOwner_AssertBalanced();
}
static SZrFunction *compile_subject(void) {
    SZrSourceCompileDiagnostic d; memset(&d, 0xA5, sizeof(d));
    SZrFunction *f = ZrTests_Returned_Compile(0u, g_subject, ZR_FALSE, &g_exec, &d);
    TEST_ASSERT_NOT_NULL(f); TEST_ASSERT_NOT_NULL(f->ownedExecIr); TEST_ASSERT_NULL(f->ownedAotIr);
    TEST_ASSERT_EQUAL_UINT(0u, d.stage); TEST_ASSERT_EQUAL_UINT(0u, d.result);
    TEST_ASSERT_EQUAL_UINT(0u, d.hasExecIr); TEST_ASSERT_EQUAL_UINT(0u, d.hasAotIr); TEST_ASSERT_EQUAL_UINT(0u, d.hasRange);
    TEST_ASSERT_TRUE(diagnostic_zero(&d.execIr));
#define ZERO(field) TEST_ASSERT_EQUAL_UINT64(0u, d.field)
    ZERO(aotIr.status); ZERO(aotIr.functionId); ZERO(aotIr.blockId); ZERO(aotIr.instructionId);
    ZERO(aotIr.index); ZERO(aotIr.expected); ZERO(aotIr.actual);
    ZERO(startOffset); ZERO(endOffset); ZERO(startLine); ZERO(startColumn); ZERO(endLine); ZERO(endColumn);
#undef ZERO
    return f;
}
static void test_real_source_finalizer_mutation_matrix(void) {
    g_runMatrix = 1u; (void)compile_subject();
    TEST_ASSERT_EQUAL_UINT(1u, g_wrapperCalls); TEST_ASSERT_EQUAL_UINT(1u, g_actualCalls);
    TEST_ASSERT_EQUAL_UINT(0u, g_errors); TEST_ASSERT_EQUAL_UINT(1u, g_scratchReleased);
    for (unsigned n = 0u; n < MATRIX_CASES; ++n) {
        const SObservation *o = &g_rows[n];
        TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, o->prepared, g_names[n]);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(2u, o->calls, g_names[n]);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(2u, o->refused, g_names[n]);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(2u, o->restored, g_names[n]);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(2u, o->diagnosticClean, g_names[n]);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, o->sameDiagnostic, g_names[n]);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, o->oldPreserved, g_names[n]);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, o->released, g_names[n]);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, o->retry, g_names[n]);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, o->oldReleased, g_names[n]);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, o->ownerComplete, g_names[n]);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, o->inputPreserved, g_names[n]);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(0u, o->errors, g_names[n]);
        if (n == ARRAY_WIDTH || n == ARRAY_CAPACITY)
            TEST_ASSERT_EQUAL_UINT(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, o->diagnostic.code);
        fprintf(stderr, "INTEGER_PROOF case=%s refused=2 restored=2 retry=1 old_preserved=1 released=1 code=%u\n",
                g_names[n], (unsigned)o->diagnostic.code);
    }
    ZrTests_Returned_FreeFunction(0u); ZrTests_Returned_Shutdown();
    g_runMatrix = 0u;
    (void)compile_subject(); /* A fresh genuine Global after all independent mutations. */
    TEST_ASSERT_EQUAL_UINT(2u, g_wrapperCalls); TEST_ASSERT_EQUAL_UINT(2u, g_actualCalls);
}
static void test_direct_proof_output_phase_and_scratch_lifetime(void) {
    g_runDirect = 1u; (void)compile_subject();
    TEST_ASSERT_EQUAL_UINT(1u, g_wrapperCalls); TEST_ASSERT_EQUAL_UINT(1u, g_actualCalls);
    TEST_ASSERT_EQUAL_UINT(0u, g_errors); TEST_ASSERT_EQUAL_UINT(1u, g_emptyPreserved);
    TEST_ASSERT_EQUAL_UINT(1u, g_phaseRefused); TEST_ASSERT_EQUAL_UINT(1u, g_scratchReleased);
    TEST_ASSERT_EQUAL_UINT(1u, g_directClean);
    fprintf(stderr, "INTEGER_PROOF direct empty_preserved=1 invalid_phase=1 scratch_released=1\n");
}

/* This oracle follows the fixed source's left-associated syntax, not a result
 * computed by the VM or an expected graph rebuilt by the compiler under test. */
static int span_owner_oracle(const SZrFunction *runtime, const SSpanObservation *o) {
    const SZrExecIrModule *m = ZrCore_Function_GetExecIr(runtime);
    const SZrExecIrFunction *f;
    SZrExecIrDiagnostic d = {0}; TZrExecIrValueId results[SPAN_VALUES] = {0}, sum = 0u, leaf = 0u;
    unsigned rows = 0u, leaves = 0u, adds = 0u, returns = 0u, nops = 0u;
#define REQUIRE_SPAN(condition) do { if (!(condition)) return 0; } while (0)
    REQUIRE_SPAN(runtime && runtime->ownedExecIr && !runtime->ownedAotIr && m && m->functionCount == 1u && m->functions);
    REQUIRE_SPAN(ZrCore_ExecIr_VerifyModule(m, &d) && diagnostic_zero(&d));
    f = &m->functions[0];
    REQUIRE_SPAN(ZrCore_ExecIr_VerifyFunction(f, ZR_EXEC_IR_VERIFY_ALL, &d) && diagnostic_zero(&d));
    REQUIRE_SPAN(!f->sealed && f->valueCount == SPAN_VALUES && f->values && f->blockCount == 1u && f->blocks);
    REQUIRE_SPAN(f->entryBlockId == f->blocks[0].id && !f->blocks[0].instructions.offset &&
            f->blocks[0].instructions.count == f->instructionCount && !f->blocks[0].predecessors.count &&
            !f->blocks[0].successors.count && f->blocks[0].terminatorInstructionId == f->instructionCount);
    REQUIRE_SPAN(m->constantCount == 1u && runtime->constantValueLength == m->constantCount &&
            m->constants && runtime->constantValueList && (const void *)m->constants != (const void *)runtime->constantValueList);
    TZrUInt32 type = f->values[0].typeToken;
    REQUIRE_SPAN(type && runtime->constantValueList[0].type == ZR_VALUE_TYPE_INT64 &&
            runtime->constantValueList[0].value.nativeObject.nativeInt64 == 1 &&
            m->constants[0].bits == 1u && m->constants[0].typeToken == type && !m->constants[0].flags);
    REQUIRE_SPAN(m->moduleHash && m->moduleHash == runtime->moduleSignatureHash &&
            f->signatureHash == runtime->sourceCallableIdentity.canonicalSignatureHash && f->functionToken == o->token);
    const SZrMetadataTokenRecord *module = ZR_NULL, *entry = ZR_NULL;
    for (TZrSize n = 0u; n < runtime->metadataTokenRecordLength; ++n) {
        const SZrMetadataTokenRecord *r = &runtime->metadataTokenRecords[n];
        if (ZR_METADATA_TOKEN_TABLE(r->token) == ZR_METADATA_TABLE_MODULE) { REQUIRE_SPAN(!module); module = r; }
        if (ZR_METADATA_TOKEN_TABLE(r->token) == ZR_METADATA_TABLE_MEMBER_DEF && r->reserved0 == ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY) {
            REQUIRE_SPAN(!entry); entry = r;
        }
    }
    REQUIRE_SPAN(module && entry && module->token == m->moduleToken && entry->ownerToken == module->token &&
            entry->token == f->functionToken && m->contract.targetToken == module->token &&
            f->contract.targetToken == entry->token && m->contract.moduleHash == m->moduleHash &&
            f->contract.moduleHash == m->moduleHash && f->contract.signatureHash == f->signatureHash && !f->contract.declaredEffects);
    REQUIRE_SPAN(f->sourceMapCount == f->instructionCount && f->sourceMaps);
    for (TZrUInt32 n = 0u; n < f->instructionCount; ++n) {
        const SZrExecIrInstruction *r = &f->instructions[n]; const SZrExecIrSourceMap *map = ZR_NULL;
        for (TZrUInt32 k = 0u; k < f->sourceMapCount; ++k) if (f->sourceMaps[k].instructionId == n + 1u) {
            REQUIRE_SPAN(!map); map = &f->sourceMaps[k];
        }
        REQUIRE_SPAN(map && map->sourceId && map->sourceId == r->sourceId && map->startOffset <= map->endOffset &&
                map->endOffset <= SPAN_SOURCE_BYTES && map->startLine == 1u && map->endLine == 1u &&
                map->startColumn == map->startOffset + 1u && map->endColumn == map->endOffset + 1u);
        REQUIRE_SPAN(!r->flags && !r->phiRange.count && !r->successorRange.count && !r->memoryIn.count &&
                !r->memoryOut.count && !r->effectIn && !r->effectOut && !r->deoptId && !r->bindingRow &&
                !r->matchTypeToken && r->typeToken == type);
        if (r->opcode == ZR_EXEC_IR_OPCODE_NOP) { REQUIRE_SPAN(!r->results.count && !r->operands.count && !r->layoutId); ++nops; continue; }
        if (r->opcode == ZR_EXEC_IR_OPCODE_RETURN) {
            REQUIRE_SPAN(!returns && rows == SPAN_VALUES && n + 1u == f->instructionCount &&
                    !r->results.count && r->operands.count == 1u && !r->layoutId &&
                    f->operandPool[r->operands.offset] == sum && !map->startOffset && map->endOffset == SPAN_SOURCE_BYTES);
            ++returns; continue;
        }
        REQUIRE_SPAN(!returns && rows < SPAN_VALUES && r->results.count == 1u);
        TZrExecIrValueId result = f->resultPool[r->results.offset];
        REQUIRE_SPAN(result && result <= f->valueCount);
        const SZrExecIrValue *value = &f->values[result - 1u];
        REQUIRE_SPAN(value->id == result && value->definition == n + 1u && value->typeToken == type && !value->flags &&
                value->ownership == ZR_EXEC_IR_OWNERSHIP_UNKNOWN && value->nullability == ZR_EXEC_IR_NULLABILITY_NONNULL);
        for (unsigned k = 0u; k < rows; ++k) REQUIRE_SPAN(results[k] != result);
        results[rows] = result;
        if (rows < 2u || rows % 2u == 1u) {
            REQUIRE_SPAN(r->opcode == ZR_EXEC_IR_OPCODE_CONSTANT && leaves < SPAN_LEAVES && !r->operands.count &&
                    r->layoutId == 0u && map->startOffset == 7u + 4u * leaves && map->endOffset == 8u + 4u * leaves);
            leaf = result; if (!leaves) sum = result; ++leaves;
        } else {
            REQUIRE_SPAN(r->opcode == ZR_EXEC_IR_OPCODE_ADD && r->operands.count == 2u && !r->layoutId &&
                    f->operandPool[r->operands.offset] == sum && f->operandPool[r->operands.offset + 1u] == leaf &&
                    map->startOffset == 7u && map->endOffset == 8u + 4u * (leaves - 1u));
            sum = result; ++adds;
        }
        ++rows;
    }
    REQUIRE_SPAN(rows == SPAN_VALUES && leaves == SPAN_LEAVES && adds == SPAN_LEAVES - 1u && returns == 1u && nops);
    REQUIRE_SPAN(m->layoutCount == 1u && m->layouts && m->layouts[0].typeToken == type &&
            m->layouts[0].byteSize == sizeof(TZrInt64) && m->layouts[0].layoutHash);
    const SZrExecIrFrameLayout *frame = f->frameLayout;
    REQUIRE_SPAN(frame && frame->slots && frame->slotCount == f->valueCount && frame->logicalSlotCount == f->valueCount &&
            frame->storageSlotCount == f->valueCount && frame->localCount == f->valueCount && !frame->parameterCount &&
            !frame->parameterPrefixCount && frame->layoutHash && frame->layoutHash == f->contract.layoutHash &&
            frame->returnBufferOffset == frame->frameByteSize && frame->frameByteAlign &&
            !(frame->frameByteAlign & (frame->frameByteAlign - 1u)) && !(frame->frameByteSize % frame->frameByteAlign));
    for (TZrUInt32 n = 0u; n < frame->slotCount; ++n) {
        const SZrExecIrFrameSlot *slot = &frame->slots[n];
        REQUIRE_SPAN(slot->kind == ZR_EXEC_IR_FRAME_SLOT_VALUE && slot->slotId && slot->slotId <= f->valueCount &&
                slot->typeToken == type && slot->byteSize == m->layouts[0].byteSize && slot->byteAlign == m->layouts[0].byteAlign &&
                slot->byteAlign && !(slot->byteOffset % slot->byteAlign) && slot->byteOffset <= frame->frameByteSize &&
                slot->byteSize <= frame->frameByteSize - slot->byteOffset);
        for (TZrUInt32 k = 0u; k < n; ++k) {
            const SZrExecIrFrameSlot *other = &frame->slots[k];
            REQUIRE_SPAN(other->slotId != slot->slotId && (slot->byteOffset >= other->byteOffset + other->byteSize ||
                    other->byteOffset >= slot->byteOffset + slot->byteSize));
        }
    }
    REQUIRE_SPAN(!f->gcMap && !f->gcMapCount && !f->gcRootCount && !f->memoryTokenCount && !f->phiCount &&
            !f->phiIncomingCount && !f->predecessorCount && !f->successorCount && !f->deoptStateCount &&
            !f->deoptValueCount && !f->deoptAggregateCount && !f->deoptAggregateFieldCount && !f->bindingRowCount);
    if (f->stateMap) REQUIRE_SPAN(f->stateMap->functionToken == f->functionToken && f->stateMap->signatureHash == f->signatureHash &&
            f->stateMap->generation == f->contract.generation && !f->stateMap->entryCount && !f->stateMap->valueCount &&
            !f->stateMap->rootCount && !f->stateMap->ownerStateCount);
#undef REQUIRE_SPAN
    return 1;
}
static int span_source_clear(const SZrSourceCompileDiagnostic *d) {
    return !d->stage && !d->result && !d->hasExecIr && !d->hasAotIr && !d->hasRange && diagnostic_zero(&d->execIr) &&
            !d->aotIr.status && !d->aotIr.functionId && !d->aotIr.blockId && !d->aotIr.instructionId &&
            !d->aotIr.index && !d->aotIr.expected && !d->aotIr.actual && !d->startOffset && !d->endOffset &&
            !d->startLine && !d->startColumn && !d->endLine && !d->endColumn;
}
static void span_close(SSpanObservation *o, unsigned expectedFailures) {
    ZrTests_Returned_FreeFunction(0u); ZrTests_Returned_FreeFunction(1u); ZrTests_Returned_Shutdown();
    o->closedOwner = ZrTests_ExecOwner_Stats();
    o->closedReads = (unsigned)(ZrTests_Returned_TryVmStats(&o->closedVm) &&
            ZrTests_Returned_TryStats(0u, &o->closedCrt[0]) && ZrTests_Returned_TryStats(1u, &o->closedCrt[1]));
    o->closedReleased = (unsigned)released(0u, o->closedOwner.records); o->crtReleased = 1u;
    for (unsigned kind = 0u; kind < 2u; ++kind) for (unsigned n = 0u; n < o->closedCrt[kind].successes; ++n) {
        unsigned frees = 0u;
        if (!ZrTests_Returned_TryRecordFrees(kind, n, &frees) || frees != 1u) o->crtReleased = 0u;
    }
    o->closedHealthy = (unsigned)(o->closedReads && o->closedReleased && o->crtReleased &&
            !o->closedOwner.live && !o->closedOwner.errors && o->closedOwner.failures == expectedFailures &&
            o->closedOwner.records == o->closedOwner.frees && !o->closedVm.live && !o->closedVm.errors &&
            !o->closedVm.diagnosticCount && !o->closedVm.dropped);
    for (unsigned kind = 0u; kind < 2u; ++kind) if (o->closedCrt[kind].live || o->closedCrt[kind].errors ||
            o->closedCrt[kind].failures || o->closedCrt[kind].successes != o->closedCrt[kind].frees) o->closedHealthy = 0u;
}
static void span_report(const char *mode, const SSpanObservation *o) {
    fprintf(stderr, "INTEGER_PROOF_SPAN_GROW mode=%s calls=%u ordinal=%u events=%u queries=%u matches=%u kind=%u count=%llu width=%llu bytes=%llu failed=%u failed_count=%u grow_free1=%u no_record=%u scratch_bytes=%llu scratch_matches=%u scratch_ordinal=%u scratch_free1=%u armed=%u prefix=%u input=%u owner=%u frame=%u oracle=%u result=%u returned=%u stage=%u source_result=%u has_exec=%u has_aot=%u has_range=%u code=%u token=%u block=%u instruction=%u source=%u expected_version=%u actual_version=%u expected_hash=%llu actual_hash=%llu V=%u P=%u original_values=%u finalized_values=%u original_words=%llu finalized_words=%llu before_live=%u after_live=%u window_released=%u closed_live=%u closed_errors=%u closed_failures=%u closed_records=%u closed_frees=%u released=%u vm_live=%u vm_errors=%u vm_diag=%u dropped=%u crt0_live=%u crt0_errors=%u crt0_successes=%u crt0_frees=%u crt1_live=%u crt1_errors=%u crt1_successes=%u crt1_frees=%u crt_released=%u reads=%u healthy=%u errors=%u\n",
            mode, o->calls, o->growOrdinal, o->eventCount, o->eventQueries, o->growCount, o->grow.kind,
            (unsigned long long)o->grow.count, (unsigned long long)o->grow.width, (unsigned long long)o->grow.bytes,
            o->grow.failed, o->failedCount, o->growFree1, o->growNoRecord, (unsigned long long)o->scratchBytes,
            o->scratchCount, o->scratchOrdinal, o->scratchFree1, o->armed, o->prefixSame, o->inputPreserved,
            o->ownerComplete, o->frameComplete, o->oracle, (unsigned)o->result, o->returned,
            (unsigned)o->source.stage, (unsigned)o->source.result, (unsigned)o->source.hasExecIr,
            (unsigned)o->source.hasAotIr, (unsigned)o->source.hasRange, (unsigned)o->lower.code, (unsigned)o->lower.functionToken,
            (unsigned)o->lower.blockId, (unsigned)o->lower.instructionId, (unsigned)o->lower.sourceId,
            (unsigned)o->lower.expectedVersion, (unsigned)o->lower.actualVersion, (unsigned long long)o->lower.expectedHash,
            (unsigned long long)o->lower.actualHash, o->semanticValues, o->places, o->originalValues, o->finalizedValues,
            (unsigned long long)o->originalWords, (unsigned long long)o->finalizedWords, o->before.live, o->after.live,
            o->windowReleased, o->closedOwner.live, o->closedOwner.errors, o->closedOwner.failures,
            o->closedOwner.records, o->closedOwner.frees, o->closedReleased, o->closedVm.live, o->closedVm.errors,
            o->closedVm.diagnosticCount, o->closedVm.dropped, o->closedCrt[0].live, o->closedCrt[0].errors,
            o->closedCrt[0].successes, o->closedCrt[0].frees, o->closedCrt[1].live, o->closedCrt[1].errors,
            o->closedCrt[1].successes, o->closedCrt[1].frees, o->crtReleased, o->closedReads, o->closedHealthy, o->errors);
}
static void span_assert(const SSpanObservation *o, unsigned mode) {
    TEST_ASSERT_EQUAL_UINT(1u, o->calls); TEST_ASSERT_EQUAL_UINT(1u, o->wrapperDelta); TEST_ASSERT_EQUAL_UINT(1u, o->actualDelta);
    TEST_ASSERT_EQUAL_UINT(0u, o->errors); TEST_ASSERT_EQUAL_UINT(1u, o->proofReady);
    TEST_ASSERT_EQUAL_UINT(1u, o->frameComplete); TEST_ASSERT_EQUAL_UINT(1u, o->inputPreserved);
    TEST_ASSERT_EQUAL_UINT(o->eventCount, o->eventQueries); TEST_ASSERT_EQUAL_UINT(1u, o->growCount);
    TEST_ASSERT_TRUE(o->growOrdinal > 0u && o->growOrdinal <= o->eventCount);
    TEST_ASSERT_EQUAL_UINT(1u, o->prefixSame);
    TEST_ASSERT_EQUAL_UINT(SPAN_VALUES, o->semanticValues); TEST_ASSERT_EQUAL_UINT(SPAN_VALUES, o->finalizedValues);
    TEST_ASSERT_TRUE((size_t)o->semanticValues + 41u > SOURCE_FINALIZED_INLINE_SPANS);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)o->semanticValues + 2u * o->places, o->originalValues);
    TEST_ASSERT_TRUE(o->places > 0u); TEST_ASSERT_TRUE(o->originalWords > ZR_OWNER_WORDS);
    TEST_ASSERT_TRUE(o->finalizedWords > ZR_OWNER_WORDS); TEST_ASSERT_NULL(o->ownerBefore);
    TEST_ASSERT_EQUAL_UINT(1u, o->closedHealthy); TEST_ASSERT_EQUAL_UINT(1u, o->closedReleased);
    TEST_ASSERT_EQUAL_UINT(1u, o->crtReleased);
    if (mode == SPAN_FAULT) {
        SZrExecIrDiagnostic expected = {0}; SZrSourceCompileDiagnostic source = o->source;
        expected.code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY; expected.functionToken = o->token;
        TEST_ASSERT_EQUAL_UINT(1u, o->armed); TEST_ASSERT_EQUAL_UINT(1u, o->failedCount);
        TEST_ASSERT_EQUAL_UINT(1u, o->grow.failed); TEST_ASSERT_EQUAL_UINT(1u, o->growNoRecord);
        TEST_ASSERT_EQUAL_UINT(g_span[0].growOrdinal, o->growOrdinal); TEST_ASSERT_EQUAL_UINT(o->growOrdinal, o->eventCount);
        TEST_ASSERT_TRUE(span_geometry_same(&o->grow, &g_span[0].grow));
        if (g_span[0].scratchOrdinal < g_span[0].growOrdinal) {
            TEST_ASSERT_EQUAL_UINT(1u, o->scratchCount);
            TEST_ASSERT_EQUAL_UINT(g_span[0].scratchOrdinal, o->scratchOrdinal);
            TEST_ASSERT_EQUAL_UINT(1u, o->scratchFree1);
        } else {
            TEST_ASSERT_EQUAL_UINT(0u, o->scratchCount); TEST_ASSERT_EQUAL_UINT(0u, o->scratchOrdinal);
            TEST_ASSERT_EQUAL_UINT(0u, o->scratchFree1);
        }
        TEST_ASSERT_EQUAL_UINT(o->before.failures + 1u, o->after.failures); TEST_ASSERT_EQUAL_UINT(1u, o->windowReleased);
        TEST_ASSERT_FALSE(o->result); TEST_ASSERT_EQUAL_UINT(0u, o->returned); TEST_ASSERT_NULL(o->ownerAfter);
        TEST_ASSERT_TRUE(diagnostic_same(&expected, &o->lower)); TEST_ASSERT_TRUE(diagnostic_same(&expected, &source.execIr));
        TEST_ASSERT_EQUAL_UINT(ZR_SOURCE_STAGE_EXECIR_OWNER, source.stage);
        TEST_ASSERT_EQUAL_UINT(ZR_SOURCE_RESULT_OUT_OF_MEMORY, source.result); TEST_ASSERT_EQUAL_UINT(1u, source.hasExecIr);
        source.stage = ZR_SOURCE_STAGE_NONE; source.result = ZR_SOURCE_RESULT_OK;
        source.hasExecIr = ZR_FALSE; memset(&source.execIr, 0, sizeof(source.execIr));
        TEST_ASSERT_TRUE(span_source_clear(&source));
    } else {
        TEST_ASSERT_EQUAL_UINT(1u, o->scratchCount); TEST_ASSERT_EQUAL_UINT(1u, o->scratchFree1);
        TEST_ASSERT_TRUE(o->scratchOrdinal > 0u && o->scratchOrdinal <= o->eventCount);
        TEST_ASSERT_TRUE(o->scratchOrdinal != o->growOrdinal);
        TEST_ASSERT_TRUE(o->result); TEST_ASSERT_EQUAL_UINT(1u, o->returned); TEST_ASSERT_EQUAL_UINT(1u, o->sourceOwner);
        TEST_ASSERT_EQUAL_UINT(1u, o->ownerComplete); TEST_ASSERT_EQUAL_UINT(1u, o->oracle);
        TEST_ASSERT_NOT_NULL(o->ownerAfter); TEST_ASSERT_TRUE(diagnostic_zero(&o->lower));
        TEST_ASSERT_TRUE(span_source_clear(&o->source)); TEST_ASSERT_EQUAL_UINT(0u, o->failedCount);
        TEST_ASSERT_EQUAL_UINT(0u, o->grow.failed); TEST_ASSERT_EQUAL_UINT(1u, o->growFree1);
        TEST_ASSERT_EQUAL_UINT(o->before.failures, o->after.failures);
        if (mode == SPAN_RETRY) {
            TEST_ASSERT_EQUAL_UINT(g_span[0].growOrdinal, o->growOrdinal);
            TEST_ASSERT_EQUAL_UINT(g_span[0].scratchOrdinal, o->scratchOrdinal);
            TEST_ASSERT_EQUAL_UINT64(g_span[0].scratchBytes, o->scratchBytes);
            TEST_ASSERT_EQUAL_UINT(g_span[0].eventCount, o->eventCount);
        }
    }
}
static void test_real_source_dynamic_span_growth_oom_cleanup_retry(void) {
    size_t used = 7u;
    memcpy(g_spanSource, "return ", used);
    for (unsigned n = 0u; n < SPAN_LEAVES; ++n) {
        if (n) { memcpy(g_spanSource + used, " + ", 3u); used += 3u; }
        g_spanSource[used++] = '1';
    }
    g_spanSource[used++] = ';'; g_spanSource[used] = '\0';
    TEST_ASSERT_EQUAL_UINT64(SPAN_SOURCE_BYTES, used); TEST_ASSERT_EQUAL_UINT64(used, strlen(g_spanSource));
    for (unsigned mode = SPAN_HEALTHY; mode <= SPAN_RETRY; ++mode) {
        SSpanObservation *o = &g_span[mode - 1u]; SZrSourceCompileDiagnostic d;
        unsigned wrappers = g_wrapperCalls, actual = g_actualCalls;
        g_runSpan = mode; memset(&d, 0xA5, sizeof(d));
        SZrFunction *f = ZrTests_Returned_Compile(0u, g_spanSource, ZR_FALSE, &g_exec, &d);
        g_runSpan = 0u; ZrTests_ExecOwner_Disarm(); o->source = d;
        o->wrapperDelta = g_wrapperCalls - wrappers; o->actualDelta = g_actualCalls - actual;
        o->returned = (unsigned)(f != ZR_NULL); o->sourceOwner = (unsigned)(f && f->ownedExecIr && !f->ownedAotIr);
        if (f) o->oracle = (unsigned)span_owner_oracle(f, o);
        span_close(o, mode == SPAN_FAULT ? 1u : 0u);
        span_report(mode == SPAN_HEALTHY ? "healthy" : mode == SPAN_FAULT ? "fault" : "retry", o);
        span_assert(o, mode);
        if (mode == SPAN_HEALTHY) g_spanBaselineReady = 1u;
        if (mode != SPAN_RETRY) {
            /* All pre-Recover facts, including the failed event and free counts,
             * remain in g_span. Reset only after the preceding health assertions. */
            ZrTests_Returned_Close(); ZrTests_ExecOwner_Reset(); ZrTests_Returned_Init();
        }
    }
}
#include "support/ssa_execir_builder_storage_cases.inc"
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_real_source_finalizer_mutation_matrix);
    RUN_TEST(test_direct_proof_output_phase_and_scratch_lifetime);
    RUN_TEST(test_real_source_dynamic_span_growth_oom_cleanup_retry);
    RUN_TEST(test_builder_storage_guards_preserve_outputs_without_allocation);
    return UNITY_END();
}
