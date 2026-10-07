#include "web-server_c.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>

#ifndef AC_WEBSERVER_DIR
#define AC_WEBSERVER_DIR "."
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Tiny JSON helpers. Not a general parser — the jasql_cli.py worker always
// replies with the flat shape {"ok":bool,"result":"...")} / {"ok":false,"error":"..."},
// emitted with compact (no-space) separators (see jasql_cli.py), which is all
// json_extract_string/json_extract_ok need to handle.
// ─────────────────────────────────────────────────────────────────────────────

static std::string json_escape(const std::string& s) {
    std::string out; out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) { char buf[8]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
                else out += (char)c;
        }
    }
    return out;
}

static std::string json_extract_string(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return "";
    p += needle.size();
    std::string out;
    while (p < json.size() && json[p] != '"') {
        if (json[p] == '\\' && p + 1 < json.size()) {
            char n = json[p + 1];
            switch (n) {
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case '"': out += '"';  break;
                case '\\': out += '\\'; break;
                default: out += n;
            }
            p += 2;
        } else {
            out += json[p++];
        }
    }
    return out;
}

static bool json_extract_ok(const std::string& json) {
    size_t p = json.find("\"ok\":");
    if (p == std::string::npos) return false;
    p += 5;
    return json.compare(p, 4, "true") == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// SQL: lazily-spawned, persistent jasql_cli.py --serve worker over a Unix socket.
// One connection kept open and reused for the program's lifetime (see the plan's
// "make it a server" note — a process spawn per query would make db_run inside a
// request handler unusably slow).
// ─────────────────────────────────────────────────────────────────────────────

namespace {

pid_t g_workerPid = -1;
int   g_workerFd  = -1;
bool  g_atexitRegistered = false;

std::string workerSocketPath() {
    static std::string path = "/tmp/ac_ws_" + std::to_string(getpid()) + ".sock";
    return path;
}

void stopWorker() {
    if (g_workerFd >= 0) { ::close(g_workerFd); g_workerFd = -1; }
    if (g_workerPid > 0) {
        kill(g_workerPid, SIGTERM);
        int status; waitpid(g_workerPid, &status, 0);
        g_workerPid = -1;
    }
    unlink(workerSocketPath().c_str());
}

bool connectWorker(const std::string& path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) != 0) { ::close(fd); return false; }
    g_workerFd = fd;
    return true;
}

bool ensureWorker() {
    if (g_workerFd >= 0) return true;
    std::string path   = workerSocketPath();
    std::string script = std::string(AC_WEBSERVER_DIR) + "/jasql_cli.py";

    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) { dup2(devnull, 0); dup2(devnull, 1); }
        execlp("python3", "python3", script.c_str(), "--serve", path.c_str(), (char*)nullptr);
        _exit(127); // python3 not found / exec failed
    }
    if (pid < 0) return false;
    g_workerPid = pid;
    if (!g_atexitRegistered) { atexit(stopWorker); g_atexitRegistered = true; }

    for (int i = 0; i < 150; i++) { // ~3s worst case for the interpreter to start
        struct stat st{};
        if (stat(path.c_str(), &st) == 0 && connectWorker(path)) return true;
        usleep(20 * 1000);
    }
    return false;
}

// Send one request line, read one response line. One reconnect/respawn attempt
// if the worker died between calls (e.g. killed externally).
std::string workerRoundtrip(const std::string& reqLine) {
    for (int attempt = 0; attempt < 2; attempt++) {
        if (!ensureWorker())
            return "{\"ok\":false,\"error\":\"jasql worker failed to start (is python3 on PATH?)\"}";
        std::string line = reqLine + "\n";
        if (::write(g_workerFd, line.data(), line.size()) != (ssize_t)line.size()) {
            ::close(g_workerFd); g_workerFd = -1;
            continue;
        }
        std::string buf;
        char chunk[4096];
        bool died = false;
        while (buf.find('\n') == std::string::npos) {
            ssize_t n = ::read(g_workerFd, chunk, sizeof(chunk));
            if (n <= 0) { ::close(g_workerFd); g_workerFd = -1; died = true; break; }
            buf.append(chunk, n);
        }
        if (!died) return buf;
    }
    return "{\"ok\":false,\"error\":\"jasql worker connection lost\"}";
}

