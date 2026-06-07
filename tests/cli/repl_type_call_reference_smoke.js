const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_type_call_reference_smoke.js <zr_vm_cli>');
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

    child.stdin.write('func pick(value: int): int {\n');
    child.stdin.write('    return value;\n');
    child.stdin.write('}\n');
    child.stdin.write('\n');
    child.stdin.write(':type pick(1 + 2)\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(output.includes('Type: int'),
        `:type should infer the prior function return type\n${output}`);
    assert(output.includes('Call: pick args=1'),
        `:type should print the shared call expression fact\n${output}`);
    assert(output.includes('Expression: binary exact'),
        `:type should print shared expression facts for call arguments\n${output}`);
    assert(output.includes('Constant: 3'),
        `:type should print folded constants from call argument expression facts\n${output}`);
    assert(output.includes('Reference: call pick'),
        `:type should print the resolved call reference fact\n${output}`);
    assert(output.includes('Declared at:'),
        `:type should print the prior function declaration location\n${output}`);
    assert(!output.includes('Type: object') &&
        !output.includes('Constant: 1') &&
        !output.includes('Constant: 2') &&
        !output.includes('failed to infer expression type') &&
        !output.includes('Compiler Error'),
        `:type should not fall back, dump leaf constants, or fail inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
