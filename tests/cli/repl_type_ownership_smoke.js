const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_type_ownership_smoke.js <zr_vm_cli>');
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

    child.stdin.write('var owner: Unique<int>;\n');
    child.stdin.write('\n');
    child.stdin.write(':type owner\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(output.includes('Type: Unique<int>'),
        `:type should infer the current unique ownership-qualified type\n${output}`);
    assert(output.includes('Expression: identifier exact'),
        `:type should preserve the owner identifier expression fact\n${output}`);
    assert(output.includes('Reference: read owner'),
        `:type should print the owner identifier reference fact\n${output}`);
    assert(output.includes('Declared at: 1:5'),
        `:type should print the owner operand declaration location for %borrow(owner)\n${output}`);
    assert(!output.includes('\nint\n') && !output.includes('failed to infer expression type'),
        `:type should not execute the ownership expression or fail inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
