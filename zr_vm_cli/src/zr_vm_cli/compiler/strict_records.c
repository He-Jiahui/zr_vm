#include "compiler/strict_records.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_parser/parser.h"

#define INVALID_SLOT UINT32_MAX

typedef struct RecordModel {
    const char *fields[ZR_STRICT_MAX_FIELDS];
    uint32_t publicMask;
    const SZrAstNode *defaults[ZR_STRICT_MAX_FIELDS];
    const SZrAstNodeArray *params;
    const SZrAstNode *constructor;
    TZrBool resource;
} RecordModel;

typedef struct Binding {
    const char *name;
    uint32_t slot;
    uint32_t depth;
} Binding;

typedef struct Lowerer {
    ZrStrictProgram program;
    RecordModel *models;
    Binding *bindings;
    SZrFileRange *locations;
    uint8_t live[ZR_STRICT_MAX_SLOTS];
    uint32_t depth, bindingCount, capacity;
    const char *path;
    char *error;
    size_t errorSize;
} Lowerer;

static uint32_t refuse(Lowerer *l, const SZrAstNode *node, const char *message) {
    if (!l->error[0]) {
        snprintf(l->error, l->errorSize, "%s:%d:%d: StrictNoGC: %s", l->path,
                 node ? node->location.start.line : 1, node ? node->location.start.column : 1, message);
    }
    return INVALID_SLOT;
}

static const char *name_of(const SZrIdentifier *id) {
    return id && id->name ? ZrCore_String_GetNativeString(id->name) : NULL;
}

static const char *identifier(const SZrAstNode *node) {
    if (!node) return NULL;
    if (node->type == ZR_AST_PRIMARY_EXPRESSION &&
        (!node->data.primaryExpression.members || !node->data.primaryExpression.members->count)) {
        node = node->data.primaryExpression.property;
    }
    return node && node->type == ZR_AST_IDENTIFIER_LITERAL ? name_of(&node->data.identifier) : NULL;
}

static size_t count(const SZrAstNodeArray *a) { return a ? a->count : 0; }
static TZrBool same(const char *a, const char *b) { return a && b && !strcmp(a, b); }

static uint32_t layout_id(const Lowerer *l, const char *name) {
    for (uint32_t index = 0; index < l->program.layoutCount; ++index)
        if (same(name, l->program.layouts[index].name)) return index + 1;
    return 0;
}

static uint32_t field_id(const RecordModel *model, uint32_t fields, const char *name) {
    for (uint32_t field = 0; field < fields; ++field) if (same(name, model->fields[field])) return field;
    return INVALID_SLOT;
}

static uint8_t scalar_type(const SZrType *type) {
    const char *name;
    if (!type || type->dimensions || type->subType || type->referenceAccess || type->ownershipQualifier ||
        type->isReadonlyView || type->isDecoratorPseudoType) return 0;
    name = identifier(type->name);
    if (same(name, "int") || same(name, "int64")) return ZR_STRICT_VALUE_I64;
    if (same(name, "bool")) return ZR_STRICT_VALUE_BOOL;
    return 0;
}

static uint32_t new_slot(Lowerer *l, uint8_t kind, uint32_t layout, const SZrAstNode *node) {
    uint32_t result = l->program.slotCount;
    if (result == ZR_STRICT_MAX_SLOTS) return refuse(l, node, "too many typed slots");
    l->program.slotCount++;
    l->program.kinds[result] = kind;
    l->program.slotLayouts[result] = layout;
    return result;
}

