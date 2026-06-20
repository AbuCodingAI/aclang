# AC Language Server Protocol (LSP)

Real-time code intelligence for AC in VS Code, Vim, Neovim, Emacs, and all LSP-compatible editors.

---

## Overview

The AC Language Server provides:
- ✅ Real-time diagnostics (syntax/type errors)
- ✅ Autocompletion (variables, functions, keywords)
- ✅ Go to definition (jump to function/variable)
- ✅ Hover information (type hints, docs)
- ✅ Symbol search (find functions, variables)
- ✅ Code formatting (indent, align)
- ✅ Refactoring (rename variable, extract function)

---

## Installation

```bash
# Install the LSP server
npm install -g ac-lsp

# VS Code extension (auto-installs LSP)
code --install-extension AbuCodingAI.ac-lsp
```

---

## Features

### 1. **Real-time Diagnostics**

As you type:
```ac
x = 5
y = $hello$
z = x + y  /* Error: int + str not allowed */
```

**Error appears immediately in editor** (red squiggle)

### 2. **Autocompletion**

```ac
Math./* <-- triggers completion */
```

Shows:
```
Math.sqrt()
Math.sin()
Math.cos()
Math.abs()
...
```

### 3. **Go to Definition**

```ac
Make foo func()
    RETURN 42
Make

x = foo()  /* Ctrl+Click → jumps to foo definition */
```

### 4. **Hover Information**

```ac
x = Math.sqrt(16)  /* Hover over sqrt → shows signature and docs */
```

Shows:
```
Math.sqrt(x: dec) -> dec
Computes square root of x exactly
```

### 5. **Symbol Search**

```
Ctrl+Shift+O → shows all functions in file
Ctrl+T → shows all symbols in workspace
```

### 6. **Code Formatting**

```bash
ac-lsp format file.ac
```

Normalizes:
- Indentation (4 spaces per level)
- Spacing (around operators)
- Line length (wrap at 100)

### 7. **Rename Refactoring**

```ac
Make fibonacci func(n)  /* Right-click → Rename → "fib" */
    ...
Make

x = fibonacci(10)  /* Auto-updated to fib(10) */
```

---

## Architecture

### **Server Components**

```
acls (AC Language Server)
├── Lexer          (ac-compiler/src/lexer.cpp shared)
├── Parser         (ac-compiler/src/parser.cpp shared)
├── Type Checker   (new: ac-lsp/src/type_checker.cpp)
├── Symbol Table   (new: ac-lsp/src/symbols.cpp)
├── Diagnostics    (new: ac-lsp/src/diagnostics.cpp)
└── LSP Handler    (new: ac-lsp/src/lsp_server.cpp)
```

### **Client Integration**

```
VS Code
  ↓
Language Extension (JavaScript/TypeScript)
  ↓
TCP/stdio
  ↓
AC Language Server (C++)
  ↓
Shared AC Compiler Components (lexer, parser, type system)
```

### **Protocol Flow**

```
1. Client sends: initialize request
2. Server responds: capabilities (what it can do)

3. Client opens file
   Server: parses + type-checks

4. Client modifies file
   Server: incremental reparse + diagnostics

5. Client requests: completion at cursor
   Server: evaluates context + returns suggestions

6. Client requests: definition
   Server: looks up symbol location

7. Client closes file
   Server: discards from memory
```

---

## Implementation Plan

### **Phase 1: Basic LSP Setup**
- ✅ Implement LSP protocol (initialize, shutdown, document sync)
- ✅ Wire up shared lexer/parser from ac-compiler
- ✅ Implement document diagnostics (syntax errors)

### **Phase 2: Type Checking**
- ✅ Build symbol table from AST
- ✅ Implement type checker
- ✅ Report type errors in real-time

### **Phase 3: Intelligence Features**
- ✅ Autocompletion (variables, functions, keywords)
- ✅ Go to definition (jump to symbol)
- ✅ Hover information (type hints + docs)

### **Phase 4: Advanced Features**
- ✅ Symbol search (Ctrl+T)
- ✅ Rename refactoring
- ✅ Code formatting

### **Phase 5: Editor Extensions**
- ✅ VS Code extension
- ✅ Vim/Neovim plugin (via coc.nvim)
- ✅ Emacs LSP client
- ✅ Sublime Text LSP client

