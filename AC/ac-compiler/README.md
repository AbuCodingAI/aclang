# AC Language Compiler

AC is a beginner-friendly compiled language with clean, readable syntax.
This compiler translates `.ac` files into various target languages.

---

## Usage

```bash
ac mycode.ac
```

The compiler reads the `AC->TARGET` line inside your file and compiles to that target automatically.
No flags needed.

You can also override the target manually:

```bash
ac mycode.ac --backend PY
```

---

## Backends

| Declaration in file | Target         | Status        |
|---------------------|----------------|---------------|
| `AC->PY`            | Python         | Implemented   |
| `AC->JS`            | JavaScript     | Implemented   |
| `AC->HTML`          | HTML           | Implemented   |
| `AC->CSS`           | CSS            | Planned       |
| `AC->ASM`           | Assembly       | In Progress   |
| `AC->Java`          | Java           | Implemented   |
| `AC->GO`            | Go             | Planned       |
| `AC->RS`            | Rust           | Planned       |
| `AC->V`             | V              | Planned       |
| `AC->C`             | C              | In Progress   |
| `AC->C++`           | C++            | In Progress   |
| `AC->BNY`           | Machine Code   | Planned       |

---

## Syntax Reference

### Comments
```
* this is a comment *
```

### Strings
```
$this is a string$
"this is also a string"
```

### Backend Declaration
Must be at the top of your file. Tells the compiler what to compile to.
```
AC->PY
```

### Variables
```
myvar = $hello$
myvar = 42
```

### Objects
```
Obj.Player
Player.config item=square(50)
Player.Sprite = AC.Search($PlayerSprite.png$)
```

### Properties
```
Player.speed = 5
Player.region = left
Player.interactive = true
```

### Method Calls
```
Player.jump()
sidebar.display($Hello$)
sidebar.ask($Command?$)
```

### Config
```
Name.config key=value
Background.config mode=livefeed
Background.config color=green
```

### Conditionals
```
IF x #= y
     * body indented *

OTHER
     * else body *
```

### Operators
| Operator | Meaning       |
|----------|---------------|
| `=`      | assign / equals (in IF) |
| `#=`     | not equal     |
| `fn arg*argfn` | multiply |
| `cmd&cmd arg` | run both commands on arg |

### Functions
```
Make jump func(arg)
     IF arg not of Obj type
          raise ERR($That is not a valid object$)
     OTHER
          arg.pixel.up(180)
```

### Tags (Blocks)
Tags open and close with the same syntax. First instance opens, second closes.
```
<mainloop>
     * your program *
<mainloop>
```

Built-in tags:

| Tag          | Purpose                        |
|--------------|--------------------------------|
| `<mainloop>` | Main program entry point       |
| `<gui>`      | GUI block                      |
| `<OBJECT>`   | Object definitions             |
| `<SCREEN>`   | Screen/display setup           |
| `<LOGIC>`    | Logic / code block             |
| `<Local>`    | Local scope                    |
| `<StartHere>` / `<EndHere>` | Loop region (reruns from StartHere) |

### Custom Tags
```
def tag <terrain>
     terrain.generation()
          * terrain logic *
```

Use a custom tag:
```
<terrain>
     SpawnTerrain
<terrain>
```

### Loop Region
```
<StartHere>
     * this block repeats *
<EndHere>
* code here runs once after loop ends *
```

### Save / Use
```
save as pic.png
use integratedWebCam
use pic.png as reference
```

### Kill
```
/kill
```
Ends the entire program.

### Foreign Blocks
Write raw code in the target language. The compiler passes it through verbatim.
Only emitted if the backend matches the target language.

```
<Foreign>
const nums = [1, 2, 3];
const sum = nums.reduce((a, b) => a + b, 0);
console.log(sum);
<Foreign>
```

**Important**: You are responsible for writing correct code for the target backend.
If you write Python code but compile to JavaScript, your code will break.

**Indentation**: Code in Foreign blocks can be indented in your `.ac` file for readability.
The compiler automatically strips the base indentation level.

Example with indentation:
```
<Foreign>
     import tkinter as tk
     root = tk.Tk()
     root.geometry("400x300")
     print("Hello world")
<Foreign>
```

The leading spaces are stripped, and the code is re-indented at the current scope level.

### Raise Error
```
raise ERR($That is not a valid object$)
```
Displays: `Preposterous: That is not a valid object`

### Event Listener
```
configure event-listener
     use listener to establish rule
          on value=space
               jump(Character)
```

### Wildcards in IF
`%` acts as a wildcard/placeholder:
```
IF %Sprite.png not found
     use %.config definition for %Sprite.png found
```

---

## Building the Compiler

```bash
sudo apt install -y g++
make
```

---

## File Extension

AC source files use the `.ac` extension.
