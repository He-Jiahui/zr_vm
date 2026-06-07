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

    child.stdin.write('var owner: %unique int;\n');
    child.stdin.write('\n');
    child.stdin.write(':type [%borrow(owner)]\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(output.includes('Type: %borrowed int[1]<%borrowed int>'),
        `:type should infer the aggregate borrowed type\n${output}`);
    assert(output.includes('Expression: array exact'),
        `:type should print the aggregate expression fact\n${output}`);
    assert(output.includes('Expression: ownership builtin exact'),
        `:type should print the nested ownership builtin expression fact\n${output}`);
    assert(output.includes('Ownership: borrow %borrowed'),
        `:type should print the nested ownership semantic fact\n${output}`);
    assert(output.includes('Reference: read owner'),
        `:type should print the nested owner operand reference fact\n${output}`);
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
