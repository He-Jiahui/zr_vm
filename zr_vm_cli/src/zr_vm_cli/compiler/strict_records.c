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
    SZrAstNode *destructor;
    TZrBool resource;
} RecordModel;

typedef struct Binding {
    const char *name;
    uint32_t slot;
    uint32_t depth;
} Binding;

typedef struct Callable {
    const char *name;
    const SZrAstNodeArray *params;
    const SZrType *output;
    SZrAstNode *body;
    uint32_t layout;
    uint8_t receiver;
    TZrBool isPublic;
} Callable;

typedef struct CallScope {
    struct CallScope *outer;
    const Callable *target;
    uint32_t result, firstLocal, exits;
} CallScope;

typedef struct ConstructorScope {
    struct ConstructorScope *outer;
    const CallScope *caller;
    uint32_t layout;
    uint32_t fields[ZR_STRICT_MAX_FIELDS];
} ConstructorScope;

typedef struct Lowerer {
    ZrStrictProgram program;
    RecordModel *models;
    Binding *bindings;
    Callable *callables;
    CallScope *call;
    ConstructorScope *constructing;
    uint32_t callableCount, bindingFloor;
    SZrFileRange *locations;
    uint8_t live[ZR_STRICT_MAX_SLOTS];
    uint32_t depth, bindingCount, capacity;
    TZrBool terminated, inDestructor;
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

static TZrBool field_owner(uint8_t kind) {
    return kind == ZR_STRICT_VALUE_UNIQUE || kind == ZR_STRICT_VALUE_SHARED ||
           kind == ZR_STRICT_VALUE_WEAK || kind == ZR_STRICT_VALUE_MANUAL_BLOCK;
}

static TZrBool field_kind(uint8_t kind) {
    return field_owner(kind) || kind == ZR_STRICT_VALUE_I64 || kind == ZR_STRICT_VALUE_BOOL;
}

static TZrBool byte_view(uint8_t kind) {
    return kind == ZR_STRICT_VALUE_BYTE_VIEW || kind == ZR_STRICT_VALUE_READONLY_BYTE_VIEW;
}

static TZrBool is_borrow(uint8_t kind) {
    return byte_view(kind) || kind == ZR_STRICT_VALUE_BORROW || kind == ZR_STRICT_VALUE_READONLY_BORROW;
}

static uint64_t allocation_site(const SZrAstNode *node) {
    return ((uint64_t)(uint32_t)node->location.start.line << 32) | (uint32_t)node->location.start.column;
}

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
    if (result == (l->inDestructor ? ZR_STRICT_MAX_DESTRUCTOR_SLOTS : ZR_STRICT_MAX_SLOTS))
        return refuse(l, node, "too many typed slots");
    l->program.slotCount++;
    l->program.kinds[result] = kind;
    l->program.slotLayouts[result] = layout;
    return result;
}

static TZrBool emit(Lowerer *l, const SZrAstNode *node, uint32_t op, uint32_t dst, uint32_t lhs,
                    uint64_t immediate) {
    uint32_t ix = l->program.instructionCount;
    if (l->inDestructor && (op == ZR_STRICT_OP_OWN_RECORD || op == ZR_STRICT_OP_MANUAL_TRY_ALLOC)) {
        (void)refuse(l, node, "a StrictNoGC destructor and its callees cannot allocate");
        return ZR_FALSE;
    }
    if (l->inDestructor && ix == ZR_STRICT_MAX_DESTRUCTOR_INSTRUCTIONS) {
        (void)refuse(l, node, "strict destructor exceeds its instruction budget");
        return ZR_FALSE;
    }
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
    switch (op) {
        case ZR_STRICT_OP_INIT_RECORD: case ZR_STRICT_OP_OWN_RECORD: case ZR_STRICT_OP_MANUAL_TRY_ALLOC:
        case ZR_STRICT_OP_TRANSFER: case ZR_STRICT_OP_CLONE: case ZR_STRICT_OP_SHARE:
        case ZR_STRICT_OP_DEGRADE: case ZR_STRICT_OP_WAKE: case ZR_STRICT_OP_BORROW: case ZR_STRICT_OP_BORROW_FIELD:
            l->live[dst] = 1;
            break;
        case ZR_STRICT_OP_FIELD_READ:
            if (field_owner(l->program.kinds[dst])) l->live[dst] = 1;
            break;
        case ZR_STRICT_OP_FIELD_WRITE:
            if (field_owner(l->program.kinds[lhs])) l->live[lhs] = 0;
            break;
        default: break;
    }
    return ZR_TRUE;
oom:
    (void)refuse(l, node, "could not grow strict instruction storage");
    return ZR_FALSE;
}

static uint32_t lookup(const Lowerer *l, const char *name) {
    for (uint32_t index = l->bindingCount; index > l->bindingFloor; --index)
        if (same(name, l->bindings[index - 1].name)) return l->bindings[index - 1].slot;
    return INVALID_SLOT;
}

