# AC web-server Library — native Python implementation.
# Server-side companion to ilib:web (client-only). Python can `import` the vendored
# JaSQL engine directly and skip the persistent-worker-over-socket indirection the
# other (non-Python/JS) backends need to talk to it — see web-server.cpp for that.
import os as _os
import socket as _socket
import sys as _sys
import urllib.parse as _urlparse

def _ilib_dir(lib):
    # This file is inlined as text into the user's generated program (readFFIFile in
    # ir_codegen.cpp), so __file__ points at the OUTPUT script, not this file's real
    # location — can't use it to find sibling files. Same AC_PATH/cwd-relative/binary-
    # relative search order as resolveIlibDir() in ir_codegen.cpp.
    rel = _os.path.join('library', 'ilib', lib)
    acp = _os.environ.get('AC_PATH')
    if acp:
        cand = _os.path.join(acp, rel)
        if _os.path.isdir(cand):
            return cand
    if _os.path.isdir(rel):
        return rel
    exe_dir = _os.path.dirname(_os.path.abspath(_sys.argv[0]))
    cand = _os.path.join(exe_dir, '..', rel)
    if _os.path.isdir(cand):
        return cand
    return rel


_sys.path.insert(0, _os.path.join(_ilib_dir('web-server'), 'jasql_vendor'))

import jasql as _jasql          # noqa: E402  (vendored)
from abudb import Database as _Database  # noqa: E402

_db = _Database()

# ── HTTP serving: one global listening socket + one "current request" ─────────
# Matches the C++ core's model (web-server.cpp) — single blocking accept() call,
# no handles, one request in flight at a time.

_listen_sock = None
_conn_sock = None
_req = {'method': '', 'path': '', 'query': {}, 'headers': {}, 'body': ''}


def server_listen(port):
    global _listen_sock
    if _listen_sock is not None:
        _listen_sock.close()
    s = _socket.socket(_socket.AF_INET, _socket.SOCK_STREAM)
    s.setsockopt(_socket.SOL_SOCKET, _socket.SO_REUSEADDR, 1)
    try:
        s.bind(('0.0.0.0', int(port)))
        s.listen(16)
    except OSError:
        return 0
    _listen_sock = s
    return 1


def server_accept():
    global _conn_sock, _req
    if _listen_sock is None:
        return 0
    if _conn_sock is not None:
        _conn_sock.close()
        _conn_sock = None
    try:
        conn, _addr = _listen_sock.accept()
    except OSError:
        return 0

    buf = b''
    while b'\r\n\r\n' not in buf:
        chunk = conn.recv(4096)
        if not chunk:
            conn.close()
            return 0
        buf += chunk
        if len(buf) > 1024 * 1024:
            conn.close()
            return 0
    header_block, _, body_so_far = buf.partition(b'\r\n\r\n')
    lines = header_block.split(b'\r\n')
    req_line = lines[0].decode('latin-1', 'replace')
    parts = req_line.split(' ')
    method = parts[0] if parts else ''
    raw_path = parts[1] if len(parts) > 1 else '/'
    path, _, qs = raw_path.partition('?')
    query = {k: v[0] if v else '' for k, v in _urlparse.parse_qs(qs, keep_blank_values=True).items()}

    headers = {}
    for hline in lines[1:]:
        htext = hline.decode('latin-1', 'replace')
        if ':' in htext:
            k, v = htext.split(':', 1)
            headers[k.strip().lower()] = v.strip()

    content_len = int(headers.get('content-length', '0') or '0')
    while len(body_so_far) < content_len:
        chunk = conn.recv(4096)
        if not chunk:
            break
        body_so_far += chunk
    body = body_so_far[:content_len] if content_len else body_so_far

    _req = {
        'method': method,
        'path': _urlparse.unquote(path),
        'query': query,
        'headers': headers,
        'body': body.decode('utf-8', 'replace'),
    }
    _conn_sock = conn
    return 1


def server_req_method():
    return _req['method']


def server_req_path():
    return _req['path']


def server_req_query(key):
    return _req['query'].get(str(key), '')