static TZrBool emit(Lowerer *l, const SZrAstNode *node, uint32_t op, uint32_t dst, uint32_t lhs,
                    uint64_t immediate) {
    uint32_t ix = l->program.instructionCount;
    if (ix == l->capacity) {
        uint32_t next = l->capacity ? l->capacity * 2 : 64;
        ZrStrictInstruction *instructions;
        SZrFileRange *locations;
        if (next > ZR_STRICT_MAX_INSTRUCTIONS) goto oom;
        instructions = realloc(l->program.instructions, next * sizeof(*instructions));
        if (!instructions) goto oom;
        l->program.instructions = instructions;
        locations = realloc(l->locations, next * sizeof(*locations));
        if (!locations) goto oom;
        l->locations = locations;
        l->capacity = next;
    }
    l->program.instructions[ix] = (ZrStrictInstruction){op, dst, lhs, 0, immediate};
    l->locations[ix] = node->location;
    l->program.instructionCount++;
    if (op == ZR_STRICT_OP_TRANSFER || op == ZR_STRICT_OP_SHARE || op == ZR_STRICT_OP_DROP ||
        op == ZR_STRICT_OP_END_BORROW) l->live[lhs] = 0;
    if (dst < l->program.slotCount && l->program.kinds[dst] >= ZR_STRICT_VALUE_RECORD &&
        op != ZR_STRICT_OP_DROP && op != ZR_STRICT_OP_END_BORROW && op != ZR_STRICT_OP_FIELD_WRITE &&
        op != ZR_STRICT_OP_RETURN && op != ZR_STRICT_OP_HALT) l->live[dst] = 1;
    return ZR_TRUE;
oom:
    (void)refuse(l, node, "could not grow strict instruction storage");
    return ZR_FALSE;
}

static uint32_t lookup(Lowerer *l, const char *name) {
    for (uint32_t index = l->bindingCount; index > 0; --index)
        if (same(name, l->bindings[index - 1].name)) return l->bindings[index - 1].slot;
    return INVALID_SLOT;
}

static TZrBool bind(Lowerer *l, const SZrAstNode *node, const char *name, uint32_t slot) {
    if (name && l->bindingCount < ZR_STRICT_MAX_SLOTS) {
        uint32_t index = l->bindingCount;
        while (index && l->bindings[index - 1].depth == l->depth) {
            if (same(name, l->bindings[--index].name)) {
                (void)refuse(l, node, "duplicate strict binding");
                return ZR_FALSE;
            }
        }
        l->bindings[l->bindingCount++] = (Binding){name, slot, l->depth};
        return ZR_TRUE;
    }
    (void)refuse(l, node, "invalid or duplicate strict binding");
    return ZR_FALSE;
}

static uint32_t literal(Lowerer *l, const SZrAstNode *node) {
    uint8_t kind = ZR_STRICT_VALUE_I64;
    uint64_t bits = 0;
    uint32_t slot;
    if (node->type == ZR_AST_INTEGER_LITERAL) bits = (uint64_t)node->data.integerLiteral.value;
    else if (node->type == ZR_AST_BOOLEAN_LITERAL) {
        kind = ZR_STRICT_VALUE_BOOL;
        bits = !!node->data.booleanLiteral.value;
    } else return INVALID_SLOT;
    slot = new_slot(l, kind, 0, node);
    return slot != INVALID_SLOT && emit(l, node, ZR_STRICT_OP_CONST, slot, 0, bits) ? slot : INVALID_SLOT;
}

static uint32_t lower_expression(Lowerer *l, const SZrAstNode *node, uint8_t expected);

static TZrBool member_expression(const SZrAstNode *node, const char **base, const char **field) {
    const SZrAstNode *part;
    if (!node || node->type != ZR_AST_PRIMARY_EXPRESSION || count(node->data.primaryExpression.members) != 1)
        return ZR_FALSE;
    part = node->data.primaryExpression.members->nodes[0];
    if (!part || part->type != ZR_AST_MEMBER_EXPRESSION || part->data.memberExpression.computed ||
        part->data.memberExpression.accessMode != ZR_POSTFIX_ACCESS_DIRECT) return ZR_FALSE;
    *base = identifier(node->data.primaryExpression.property);
    *field = identifier(part->data.memberExpression.property);
    return *base && *field;
}

