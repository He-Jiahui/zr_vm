#include "exec_ir/source/exec_ir_source_integer_expression_internal.h"
#include "exec_ir/source/exec_ir_source_callable_query_internal.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_core/value.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* This witness owns one workspace, never any borrowed compiler/AST/IR row. */
struct SSourceIntegerVisit { const SZrAstNode *node; TZrValueId value; };

static void reset(SZrExecIrDiagnostic *d) { if (d) memset(d, 0, sizeof(*d)); }
static TZrBool fail(const SSourceIntegerProof *p, SZrExecIrDiagnostic *d,
        EZrExecutionDiagnosticCode code, TZrUInt32 source) {
    if (d) { d->code = code; d->functionToken = p ? p->functionToken : 0u; d->sourceId = source; }
    return ZR_FALSE;
}
static TZrBool exec_fail(const SSourceIntegerProof *p, SZrExecIrDiagnostic *d,
        EZrExecutionDiagnosticCode code, TZrUInt32 source, TZrUInt32 actual) {
    fail(p, d, code, source);
    if (d) { d->instructionId = actual; d->blockId = actual ? 1u : 0u; }
    return ZR_FALSE;
}
static TZrBool span(const void *v, size_t n, size_t width, size_t align) {
    return (TZrBool)((!n || v) && (!v || (uintptr_t)v % align == 0u) &&
            n <= SIZE_MAX / width && n * width <= UINTPTR_MAX - (uintptr_t)v);
}
static TZrBool array(const SZrArray *a, size_t width, size_t align) {
    return (TZrBool)(span(a, 1u, sizeof(*a), _Alignof(SZrArray)) && a->isValid &&
            a->elementSize == width && a->length <= a->capacity &&
            span(a->head, a->capacity, width, align));
}
static TZrBool range_valid(const SZrFileRange *r) {
    return (TZrBool)(r->start.offset <= r->end.offset && r->end.offset <= UINT32_MAX &&
            r->start.line > 0 && r->start.column > 0 && r->end.column > 0 &&
            r->end.line >= r->start.line &&
            (r->end.line != r->start.line || r->end.column >= r->start.column));
}
static TZrBool same_range(const SZrFileRange *a, const SZrFileRange *b) {
    return (TZrBool)(a->source == b->source && a->start.offset == b->start.offset &&
            a->end.offset == b->end.offset && a->start.line == b->start.line &&
            a->start.column == b->start.column && a->end.line == b->end.line &&
            a->end.column == b->end.column);
}
static TZrBool contains(const SZrFileRange *a, const SZrFileRange *b) {
    return (TZrBool)(a->source == b->source && a->start.offset <= b->start.offset &&
            a->end.offset >= b->end.offset);
}
static const SZrSemanticIrInstruction *rows(const SSourceIntegerProof *p) {
    return (const SZrSemanticIrInstruction *)p->semantic->instructions.head;
}
static const SZrSemanticIrValue *values(const SSourceIntegerProof *p) {
    return (const SZrSemanticIrValue *)p->semantic->values.head;
}
static const TZrValueId *operands(const SSourceIntegerProof *p) {
    return (const TZrValueId *)p->semantic->valueOperands.head;
}
static const SZrSemanticSymbolRecord *symbol(const SSourceIntegerProof *p) {
    const SZrSemanticSymbolRecord *s = (const SZrSemanticSymbolRecord *)p->context->symbols.head, *found = ZR_NULL;
    for (TZrSize i = 0u; i < p->context->symbols.length; ++i) {
        if (s[i].id == p->semantic->symbolId) { if (found) return ZR_NULL; found = &s[i]; }
    }
    return found;
}
static TZrBool storage(const SZrSemanticIrFunction *s, const SZrSemanticContext *c) {
#define A(field, type) if (!array(&s->field, sizeof(type), _Alignof(type))) return ZR_FALSE
    A(places.places, SZrParserPlace); A(cfg.blocks, SZrParserCfgBlock);
    A(locals, SZrSemanticIrLocal); A(values, SZrSemanticIrValue);
    A(instructions, SZrSemanticIrInstruction); A(valueOperands, TZrValueId);
    A(regions, SZrSemanticIrRegion); A(cleanupScopes, SZrSemanticIrCleanupScope);
    A(sourceMap, SZrSemanticIrSourceMapEntry); A(loanFacts, SZrSemanticIrLoanFact);
    A(escapeFacts, SZrSemanticEscapeFact); A(contiguousViewFacts, SZrSemanticContiguousViewFact);
    A(boundsFacts, SZrSemanticBoundsFact); A(scalarScratchProofs, SZrSemanticIrScalarScratchProof);
#undef A
    return (TZrBool)(s->state && s->state == c->state && s->places.state == s->state &&
            s->cfg.state == s->state &&
            array(&c->symbols, sizeof(SZrSemanticSymbolRecord), _Alignof(SZrSemanticSymbolRecord)) &&
            array(&c->canonicalTypes, sizeof(SZrCanonicalTypeNode), _Alignof(SZrCanonicalTypeNode)));
}
static TZrBool inert(const SZrSemanticIrInstruction *r) {
    return (TZrBool)(!r->scalarConversionTypeToken && !r->matchTypeId && !r->valueId &&
            !r->auxiliaryValueId && !r->symbolId && !r->accessorSymbolId && !r->constructorId &&
            r->ownershipOperation == ZR_SEMANTIC_OWNERSHIP_NONE &&
            r->targetBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID && !r->loanId && !r->regionId &&
            !r->cleanupScopeId && !r->comparisonPredicate && !r->comparisonOperandTypeId &&
            r->escape == ZR_SEMANTIC_ESCAPE_LOCAL);
}
static TZrBool geometry(SSourceIntegerProof *p) {
    const SZrSemanticIrFunction *s = p->semantic;
    const SZrParserCfgBlock *b = (const SZrParserCfgBlock *)s->cfg.blocks.head;
    const SZrSemanticIrRegion *r = (const SZrSemanticIrRegion *)s->regions.head;
    const SZrFileRange zero = {0};
    if (!s->values.length || !s->places.places.length || s->instructions.length > UINT32_MAX ||
        s->values.length > UINT32_MAX || s->places.places.length > UINT32_MAX ||
        (uint64_t)s->values.length + 2u * (uint64_t)s->places.places.length > UINT32_MAX ||
        s->valueOperands.length > UINT32_MAX ||
        s->instructions.length < 3u || s->sourceMap.length != s->instructions.length ||
        s->cfg.blocks.length != 1u || s->cfg.entryBlockId != 0u || s->cfg.exitBlockId != 0u ||
        s->regions.length != 1u || s->locals.length || s->cleanupScopes.length || s->loanFacts.length ||
        s->escapeFacts.length || s->contiguousViewFacts.length || s->boundsFacts.length ||
        s->scalarScratchProofs.length) return ZR_FALSE;
    if (b->id || b->kind != ZR_PARSER_CFG_BLOCK_ENTRY || b->statement || b->firstInstructionIndex ||
        b->instructionCount != s->instructions.length || b->successorCount || b->predecessorCount ||
        b->terminatorKind != ZR_PARSER_CFG_TERMINATOR_RETURN ||
        !array(&b->outgoingEdges, sizeof(SZrParserCfgEdge), _Alignof(SZrParserCfgEdge)) ||
        b->outgoingEdges.length || r->id != 1u || r->parentId ||
        r->escapeBound != ZR_SEMANTIC_ESCAPE_FUNCTION || !same_range(&r->sourceRange, &zero)) return ZR_FALSE;
    p->instructionCount = (TZrUInt32)s->instructions.length;
    p->valueCount = (TZrUInt32)s->values.length;
    p->placeCount = (TZrUInt32)s->places.places.length;
    return ZR_TRUE;
}
/* Checked packed slices: every alignment is a power of two from a C type. */
static TZrBool slice(size_t *total, size_t n, size_t width, size_t align, size_t *offset) {
    size_t padding = (align - *total % align) % align;
    if (padding > SIZE_MAX - *total || n > SIZE_MAX / width) return ZR_FALSE;
    *total += padding; *offset = *total;
    if (n * width > SIZE_MAX - *total) return ZR_FALSE;
    *total += n * width; return ZR_TRUE;
}
static TZrBool workspace(SSourceIntegerProof *p, SZrExecIrDiagnostic *d) {
    size_t n = 0u, a, b, c, e, f;
    if (!slice(&n, p->valueCount, sizeof(*p->astByValue), _Alignof(SZrAstNode *), &a) ||
        !slice(&n, p->placeCount, sizeof(TZrUInt32), _Alignof(TZrUInt32), &b) ||
        !slice(&n, p->instructionCount, sizeof(TZrUInt32), _Alignof(TZrUInt32), &c) ||
        !slice(&n, p->instructionCount, sizeof(TZrUInt32), _Alignof(TZrUInt32), &e) ||
        !slice(&n, p->valueCount, sizeof(*p->visits), _Alignof(struct SSourceIntegerVisit), &f))
        return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, 0u);
    p->scratch = malloc(n);
    if (!p->scratch) return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, 0u);
    memset(p->scratch, 0, n); p->scratchBytes = n;
    p->astByValue = (const SZrAstNode **)((unsigned char *)p->scratch + a);
    p->placeInstructionIds = (TZrUInt32 *)((unsigned char *)p->scratch + b);
    p->instructionIds[0] = (TZrUInt32 *)((unsigned char *)p->scratch + c);
    p->instructionIds[1] = (TZrUInt32 *)((unsigned char *)p->scratch + e);
    p->visits = (struct SSourceIntegerVisit *)((unsigned char *)p->scratch + f);
    return ZR_TRUE;
}
static TZrBool canonical_rows(SSourceIntegerProof *p, SZrExecIrDiagnostic *d) {
    const SZrSemanticIrFunction *s = p->semantic;
    const SZrSemanticIrInstruction *rs = rows(p);
    const SZrSemanticIrValue *vs = values(p);
    const SZrSemanticIrSourceMapEntry *maps = (const SZrSemanticIrSourceMapEntry *)s->sourceMap.head;
    const SZrParserPlace *ps = (const SZrParserPlace *)s->places.places.head;
    TZrUInt32 operandEnd = 0u, valueRows = 0u, bases = 0u;
    for (TZrUInt32 i = 0u; i < p->instructionCount; ++i) {
        const SZrSemanticIrInstruction *r = &rs[i];
        if (r->id != i + 1u || r->typeId != p->integerTypeId || !inert(r) ||
            !range_valid(&r->sourceRange) || maps[i].instructionId != r->id ||
            !same_range(&maps[i].sourceRange, &r->sourceRange) ||
            r->operandStart != operandEnd || r->operandStart > s->valueOperands.length ||
            r->operandCount > s->valueOperands.length - r->operandStart)
            return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->id);
        operandEnd += r->operandCount;
        for (TZrUInt32 j = 0u; j < r->operandCount; ++j) {
            TZrValueId v = operands(p)[r->operandStart + j];
            if (!v || v > p->valueCount || !vs[v - 1u].definitionInstructionId ||
                vs[v - 1u].definitionInstructionId >= r->id)
                return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, r->id);
        }
        if (r->opcode == ZR_SEMANTIC_IR_PLACE_BASE) {
            if (!r->placeId || r->placeId > p->placeCount || r->resultValueId || r->operandCount ||
                r->hasConstantPoolIndex || r->constantPoolIndex || p->placeInstructionIds[r->placeId - 1u])
                return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->id);
            p->placeInstructionIds[r->placeId - 1u] = r->id; ++bases;
        } else if (r->opcode == ZR_SEMANTIC_IR_RETURN) {
            if (i + 1u != p->instructionCount || r->resultValueId || r->operandCount != 1u ||
                r->placeId || r->hasConstantPoolIndex || r->constantPoolIndex)
                return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->id);
            p->returnInstructionId = r->id; p->returnValueId = operands(p)[r->operandStart];
        } else {
            TZrUInt32 arity;
            if (r->opcode == ZR_SEMANTIC_IR_CONSTANT) { arity = 0u; ++p->constantInstructionCount; }
            else if (r->opcode == ZR_SEMANTIC_IR_ADD || r->opcode == ZR_SEMANTIC_IR_SUB ||
                     r->opcode == ZR_SEMANTIC_IR_MUL) arity = 2u;
            else return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->id);
            if (r->placeId || r->operandCount != arity || !r->resultValueId ||
                r->resultValueId > p->valueCount ||
                (r->opcode == ZR_SEMANTIC_IR_CONSTANT ? !r->hasConstantPoolIndex :
                    (r->hasConstantPoolIndex || r->constantPoolIndex)) ||
                vs[r->resultValueId - 1u].definitionInstructionId != r->id)
                return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->id);
            ++valueRows;
        }
    }
    if (!p->returnInstructionId || !p->constantInstructionCount || valueRows != p->valueCount ||
        bases != p->placeCount || operandEnd != s->valueOperands.length)
        return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
    for (TZrUInt32 i = 0u; i < p->valueCount; ++i) {
        const SZrSemanticIrValue *v = &vs[i];
        if (v->id != i + 1u || v->typeId != p->integerTypeId || !v->definitionInstructionId ||
            v->definitionInstructionId > p->instructionCount ||
            rs[v->definitionInstructionId - 1u].resultValueId != v->id ||
            !same_range(&v->sourceRange, &rs[v->definitionInstructionId - 1u].sourceRange) ||
            v->facts.typeId != p->integerTypeId || v->facts.ownership != ZR_SEMANTIC_VALUE_OWNERSHIP_VALUE ||
            v->facts.nullability != ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL)
            return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, v->definitionInstructionId);
    }
    for (TZrUInt32 i = 0u; i < p->placeCount; ++i) {
        const SZrParserPlace *v = &ps[i];
        TZrBool valueRange = ZR_FALSE;
        if (v->id != i + 1u || v->parentId || v->typeId != p->integerTypeId ||
            v->base.kind != ZR_PARSER_PLACE_BASE_TEMPORARY || !p->placeInstructionIds[i] ||
            !array(&v->projections, sizeof(SZrParserPlaceProjection), _Alignof(SZrParserPlaceProjection)) ||
            v->projections.length || !same_range(&v->sourceRange, &rs[p->placeInstructionIds[i] - 1u].sourceRange))
            return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, p->placeInstructionIds[i]);
        /* Compaction has no AST input: establish actual SemIR provenance here,
         * without asserting one place per value or a unique reusable slot. */
        for (TZrUInt32 j = 0u; j < p->valueCount; ++j)
            if (same_range(&v->sourceRange, &vs[j].sourceRange)) valueRange = ZR_TRUE;
        if (!valueRange) return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, p->placeInstructionIds[i]);
    }
    return ZR_TRUE;
}
void source_integer_proof_init(SSourceIntegerProof *p) { if (p) memset(p, 0, sizeof(*p)); }
void source_integer_proof_free(SSourceIntegerProof *p) {
    if (p) { if (p->scratch) free(p->scratch); memset(p, 0, sizeof(*p)); }
}
TZrBool source_integer_prove(const SZrSemanticIrFunction *s, const SZrSemanticContext *c,
        TZrMetadataToken token, SSourceIntegerProof *output, SZrExecIrDiagnostic *d) {
    SSourceIntegerProof p = {0}, empty = {0}; SSourceCallableFacts facts = {0};
    const SZrSemanticSymbolRecord *sym;
    reset(d);
    if (!span(s, 1u, sizeof(*s), _Alignof(SZrSemanticIrFunction)) ||
        !span(c, 1u, sizeof(*c), _Alignof(SZrSemanticContext)) ||
        !span(output, 1u, sizeof(*output), _Alignof(SSourceIntegerProof)))
        return fail(ZR_NULL, d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    p.semantic = s; p.context = c; p.functionToken = token;
    if (memcmp(output, &empty, sizeof(empty)) || !storage(s, c))
        return fail(&p, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
    if (!source_callable_query(c, s->callableTypeId, token, &facts, d)) return ZR_FALSE;
    sym = symbol(&p);
    if (!s->symbolId || !sym || sym->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION || sym->typeId != s->callableTypeId ||
        sym->overloadSetId || !geometry(&p)) return fail(&p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
    p.callableTypeId = facts.callableTypeId; p.integerTypeId = facts.returnTypeId;
    p.signatureHash = facts.signatureHash;
    if (!workspace(&p, d)) return ZR_FALSE;
    if (!canonical_rows(&p, d)) { source_integer_proof_free(&p); return ZR_FALSE; }
    p.canonicalProven = ZR_TRUE; memcpy(output, &p, sizeof(p)); return ZR_TRUE;
}
static TZrBool node_header(const SZrAstNode *n) {
    return span(n, 1u, sizeof(*n), _Alignof(SZrAstNode));
}
TZrBool source_integer_check_ast(SSourceIntegerProof *p, const SZrAstNode *script,
        const SZrFunctionSourceCallableIdentity *id, SZrExecIrDiagnostic *d) {
    const SZrSemanticSymbolRecord *sym;
    const SZrAstNodeArray *statements; const SZrAstNode *returned;
    TZrUInt32 read = 0u, count = 1u;
    reset(d);
    if (!p || !p->canonicalProven)
        return fail(p, d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    p->astProven = p->poolsProven = ZR_FALSE; p->checkedScript = ZR_NULL; p->checkedIdentity = ZR_NULL;
    if (!script || !span(id, 1u, sizeof(*id), _Alignof(SZrFunctionSourceCallableIdentity)))
        return fail(p, d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    memset(p->astByValue, 0, (size_t)p->valueCount * sizeof(*p->astByValue));
    sym = symbol(p);
    if (!sym || sym->astNode != script || !node_header(script) || script->type != ZR_AST_SCRIPT ||
        !range_valid(&script->location) || !same_range(&sym->location, &script->location) ||
        id->schemaVersion != ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1 ||
        id->typeId != p->callableTypeId || id->symbolId != p->semantic->symbolId ||
        id->canonicalSignatureHash != p->signatureHash || !id->hasExplicitNoArgsI64 ||
        id->returnPrimitive != ZR_VALUE_TYPE_INT64 || id->parameterCount || id->receiverFlags || id->effectFlags ||
        id->declarationRange.startLine != (TZrUInt32)script->location.start.line ||
        id->declarationRange.startColumn != (TZrUInt32)script->location.start.column ||
        id->declarationRange.endLine != (TZrUInt32)script->location.end.line ||
        id->declarationRange.endColumn != (TZrUInt32)script->location.end.column)
        return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
    statements = script->data.script.statements;
    if (!span(statements, 1u, sizeof(*statements), _Alignof(SZrAstNodeArray)) ||
        statements->count > statements->capacity ||
        !span(statements->nodes, statements->capacity, sizeof(*statements->nodes), _Alignof(SZrAstNode *)) ||
        statements->count != 1u) return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
    returned = statements->nodes[0];
    if (!node_header(returned) || returned->type != ZR_AST_RETURN_STATEMENT ||
        returned->data.returnStatement.isReferenceReturn || !range_valid(&returned->location) ||
        !contains(&script->location, &returned->location) ||
        !same_range(&returned->location, &rows(p)[p->returnInstructionId - 1u].sourceRange))
        return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, p->returnInstructionId);
    p->visits[0].node = returned->data.returnStatement.expr; p->visits[0].value = p->returnValueId;
    while (read < count) {
        struct SSourceIntegerVisit visit = p->visits[read++];
        const SZrAstNode *n = visit.node;
        const SZrSemanticIrInstruction *r = &rows(p)[values(p)[visit.value - 1u].definitionInstructionId - 1u];
        if (!node_header(n) || n == script || n == returned || p->astByValue[visit.value - 1u] ||
            !range_valid(&n->location) || !contains(&returned->location, &n->location) ||
            !same_range(&n->location, &r->sourceRange)) return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->id);
        for (TZrUInt32 i = 0u; i < p->valueCount; ++i)
            if (p->astByValue[i] == n) return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->id);
        p->astByValue[visit.value - 1u] = n;
        if (r->opcode == ZR_SEMANTIC_IR_CONSTANT) {
            if (n->type != ZR_AST_INTEGER_LITERAL) return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->id);
        } else {
            char op;
            if (n->type != ZR_AST_BINARY_EXPRESSION ||
                !span(n->data.binaryExpression.op.op, 2u, sizeof(TZrChar), _Alignof(TZrChar)) ||
                n->data.binaryExpression.op.op[1] != '\0') return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->id);
            op = n->data.binaryExpression.op.op[0];
            if (!((op == '+' && r->opcode == ZR_SEMANTIC_IR_ADD) ||
                  (op == '-' && r->opcode == ZR_SEMANTIC_IR_SUB) ||
                  (op == '*' && r->opcode == ZR_SEMANTIC_IR_MUL)) || count > p->valueCount ||
                p->valueCount - count < 2u) return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->id);
            p->visits[count].node = n->data.binaryExpression.left;
            p->visits[count++].value = operands(p)[r->operandStart];
            p->visits[count].node = n->data.binaryExpression.right;
            p->visits[count++].value = operands(p)[r->operandStart + 1u];
            if (!node_header(n->data.binaryExpression.left) || !node_header(n->data.binaryExpression.right) ||
                !contains(&n->location, &n->data.binaryExpression.left->location) ||
                !contains(&n->location, &n->data.binaryExpression.right->location))
                return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->id);
        }
    }
    if (count != p->valueCount) return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
    for (TZrUInt32 i = 0u; i < p->valueCount; ++i)
        if (!p->astByValue[i]) return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
    for (TZrUInt32 i = 0u; i < p->placeCount; ++i) {
        TZrBool matched = ZR_FALSE;
        const SZrFileRange *r = &rows(p)[p->placeInstructionIds[i] - 1u].sourceRange;
        for (TZrUInt32 j = 0u; j < p->valueCount; ++j)
            if (same_range(r, &p->astByValue[j]->location)) matched = ZR_TRUE;
        if (!matched) return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, p->placeInstructionIds[i]);
    }
    p->checkedScript = script; p->checkedIdentity = id; p->astProven = ZR_TRUE; return ZR_TRUE;
}
TZrBool source_integer_check_pools(SSourceIntegerProof *p, const SZrArray *compiler,
        const SZrTypeValue *runtime, TZrUInt32 count, SZrExecIrDiagnostic *d) {
    const SZrTypeValue *a;
    reset(d);
    if (!p || !p->canonicalProven)
        return fail(p, d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    p->poolsProven = ZR_FALSE; p->compilerConstants = ZR_NULL; p->functionConstants = ZR_NULL;
    p->functionConstantCount = 0u;
    if (!array(compiler, sizeof(SZrTypeValue), _Alignof(SZrTypeValue)) || compiler->length != count ||
        !count || !span(runtime, count, sizeof(*runtime), _Alignof(SZrTypeValue)))
        return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
    a = (const SZrTypeValue *)compiler->head;
    for (TZrUInt32 i = 0u; i < count; ++i) {
        TZrBool referenced = ZR_FALSE;
        if (a[i].type != ZR_VALUE_TYPE_INT64 || runtime[i].type != ZR_VALUE_TYPE_INT64 ||
            a[i].value.nativeObject.nativeInt64 != runtime[i].value.nativeObject.nativeInt64)
            return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
        for (TZrUInt32 j = 0u; j < p->instructionCount; ++j)
            if (rows(p)[j].opcode == ZR_SEMANTIC_IR_CONSTANT && rows(p)[j].constantPoolIndex == i) referenced = ZR_TRUE;
        if (!referenced) return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
    }
    for (TZrUInt32 i = 0u; i < p->instructionCount; ++i) {
        const SZrSemanticIrInstruction *r = &rows(p)[i];
        if (r->opcode != ZR_SEMANTIC_IR_CONSTANT) continue;
        if (r->constantPoolIndex >= count) return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, r->id);
        if (p->astProven && (!p->astByValue[r->resultValueId - 1u] ||
            p->astByValue[r->resultValueId - 1u]->type != ZR_AST_INTEGER_LITERAL ||
            p->astByValue[r->resultValueId - 1u]->data.integerLiteral.value !=
                    a[r->constantPoolIndex].value.nativeObject.nativeInt64))
            return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, r->id);
    }
    p->compilerConstants = compiler; p->functionConstants = runtime; p->functionConstantCount = count;
    p->poolsProven = ZR_TRUE; return ZR_TRUE;
}
TZrBool source_integer_constant_at(const SSourceIntegerProof *p, TZrUInt32 index,
        SZrExecIrConstant *output, SZrExecIrDiagnostic *d) {
    SZrExecIrConstant candidate = {0}; const SZrTypeValue *a, *b;
    reset(d);
    if (!p || !output || !p->poolsProven)
        return fail(p, d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    if (!array(p->compilerConstants, sizeof(SZrTypeValue), _Alignof(SZrTypeValue)) ||
        p->compilerConstants->length != p->functionConstantCount || index >= p->functionConstantCount ||
        !span(p->functionConstants, p->functionConstantCount, sizeof(*b), _Alignof(SZrTypeValue)))
        return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
    a = &((const SZrTypeValue *)p->compilerConstants->head)[index]; b = &p->functionConstants[index];
    if (a->type != ZR_VALUE_TYPE_INT64 || b->type != ZR_VALUE_TYPE_INT64 ||
        a->value.nativeObject.nativeInt64 != b->value.nativeObject.nativeInt64)
        return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 0u);
    candidate.typeToken = p->integerTypeId; candidate.bits = (TZrUInt64)a->value.nativeObject.nativeInt64;
    memcpy(output, &candidate, sizeof(candidate)); return ZR_TRUE;
}
TZrBool source_integer_pure_instruction(const SZrExecIrInstruction *r) {
    return source_integer_pure_fields(r);
}
static TZrBool exec_storage(const SZrExecIrFunction *f) {
#define A(field, n, cap, type) if (f->n > f->cap || !span(f->field, f->cap, sizeof(type), \
        _Alignof(type))) return ZR_FALSE
    A(values, valueCount, valueCapacity, SZrExecIrValue);
    A(instructions, instructionCount, instructionCapacity, SZrExecIrInstruction);
    A(blocks, blockCount, blockCapacity, SZrExecIrBlock);
    A(operandPool, operandCount, operandCapacity, TZrExecIrValueId);
    A(resultPool, resultCount, resultCapacity, TZrExecIrValueId);
    A(sourceMaps, sourceMapCount, sourceMapCapacity, SZrExecIrSourceMap);
    A(memoryTokenPool, memoryTokenCount, memoryTokenCapacity, TZrExecIrMemoryTokenId);
    A(phiPool, phiCount, phiCapacity, SZrExecIrPhi);
    A(phiIncoming, phiIncomingCount, phiIncomingCapacity, SZrExecIrPhiIncoming);
    A(predecessors, predecessorCount, predecessorCapacity, TZrExecIrBlockId);
    A(successors, successorCount, successorCapacity, TZrExecIrBlockId);
    A(gcMap, gcMapCount, gcMapCapacity, SZrExecIrGcMap);
    A(gcRoots, gcRootCount, gcRootCapacity, TZrExecIrValueId);
    A(deoptStates, deoptStateCount, deoptStateCapacity, SZrExecIrDeoptState);
    A(deoptValues, deoptValueCount, deoptValueCapacity, TZrExecIrValueId);
    A(deoptAggregates, deoptAggregateCount, deoptAggregateCapacity, SZrExecIrDeoptAggregate);
    A(deoptAggregateFields, deoptAggregateFieldCount, deoptAggregateFieldCapacity, SZrExecIrDeoptAggregateField);
    A(bindingRows, bindingRowCount, bindingRowCapacity, SZrExecIrBindingRow);
