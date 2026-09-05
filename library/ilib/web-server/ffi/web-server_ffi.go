// AC ilib: web-server — Go FFI. Pure Go, no cgo (same policy as web_ffi.go): HTTP
// serving uses net's real blocking Listener.Accept() (Go has OS threads, not an
// event loop, so this just works — no JS-style subprocess-bridge trick needed).
// SQL lazily spawns the persistent jasql_cli.py worker (same one the C++ core and
// JS backend talk to) and keeps ONE net.Conn open to it for the process lifetime.
package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"
)

// ── SQL: persistent jasql_cli.py worker over a Unix socket ────────────────────

var (
	sqlConn   net.Conn
	sqlReader *bufio.Reader
	sqlCmd    *exec.Cmd
	sqlSock   = fmt.Sprintf("/tmp/ac_ws_go_%d.sock", os.Getpid())
)

func serverDir() string {
	_, thisFile, _, _ := runtimeCaller()
	return filepath.Dir(filepath.Dir(thisFile)) // ffi/ -> web-server/
}

// runtimeCaller avoids importing "runtime" just for Caller(); this file is always
// compiled in place by the AC compiler alongside the rest of the generated program,
// so its own source directory isn't meaningful at runtime the way it is in C++'s
// build-time macro. Resolve the ilib dir the same way the compiler itself does:
// relative to cwd, then AC_PATH, mirroring resolveIlibDir in ir_codegen.cpp.
func runtimeCaller() (uintptr, string, int, bool) { return 0, ilibDir("web-server"), 0, true }

// Mirrors resolveIlibDir()'s search order in ir_codegen.cpp (AC_PATH, then cwd,
// then relative to the running binary's own location) — this FFI file is inlined
// as plain text into the user's generated .go file and built from wherever they
// run `go build`, so it can't assume it's running from inside the AC repo.
func ilibDir(lib string) string {
	rel := filepath.Join("library", "ilib", lib)
	if acp := os.Getenv("AC_PATH"); acp != "" {
		if cand := filepath.Join(acp, rel); dirExists(cand) {
			return cand
		}
	}
	if cand := filepath.Join(".", rel); dirExists(cand) {
		return cand
	}
	if exe, err := os.Executable(); err == nil {
		if cand := filepath.Join(filepath.Dir(exe), "..", rel); dirExists(cand) {
			return cand
		}
	}
	return filepath.Join(".", rel)
}

func dirExists(p string) bool {
	st, err := os.Stat(p)
	return err == nil && st.IsDir()
}

func ensureSQLWorker() bool {
	if sqlConn != nil {
		return true
	}
	script := filepath.Join(ilibDir("web-server"), "jasql_cli.py")
	cmd := exec.Command("python3", script, "--serve", sqlSock)
	if err := cmd.Start(); err != nil {
		return false
	}
	sqlCmd = cmd
	for i := 0; i < 150; i++ {
		if conn, err := net.Dial("unix", sqlSock); err == nil {
			sqlConn = conn
			sqlReader = bufio.NewReader(conn)
			return true
		}
		time.Sleep(20 * time.Millisecond)
	}
	return false
}

func sqlRoundtrip(reqObj map[string]interface{}) map[string]interface{} {
	fail := func(msg string) map[string]interface{} {
		return map[string]interface{}{"ok": false, "error": msg}
	}
	if !ensureSQLWorker() {
		return fail("jasql worker failed to start (is python3 on PATH?)")
	}
	line, _ := json.Marshal(reqObj)
	if _, err := sqlConn.Write(append(line, '\n')); err != nil {
		sqlConn.Close()
		sqlConn = nil
		return fail("write failed: " + err.Error())
	}
	respLine, err := sqlReader.ReadString('\n')
	if err != nil {
		sqlConn.Close()
		sqlConn = nil
		return fail("read failed: " + err.Error())
	}
	var resp map[string]interface{}
	if err := json.Unmarshal([]byte(respLine), &resp); err != nil {
		return fail("bad worker response: " + respLine)
	}
	return resp
}

func resultOrError(resp map[string]interface{}) string {
	if ok, _ := resp["ok"].(bool); ok {
		if r, ok2 := resp["result"].(string); ok2 {
			return r
		}
		return fmt.Sprint(resp["result"])
	}
	return "Preposterous: " + fmt.Sprint(resp["error"])
}

// ── HTTP serving: real blocking net.Listener/Accept, minimal HTTP/1.1 parsing ──

var (
	listener net.Listener
	connCur  net.Conn
	reqCur   struct {
		method, path, body string
		query, headers      map[string]string
	}
)

func parseQueryString(qs string) map[string]string {
	out := map[string]string{}
	vals, _ := url.ParseQuery(qs)
	for k, v := range vals {
		if len(v) > 0 {
			out[k] = v[0]
		} else {
			out[k] = ""
		}
	}
	return out
}

