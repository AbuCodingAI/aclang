// C++ backend FFI for web library
// Dynamically loads libacweb.so and calls exported functions

#include <dlfcn.h>
#include <cstdlib>
#include <iostream>
#include <cstring>

typedef void (*web_open_t)(const char*);
typedef void (*web_file_open_t)(const char*);
typedef void (*web_popen_t)(const char*);
typedef void (*web_ropen_t)(const char*);
typedef void (*web_browser_t)();
typedef int (*web_pdf_t)(const char*);
typedef int (*web_text_t)(const char*);
typedef int (*web_inspect_t)(const char*);
typedef void (*web_ac_page_t)();
typedef const char* (*web_help_t)();

static void* lib_handle = nullptr;

static void load_library() {
    if (lib_handle) return;
    
    const char* lib_path = "libacweb.so";
    lib_handle = dlopen(lib_path, RTLD_LAZY);
    
    if (!lib_handle) {
        std::cerr << "Error loading libacweb.so: " << dlerror() << std::endl;
        std::exit(1);
    }
}

extern "C" {

void web_open(const char* link) {
    load_library();
    web_open_t func = (web_open_t)dlsym(lib_handle, "ac_web_open");
    if (!func) {
        std::cerr << "Error: ac_web_open not found in libacweb.so" << std::endl;
        return;
    }
    func(link);
}

void web_file_open(const char* file) {
    load_library();
    web_file_open_t func = (web_file_open_t)dlsym(lib_handle, "ac_web_file_open");
    if (!func) {
        std::cerr << "Error: ac_web_file_open not found in libacweb.so" << std::endl;
        return;
    }
    func(file);
}

void web_popen(const char* raw_link) {
    load_library();
    web_popen_t func = (web_popen_t)dlsym(lib_handle, "ac_web_popen");
    if (!func) {
        std::cerr << "Error: ac_web_popen not found in libacweb.so" << std::endl;
        return;
    }
    func(raw_link);
}

void web_ropen(const char* identifier) {
    load_library();
    web_ropen_t func = (web_ropen_t)dlsym(lib_handle, "ac_web_ropen");
    if (!func) {
        std::cerr << "Error: ac_web_ropen not found in libacweb.so" << std::endl;
        return;
    }
    func(identifier);
}

void web_browser() {
    load_library();
    web_browser_t func = (web_browser_t)dlsym(lib_handle, "ac_web_browser");
    if (!func) {
        std::cerr << "Error: ac_web_browser not found in libacweb.so" << std::endl;
        return;
    }
    func();
}

int web_pdf(const char* pdf) {
    load_library();
    web_pdf_t func = (web_pdf_t)dlsym(lib_handle, "ac_web_pdf");
    if (!func) {
        std::cerr << "Error: ac_web_pdf not found in libacweb.so" << std::endl;
        return 0;
    }
    return func(pdf);
}

int web_text(const char* text) {
    load_library();
    web_text_t func = (web_text_t)dlsym(lib_handle, "ac_web_text");
    if (!func) {
        std::cerr << "Error: ac_web_text not found in libacweb.so" << std::endl;
        return 0;
    }
    return func(text);
}

int web_inspect(const char* program) {
    load_library();
    web_inspect_t func = (web_inspect_t)dlsym(lib_handle, "ac_web_inspect");
    if (!func) {
        std::cerr << "Error: ac_web_inspect not found in libacweb.so" << std::endl;
        return 0;
    }
    return func(program);
}

void web_ac_page() {
    load_library();
    web_ac_page_t func = (web_ac_page_t)dlsym(lib_handle, "ac_web_ac_page");
    if (!func) {
        std::cerr << "Error: ac_web_ac_page not found in libacweb.so" << std::endl;
        return;
    }
    func();
}

const char* web_help() {
    load_library();
    web_help_t func = (web_help_t)dlsym(lib_handle, "ac_web_help");
    if (!func) {
        std::cerr << "Error: ac_web_help not found in libacweb.so" << std::endl;
        return "";
    }
    return func();
}

}
