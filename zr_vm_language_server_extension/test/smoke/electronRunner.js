const { runSmokeSuite } = require('./smokeSuite.js');

async function run() {
    await runSmokeSuite({
        expectedMode: process.env.ZR_TEST_SERVER_MODE || 'native',
        focus: process.env.ZR_TEST_SMOKE_FOCUS || 'all',
    });
}

module.exports = {
    run,
};