---

## File Structure

```
AC/ac-lsp/
  src/
    main.cpp              /* Entry point */
    lsp_server.cpp        /* LSP protocol handler */
    type_checker.cpp      /* Type checking */
    symbols.cpp           /* Symbol table + lookup */
    diagnostics.cpp       /* Error reporting */
    completions.cpp       /* Autocompletion */
    hover.cpp             /* Hover information */
    definition.cpp        /* Go to definition */
  include/
    lsp_server.hpp
    type_checker.hpp
    symbols.hpp
  test/
    test_completions.cpp
    test_diagnostics.cpp
  Makefile

AC/ac-vscode/              /* VS Code extension */
  src/
    extension.ts
    client.ts
  package.json
  tsconfig.json
```

---

## Build & Installation

```bash
# Build LSP server
cd AC/ac-lsp
make                      # Outputs: acls binary

# Test LSP
acls --test               # Runs unit tests

# Install VS Code extension
code --install-extension ac-vscode-0.1.0.vsix
```

---

## Usage Examples

### **VS Code**

1. Install extension from marketplace
2. Open AC file
3. Get real-time errors + completions

### **Vim/Neovim** (via coc.nvim)

```vim
:CocInstall coc-ac
```

Then use: `<leader>gd` (go to definition), `<leader>rn` (rename), etc.

### **Emacs** (via lsp-mode)

```elisp
(require 'lsp-mode)
(add-to-list 'lsp-language-id-configuration '(ac-mode . "ac"))
(lsp-register-client
  (make-lsp-client :new-connection (lsp-stdio-connection "acls")))
(add-hook 'ac-mode-hook #'lsp)
```

---

## Protocol Operations

### **Supported Operations**

| Operation | Capability |
|-----------|-----------|
| `initialize` | ✅ Setup server |
| `textDocument/didOpen` | ✅ File opened |
| `textDocument/didChange` | ✅ File modified |
| `textDocument/didClose` | ✅ File closed |
| `textDocument/diagnostics` | ✅ Report errors |
| `textDocument/completion` | ✅ Autocompletion |
| `textDocument/hover` | ✅ Hover info |
| `textDocument/definition` | ✅ Go to definition |
| `textDocument/references` | ✅ Find all uses |
| `textDocument/documentSymbol` | ✅ Symbol list |
| `textDocument/rename` | ✅ Rename variable |
| `textDocument/formatting` | ✅ Format code |

---

## Example: Completion at Cursor

**User types:**
```ac
Math.s|  /* cursor here */
```

**Server receives:**
```json
{
  "method": "textDocument/completion",
  "params": {
    "textDocument": {"uri": "file:///home/user/script.ac"},
    "position": {"line": 2, "character": 7}
  }
}
```

**Server responds:**
```json
{
  "isIncomplete": false,
  "items": [
    {
      "label": "sqrt",
      "kind": 6,
      "detail": "(x: dec) -> dec",
      "documentation": "Square root of x"
    },
    {
      "label": "sin",
      "kind": 6,
      "detail": "(x: dec) -> dec",
      "documentation": "Sine of x (radians)"
    }
  ]
}
```

**VS Code shows dropdown:**
```
√ sqrt
  sin
  sinh
  ...
```

---

## Performance

**Goals:**
- Diagnostics: < 100ms (acceptable latency)
- Completions: < 50ms (snappy)
- Definition: instant (cached symbol table)

**Optimizations:**
- Incremental parsing (only reparse changed region)
- Symbol table caching (invalidate on edits)
- Lazy type checking (check on demand)
- Worker threads (don't block UI)

---

## Debugging LSP

```bash
# Run server in debug mode
acls --debug --log /tmp/acls.log

# Tail logs
tail -f /tmp/acls.log

# VS Code LSP inspector
Ctrl+Shift+P → "LSP: Show Output Channel"
```

---

## Summary

**AC Language Server brings IDE-class features to any LSP editor.**

- ✅ Real-time error checking
- ✅ Smart autocompletion
- ✅ Jump to definition
- ✅ Symbol search
- ✅ Refactoring
- ✅ Works everywhere (VS Code, Vim, Emacs, etc.)

**Status:** Ready to implement with shared compiler components.