std::string dbCall(const std::string& reqLine) {
    std::string resp = workerRoundtrip(reqLine);
    return json_extract_ok(resp) ? json_extract_string(resp, "result")
                                  : ("Preposterous: " + json_extract_string(resp, "error"));
}

} // namespace

extern "C" {

const char* ac_server_db_run(const char* statement) {
    static std::string result;
    result = dbCall("{\"cmd\":\"run\",\"stmt\":\"" + json_escape(statement ? statement : "") + "\"}");
    return result.c_str();
}

const char* ac_server_db_run_p(const char* statement, const char* params_json) {
    static std::string result;
    std::string pj = (params_json && *params_json) ? params_json : "null";
    result = dbCall("{\"cmd\":\"run\",\"stmt\":\"" + json_escape(statement ? statement : "")
                     + "\",\"params\":" + pj + "}");
    return result.c_str();
}

const char* ac_server_db_import(const char* path) {
    static std::string result;
    result = dbCall("{\"cmd\":\"import\",\"path\":\"" + json_escape(path ? path : "") + "\"}");
    return result.c_str();
}

const char* ac_server_db_reset(void) {
    static std::string result;
    result = dbCall("{\"cmd\":\"reset\"}");
    return result.c_str();
}

void ac_server_db_stop(void) { stopWorker(); }

} // extern "C"

// ─────────────────────────────────────────────────────────────────────────────
// HTTP serving: one global listening socket + one "current request" (same
// singleton-state model gl/camera already use — no handles cross into AC code).
// Minimal HTTP/1.1: request line + headers + Content-Length body. No chunked
// transfer-encoding, no keep-alive (every response closes the connection) —
// enough for a request/response server, not a production reverse-proxy target.
// ─────────────────────────────────────────────────────────────────────────────

namespace {

int g_listenFd = -1;
int g_connFd   = -1;

struct HttpRequest {
    std::string method, path, body;
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> headers; // lowercased keys
};
HttpRequest g_req;

std::string toLowerStr(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

std::string urlDecode(const std::string& s) {
    auto hexval = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    std::string out; out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() &&
            isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) {
            out += (char)((hexval(s[i + 1]) << 4) | hexval(s[i + 2]));
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

void parseQueryString(const std::string& qs, std::map<std::string, std::string>& out) {
    size_t i = 0;
    while (i < qs.size()) {
        size_t amp = qs.find('&', i);
        std::string pair = qs.substr(i, amp == std::string::npos ? std::string::npos : amp - i);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) out[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
        else if (!pair.empty())      out[urlDecode(pair)] = "";
        if (amp == std::string::npos) break;
        i = amp + 1;
    }
}

int sendResponse(int status, const char* body, const char* contentType) {
    if (g_connFd < 0) return 0;
    if (!body) body = "";
    static const std::map<int, std::string> reasons = {
        {200, "OK"}, {201, "Created"}, {204, "No Content"}, {301, "Moved Permanently"},
        {302, "Found"}, {400, "Bad Request"}, {401, "Unauthorized"}, {403, "Forbidden"},
        {404, "Not Found"}, {405, "Method Not Allowed"}, {500, "Internal Server Error"},
    };
    auto it = reasons.find(status);
    std::string reason = (it == reasons.end()) ? "OK" : it->second;
    std::ostringstream out;
    out << "HTTP/1.1 " << status << " " << reason << "\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << strlen(body) << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    std::string s = out.str();
    ssize_t written = ::write(g_connFd, s.data(), s.size());
    ::close(g_connFd);
    g_connFd = -1;
    return written == (ssize_t)s.size() ? 1 : 0;
}

} // namespace

extern "C" {

int ac_server_listen(int port) {
    if (g_listenFd >= 0) { ::close(g_listenFd); g_listenFd = -1; }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);
    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) != 0) { ::close(fd); return 0; }
    if (::listen(fd, 16) != 0) { ::close(fd); return 0; }
    g_listenFd = fd;
    return 1;
}