static uint32_t lower_construct(Lowerer *l, const SZrAstNode *node, const char *name,
                                const SZrAstNodeArray *args, TZrBool owned) {
    uint32_t type = layout_id(l, name), output, first, fields[ZR_STRICT_MAX_FIELDS];
    const RecordModel *model;
    const ZrStrictRecordLayout *layout;
    if (!type) return refuse(l, node, "unknown strict record layout");
    model = &l->models[type - 1];
    layout = &l->program.layouts[type - 1];
    if ((model->resource && !owned) || count(args) != count(model->params))
        return refuse(l, node, "construction requires matching positional arguments and resource ownership");
    /* Resolve arguments before entering the constructor's lexical environment. */
    uint32_t values[ZR_STRICT_MAX_FIELDS];
    for (uint32_t index = 0; index < count(args); ++index) {
        values[index] = lower_expression(l, args->nodes[index], 0);
        if (values[index] == INVALID_SLOT) return INVALID_SLOT;
    }
    for (uint32_t index = 0; index < count(args); ++index) {
        const SZrParameter *parameter = &model->params->nodes[index]->data.parameter;
        if (scalar_type(parameter->typeInfo) != l->program.kinds[values[index]]) goto fail;
    }
    for (uint32_t field = 0; field < layout->fieldCount; ++field) {
        fields[field] = model->defaults[field] ? literal(l, model->defaults[field]) : INVALID_SLOT;
        if (model->defaults[field] && fields[field] == INVALID_SLOT) goto fail;
    }
    if (model->constructor) {
        const SZrAstNodeArray *body = model->constructor->data.block.body;
        for (size_t index = 0; index < count(body); ++index) {
            const SZrAstNode *stmt = body->nodes[index];
            const SZrAstNode *assign = stmt->type == ZR_AST_EXPRESSION_STATEMENT ? stmt->data.expressionStatement.expr : stmt;
            const char *base, *member, *param;
            uint32_t field, value = INVALID_SLOT;
            if (!assign || assign->type != ZR_AST_ASSIGNMENT_EXPRESSION || !same(assign->data.assignmentExpression.op.op, "=") ||
                !member_expression(assign->data.assignmentExpression.left, &base, &member) || !same(base, "this")) goto fail;
            field = field_id(model, layout->fieldCount, member);
            if (field == INVALID_SLOT || (fields[field] != INVALID_SLOT && (layout->readonlyMask & (1u << field)))) goto fail;
            param = identifier(assign->data.assignmentExpression.right);
            if (param) {
                for (uint32_t arg = 0; arg < count(model->params); ++arg) {
                    if (same(param, name_of(model->params->nodes[arg]->data.parameter.name))) value = values[arg];
                }
            } else {
                value = literal(l, assign->data.assignmentExpression.right);
            }
            if (value == INVALID_SLOT || l->program.kinds[value] != layout->fields[field]) goto fail;
            fields[field] = value;
        }
    }
    first = l->program.slotCount;
    for (uint32_t field = 0; field < layout->fieldCount; ++field) {
        uint32_t slot;
        if (fields[field] == INVALID_SLOT || l->program.kinds[fields[field]] != layout->fields[field]) goto fail;
        slot = new_slot(l, layout->fields[field], 0, node);
        if (slot == INVALID_SLOT || !emit(l, node, ZR_STRICT_OP_COPY, slot, fields[field], 0)) goto fail;
    }
    output = new_slot(l, owned ? ZR_STRICT_VALUE_UNIQUE : ZR_STRICT_VALUE_RECORD, type, node);
    if (output == INVALID_SLOT || !emit(l, node, owned ? ZR_STRICT_OP_OWN_RECORD : ZR_STRICT_OP_INIT_RECORD,
            output, first, owned ? ((uint64_t)(uint32_t)node->location.start.line << 32) | (uint32_t)node->location.start.column : 0))
        return INVALID_SLOT;
    return output;
fail:
    return refuse(l, node, "strict constructor requires each field to be initialized from a matching scalar parameter or literal");
}

static uint32_t ownership_expression(Lowerer *l, const SZrAstNode *node) {
    const SZrOwnershipIntrinsicExpression *expr = &node->data.ownershipIntrinsicExpression;
    uint32_t input = lower_expression(l, expr->argument, 0), output, op;
    uint8_t kind;
    if (input == INVALID_SLOT) return INVALID_SLOT;
    switch (expr->operation) {
        case ZR_OWNERSHIP_INTRINSIC_SHARE: kind = ZR_STRICT_VALUE_SHARED; op = ZR_STRICT_OP_SHARE; break;
        case ZR_OWNERSHIP_INTRINSIC_DEGRADE: kind = ZR_STRICT_VALUE_WEAK; op = ZR_STRICT_OP_DEGRADE; break;
        case ZR_OWNERSHIP_INTRINSIC_WAKE: kind = ZR_STRICT_VALUE_MAYBE_SHARED; op = ZR_STRICT_OP_WAKE; break;
        default: return refuse(l, node, "unsupported strict ownership expression");
    }
    output = new_slot(l, kind, l->program.slotLayouts[input], node);
    return output != INVALID_SLOT && emit(l, node, op, output, input, 0) ? output : INVALID_SLOT;
}

