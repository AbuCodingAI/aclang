// AC ilib: os — JavaScript FFI
// Inlined by AC->JS compiler when "use ilib os" is declared.
'use strict';
const _child = require('child_process');
const _fs    = require('fs');
const _path  = require('path');

const _SBASH_FORBIDDEN = [
    /\bsudo\b/,
    /\bsu\s/,
    /function\s+\w+\s*\(/,
    /\(\s*\)\s*\{/,
    /&\s*$/,
    /&\s+\w/,
    /\bnohup\b/,
    /\bdisown\b/,
];

function os_bash(cmd) {
    try {
        _child.execSync(String(cmd), {stdio: 'inherit'});
        return 0;
    } catch(e) { return e.status || -1; }
}
function os_sbash(cmd) {
    const s = String(cmd);
    for (const pat of _SBASH_FORBIDDEN) {
        if (pat.test(s)) {
            process.stderr.write(`[os.sbash] blocked: ${pat}\n`);
            return -1;
        }
    }
    try {
        _child.execSync(s, {stdio: 'inherit'});
        return 0;
    } catch(e) { return e.status || -1; }
}
function os_app_open(app) {
    const s = String(app);
    const launcher = process.platform === 'darwin' ? 'open'
                   : process.platform === 'win32'  ? 'start'
                   : 'xdg-open';
    _child.spawn(launcher, [s], {detached: true, stdio: 'ignore'}).unref();
    return 0;
}
function os_mkfile(p) {
    try { _fs.closeSync(_fs.openSync(String(p), 'a')); return 0; }
    catch(e) { process.stderr.write(`[os.mkfile] ${e}\n`); return -1; }
}
function os_rmfile(p) {
    try { _fs.unlinkSync(String(p)); return 0; }
    catch(e) { process.stderr.write(`[os.rmfile] ${e}\n`); return -1; }
}
function os_mkdir(p) {
    try { _fs.mkdirSync(String(p), {recursive: true}); return 0; }
    catch(e) { process.stderr.write(`[os.mkdir] ${e}\n`); return -1; }
}
function os_rmdir(p) {
    try { _fs.rmSync(String(p), {recursive: true, force: true}); return 0; }
    catch(e) { process.stderr.write(`[os.rmdir] ${e}\n`); return -1; }
}
function os_exists(p)  { return _fs.existsSync(String(p)) ? 1 : 0; }
function os_cwd()      { return process.cwd(); }
function os_env(key)   { return process.env[String(key)] || ""; }
function os_write_to(p, content) {
    try { _fs.writeFileSync(String(p), String(content)); return 0; }
    catch(e) { process.stderr.write(`[os.write_to] ${e}\n`); return -1; }
}
function os_append_to(p, content) {
    try { _fs.appendFileSync(String(p), String(content)); return 0; }
    catch(e) { process.stderr.write(`[os.append_to] ${e}\n`); return -1; }
}
function os_read(p) {
    try { return _fs.readFileSync(String(p), 'utf8'); }
    catch(e) { process.stderr.write(`[os.read] ${e}\n`); return ""; }
}

const os = {
    bash:     os_bash,
    sbash:    os_sbash,
    app_open: os_app_open,
    mkfile:   os_mkfile,
    rmfile:   os_rmfile,
    mkdir:    os_mkdir,
    rmdir:    os_rmdir,
    exists:   os_exists,
    cwd:      os_cwd,
    env:      os_env,
    write_to: os_write_to,
    append_to:os_append_to,
    read:     os_read,
};
