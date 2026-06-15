import webbrowser as _wb

def web_open(link):
    """Open a URL with https:// prefix"""
    _wb.open("https://" + str(link))

def web_file_open(file):
    """Open a local file"""
    _wb.open("file://" + str(file))

def web_popen(raw_link):
    """Open a raw URL without modifications"""
    _wb.open(str(raw_link))

def web_ropen(identifier):
    """Open identifier.com"""
    _wb.open("https://" + str(identifier) + ".com")

def web_browser():
    """Open default browser to Google"""
    _wb.open("https://www.google.com")

def web_pdf(pdf):
    """Open a PDF file"""
    pdf_str = str(pdf)
    if pdf_str.endswith(".pdf"):
        web_file_open(pdf_str)
        return True
    else:
        return False

def web_text(text):
    """Open a text file (.txt, .md, .bashrc, .zshrc)"""
    text_str = str(text)
    valid_exts = [".txt", ".md", ".bashrc", ".zshrc"]
    if any(text_str.endswith(ext) for ext in valid_exts):
        web_file_open(text_str)
        return True
    else:
        return False

def web_inspect(program):
    """Open a source code file for inspection"""
    prog_str = str(program)
    valid_exts = [".py", ".java", ".js", ".c", ".cpp", ".v", ".ac", ".s", ".go", ".rs", ".sh", ".html", ".sql"]
    if any(prog_str.endswith(ext) for ext in valid_exts):
        web_file_open(prog_str)
        return True
    else:
        return False

def web_ac_page():
    """Open AC language website"""
    _wb.open("https://aclang.vercel.app")

def web_page_get(url):
    """Get page contents (stub - returns None, needs urllib)"""
    try:
        import urllib.request
        response = urllib.request.urlopen("https://" + str(url))
        return response.read().decode('utf-8')
    except:
        return None

def web_help():
    """Display help for all web functions"""
    help_text = """=== Web Library Functions ===
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
web.help()               - Shows this help message"""
    print(help_text)
