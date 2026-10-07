// AC ilib: os — JavaScript FFI
// Inlined by AC->JS compiler when "use ilib os" is declared.
'use strict';
const _child = require('child_process');
const _fs    = require('fs');
// (no 'path' module use in this file — a dead `const _path = require('path')` here used to
// collide with widgets_ffi.js's own _path when both ilibs were inlined into one program,
// "Identifier '_path' has already been declared" — verified: ac_ide.ac, which uses both.)

const _SBASH_FORBIDDEN = [
    /\bsudo\b/,
    /\bsu\s/,
    /\bscreen\b/,   // parity with PY/Rust/Java (backgrounding via screen/tmux was let through on JS)
    /\btmux\b/,
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
            process.stderr.write(`[os.sbash] The p in bash stands for protection (blocked: ${pat})\n`);
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
    // Match PY/Rust: ensure a trailing newline (was omitted → different file contents per backend).
    try { let s = String(content); if (!s.endsWith('\n')) s += '\n'; _fs.appendFileSync(String(p), s); return 0; }
    catch(e) { process.stderr.write(`[os.append_to] ${e}\n`); return -1; }
}
function os_read(p) {
    try { return _fs.readFileSync(String(p), 'utf8'); }
    catch(e) { process.stderr.write(`[os.read] ${e}\n`); return ""; }
}


function os_isdir(p)  { try { return _fs.statSync(String(p)).isDirectory() ? 1 : 0; } catch { return 0; } }
function os_isfile(p) { try { return _fs.statSync(String(p)).isFile() ? 1 : 0; } catch { return 0; } }
function os_size(p)   { try { return _fs.statSync(String(p)).size; } catch { return -1; } }
function os_mtime(p)  { try { return Math.floor(_fs.statSync(String(p)).mtimeMs / 1000); } catch { return -1; } }
function os_copy(src, dst) {
    try { _fs.copyFileSync(String(src), String(dst)); return 0; }
    catch (e) { process.stderr.write(`[os.copy] ${e.message}\n`); return -1; }
}
function os_move(src, dst) {
    try { _fs.renameSync(String(src), String(dst)); return 0; }
    catch (e) {
        // rename() fails across filesystems: copy, then remove the original
        try { _fs.copyFileSync(String(src), String(dst)); _fs.unlinkSync(String(src)); return 0; }
        catch (e2) { process.stderr.write(`[os.move] ${e2.message}\n`); return -1; }
    }
}
function os_listdir(p) {
    try { return _fs.readdirSync(String(p)).sort(); }
    catch (e) { process.stderr.write(`[os.listdir] ${e.message}\n`); return []; }
}
function os_tmpdir() { return process.env.TMPDIR || process.env.TEMP || '/tmp'; }
function os_tmpfile(suffix = '') {
    const dir = os_tmpdir();
    for (let tries = 0; tries < 100; tries++) {
        const p = dir + '/acos_' + Math.random().toString(36).slice(2, 10) + String(suffix);
        try { _fs.writeFileSync(p, '', { flag: 'wx' }); return p; } catch (e) { if (e.code !== 'EEXIST') return ''; }
    }
    return '';
}
function os_mktmpdir() { return _fs.mkdtempSync(os_tmpdir() + '/acos_'); }
function os_chdir(p) {
    try { process.chdir(String(p)); return 0; }
    catch (e) { process.stderr.write(`[os.chdir] ${e.message}\n`); return -1; }
}
function os_join(a, b) {
    a = String(a); b = String(b);
    if (!a) return b;
    if (!b) return a;
    if (b[0] === '/') return b;
    return a.endsWith('/') ? a + b : a + '/' + b;
}
function os_basename(p) { p = String(p); return p.slice(p.lastIndexOf('/') + 1); }
function os_dirname(p) {
    p = String(p);
    const cut = p.lastIndexOf('/');
    if (cut < 0) return '';
    if (cut === 0) return '/';
    return p.slice(0, cut);
}
function os_homedir() { return process.env.HOME || ''; }

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
    isdir: os_isdir,
    isfile: os_isfile,
    size: os_size,
    mtime: os_mtime,
    copy: os_copy,
    move: os_move,
    listdir: os_listdir,
    tmpdir: os_tmpdir,
    tmpfile: os_tmpfile,
    mktmpdir: os_mktmpdir,
    chdir: os_chdir,
    join: os_join,
    basename: os_basename,
    dirname: os_dirname,
    homedir: os_homedir,
};