static uint32_t lower_binary(Lowerer *l, const SZrAstNode *node) {
    const SZrBinaryExpression *expr = &node->data.binaryExpression;
    const char *op = expr->op.op;
    const SZrAstNode *lhs = expr->left, *rhs = expr->right;
    uint32_t a, b, dst, operation;
    uint8_t kind = ZR_STRICT_VALUE_I64;
    if ((same(op, "==") || same(op, "!=")) && (lhs->type == ZR_AST_NULL_LITERAL || rhs->type == ZR_AST_NULL_LITERAL)) {
        a = lower_expression(l, lhs->type == ZR_AST_NULL_LITERAL ? rhs : lhs, 0);
        if (a == INVALID_SLOT) return INVALID_SLOT;
        dst = new_slot(l, ZR_STRICT_VALUE_BOOL, 0, node);
        if (dst == INVALID_SLOT || !emit(l, node, ZR_STRICT_OP_IS_NULL, dst, a, 0) ||
            (same(op, "!=") && !emit(l, node, ZR_STRICT_OP_NOT, dst, dst, 0))) return INVALID_SLOT;
        return dst;
    }
    if (same(op, "+")) operation = ZR_STRICT_OP_ADD;
    else if (same(op, "-")) operation = ZR_STRICT_OP_SUB;
    else if (same(op, "*")) operation = ZR_STRICT_OP_MUL;
    else if (same(op, "/")) operation = ZR_STRICT_OP_DIV;
    else if (same(op, "%")) operation = ZR_STRICT_OP_MOD;
    else {
        kind = ZR_STRICT_VALUE_BOOL;
        if (same(op, "==")) operation = ZR_STRICT_OP_EQ;
        else if (same(op, "!=")) operation = ZR_STRICT_OP_NE;
        else if (same(op, "<")) operation = ZR_STRICT_OP_LT;
        else if (same(op, "<=")) operation = ZR_STRICT_OP_LE;
        else if (same(op, ">")) operation = ZR_STRICT_OP_GT;
        else if (same(op, ">=")) operation = ZR_STRICT_OP_GE;
        else return refuse(l, node, "unsupported strict binary operation");
    }
    a = lower_expression(l, lhs, 0);
    if (a == INVALID_SLOT) return INVALID_SLOT;
    b = lower_expression(l, rhs, 0);
    if (b == INVALID_SLOT) return INVALID_SLOT;
    dst = new_slot(l, kind, 0, node);
    if (dst == INVALID_SLOT || !emit(l, node, operation, dst, a, 0)) return INVALID_SLOT;
    l->program.instructions[l->program.instructionCount - 1].rhs = b;
    return dst;
}

static uint32_t resolve_field(Lowerer *l, const SZrAstNode *node, uint32_t *slot) {
    const char *baseName, *field;
    uint32_t type, index;
    if (!member_expression(node, &baseName, &field) || (*slot = lookup(l, baseName)) == INVALID_SLOT ||
        !(type = l->program.slotLayouts[*slot])) return refuse(l, node, "expected a statically resolved strict record field");
    index = field_id(&l->models[type - 1], l->program.layouts[type - 1].fieldCount, field);
    if (index == INVALID_SLOT || !(l->models[type - 1].publicMask & (1u << index)))
        return refuse(l, node, "unknown or private strict record field");
    return index;
}

