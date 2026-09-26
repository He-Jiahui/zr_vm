use std::fs;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::{mpsc, Arc};
use std::time::Duration;

use zr_vm_rust_binding::{
    CallBudget, CallError, CallTermination, CancellationToken, CompileOptions, ExecutionDeadline,
    ExecutionMode, FunctionBuilder, ModuleBuilder, ProjectSession, ProjectWorkspace, RunOptions,
    RuntimeBuilder, Value,
};
use zr_vm_rust_binding_sys::ZrRustBindingStatus;

fn assert_termination(error: CallError, expected: CallTermination) {
    assert_eq!(
        error.error.status,
        ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED
    );
    assert_eq!(error.termination, Some(expected));
}

fn call_error(session: &mut ProjectSession, export: &str, budget: &CallBudget) -> CallError {
    match session.call_module_export_with_budget("main", export, &[], budget) {
        Ok(_) => panic!("{export} should terminate"),
        Err(error) => error,
    }
}

fn assert_recovered(session: &mut ProjectSession) -> Result<(), Box<dyn std::error::Error>> {
    assert_eq!(
        session.call_module_export("main", "ping", &[])?.as_int()?,
        123
    );
    Ok(())
}

// This integration binary owns one session at a time because the C binding
// retains process-global native registries and diagnostic storage.
#[test]
fn retained_export_execution_budget_regressions() -> Result<(), Box<dyn std::error::Error>> {
    let temp = tempfile::tempdir()?;
    let workspace = ProjectWorkspace::scaffold(temp.path().join("budget"), "budget")?;
    fs::write(
        workspace.project_root()?.join("src/main.zr"),
        concat!(
            "let host = import(\"budget_host\");\n",
            "var caught: int = 0;\n",
            "pub ping(): int { return 123; }\n",
            "pub spin(): int { while (true) { } return 0; }\n",
            "pub signalledSpin(): int { host.started(); return spin(); }\n",
            "pub protectedSpin(): int {\n",
            "  try { return spin(); } catch (error) { caught = caught + 1; }\n",
            "  finally { caught = caught + 10; }\n",
            "  return 0;\n",
            "}\n",
            "pub caughtCount(): int { return caught; }\n",
            "pub nativeTwice(): int { host.boundary(); return host.boundary(); }\n",
            "pub heapWork(): int {\n",
            "  var container = %import(\"zr.container\");\n",
            "  var values = new container.Array<uint>();\n",
            "  var index = 0;\n",
            "  while (index < 8192) { values.add(<uint>index); index = index + 1; }\n",
            "  return values.length;\n",
            "}\n",
            "pub collect(): int { var gc = %import(\"zr.system.gc\"); gc.collect(\"full\"); return 1; }\n",
            "return 0;\n",
        ),
    )?;
    let token = CancellationToken::new()?;
    let native_token = token.clone();
    let native_calls = Arc::new(AtomicUsize::new(0));
    let native_counter = Arc::clone(&native_calls);
    let native_mode = Arc::new(AtomicUsize::new(0));
    let callback_mode = Arc::clone(&native_mode);
    let (started_sender, started_receiver) = mpsc::channel();
    let module = ModuleBuilder::new("budget_host")
        .add_function(
            FunctionBuilder::new("boundary", 0, 0, move |_| {
                native_counter.fetch_add(1, Ordering::SeqCst);
                match callback_mode.load(Ordering::SeqCst) {
                    1 => native_token.cancel(),
                    2 => std::thread::sleep(Duration::from_millis(150)),
                    _ => {}
                }
                Value::new_int(7)
            })
            .return_type("int"),
        )
        .add_function(
            FunctionBuilder::new("started", 0, 0, move |_| {
                started_sender.send(()).expect("canceller is waiting");
                Value::new_int(0)
            })
            .return_type("int"),
        )
        .build()?;
    let mut runtime = RuntimeBuilder::standard().build()?;
    let _registration = runtime.register_native_module(module)?;
    workspace.compile(&mut runtime, &CompileOptions::default())?;

    // Both source and .zro project sessions use the real bytecode dispatcher.
    for execution_mode in [ExecutionMode::Interp, ExecutionMode::Binary] {
        let mut session = workspace.start_session(
            &mut runtime,
            &RunOptions {
                execution_mode,
                ..RunOptions::default()
            },
        )?;
        let unlimited = CallBudget::default();
        let measured = session.call_module_export_with_budget("main", "ping", &[], &unlimited)?;
        let exact = measured.usage.executed_instructions;
        assert!(exact > 0);
        assert_eq!(measured.value.as_int()?, 123);
        drop(measured);
        let error = call_error(&mut session, "missingExport", &unlimited);
        assert_eq!(error.termination, None);
        assert_eq!(
            error.error.status,
            ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_NOT_FOUND
        );
        assert_recovered(&mut session)?;
        let budget = CallBudget {
            max_instructions: Some(exact),
            ..CallBudget::default()
        };
        let result = session.call_module_export_with_budget("main", "ping", &[], &budget)?;
        assert_eq!(result.value.as_int()?, 123);
        assert_eq!(result.usage.executed_instructions, exact);
        drop(result);
        for limit in [0, exact - 1] {
            let budget = CallBudget {
                max_instructions: Some(limit),
                ..CallBudget::default()
            };
            let error = call_error(&mut session, "ping", &budget);
            assert_eq!(error.usage.executed_instructions, limit);
            assert_termination(error, CallTermination::InstructionLimit);
            assert_recovered(&mut session)?;
        }
        let budget = CallBudget {
            max_instructions: Some(257),
            ..CallBudget::default()
        };
        let error = call_error(&mut session, "protectedSpin", &budget);
        assert_eq!(error.usage.executed_instructions, 257);
        assert_termination(error, CallTermination::InstructionLimit);
        assert_eq!(
            session
                .call_module_export("main", "caughtCount", &[])?
                .as_int()?,
            0
        );
        assert_recovered(&mut session)?;

        let budget = CallBudget {
            deadline: Some(ExecutionDeadline::after(Duration::ZERO)),
            ..CallBudget::default()
        };
        let error = call_error(&mut session, "ping", &budget);
        assert_eq!(error.usage.executed_instructions, 0);
        assert_termination(error, CallTermination::Deadline);
        assert_recovered(&mut session)?;
        let budget = CallBudget {
            deadline: Some(ExecutionDeadline::after(Duration::from_millis(20))),
            ..CallBudget::default()
        };
        let error = call_error(&mut session, "spin", &budget);
        assert!(error.usage.executed_instructions > 0);
        assert_termination(error, CallTermination::Deadline);
        assert_recovered(&mut session)?;
    }

    let mut session = workspace.start_session(&mut runtime, &RunOptions::default())?;
    let concurrent_token = CancellationToken::new()?;
    let canceller_token = concurrent_token.clone();
    let canceller = std::thread::spawn(move || {
        let started = started_receiver.recv_timeout(Duration::from_secs(2));
        // The channel confirms entry into this export; the delay allows the
        // bytecode-only infinite loop to start before the atomic request.
        std::thread::sleep(Duration::from_millis(20));
        canceller_token.cancel();
        started.is_ok()
    });
    let budget = CallBudget {
        deadline: Some(ExecutionDeadline::after(Duration::from_secs(3))),
        cancellation: Some(concurrent_token.clone()),
        ..CallBudget::default()
    };
    let outcome = session.call_module_export_with_budget("main", "signalledSpin", &[], &budget);
    assert!(canceller.join().expect("canceller must return"));
    let error = match outcome {
        Ok(_) => panic!("infinite loop should be cancelled"),
        Err(error) => error,
    };
    assert!(error.usage.executed_instructions > 0);
    assert!(concurrent_token.is_cancelled());
    assert_termination(error, CallTermination::Cancelled);
    assert_recovered(&mut session)?;

    native_mode.store(1, Ordering::SeqCst);
    let budget = CallBudget {
        cancellation: Some(token.clone()),
        ..CallBudget::default()
    };
    assert_termination(
        call_error(&mut session, "nativeTwice", &budget),
        CallTermination::Cancelled,
    );
    assert_eq!(native_calls.load(Ordering::SeqCst), 1);
    let error = call_error(&mut session, "nativeTwice", &budget);
    assert_eq!(error.usage.executed_instructions, 0);
    assert_termination(error, CallTermination::Cancelled);
    assert_eq!(native_calls.load(Ordering::SeqCst), 1);
    assert_recovered(&mut session)?;

    native_mode.store(2, Ordering::SeqCst);
    let budget = CallBudget {
        deadline: Some(ExecutionDeadline::after(Duration::from_millis(100))),
        ..CallBudget::default()
    };
    assert_termination(
        call_error(&mut session, "nativeTwice", &budget),
        CallTermination::Deadline,
    );
    assert_eq!(native_calls.load(Ordering::SeqCst), 2);
    assert_recovered(&mut session)?;
    native_mode.store(0, Ordering::SeqCst);
    assert_eq!(
        session
            .call_module_export("main", "nativeTwice", &[])?
            .as_int()?,
        7
    );
    assert_eq!(native_calls.load(Ordering::SeqCst), 4);
    assert_eq!(session.gc_step(1000)?.cross_boundary_reference_count, 0);

    let budget = CallBudget {
        max_native_calls: Some(2),
        ..CallBudget::default()
    };
    let result = session.call_module_export_with_budget("main", "nativeTwice", &[], &budget)?;
    assert_eq!(
        result.usage.native_calls, 2,
        "binding thunks must not double count"
    );
    drop(result);
    for limit in [0, 1] {
        let before = native_calls.load(Ordering::SeqCst);
        let budget = CallBudget {
            max_native_calls: Some(limit),
            ..CallBudget::default()
        };
        let error = call_error(&mut session, "nativeTwice", &budget);
        assert_eq!(error.usage.native_calls, limit);
        assert_eq!(native_calls.load(Ordering::SeqCst) - before, limit as usize);
        assert_termination(error, CallTermination::NativeCallLimit);
        assert_recovered(&mut session)?;
    }
    let baseline =
        session.call_module_export_with_budget("main", "ping", &[], &CallBudget::default())?;
    let heap = baseline.usage.peak_heap_bytes;
    assert!(heap > 0);
    drop(baseline);
    let budget = CallBudget {
        max_heap_bytes: Some(0),
        ..CallBudget::default()
    };
    let error = call_error(&mut session, "ping", &budget);
    assert_eq!(error.usage.executed_instructions, 0);
    assert!(error.usage.peak_heap_bytes > 0);
    assert_termination(error, CallTermination::HeapLimit);
    assert_recovered(&mut session)?;
    let budget = CallBudget {
        max_heap_bytes: Some(heap + 4096),
        ..CallBudget::default()
    };
    let error = call_error(&mut session, "heapWork", &budget);
    assert!(error.usage.peak_heap_bytes > heap + 4096);
    assert_termination(error, CallTermination::HeapLimit);
    assert_recovered(&mut session)?;
    let collected =
        session.call_module_export_with_budget("main", "collect", &[], &CallBudget::default())?;
    assert!(collected.usage.gc_time > Duration::ZERO);
    assert!(collected.usage.gc_time <= collected.usage.elapsed);
    drop(collected);
    let budget = CallBudget {
        max_gc_time: Some(Duration::ZERO),
        ..CallBudget::default()
    };
    let error = call_error(&mut session, "collect", &budget);
    assert!(error.usage.gc_time > Duration::ZERO);
    assert_termination(error, CallTermination::GcTimeLimit);
    assert_recovered(&mut session)?;
    Ok(())
}
