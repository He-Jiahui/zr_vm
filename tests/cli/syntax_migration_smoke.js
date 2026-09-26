"use strict";

const assert = require("assert");
const childProcess = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

const cli = process.argv[2];
const fixture = path.resolve(__dirname, "../fixtures/syntax_migration_frontend/input/machine_forms.zr");
const fixtureRelative = "tests/fixtures/syntax_migration_frontend/input/machine_forms.zr";
const expectedReport = path.resolve(__dirname, "../fixtures/syntax_migration_frontend/expected/machine_forms.json");
const repoRoot = path.resolve(__dirname, "../..");

assert(cli, "expected zr_vm_cli executable path");
const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), "zr-vm-syntax-migration-"));
const target = path.join(tempRoot, "machine_forms.zr");
const original = fs.readFileSync(fixture, "utf8");

// 从仓库 fixture 对照稳定 JSON，再在临时副本上覆盖只检查、实际写入、幂等性和生成目录过滤。
// 现有验收脚本手动传入已构建的 CLI；只有临时副本允许被 --write 改动。
// TODO: tests/CMakeLists.txt 目前只注册 C 版 cli_syntax_migration；需确认本脚本是否应纳入常规 CTest。
try {
    const golden = childProcess.spawnSync(cli, ["migrate", "syntax", fixtureRelative, "--check", "--format", "json"], {
        cwd: repoRoot,
        encoding: "utf8"
    });
    assert.strictEqual(golden.status, 0, golden.stderr);
    // BUG: 当前 legacy_migration.c 将 %module 标为 machineApplicable/06A，golden JSON 仍要求 targetNotPromoted/06B；此断言在现存 CLI 上失败。
    assert.strictEqual(golden.stdout.trim(), fs.readFileSync(expectedReport, "utf8").trim());

    fs.writeFileSync(target, original);
    const check = childProcess.spawnSync(cli, ["migrate", "syntax", target, "--check", "--format", "json"], {
        encoding: "utf8"
    });
    assert.strictEqual(check.status, 0, check.stderr);
    const report = JSON.parse(check.stdout);
    assert.strictEqual(report.schemaVersion, 1);
    assert.strictEqual(report.write, false);
    // TODO: 当前 %module 规则已有 fix，下面仍要求 targetNotPromoted 且无 fix；修正首个 golden 分歧后需继续跑到此断言核对。
    assert(report.items.some((item) => item.oldConstructKind === "percentModule" &&
        item.applicability === "targetNotPromoted" && !item.hasFix));
    assert(report.items.some((item) => item.oldConstructKind === "percentOwned" && item.hasFix));
    assert(report.items.every((item) => !item.file.includes("\\")));
    assert.strictEqual(fs.readFileSync(target, "utf8"), original);

    fs.writeFileSync(`${target}.zr-migrate-tmp`, "do not replace\n");
    const write = childProcess.spawnSync(cli, ["migrate", "syntax", target, "--write", "--format", "json"], {
        encoding: "utf8"
    });
    assert.strictEqual(write.status, 0, write.stderr);
    const migrated = fs.readFileSync(target, "utf8");
    // TODO: 当前规则会改写 %module，旧语法保留断言可能同样过时；需跑通前面的检查后验证 --write 实际产物。
    assert(migrated.includes("%module migration.fixture;"));
    assert(migrated.includes("resource class Handle {}"));
    assert(migrated.includes("9 % 2"));
    assert.strictEqual(fs.readFileSync(`${target}.zr-migrate-tmp`, "utf8"), "do not replace\n");

    const second = childProcess.spawnSync(cli, ["migrate", "syntax", target, "--check", "--format", "json"], {
        encoding: "utf8"
    });
    assert.strictEqual(second.status, 0, second.stderr);
    const secondReport = JSON.parse(second.stdout);
    assert.strictEqual(secondReport.items.filter((item) => item.hasFix).length, 0);
    assert.strictEqual(fs.readFileSync(target, "utf8"), migrated);

    const nestedDirectory = path.join(tempRoot, "nested");
    const generatedDirectory = path.join(tempRoot, "generated");
    const nested = path.join(nestedDirectory, "nested.zr");
    const generated = path.join(generatedDirectory, "generated.zr");
    fs.mkdirSync(nestedDirectory);
    fs.mkdirSync(generatedDirectory);
    fs.writeFileSync(nested, "%owned class Nested {}\n");
    fs.writeFileSync(generated, "%owned class Generated {}\n");

    const directoryWrite = childProcess.spawnSync(cli, ["migrate", "syntax", tempRoot, "--write", "--format", "json"], {
        encoding: "utf8"
    });
    assert.strictEqual(directoryWrite.status, 0, directoryWrite.stderr);
    assert(fs.readFileSync(nested, "utf8").includes("resource class Nested {}"));
    assert.strictEqual(fs.readFileSync(generated, "utf8"), "%owned class Generated {}\n");

    const directGenerated = childProcess.spawnSync(cli, ["migrate", "syntax", generated, "--write", "--format", "json"], {
        encoding: "utf8"
    });
    assert.notStrictEqual(directGenerated.status, 0);
    assert.strictEqual(fs.readFileSync(generated, "utf8"), "%owned class Generated {}\n");

    const includeGenerated = childProcess.spawnSync(cli, [
        "migrate", "syntax", tempRoot, "--write", "--format", "json", "--include-generated"
    ], { encoding: "utf8" });
    assert.strictEqual(includeGenerated.status, 0, includeGenerated.stderr);
    assert(fs.readFileSync(generated, "utf8").includes("resource class Generated {}"));
} finally {
    fs.rmdirSync(tempRoot, { recursive: true });
}
