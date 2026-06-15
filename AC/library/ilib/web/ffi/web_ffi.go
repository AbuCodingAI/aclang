package main

import (
	"fmt"
	"os/exec"
	"runtime"
	"strings"
)

var web = struct {
	open      func(string)
	fileOpen  func(string)
	popen     func(string)
	ropen     func(string)
	browser   func()
	pdf       func(string) bool
	text      func(string) bool
	inspect   func(string) bool
	acPage    func()
	pageGet   func(string) string
	help      func()
}{
	open: func(link string) {
		openURL("https://" + link)
	},
	fileOpen: func(file string) {
		openURL("file://" + file)
	},
	popen: func(rawLink string) {
		openURL(rawLink)
	},
	ropen: func(identifier string) {
		openURL("https://" + identifier + ".com")
	},
	browser: func() {
		openURL("https://www.google.com")
	},
	pdf: func(pdf string) bool {
		if strings.HasSuffix(pdf, ".pdf") {
			web.fileOpen(pdf)
			return true
		}
		return false
	},
	text: func(text string) bool {
		validExts := []string{".txt", ".md", ".bashrc", ".zshrc"}
		for _, ext := range validExts {
			if strings.HasSuffix(text, ext) {
				web.fileOpen(text)
				return true
			}
		}
		return false
	},
	inspect: func(program string) bool {
		validExts := []string{".py", ".java", ".js", ".c", ".cpp", ".v", ".ac", ".s", ".go", ".rs", ".sh", ".html", ".sql"}
		for _, ext := range validExts {
			if strings.HasSuffix(program, ext) {
				web.fileOpen(program)
				return true
			}
		}
		return false
	},
	acPage: func() {
		openURL("https://aclang.vercel.app")
	},
	pageGet: func(url string) string {
		// Stub - would need net/http for real implementation
		return ""
	},
	help: func() {
		helpText := `=== Web Library Functions ===
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
web.help()               - Shows this help message`
		fmt.Println(helpText)
	},
}

func openURL(url string) {
	var cmd string
	var args []string

	switch runtime.GOOS {
	case "darwin":
		cmd = "open"
	case "windows":
		cmd = "cmd"
		args = []string{"/c", "start"}
	default:
		cmd = "xdg-open"
	}

	args = append(args, url)
	exec.Command(cmd, args...).Start()
}
