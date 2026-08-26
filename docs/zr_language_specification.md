# ZR Current Language Syntax

> Status: current. `README.md` is the user-facing authority. This document
> records the production grammar boundary used by the parser, tooling, and
> current syntax reference project.

## Source Structure

Modules use an unquoted module path and explicit statement terminators:

```zr
module app.main;
let system = import("zr.system");
```

`import(...)` is a dedicated static-import expression. It accepts one string
literal and is normally bound by a module-scope `let`. Package, registered
native, alias, artifact, and file-locator forms live inside that literal; they
are not language keywords.

## Functions And Callable Types

All definitions use `fn` and introduce the return type with `:`:

```zr
fn add(left: int, right: int): int {
    return left + right;
}
```

Callable types use `->`; expression-bodied anonymous functions use `=>`:

```zr
let transform: fn(int) -> int = fn(value: int): int => value + 1;
```

Parameter contracts put the passing form in the TypeRef:

```zr
fn read(value: in int): int { return value; }
fn update(value: ref int): void { }
fn inspect(value: ref readonly int): int { return value; }
fn produce(value: out int): void { }
```

## Types, Construction, And Ownership

Value, GC object, and resource construction are distinct:

```zr
let point = init Point(1, 2);
let object = new Widget();
let owner = own FileHandle();
```

`Unique<T>`, `Shared<T>`, and `Weak<T>` are the canonical owner types. Scoped
access uses `ref T` or `ref readonly T`. Ownership control uses only the
reserved intrinsic calls `share(owner)`, `degrade(shared)`, `wake(weak)`,
`intoGc(owner)`, and `drop(owner)`. The `.` and `?.` operators exclusively
access the receiver target. Direct access to an absent nullable/Weak target
throws `NullReferenceError`; optional access returns null or performs a void
no-op and skips the guarded suffix.

Postfix target access and calls use these forms:

```zr
receiver.member
receiver.method(args)
receiver?.member
receiver?.method(args)
callable(args)
callable?.(args)
```

`?.(` is the optional-call segment. There is no `callable.(args)` form. An
optional receiver or callable guard skips the complete remaining postfix suffix,
including argument evaluation, when its target is absent.

Runtime type construction never reuses static construction syntax. It goes
through `zr.reflection`:

```zr
let reflection = import("zr.reflection");
let constructible = reflection.requireConstructible(runtimeType);
let value = constructible.createInstance(...arguments);
```

## Properties

Properties use the unified declaration and accessor grammar. Storage remains
an explicit field:

```zr
class Counter {
    pri var stored: int = 0;

    pub property value: int {
        pub get { return this.stored; }
        pri set { this.stored = value; }
    }
}
```

## Native, Compile-Time, Async, Iteration, And Tests

Native declarations use `native extern`:

```zr
native extern("math") {
    fn add(left: i32, right: i32): i32;
}
```

Compile-time execution uses `comptime` plus typed `zr.compile` metadata. It
does not use a runtime decorator or a `%compileTime` directive.

Async and iterator functions declare their real carrier types:

```zr
let task = import("zr.task");
let iteration = import("zr.iteration");

async fn work(): task.Task<int> { return 7; }
fn values(): iteration.Iterator<int> { yield 7; }
```

Tests are ordinary functions with `zr.testing` metadata:

```zr
let testing = import("zr.testing");

#zr.testing.test#
fn examplePasses(): void {
    testing.assert(true);
}
```

## Terminators And Percent Operator

Newlines are trivia and never insert semicolons. Simple declarations,
bindings, expressions, assignments, `return`, `throw`, `break`, `continue`,
and bodyless declarations require `;`. Braced declarations and compound
control-flow statements are closed by their grammar.

`%` and `%=` remain the modulo and modulo-assignment operators. No
percent-prefixed identifier is a production keyword.

## Removed Source Forms

The following are not accepted by the production parser:

- `%module`, `%import`, `%extern`, `%compileTime`, `%test`, and `%owned`;
- `%borrow`, `%loan`, `%borrowed`, `%loaned`, `%unique`, `%shared`, `%weak`,
  `%using`, `%type`, `%in`, `%out`, `%ref`, and `%func`;
- `func` definitions, keywordless definitions, old return delimiters, and old
  callable-type `=>`;
- `$Type(...)`, `$(runtimeType)(...)`, bare ownership constructors, and old
  generator `out` statements;
- user-authored `intermediate ...` instruction declarations.
- unresolved legacy ownership-member operations such as `owner.share()`,
  `shared.weak()`, `weak.upgrade()`, and `owner.intoGc()` have no ownership
  meaning. A matching real target member remains a legal ordinary member call;
  otherwise use the reserved intrinsic call named by the migration diagnostic.

The lexer/parser may recognize a removed percent-prefixed or grammar spelling
only to emit the fatal `legacy_syntax_removed` diagnostic. That path returns no
production AST and cannot reach semantic lowering, VM/AOT, artifact, CLI, or
LSP execution. A legacy ownership-member spelling is different: it first parses
as ordinary target access, and only a failed real member lookup may publish the
fatal `removed_ownership_member_syntax` diagnostic and its structured intrinsic
edit. That diagnostic cannot select ownership typing or lowering. Repository-
wide batch edits remain the responsibility of the explicit syntax migration
frontend.

## Executable Reference

The current positive and negative source catalog is
`tests/fixtures/projects/syntax_reference_v1`. Its coverage manifest contains
no `design-pending` entries. The application entry imports its host module and
returns checksum `7` in interpreter and binary-first project execution. AOT,
artifact, reflection, pooling, compile-time, async, iterator, testing, and LSP
contracts are additionally enforced by their owner-gate test matrices.