#undef A
    if (f->frameLayout) {
        const SZrExecIrFrameLayout *r = f->frameLayout;
        if (!span(r, 1u, sizeof(*r), _Alignof(SZrExecIrFrameLayout)) || r->slotCount > r->slotCapacity ||
            !span(r->slots, r->slotCapacity, sizeof(*r->slots), _Alignof(SZrExecIrFrameSlot))) return ZR_FALSE;
    }
    /* GC/state side objects are optional, but an allocated empty table still
     * needs real typed storage and its actual stable function identity. */
    if (f->gcMap) {
        const SZrExecIrGcMap *g = f->gcMap;
        if (!span(g, 1u, sizeof(*g), _Alignof(SZrExecIrGcMap)) ||
            g->entryCount > g->entryCapacity || g->slotIndexCount > g->slotIndexCapacity ||
            g->inlineRefOffsetCount > g->inlineRefOffsetCapacity ||
            !span(g->entries, g->entryCapacity, sizeof(*g->entries), _Alignof(SZrExecIrGcMapEntry)) ||
            !span(g->slotIndexPool, g->slotIndexCapacity, sizeof(*g->slotIndexPool), _Alignof(TZrUInt32)) ||
            !span(g->inlineRefOffsetPool, g->inlineRefOffsetCapacity, sizeof(*g->inlineRefOffsetPool), _Alignof(TZrUInt32)) ||
            g->entryCount || g->slotIndexCount || g->inlineRefOffsetCount || g->safepointId ||
            g->sourceId || g->rootRange.count) return ZR_FALSE;
    }
    if (f->stateMap) {
        const SZrExecIrStateMap *s = f->stateMap;
        if (!span(s, 1u, sizeof(*s), _Alignof(SZrExecIrStateMap)) ||
            s->entryCount > s->entryCapacity || s->valueCount > s->valueCapacity ||
            s->rootCount > s->rootCapacity || s->ownerStateCount > s->ownerStateCapacity ||
            !span(s->entries, s->entryCapacity, sizeof(*s->entries), _Alignof(SZrExecIrStateMapEntry)) ||
            !span(s->valuePool, s->valueCapacity, sizeof(*s->valuePool), _Alignof(TZrExecIrValueId)) ||
            !span(s->rootPool, s->rootCapacity, sizeof(*s->rootPool), _Alignof(TZrExecIrValueId)) ||
            !span(s->ownerStatePool, s->ownerStateCapacity, sizeof(*s->ownerStatePool), _Alignof(TZrUInt32)) ||
            s->entryCount || s->valueCount || s->rootCount || s->ownerStateCount ||
            s->functionToken != f->functionToken || s->signatureHash != f->signatureHash ||
            s->generation != f->contract.generation) return ZR_FALSE;
    }
    return ZR_TRUE;
}
static TZrBool map_matches(const SZrExecIrSourceMap *map, const SZrSemanticIrInstruction *r,
        TZrUInt32 actual) {
    const SZrFileRange *q = &r->sourceRange;
    return (TZrBool)(map->instructionId == actual && map->sourceId == r->id &&
            map->startOffset == q->start.offset && map->endOffset == q->end.offset &&
            map->startLine == (TZrUInt32)q->start.line && map->endLine == (TZrUInt32)q->end.line &&
            map->startColumn == (TZrUInt32)q->start.column && map->endColumn == (TZrUInt32)q->end.column);
}
static TZrUInt16 opcode(EZrSemanticIrOpcode op) {
    switch (op) {
        case ZR_SEMANTIC_IR_CONSTANT: return ZR_EXEC_IR_OPCODE_CONSTANT;
        case ZR_SEMANTIC_IR_ADD: return ZR_EXEC_IR_OPCODE_ADD;
        case ZR_SEMANTIC_IR_SUB: return ZR_EXEC_IR_OPCODE_SUB;
        case ZR_SEMANTIC_IR_MUL: return ZR_EXEC_IR_OPCODE_MUL;
        case ZR_SEMANTIC_IR_RETURN: return ZR_EXEC_IR_OPCODE_RETURN;
        default: return ZR_EXEC_IR_OPCODE_NOP;
    }
}
TZrBool source_integer_check_function(SSourceIntegerProof *p, const SZrExecIrFunction *f,
        ESourceIntegerPhase phase, SZrExecIrDiagnostic *d) {
    TZrUInt32 *map;
    reset(d);
    if (!p || !p->canonicalProven)
        return fail(p, d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    if (phase != SOURCE_INTEGER_ORIGINAL && phase != SOURCE_INTEGER_COMPACTED) {
        p->checkedFunctions[0] = p->checkedFunctions[1] = ZR_NULL;
        return fail(p, d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    }
    p->checkedFunctions[phase] = ZR_NULL; map = p->instructionIds[phase];
    memset(map, 0, (size_t)p->instructionCount * sizeof(*map));
    if (!span(f, 1u, sizeof(*f), _Alignof(SZrExecIrFunction)))
        return fail(p, d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    if (!exec_storage(f) || f->instructionCount != p->instructionCount ||
        f->valueCount != p->valueCount + (phase == SOURCE_INTEGER_ORIGINAL ? 2u * p->placeCount : 0u) ||
        f->blockCount != 1u || f->entryBlockId != 1u || f->sourceMapCount != p->instructionCount ||
        f->functionToken != p->functionToken || f->signatureHash != p->signatureHash ||
        f->contract.targetToken != f->functionToken || f->contract.signatureHash != f->signatureHash ||
        f->contract.schemaVersion != ZR_EXECUTION_CONTRACT_SCHEMA_VERSION ||
        f->contract.abiVersion != ZR_EXECUTION_CONTRACT_ABI_VERSION ||
        f->contract.logicalVersion != ZR_EXECUTION_CONTRACT_LOGICAL_VERSION ||
        f->contract.reserved0 || f->contract.reserved1 || !f->contract.generation ||
        f->contract.requiredCapabilities || f->contract.declaredEffects ||
        f->bindingRowsSchemaVersion != ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY ||
        f->predecessorCount || f->successorCount || f->phiCount || f->phiIncomingCount || f->memoryTokenCount ||
        f->gcMapCount || f->gcRootCount || f->deoptStateCount || f->deoptValueCount ||
        f->deoptAggregateCount || f->deoptAggregateFieldCount || f->bindingRowCount ||
        (phase == SOURCE_INTEGER_ORIGINAL && (f->frameLayout || f->contract.layoutHash)))
        return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
    const SZrExecIrBlock *block = &f->blocks[0];
    if (block->id != 1u || block->flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY || block->instructions.start ||
        block->instructions.count != p->instructionCount || block->predecessors.count ||
        block->successors.count || block->phis.count || block->effectPhiResult || block->effectPhiIncomings.count)
        return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
    for (TZrUInt32 j = 0u; j < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++j)
        if (block->memoryPhiResults[j] || block->memoryPhiIncomings[j].count)
            return fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
    for (TZrUInt32 i = 0u; i < f->instructionCount; ++i) {
        const SZrExecIrInstruction *r = &f->instructions[i]; const SZrExecIrSourceMap *m = ZR_NULL;
        if (!r->sourceId || r->sourceId > p->instructionCount || map[r->sourceId - 1u])
            return exec_fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->sourceId, i + 1u);
        map[r->sourceId - 1u] = i + 1u;
        for (TZrUInt32 j = 0u; j < f->sourceMapCount; ++j)
            if (f->sourceMaps[j].instructionId == i + 1u) {
                if (m) return exec_fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->sourceId, i + 1u);
                m = &f->sourceMaps[j];
            }
        if (!m || !map_matches(m, &rows(p)[r->sourceId - 1u], i + 1u) ||
            r->typeToken != p->integerTypeId || r->operands.start > f->operandCount ||
            r->operands.count > f->operandCount - r->operands.start || r->results.start > f->resultCount ||
            r->results.count > f->resultCount - r->results.start)
            return exec_fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->sourceId, i + 1u);
    }
    if (block->terminatorInstructionId != map[p->returnInstructionId - 1u])
        return exec_fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, p->returnInstructionId,
                map[p->returnInstructionId - 1u]);
    for (TZrUInt32 i = 0u; i < p->instructionCount; ++i) {
        const SZrSemanticIrInstruction *s = &rows(p)[i];
        const SZrExecIrInstruction *r = &f->instructions[map[i] - 1u];
        ESourceIntegerPhase other = phase == SOURCE_INTEGER_ORIGINAL ? SOURCE_INTEGER_COMPACTED : SOURCE_INTEGER_ORIGINAL;
        if (p->checkedFunctions[other] && p->instructionIds[other][i] != map[i])
            return exec_fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, s->id, map[i]);
        if (s->opcode == ZR_SEMANTIC_IR_PLACE_BASE && phase == SOURCE_INTEGER_ORIGINAL) {
            SZrExecIrInstruction pure = *r; pure.opcode = ZR_EXEC_IR_OPCODE_NOP;
            if (r->opcode != ZR_EXEC_IR_OPCODE_PLACE_BASE || !source_integer_pure_fields(&pure) ||
                r->operands.count != 1u || r->results.count != 1u ||
                f->operandPool[r->operands.start] != p->valueCount + p->placeCount + s->placeId ||
                f->resultPool[r->results.start] != p->valueCount + s->placeId)
                return exec_fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, s->id, map[i]);
        } else {
            TZrBool base = (TZrBool)(s->opcode == ZR_SEMANTIC_IR_PLACE_BASE);
            if (!source_integer_pure_instruction(r) || r->opcode != opcode(s->opcode) ||
                r->operands.count != s->operandCount || r->results.count != (s->resultValueId ? 1u : 0u) ||
                (s->opcode == ZR_SEMANTIC_IR_CONSTANT && r->layoutId != s->constantPoolIndex) ||
                (base && (r->operands.count || r->results.count)))
                return exec_fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, s->id, map[i]);
            for (TZrUInt32 j = 0u; j < s->operandCount; ++j)
                if (f->operandPool[r->operands.start + j] != operands(p)[s->operandStart + j] ||
                    map[values(p)[operands(p)[s->operandStart + j] - 1u].definitionInstructionId - 1u] >= map[i])
                    return exec_fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, s->id, map[i]);
            if (s->resultValueId && f->resultPool[r->results.start] != s->resultValueId)
                return exec_fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, s->id, map[i]);
        }
    }
    for (TZrUInt32 i = 0u; i < f->valueCount; ++i) {
        const SZrExecIrValue *v = &f->values[i]; TZrUInt32 definition, flags = 0u;
        EZrExecIrNullability nullable = ZR_EXEC_IR_NULLABILITY_NONNULL;
        if (i < p->valueCount) definition = map[values(p)[i].definitionInstructionId - 1u];
        else if (i < p->valueCount + p->placeCount) {
            definition = map[p->placeInstructionIds[i - p->valueCount] - 1u];
            flags = ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS; nullable = ZR_EXEC_IR_NULLABILITY_UNKNOWN;
        } else { definition = 0u; flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY; nullable = ZR_EXEC_IR_NULLABILITY_UNKNOWN; }
        if (v->id != i + 1u || v->definition != definition || v->typeToken != p->integerTypeId ||
            v->flags != flags || v->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN || v->nullability != nullable)
        return exec_fail(p, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                i < p->valueCount ? values(p)[i].definitionInstructionId :
                (i < p->valueCount + p->placeCount ? p->placeInstructionIds[i - p->valueCount] : 0u), definition);
    }
    /* Every extra live operand is excluded above by exact per-row relationships:
     * each base consumes its own provenance once, and no address is consumed. */
    p->checkedFunctions[phase] = f; return ZR_TRUE;
}
