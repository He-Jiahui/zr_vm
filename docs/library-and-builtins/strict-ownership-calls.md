# Calling functions and methods in StrictNoGC

An explicit owner can cross a call boundary. A parameter or result of type `Unique<T>` transfers ownership. Parameters of type `Shared<T>` or `Weak<T>` copy the handle and keep the original binding usable. A `ref` or `ref readonly` parameter borrows the caller's value for the call. It cannot be returned. Argument borrows end when the call returns, and the verifier checks that writable argument and receiver loans do not overlap.

```zr
resource class Student {
    var id: int;
    pub @constructor(id: int) { this.id = id; }
    pub const fn score(): int { return this.id; }
}

fn TakeStudent(id: int): Unique<Student> { return own Student(id); }
fn Score(student: ref readonly Student): int { return student.score(); }

let owner = TakeStudent(42);
let result = Score(ref owner);
drop(owner);
return result;
```

A mutable method borrows its receiver exclusively. A `const` method borrows it for reading and can be called through `Shared<T>`. Static methods have no receiver. The strict compiler currently inlines the statically resolved body; callable values, dynamic dispatch, and recursion do not use this path.

A strict resource class can declare `@destructor()`. The runtime invokes it when a `Unique<T>` is dropped or the last `Shared<T>` is released, including during VM fault cleanup. A weak observer does not keep the payload alive. Destructors can use `this`, inline values, and statically resolved methods, but neither the destructor nor a function it calls can allocate. If both the program and a destructor fault, the original program fault is reported and cleanup continues for other live owners.

For fixed scalar layouts, construction evaluates the field initializers before publishing the new record. A constructor may read fields it has already initialized. If an initializer faults, the new value never becomes an owner and its destructor does not run. Previously live owners still unwind.
