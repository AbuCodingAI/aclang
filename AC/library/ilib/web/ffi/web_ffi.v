import os
import net.http

pub struct Web {}

pub fn (w Web) open(link string) {
    open_url('https://${link}')
}

pub fn (w Web) file_open(file string) {
    open_url('file://${file}')
}

pub fn (w Web) popen(raw_link string) {
    open_url(raw_link)
}

pub fn (w Web) ropen(identifier string) {
    open_url('https://${identifier}.com')
}

pub fn (w Web) browser() {
    open_url('https://www.google.com')
}

pub fn (w Web) pdf(pdf string) bool {
    if pdf.ends_with('.pdf') {
        w.file_open(pdf)
        return true
    }
    return false
}

pub fn (w Web) text(text string) bool {
    valid_exts := ['.txt', '.md', '.bashrc', '.zshrc']
    for ext in valid_exts {
        if text.ends_with(ext) {
            w.file_open(text)
            return true
        }
    }
    return false
}

pub fn (w Web) inspect(program string) bool {
    valid_exts := ['.py', '.java', '.js', '.c', '.cpp', '.v', '.ac', '.s', '.go', '.rs', '.sh', '.html', '.sql']
    for ext in valid_exts {
        if program.ends_with(ext) {
            w.file_open(program)
            return true
        }
    }
    return false
}

pub fn (w Web) ac_page() {
    open_url('https://aclang.vercel.app')
}

pub fn (w Web) page_get(url string) ?string {
    response := http.get('https://${url}') or { return none }
    return response.body
}

pub fn (w Web) help() {
    help_text := '=== Web Library Functions ===
web.open(link)           - Opens https://{link}
web.file_open(file)      - Opens file://{file}
web.popen(raw_link)      - Opens {raw_link} as-is
web.ropen(search)        - Opens https://{search}.com
web.browser()            - Opens https://www.google.com
web.pdf(pdf)             - Opens file://{pdf}, must be .pdf
web.text(text)           - Opens file://{text}, must be .txt, .md, .bashrc, or .zshrc
web.inspect(program)     - Opens file://{program}, must be source code
web.ac_page()            - Opens the AC language website
web.page_get(link)       - Gets HTML contents of a webpage
web.help()               - Shows this help message'
    println(help_text)
}

fn open_url(url string) {
    $if windows {
        os.system('start ${url}')
    } $else if macos {
        os.system('open ${url}')
    } $else {
        os.system('xdg-open ${url}')
    }
}