static uint32_t lower_expression(Lowerer *l, const SZrAstNode *node, uint8_t expected) {
    const char *name = identifier(node);
    uint32_t slot, input, index;
    if (!node) return refuse(l, node, "expected a strict expression");
    if (name) {
        slot = lookup(l, name);
        if (slot == INVALID_SLOT || (l->program.kinds[slot] >= ZR_STRICT_VALUE_RECORD && !l->live[slot]))
            return refuse(l, node, "unknown binding or use after move or drop");
        return slot;
    }
    switch (node->type) {
        case ZR_AST_INTEGER_LITERAL: case ZR_AST_BOOLEAN_LITERAL: return literal(l, node);
        case ZR_AST_BINARY_EXPRESSION: return lower_binary(l, node);
        case ZR_AST_CONSTRUCT_EXPRESSION: {
            const SZrConstructExpression *expr = &node->data.constructExpression;
            if (expr->isResourceSurface && expr->builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE)
                return lower_construct(l, node, identifier(expr->target), expr->args, ZR_TRUE);
            if (!expr->isNew && expr->builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_BORROW) {
                if (expected != ZR_STRICT_VALUE_BORROW && expected != ZR_STRICT_VALUE_READONLY_BORROW)
                    return refuse(l, node, "declare a scoped ref or ref readonly binding for this borrow");
                input = lookup(l, identifier(expr->target));
                if (input == INVALID_SLOT || !l->program.slotLayouts[input])
                    return refuse(l, node, "borrow requires a live record binding");
                slot = new_slot(l, expected, l->program.slotLayouts[input], node);
                return slot != INVALID_SLOT && emit(l, node, ZR_STRICT_OP_BORROW, slot, input, 0) ? slot : INVALID_SLOT;
            }
            break;
        }
        case ZR_AST_STRUCT_INIT_EXPRESSION: {
            const SZrStructInitExpression *expr = &node->data.structInitExpression;
            if (!expr->hasNamedArgs && expr->typeInfo && !expr->typeInfo->dimensions && !expr->typeInfo->subType &&
                !expr->typeInfo->referenceAccess && !expr->typeInfo->ownershipQualifier)
                return lower_construct(l, node, identifier(expr->typeInfo->name), expr->args, ZR_FALSE);
            break;
        }
        case ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION: return ownership_expression(l, node);
        case ZR_AST_PRIMARY_EXPRESSION:
            index = resolve_field(l, node, &input);
            if (index == INVALID_SLOT) return INVALID_SLOT;
            slot = new_slot(l, l->program.layouts[l->program.slotLayouts[input] - 1].fields[index], 0, node);
            return slot != INVALID_SLOT && emit(l, node, ZR_STRICT_OP_FIELD_READ, slot, input, index) ? slot : INVALID_SLOT;
        case ZR_AST_UNARY_EXPRESSION:
            if (same(node->data.unaryExpression.op.op, "-") || same(node->data.unaryExpression.op.op, "!")) {
                TZrBool neg = same(node->data.unaryExpression.op.op, "-");
                input = lower_expression(l, node->data.unaryExpression.argument, 0);
                if (input == INVALID_SLOT) return INVALID_SLOT;
                slot = new_slot(l, neg ? ZR_STRICT_VALUE_I64 : ZR_STRICT_VALUE_BOOL, 0, node);
                return slot != INVALID_SLOT && emit(l, node, neg ? ZR_STRICT_OP_NEG : ZR_STRICT_OP_NOT,
                                                      slot, input, 0) ? slot : INVALID_SLOT;
            }
            break;
        default: break;
    }
    return refuse(l, node, "expression has no verified StrictNoGC lowering");
}

static uint8_t binding_type(Lowerer *l, const SZrType *type, uint32_t *layout) {
    const SZrAstNode *node;
    uint8_t scalar = scalar_type(type);
    if (scalar) return scalar;
    if (!type || type->dimensions || type->subType || type->isDecoratorPseudoType ||
        type->isReadonlyView || !type->name) return 0;
    node = type->name;
    if (node->type == ZR_AST_GENERIC_TYPE && !type->referenceAccess) {
        const SZrGenericType *generic = &node->data.genericType;
        if (!type->isImplicitBuiltinType || count(generic->params) != 1) return 0;
        const SZrAstNode *argument = generic->params->nodes[0];
        if (argument->type == ZR_AST_TYPE) {
            const SZrType *inner = &argument->data.type;
            if (inner->dimensions || inner->subType || inner->referenceAccess || inner->ownershipQualifier ||
                inner->isReadonlyView || inner->isDecoratorPseudoType) return 0;
            argument = inner->name;
        }
        if (!(*layout = layout_id(l, identifier(argument)))) return 0;
        switch (type->ownershipQualifier) {
            case ZR_OWNERSHIP_QUALIFIER_UNIQUE: return ZR_STRICT_VALUE_UNIQUE;
            case ZR_OWNERSHIP_QUALIFIER_SHARED: return ZR_STRICT_VALUE_SHARED;
            case ZR_OWNERSHIP_QUALIFIER_WEAK: return ZR_STRICT_VALUE_WEAK;
            default: return 0;
        }
    }
    if (type->ownershipQualifier) return 0;
    if (!(*layout = layout_id(l, identifier(node)))) return 0;
    if (type->referenceAccess == ZR_REFERENCE_ACCESS_READONLY) return ZR_STRICT_VALUE_READONLY_BORROW;
    if (type->referenceAccess == ZR_REFERENCE_ACCESS_WRITABLE) return ZR_STRICT_VALUE_BORROW;
    return ZR_STRICT_VALUE_RECORD;
}

