# AC Language: `fn` Keyword Semantics

## The Problem with `*`

In most languages, `*` is used for both:
- Multiplication: `a * b`
- Comments: `/* comment */`

This creates ambiguity and parsing complexity.

## AC's Solution: `fn` Keyword

AC makes a brilliant design choice:

### Default Behavior
- **`*` is ALWAYS for comments**: `* This is a comment *`
- **`@` is the multiplication operator**: `result = 5 @ 3`
- **`&` and `&&` are NOT allowed** (reserved/error)

### With `fn` Prefix
- **`fn` enables `*` as multiplication**: `result = fn 5 * 3`
- **`fn` enables `&` for method chaining**: `fn obj.method() & another()`
- **`fn` enables `&&` for chaining**: `fn obj.method() && another()`
- **`fn` allows `$` in strings**: `fn x="I have $5"` (otherwise `$` is string delimiter)

## Examples

### Correct Usage
```ac
* This is a comment *

a = 5 @ 3              * Regular multiplication *
b = fn 5 * 3           * fn enables * for multiply *
c = fn 2 * (3 + 4)     * fn enables * in expressions *

result @= 2            * Multiply-assign with @ *

* Method chaining requires fn *
fn obj.method() & another() && third()

* Strings with $ require fn *
price = $5 dollars$    * Normal string with $ delimiters *
msg = fn x="I have $5" * fn allows $ inside quotes *
```

### Incorrect Usage (Should Error)
```ac
a = 5 * 3              * ERROR: * is comment syntax, not multiply *
result *= 2            * ERROR: * is comment syntax *
obj.method() & another()  * ERROR: & not allowed without fn *
x = "I have $5"        * ERROR: $ is string delimiter, can't be inside quotes *
```

## Current Implementation Issues

### Issue 1: Lexer Behavior
The lexer currently tries to be "smart" about `*`:
```cpp
// Comments: *...* — only when * appears as standalone token
if (src[pos] == '*') {
    // peek ahead: if there's a closing * before a newline, it's a comment
    size_t closePos = src.find('*', pos + 1);
    if (closePos != std::string::npos && ...) {
        // treat as comment
    }
    // otherwise it's a multiply star
    tokens.emplace_back(TokenType::MULTIPLY, "*", line, col);
}
```

**This is wrong!** The lexer shouldn't guess. It should:
1. Always tokenize `*` as COMMENT_DELIMITER
2. Let the parser decide based on `fn` context

### Issue 2: Parser Behavior
The parser currently:
1. Sees `fn` keyword
2. Marks the expression with `__fn__` prefix
3. Passes it to codegen

**This is incomplete!** The parser should:
1. See `fn` keyword
2. Enter "fn mode" for the following expression
3. In "fn mode", treat `*` tokens as MULTIPLY
4. In "fn mode", allow `&` and `&&` tokens
5. Exit "fn mode" after the expression

### Issue 3: Codegen Behavior
Codegens currently:
```cpp
// Remove 'fn' keywords
for (size_t p = 0; (p = r.find("fn", p)) != std::string::npos;) {
    if (p + 2 < r.size() && r[p + 2] == ' ') {
        r.erase(p, 3);  // Just remove it
    }
}
```

**This is wrong!** By the time codegen runs, `fn` should already be processed by the parser.

## Correct Implementation Strategy

### Phase 1: Lexer (Current - Mostly Correct)
```cpp
if (src[pos] == '*') {
    // Check if it's a comment: *...*
    size_t closePos = src.find('*', pos + 1);
    if (closePos != std::string::npos && ...) {
        // Skip comment
        continue;
    }
    // Otherwise, emit MULTIPLY token (parser will validate)
    tokens.emplace_back(TokenType::MULTIPLY, "*", line, col);
}
```

### Phase 2: Parser (Needs Fix)
```cpp
if (token.type == TokenType::KW_FN) {
    advance();  // consume 'fn'
    inFnMode = true;
    auto expr = parseExpression();
    inFnMode = false;
    return expr;
}

// In parseExpression():
if (token.type == TokenType::MULTIPLY) {
    if (!inFnMode) {
        throw SYNTAX_ERROR("'*' can only be used with 'fn' prefix. Use '@' for multiplication.");
    }
    // Process as multiply
}

if (token.type == TokenType::AMPERSAND) {
    if (!inFnMode) {
        throw SYNTAX_ERROR("'&' can only be used with 'fn' prefix for method chaining.");
    }
    // Process as chain
}
```

### Phase 3: Codegen (Needs Fix)
```cpp
// By the time we reach codegen, fn is already processed
// Just translate the operators:
case NodeType::BinaryExpr:
    if (node.op == "*") {
        emit(left + " * " + right);  // Already validated by parser
    } else if (node.op == "@") {
        emit(left + " * " + right);  // @ becomes * in target language
    }
```

## Why This Design is Brilliant

1. **No ambiguity**: `*` is always comments unless explicitly enabled
2. **Clear intent**: `fn` signals "this is math/chaining, not comments"
3. **Backward compatible**: Existing code with `@` still works
4. **Prevents errors**: Can't accidentally use `*` for math
5. **Enables chaining**: `&` and `&&` are reserved for `fn` context

## Migration Path

For existing code using `*` incorrectly:
```ac
* Old (broken) *
result = 5 * 3

* New (correct) *
result = fn 5 * 3
* OR *
result = 5 @ 3
```

## Summary

- `*` = comments (always)
- `@` = multiplication (default)
- `fn *` = multiplication (explicit)
- `fn &` = method chaining (explicit)
- `fn &&` = method chaining (explicit)

This makes AC's syntax unambiguous and self-documenting!
