// AC ilib: web-server — Rust FFI. Pure std, no external crate: std::net gives real
// blocking sockets (TcpListener/UnixStream) so HTTP serving and the SQL worker
// connection both work exactly like the C++ core — no JS-style subprocess-bridge
// trick needed here.
use std::cell::RefCell;
use std::collections::HashMap;
// Write is already imported by the compiler's own Rust codegen boilerplate — a
// second `use std::io::Write` here would collide with it, so it's deliberately
// left out; `.write_all(...)` calls below still resolve against that import.
use std::io::{Read, BufRead, BufReader};
use std::net::{TcpListener, TcpStream};
use std::os::unix::net::UnixStream;
use std::path::{Path, PathBuf};
use std::process::{Child, Command};

#[derive(Default)]
struct HttpRequest {
    method: String,
    path: String,
    query: HashMap<String, String>,
    headers: HashMap<String, String>,
    body: String,
}

#[derive(Default)]
struct WsState {
    listener: Option<TcpListener>,
    conn: Option<TcpStream>,
    req: HttpRequest,
    sql_conn: Option<UnixStream>,
    sql_child: Option<Child>,
}

thread_local! {
    static STATE: RefCell<WsState> = RefCell::new(WsState::default());
}

fn ilib_dir(lib: &str) -> PathBuf {
    let rel = Path::new("library").join("ilib").join(lib);
    if let Ok(acp) = std::env::var("AC_PATH") {
        let cand = Path::new(&acp).join(&rel);
        if cand.is_dir() { return cand; }
    }
    let cand = Path::new(".").join(&rel);
    if cand.is_dir() { return cand; }
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            let cand = dir.join("..").join(&rel);
            if cand.is_dir() { return cand; }
        }
    }
    Path::new(".").join(&rel)
}

fn url_decode(s: &str) -> String {
    let bytes = s.as_bytes();
    let mut out = Vec::with_capacity(bytes.len());
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'%' && i + 2 < bytes.len() {
            if let Ok(v) = u8::from_str_radix(&s[i + 1..i + 3], 16) {
                out.push(v);
                i += 3;
                continue;
            }
        }
        if bytes[i] == b'+' { out.push(b' '); } else { out.push(bytes[i]); }
        i += 1;
    }
    String::from_utf8_lossy(&out).into_owned()
}

fn parse_query(qs: &str) -> HashMap<String, String> {
    let mut out = HashMap::new();
    if qs.is_empty() { return out; }
    for pair in qs.split('&') {
        let mut it = pair.splitn(2, '=');
        let k = it.next().unwrap_or("");
        let v = it.next().unwrap_or("");
        if !k.is_empty() { out.insert(url_decode(k), url_decode(v)); }
    }
    out
}

// ── SQL: persistent jasql_cli.py worker over a Unix socket ────────────────────

fn ensure_sql_worker(st: &mut WsState) -> bool {
    if st.sql_conn.is_some() { return true; }
    let sock_path = format!("/tmp/ac_ws_rs_{}.sock", std::process::id());
    let script = ilib_dir("web-server").join("jasql_cli.py");
    match Command::new("python3").arg(&script).arg("--serve").arg(&sock_path).spawn() {
        Ok(child) => st.sql_child = Some(child),
        Err(_) => return false,
    }
    for _ in 0..150 {
        if Path::new(&sock_path).exists() {
            if let Ok(conn) = UnixStream::connect(&sock_path) {
                st.sql_conn = Some(conn);
                return true;
            }
        }
        std::thread::sleep(std::time::Duration::from_millis(20));
    }
    false
}

