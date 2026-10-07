#ifndef AC_WEBSERVER_C_H
#define AC_WEBSERVER_C_H

#ifdef __cplusplus
extern "C" {
#endif

/* HTTP serving — one global listening socket, one "current request" (matches AC's
 * existing singleton-state ilibs: gl's window, camera's device — no handles cross
 * into AC code). Single request in flight at a time; accept() blocks for the next
 * one. */
int         ac_server_listen(int port);
int         ac_server_accept(void);   /* blocks; 1 = request ready, 0 = error/closed */
const char* ac_server_req_method(void);
const char* ac_server_req_path(void);
const char* ac_server_req_query(const char* key);
const char* ac_server_req_body(void);
const char* ac_server_req_header(const char* name);
int         ac_server_respond(int status, const char* body);
int         ac_server_respond_json(int status, const char* json_body);
void        ac_server_close(void);

/* SQL via a lazily-spawned, persistent JaSQL/AbuDB worker (jasql_cli.py --serve),
 * reused for the program's lifetime instead of a process spawn per query.
 * db_run: no binding — for statements with no embedded external/untrusted data.
 * db_run_p: statement uses ? / :name placeholders; params_json is a JSON array
 * (positional) or object (named) string — bound server-side by JaSQL's own
 * parameter binding, never string-concatenated here or by the caller. */
const char* ac_server_db_run(const char* statement);
const char* ac_server_db_run_p(const char* statement, const char* params_json);
const char* ac_server_db_import(const char* path);
const char* ac_server_db_reset(void);
void        ac_server_db_stop(void);

const char* ac_server_help(void);

/* Plain-C call form: the AC compiler emits server_x(...) for the C backend and
 * server.x(...) for C++ — mirrors web_c.h's web_x(...) wrappers. */
static inline int         server_listen(int port) { return ac_server_listen(port); }
static inline int         server_accept(void) { return ac_server_accept(); }
static inline const char* server_req_method(void) { return ac_server_req_method(); }
static inline const char* server_req_path(void) { return ac_server_req_path(); }
static inline const char* server_req_query(const char* key) { return ac_server_req_query(key); }
static inline const char* server_req_body(void) { return ac_server_req_body(); }
static inline const char* server_req_header(const char* name) { return ac_server_req_header(name); }
static inline int         server_respond(int status, const char* body) { return ac_server_respond(status, body); }
static inline int         server_respond_json(int status, const char* json_body) { return ac_server_respond_json(status, json_body); }
static inline void        server_close(void) { ac_server_close(); }

static inline const char* server_db_run(const char* statement) { return ac_server_db_run(statement); }
static inline const char* server_db_run_p(const char* statement, const char* params_json) { return ac_server_db_run_p(statement, params_json); }
static inline const char* server_db_import(const char* path) { return ac_server_db_import(path); }
static inline const char* server_db_reset(void) { return ac_server_db_reset(); }
static inline void        server_db_stop(void) { ac_server_db_stop(); }

static inline const char* server_help(void) { return ac_server_help(); }

#ifdef __cplusplus
}

struct ACWebServerNamespace {
    int  listen(int port) const { return ac_server_listen(port); }
    int  accept() const { return ac_server_accept(); }
    const char* req_method() const { return ac_server_req_method(); }
    const char* req_path() const { return ac_server_req_path(); }
    const char* req_query(const char* key) const { return ac_server_req_query(key); }
    const char* req_body() const { return ac_server_req_body(); }
    const char* req_header(const char* name) const { return ac_server_req_header(name); }
    int  respond(int status, const char* body) const { return ac_server_respond(status, body); }
    int  respond_json(int status, const char* json_body) const { return ac_server_respond_json(status, json_body); }
    void close() const { ac_server_close(); }

    const char* db_run(const char* statement) const { return ac_server_db_run(statement); }
    const char* db_run_p(const char* statement, const char* params_json) const { return ac_server_db_run_p(statement, params_json); }
    const char* db_import(const char* path) const { return ac_server_db_import(path); }
    const char* db_reset() const { return ac_server_db_reset(); }
    void db_stop() const { ac_server_db_stop(); }

    const char* help() const { return ac_server_help(); }
};

static const ACWebServerNamespace server;
#endif

#endif /* AC_WEBSERVER_C_H */
