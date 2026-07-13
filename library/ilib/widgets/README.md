# AC Widgets Library

A Tkinter GUI abstraction layer for AC using modern ttk (themed) widgets. Clean, AC-style API for GUI development.

## Usage

```ac
AC->PY
use ilib widgets

<mainloop>
    root = Screen(title=$My App$, geometry=$400x300$)
    label = display(master=root, text=$Hello World$)
    label.pack
    button = btn(master=root, text=$Click Me$, cmd=null)
    button.pack
    /kill
<mainloop>
```

## Widget Classes

### Screen
Main application window.

```ac
root = Screen(title=$My App$, geometry=$400x300$)
root.mainloop
```

### Display Widgets

**display** - Styled label
```ac
label = display(master=root, text=$Hello$)
label.pack
```

**advance** - Progress bar
```ac
progress = advance(master=root, length=$200$, mode=$determinate$)
progress.pack
progress.set($50$)
```

### Input Widgets

**ask** - Styled text input
```ac
entry = ask(master=root, width=$30$)
entry.pack
value = entry.get
```

**dropdown** - Dropdown/combobox
```ac
combo = dropdown(master=root, values=$Python,JavaScript,Rust$)
combo.pack
```

### Selection Widgets

**btn** - Styled button
```ac
button = btn(master=root, text=$Click Me$, cmd=null)
button.pack
```

**ckbtn** - Styled checkbox
```ac
checkbox = ckbtn(master=root, text=$Accept$)
checkbox.pack
```

**radbtn** - Styled radio button
```ac
radio = radbtn(master=root, text=$Option 1$, variable=var, value=$1$)
radio.pack
```

### Container Widgets

**group** - Styled frame/container
```ac
frame = group(master=root, text=$Settings$)
frame.pack
```

**tabs** - Tabbed interface
```ac
notebook = tabs(master=root)
notebook.pack
tab1 = notebook.add_tab($Tab 1$)
```

**table** - Table/tree view
```ac
tree = table(master=root, columns=$Name,Age,City$, height=$10$)
tree.pack
tree.add($John,25,NYC$)
```

### Other Widgets

**slider** - Styled scale/slider
```ac
scale = slider(master=root, from_val=$0$, to_val=$100$, orient=$horizontal$)
scale.pack
value = scale.get
```

**scroller** - Scrollbar
```ac
scrollbar = scroller(master=root, orient=$vertical$)
scrollbar.pack
```

**listbox** - List selection widget
```ac
list_widget = listbox(master=root, width=$30$, height=$5$)
list_widget.pack
list_widget.add $Python$
list_widget.add $JavaScript$
items = list_widget.get
```

**sketch** - Drawing canvas
```ac
canvas = sketch(master=root, width=$400$, height=$300$)
canvas.pack
canvas.line $10$ $10$ $100$ $100$ fill=$blue$
canvas.rect $120$ $20$ $200$ $100$ fill=$red$
canvas.circle $250$ $60$ $30$ fill=$green$
canvas.text $320$ $60$ $AC Widgets$ fill=$black$
```

## Common Methods

All widgets support:

- `.pack` - Pack widget into parent
- `.config` - Configure widget properties
- `.get` - Get widget value (for input widgets)
- `.set` - Set widget value (for input widgets)
- `.destroy` - Destroy widget

## Implementation Notes

- Uses modern ttk (themed) widgets for better appearance
- Canvas and Listbox use classic tk widgets (no ttk equivalents)
- All widgets are Python classes that wrap Tkinter widgets
- Compiles to Python, can be compiled to binary with PyInstaller
- Automatic `root.mainloop()` injection when using widgets library
