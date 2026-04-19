use std::fs;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::{Arc, Mutex, MutexGuard, OnceLock};

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

fn acquire_test_lock() -> MutexGuard<'static, ()> {
    static TEST_LOCK: OnceLock<Mutex<()>> = OnceLock::new();

    // The binding layer still carries process-global C state.
    // Keep Rust tests single-threaded within this test binary.
    TEST_LOCK
        .get_or_init(|| Mutex::new(()))
        .lock()
        .expect("rust binding integration tests should acquire the process-global lock")
}

#[test]
fn native_module_registration_roundtrip() -> Result<(), Box<dyn std::error::Error>> {
    let _guard = acquire_test_lock();
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

    let interp = workspace.run(
        &mut runtime,
        &RunOptions {
            execution_mode: ExecutionMode::Interp,
            ..RunOptions::default()
        },
    )?;
    assert_eq!(interp.as_int()?, 125);
    drop(interp);

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
fn native_module_registration_drop_waits_for_live_result_handles(
) -> Result<(), Box<dyn std::error::Error>> {
    let _guard = acquire_test_lock();
    let temp = tempfile::tempdir()?;
    let root = temp.path().join("native_module_live_result_project");
    let workspace = ProjectWorkspace::scaffold(&root, "native_module_live_result_project")?;
    let main_source = workspace.project_root()?.join("src").join("main.zr");
    fs::write(
        &main_source,
        concat!(
            "var host = %import(\"host_demo\");\n",
            "return host.answer + host.bump(2, 3);\n",
        ),
    )?;

    let function_drop_count = Arc::new(AtomicUsize::new(0));
    let function_drop_probe = DropProbe {
        counter: Arc::clone(&function_drop_count),
    };

    let module = ModuleBuilder::new("host_demo")
        .documentation("Host demo module.")
        .module_version("1.0.0")
        .int_constant("answer", 100, "int", "Answer constant.")
        .add_function(
            FunctionBuilder::new("bump", 2, 2, move |context| {
                let _probe = &function_drop_probe;
                let left = context.argument(0)?.as_int()?;
                let right = context.argument(1)?.as_int()?;
                Value::new_int(left + right)
            })
            .parameter("left", "int", "left operand")
            .parameter("right", "int", "right operand")
            .return_type("int")
            .documentation("Adds two values."),
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
            execution_mode: ExecutionMode::Interp,
            ..RunOptions::default()
        },
    )?;

    drop(registration);
    assert_eq!(function_drop_count.load(Ordering::SeqCst), 0);
    assert_eq!(result.as_int()?, 105);

    drop(result);
    assert_eq!(function_drop_count.load(Ordering::SeqCst), 1);
    Ok(())
}

#[test]
fn native_module_reregistration_replaces_active_entry_without_invalidating_live_results(
) -> Result<(), Box<dyn std::error::Error>> {
    let _guard = acquire_test_lock();
    let temp = tempfile::tempdir()?;
    let root = temp.path().join("native_module_overlap_reregister_project");
    let workspace = ProjectWorkspace::scaffold(&root, "native_module_overlap_reregister_project")?;
    let main_source = workspace.project_root()?.join("src").join("main.zr");
    fs::write(
        &main_source,
        concat!(
            "var host = %import(\"host_demo\");\n",
            "return host.bump(0, 0);\n",
        ),
    )?;

    let build_module = |answer: i64, counter: Arc<AtomicUsize>| {
        let probe = DropProbe { counter };
        ModuleBuilder::new("host_demo")
            .documentation("Host demo module.")
            .module_version("1.0.0")
            .add_function(
                FunctionBuilder::new("bump", 2, 2, move |context| {
                    let _probe = &probe;
                    context.check_arity(2, 2)?;
                    Value::new_int(answer)
                })
                .parameter("left", "int", "left operand")
                .parameter("right", "int", "right operand")
                .return_type("int")
                .documentation("Returns the active registration generation."),
            )
            .build()
    };

    let first_drop_count = Arc::new(AtomicUsize::new(0));
    let second_drop_count = Arc::new(AtomicUsize::new(0));

    let mut runtime = RuntimeBuilder::standard().build()?;
    let first_registration =
        runtime.register_native_module(build_module(100, Arc::clone(&first_drop_count))?)?;
    let compile = workspace.compile(
        &mut runtime,
        &CompileOptions {
            emit_intermediate: false,
            incremental: false,
        },
    )?;
    assert!(compile.compiled > 0);

    let first_result = workspace.run(
        &mut runtime,
        &RunOptions {
            execution_mode: ExecutionMode::Interp,
            ..RunOptions::default()
        },
    )?;
    assert_eq!(first_result.as_int()?, 100);

    drop(first_registration);
    assert_eq!(first_drop_count.load(Ordering::SeqCst), 0);

    let second_registration =
        runtime.register_native_module(build_module(250, Arc::clone(&second_drop_count))?)?;
    let second_result = workspace.run(
        &mut runtime,
        &RunOptions {
            execution_mode: ExecutionMode::Interp,
            ..RunOptions::default()
        },
    )?;
    assert_eq!(second_result.as_int()?, 250);

    assert_eq!(first_result.as_int()?, 100);
    drop(first_result);
    assert_eq!(first_drop_count.load(Ordering::SeqCst), 1);
    assert_eq!(second_drop_count.load(Ordering::SeqCst), 0);

    drop(second_registration);
    assert_eq!(second_drop_count.load(Ordering::SeqCst), 0);
    assert_eq!(second_result.as_int()?, 250);

    drop(second_result);
    assert_eq!(second_drop_count.load(Ordering::SeqCst), 1);
    Ok(())
}

