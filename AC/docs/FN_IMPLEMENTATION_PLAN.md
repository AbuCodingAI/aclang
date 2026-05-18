# Implementation Plan: Complete `fn` Keyword Support

## Current Status

### What Works
- ✅ `fn` keyword is recognized by lexer
- ✅ `*` is treated as comments by default
- ✅ `&` and `&&` tokens exist
- ✅ `@=` compound assignment works

### What's Broken
- ❌ `@` as multiplication operator (not parsed at all!)
- ❌ `fn *` for multiplication (removes `fn`, keeps `*`)
- ❌ `fn &` and `fn &&` for chaining (not implemented)
- ❌ `fn "string with $"` (lexer breaks on `$`)

## Implementation Tasks

### Task 1: Implement `@` as Multiplication Operator
**Priority: HIGH** (This is fundamental)

#### Lexer Changes
Currently `@` is tokenized as IDENTIFIER. Need to:
```cpp
// In lexer.cpp, single char tokens section:
case '@':
    if (pos < src.size() && src[pos] == '=') {
        pos++; col++;
        tokens.emplace_back(TokenType::AT_EQUAL, "@=", line, sc);
    } else {
        tokens.emplace_back(TokenType::AT, "@", line, sc);  // NEW!
    }
    break;
```

#### Token Changes
```cpp
// In token.hpp:
enum class TokenType {
    // ... existing tokens ...
    AT,             // @ (multiplication operator)
    AT_EQUAL,       // @= (already exists)
    // ...
};
```

#### Parser Changes
```cpp
// In parser.cpp, parseExpression():
// Add @ to binary operators with same precedence as *
if (at(TokenType::AT) || at(TokenType::MULTIPLY)) {
    std::string op = advance().value;
    // ... handle multiplication
}
```

#### Codegen Changes
All codegens need to translate `@` to `*`:
```cpp
// In translateExpr():
for (size_t p = 0; (p = r.find("@", p)) != std::string::npos;) {
    r.replace(p, 1, "*");
    p++;
}
```

### Task 2: Implement `fn *` for Multiplication
**Priority: HIGH**

#### Current Behavior
```ac
b = fn 5 * 3    # Parser marks as __fn__, codegen removes fn
```

#### Desired Behavior
```ac
b = fn 5 * 3    # Should work, * is multiplication
b = 5 * 3       # Should ERROR: * is comment syntax
```

#### Implementation
The parser needs to track `fn` context:

```cpp
// In parser.cpp:
class Parser {
    bool inFnContext = false;  // NEW!
    
    NodePtr parseExpression() {
        if (at(TokenType::KW_FN)) {
            advance();  // consume 'fn'
            inFnContext = true;
            auto expr = parseExpressionInner();
            inFnContext = false;
            return expr;
        }
        return parseExpressionInner();
    }
    
    NodePtr parseExpressionInner() {
        // ... existing expression parsing ...
        
        if (at(TokenType::MULTIPLY)) {
            if (!inFnContext) {
                throw SYNTAX_ERROR("'*' can only be used with 'fn' prefix. Use '@' for multiplication.");
            }
            // Process as multiplication
        }
    }
};
```

### Task 3: Implement `fn &` and `fn &&` for Chaining
**Priority: MEDIUM**

#### Current Behavior
```ac
obj.method() & another()    # Lexer tokenizes &, parser ignores it
```

#### Desired Behavior
```ac
fn obj.method() & another()     # OK: chaining with same args
fn obj.method() && another()    # OK: chaining with different args
obj.method() & another()        # ERROR: & requires fn
```

#### Implementation
Similar to `*` handling:

```cpp
// In parser.cpp:
if (at(TokenType::AMPERSAND) || at(TokenType::DOUBLE_AMPERSAND)) {
    if (!inFnContext) {
        throw SYNTAX_ERROR("'&' and '&&' can only be used with 'fn' prefix for method chaining.");
    }
    // Create MethodChain node
}
```

### Task 4: Implement `fn "string with $"`
**Priority: MEDIUM**

#### Current Behavior
```ac
msg = fn x="I have $5"    # Lexer breaks: $ starts new string
```

#### Desired Behavior
```ac
msg = fn x="I have $5"    # OK: $ is literal inside quotes
msg = x="I have $5"       # ERROR: $ is string delimiter
```

#### Implementation Options

**Option A: Lexer State Machine** (Complex but correct)
```cpp
class Lexer {
    bool inFnContext = false;
    
    std::vector<Token> tokenize() {
        // When we see 'fn' keyword, set inFnContext = true
        // When we see newline/semicolon, set inFnContext = false
        
        // In string parsing:
        if (src[pos] == '"') {
            std::string s;
            while (pos < src.size() && src[pos] != '"') {
                if (src[pos] == '
</content>
</file> && !inFnContext) {
                    break;  // $ ends string (normal mode)
                }
                s += src[pos++];  // $ is literal (fn mode)
            }
        }
    }
};
```

**Option B: Escape Sequence** (Simpler)
```ac
* Use \$ to escape dollar in strings *
msg = x="I have \$5"    # Works without fn
```

**Option C: Different String Syntax** (Cleanest)
```ac
* Use triple quotes for literal strings *
msg = """I have $5"""   # $ is literal
msg = $I have 5 dollars$  # $ delimiters, no $ inside
```

**Recommendation**: Option A (proper fn context) for consistency with other fn features.

### Task 5: Update Documentation
**Priority: LOW**

Update all docs to explain:
- `@` is default multiplication
- `fn *` enables * for multiplication
- `fn &` and `fn &&` enable chaining
- `fn "..."` allows $ in strings

## Testing Strategy

### Test Cases

```ac
* Test 1: @ multiplication *
a = 5 @ 3
assert a is 15

* Test 2: fn * multiplication *
b = fn 5 * 3
assert b is 15

* Test 3: * without fn should error *
c = 5 * 3  * Should throw error *

* Test 4: @= compound assignment *
x = 5
x @= 3
assert x is 15

* Test 5: fn & chaining *
fn obj.method() & another()

* Test 6: & without fn should error *
obj.method() & another()  * Should throw error *

* Test 7: fn string with $ *
msg = fn x="I have $5"
assert msg is "I have $5"

* Test 8: string with $ without fn should error *
msg = x="I have $5"  * Should throw error *
```

## Implementation Order

1. **Task 1** (@ operator) - Foundation for everything else
2. **Task 2** (fn * validation) - Core fn semantics
3. **Task 3** (fn & chaining) - Method chaining support
4. **Task 4** (fn $ in strings) - String literal support
5. **Task 5** (Documentation) - User-facing docs

## Estimated Effort

- Task 1: 2-3 hours (lexer, parser, all codegens)
- Task 2: 1-2 hours (parser validation)
- Task 3: 1-2 hours (parser + codegen)
- Task 4: 3-4 hours (lexer state machine)
- Task 5: 1 hour (documentation)

**Total: 8-12 hours of development**

## Breaking Changes

This will break existing code that uses:
- `*` for multiplication without `fn`
- `&` or `&&` without `fn`
- `$` inside `"..."` strings without `fn`

Migration path:
- Add `fn` prefix where needed
- Or use `@` for multiplication
- Or use `$...$` string delimiters instead of `"..."`