def server_req_body():
    return _req['body']


def server_req_header(name):
    return _req['headers'].get(str(name).lower(), '')


_REASONS = {
    200: 'OK', 201: 'Created', 204: 'No Content', 301: 'Moved Permanently',
    302: 'Found', 400: 'Bad Request', 401: 'Unauthorized', 403: 'Forbidden',
    404: 'Not Found', 405: 'Method Not Allowed', 500: 'Internal Server Error',
}


def _send_response(status, body, content_type):
    global _conn_sock
    if _conn_sock is None:
        return 0
    body = '' if body is None else str(body)
    body_bytes = body.encode('utf-8')
    reason = _REASONS.get(int(status), 'OK')
    resp = (
        f"HTTP/1.1 {status} {reason}\r\n"
        f"Content-Type: {content_type}\r\n"
        f"Content-Length: {len(body_bytes)}\r\n"
        f"Connection: close\r\n\r\n"
    ).encode('utf-8') + body_bytes
    try:
        _conn_sock.sendall(resp)
        ok = 1
    except OSError:
        ok = 0
    _conn_sock.close()
    _conn_sock = None
    return ok


def server_respond(status, body):
    return _send_response(status, body, 'text/plain; charset=utf-8')


def server_respond_json(status, json_body):
    return _send_response(status, json_body, 'application/json')


def server_close():
    global _listen_sock, _conn_sock
    if _conn_sock is not None:
        _conn_sock.close()
        _conn_sock = None
    if _listen_sock is not None:
        _listen_sock.close()
        _listen_sock = None


# ── SQL: JaSQL/AbuDB, in-process (no worker needed — Python can just import it) ─
# db_run: no placeholders — trusted/static text only.
# db_run_p: statement has ?/:name placeholders; params is a JSON array or object
# string, bound server-side by jasql.execute()'s own parameter binding — never
# string-concatenated here.

def server_db_run(statement):
    try:
        return str(_jasql.execute(_db, str(statement)))
    except Exception as e:
        return f"Preposterous: {type(e).__name__}: {e}"


def server_db_run_p(statement, params_json):
    import json as _json
    try:
        params = _json.loads(params_json) if params_json else None
        return str(_jasql.execute(_db, str(statement), params=params))
    except Exception as e:
        return f"Preposterous: {type(e).__name__}: {e}"


def server_db_import(path):
    path = str(path)
    ext = _os.path.splitext(path)[1].lower()
    try:
        if ext == '.datac':
            tables = _db.import_datac(path)
        elif ext == '.csv':
            tables = [_db.import_csv(path)]
        elif ext in ('.xlsx', '.xls'):
            tables = [_db.import_xlsx(path)]
        else:
            return f"Preposterous: unsupported import extension: {ext or '(none)'}"
        names = ', '.join(f"'{t.name}'" for t in tables)
        return f"Imported {names} from {path}"
    except Exception as e:
        return f"Preposterous: {type(e).__name__}: {e}"


def server_db_reset():
    global _db
    _db = _Database()
    return 'reset'


def server_db_stop():
    pass  # no worker process in the Python backend — nothing to stop


def server_help():
    print("""=== web-server Library ===
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
server.db_import(path) / db_reset() / db_stop()""")


# Namespace object so AC's `server.listen(...)` etc. resolve (like web.*/maudio.*).
class server:
    listen        = staticmethod(server_listen)
    accept        = staticmethod(server_accept)
    req_method    = staticmethod(server_req_method)
    req_path      = staticmethod(server_req_path)
    req_query     = staticmethod(server_req_query)
    req_body      = staticmethod(server_req_body)
    req_header    = staticmethod(server_req_header)
    respond       = staticmethod(server_respond)
    respond_json  = staticmethod(server_respond_json)
    close         = staticmethod(server_close)
    db_run        = staticmethod(server_db_run)
    db_run_p      = staticmethod(server_db_run_p)
    db_import     = staticmethod(server_db_import)
    db_reset      = staticmethod(server_db_reset)
    db_stop       = staticmethod(server_db_stop)
    help          = staticmethod(server_help)