static TZrBool bind(Lowerer *l, const SZrAstNode *node, const char *name, uint32_t slot) {
    if (name && l->bindingCount < ZR_STRICT_MAX_SLOTS) {
        uint32_t index = l->bindingCount;
        while (index > l->bindingFloor && l->bindings[index - 1].depth == l->depth) {
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
static TZrBool lower_call(Lowerer *l, const SZrAstNode *node, TZrBool needResult, uint32_t *result);
static TZrBool release_slots(Lowerer *l, const SZrAstNode *node, uint32_t first);
static TZrBool release_range(Lowerer *l, const SZrAstNode *node, uint32_t first, uint32_t last);
static uint8_t binding_type(Lowerer *l, const SZrType *type, uint32_t *layout);
static uint32_t assignment_op(const Lowerer *l, uint8_t kind, uint32_t type);
static uint32_t resolve_field(Lowerer *l, const SZrAstNode *node, uint32_t *slot);

static const SZrFunctionCall *final_call(const SZrAstNode *node) {
    const SZrAstNode *last;
    const SZrAstNodeArray *members;
    if (node->type != ZR_AST_PRIMARY_EXPRESSION) return NULL;
    members = node->data.primaryExpression.members;
    if (!count(members)) return NULL;
    last = members->nodes[members->count - 1];
    return last->type == ZR_AST_FUNCTION_CALL ? &last->data.functionCall : NULL;
}

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
    uint32_t type = layout_id(l, name), output, first, depth = 0, firstLocal = l->program.slotCount;
    const RecordModel *model;
    const ZrStrictRecordLayout *layout;
    if (!type) return refuse(l, node, "unknown strict record layout");
    model = &l->models[type - 1];
    layout = &l->program.layouts[type - 1];
    if ((model->resource && !owned) || count(args) != count(model->params))
        return refuse(l, node, "construction requires matching positional arguments and resource ownership");
    /* Evaluate arguments in the caller. The construction itself cannot
     * capture caller locals or expose its uninitialized receiver to a method. */
    uint32_t values[ZR_STRICT_MAX_FIELDS];
    for (uint32_t index = 0; index < count(args); ++index) {
        const SZrParameter *parameter = &model->params->nodes[index]->data.parameter;
        uint32_t paramLayout = 0;
        uint8_t paramKind = binding_type(l, parameter->typeInfo, &paramLayout);
        uint32_t value = lower_expression(l, args->nodes[index], paramKind);
        if (value == INVALID_SLOT) return INVALID_SLOT;
        if (paramKind != l->program.kinds[value] || paramLayout != l->program.slotLayouts[value])
            return refuse(l, node, "constructor argument does not match the declared strict type");
        values[index] = new_slot(l, paramKind, paramLayout, node);
        if (values[index] == INVALID_SLOT || !emit(l, node, assignment_op(l, paramKind, paramLayout), values[index], value, 0))
            return INVALID_SLOT;
    }
    for (const ConstructorScope *ancestor = l->constructing; ancestor; ancestor = ancestor->outer)
        if (ancestor->layout == type || ++depth >= 64)
            return refuse(l, node, "recursive strict constructor requires a separate execution frame");

    ConstructorScope scope = {.outer = l->constructing, .caller = l->call, .layout = type};
    uint32_t savedBindings = l->bindingCount, savedFloor = l->bindingFloor;
    l->constructing = &scope;
    l->bindingFloor = savedBindings;
    ++l->depth;
    for (uint32_t index = 0; index < count(args); ++index) {
        if (!bind(l, node, name_of(model->params->nodes[index]->data.parameter.name), values[index])) goto fail;
    }
    for (uint32_t field = 0; field < layout->fieldCount; ++field) {
        scope.fields[field] = model->defaults[field] ? literal(l, model->defaults[field]) : INVALID_SLOT;
        if (model->defaults[field] && scope.fields[field] == INVALID_SLOT) goto fail;
    }
    if (model->constructor) {
        const SZrAstNodeArray *body = model->constructor->data.block.body;
        for (size_t index = 0; index < count(body); ++index) {
            const SZrAstNode *stmt = body->nodes[index];
            const SZrAstNode *assign = stmt->type == ZR_AST_EXPRESSION_STATEMENT ? stmt->data.expressionStatement.expr : stmt;
            const char *base, *member;
            if (!assign || assign->type != ZR_AST_ASSIGNMENT_EXPRESSION || !same(assign->data.assignmentExpression.op.op, "=") ||
                !member_expression(assign->data.assignmentExpression.left, &base, &member) || !same(base, "this")) goto fail;
            uint32_t field = field_id(model, layout->fieldCount, member);
            if (field == INVALID_SLOT || (scope.fields[field] != INVALID_SLOT && (layout->readonlyMask & (1u << field)))) goto fail;
            uint8_t kind = layout->fields[field];
            uint32_t fieldType = layout->fieldLayouts[field];
            uint32_t value = lower_expression(l, assign->data.assignmentExpression.right, kind);
            if (value == INVALID_SLOT || l->program.kinds[value] != kind || l->program.slotLayouts[value] != fieldType) goto fail;
            if (field_owner(kind)) {
                uint32_t staged = new_slot(l, kind, fieldType, assign);
                if (staged == INVALID_SLOT || !emit(l, assign, assignment_op(l, kind, fieldType), staged, value, 0)) goto fail;
                uint32_t previous = scope.fields[field];
                if (previous != INVALID_SLOT && !emit(l, assign, ZR_STRICT_OP_DROP, 0, previous, 0)) goto fail;
                value = staged;
            }
            scope.fields[field] = value;
        }
    }
    /* A partially constructed record never owns the staging slots. The frame
     * drops them on arithmetic/allocation failure, without calling its destructor. */
    first = l->program.slotCount;
    for (uint32_t field = 0; field < layout->fieldCount; ++field) {
        uint8_t kind = layout->fields[field];
        uint32_t slot;
        if (scope.fields[field] == INVALID_SLOT) goto fail;
        slot = new_slot(l, kind, layout->fieldLayouts[field], node);
        if (slot == INVALID_SLOT || !emit(l, node, field_owner(kind) ? ZR_STRICT_OP_TRANSFER : ZR_STRICT_OP_COPY,
                                        slot, scope.fields[field], 0)) goto fail;
    }
    if (!release_range(l, node, firstLocal, first)) goto fail;
    output = new_slot(l, owned ? ZR_STRICT_VALUE_UNIQUE : ZR_STRICT_VALUE_RECORD, type, node);
    if (output == INVALID_SLOT || !emit(l, node, owned ? ZR_STRICT_OP_OWN_RECORD : ZR_STRICT_OP_INIT_RECORD,
            output, first, owned ? allocation_site(node) : 0)) goto fail;
    for (uint32_t field = 0; field < layout->fieldCount; ++field)
        if (field_owner(layout->fields[field])) l->live[first + field] = 0;
    goto done;
fail:
    output = refuse(l, node, "strict constructor must initialize every field with a matching value or owner");
done:
    --l->depth;
    l->bindingCount = savedBindings;
    l->bindingFloor = savedFloor;
    l->constructing = scope.outer;
    return output;
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
    if (index == INVALID_SLOT || (!(l->models[type - 1].publicMask & (1u << index)) &&
                                 (!l->call || l->call->target->layout != type)))
        return refuse(l, node, "unknown or private strict record field");
    return index;
}

static const SZrFunctionCall *manual_try_alloc_call(const SZrAstNode *node) {
    const SZrAstNodeArray *members = node->data.primaryExpression.members;
    const SZrAstNode *member, *call;
    if (count(members) != 2 || !same(identifier(node->data.primaryExpression.property), "ManualBlock")) return NULL;
    member = members->nodes[0];
    call = members->nodes[1];
    if (member->type != ZR_AST_MEMBER_EXPRESSION || member->data.memberExpression.computed ||
        member->data.memberExpression.accessMode != ZR_POSTFIX_ACCESS_DIRECT ||
        !same(identifier(member->data.memberExpression.property), "tryAlloc") ||
        call->type != ZR_AST_FUNCTION_CALL || call->data.functionCall.accessMode != ZR_POSTFIX_ACCESS_DIRECT) return NULL;
    return &call->data.functionCall;
}

static uint32_t lower_manual_try_alloc(Lowerer *l, const SZrAstNode *node, const SZrFunctionCall *call) {
    uint32_t size, dst;
    if (lookup(l, "ManualBlock") != INVALID_SLOT || call->hasNamedArgs || count(call->genericArguments) || count(call->args) != 1)
        return refuse(l, node, "ManualBlock.tryAlloc requires one positional byte count");
    if (call->argumentMarkers && call->argumentMarkers->length) {
        const SZrCallArgumentSyntax *marker = ZrCore_Array_Get(call->argumentMarkers, 0);
        if (!marker || marker->marker != ZR_CALL_ARGUMENT_MARKER_NONE)
            return refuse(l, node, "ManualBlock.tryAlloc takes a byte count by value");
    }
    size = lower_expression(l, call->args->nodes[0], ZR_STRICT_VALUE_I64);
    if (size == INVALID_SLOT) return INVALID_SLOT;
    if (l->program.kinds[size] != ZR_STRICT_VALUE_I64) return refuse(l, node, "manual byte count must be int");
    dst = new_slot(l, ZR_STRICT_VALUE_MANUAL_BLOCK, 0, node);
    return dst != INVALID_SLOT && emit(l, node, ZR_STRICT_OP_MANUAL_TRY_ALLOC, dst, size, allocation_site(node))
            ? dst : INVALID_SLOT;
}

/* Match a single member selection off a local byte view; multi-part or
 * computed access on ordinary records stays outside this built-in. */
static const SZrMemberExpression *manual_view_member(Lowerer *l, const SZrAstNode *node, uint32_t *view) {
    const SZrAstNode *member;
    uint32_t local;
    if (!node || node->type != ZR_AST_PRIMARY_EXPRESSION || count(node->data.primaryExpression.members) != 1) return NULL;
    local = lookup(l, identifier(node->data.primaryExpression.property));
    if (local == INVALID_SLOT || !byte_view(l->program.kinds[local])) return NULL;
    member = node->data.primaryExpression.members->nodes[0];
    if (member->type != ZR_AST_MEMBER_EXPRESSION || member->data.memberExpression.accessMode != ZR_POSTFIX_ACCESS_DIRECT)
        return NULL;
    *view = local;
    return &member->data.memberExpression;
}

static uint32_t lower_byte_read(Lowerer *l, const SZrAstNode *node, uint32_t view, const SZrMemberExpression *member) {
    uint32_t op, index = 0, result;
    if (!l->live[view]) return refuse(l, node, "byte view has expired");
    if (member->computed) {
        index = lower_expression(l, member->property, ZR_STRICT_VALUE_I64);
        if (index == INVALID_SLOT) return INVALID_SLOT;
        if (l->program.kinds[index] != ZR_STRICT_VALUE_I64) return refuse(l, node, "byte index must be int");
        op = ZR_STRICT_OP_BYTE_READ;
    } else {
        if (!same(identifier(member->property), "length")) return refuse(l, node, "unknown byte view member");
        op = ZR_STRICT_OP_BYTE_LENGTH;
    }
    result = new_slot(l, ZR_STRICT_VALUE_I64, 0, node);
    if (result == INVALID_SLOT || !emit(l, node, op, result, view, 0)) return INVALID_SLOT;
    l->program.instructions[l->program.instructionCount - 1].rhs = index;
    return result;
}

static TZrBool lower_byte_write(Lowerer *l, const SZrAstNode *node, uint32_t view, const SZrMemberExpression *member) {
    uint32_t index, input;
    if (!member->computed || !l->live[view] || l->program.kinds[view] != ZR_STRICT_VALUE_BYTE_VIEW) {
        (void)refuse(l, node, "byte assignment requires an indexed writable view");
        return ZR_FALSE;
    }
    index = lower_expression(l, member->property, ZR_STRICT_VALUE_I64);
    input = lower_expression(l, node->data.assignmentExpression.right, ZR_STRICT_VALUE_I64);
    if (index == INVALID_SLOT || input == INVALID_SLOT) return ZR_FALSE;
    if (l->program.kinds[index] != ZR_STRICT_VALUE_I64 || l->program.kinds[input] != ZR_STRICT_VALUE_I64) {
        (void)refuse(l, node, "byte index and assigned value must be int");
        return ZR_FALSE;
    }
    if (!emit(l, node, ZR_STRICT_OP_BYTE_WRITE, view, index, 0)) return ZR_FALSE;
    l->program.instructions[l->program.instructionCount - 1].rhs = input;
    return ZR_TRUE;
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
                if (!is_borrow(expected))
                    return refuse(l, node, "declare a scoped ref or ref readonly binding for this borrow");
                const char *base, *field;
                if (member_expression(expr->target, &base, &field)) {
                    uint32_t member = resolve_field(l, expr->target, &input);
                    if (member == INVALID_SLOT) return INVALID_SLOT;
                    const ZrStrictRecordLayout *layout = &l->program.layouts[l->program.slotLayouts[input] - 1];
                    slot = new_slot(l, expected, layout->fieldLayouts[member], node);
                    return slot != INVALID_SLOT && emit(l, node, ZR_STRICT_OP_BORROW_FIELD, slot, input, member)
                            ? slot : INVALID_SLOT;
                }
                input = lookup(l, identifier(expr->target));
                if (input == INVALID_SLOT || !l->live[input]) return refuse(l, node, "borrow requires a live owner");
                if (byte_view(expected) ? (l->program.kinds[input] != ZR_STRICT_VALUE_MANUAL_BLOCK &&
                                           !byte_view(l->program.kinds[input])) : !l->program.slotLayouts[input])
                    return refuse(l, node, "borrow target does not match the requested view");
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
        case ZR_AST_PRIMARY_EXPRESSION: {
            const SZrFunctionCall *allocation = manual_try_alloc_call(node);
            const SZrMemberExpression *member;
            const char *base, *field;
            const ConstructorScope *constructor = l->constructing;
            if (constructor && constructor->caller == l->call && member_expression(node, &base, &field) && same(base, "this")) {
                uint32_t type = constructor->layout;
                index = field_id(&l->models[type - 1], l->program.layouts[type - 1].fieldCount, field);
                if (index == INVALID_SLOT || constructor->fields[index] == INVALID_SLOT)
                    return refuse(l, node, "constructor read an uninitialized record field");
                if (l->program.layouts[type - 1].fields[index] == ZR_STRICT_VALUE_UNIQUE ||
                    l->program.layouts[type - 1].fields[index] == ZR_STRICT_VALUE_MANUAL_BLOCK)
                    return refuse(l, node, "cannot copy a unique field while constructing a record");
                return constructor->fields[index];
            }
            if (allocation) return lower_manual_try_alloc(l, node, allocation);
            if (final_call(node)) return lower_call(l, node, ZR_TRUE, &slot) ? slot : INVALID_SLOT;
            member = manual_view_member(l, node, &input);
            if (member) return lower_byte_read(l, node, input, member);
            index = resolve_field(l, node, &input);
            if (index == INVALID_SLOT) return INVALID_SLOT;
            const ZrStrictRecordLayout *layout = &l->program.layouts[l->program.slotLayouts[input] - 1];
            uint8_t kind = layout->fields[index];
            if (kind == ZR_STRICT_VALUE_UNIQUE || kind == ZR_STRICT_VALUE_MANUAL_BLOCK)
                return refuse(l, node, "borrow this unique field instead of copying it");
            slot = new_slot(l, kind, layout->fieldLayouts[index], node);
            return slot != INVALID_SLOT && emit(l, node, ZR_STRICT_OP_FIELD_READ, slot, input, index) ? slot : INVALID_SLOT;
        }
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
    if (same(identifier(node), "ManualBlock")) {
        if (type->referenceAccess == ZR_REFERENCE_ACCESS_READONLY) return ZR_STRICT_VALUE_READONLY_BYTE_VIEW;
        if (type->referenceAccess == ZR_REFERENCE_ACCESS_WRITABLE) return ZR_STRICT_VALUE_BYTE_VIEW;
        return ZR_STRICT_VALUE_MANUAL_BLOCK;
    }
    if (!(*layout = layout_id(l, identifier(node)))) return 0;
    if (type->referenceAccess == ZR_REFERENCE_ACCESS_READONLY) return ZR_STRICT_VALUE_READONLY_BORROW;
    if (type->referenceAccess == ZR_REFERENCE_ACCESS_WRITABLE) return ZR_STRICT_VALUE_BORROW;
    return ZR_STRICT_VALUE_RECORD;
}

static uint8_t parameter_kind(Lowerer *l, const SZrParameter *param, uint32_t *layout) {
    SZrType type;
    if (!param->typeInfo) return 0;
    type = *param->typeInfo;
    /* The parser separates the callable passing form from the parameter's
     * type, while local ref declarations carry it on the type itself. */
    if (param->passingMode == ZR_PARAMETER_PASSING_MODE_REF) {
        if (type.referenceAccess != ZR_REFERENCE_ACCESS_NONE) return 0;
        switch (param->sourcePassingForm) {
            case ZR_PARAMETER_SOURCE_REF:
            case ZR_PARAMETER_SOURCE_SCOPED_REF:
                type.referenceAccess = ZR_REFERENCE_ACCESS_WRITABLE;
                break;
            case ZR_PARAMETER_SOURCE_REF_READONLY:
            case ZR_PARAMETER_SOURCE_SCOPED_REF_READONLY:
                type.referenceAccess = ZR_REFERENCE_ACCESS_READONLY;
                break;
            default: return 0;
        }
    } else if (param->passingMode != ZR_PARAMETER_PASSING_MODE_VALUE) return 0;
    return binding_type(l, &type, layout);
}

static TZrBool lower_statement(Lowerer *l, const SZrAstNode *node);

static TZrBool release_range(Lowerer *l, const SZrAstNode *node, uint32_t first, uint32_t last) {
    for (uint32_t slot = last; slot > first;) {
        uint8_t kind = l->program.kinds[--slot];
        if (!l->live[slot]) continue;
        if (!l->terminated && !emit(l, node, is_borrow(kind) ? ZR_STRICT_OP_END_BORROW : ZR_STRICT_OP_DROP,
                                    0, slot, 0)) return ZR_FALSE;
        l->live[slot] = 0;
    }
    return ZR_TRUE;
}

static TZrBool release_slots(Lowerer *l, const SZrAstNode *node, uint32_t first) {
    return release_range(l, node, first, l->program.slotCount);
}

static TZrBool lower_block(Lowerer *l, const SZrAstNode *node) {
    const SZrAstNodeArray *body = node->data.block.body;
    uint32_t first = l->program.slotCount, bindings = l->bindingCount;
    ++l->depth;
    for (size_t index = 0; index < count(body); ++index)
        if (!lower_statement(l, body->nodes[index])) return ZR_FALSE;
    if (!release_slots(l, node, first)) return ZR_FALSE;
    --l->depth;
    l->bindingCount = bindings;
    return ZR_TRUE;
}

static uint32_t lower_condition(Lowerer *l, const SZrAstNode *condition) {
    uint32_t first = l->program.slotCount;
    uint32_t slot = lower_expression(l, condition, ZR_STRICT_VALUE_BOOL);
    if (slot == INVALID_SLOT) return INVALID_SLOT;
    if (l->program.kinds[slot] != ZR_STRICT_VALUE_BOOL)
        return refuse(l, condition, "strict condition must be bool");
    return release_slots(l, condition, first) ? slot : INVALID_SLOT;
}

static TZrBool lower_if(Lowerer *l, const SZrAstNode *node) {
    const SZrIfExpression *branch = &node->data.ifExpression;
    uint32_t condition = lower_condition(l, branch->condition);
    uint32_t jumpElse = l->program.instructionCount, jumpEnd = INVALID_SLOT;
    uint32_t owned = l->program.slotCount;
    uint8_t *state;
    TZrBool thenReturns, ok = ZR_FALSE;
    if (condition == INVALID_SLOT || !emit(l, node, ZR_STRICT_OP_JUMP_IF_FALSE, 0, condition, 0)) return ZR_FALSE;
    state = malloc((size_t)owned * 2);
    if (!state) {
        (void)refuse(l, node, "could not save ownership state at branch");
        return ZR_FALSE;
    }
    memcpy(state, l->live, owned);
    if (!lower_statement(l, branch->thenExpr)) goto done;
    thenReturns = l->terminated;
    if (branch->elseExpr) {
        if (!thenReturns) {
            jumpEnd = l->program.instructionCount;
            if (!emit(l, node, ZR_STRICT_OP_JUMP, 0, 0, 0)) goto done;
        }
        memcpy(state + owned, l->live, owned);
        memcpy(l->live, state, owned);
        l->terminated = ZR_FALSE;
        l->program.instructions[jumpElse].immediate = l->program.instructionCount;
        if (!lower_statement(l, branch->elseExpr)) goto done;
        if (!thenReturns) {
            if (l->terminated) memcpy(l->live, state + owned, owned);
            else if (memcmp(l->live, state + owned, owned)) goto disagree;
            l->terminated = ZR_FALSE;
            l->program.instructions[jumpEnd].immediate = l->program.instructionCount;
        }
    } else {
        l->program.instructions[jumpElse].immediate = l->program.instructionCount;
        if (thenReturns) memcpy(l->live, state, owned);
        else if (memcmp(l->live, state, owned)) goto disagree;
        l->terminated = ZR_FALSE;
    }
    ok = ZR_TRUE;
    goto done;
disagree:
    (void)refuse(l, node, "branches that continue must agree on which owners are live");
done:
    free(state);
    return ok;
}

static TZrBool lower_while(Lowerer *l, const SZrAstNode *node) {
    const SZrWhileLoop *loop = &node->data.whileLoop;
    uint32_t repeat = l->program.instructionCount;
    uint32_t condition = lower_condition(l, loop->cond);
    uint32_t exit = l->program.instructionCount, owned = l->program.slotCount;
    uint8_t *state;
    TZrBool ok = ZR_FALSE;
    if (condition == INVALID_SLOT || !emit(l, node, ZR_STRICT_OP_JUMP_IF_FALSE, 0, condition, 0)) return ZR_FALSE;
    state = malloc(owned);
    if (!state) {
        (void)refuse(l, node, "could not save ownership state at loop");
        return ZR_FALSE;
    }
    memcpy(state, l->live, owned);
    if (!lower_statement(l, loop->block)) goto done;
    if (!l->terminated) {
        if (memcmp(l->live, state, owned)) {
            (void)refuse(l, node, "each loop iteration must preserve the ownership state at its entry");
            goto done;
        }
        if (!emit(l, node, ZR_STRICT_OP_JUMP, 0, 0, repeat)) goto done;
    } else {
        memcpy(l->live, state, owned);
    }
    l->terminated = ZR_FALSE;
    l->program.instructions[exit].immediate = l->program.instructionCount;
    ok = ZR_TRUE;
done:
    free(state);
    return ok;
}

static const Callable *find_callable(const Lowerer *l, uint32_t layout, const char *name) {
    for (uint32_t index = 0; index < l->callableCount; ++index) {
        const Callable *candidate = &l->callables[index];
        if (candidate->layout == layout && same(candidate->name, name)) return candidate;
    }
    return NULL;
}

static TZrBool void_result(const SZrType *type) {
    return !type || (same(identifier(type->name), "void") && !type->referenceAccess && !type->dimensions &&
                     !type->subType && !type->ownershipQualifier && !type->isReadonlyView && !type->isDecoratorPseudoType);
}

static TZrBool match_slot(const Lowerer *l, uint32_t slot, uint8_t kind, uint32_t layout) {
    return slot != INVALID_SLOT && l->program.kinds[slot] == kind && l->program.slotLayouts[slot] == layout;
}

static uint32_t assignment_op(const Lowerer *l, uint8_t kind, uint32_t type) {
    if (kind < ZR_STRICT_VALUE_RECORD) return ZR_STRICT_OP_COPY;
    if (kind == ZR_STRICT_VALUE_UNIQUE || kind == ZR_STRICT_VALUE_MANUAL_BLOCK) return ZR_STRICT_OP_TRANSFER;
    if (kind == ZR_STRICT_VALUE_RECORD) {
        const ZrStrictRecordLayout *layout = &l->program.layouts[type - 1];
        for (uint32_t field = 0; field < layout->fieldCount; ++field)
            if (field_owner(layout->fields[field])) return ZR_STRICT_OP_TRANSFER;
    }
    return ZR_STRICT_OP_CLONE;
}

static const Callable *resolve_call(const Lowerer *l, const SZrAstNode *node, uint32_t *receiver) {
    const SZrPrimaryExpression *primary = &node->data.primaryExpression;
    const char *base = identifier(primary->property);
    const SZrAstNode *member;
    const Callable *target;
    uint32_t type, slot;
    if (!base) return NULL;
    slot = lookup(l, base);
    if (count(primary->members) == 1 && slot == INVALID_SLOT) return find_callable(l, 0, base);
    if (count(primary->members) != 2) return NULL;
    member = primary->members->nodes[0];
    if (member->type != ZR_AST_MEMBER_EXPRESSION || member->data.memberExpression.computed ||
        member->data.memberExpression.accessMode != ZR_POSTFIX_ACCESS_DIRECT) return NULL;
    type = slot == INVALID_SLOT ? layout_id(l, base) : l->program.slotLayouts[slot];
    if (!type) return NULL;
    target = find_callable(l, type, identifier(member->data.memberExpression.property));
    if (!target || (target->receiver != 0) != (slot != INVALID_SLOT)) return NULL;
    if (!target->isPublic && (!l->call || l->call->target->layout != type)) return NULL;
    if (slot != INVALID_SLOT && !l->live[slot]) return NULL;
    *receiver = slot;
    return target;
}

/* Inline a statically resolved body. The only artifact edges are typed jumps;
 * the callee sees arguments and its receiver, with no access to caller locals. */
static TZrBool lower_call(Lowerer *l, const SZrAstNode *node, TZrBool needResult, uint32_t *result) {
    const SZrFunctionCall *invocation = final_call(node);
    uint32_t receiver = INVALID_SLOT, loan = INVALID_SLOT, output = INVALID_SLOT, type = 0;
    const Callable *target = resolve_call(l, node, &receiver);
    uint32_t depth = 0, first, savedBindings = l->bindingCount, savedFloor = l->bindingFloor;
    uint32_t args[ZR_STRICT_MAX_FIELDS];
    uint8_t kind = 0;
    TZrBool savedTermination = l->terminated, ok = ZR_FALSE;
    const char *error = "call target is not a statically resolved StrictNoGC function or method";
    if (!target || !invocation || invocation->accessMode != ZR_POSTFIX_ACCESS_DIRECT) goto fail;
    if (invocation->hasNamedArgs || count(invocation->genericArguments) || count(invocation->args) != count(target->params)) {
        error = "strict calls require matching positional arguments";
        goto fail;
    }
    for (const CallScope *outer = l->call; outer; outer = outer->outer) {
        if (outer->target == target || ++depth >= 64) {
            error = "recursive or deeply nested strict calls require a separate executable frame";
            goto fail;
        }
    }
    if (!void_result(target->output)) {
        kind = binding_type(l, target->output, &type);
        if (!kind || is_borrow(kind)) {
            error = "strict calls must return a fixed value or owner; a scoped view cannot escape";
            goto fail;
        }
        output = new_slot(l, kind, type, node);
        if (output == INVALID_SLOT) return ZR_FALSE;
    } else if (needResult) {
        error = "void strict call has no value";
        goto fail;
    }
    first = l->program.slotCount;
    if (receiver != INVALID_SLOT) {
        loan = new_slot(l, target->receiver, target->layout, node);
        if (loan == INVALID_SLOT || !emit(l, node, ZR_STRICT_OP_BORROW, loan, receiver, 0)) return ZR_FALSE;
    }
    for (size_t index = 0; index < count(target->params); ++index) {
        const SZrParameter *parameter = &target->params->nodes[index]->data.parameter;
        const SZrCallArgumentSyntax *marker = invocation->argumentMarkers && index < invocation->argumentMarkers->length
                                             ? ZrCore_Array_Get(invocation->argumentMarkers, index) : NULL;
        uint32_t layout = 0, input;
        kind = parameter_kind(l, parameter, &layout);
        if (!kind || (marker && (marker->marker == ZR_CALL_ARGUMENT_MARKER_OUT ||
                                 (!is_borrow(kind) && marker->marker != ZR_CALL_ARGUMENT_MARKER_NONE)))) {
            error = "unsupported strict argument or parameter type";
            goto fail;
        }
        input = lower_expression(l, invocation->args->nodes[index], kind);
        if (input == INVALID_SLOT) return ZR_FALSE;
        if (is_borrow(kind)) {
            uint8_t inferred = l->program.kinds[input];
            TZrBool marked = marker && marker->marker == ZR_CALL_ARGUMENT_MARKER_REF;
            TZrBool owner = byte_view(kind) ? inferred == ZR_STRICT_VALUE_MANUAL_BLOCK :
                (inferred == ZR_STRICT_VALUE_RECORD || inferred == ZR_STRICT_VALUE_UNIQUE || inferred == ZR_STRICT_VALUE_SHARED);
            if (l->program.slotLayouts[input] == layout && (inferred == kind || (marked && owner) ||
                (kind == ZR_STRICT_VALUE_READONLY_BORROW && inferred == ZR_STRICT_VALUE_BORROW) ||
                (kind == ZR_STRICT_VALUE_READONLY_BYTE_VIEW && inferred == ZR_STRICT_VALUE_BYTE_VIEW))) {
                args[index] = new_slot(l, kind, layout, node);
                if (args[index] == INVALID_SLOT || !emit(l, node, ZR_STRICT_OP_BORROW, args[index], input, 0)) return ZR_FALSE;
                continue;
            }
        } else if (match_slot(l, input, kind, layout)) {
            args[index] = new_slot(l, kind, layout, node);
            if (args[index] == INVALID_SLOT || !emit(l, node, assignment_op(l, kind, layout), args[index], input, 0)) return ZR_FALSE;
            continue;
        }
        error = "argument does not match the strict parameter type";
        goto fail;
    }

    CallScope scope = {l->call, target, output, first, 0};
    l->call = &scope;
    l->bindingFloor = savedBindings;
    l->terminated = ZR_FALSE;
    ++l->depth;
    if (loan != INVALID_SLOT && !bind(l, node, "this", loan)) goto done;
    for (size_t index = 0; index < count(target->params); ++index) {
        if (!bind(l, node, name_of(target->params->nodes[index]->data.parameter.name), args[index])) goto done;
    }
    if (!lower_block(l, target->body)) goto done;
    if (!l->terminated) {
        if (output != INVALID_SLOT) {
            (void)refuse(l, node, "every continuing path through a strict call must return its declared value");
            goto done;
        }
        if (!release_slots(l, node, first)) goto done;
    }
    while (scope.exits) {
        ZrStrictInstruction *jump = &l->program.instructions[scope.exits - 1];
        scope.exits = (uint32_t)jump->immediate;
        jump->immediate = l->program.instructionCount;
    }
    if (output != INVALID_SLOT && l->program.kinds[output] >= ZR_STRICT_VALUE_RECORD) l->live[output] = 1;
    *result = output;
    ok = ZR_TRUE;
done:
    --l->depth;
    l->call = scope.outer;
    l->bindingFloor = savedFloor;
    l->bindingCount = savedBindings;
    l->terminated = savedTermination;
    return ok;
fail:
    (void)refuse(l, node, error);
    return ZR_FALSE;
}

static TZrBool lower_call_return(Lowerer *l, const SZrAstNode *node) {
    uint32_t out = l->call->result;
    const SZrAstNode *value = node->data.returnStatement.expr;
    if (out != INVALID_SLOT && value) {
        uint8_t kind = l->program.kinds[out];
        uint32_t input = lower_expression(l, value, kind);
        if (input == INVALID_SLOT) return ZR_FALSE;
        if (!match_slot(l, input, kind, l->program.slotLayouts[out])) goto mismatch;
        if (!emit(l, node, assignment_op(l, kind, l->program.slotLayouts[out]), out, input, 0)) return ZR_FALSE;
    } else if (out != INVALID_SLOT || value) {
        goto mismatch;
    }
    if (!release_slots(l, node, l->call->firstLocal) ||
        !emit(l, node, ZR_STRICT_OP_JUMP, 0, 0, l->call->exits)) return ZR_FALSE;
    l->call->exits = l->program.instructionCount;
    l->terminated = ZR_TRUE;
    return ZR_TRUE;
mismatch:
    (void)refuse(l, node, "return does not match the strict callable signature");
    return ZR_FALSE;
}

static TZrBool lower_statement(Lowerer *l, const SZrAstNode *node) {
    uint32_t input, dst, type = 0, field;
    const SZrAstNode *expr = node->type == ZR_AST_EXPRESSION_STATEMENT ? node->data.expressionStatement.expr : node;
    if (l->terminated) {
        (void)refuse(l, node, "unreachable statement after unconditional strict exit");
        return ZR_FALSE;
    }
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
                uint32_t op = assignment_op(l, kind, l->program.slotLayouts[input]);
                dst = new_slot(l, kind, l->program.slotLayouts[input], node);
                if (dst == INVALID_SLOT || !emit(l, node, op, dst, input, 0)) return ZR_FALSE;
            }
            return bind(l, node, identifier(decl->pattern), dst);
        }
        case ZR_AST_BLOCK: return lower_block(l, expr);
        case ZR_AST_IF_EXPRESSION:
            if (expr->data.ifExpression.isStatement) return lower_if(l, expr);
            break;
        case ZR_AST_WHILE_LOOP:
            if (expr->data.whileLoop.isStatement) return lower_while(l, expr);
            break;
        case ZR_AST_RETURN_STATEMENT:
            if (expr->data.returnStatement.isReferenceReturn) break;
            if (l->call) return lower_call_return(l, expr);
            if (!expr->data.returnStatement.expr) {
                l->terminated = ZR_TRUE;
                return emit(l, node, ZR_STRICT_OP_HALT, 0, 0, 0);
            }
            input = lower_expression(l, expr->data.returnStatement.expr, 0);
            l->terminated = ZR_TRUE;
            return input != INVALID_SLOT && emit(l, node, ZR_STRICT_OP_RETURN, 0, input, 0);
        case ZR_AST_PRIMARY_EXPRESSION: {
            uint32_t first = l->program.slotCount;
            if (!final_call(expr)) break;
            return lower_call(l, expr, ZR_FALSE, &input) && release_slots(l, node, first);
        }
        case ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION:
            if (expr->data.ownershipIntrinsicExpression.operation != ZR_OWNERSHIP_INTRINSIC_DROP) break;
            input = lower_expression(l, expr->data.ownershipIntrinsicExpression.argument, 0);
            return input != INVALID_SLOT && emit(l, node, ZR_STRICT_OP_DROP, 0, input, 0);
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            const SZrMemberExpression *member;
            if (!same(expr->data.assignmentExpression.op.op, "=")) break;
            member = manual_view_member(l, expr->data.assignmentExpression.left, &dst);
            if (member) return lower_byte_write(l, expr, dst, member);
            field = resolve_field(l, expr->data.assignmentExpression.left, &dst);
            if (field == INVALID_SLOT) return ZR_FALSE;
            const ZrStrictRecordLayout *layout = &l->program.layouts[l->program.slotLayouts[dst] - 1];
            uint8_t kind = layout->fields[field];
            uint32_t fieldType = layout->fieldLayouts[field];
            input = lower_expression(l, expr->data.assignmentExpression.right, kind);
            if (input == INVALID_SLOT) return ZR_FALSE;
            if (!match_slot(l, input, kind, fieldType)) {
                (void)refuse(l, expr, "assigned value does not match the strict field type");
                return ZR_FALSE;
            }
            if (kind == ZR_STRICT_VALUE_SHARED || kind == ZR_STRICT_VALUE_WEAK) {
                uint32_t copy = new_slot(l, kind, fieldType, node);
                if (copy == INVALID_SLOT || !emit(l, node, ZR_STRICT_OP_CLONE, copy, input, 0)) return ZR_FALSE;
                input = copy;
            }
            return emit(l, node, ZR_STRICT_OP_FIELD_WRITE, dst, input, field);
        }
        default: break;
    }
    (void)refuse(l, expr, "statement has no verified StrictNoGC lowering");
    return ZR_FALSE;
}

