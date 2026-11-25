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
