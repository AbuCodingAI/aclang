// AC ilib: web-server — V FFI. V's `net` module gives real blocking sockets, so
// HTTP serving and the SQL worker connection both work like the C++ core. Needs
// `-enable-globals` (wired into the V invocation in main.cpp) for the module-level
// mutable state a singleton server needs (V discourages globals by default).
import net
import net.unix
import net.urllib
import os
import io
import time

__global (
	listener    &net.TcpListener
	conn        &net.TcpConn
	req_method  string
	req_path    string
	req_body    string
	req_query   map[string]string
	req_headers map[string]string

	sql_conn    &unix.StreamConn
	sql_process &os.Process
)

fn ilib_dir(lib string) string {
	rel := 'library/ilib/${lib}'
	acp := os.getenv('AC_PATH')
	if acp != '' {
		cand := '${acp}/${rel}'
		if os.is_dir(cand) {
			return cand
		}
	}
	if os.is_dir(rel) {
		return rel
	}
	exe_dir := os.dir(os.executable())
	cand2 := '${exe_dir}/../${rel}'
	if os.is_dir(cand2) {
		return cand2
	}
	return rel
}

fn json_escape(s string) string {
	mut out := []u8{}
	for c in s {
		match c {
			`"` { out << `\\` out << `"` }
			`\\` { out << `\\` out << `\\` }
			`\n` { out << `\\` out << `n` }
			`\r` { out << `\\` out << `r` }
			`\t` { out << `\\` out << `t` }
			else { out << c }
		}
	}
	return out.bytestr()
}

fn json_extract_string(json string, key string) string {
	needle := '"${key}":"'
	idx := json.index(needle) or { return '' }
	mut i := idx + needle.len
	mut out := []u8{}
	for i < json.len && json[i] != `"` {
		if json[i] == `\\` && i + 1 < json.len {
			n := json[i + 1]
			match n {
				`n` { out << `\n` }
				`r` { out << `\r` }
				`t` { out << `\t` }
				else { out << n }
			}
			i += 2
		} else {
			out << json[i]
			i++
		}
	}
	return out.bytestr()
}

fn json_extract_ok(json string) bool {
	idx := json.index('"ok":') or { return false }
	rest := json[idx + 5..]
	return rest.starts_with('true')
}

// ── SQL: persistent jasql_cli.py worker over a Unix socket ────────────────────

fn ensure_sql_worker() bool {
	if !isnil(sql_conn) {
		return true
	}
	sock_path := '/tmp/ac_ws_v_${os.getpid()}.sock'
	script := '${ilib_dir('web-server')}/jasql_cli.py'
	// os.Process uses execve, not execvp — it does NOT search PATH itself, unlike
	// python3/subprocess or Go/Rust's exec.Command, so the absolute path must be
	// resolved first.
	py := os.find_abs_path_of_executable('python3') or { return false }
	mut p := os.new_process(py)
	p.set_args([script, '--serve', sock_path])
	p.run()
	sql_process = p
	for _ in 0 .. 150 {
		if os.exists(sock_path) {
			c := unix.connect_stream(sock_path) or { time.sleep(20 * time.millisecond) continue }
			sql_conn = c
			return true
		}
		time.sleep(20 * time.millisecond)
	}
	return false
}

fn sql_roundtrip(req_line string) string {
	if !ensure_sql_worker() {
		return '{"ok":false,"error":"jasql worker failed to start (is python3 on PATH?)"}'
	}
	mut c := sql_conn
	c.write((req_line + '\n').bytes()) or {
		sql_conn = unsafe { nil }
		return '{"ok":false,"error":"jasql worker write failed"}'
	}
	mut buf := []u8{len: 65536}
	mut acc := []u8{}
	for !acc.contains(`\n`) {
		n := c.read(mut buf) or {
			sql_conn = unsafe { nil }
			return '{"ok":false,"error":"jasql worker connection lost"}'
		}
		acc << buf[..n]
	}
	return acc.bytestr()
}

fn db_call(req_line string) string {
	resp := sql_roundtrip(req_line)
	if json_extract_ok(resp) {
		return json_extract_string(resp, 'result')
	}
	return 'Preposterous: ' + json_extract_string(resp, 'error')
}

// ── HTTP serving: real blocking TcpListener/accept, minimal HTTP/1.1 parsing ──

fn reason_for(status int) string {
	return match status {
		200 { 'OK' }
		201 { 'Created' }
		204 { 'No Content' }
		301 { 'Moved Permanently' }
		302 { 'Found' }
		400 { 'Bad Request' }
		401 { 'Unauthorized' }
		403 { 'Forbidden' }
		404 { 'Not Found' }
		405 { 'Method Not Allowed' }
		500 { 'Internal Server Error' }
		else { 'OK' }
	}
}