static TZrBool add_callable(Lowerer *l, const SZrAstNode *node, uint32_t layout) {
    Callable call = {.layout = layout};
    EZrMethodReceiverModifier receiver = ZR_METHOD_RECEIVER_DEFAULT;
    TZrBool isStatic = ZR_TRUE, implicitReadonly = ZR_FALSE;
    if (node->type == ZR_AST_FUNCTION_DECLARATION && !layout) {
        const SZrFunctionDeclaration *f = &node->data.functionDeclaration;
        if (f->generic || f->args || f->isAsync || count(f->decorators)) goto unsupported;
        call = (Callable){name_of(f->name), f->params, f->returnType, f->body, 0, 0, ZR_TRUE};
    } else if (node->type == ZR_AST_CLASS_METHOD && layout) {
        const SZrClassMethod *m = &node->data.classMethod;
        if (m->generic || m->args || m->isAsync || m->receiverQualifier || count(m->decorators) ||
            (m->modifierFlags & ~((TZrUInt32)ZR_DECLARATION_MODIFIER_FINAL))) goto unsupported;
        call = (Callable){name_of(m->name), m->params, m->returnType, m->body, layout, 0, m->access == ZR_ACCESS_PUBLIC};
        isStatic = m->isStatic;
        receiver = m->receiverModifier;
    } else if (node->type == ZR_AST_STRUCT_METHOD && layout) {
        const SZrStructMethod *m = &node->data.structMethod;
        if (m->generic || m->args || m->isAsync || m->receiverQualifier || count(m->decorators)) goto unsupported;
        call = (Callable){name_of(m->name), m->params, m->returnType, m->body, layout, 0, m->access == ZR_ACCESS_PUBLIC};
        isStatic = m->isStatic;
        receiver = m->receiverModifier;
        implicitReadonly = m->isImplicitReadonlyReceiver;
    } else goto unsupported;

    if (!isStatic) call.receiver = implicitReadonly || receiver == ZR_METHOD_RECEIVER_CONST
                                      ? ZR_STRICT_VALUE_READONLY_BORROW : ZR_STRICT_VALUE_BORROW;
    if (!call.name || !call.body || call.body->type != ZR_AST_BLOCK || count(call.params) > ZR_STRICT_MAX_FIELDS ||
        l->callableCount == ZR_STRICT_MAX_SLOTS || find_callable(l, layout, call.name)) goto unsupported;
    for (size_t index = 0; index < count(call.params); ++index) {
        const SZrAstNode *arg = call.params->nodes[index];
        const SZrParameter *param;
        const char *name;
        if (!arg || arg->type != ZR_AST_PARAMETER) goto unsupported;
        param = &arg->data.parameter;
        name = name_of(param->name);
        if (!name || same(name, "this") || !param->typeInfo || param->defaultValue || count(param->decorators) ||
            (param->passingMode != ZR_PARAMETER_PASSING_MODE_VALUE && param->passingMode != ZR_PARAMETER_PASSING_MODE_REF))
            goto unsupported;
        for (size_t prior = 0; prior < index; ++prior)
            if (same(name, name_of(call.params->nodes[prior]->data.parameter.name))) goto unsupported;
    }
    l->callables[l->callableCount++] = call;
    return ZR_TRUE;
unsupported:
    (void)refuse(l, node, "strict call declarations require a unique, statically resolved signature");
    return ZR_FALSE;
}