fn sql_roundtrip(st: &mut WsState, req_line: &str) -> String {
    if !ensure_sql_worker(st) {
        return r#"{"ok":false,"error":"jasql worker failed to start (is python3 on PATH?)"}"#.to_string();
    }
    let conn = st.sql_conn.as_mut().unwrap();
    if conn.write_all(req_line.as_bytes()).and_then(|_| conn.write_all(b"\n")).is_err() {
        st.sql_conn = None;
        return r#"{"ok":false,"error":"jasql worker write failed"}"#.to_string();
    }
    let mut reader = BufReader::new(conn.try_clone().unwrap());
    let mut line = String::new();
    if reader.read_line(&mut line).unwrap_or(0) == 0 {
        st.sql_conn = None;
        return r#"{"ok":false,"error":"jasql worker connection lost"}"#.to_string();
    }
    line
}

fn json_escape(s: &str) -> String {
    let mut out = String::with_capacity(s.len() + 8);
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if (c as u32) < 0x20 => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out
}

fn json_extract_string(json: &str, key: &str) -> String {
    let needle = format!("\"{}\":\"", key);
    let Some(start) = json.find(&needle) else { return String::new(); };
    let mut chars = json[start + needle.len()..].chars();
    let mut out = String::new();
    while let Some(c) = chars.next() {
        match c {
            '"' => break,
            '\\' => match chars.next() {
                Some('n') => out.push('\n'),
                Some('r') => out.push('\r'),
                Some('t') => out.push('\t'),
                Some(o) => out.push(o),
                None => break,
            },
            c => out.push(c),
        }
    }
    out
}

fn json_extract_ok(json: &str) -> bool {
    json.find("\"ok\":").map(|p| json[p + 5..].starts_with("true")).unwrap_or(false)
}

fn db_call(st: &mut WsState, req_line: &str) -> String {
    let resp = sql_roundtrip(st, req_line);
    if json_extract_ok(&resp) {
        json_extract_string(&resp, "result")
    } else {
        format!("Preposterous: {}", json_extract_string(&resp, "error"))
    }
}

// ── HTTP serving: real blocking TcpListener/accept, minimal HTTP/1.1 parsing ──

const REASONS: &[(i64, &str)] = &[
    (200, "OK"), (201, "Created"), (204, "No Content"), (301, "Moved Permanently"),
    (302, "Found"), (400, "Bad Request"), (401, "Unauthorized"), (403, "Forbidden"),
    (404, "Not Found"), (405, "Method Not Allowed"), (500, "Internal Server Error"),
];

fn send_response(st: &mut WsState, status: i64, body: &str, content_type: &str) -> i64 {
    let Some(mut conn) = st.conn.take() else { return 0; };
    let reason = REASONS.iter().find(|(c, _)| *c == status).map(|(_, r)| *r).unwrap_or("OK");
    let resp = format!(
        "HTTP/1.1 {} {}\r\nContent-Type: {}\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
        status, reason, content_type, body.len(), body
    );
    let ok = conn.write_all(resp.as_bytes()).is_ok();
    let _ = conn.shutdown(std::net::Shutdown::Both);
    if ok { 1 } else { 0 }
}

pub struct WebServerLib;

impl WebServerLib {
    pub fn listen(&self, port: i64) -> i64 {
        STATE.with(|s| {
            let mut st = s.borrow_mut();
            match TcpListener::bind(("0.0.0.0", port as u16)) {
                Ok(l) => { st.listener = Some(l); 1 }
                Err(_) => 0,
            }
        })
    }