// Field names are exact snake_case matches for the calls the compiler emits
// (server.req_method(...), server.db_run_p(...), ...) — the AC codegen emits
// the .acl target verbatim, it does NOT camelCase it, so these must match literally
// (unlike Go convention; underscores in Go identifiers are legal, just unidiomatic).
// int64 (not int) — AC's Go codegen declares call results as int64 by default (see
// camera_ffi.go's identical fix); a plain `int` return here was a "cannot use ... as
// int64 value" compile error at every call site.
var server = struct {
	listen       func(int64) int64
	accept       func() int64
	req_method   func() string
	req_path     func() string
	req_query    func(string) string
	req_body     func() string
	req_header   func(string) string
	respond      func(int64, string) int64
	respond_json func(int64, string) int64
	close        func()
	db_run       func(string) string
	db_run_p     func(string, string) string
	db_import    func(string) string
	db_reset     func() string
	db_stop      func()
	help         func()
}{
	listen: func(port int64) int64 {
		if listener != nil {
			listener.Close()
		}
		l, err := net.Listen("tcp", fmt.Sprintf("0.0.0.0:%d", port))
		if err != nil {
			return 0
		}
		listener = l
		return 1
	},
	accept: func() int64 {
		if listener == nil {
			return 0
		}
		if connCur != nil {
			connCur.Close()
			connCur = nil
		}
		conn, err := listener.Accept()
		if err != nil {
			return 0
		}
		r := bufio.NewReader(conn)
		reqLine, err := r.ReadString('\n')
		if err != nil {
			conn.Close()
			return 0
		}
		parts := strings.Fields(strings.TrimRight(reqLine, "\r\n"))
		method, rawPath := "", "/"
		if len(parts) > 0 {
			method = parts[0]
		}
		if len(parts) > 1 {
			rawPath = parts[1]
		}
		p, qs, _ := strings.Cut(rawPath, "?")
		decodedPath, _ := url.QueryUnescape(p)

		headers := map[string]string{}
		for {
			line, err := r.ReadString('\n')
			if err != nil {
				break
			}
			line = strings.TrimRight(line, "\r\n")
			if line == "" {
				break
			}
			if k, v, ok := strings.Cut(line, ":"); ok {
				headers[strings.ToLower(strings.TrimSpace(k))] = strings.TrimSpace(v)
			}
		}
		contentLen, _ := strconv.Atoi(headers["content-length"])
		bodyBuf := make([]byte, contentLen)
		if contentLen > 0 {
			_, _ = readFull(r, bodyBuf)
		}

		reqCur.method = method
		reqCur.path = decodedPath
		reqCur.query = parseQueryString(qs)
		reqCur.headers = headers
		reqCur.body = string(bodyBuf)
		connCur = conn
		return 1
	},
	req_method: func() string { return reqCur.method },
	req_path:   func() string { return reqCur.path },
	req_body:   func() string { return reqCur.body },
	req_query: func(key string) string {
		if reqCur.query == nil {
			return ""
		}
		return reqCur.query[key]
	},
	req_header: func(name string) string {
		if reqCur.headers == nil {
			return ""
		}
		return reqCur.headers[strings.ToLower(name)]
	},
	respond: func(status int64, body string) int64 {
		return int64(sendResponse(int(status), body, "text/plain; charset=utf-8"))
	},
	respond_json: func(status int64, jsonBody string) int64 {
		return int64(sendResponse(int(status), jsonBody, "application/json"))
	},
	close: func() {
		if connCur != nil {
			connCur.Close()
			connCur = nil
		}
		if listener != nil {
			listener.Close()
			listener = nil
		}
	},
	db_run: func(statement string) string {
		return resultOrError(sqlRoundtrip(map[string]interface{}{"cmd": "run", "stmt": statement}))
	},
	db_run_p: func(statement string, paramsJSON string) string {
		var params interface{}
		if paramsJSON != "" {
			_ = json.Unmarshal([]byte(paramsJSON), &params)
		}
		return resultOrError(sqlRoundtrip(map[string]interface{}{"cmd": "run", "stmt": statement, "params": params}))
	},
	db_import: func(path string) string {
		return resultOrError(sqlRoundtrip(map[string]interface{}{"cmd": "import", "path": path}))
	},
	db_reset: func() string {
		return resultOrError(sqlRoundtrip(map[string]interface{}{"cmd": "reset"}))
	},
	db_stop: func() {
		if sqlConn != nil {
			sqlConn.Close()
			sqlConn = nil
		}
		if sqlCmd != nil && sqlCmd.Process != nil {
			sqlCmd.Process.Kill()
			sqlCmd = nil
		}
	},
	help: func() {
		fmt.Println(`=== web-server Library ===
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
server.db_import(path) / db_reset() / db_stop()`)
	},
}

func readFull(r *bufio.Reader, buf []byte) (int, error) {
	total := 0
	for total < len(buf) {
		n, err := r.Read(buf[total:])
		total += n
		if err != nil {
			return total, err
		}
	}
	return total, nil
}

func sendResponse(status int, body string, contentType string) int {
	if connCur == nil {
		return 0
	}
	reasons := map[int]string{
		200: "OK", 201: "Created", 204: "No Content", 301: "Moved Permanently",
		302: "Found", 400: "Bad Request", 401: "Unauthorized", 403: "Forbidden",
		404: "Not Found", 405: "Method Not Allowed", 500: "Internal Server Error",
	}
	reason, ok := reasons[status]
	if !ok {
		reason = "OK"
	}
	resp := fmt.Sprintf("HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
		status, reason, contentType, len(body), body)
	_, err := connCur.Write([]byte(resp))
	connCur.Close()
	connCur = nil
	return errToInt(err)
}

func errToInt(err error) int {
	if err != nil {
		return 0
	}
	return 1
}
