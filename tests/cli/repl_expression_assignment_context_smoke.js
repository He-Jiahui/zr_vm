const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_expression_assignment_context_smoke.js <zr_vm_cli>');
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

    child.stdin.write('var seed = 2;\n');
    child.stdin.write('\n');
    child.stdin.write('seed = 5;\n');
    child.stdin.write('\n');
    child.stdin.write('seed + 1\n');
    child.stdin.write('\n');
    child.stdin.write(':type seed + 1\n');
    child.stdin.write('var obj = {a: 10};\n');
    child.stdin.write('\n');
    child.stdin.write('obj.a = 40;\n');
    child.stdin.write('\n');
    child.stdin.write('obj.a\n');
    child.stdin.write('\n');
    child.stdin.write('var values = [10];\n');
    child.stdin.write('\n');
    child.stdin.write('values[0] = 40;\n');
    child.stdin.write('\n');
    child.stdin.write('values[0] + 2\n');
    child.stdin.write('\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    const normalizedOutput = output.replace(/\r\n/g, '\n');
    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(normalizedOutput.includes('\n6\n'),
        `bare expression should execute against the latest persisted assignment\n${output}`);
    assert(output.includes('Numeric range: 6..6'),
        `:type should infer against the latest persisted assignment\n${output}`);
    assert(!output.includes('Numeric range: 3..3'),
        `:type should not keep using the stale declaration initializer after assignment\n${output}`);
    assert(normalizedOutput.includes('\n40\n'),
        `member assignment should persist for later REPL member reads\n${output}`);
    assert(!normalizedOutput.includes('\n10\n'),
        `member assignment should not fall back to the stale object literal value\n${output}`);
    assert(normalizedOutput.includes('\n42\n'),
        `index assignment should persist for later REPL expression execution\n${output}`);
    assert(!normalizedOutput.includes('\n12\n'),
        `index assignment should not fall back to the stale array literal value\n${output}`);
    assert(!output.includes('undefined variable') &&
        !output.includes('Unknown identifier') &&
        !output.includes('failed to infer expression type'),
        `assignment context should not break later expression inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
