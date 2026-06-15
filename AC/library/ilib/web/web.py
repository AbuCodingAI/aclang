import webbrowser as wb 
def web_open(link):
    wb.open("https://"+link)
def web_file_open(file):
    wb.open("file://"+file)
def web_popen(raw_link):
    wb.open(raw_link)
def web_ropen(identifier):
    wb.open("https://"+identifier+".com")
def web_browser():
    wb.open("https://www.google.com")
def web_pdf(pdf):
    if pdf.endswith(".pdf"):
        web_file_open(pdf)
    else:
        return "File not pdf"
def web_text(text):
    if not text.endswith(".txt") or not text.endswith(".md") or not text.endswith(".bashrc") or not text.endswith(".zshrc"):
        return "File not text"
    else:
        web_file_open(text)
def inspect(program):
    if text.endswith(".py") or text.endswith(".java") or text.endswith(".js") or text.endswith(".c") or text.endswith(".cpp") or text.endswith(".v") or text.endswith(".ac") or text.endswith(".s") or text.endswith(".go") or text.endswith(".rs") or text.endswith(".sh") or text.endswith(".html") or text.endswith("sql"):
        web_file_open(program)
    else:
        return "Unsupported Program"
def ac_page():
    web_open("aclang.vercel.app")

# add a web_page_get function that gets the contents of the page, maybe like the unstyled html
#def web_page_get(url):
#   page=retrieve(url)
#   store page in f"{url}.html"
def web_help():
    print("=== Web Functions ===")
    print("web.open(link) -Opens https://{link}")
    print("web.file_open(file) -Opens file://{file})")
    print("web.popen(raw_link) -Opens {raw_link}")
    print("web.ropen(search) -Opens https://{search}.com")
    print("web.browser -Opens https://www.google.com")
    print("web.pdf(pdf) -Opens file://{pdf}, must be .pdf")
    print("web.text(text) -Opens file://{text}, must be .txt, .md, .bashrc, or .zshrc")
    print("web.inspect(program) -Opens file://{program}, must be .py, .java, .js, .c, .cpp, .v, .ac, .sh, .go, .rs or .s")
    print("ac_page -Opens the Vercel hosted page for AC")
    print("web.page_get(link) -Gets the contents of the page in unstyled html")
    print("web.help -Opens this help guide")