static TZrBool lower_statement(Lowerer *l, const SZrAstNode *node);

static TZrBool lower_block(Lowerer *l, const SZrAstNode *node) {
    const SZrAstNodeArray *body = node->data.block.body;
    uint32_t first = l->program.slotCount, bindings = l->bindingCount;
    ++l->depth;
    for (size_t index = 0; index < count(body); ++index)
        if (!lower_statement(l, body->nodes[index])) return ZR_FALSE;
    for (uint32_t slot = l->program.slotCount; slot > first;) {
        uint8_t kind = l->program.kinds[--slot];
        if (l->live[slot] && !emit(l, node, kind == ZR_STRICT_VALUE_BORROW || kind == ZR_STRICT_VALUE_READONLY_BORROW
                ? ZR_STRICT_OP_END_BORROW : ZR_STRICT_OP_DROP, 0, slot, 0)) return ZR_FALSE;
    }
    --l->depth;
    l->bindingCount = bindings;
    return ZR_TRUE;
}

static TZrBool lower_statement(Lowerer *l, const SZrAstNode *node) {
    uint32_t input, dst, type = 0, field;
    const SZrAstNode *expr = node->type == ZR_AST_EXPRESSION_STATEMENT ? node->data.expressionStatement.expr : node;
    switch (expr->type) {
        case ZR_AST_VARIABLE_DECLARATION: {
            const SZrVariableDeclaration *decl = &expr->data.variableDeclaration;
            uint8_t kind = decl->typeInfo ? binding_type(l, decl->typeInfo, &type) : 0;
            if (decl->typeInfo && !kind) break;
            input = lower_expression(l, decl->value, kind);
            if (input == INVALID_SLOT) return ZR_FALSE;
            if (kind && (kind != l->program.kinds[input] || type != l->program.slotLayouts[input])) {
                (void)refuse(l, node, "initializer does not match the declared strict type");
                return ZR_FALSE;
            }
            kind = l->program.kinds[input];
            dst = input;
            if (identifier(decl->value)) {
                uint32_t op = kind < ZR_STRICT_VALUE_RECORD ? ZR_STRICT_OP_COPY :
                    kind == ZR_STRICT_VALUE_UNIQUE ? ZR_STRICT_OP_TRANSFER : ZR_STRICT_OP_CLONE;
                dst = new_slot(l, kind, l->program.slotLayouts[input], node);
                if (dst == INVALID_SLOT || !emit(l, node, op, dst, input, 0)) return ZR_FALSE;
            }
            return bind(l, node, identifier(decl->pattern), dst);
        }
        case ZR_AST_BLOCK: return lower_block(l, expr);
        case ZR_AST_RETURN_STATEMENT:
            if (l->depth || expr->data.returnStatement.isReferenceReturn) break;
            if (!expr->data.returnStatement.expr) return emit(l, node, ZR_STRICT_OP_HALT, 0, 0, 0);
            input = lower_expression(l, expr->data.returnStatement.expr, 0);
            return input != INVALID_SLOT && emit(l, node, ZR_STRICT_OP_RETURN, 0, input, 0);
        case ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION:
            if (expr->data.ownershipIntrinsicExpression.operation != ZR_OWNERSHIP_INTRINSIC_DROP) break;
            input = lower_expression(l, expr->data.ownershipIntrinsicExpression.argument, 0);
            return input != INVALID_SLOT && emit(l, node, ZR_STRICT_OP_DROP, 0, input, 0);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            if (!same(expr->data.assignmentExpression.op.op, "=")) break;
            field = resolve_field(l, expr->data.assignmentExpression.left, &dst);
            if (field == INVALID_SLOT) return ZR_FALSE;
            input = lower_expression(l, expr->data.assignmentExpression.right, 0);
            return input != INVALID_SLOT && emit(l, node, ZR_STRICT_OP_FIELD_WRITE, dst, input, field);
        default: break;
    }
    (void)refuse(l, expr, "statement has no verified StrictNoGC lowering");
    return ZR_FALSE;
}