#[test]
fn native_module_registration_drop_allows_re_registration() -> Result<(), Box<dyn std::error::Error>> {
    let _guard = acquire_test_lock();
    let temp = tempfile::tempdir()?;
    let root = temp.path().join("native_module_reregister_project");
    let workspace = ProjectWorkspace::scaffold(&root, "native_module_reregister_project")?;
    let main_source = workspace.project_root()?.join("src").join("main.zr");
    fs::write(
        &main_source,
        concat!(
            "var host = %import(\"host_demo\");\n",
            "return host.answer;\n",
        ),
    )?;

    let build_module = |answer| {
        ModuleBuilder::new("host_demo")
            .documentation("Host demo module.")
            .module_version("1.0.0")
            .int_constant("answer", answer, "int", "Answer constant.")
            .build()
    };

    let mut runtime = RuntimeBuilder::standard().build()?;
    let first_registration = runtime.register_native_module(build_module(100)?)?;

    let duplicate_error = match runtime.register_native_module(build_module(200)?) {
        Ok(_) => panic!("duplicate native module registration should fail"),
        Err(error) => error,
    };
    assert_eq!(
        duplicate_error.status,
        ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_ALREADY_EXISTS
    );
    assert!(
        duplicate_error
            .message
            .contains("native module already registered on runtime")
            || duplicate_error.message.contains("ALREADY_EXISTS")
    );

    let compile = workspace.compile(
        &mut runtime,
        &CompileOptions {
            emit_intermediate: false,
            incremental: false,
        },
    )?;
    assert!(compile.compiled > 0);

    let first_interp = workspace.run(
        &mut runtime,
        &RunOptions {
            execution_mode: ExecutionMode::Interp,
            ..RunOptions::default()
        },
    )?;
    assert_eq!(first_interp.as_int()?, 100);
    drop(first_interp);

    let first_binary = workspace.run(
        &mut runtime,
        &RunOptions {
            execution_mode: ExecutionMode::Binary,
            ..RunOptions::default()
        },
    )?;
    assert_eq!(first_binary.as_int()?, 100);
    drop(first_binary);

    drop(first_registration);

    let second_registration = runtime.register_native_module(build_module(250)?)?;

    let second_interp = workspace.run(
        &mut runtime,
        &RunOptions {
            execution_mode: ExecutionMode::Interp,
            ..RunOptions::default()
        },
    )?;
    assert_eq!(second_interp.as_int()?, 250);
    drop(second_interp);

    let second_binary = workspace.run(
        &mut runtime,
        &RunOptions {
            execution_mode: ExecutionMode::Binary,
            ..RunOptions::default()
        },
    )?;
    assert_eq!(second_binary.as_int()?, 250);
    drop(second_binary);
    drop(second_registration);
    Ok(())
}

