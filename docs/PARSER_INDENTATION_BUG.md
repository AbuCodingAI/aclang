# CRITICAL: Parser Indentation Bug

## The Problem

The parser accepts **completely invalid syntax** with no indentation because `skipAll()` throws away INDENT/DEDENT tokens.

### Invalid Code That Compiles (SHOULD FAIL):
```ac
AC->RS
<mainloop>
<LOGIC>
x = true
y = false
display x
display y
/kill
<LOGIC>
<mainloop>
```

**Problems:**
1. ❌ No indentation inside `<mainloop>`
2. ❌ `<LOGIC>` tag is outdated/wrong
3. ❌ `display` should be `Term.display()` or method call
4. ❌ Closing tags wrong (`<LOGIC>` instead of `</LOGIC>`)

### Valid Code (What SHOULD be required):
```ac
AC->PY

Make draw_game func(canvas, ball_x, ball_y)
    canvas.clear
    canvas.circle(x=ball_x, y=ball_y, radius=5, fill=$yellow$)

<mainloop>
    ball_x = 400
    ball_y = 300
    
    WHILST game_running is true
        ball_x += 5
        ball_y += 5
        
        IF ball_x > 800
            ball_x = 0
        
        Term.display(ball_x)
    
    /kill
<mainloop>
```

**Correct:**
1. ✅ Proper indentation (4 spaces)
2. ✅ Function bodies indented
3. ✅ WHILST/IF blocks indented
4. ✅ Method calls with proper syntax

## Root Cause

### The Culprit: `skipAll()`

```cpp
void skipAll() {
    while (at(TokenType::NEWLINE) || at(TokenType::INDENT) || at(TokenType::DEDENT))
        advance();
}
```

This function is called in:
1. `parse()` - Top level parsing
2. `parseStatement()` - Every statement
3. `parseTagBlock()` - Tag blocks

**Result:** All indentation information is discarded!

### Why This Exists

The function was probably added to handle:
- Extra newlines between statements
- Flexible whitespace at top level

But it's too aggressive - it throws away ALL indentation, even where it's required.

## The Fix

### Strategy 1: Replace `skipAll()` with `skipNewlines()`

```cpp
// BEFORE (wrong):
void skipAll() {
    while (at(TokenType::NEWLINE) || at(TokenType::INDENT) || at(TokenType::DEDENT))
        advance();
}

// AFTER (correct):
void skipNewlines() {
    while (at(TokenType::NEWLINE))
        advance();
}
```

Then update all call sites:
- Top level: Use `skipNewlines()` (flexible)
- Inside blocks: **Require** INDENT/DEDENT

### Strategy 2: Context-Aware Skipping

```cpp
void skipWhitespace(bool allowIndent = false) {
    if (allowIndent) {
        // Top level - skip everything
        while (at(TokenType::NEWLINE) || at(TokenType::INDENT) || at(TokenType::DEDENT))
            advance();
    } else {
        // Inside blocks - only skip newlines
        while (at(TokenType::NEWLINE))
            advance();
    }
}
```

### Strategy 3: Explicit Indentation Checking

```cpp
void expectIndent() {
    skipNewlines();
    if (!at(TokenType::INDENT)) {
        error("Expected indentation");
    }
    advance();
}

void expectDedent() {
    skipNewlines();
    if (!at(TokenType::DEDENT)) {
        error("Expected dedent");
    }
    advance();
}
```

## Recommended Fix (Strategy 1 + 3)

### Step 1: Add proper skip functions

```cpp
// Skip only newlines (safe everywhere)
void skipNewlines() {
    while (at(TokenType::NEWLINE))
        advance();
}

// Skip newlines and indentation (only at top level)
void skipAll() {
    while (at(TokenType::NEWLINE) || at(TokenType::INDENT) || at(TokenType::DEDENT))
        advance();
}

// Require indentation
void expectIndent() {
    skipNewlines();
    if (!at(TokenType::INDENT)) {
        error("Expected indentation at line " + std::to_string(current().line));
    }
    advance();
}

// Require dedent
void expectDedent() {
    skipNewlines();
    if (!at(TokenType::DEDENT)) {
        error("Expected dedent at line " + std::to_string(current().line));
    }
    advance();
}
```

