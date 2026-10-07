// AC web-server Library FFI for JavaScript — Node.js only (a TCP server / SQL
// engine has no meaning in a browser sandbox; ilib:web itself stays browser+Node,
// this ilib targets Node).
//
// Node has no synchronous socket API, so `server.accept()` etc. can't hold a
// persistent open connection to a worker the way the C++ core does. Instead this
// shells out synchronously (execFileSync + ws_bridge.py) to two lazily-spawned,
// long-lived Python workers that own the real blocking I/O:
//   - jasql_cli.py  (SQL — same worker + protocol the C++ core talks to)
//   - http_worker.py (HTTP serving — Node-only, see that file's header)
// The per-call Python-startup cost (~10-20ms) is the honest tradeoff for Node not
// having first-class blocking I/O.

const { spawnSync, spawn, execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

// `__dirname` is meaningless here — this file is inlined as TEXT into the user's
// generated program (readFFIFile in ir_codegen.cpp), so __dirname/__file__ point at
// the OUTPUT script's directory, not this file's real location. `_ac_web_server_lib_dir`
// is emitted by the compiler right before this file is inlined (same AC_PATH/cwd-search
// logic every other backend uses) — same fix as camera_ffi.js's own _ac_camera_lib_dir.
const _HERE = typeof _ac_web_server_lib_dir !== 'undefined' ? _ac_web_server_lib_dir : __dirname;
const _JASQL_CLI = path.join(_HERE, 'jasql_cli.py');
const _HTTP_WORKER = path.join(_HERE, 'http_worker.py');
const _BRIDGE = path.join(_HERE, 'ws_bridge.py');

const _sqlSock = '/tmp/ac_ws_js_sql_' + process.pid + '.sock';
const _httpSock = '/tmp/ac_ws_js_http_' + process.pid + '.sock';

let _sqlWorker = null;
let _httpWorkerProc = null;

function _sleepMs(ms) {
  // Real synchronous sleep via Atomics.wait — no busy loop, no shelling out.
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
}

function _ensureWorker(procRef, script, sockPath) {
  if (procRef.p) return procRef.p;
  const child = spawn('python3', [script, '--serve', sockPath], {
    stdio: ['ignore', 'ignore', 'ignore'],
    detached: true,
  });
  child.unref();
  for (let i = 0; i < 150 && !fs.existsSync(sockPath); i++) _sleepMs(20);
  procRef.p = child;
  process.on('exit', () => {
    try { child.kill('SIGTERM'); } catch (e) { /* already gone */ }
    try { fs.unlinkSync(sockPath); } catch (e) { /* already gone */ }
  });
  return child;
}

function _roundtrip(sockPath, reqObj) {
  const line = JSON.stringify(reqObj);
  let out;
  try {
    out = execFileSync('python3', [_BRIDGE, sockPath, line], { encoding: 'utf8' });
  } catch (e) {
    return { ok: false, error: 'bridge exec failed: ' + e.message };
  }
  try {
    return JSON.parse(out);
  } catch (e) {
    return { ok: false, error: 'bad worker response: ' + out };
  }
}

const _sqlProcRef = {};
function _sqlCall(reqObj) {
  _ensureWorker(_sqlProcRef, _JASQL_CLI, _sqlSock);
  return _roundtrip(_sqlSock, reqObj);
}

const _httpProcRef = {};
function _httpCall(reqObj) {
  _ensureWorker(_httpProcRef, _HTTP_WORKER, _httpSock);
  return _roundtrip(_httpSock, reqObj);
}

function _resultOrError(resp) {
  return resp.ok ? String(resp.result) : ('Preposterous: ' + resp.error);
}

// ── SQL ─────────────────────────────────────────────────────────────────────

function server_db_run(statement) {
  return _resultOrError(_sqlCall({ cmd: 'run', stmt: String(statement) }));
}

function server_db_run_p(statement, params_json) {
  let params = null;
  if (params_json) {
    try { params = JSON.parse(params_json); } catch (e) { /* leave null -> worker rejects cleanly */ }
  }
  return _resultOrError(_sqlCall({ cmd: 'run', stmt: String(statement), params: params }));
}

function server_db_import(p) {
  return _resultOrError(_sqlCall({ cmd: 'import', path: String(p) }));
}

function server_db_reset() {
  return _resultOrError(_sqlCall({ cmd: 'reset' }));
}

function server_db_stop() {
  if (_sqlProcRef.p) { try { _sqlProcRef.p.kill('SIGTERM'); } catch (e) {} _sqlProcRef.p = null; }
}

// ── HTTP ────────────────────────────────────────────────────────────────────

function server_listen(port) {
  const r = _httpCall({ cmd: 'listen', port: Number(port) });
  return r.ok ? 1 : 0;
}

function server_accept() {
  const r = _httpCall({ cmd: 'accept' });
  return r.ok ? 1 : 0;
}

function server_req_method() { return _resultOrError(_httpCall({ cmd: 'req_method' })); }
function server_req_path()   { return _resultOrError(_httpCall({ cmd: 'req_path' })); }
function server_req_body()   { return _resultOrError(_httpCall({ cmd: 'req_body' })); }
function server_req_query(key) { return _resultOrError(_httpCall({ cmd: 'req_query', key: String(key) })); }
function server_req_header(name) { return _resultOrError(_httpCall({ cmd: 'req_header', name: String(name) })); }

function server_respond(status, body) {
  const r = _httpCall({ cmd: 'respond', status: Number(status), body: String(body == null ? '' : body) });
  return r.ok ? 1 : 0;
}

function server_respond_json(status, json_body) {
  const r = _httpCall({ cmd: 'respond_json', status: Number(status), body: String(json_body == null ? '' : json_body) });
  return r.ok ? 1 : 0;
}

function server_close() {
  _httpCall({ cmd: 'close' });
  if (_httpProcRef.p) { try { _httpProcRef.p.kill('SIGTERM'); } catch (e) {} _httpProcRef.p = null; }
}

function server_help() {
  console.log(`=== web-server Library ===
One request in flight at a time (single blocking accept loop, no threads).

server.listen(port)
server.accept()                          - blocks for next request
server.req_method() / req_path() / req_body()
server.req_query(key) / req_header(name)
server.respond(status, body) / respond_json(status, json_body)
server.close()

server.db_run(statement)                 - no placeholders; trusted/static text only
server.db_run_p(statement, paramsJson)   - ?/:name placeholders, bound server-side
                                               by JaSQL (JSON array/object string) -
                                               the safe path for request-derived data
server.db_import(path) / db_reset() / db_stop()`);
}

const server = {
  listen: server_listen,
  accept: server_accept,
  req_method: server_req_method,
  req_path: server_req_path,
  req_query: server_req_query,
  req_body: server_req_body,
  req_header: server_req_header,
  respond: server_respond,
  respond_json: server_respond_json,
  close: server_close,
  db_run: server_db_run,
  db_run_p: server_db_run_p,
  db_import: server_db_import,
  db_reset: server_db_reset,
  db_stop: server_db_stop,
  help: server_help,
};