#[test]
fn native_module_unregistration_removes_visibility_from_run_and_export_paths(
) -> Result<(), Box<dyn std::error::Error>> {
    let _guard = acquire_test_lock();
    let temp = tempfile::tempdir()?;
    let root = temp.path().join("native_module_unregister_project");
    let workspace = ProjectWorkspace::scaffold(&root, "native_module_unregister_project")?;
    let main_source = workspace.project_root()?.join("src").join("main.zr");
    fs::write(
        &main_source,
        concat!(
            "var host = %import(\"host_demo\");\n",
            "\n",
            "pub replay(): int {\n",
            "    return host.answer + host.bump(2, 3);\n",
            "}\n",
            "\n",
            "return replay();\n",
        ),
    )?;

    let call_count = Arc::new(AtomicUsize::new(0));
    let callback_calls = Arc::clone(&call_count);
    let module = ModuleBuilder::new("host_demo")
        .documentation("Host demo module.")
        .module_version("1.0.0")
        .int_constant("answer", 100, "int", "Answer constant.")
        .add_function(
            FunctionBuilder::new("bump", 2, 2, move |context| {
                callback_calls.fetch_add(1, Ordering::SeqCst);
                let left = context.argument(0)?.as_int()?;
                let right = context.argument(1)?.as_int()?;
                Value::new_int(left + right)
            })
            .parameter("left", "int", "left operand")
            .parameter("right", "int", "right operand")
            .return_type("int")
            .documentation("Adds two values."),
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

    let interp = workspace.run(
        &mut runtime,
        &RunOptions {
            execution_mode: ExecutionMode::Interp,
            ..RunOptions::default()
        },
    )?;
    assert_eq!(interp.as_int()?, 105);
    drop(interp);

    let binary = workspace.run(
        &mut runtime,
        &RunOptions {
            execution_mode: ExecutionMode::Binary,
            ..RunOptions::default()
        },
    )?;
    assert_eq!(binary.as_int()?, 105);
    drop(binary);

    let export = workspace.call_module_export(
        &mut runtime,
        &RunOptions::default(),
        "main",
        "replay",
        &[],
    )?;
    assert_eq!(export.as_int()?, 105);
    drop(export);

    // call_module_export runs in a fresh project runtime. Loading `main`
    // evaluates the module body once before the explicit `replay` export call.
    assert_eq!(call_count.load(Ordering::SeqCst), 4);
    drop(registration);

    let interp_error = match workspace.run(
        &mut runtime,
        &RunOptions {
            execution_mode: ExecutionMode::Interp,
            ..RunOptions::default()
        },
    ) {
        Ok(_) => panic!("interp run should fail after native module unregister"),
        Err(error) => error,
    };
    assert_eq!(
        interp_error.status,
        ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_RUNTIME_ERROR
    );
    assert!(
        interp_error.message.contains("failed to run project")
            || interp_error.message.contains("RUNTIME_ERROR")
    );
    assert_eq!(call_count.load(Ordering::SeqCst), 4);

    let binary_error = match workspace.run(
        &mut runtime,
        &RunOptions {
            execution_mode: ExecutionMode::Binary,
            ..RunOptions::default()
        },
    ) {
        Ok(_) => panic!("binary run should fail after native module unregister"),
        Err(error) => error,
    };
    assert_eq!(
        binary_error.status,
        ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_RUNTIME_ERROR
    );
    assert!(
        binary_error.message.contains("failed to run project")
            || binary_error.message.contains("RUNTIME_ERROR")
    );
    assert_eq!(call_count.load(Ordering::SeqCst), 4);

    let export_error = match workspace.call_module_export(
        &mut runtime,
        &RunOptions::default(),
        "main",
        "replay",
        &[],
    ) {
        Ok(_) => panic!("call_module_export should fail after native module unregister"),
        Err(error) => error,
    };
    assert_eq!(
        export_error.status,
        ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_RUNTIME_ERROR
    );
    assert!(
        export_error
            .message
            .contains("failed to prepare project runtime for export call")
            || export_error
                .message
                .contains("failed to call module export main.replay")
            || export_error.message.contains("RUNTIME_ERROR")
    );
    assert_eq!(call_count.load(Ordering::SeqCst), 4);
    Ok(())
}

#[test]
fn invalid_native_function_descriptor_returns_error() {
    let _guard = acquire_test_lock();
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
