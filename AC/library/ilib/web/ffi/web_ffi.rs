use std::process::Command;

pub struct WebLib;

impl WebLib {
    pub fn open(link: &str) {
        Self::open_url(&format!("https://{}", link));
    }

    pub fn file_open(file: &str) {
        Self::open_url(&format!("file://{}", file));
    }

    pub fn popen(raw_link: &str) {
        Self::open_url(raw_link);
    }

    pub fn ropen(identifier: &str) {
        Self::open_url(&format!("https://{}.com", identifier));
    }

    pub fn browser() {
        Self::open_url("https://www.google.com");
    }

    pub fn pdf(pdf: &str) -> bool {
        if pdf.ends_with(".pdf") {
            Self::file_open(pdf);
            true
        } else {
            false
        }
    }

    pub fn text(text: &str) -> bool {
        let valid_exts = [".txt", ".md", ".bashrc", ".zshrc"];
        if valid_exts.iter().any(|ext| text.ends_with(ext)) {
            Self::file_open(text);
            true
        } else {
            false
        }
    }

    pub fn inspect(program: &str) -> bool {
        let valid_exts = [".py", ".java", ".js", ".c", ".cpp", ".v", ".ac", ".s", ".go", ".rs", ".sh", ".html", ".sql"];
        if valid_exts.iter().any(|ext| program.ends_with(ext)) {
            Self::file_open(program);
            true
        } else {
            false
        }
    }

    pub fn ac_page() {
        Self::open_url("https://aclang.vercel.app");
    }

    pub fn page_get(_url: &str) -> Option<String> {
        // Stub - would need reqwest crate for real implementation
        None
    }

    pub fn help() {
        let help_text = r#"=== Web Library Functions ===
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
web.help()               - Shows this help message"#;
        println!("{}", help_text);
    }

    fn open_url(url: &str) {
        #[cfg(target_os = "windows")]
        {
            let _ = Command::new("cmd")
                .args(&["/C", "start", url])
                .output();
        }
        #[cfg(target_os = "macos")]
        {
            let _ = Command::new("open").arg(url).output();
        }
        #[cfg(target_os = "linux")]
        {
            let _ = Command::new("xdg-open").arg(url).output();
        }
    }
}

pub use WebLib as web;
