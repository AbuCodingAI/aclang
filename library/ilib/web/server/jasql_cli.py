#!/usr/bin/env python3
"""
jasql_cli.py --serve <socket_path>

Persistent JaSQL/AbuDB worker for AC's `web-server` ilib. Lazily spawned once by the
C++ core (web-server.cpp) and kept alive for the program's lifetime, instead of
forking a fresh Python interpreter per query — a program calling server.db_run()
from inside a request handler needs that to be cheap, not a process spawn each time.

Wire protocol: newline-delimited JSON over a Unix domain socket, one connection kept
open and reused for many requests.

  request:  {"cmd": "run", "stmt": "...", "params": [...] | {"name": val, ...}}
            -- one JaSQL statement; ? / :name placeholders are bound server-side by
            -- jasql.py's own _bind_params — never string-concatenated by the caller
            -- or by this file. See jasql.py:474 / jasql.py:1610-1612.
            {"cmd": "import", "path": "..."}   -- .datac / .csv / .xlsx by extension
            {"cmd": "reset"}                   -- fresh empty Database()
            {"cmd": "ping"}                    -- liveness check

  response: {"ok": true,  "result": "<text>"}
            {"ok": false, "error": "<text>"}
"""
import json
import os
import socket
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_HERE, "jasql_vendor"))

import jasql          # noqa: E402  (vendored — see jasql_vendor/)
from abudb import Database  # noqa: E402


def _do_import(db, path):
    ext = os.path.splitext(path)[1].lower()
    if ext == '.datac':
        tables = db.import_datac(path)
    elif ext == '.csv':
        tables = [db.import_csv(path)]
    elif ext in ('.xlsx', '.xls'):
        tables = [db.import_xlsx(path)]
    else:
        raise ValueError(f"unsupported import extension: {ext or '(none)'}")
    names = ', '.join(f"'{t.name}'" for t in tables)
    return f"Imported {names} from {path}"


def _handle(state, req):
    """state = {'db': Database()}; mutated in place for 'reset'."""
    cmd = req.get('cmd', 'run')
    if cmd == 'ping':
        return {'ok': True, 'result': 'pong'}
    if cmd == 'reset':
        state['db'] = Database()
        return {'ok': True, 'result': 'reset'}
    if cmd == 'import':
        return {'ok': True, 'result': _do_import(state['db'], req['path'])}
    # default: run a JaSQL statement
    stmt = req.get('stmt', '')
    params = req.get('params')
    result = jasql.execute(state['db'], stmt, params=params)
    return {'ok': True, 'result': str(result)}


def _serve_conn(conn, state):
    buf = b''
    while True:
        nl = buf.find(b'\n')
        while nl < 0:
            chunk = conn.recv(65536)
            if not chunk:
                return  # client disconnected
            buf += chunk
            nl = buf.find(b'\n')
        line, buf = buf[:nl], buf[nl + 1:]
        if not line.strip():
            continue
        try:
            req = json.loads(line.decode('utf-8'))
            resp = _handle(state, req)
        except Exception as e:  # keep the worker alive across a bad request
            resp = {'ok': False, 'error': f"{type(e).__name__}: {e}"}
        try:
            # Compact, whitespace-free separators: the C++ core's hand-rolled reader
            # matches on `"key":` with no space after the colon.
            conn.sendall((json.dumps(resp, separators=(',', ':')) + '\n').encode('utf-8'))
        except OSError:
            return


def serve(socket_path):
    if os.path.exists(socket_path):
        os.unlink(socket_path)
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(socket_path)
    srv.listen(1)
    state = {'db': Database()}
    try:
        while True:
            conn, _addr = srv.accept()
            try:
                _serve_conn(conn, state)
            finally:
                conn.close()
    finally:
        srv.close()
        try:
            os.unlink(socket_path)
        except OSError:
            pass


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--serve':
        serve(sys.argv[2])
    else:
        print('usage: jasql_cli.py --serve <socket_path>', file=sys.stderr)
        sys.exit(1)
