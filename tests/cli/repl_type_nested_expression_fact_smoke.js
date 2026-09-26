const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_type_nested_expression_fact_smoke.js <zr_vm_cli>');
    process.exit(1);
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// CTest 传入 CLI 可执行路径；此场景查询数组中的二元式，验证 :type 遍历嵌套表达式并展示折叠后的常量而非叶子值。
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

    child.stdin.write(':type [1 + 2]\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(output.includes('Type: int[1]<int>'),
        `:type should infer the aggregate expression type\n${output}`);
    assert(output.includes('Expression: array exact'),
        `:type should print the aggregate expression fact\n${output}`);
    assert(output.includes('Expression: binary exact'),
        `:type should print the nested element expression fact\n${output}`);
    assert(output.includes('Constant: 3'),
        `:type should print folded constants from nested expression facts\n${output}`);
    assert(!output.includes('Constant: 1') &&
        !output.includes('Constant: 2') &&
        !output.includes('\n[3]\n') &&
        !output.includes('failed to infer expression type'),
        `:type should not dump leaf constants, execute the expression, or fail inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