### Step 2: Update parseBlock()

```cpp
NodePtr parseBlock() {
    auto block = std::make_unique<ASTNode>(NodeType::Block);
    skipNewlines();
    
    // REQUIRE indentation for blocks
    expectIndent();
    
    while (!at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
        auto stmt = parseStatement();
        if (stmt) block->children.push_back(std::move(stmt));
        skipNewlines();  // Only skip newlines, not indents
    }
    
    // REQUIRE dedent at end
    expectDedent();
    
    return block;
}
```

### Step 3: Update parseStatement()

```cpp
NodePtr parseStatement() {
    skipNewlines();  // Changed from skipAll()
    
    if (at(TokenType::END_OF_FILE)) return nullptr;
    
    // ... rest of parsing
}
```

### Step 4: Update parseFuncDef()

```cpp
NodePtr parseFuncDef() {
    // ... parse function signature ...
    
    skipNewlines();
    
    // REQUIRE indentation for function body
    expectIndent();
    
    auto body = std::make_unique<ASTNode>(NodeType::Block);
    while (!at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
        auto stmt = parseStatement();
        if (stmt) body->children.push_back(std::move(stmt));
        skipNewlines();
    }
    
    expectDedent();
    
    func->children.push_back(std::move(body));
    return func;
}
```

### Step 5: Update parseIfStmt(), parseWhilstLoop(), etc.

```cpp
NodePtr parseIfStmt() {
    // ... parse condition ...
    
    skipNewlines();
    expectIndent();  // REQUIRE indentation
    
    auto body = std::make_unique<ASTNode>(NodeType::Block);
    while (!at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
        auto stmt = parseStatement();
        if (stmt) body->children.push_back(std::move(stmt));
        skipNewlines();
    }
    
    expectDedent();  // REQUIRE dedent
    
    node->children.push_back(std::move(body));
    return node;
}
```

## Testing

### Test 1: Invalid indentation should FAIL

```ac
AC->PY
<mainloop>
x = 5
y = 10
/kill
<mainloop>
```

**Expected:** Error: "Expected indentation at line 3"

### Test 2: Valid indentation should PASS

```ac
AC->PY

<mainloop>
    x = 5
    y = 10
    /kill
<mainloop>
```

**Expected:** Compiles successfully

### Test 3: Function without indentation should FAIL

```ac
AC->PY

Make add func(a, b)
return a + b

<mainloop>
    result = add(5, 3)
    /kill
<mainloop>
```

**Expected:** Error: "Expected indentation at line 4"

### Test 4: Function with indentation should PASS

```ac
AC->PY

Make add func(a, b)
    return a + b

<mainloop>
    result = add(5, 3)
    /kill
<mainloop>
```

**Expected:** Compiles successfully

## Impact

### Before Fix:
- ❌ Any garbage syntax compiles
- ❌ No indentation enforcement
- ❌ Confusing error messages
- ❌ Hard to debug

### After Fix:
- ✅ Only valid syntax compiles
- ✅ Clear indentation requirements
- ✅ Better error messages
- ✅ Easier to debug

## Priority

**CRITICAL** - This is a fundamental parser bug that:
1. Accepts invalid programs
2. Makes debugging impossible
3. Breaks language consistency
4. Confuses users

## Estimated Time

- Add helper functions: 30 minutes
- Update parseBlock(): 15 minutes
- Update parseFuncDef(): 15 minutes
- Update parseIfStmt(), parseWhilstLoop(), etc.: 1 hour
- Testing: 1 hour
- **Total: ~3 hours**

## Next Steps

1. Add `expectIndent()` and `expectDedent()` functions
2. Replace `skipAll()` with `skipNewlines()` in most places
3. Add indentation requirements to all block-based constructs
4. Test with valid and invalid examples
5. Update error messages to be helpful
