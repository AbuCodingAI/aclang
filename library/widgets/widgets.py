"""
AC Widgets Library - Tkinter GUI Abstraction
Clean AC-style API for GUI development with ttk (modern) widgets
"""

import tkinter as tk
from tkinter import ttk

class Screen:
    def __init__(self, title=None, geometry=None):
        self.root = tk.Tk()
        if title:
            self.root.title(title)
        if geometry:
            self.root.geometry(geometry)
    
    def mainloop(self):
        self.root.mainloop()
    
    def update(self):
        self.root.update()
    
    def destroy(self):
        self.root.destroy()

class display:
    def __init__(self, master=None, text=None):
        self.label = ttk.Label(master.root if isinstance(master, Screen) else master, text=text or "")
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.label.pack(**kwargs)
    
    def config(self, **kwargs):
        self.label.config(**kwargs)
    
    def set(self, value):
        self.label.config(text=value)

class ask:
    def __init__(self, master=None, width=None):
        self.entry = ttk.Entry(master.root if isinstance(master, Screen) else master, width=width or 20)
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.entry.pack(**kwargs)
    
    def get(self):
        return self.entry.get()
    
    def set(self, value):
        self.entry.delete(0, tk.END)
        self.entry.insert(0, value)

class btn:
    def __init__(self, master=None, text=None, cmd=None):
        self.button = ttk.Button(master.root if isinstance(master, Screen) else master, text=text or "", command=cmd)
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.button.pack(**kwargs)
    
    def config(self, **kwargs):
        self.button.config(**kwargs)

class ckbtn:
    def __init__(self, master=None, text=None):
        self.checkbox = ttk.Checkbutton(master.root if isinstance(master, Screen) else master, text=text or "")
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.checkbox.pack(**kwargs)
    
    def get(self):
        return self.checkbox.instate(['selected'])
    
    def set(self, value):
        if value:
            self.checkbox.state(['selected'])
        else:
            self.checkbox.state(['!selected'])

class radbtn:
    def __init__(self, master=None, text=None, variable=None, value=None):
        self.radio = ttk.Radiobutton(master.root if isinstance(master, Screen) else master, text=text or "", variable=variable, value=value)
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.radio.pack(**kwargs)

class dropdown:
    def __init__(self, master=None, values=None):
        self.combo = ttk.Combobox(master.root if isinstance(master, Screen) else master, values=values or [])
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.combo.pack(**kwargs)
    
    def get(self):
        return self.combo.get()
    
    def set(self, value):
        self.combo.set(value)

class advance:
    def __init__(self, master=None, length=None, mode=None):
        self.progress = ttk.Progressbar(master.root if isinstance(master, Screen) else master, length=length or 200, mode=mode or "determinate")
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.progress.pack(**kwargs)
    
    def set(self, value):
        self.progress['value'] = value
    
    def get(self):
        return self.progress['value']

class table:
    def __init__(self, master=None, columns=None, height=None):
        self.tree = ttk.Treeview(master.root if isinstance(master, Screen) else master, columns=columns or [], height=height or 10)
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.tree.pack(**kwargs)
    
    def add(self, values):
        self.tree.insert('', 'end', values=values)
    
    def get(self):
        return [self.tree.item(item)['values'] for item in self.tree.get_children()]

class tabs:
    def __init__(self, master=None):
        self.notebook = ttk.Notebook(master.root if isinstance(master, Screen) else master)
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.notebook.pack(**kwargs)
    
    def add_tab(self, name):
        frame = ttk.Frame(self.notebook)
        self.notebook.add(frame, text=name)
        return frame

class group:
    def __init__(self, master=None, text=None):
        self.frame = ttk.Frame(master.root if isinstance(master, Screen) else master)
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.frame.pack(**kwargs)

class scroller:
    def __init__(self, master=None, orient=None):
        self.scrollbar = ttk.Scrollbar(master.root if isinstance(master, Screen) else master, orient=orient or "vertical")
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.scrollbar.pack(**kwargs)

class slider:
    def __init__(self, master=None, from_val=None, to_val=None, orient=None):
        self.scale = ttk.Scale(
            master.root if isinstance(master, Screen) else master,
            from_=from_val or 0,
            to=to_val or 100,
            orient=orient or "horizontal"
        )
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.scale.pack(**kwargs)
    
    def get(self):
        return self.scale.get()
    
    def set(self, value):
        self.scale.set(value)

class listbox:
    def __init__(self, master=None, width=None, height=None):
        self.listbox = tk.Listbox(
            master.root if isinstance(master, Screen) else master,
            width=width or 30,
            height=height or 5
        )
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.listbox.pack(**kwargs)
    
    def add(self, item):
        self.listbox.insert(tk.END, item)
    
    def get(self):
        return list(self.listbox.get(0, tk.END))

class sketch:
    def __init__(self, master=None, width=None, height=None):
        self.canvas = tk.Canvas(
            master.root if isinstance(master, Screen) else master,
            width=width or 400,
            height=height or 300
        )
    
    def pack(self, space_x=None, space_y=None):
        kwargs = {}
        if space_x is not None:
            kwargs['padx'] = space_x
        if space_y is not None:
            kwargs['pady'] = space_y
        self.canvas.pack(**kwargs)
    
    def config(self, **kwargs):
        self.canvas.config(**kwargs)
    
    def line(self, x1, y1, x2, y2, **kwargs):
        self.canvas.create_line(x1, y1, x2, y2, **kwargs)
    
    def rect(self, x1, y1, x2, y2, **kwargs):
        self.canvas.create_rectangle(x1, y1, x2, y2, **kwargs)
    
    def circle(self, x, y, radius, **kwargs):
        self.canvas.create_oval(x - radius, y - radius, x + radius, y + radius, **kwargs)
    
    def text(self, x, y, text, **kwargs):
        self.canvas.create_text(x, y, text=text, **kwargs)
    
    def clear(self):
        self.canvas.delete("all")