int ac_server_accept(void) {
    if (g_listenFd < 0) return 0;
    if (g_connFd >= 0) { ::close(g_connFd); g_connFd = -1; } // prior request never got respond()'d

    sockaddr_in peer{}; socklen_t plen = sizeof(peer);
    int fd = ::accept(g_listenFd, (sockaddr*)&peer, &plen);
    if (fd < 0) return 0;

    std::string buf;
    char chunk[4096];
    size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos) {
        ssize_t n = ::read(fd, chunk, sizeof(chunk));
        if (n <= 0) { ::close(fd); return 0; }
        buf.append(chunk, n);
        headerEnd = buf.find("\r\n\r\n");
        if (buf.size() > 1 * 1024 * 1024) { ::close(fd); return 0; } // header too large
    }
    std::string headerBlock = buf.substr(0, headerEnd);
    std::string bodySoFar   = buf.substr(headerEnd + 4);

    g_req = HttpRequest{};
    size_t lineEnd = headerBlock.find("\r\n");
    std::string reqLine = headerBlock.substr(0, lineEnd);
    {
        std::istringstream ls(reqLine);
        std::string rawPath;
        ls >> g_req.method >> rawPath;
        size_t q = rawPath.find('?');
        if (q == std::string::npos) {
            g_req.path = urlDecode(rawPath);
        } else {
            g_req.path = urlDecode(rawPath.substr(0, q));
            parseQueryString(rawPath.substr(q + 1), g_req.query);
        }
    }
    size_t p = (lineEnd == std::string::npos) ? headerBlock.size() : lineEnd + 2;
    while (p < headerBlock.size()) {
        size_t nl = headerBlock.find("\r\n", p);
        std::string hline = headerBlock.substr(p, nl == std::string::npos ? std::string::npos : nl - p);
        size_t colon = hline.find(':');
        if (colon != std::string::npos) {
            std::string k = toLowerStr(hline.substr(0, colon));
            std::string v = hline.substr(colon + 1);
            size_t vs = v.find_first_not_of(" \t");
            g_req.headers[k] = (vs == std::string::npos) ? "" : v.substr(vs);
        }
        if (nl == std::string::npos) break;
        p = nl + 2;
    }

    size_t contentLen = 0;
    auto clIt = g_req.headers.find("content-length");
    if (clIt != g_req.headers.end()) contentLen = (size_t)strtoul(clIt->second.c_str(), nullptr, 10);
    while (bodySoFar.size() < contentLen) {
        ssize_t n = ::read(fd, chunk, sizeof(chunk));
        if (n <= 0) break;
        bodySoFar.append(chunk, n);
    }
    g_req.body = bodySoFar.substr(0, contentLen ? contentLen : bodySoFar.size());

    g_connFd = fd;
    return 1;
}

const char* ac_server_req_method(void) { return g_req.method.c_str(); }
const char* ac_server_req_path(void)   { return g_req.path.c_str(); }
const char* ac_server_req_body(void)   { return g_req.body.c_str(); }

const char* ac_server_req_query(const char* key) {
    static std::string out;
    auto it = g_req.query.find(key ? key : "");
    out = (it == g_req.query.end()) ? "" : it->second;
    return out.c_str();
}

const char* ac_server_req_header(const char* name) {
    static std::string out;
    auto it = g_req.headers.find(toLowerStr(name ? name : ""));
    out = (it == g_req.headers.end()) ? "" : it->second;
    return out.c_str();
}

int ac_server_respond(int status, const char* body) {
    return sendResponse(status, body, "text/plain; charset=utf-8");
}

int ac_server_respond_json(int status, const char* json_body) {
    return sendResponse(status, json_body, "application/json");
}

void ac_server_close(void) {
    if (g_connFd >= 0)   { ::close(g_connFd);   g_connFd   = -1; }
    if (g_listenFd >= 0) { ::close(g_listenFd); g_listenFd = -1; }
}

const char* ac_server_help(void) {
    return
        "=== web-server Library ===\n"
        "One request in flight at a time (single blocking accept loop, no threads).\n\n"
        "server.listen(port)\n"
        "server.accept()                          -- blocks for next request\n"
        "server.req_method() / req_path() / req_body()\n"
        "server.req_query(key) / req_header(name)\n"
        "server.respond(status, body) / respond_json(status, json_body)\n"
        "server.close()\n\n"
        "server.db_run(statement)                 -- no placeholders; trusted/static text only\n"
        "server.db_run_p(statement, paramsJson)    -- ? / :name placeholders, bound server-side\n"
        "                                                 by JaSQL (JSON array/object string) --\n"
        "                                                 the safe path for request-derived data\n"
        "server.db_import(path) / db_reset() / db_stop()\n";
}

} // extern "C"
