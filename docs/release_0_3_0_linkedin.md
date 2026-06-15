# AC v0.3.0 — LinkedIn Release Post

---

🚀 **AC Language v0.3.0 is out.**

AC is a multi-target compiled language I've been building — write once, compile to Python, JavaScript, C, C++, Java, Rust, Go, V, Assembly, and native binary. Today's release is 3rd one now.

---

**What's new in 0.3.0:**

🏷️ **Tags are now first-class**
AC's structural tags (`<mainloop>`, `<free>`, `<bound>`) used to be parser hints. Now they're part of the IR — they appear in the compiled output, carry real semantics, and every backend handles them natively.

- `<free>` — variables declared inside are globally accessible across the program
- `<bound>` — creates a proper scoped block; in Python it compiles to `if True:`, in C/Rust/Go it becomes `{ }`

🔢 **Math library expanded**
`math.phi` (golden ratio), `math.pi(n)` and `math.e(n)` with arbitrary precision, and full dot-call syntax across every backend.

🎮 **GL library — real-time GUI and games**
`ilib gl` is an SDL2-backed graphics library that works from AC source. Create windows, draw objects, handle collisions, respond to keyboard input, and run a frame loop — all from AC syntax. Works on Linux and Windows. The HTML backend falls back to a Canvas renderer automatically.

```ac
AC->PY
use ilib gl

<mainloop>
    gl.init()
    gl.screen.create(800, 600, $My Game$)
    gl.obj.create($ball$)
    gl.obj.square($ball$, 20)
    gl.obj.pos($ball$, 400, 300)
    gl.obj.set_speed($ball$, 200.0)

    <StartHere>
        dt = gl.frame.delta()
        gl.frame.update(dt)
        IF gl.hitbox.boundary($ball$)
            gl.obj.speed_mult($ball$, -1.0)
        gl.frame.render()
        gl.frame.end()
    <EndHere>
<mainloop>
```

🪟 **Widgets library — desktop GUI for all backends**
`ilib widgets` brings desktop GUI components to every AC target. Backed by tkinter on Python, DOM on JavaScript/HTML, GTK3 on C/C++, Swing on Java, fyne on Go, gtk4 on Rust, and V's UI module — the same AC source compiles to a native GUI on each platform.

Widgets available: `Screen`, `display`, `ask`, `btn`, `ckbtn`, `radbtn`, `dropdown`, `advance`, `table`, `tabs`, `group`, `scroller`, `slider`, `listbox`, `sketch`.

```ac
AC->PY
use ilib widgets

<mainloop>
    root = Screen(title=$My App$, geometry=$400x300$)
    label = display(master=root, text=$Hello from AC$)
    label.pack
    button = btn(master=root, text=$Click Me$, cmd=null)
    button.pack
    root.mainloop
<mainloop>
```

📷 **Camera library**
`ilib camera` — cross-platform camera access with a clean AC interface.

🔊 **Machine audio library**
`ilib machine-audio` — low-level audio playback, ready to use.

📦 **datac keyword**
`.datac` files are AC's structured data format. The new `datac` keyword lets you load them directly:
```
use datac "students.datac" as students
```
This is also the foundation for jasql and AbuDB — more on those soon.

🛠️ **Under the hood**
- Hash-based IR cache now correctly handles all scoping edge cases
- AST cache version bumped; stale caches auto-invalidate
- Makefile tracks header dependencies properly — no more stale object files
- Compiler is ~15% faster on repeat builds due to cache improvements
- All compiler errors now consistently prefixed `Preposterous:` — AC's error identity

---

**What is AC?**

AC is a language designed around the idea that you shouldn't have to choose your target at the start. The same source file compiles to a Python script, a native binary, a JavaScript module, or a Java class — your choice at compile time.

The compiler is written in C++17, open source, and available on GitHub.

👉 github.com/AbuCodingAI/aclang

More on AC here:
👉 aclang.vercel.app

---

Still early days — more coming. If you're curious, drop a comment or message me.

#programming #compilerdevelopment #opensource #programminglanguages #buildinpublic