/* Call bodies that are never referenced do not reach executable lowering.
 * Still reject explicit managed value forms there, before discarding the body. */
static TZrBool check_callable_value_form(const SZrAstNode *node, void *context) {
    switch (node->type) {
        case ZR_AST_STRING_LITERAL: case ZR_AST_TEMPLATE_STRING_LITERAL:
        case ZR_AST_ARRAY_LITERAL: case ZR_AST_OBJECT_LITERAL: case ZR_AST_LAMBDA_EXPRESSION:
        case ZR_AST_TYPE_QUERY_EXPRESSION: case ZR_AST_GENERATOR_EXPRESSION:
        case ZR_AST_YIELD_STATEMENT: case ZR_AST_AWAIT_EXPRESSION:
            (void)refuse(context, node, "strict callable contains a managed or unsupported value form");
            return ZR_FALSE;
        default: return ZR_TRUE;
    }
}

static TZrBool check_callable_types(Lowerer *l, const Callable *call) {
    uint32_t layout = 0;
    if (!void_result(call->output)) {
        uint8_t kind = binding_type(l, call->output, &layout);
        if (!kind || is_borrow(kind)) {
            (void)refuse(l, call->body, "strict calls must return a fixed value or owner; a scoped view cannot escape");
            return ZR_FALSE;
        }
    }
    for (size_t index = 0; index < count(call->params); ++index) {
        const SZrAstNode *param = call->params->nodes[index];
        if (!parameter_kind(l, &param->data.parameter, &layout)) {
            (void)refuse(l, param, "unsupported strict parameter type");
            return ZR_FALSE;
        }
    }
    if (!ZrParser_Ast_Visit(call->body, check_callable_value_form, l)) {
        (void)refuse(l, call->body, "could not verify strict call body");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static const char *record_name(const SZrAstNode *node) {
    return name_of(node->type == ZR_AST_CLASS_DECLARATION ? node->data.classDeclaration.name
                                                        : node->data.structDeclaration.name);
}

static TZrBool register_record(Lowerer *l, const SZrAstNode *node) {
    const char *name = record_name(node);
    if (!name || same(name, "ManualBlock") || strlen(name) >= ZR_STRICT_LAYOUT_NAME_SIZE || layout_id(l, name) ||
        l->program.layoutCount == ZR_STRICT_MAX_LAYOUTS) {
        (void)refuse(l, node, "invalid or duplicate strict record name");
        return ZR_FALSE;
    }
    memcpy(l->program.layouts[l->program.layoutCount++].name, name, strlen(name) + 1);
    return ZR_TRUE;
}

static TZrBool add_record(Lowerer *l, const SZrAstNode *node) {
    TZrBool resource = node->type == ZR_AST_CLASS_DECLARATION;
    const SZrAstNodeArray *members;
    const uint32_t layoutIndex = layout_id(l, record_name(node));
    TZrBool readonly = ZR_FALSE;
    ZrStrictRecordLayout *layout = &l->program.layouts[layoutIndex - 1];
    RecordModel *model = &l->models[layoutIndex - 1];
    if (resource) {
        const SZrClassDeclaration *decl = &node->data.classDeclaration;
        if (!decl->isOwned || decl->generic || count(decl->inherits) || count(decl->decorators)) goto unsupported;
        members = decl->members;
    } else {
        const SZrStructDeclaration *decl = &node->data.structDeclaration;
        if (decl->isRefLike || decl->generic || count(decl->inherits) || count(decl->decorators)) goto unsupported;
        members = decl->members;
        readonly = decl->isReadonly;
    }
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
        } else if (resource && member->type == ZR_AST_CLASS_META_FUNCTION &&
                   same(name_of(member->data.classMetaFunction.meta), "destructor")) {
            const SZrClassMetaFunction *m = &member->data.classMetaFunction;
            if (model->destructor || m->isStatic || m->hasSuperCall || count(m->params) || m->args ||
                !void_result(m->returnType) || !m->body || m->body->type != ZR_AST_BLOCK) goto unsupported;
            model->destructor = m->body;
            continue;
        } else if ((resource && member->type == ZR_AST_CLASS_METHOD) || (!resource && member->type == ZR_AST_STRUCT_METHOD)) {
            if (!add_callable(l, member, layoutIndex)) return ZR_FALSE;
            continue;
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
                uint32_t paramLayout = 0;
                if (!arg || arg->type != ZR_AST_PARAMETER ||
                    !field_kind(binding_type(l, arg->data.parameter.typeInfo, &paramLayout)) ||
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
        uint32_t fieldType = 0;
        kind = binding_type(l, type, &fieldType);
        if (!field_kind(kind) || !fieldName || layout->fieldCount == ZR_STRICT_MAX_FIELDS ||
            field_id(model, layout->fieldCount, fieldName) != INVALID_SLOT ||
            (init && !((kind == ZR_STRICT_VALUE_I64 && init->type == ZR_AST_INTEGER_LITERAL) ||
                       (kind == ZR_STRICT_VALUE_BOOL && init->type == ZR_AST_BOOLEAN_LITERAL)))) goto unsupported;
        uint32_t field = layout->fieldCount++;
        model->fields[field] = fieldName;
        model->defaults[field] = init;
        layout->fields[field] = kind;
        layout->fieldLayouts[field] = fieldType;
        if (isConst || readonly) layout->readonlyMask |= 1u << field;
        if (access == ZR_ACCESS_PUBLIC) model->publicMask |= 1u << field;
    }
    for (uint32_t field = 0; field < layout->fieldCount; ++field)
        if (find_callable(l, layoutIndex, model->fields[field])) goto unsupported;
    return ZR_TRUE;
unsupported:
    (void)refuse(l, node, "strict record requires fixed scalar or owner fields and a statically checked constructor");
    return ZR_FALSE;
}

static TZrBool lower_destructor(Lowerer *program, uint32_t layout) {
    SZrAstNode *body = program->models[layout - 1].destructor;
    if (!body) return ZR_TRUE;
    Lowerer l = {.models = program->models, .bindings = program->bindings, .callables = program->callables,
                 .callableCount = program->callableCount, .inDestructor = ZR_TRUE, .path = program->path,
                 .error = program->error, .errorSize = program->errorSize};
    Callable target = {.name = "destructor", .body = body, .layout = layout, .receiver = ZR_STRICT_VALUE_BORROW};
    CallScope scope = {NULL, &target, INVALID_SLOT, 1, 0};
    TZrBool ok = ZR_FALSE;
    const uint32_t limit = ZR_STRICT_MAX_DESTRUCTOR_SLOTS;
    l.program.kinds = calloc(limit, sizeof(*l.program.kinds));
    l.program.slotLayouts = calloc(limit, sizeof(*l.program.slotLayouts));
    l.program.layouts = program->program.layouts;
    l.program.layoutCount = program->program.layoutCount;
    l.call = &scope;
    if (!l.program.kinds || !l.program.slotLayouts ||
        !ZrParser_Ast_Visit(body, check_callable_value_form, &l)) goto done;
    uint32_t receiver = new_slot(&l, ZR_STRICT_VALUE_RECORD, layout, body);
    if (receiver == INVALID_SLOT || !bind(&l, body, "this", receiver)) goto done;
    l.live[receiver] = 1; /* the runtime lends the payload for cleanup */
    if (!lower_block(&l, body)) goto done;
    while (scope.exits) {
        ZrStrictInstruction *exit = &l.program.instructions[scope.exits - 1];
        scope.exits = (uint32_t)exit->immediate;
        exit->immediate = l.program.instructionCount;
    }
    if (!emit(&l, body, ZR_STRICT_OP_HALT, 0, 0, 0)) goto done;
    program->program.layouts[layout - 1].destructor = (ZrStrictDestructor){l.program.kinds, l.program.slotLayouts,
        l.program.instructions, l.program.slotCount, l.program.instructionCount};
    ok = ZR_TRUE;
done:
    if (!ok) {
        (void)refuse(&l, body, "could not lower strict destructor");
        free(l.program.kinds);
        free(l.program.slotLayouts);
        free(l.program.instructions);
    }
    free(l.locations);
    return ok;
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
    l.callables = calloc(ZR_STRICT_MAX_SLOTS, sizeof(*l.callables));
    if (!l.program.kinds || !l.program.slotLayouts || !l.program.layouts || !l.models || !l.bindings || !l.callables) goto done;
    statements = root->data.script.statements;
    for (size_t index = 0; index < count(statements); ++index) {
        const SZrAstNode *node = statements->nodes[index];
        if ((node->type == ZR_AST_STRUCT_DECLARATION || node->type == ZR_AST_CLASS_DECLARATION) &&
            !register_record(&l, node)) goto done;
    }
    for (size_t index = 0; index < count(statements); ++index) {
        const SZrAstNode *node = statements->nodes[index];
        if ((node->type == ZR_AST_STRUCT_DECLARATION || node->type == ZR_AST_CLASS_DECLARATION) &&
            !add_record(&l, node)) goto done;
        if (node->type == ZR_AST_FUNCTION_DECLARATION && !add_callable(&l, node, 0)) goto done;
    }
    for (uint32_t index = 0; index < l.callableCount; ++index)
        if (!check_callable_types(&l, &l.callables[index])) goto done;
    for (uint32_t layout = 1; layout <= l.program.layoutCount; ++layout)
        if (!lower_destructor(&l, layout)) goto done;
    for (size_t index = 0; index < count(statements); ++index) {
        const SZrAstNode *node = statements->nodes[index];
        if (node->type == ZR_AST_STRUCT_DECLARATION || node->type == ZR_AST_CLASS_DECLARATION ||
            node->type == ZR_AST_MODULE_DECLARATION || node->type == ZR_AST_FUNCTION_DECLARATION) continue;
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
    free(l.callables);
    free(l.locations);
    ZrStrictProgram_Free(&l.program);
    ZrParser_Ast_Free(state, root);
    return success;
}
