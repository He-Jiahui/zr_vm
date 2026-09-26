const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_type_lambda_local_fact_smoke.js <zr_vm_cli>');
    process.exit(1);
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// CTest 传入 CLI 可执行路径；此场景查询 lambda 局部变量的折叠结果与读引用，防止函数体语义遍历只停留在顶层 lambda 节点。
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

    child.stdin.write(':type fn(): int => { var folded = 1 + 2; return folded; }\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(output.includes('Type: fn() -> int'),
        `:type should infer the lambda return type from the local initializer\n${output}`);
    assert(output.includes('Expression: lambda exact'),
        `:type should print the lambda expression fact\n${output}`);
    assert(output.includes('Expression: binary exact'),
        `:type should print expression facts from lambda-local initializers\n${output}`);
    assert(output.includes('Constant: 3'),
        `:type should print folded constants from lambda-local initializer facts\n${output}`);
    assert(output.includes('Numeric range: 3..3'),
        `:type should print numeric facts from lambda-local initializers\n${output}`);
    assert(output.includes('Reference: read folded'),
        `:type should keep printing the return expression reference fact\n${output}`);
    assert(!output.includes('Constant: 1') &&
        !output.includes('Constant: 2') &&
        !output.includes('failed to infer expression type') &&
        !output.includes('Compiler Error'),
        `:type should not dump leaf constants or fail lambda-local inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