fn send_response(status int, body string, content_type string) int {
	if isnil(conn) {
		return 0
	}
	resp := 'HTTP/1.1 ${status} ${reason_for(status)}\r\nContent-Type: ${content_type}\r\nContent-Length: ${body.len}\r\nConnection: close\r\n\r\n${body}'
	mut c := conn
	c.write(resp.bytes()) or { conn = unsafe { nil } return 0 }
	c.close() or {}
	conn = unsafe { nil }
	return 1
}

pub struct WebServer {}

pub fn (w WebServer) listen(port int) int {
	l := net.listen_tcp(.ip, ':${port}') or { return 0 }
	listener = l
	return 1
}

pub fn (w WebServer) accept() int {
	if isnil(listener) {
		return 0
	}
	if !isnil(conn) {
		conn.close() or {}
		conn = unsafe { nil }
	}
	mut l := listener
	new_conn := l.accept() or { return 0 }
	mut c := new_conn
	mut reader := io.new_buffered_reader(reader: c)
	req_line := reader.read_line() or { return 0 }
	parts := req_line.trim_space().split(' ')
	req_method = if parts.len > 0 { parts[0] } else { '' }
	raw_target := if parts.len > 1 { parts[1] } else { '/' }
	path_and_qs := raw_target.split_nth('?', 2)
	raw_path := path_and_qs[0]
	qs := if path_and_qs.len > 1 { path_and_qs[1] } else { '' }
	req_path = urllib.query_unescape(raw_path) or { raw_path }
	mut qmap := map[string]string{}
	if qs != '' {
		for pair in qs.split('&') {
			kv := pair.split_nth('=', 2)
			key := urllib.query_unescape(kv[0]) or { kv[0] }
			val := if kv.len > 1 { urllib.query_unescape(kv[1]) or { kv[1] } } else { '' }
			qmap[key] = val
		}
	}
	req_query = qmap.move()

	mut hmap := map[string]string{}
	for {
		line := reader.read_line() or { break }
		trimmed := line.trim_space()
		if trimmed == '' {
			break
		}
		idx := trimmed.index(':') or { continue }
		hmap[trimmed[..idx].trim_space().to_lower()] = trimmed[idx + 1..].trim_space()
	}
	req_headers = hmap.move()

	content_len := hmap['content-length'].int()
	mut body_buf := []u8{len: content_len}
	if content_len > 0 {
		reader.read(mut body_buf) or {}
	}
	req_body = body_buf.bytestr()

	conn = c
	return 1
}

pub fn (w WebServer) req_method() string { return req_method }
pub fn (w WebServer) req_path() string { return req_path }
pub fn (w WebServer) req_body() string { return req_body }
pub fn (w WebServer) req_query(key string) string { return req_query[key] }
pub fn (w WebServer) req_header(name string) string { return req_headers[name.to_lower()] }

pub fn (w WebServer) respond(status int, body string) int {
	return send_response(status, body, 'text/plain; charset=utf-8')
}

pub fn (w WebServer) respond_json(status int, json_body string) int {
	return send_response(status, json_body, 'application/json')
}

pub fn (w WebServer) close() {
	if !isnil(conn) {
		conn.close() or {}
		conn = unsafe { nil }
	}
	if !isnil(listener) {
		listener.close() or {}
		listener = unsafe { nil }
	}
}

pub fn (w WebServer) db_run(statement string) string {
	return db_call('{"cmd":"run","stmt":"${json_escape(statement)}"}')
}

pub fn (w WebServer) db_run_p(statement string, params_json string) string {
	pj := if params_json == '' { 'null' } else { params_json }
	return db_call('{"cmd":"run","stmt":"${json_escape(statement)}","params":${pj}}')
}

pub fn (w WebServer) db_import(path string) string {
	return db_call('{"cmd":"import","path":"${json_escape(path)}"}')
}

pub fn (w WebServer) db_reset() string {
	return db_call('{"cmd":"reset"}')
}

pub fn (w WebServer) db_stop() {
	if !isnil(sql_conn) {
		sql_conn.close() or {}
		sql_conn = unsafe { nil }
	}
	if !isnil(sql_process) {
		mut p := sql_process
		p.signal_term()
		sql_process = unsafe { nil }
	}
}

pub fn (w WebServer) help() {
	help_text := '=== web-server Library ===
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
server.db_import(path) / db_reset() / db_stop()'
	println(help_text)
}

pub const server = WebServer{}
