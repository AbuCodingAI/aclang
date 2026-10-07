/* BNY backend FFI for web-server library. Dynamically loads libacserver.so,
 * which already implements HTTP serving AND SQL entirely in C++ (real sockets,
 * a persistent jasql_cli.py worker) — this shim is pure dlopen/dlsym call-through,
 * no raw-socket or protocol work needed in machine code, same as db_run already
 * was: every server.* function is just one more dlsym lookup into the .so.
 *
 * KNOWN BUG (2026-07-30, unresolved): db_run/db_run_p/db_import/db_reset/db_stop
 * are verified working end-to-end through the real `ac` compiler on BNY.
 * server.listen(port) alone also works. But server.accept() called right
 * after listen() does NOT actually block for a connection — it returns 1
 * immediately (confirmed: a curl request made after accept() "succeeds" fails to
 * connect, meaning the process had already exited). Root cause not found; it's
 * somewhere in exp_bny.cpp's raw x86-64 call-emission for back-to-back external
 * calls into this .so (this file's dlsym wrappers are exercised correctly in
 * isolation — the bug is upstream of them, in how BNY emits/sequences the calls),
 * not in this shim. Needs real debugging time in exp_bny.cpp before HTTP serving
 * on BNY can be trusted; SQL is unaffected and safe to use.
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

typedef const char* (*server_db_run_t)(const char*);
typedef const char* (*server_db_run_p_t)(const char*, const char*);
typedef const char* (*server_db_import_t)(const char*);
typedef const char* (*server_db_reset_t)(void);
typedef void        (*server_db_stop_t)(void);
typedef const char* (*server_help_t)(void);

typedef int         (*server_listen_t)(int);
typedef int         (*server_accept_t)(void);
typedef const char* (*server_req_method_t)(void);
typedef const char* (*server_req_path_t)(void);
typedef const char* (*server_req_query_t)(const char*);
typedef const char* (*server_req_body_t)(void);
typedef const char* (*server_req_header_t)(const char*);
typedef int         (*server_respond_t)(int, const char*);
typedef int         (*server_respond_json_t)(int, const char*);
typedef void        (*server_close_t)(void);

static void* lib_handle = NULL;

static void load_library() {
    if (lib_handle) return;
    lib_handle = dlopen("libacserver.so", RTLD_LAZY);
    if (!lib_handle) {
        fprintf(stderr, "Error loading libacserver.so: %s\n", dlerror());
        exit(1);
    }
}

const char* server_db_run(const char* statement) {
    load_library();
    server_db_run_t func = (server_db_run_t)dlsym(lib_handle, "ac_server_db_run");
    if (!func) { fprintf(stderr, "Error: ac_server_db_run not found\n"); return ""; }
    return func(statement);
}

const char* server_db_run_p(const char* statement, const char* params_json) {
    load_library();
    server_db_run_p_t func = (server_db_run_p_t)dlsym(lib_handle, "ac_server_db_run_p");
    if (!func) { fprintf(stderr, "Error: ac_server_db_run_p not found\n"); return ""; }
    return func(statement, params_json);
}

const char* server_db_import(const char* path) {
    load_library();
    server_db_import_t func = (server_db_import_t)dlsym(lib_handle, "ac_server_db_import");
    if (!func) { fprintf(stderr, "Error: ac_server_db_import not found\n"); return ""; }
    return func(path);
}

const char* server_db_reset() {
    load_library();
    server_db_reset_t func = (server_db_reset_t)dlsym(lib_handle, "ac_server_db_reset");
    if (!func) { fprintf(stderr, "Error: ac_server_db_reset not found\n"); return ""; }
    return func();
}

void server_db_stop() {
    load_library();
    server_db_stop_t func = (server_db_stop_t)dlsym(lib_handle, "ac_server_db_stop");
    if (!func) { fprintf(stderr, "Error: ac_server_db_stop not found\n"); return; }
    func();
}

const char* server_help() {
    load_library();
    server_help_t func = (server_help_t)dlsym(lib_handle, "ac_server_help");
    if (!func) { fprintf(stderr, "Error: ac_server_help not found\n"); return ""; }
    return func();
}

int server_listen(int port) {
    load_library();
    server_listen_t func = (server_listen_t)dlsym(lib_handle, "ac_server_listen");
    if (!func) { fprintf(stderr, "Error: ac_server_listen not found\n"); return 0; }
    return func(port);
}

int server_accept() {
    load_library();
    server_accept_t func = (server_accept_t)dlsym(lib_handle, "ac_server_accept");
    if (!func) { fprintf(stderr, "Error: ac_server_accept not found\n"); return 0; }
    return func();
}

const char* server_req_method() {
    load_library();
    server_req_method_t func = (server_req_method_t)dlsym(lib_handle, "ac_server_req_method");
    if (!func) { fprintf(stderr, "Error: ac_server_req_method not found\n"); return ""; }
    return func();
}

const char* server_req_path() {
    load_library();
    server_req_path_t func = (server_req_path_t)dlsym(lib_handle, "ac_server_req_path");
    if (!func) { fprintf(stderr, "Error: ac_server_req_path not found\n"); return ""; }
    return func();
}

const char* server_req_query(const char* key) {
    load_library();
    server_req_query_t func = (server_req_query_t)dlsym(lib_handle, "ac_server_req_query");
    if (!func) { fprintf(stderr, "Error: ac_server_req_query not found\n"); return ""; }
    return func(key);
}

const char* server_req_body() {
    load_library();
    server_req_body_t func = (server_req_body_t)dlsym(lib_handle, "ac_server_req_body");
    if (!func) { fprintf(stderr, "Error: ac_server_req_body not found\n"); return ""; }
    return func();
}

const char* server_req_header(const char* name) {
    load_library();
    server_req_header_t func = (server_req_header_t)dlsym(lib_handle, "ac_server_req_header");
    if (!func) { fprintf(stderr, "Error: ac_server_req_header not found\n"); return ""; }
    return func(name);
}

int server_respond(int status, const char* body) {
    load_library();
    server_respond_t func = (server_respond_t)dlsym(lib_handle, "ac_server_respond");
    if (!func) { fprintf(stderr, "Error: ac_server_respond not found\n"); return 0; }
    return func(status, body);
}

int server_respond_json(int status, const char* json_body) {
    load_library();
    server_respond_json_t func = (server_respond_json_t)dlsym(lib_handle, "ac_server_respond_json");
    if (!func) { fprintf(stderr, "Error: ac_server_respond_json not found\n"); return 0; }
    return func(status, json_body);
}

void server_close() {
    load_library();
    server_close_t func = (server_close_t)dlsym(lib_handle, "ac_server_close");
    if (!func) { fprintf(stderr, "Error: ac_server_close not found\n"); return; }
    func();
}
