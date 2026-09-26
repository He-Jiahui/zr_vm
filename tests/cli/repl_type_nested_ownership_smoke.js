const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_type_nested_ownership_smoke.js <zr_vm_cli>');
    process.exit(1);
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// CTest 传入 CLI 可执行路径；此场景先发布所有权变量再查询数组包裹的引用，验证聚合类型与内部读引用、声明位置相互一致。
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

    child.stdin.write('var owner: Unique<int>;\n');
    child.stdin.write('\n');
    child.stdin.write(':type [owner]\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(output.includes('Type: Unique<int>[1]<Unique<int>>'),
        `:type should infer the aggregate unique ownership type\n${output}`);
    assert(output.includes('Expression: array exact'),
        `:type should print the aggregate expression fact\n${output}`);
    assert(output.includes('Expression: identifier exact'),
        `:type should print the nested owner identifier expression fact\n${output}`);
    assert(output.includes('Reference: read owner'),
        `:type should print the nested owner identifier reference fact\n${output}`);
    assert(output.includes('Declared at: 1:5'),
        `:type should print the nested owner operand declaration location\n${output}`);
    assert(!output.includes('failed to infer expression type') &&
        !output.includes('Compiler Error'),
        `:type should not fail nested ownership inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