static TZrBool add_record(Lowerer *l, const SZrAstNode *node) {
    TZrBool resource = node->type == ZR_AST_CLASS_DECLARATION;
    const SZrAstNodeArray *members;
    const char *name;
    TZrBool readonly = ZR_FALSE;
    ZrStrictRecordLayout *layout;
    RecordModel *model;
    if (resource) {
        const SZrClassDeclaration *decl = &node->data.classDeclaration;
        if (!decl->isOwned || decl->generic || count(decl->inherits) || count(decl->decorators)) goto unsupported;
        name = name_of(decl->name);
        members = decl->members;
    } else {
        const SZrStructDeclaration *decl = &node->data.structDeclaration;
        if (decl->isRefLike || decl->generic || count(decl->inherits) || count(decl->decorators)) goto unsupported;
        name = name_of(decl->name);
        members = decl->members;
        readonly = decl->isReadonly;
    }
    if (!name || strlen(name) >= ZR_STRICT_LAYOUT_NAME_SIZE || layout_id(l, name) ||
        l->program.layoutCount == ZR_STRICT_MAX_LAYOUTS) goto unsupported;
    layout = &l->program.layouts[l->program.layoutCount];
    model = &l->models[l->program.layoutCount++];
    memcpy(layout->name, name, strlen(name) + 1);
    model->resource = resource;
    for (size_t index = 0; index < count(members); ++index) {
        const SZrAstNode *member = members->nodes[index];
        const SZrType *type;
        const SZrAstNode *init;
        const char *fieldName;
        uint8_t kind;
        TZrBool isConst;
        EZrAccessModifier access;
        if (member->type == ZR_AST_CLASS_FIELD && resource) {
            const SZrClassField *f = &member->data.classField;
            if (f->isStatic || count(f->decorators) || f->reservedRemovedUsingManaged) goto unsupported;
            type = f->typeInfo; init = f->init; fieldName = name_of(f->name); isConst = f->isConst; access = f->access;
        } else if (member->type == ZR_AST_STRUCT_FIELD && !resource) {
            const SZrStructField *f = &member->data.structField;
            if (f->isStatic || count(f->decorators) || f->reservedRemovedUsingManaged) goto unsupported;
            type = f->typeInfo; init = f->init; fieldName = name_of(f->name); isConst = f->isConst; access = f->access;
        } else {
            const SZrAstNode *body;
            const SZrAstNodeArray *params;
            if (model->constructor) goto unsupported;
            if (resource && member->type == ZR_AST_CLASS_META_FUNCTION) {
                const SZrClassMetaFunction *m = &member->data.classMetaFunction;
                if (!same(name_of(m->meta), "constructor") || m->isStatic || m->hasSuperCall || m->args ||
                    m->returnType || m->access != ZR_ACCESS_PUBLIC) goto unsupported;
                body = m->body; params = m->params;
            } else if (!resource && member->type == ZR_AST_STRUCT_META_FUNCTION) {
                const SZrStructMetaFunction *m = &member->data.structMetaFunction;
                if (!same(name_of(m->meta), "constructor") || m->isStatic || m->args ||
                    m->returnType || m->access != ZR_ACCESS_PUBLIC) goto unsupported;
                body = m->body; params = m->params;
            } else goto unsupported;
            if (!body || body->type != ZR_AST_BLOCK || count(params) > ZR_STRICT_MAX_FIELDS) goto unsupported;
            for (size_t param = 0; param < count(params); ++param) {
                const SZrAstNode *arg = params->nodes[param];
                if (!arg || arg->type != ZR_AST_PARAMETER || !scalar_type(arg->data.parameter.typeInfo) ||
                    arg->data.parameter.defaultValue || count(arg->data.parameter.decorators) ||
                    arg->data.parameter.passingMode != ZR_PARAMETER_PASSING_MODE_VALUE) goto unsupported;
                for (size_t prior = 0; prior < param; ++prior) {
                    if (same(name_of(arg->data.parameter.name), name_of(params->nodes[prior]->data.parameter.name)))
                        goto unsupported;
                }
            }
            model->constructor = body;
            model->params = params;
            continue;
        }
        kind = scalar_type(type);
        if (!kind || !fieldName || layout->fieldCount == ZR_STRICT_MAX_FIELDS ||
            field_id(model, layout->fieldCount, fieldName) != INVALID_SLOT ||
            (init && !((kind == ZR_STRICT_VALUE_I64 && init->type == ZR_AST_INTEGER_LITERAL) ||
                       (kind == ZR_STRICT_VALUE_BOOL && init->type == ZR_AST_BOOLEAN_LITERAL)))) goto unsupported;
        uint32_t field = layout->fieldCount++;
        model->fields[field] = fieldName;
        model->defaults[field] = init;
        layout->fields[field] = kind;
        if (isConst || readonly) layout->readonlyMask |= 1u << field;
        if (access == ZR_ACCESS_PUBLIC) model->publicMask |= 1u << field;
    }
    return ZR_TRUE;
unsupported:
    (void)refuse(l, node, "strict record requires a fixed scalar layout and a statically checked constructor");
    return ZR_FALSE;
}

