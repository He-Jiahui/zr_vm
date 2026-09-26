const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_type_reachability_smoke.js <zr_vm_cli>');
    process.exit(1);
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// CTest 传入 CLI 可执行路径；此场景查询常量短路表达式，验证 :type 将右操作数标为不可达而不是执行并输出求值结果。
// TODO: 这里等待 exit 后立即检查已收集输出；子进程 stdio 可能尚未关闭，需用慢速或大量尾部输出的假 CLI 验证是否漏读，再决定是否等待 close。
async function main() {
    const child = spawn(cliPath, [], {
        stdio: ['pipe', 'pipe', 'pipe'],
        windowsHide: true,
    });
    let output = '';
    child.stdout.on('data', (chunk) => {
        output += chunk.toString();
    });
    child.stderr.on('data', (chunk) => {
        output += chunk.toString();
    });

    child.stdin.write(':type true || false\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(output.includes('Type: bool'),
        `:type should infer the logical expression type\n${output}`);
    assert(output.includes('Logical flow: short-circuits right operand'),
        `:type should keep printing the shared logical short-circuit fact\n${output}`);
    assert(output.includes('Reachability: unreachable because short-circuit skips evaluation'),
        `:type should print the shared reachability fact for the skipped operand\n${output}`);
    assert(!output.includes('\ntrue\n') && !output.includes('failed to infer expression type'),
        `:type should not execute the logical expression or fail inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
