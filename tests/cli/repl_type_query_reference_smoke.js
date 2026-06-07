const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_type_query_reference_smoke.js <zr_vm_cli>');
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
    child.stdin.write(':type %type(owner)\n');
    child.stdin.write(':type %type(1 + 2)\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(output.includes('Reference: read owner'),
        `:type should print references from inside %type(...) operands\n${output}`);
    assert(output.includes('Declared at: 1:5'),
        `:type should preserve the operand declaration location\n${output}`);
    assert(output.includes('Expression: binary exact'),
        `:type should print expression facts from inside %type(...) operands\n${output}`);
    assert(output.includes('Constant: 3'),
        `:type should print folded constants from inside %type(...) operands\n${output}`);
    assert(!output.includes('failed to infer expression type') &&
        !output.includes('Constant: 1') &&
        !output.includes('Constant: 2') &&
        !output.includes('Compiler Error'),
        `:type should not dump leaf constants or fail type-query operand inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