TZrBool ZrCli_StrictRecords_Compile(SZrState *state, const TZrChar *source, TZrSize length,
                                    const TZrChar *path, ZrStrictProgram *out,
                                    TZrChar *error, TZrSize errorSize) {
    SZrParserState parser;
    SZrAstNode *root = NULL;
    SZrString *sourceName;
    Lowerer l = {.path = path, .error = error, .errorSize = errorSize};
    const SZrAstNodeArray *statements;
    uint32_t badPc;
    TZrBool success = ZR_FALSE;
    sourceName = ZrCore_String_Create(state, (TZrNativeString)path, strlen(path));
    if (!sourceName) goto done;
    ZrParser_State_Init(&parser, state, source, length, sourceName);
    root = ZrParser_ParseWithState(&parser);
    TZrBool failed = parser.hasError;
    ZrParser_State_Free(&parser);
    if (failed || !root || root->type != ZR_AST_SCRIPT) goto done;
    l.program.kinds = calloc(ZR_STRICT_MAX_SLOTS, sizeof(*l.program.kinds));
    l.program.slotLayouts = calloc(ZR_STRICT_MAX_SLOTS, sizeof(*l.program.slotLayouts));
    l.program.layouts = calloc(ZR_STRICT_MAX_LAYOUTS, sizeof(*l.program.layouts));
    l.models = calloc(ZR_STRICT_MAX_LAYOUTS, sizeof(*l.models));
    l.bindings = calloc(ZR_STRICT_MAX_SLOTS, sizeof(*l.bindings));
    if (!l.program.kinds || !l.program.slotLayouts || !l.program.layouts || !l.models || !l.bindings) goto done;
    statements = root->data.script.statements;
    for (size_t index = 0; index < count(statements); ++index) {
        const SZrAstNode *node = statements->nodes[index];
        if ((node->type == ZR_AST_STRUCT_DECLARATION || node->type == ZR_AST_CLASS_DECLARATION) &&
            !add_record(&l, node)) goto done;
    }
    for (size_t index = 0; index < count(statements); ++index) {
        const SZrAstNode *node = statements->nodes[index];
        if (node->type == ZR_AST_STRUCT_DECLARATION || node->type == ZR_AST_CLASS_DECLARATION ||
            node->type == ZR_AST_MODULE_DECLARATION) continue;
        if (!lower_statement(&l, node)) goto done;
    }
    if (!emit(&l, root, ZR_STRICT_OP_HALT, 0, 0, 0)) goto done;
    if (ZrStrictProgram_Verify(&l.program, &badPc) != ZR_STRICT_PROGRAM_OK) {
        SZrAstNode at = {.location = badPc < l.program.instructionCount ? l.locations[badPc] : root->location};
        (void)refuse(&l, &at, "invalid ownership, conflicting borrow, readonly field, or incompatible operand type");
        goto done;
    }
    *out = l.program;
    l.program = (ZrStrictProgram){0};
    success = ZR_TRUE;
done:
    if (!success && !error[0]) (void)refuse(&l, root, "could not build the strict record program");
    free(l.models);
    free(l.bindings);
    free(l.locations);
    ZrStrictProgram_Free(&l.program);
    ZrParser_Ast_Free(state, root);
    return success;
}
