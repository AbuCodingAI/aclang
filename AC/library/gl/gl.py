# AC ilib: gl — GUI / Graphics library (Python backend)
# Wraps tkinter (stdlib) with a simple API. Falls back to stub if unavailable.
try:
    import tkinter as _tk
    _HAS_TK = True
except ImportError:
    _HAS_TK = False

_BLACK  = "#000000"
_WHITE  = "#ffffff"
_RED    = "#ff0000"
_GREEN  = "#00c800"
_BLUE   = "#0000ff"
_YELLOW = "#ffff00"

class Window:
    def __init__(self):
        self._root = None
        self._canvas = None
        self.open = False

    def create(self, width=800, height=600, title="AC Window"):
        if not _HAS_TK:
            print(f"[gl] tkinter not available — stub mode. Window: {title}")
            self.open = True
            return self
        self._root = _tk.Tk()
        self._root.title(title)
        self._canvas = _tk.Canvas(self._root, width=width, height=height, bg="black")
        self._canvas.pack()
        self.open = True
        return self

    def clear(self, color="#000000"):
        if self._canvas: self._canvas.config(bg=color); self._canvas.delete("all")

    def draw_rect(self, x, y, w, h, color="#ffffff"):
        if self._canvas: self._canvas.create_rectangle(x, y, x+w, y+h, fill=color, outline=color)
        else: print(f"[gl::rect] ({x},{y}) {w}x{h} {color}")

    def draw_line(self, x1, y1, x2, y2, color="#ffffff"):
        if self._canvas: self._canvas.create_line(x1, y1, x2, y2, fill=color)
        else: print(f"[gl::line] ({x1},{y1})->({x2},{y2}) {color}")

    def draw_text(self, x, y, text, color="#ffffff"):
        if self._canvas: self._canvas.create_text(x, y, text=text, fill=color)
        else: print(f"[gl::text] ({x},{y}) {text}")

    def present(self):
        if self._root: self._root.update()

    def poll_events(self):
        if not self.open: return False
        if self._root:
            try: self._root.update_idletasks(); self._root.update()
            except _tk.TclError: self.open = False; return False
        return self.open

    def destroy(self):
        if self._root: self._root.destroy()
        self.open = False

_default_window = Window()

def gl_open(w=800, h=600, title="AC Window"): return _default_window.create(w, h, title)
def gl_clear(color="#000000"): _default_window.clear(color)
def gl_rect(x, y, w, h, color="#ffffff"): _default_window.draw_rect(x, y, w, h, color)
def gl_line(x1, y1, x2, y2, color="#ffffff"): _default_window.draw_line(x1, y1, x2, y2, color)
def gl_text(x, y, text, color="#ffffff"): _default_window.draw_text(x, y, text, color)
def gl_present(): _default_window.present()
def gl_running(): return _default_window.poll_events()
def gl_close(): _default_window.destroy()

def gl_message(msg, title="AC"):
    if _HAS_TK:
        import tkinter.messagebox as mb
        mb.showinfo(title, msg)
    else:
        print(f"[gl::message] {title}: {msg}")
