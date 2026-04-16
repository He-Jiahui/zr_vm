use std::fs;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

use zr_vm_rust_binding::{
    CompileOptions, ExecutionMode, FunctionBuilder, ModuleBuilder, ProjectWorkspace,
    PrototypeType, RunOptions, RuntimeBuilder, TypeBuilder, Value,
};
use zr_vm_rust_binding_sys::ZrRustBindingStatus;

struct DropProbe {
    counter: Arc<AtomicUsize>,
}

impl Drop for DropProbe {
    fn drop(&mut self) {
        self.counter.fetch_add(1, Ordering::SeqCst);
    }
}

#[test]
fn native_module_registration_roundtrip() -> Result<(), Box<dyn std::error::Error>> {
    let temp = tempfile::tempdir()?;
    let root = temp.path().join("native_module_project");
    let workspace = ProjectWorkspace::scaffold(&root, "native_module_project")?;
    let main_source = workspace.project_root()?.join("src").join("main.zr");
    fs::write(
        &main_source,
        concat!(
            "var host = %import(\"host_demo\");\n",
            "return host.answer + host.bump(2, 3) + host.Counter.mul(4, 5);\n",
        ),
    )?;

    let function_drop_count = Arc::new(AtomicUsize::new(0));
    let method_drop_count = Arc::new(AtomicUsize::new(0));
    let function_drop_probe = DropProbe {
        counter: Arc::clone(&function_drop_count),
    };
    let method_drop_probe = DropProbe {
        counter: Arc::clone(&method_drop_count),
    };

    let module = ModuleBuilder::new("host_demo")
        .documentation("Host demo module.")
        .module_version("1.0.0")
        .int_constant("answer", 100, "int", "Answer constant.")
        .add_function(
            FunctionBuilder::new("bump", 2, 2, move |context| {
                let _probe = &function_drop_probe;
                assert_eq!(context.module_name()?, "host_demo");
                assert_eq!(context.type_name()?, None);
                assert_eq!(context.callable_name()?, "bump");
                assert_eq!(context.argument_count()?, 2);
                context.check_arity(2, 2)?;
                assert!(context.self_value()?.is_none());

                let left = context.argument(0)?.as_int()?;
                let right = context.argument(1)?.as_int()?;
                Value::new_int(left + right)
            })
            .parameter("left", "int", "left operand")
            .parameter("right", "int", "right operand")
            .return_type("int")
            .documentation("Adds two values."),
        )
        .add_type(
            TypeBuilder::new("Counter", PrototypeType::Class)
                .documentation("Host-side native counter helpers.")
                .add_static_method(
                    FunctionBuilder::new("mul", 2, 2, move |context| {
                        let _probe = &method_drop_probe;
                        assert_eq!(context.module_name()?, "host_demo");
                        assert_eq!(context.type_name()?, Some("Counter".to_string()));
                        assert_eq!(context.callable_name()?, "mul");
                        assert_eq!(context.argument_count()?, 2);
                        context.check_arity(2, 2)?;
                        assert!(context.self_value()?.is_none());

                        let left = context.argument(0)?.as_int()?;
                        let right = context.argument(1)?.as_int()?;
                        Value::new_int(left * right)
                    })
                    .parameter("left", "int", "left operand")
                    .parameter("right", "int", "right operand")
                    .return_type("int")
                    .documentation("Multiplies two values."),
                ),
        )
        .build()?;

    let mut runtime = RuntimeBuilder::standard().build()?;
    let registration = runtime.register_native_module(module)?;
    let compile = workspace.compile(
        &mut runtime,
        &CompileOptions {
            emit_intermediate: false,
            incremental: false,
        },
    )?;
    assert!(compile.compiled > 0);

    let result = workspace.run(
        &mut runtime,
        &RunOptions {
            execution_mode: ExecutionMode::Binary,
            ..RunOptions::default()
        },
    )?;
    assert_eq!(result.as_int()?, 125);

    drop(result);
    drop(registration);
    assert_eq!(function_drop_count.load(Ordering::SeqCst), 1);
    assert_eq!(method_drop_count.load(Ordering::SeqCst), 1);
    Ok(())
}

#[test]
fn invalid_native_function_descriptor_returns_error() {
    let error = match ModuleBuilder::new("invalid_demo")
        .add_function(FunctionBuilder::new("bad", 2, 1, |_context| Value::new_null()))
        .build()
    {
        Ok(_) => panic!("invalid arity should fail"),
        Err(error) => error,
    };

    assert_eq!(
        error.status,
        ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INTERNAL_ERROR
    );
}
