use std::fs;

use zr_vm_rust_binding::{ProjectWorkspace, RunOptions, RuntimeBuilder, ValueKind};

#[test]
fn rollback_restores_retained_module_state_and_next_tick() -> Result<(), Box<dyn std::error::Error>>
{
    let temp = tempfile::tempdir()?;
    let root = temp.path().join("checkpoint_project");
    let workspace = ProjectWorkspace::scaffold(&root, "checkpoint_project")?;
    fs::write(
        workspace.project_root()?.join("src").join("main.zr"),
        concat!(
            "module main;\n",
            "var retained = 0;\n",
            "pub fn tick(): int {\n",
            "    retained = retained + 1;\n",
            "    return retained;\n",
            "}\n",
            "return 0;\n",
        ),
    )?;

    let mut runtime = RuntimeBuilder::standard().build()?;
    let mut session = workspace.start_session(&mut runtime, &RunOptions::default())?;
    let checkpoint = session.checkpoint()?;

    let first = session.call_module_export("main", "tick", &[])?;
    assert_eq!(first.kind(), ValueKind::Int);
    assert_eq!(first.as_int()?, 1);
    drop(first);

    session.rollback(&checkpoint)?;
    let replay = session.call_module_export("main", "tick", &[])?;
    assert_eq!(replay.as_int()?, 1);
    Ok(())
}

#[test]
fn rollback_restores_retained_container_uint_array() -> Result<(), Box<dyn std::error::Error>> {
    let temp = tempfile::tempdir()?;
    let root = temp.path().join("checkpoint_container_array_project");
    let workspace = ProjectWorkspace::scaffold(&root, "checkpoint_container_array_project")?;
    fs::write(
        workspace.project_root()?.join("src").join("main.zr"),
        concat!(
            "module main;\n",
            "let container = %import(\"zr.container\");\n",
            "var retained = new container.Array<uint>();\n",
            "retained.add(<uint>7);\n",
            "pub fn tick(): int {\n",
            "    retained[0] = <uint>(<int>retained[0] + 1);\n",
            "    retained.add(<uint>retained.length);\n",
            "    return <int>retained[0] * 100 + retained.length;\n",
            "}\n",
            "return 0;\n",
        ),
    )?;

    let mut runtime = RuntimeBuilder::standard().build()?;
    let mut session = workspace.start_session(&mut runtime, &RunOptions::default())?;
    let checkpoint = session.checkpoint()?;

    assert_eq!(
        session.call_module_export("main", "tick", &[])?.as_int()?,
        802
    );
    session.rollback(&checkpoint)?;
    assert_eq!(
        session.call_module_export("main", "tick", &[])?.as_int()?,
        802
    );
    Ok(())
}

#[test]
fn nested_checkpoints_do_not_count_as_live_value_roots() -> Result<(), Box<dyn std::error::Error>> {
    let temp = tempfile::tempdir()?;
    let root = temp.path().join("nested_checkpoint_project");
    let workspace = ProjectWorkspace::scaffold(&root, "nested_checkpoint_project")?;
    fs::write(
        workspace.project_root()?.join("src").join("main.zr"),
        concat!(
            "module main;\n",
            "var retained = 0;\n",
            "pub fn tick(): int { retained = retained + 1; return retained; }\n",
            "return 0;\n",
        ),
    )?;

    let mut runtime = RuntimeBuilder::standard().build()?;
    let mut session = workspace.start_session(&mut runtime, &RunOptions::default())?;
    let outer = session.checkpoint()?;
    drop(session.call_module_export("main", "tick", &[])?);
    let inner = session.checkpoint()?;
    drop(session.call_module_export("main", "tick", &[])?);

    session.rollback(&inner)?;
    assert_eq!(
        session.call_module_export("main", "tick", &[])?.as_int()?,
        2
    );
    session.rollback(&outer)?;
    assert_eq!(
        session.call_module_export("main", "tick", &[])?.as_int()?,
        1
    );
    Ok(())
}

#[test]
fn checkpoint_rejects_live_vm_value_roots() -> Result<(), Box<dyn std::error::Error>> {
    let temp = tempfile::tempdir()?;
    let root = temp.path().join("rooted_checkpoint_project");
    let workspace = ProjectWorkspace::scaffold(&root, "rooted_checkpoint_project")?;
    fs::write(
        workspace.project_root()?.join("src").join("main.zr"),
        "module main; pub fn rooted(): string { return \"rooted\"; } return 0;\n",
    )?;

    let mut runtime = RuntimeBuilder::standard().build()?;
    let mut session = workspace.start_session(&mut runtime, &RunOptions::default())?;
    let rooted = session.call_module_export("main", "rooted", &[])?;
    assert!(session.checkpoint().is_err());
    drop(rooted);
    assert!(session.checkpoint().is_ok());
    Ok(())
}