    pub fn accept(&self) -> i64 {
        STATE.with(|s| {
            let mut st = s.borrow_mut();
            st.conn = None;
            let Some(listener) = st.listener.as_ref() else { return 0; };
            let Ok((mut conn, _)) = listener.accept() else { return 0; };

            let mut reader = BufReader::new(conn.try_clone().unwrap());
            let mut req_line = String::new();
            if reader.read_line(&mut req_line).unwrap_or(0) == 0 { return 0; }
            let parts: Vec<&str> = req_line.trim_end().splitn(2, ' ').collect();
            let method = parts.get(0).unwrap_or(&"").to_string();
            let raw_target = parts.get(1).map(|s| s.split(' ').next().unwrap_or(s)).unwrap_or("/");
            let (raw_path, qs) = match raw_target.split_once('?') {
                Some((p, q)) => (p, q),
                None => (raw_target, ""),
            };
            let path = url_decode(raw_path);
            let query = parse_query(qs);

            let mut headers = HashMap::new();
            loop {
                let mut hline = String::new();
                if reader.read_line(&mut hline).unwrap_or(0) == 0 { break; }
                let trimmed = hline.trim_end();
                if trimmed.is_empty() { break; }
                if let Some((k, v)) = trimmed.split_once(':') {
                    headers.insert(k.trim().to_lowercase(), v.trim().to_string());
                }
            }
            let content_len: usize = headers.get("content-length").and_then(|v| v.parse().ok()).unwrap_or(0);
            let mut body_buf = vec![0u8; content_len];
            if content_len > 0 { let _ = reader.read_exact(&mut body_buf); }

            st.req = HttpRequest {
                method, path, query, headers,
                body: String::from_utf8_lossy(&body_buf).into_owned(),
            };
            let _ = &mut conn; // keep original (unbuffered) handle for writing the response
            st.conn = Some(conn);
            1
        })
    }

    pub fn req_method(&self) -> String { STATE.with(|s| s.borrow().req.method.clone()) }
    pub fn req_path(&self)   -> String { STATE.with(|s| s.borrow().req.path.clone()) }
    pub fn req_body(&self)   -> String { STATE.with(|s| s.borrow().req.body.clone()) }
    pub fn req_query(&self, key: &str) -> String {
        STATE.with(|s| s.borrow().req.query.get(key).cloned().unwrap_or_default())
    }
    pub fn req_header(&self, name: &str) -> String {
        STATE.with(|s| s.borrow().req.headers.get(&name.to_lowercase()).cloned().unwrap_or_default())
    }

    pub fn respond(&self, status: i64, body: &str) -> i64 {
        STATE.with(|s| send_response(&mut s.borrow_mut(), status, body, "text/plain; charset=utf-8"))
    }
    pub fn respond_json(&self, status: i64, json_body: &str) -> i64 {
        STATE.with(|s| send_response(&mut s.borrow_mut(), status, json_body, "application/json"))
    }

    pub fn close(&self) {
        STATE.with(|s| {
            let mut st = s.borrow_mut();
            st.conn = None;
            st.listener = None;
        });
    }

    pub fn db_run(&self, statement: &str) -> String {
        STATE.with(|s| {
            let req = format!(r#"{{"cmd":"run","stmt":"{}"}}"#, json_escape(statement));
            db_call(&mut s.borrow_mut(), &req)
        })
    }

    pub fn db_run_p(&self, statement: &str, params_json: &str) -> String {
        STATE.with(|s| {
            let pj = if params_json.is_empty() { "null" } else { params_json };
            let req = format!(r#"{{"cmd":"run","stmt":"{}","params":{}}}"#, json_escape(statement), pj);
            db_call(&mut s.borrow_mut(), &req)
        })
    }

    pub fn db_import(&self, path: &str) -> String {
        STATE.with(|s| {
            let req = format!(r#"{{"cmd":"import","path":"{}"}}"#, json_escape(path));
            db_call(&mut s.borrow_mut(), &req)
        })
    }

    pub fn db_reset(&self) -> String {
        STATE.with(|s| db_call(&mut s.borrow_mut(), r#"{"cmd":"reset"}"#))
    }

    pub fn db_stop(&self) {
        STATE.with(|s| {
            let mut st = s.borrow_mut();
            st.sql_conn = None;
            if let Some(mut child) = st.sql_child.take() {
                let _ = child.kill();
            }
        });
    }

    pub fn help(&self) -> String {
        let help_text = r#"=== web-server Library ===
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
server.db_import(path) / db_reset() / db_stop()"#;
        println!("{}", help_text);
        help_text.to_string()
    }
}

pub static server: WebServerLib = WebServerLib;
