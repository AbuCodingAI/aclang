#!/usr/bin/env python3
"""
ws_bridge.py <socket_path> <json_request_line>

Node has no synchronous socket API, so it can't hold a persistent connection open
to jasql_cli.py / http_worker.py the way the C++ core does. Instead, JS shells out
to this script once per call (execFileSync — genuinely synchronous from JS's side)
to do ONE request/response round-trip against the already-running worker: connect,
send the given line, read one line back, print it to stdout, exit. The per-call
Python-startup cost (~10-20ms) is the honest tradeoff for JS/Node not having
first-class blocking I/O — see web-server_ffi.js.
"""
import socket
import sys

if len(sys.argv) != 3:
    print('{"ok":false,"error":"usage: ws_bridge.py <socket_path> <json_line>"}')
    sys.exit(1)

socket_path, req_line = sys.argv[1], sys.argv[2]

try:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(socket_path)
    s.sendall(req_line.encode('utf-8') + b'\n')
    buf = b''
    while b'\n' not in buf:
        chunk = s.recv(65536)
        if not chunk:
            break
        buf += chunk
    s.close()
    sys.stdout.write(buf.decode('utf-8', 'replace'))
except Exception as e:
    print('{"ok":false,"error":"bridge: %s: %s"}' % (type(e).__name__, str(e).replace('"', "'")))
