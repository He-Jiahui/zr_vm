const { fileURLToPath } = require('url');

// File URIs may spell unreserved bytes such as '~' with percent escapes.
// Virtual document URIs retain their exact identity.
function documentUriMatches(expectedUri, actualUri) {
    if (actualUri === expectedUri) {
        return true;
    }
    if (typeof actualUri !== 'string' || typeof expectedUri !== 'string') {
        return false;
    }
    try {
        const expectedPath = fileURLToPath(expectedUri);
        const actualPath = fileURLToPath(actualUri);
        return process.platform === 'win32'
            ? expectedPath.toLowerCase() === actualPath.toLowerCase()
            : expectedPath === actualPath;
    } catch {
        return false;
    }
}

module.exports = { documentUriMatches };
