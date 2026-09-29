# Source Generic Instance Member Inference

## Scope

- Keep `new Box<int>()` and `new Derived<int>()` as instance receivers during member type inference.
- Preserve closed generic field substitution on the class and inherited-field paths.
- Preserve prototype-member inference for enum type roots.

## Baseline

- Before the production guard, the strengthened type-inference suite reported 125 tests and 2
  failures. The variable-instance field controls passed. The direct `Box<int>` and `Derived<int>`
  reads failed with `Instance field requires an instance receiver` at their field access locations.
- Both failures occurred before field-type substitution. The inferred closed type name was being
  treated as a prototype receiver.

## Test Inventory

- `test_type_inference_source_generic_class_member_substitutes_closed_field_type`: named and
  temporary `Box<int>` field reads return `int64`.
- `test_type_inference_source_generic_inheritance_substitutes_closed_base_member_type`: named and
  temporary `Derived<int>` reads of `Base<T>.value` return `int64`; the closed prototype retains
  `Base<int>` as its base.
- `test_type_inference_source_generic_method_supports_explicit_arguments_inference_and_receiver_closure`:
  named generic instance receiver remains closed to `Box<int>`.
- Native and source extern enum member tests preserve prototype receiver behavior and enum type.

## Tooling Evidence

- WSL GCC build, using the existing D cache:
  `cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_type_inference_test -j8`
  completed successfully after the source guard; Ninja rebuilt the parser source, parser library,
  and focused test executable.
- The direct suite command was:
  `/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_type_inference_test`
- `ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -N -R type_inference` found no registered
  CTest case in this cache, so validation used the direct Unity executable.

## Results

- RED: 125 tests, 2 failures; both failures reported `Instance field requires an instance receiver`.
- GREEN: 125 tests, 0 failures, 0 ignored.
- The generic class field, generic inherited field, generic receiver-method, native enum, and source
  extern enum cases all passed in the GREEN run.
- The compiler emitted existing `ZrLibParameterDescriptor.passingMode` missing-initializer warnings
  while rebuilding `test_type_inference.c`; the build succeeded.

## Acceptance Decision

- Accepted for this focused parser type-inference scope: the direct construction regressions and
  named-instance controls pass, and existing prototype enum controls remain green.
- No production access-control or generic substitution logic changed; the receiver classifier now
  leaves constructed expressions on the instance path.
