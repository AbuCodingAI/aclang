#!/usr/bin/env python3
"""
http_worker.py --serve <socket_path>

Persistent HTTP-serving worker for the JS backend of AC's `web-server` ilib. JS
(Node) has no synchronous socket API, so it can't do a real blocking accept() the
way the C++ core or native Python backend do — instead it shells out synchronously
(execFileSync + ws_bridge.py) to this long-lived worker, one command per call. This
worker owns the actual listening socket + "current request" state (same
single-request-in-flight singleton model as web-server.cpp / web-server_ffi.py) and
does the real blocking I/O so JS doesn't have to.

Wire protocol: identical shape to jasql_cli.py — newline-delimited JSON, one
connection kept open and reused across calls.

  {"cmd": "listen", "port": N}
  {"cmd": "accept"}                                    -- blocks for next request
  {"cmd": "req_method"} / {"cmd": "req_path"} / {"cmd": "req_body"}
  {"cmd": "req_query", "key": "..."} / {"cmd": "req_header", "name": "..."}
  {"cmd": "respond", "status": N, "body": "...", "content_type": "..."}
  {"cmd": "close"}
"""
import json
import os
import socket
import sys
import urllib.parse

_listen_sock = None
_conn_sock = None
_req = {'method': '', 'path': '', 'query': {}, 'headers': {}, 'body': ''}

_REASONS = {
    200: 'OK', 201: 'Created', 204: 'No Content', 301: 'Moved Permanently',
    302: 'Found', 400: 'Bad Request', 401: 'Unauthorized', 403: 'Forbidden',
    404: 'Not Found', 405: 'Method Not Allowed', 500: 'Internal Server Error',
}


def _do_listen(port):
    global _listen_sock
    if _listen_sock is not None:
        _listen_sock.close()
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.bind(('0.0.0.0', int(port)))
        s.listen(16)
    except OSError as e:
        return {'ok': False, 'error': str(e)}
    _listen_sock = s
    return {'ok': True, 'result': 'listening'}


def _do_accept():
    global _conn_sock, _req
    if _listen_sock is None:
        return {'ok': False, 'error': 'not listening'}
    if _conn_sock is not None:
        _conn_sock.close()
        _conn_sock = None
    try:
        conn, _addr = _listen_sock.accept()
    except OSError as e:
        return {'ok': False, 'error': str(e)}

    buf = b''
    while b'\r\n\r\n' not in buf:
        chunk = conn.recv(4096)
        if not chunk:
            conn.close()
            return {'ok': False, 'error': 'client closed before headers'}
        buf += chunk
        if len(buf) > 1024 * 1024:
            conn.close()
            return {'ok': False, 'error': 'header too large'}
    header_block, _, body_so_far = buf.partition(b'\r\n\r\n')
    lines = header_block.split(b'\r\n')
    req_line = lines[0].decode('latin-1', 'replace')
    parts = req_line.split(' ')
    method = parts[0] if parts else ''
    raw_path = parts[1] if len(parts) > 1 else '/'
    path, _, qs = raw_path.partition('?')
    query = {k: v[0] if v else '' for k, v in urllib.parse.parse_qs(qs, keep_blank_values=True).items()}

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
        'path': urllib.parse.unquote(path),
        'query': query,
        'headers': headers,
        'body': body.decode('utf-8', 'replace'),
    }
    _conn_sock = conn
    return {'ok': True, 'result': 'accepted'}


def _do_respond(status, body, content_type):
    global _conn_sock
    if _conn_sock is None:
        return {'ok': False, 'error': 'no request in flight'}
    body = '' if body is None else str(body)
    body_bytes = body.encode('utf-8')
    reason = _REASONS.get(int(status), 'OK')
    resp = (
        f"HTTP/1.1 {status} {reason}\r\n"
        f"Content-Type: {content_type}\r\n"
        f"Content-Length: {len(body_bytes)}\r\n"
        f"Connection: close\r\n\r\n"
    ).encode('utf-8') + body_bytes
    ok = True
    try:
        _conn_sock.sendall(resp)
    except OSError:
        ok = False
    _conn_sock.close()
    _conn_sock = None
    return {'ok': ok, 'result': 'sent' if ok else 'send failed'}


def _handle(req):
    cmd = req.get('cmd', '')
    if cmd == 'ping':
        return {'ok': True, 'result': 'pong'}
    if cmd == 'listen':
        return _do_listen(req.get('port', 0))
    if cmd == 'accept':
        return _do_accept()
    if cmd == 'req_method':
        return {'ok': True, 'result': _req['method']}
    if cmd == 'req_path':
        return {'ok': True, 'result': _req['path']}
    if cmd == 'req_body':
        return {'ok': True, 'result': _req['body']}
    if cmd == 'req_query':
        return {'ok': True, 'result': _req['query'].get(req.get('key', ''), '')}
    if cmd == 'req_header':
        return {'ok': True, 'result': _req['headers'].get(str(req.get('name', '')).lower(), '')}
    if cmd == 'respond':
        return _do_respond(req.get('status', 200), req.get('body', ''), 'text/plain; charset=utf-8')
    if cmd == 'respond_json':
        return _do_respond(req.get('status', 200), req.get('body', ''), 'application/json')
    if cmd == 'close':
        global _listen_sock, _conn_sock
        if _conn_sock is not None:
            _conn_sock.close(); _conn_sock = None
        if _listen_sock is not None:
            _listen_sock.close(); _listen_sock = None
        return {'ok': True, 'result': 'closed'}
    return {'ok': False, 'error': f'unknown cmd: {cmd}'}


def _serve_conn(conn):
    buf = b''
    while True:
        nl = buf.find(b'\n')
        while nl < 0:
            chunk = conn.recv(65536)
            if not chunk:
                return
            buf += chunk
            nl = buf.find(b'\n')
        line, buf = buf[:nl], buf[nl + 1:]
        if not line.strip():
            continue
        try:
            resp = _handle(json.loads(line.decode('utf-8')))
        except Exception as e:
            resp = {'ok': False, 'error': f'{type(e).__name__}: {e}'}
        try:
            conn.sendall((json.dumps(resp, separators=(',', ':')) + '\n').encode('utf-8'))
        except OSError:
            return


def serve(socket_path):
    if os.path.exists(socket_path):
        os.unlink(socket_path)
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(socket_path)
    srv.listen(4)
    try:
        while True:
            conn, _addr = srv.accept()
            try:
                _serve_conn(conn)
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
        print('usage: http_worker.py --serve <socket_path>', file=sys.stderr)
        sys.exit(1)
