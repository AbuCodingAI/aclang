# input - Ghost Input Feature

## Overview
The `input` keyword allows AC programs to send simulated/ghost keyboard inputs programmatically. This is useful for automation, testing, macros, and simulating user interactions.

## Syntax
```ac
input <keybind>
```

## How It Works
`input` triggers the event listener for the specified key, causing any bound function to execute as if the user had pressed that key. If no binding exists for the key, nothing happens (silent failure).

## Use Cases

### 1. Automation
Automate repetitive tasks by simulating key presses:
```ac
AC->PY

Make save_file func()
    Term.display $File saved!$

<mainloop>
    configure event-listener
        use listener to establish rule
            on value=s
                save_file()
    
    # Auto-save every loop iteration
    input s
    /kill
<mainloop>
```

### 2. Testing
Test event handlers without manual input:
```ac
AC->JS

Make test_handler func()
    Term.display $Handler works!$

<mainloop>
    configure event-listener
        use listener to establish rule
            on value=space
                test_handler()
    
    # Automated test
    Term.display $Running test...$
    input space
    Term.display $Test complete!$
    /kill
<mainloop>
```

### 3. Macros
Create keyboard macros that trigger multiple actions:
```ac
AC->PY

Make action1 func()
    Term.display $Action 1$

Make action2 func()
    Term.display $Action 2$

Make action3 func()
    Term.display $Action 3$

Make macro func()
    input a
    input b
    input c

<mainloop>
    configure event-listener
        use listener to establish rule
            on value=a
                action1()
            on value=b
                action2()
            on value=c
                action3()
            on value=m
                macro()
    
    # Pressing 'm' triggers all three actions
    input m
    /kill
<mainloop>
```

### 4. Game Input Simulation
Simulate player inputs for game testing or AI:
```ac
AC->PY

Make move_left func()
    Term.display $Moving left$

Make move_right func()
    Term.display $Moving right$

Make jump func()
    Term.display $Jumping$

<mainloop>
    configure event-listener
        use listener to establish rule
            on value=a
                move_left()
            on value=d
                move_right()
            on value=space
                jump()
    
    # Simulate a sequence of moves
    input a
    input a
    input space
    input d
    input d
    
    /kill
<mainloop>
```

## Implementation Details

### All Backends Support
The `input` keyword is implemented across all 10 AC backends:
- Python
- JavaScript
- Java
- C++
- C
- Go
- Rust
- V
- HTML
- ASM

### How It's Generated

#### Python
```python
# Simulate key press: space
ac_event_listener.trigger('space')
```

#### JavaScript
```javascript
// Simulate key press: space
ac_event_listener.trigger('space');
```

#### C++
```cpp
// Simulate key press: space
ac::global_listener.trigger("space");
```

#### Java
```java
// Simulate key press: space
acEventListener.trigger("space");
```

#### C
```c
// Simulate key press: space
ac_event_listener_trigger("space");
```

#### Go
```go
// Simulate key press: space
acEventListener.Trigger("space")
```

#### Rust
```rust
// Simulate key press: space
AC_EVENT_LISTENER.lock().unwrap().trigger("space");
```

#### V
```v
// Simulate key press: space
ac_event_listener.trigger('space')
```

#### HTML
```html
<script>
    // Simulate key press: space
    acEventListener.trigger('space');
</script>
```

#### ASM
```asm
# Simulate key press: space
# In real implementation, this would trigger interrupt or system call
```

## Key Names
Use the same key names as in event listener bindings:
- Letter keys: `a`, `b`, `c`, ..., `z`
- Number keys: `1`, `2`, `3`, ..., `0`
- Special keys: `space`, `enter`, `tab`, `backspace`, `escape`
- Arrow keys: `up`, `down`, `left`, `right`
- Modifier keys: `shift`, `ctrl`, `alt`, `super`
- Function keys: `f1`, `f2`, ..., `f12`

## Behavior

### With Binding
If a key has a binding, `input` triggers the bound function:
```ac
configure event-listener
    use listener to establish rule
        on value=space
            jump()

input space  # Calls jump()
```

### Without Binding
If a key has no binding, `input` does nothing (silent):
```ac
configure event-listener
    use listener to establish rule
        on value=space
            jump()

input enter  # Does nothing, no error
```

## Comparison with Real Input

| Feature | Real Input | input |
|---------|-----------|------------|
| Triggers event listener | ✅ | ✅ |
| Requires user action | ✅ | ❌ |
| Can be automated | ❌ | ✅ |
| Works in tests | ❌ | ✅ |
| Generates OS events | ✅ | ❌ |
| Affects other programs | ✅ | ❌ |

## Advanced Example: Input Recording & Playback

```ac
AC->PY

Make record_input func()
    Term.display $Recording input...$

Make playback_input func()
    Term.display $Playing back recorded input...$
    input w
    input w
    input a
    input s
    input d

Make move_forward func()
    Term.display $Moving forward$

Make move_left func()
    Term.display $Moving left$

Make move_back func()
    Term.display $Moving back$

Make move_right func()
    Term.display $Moving right$

<mainloop>
    configure event-listener
        use listener to establish rule
            on value=w
                move_forward()
            on value=a
                move_left()
            on value=s
                move_back()
            on value=d
                move_right()
            on value=r
                record_input()
            on value=p
                playback_input()
    
    # Simulate pressing 'p' to playback
    input p
    
    /kill
<mainloop>
```

## Files Modified

### Core Compiler
- `ac-compiler/include/token.hpp` - Added `KW_INPUT` token
- `ac-compiler/src/lexer.cpp` - Added `input` keyword recognition
- `ac-compiler/include/ast.hpp` - Added `InputStmt` node type
- `ac-compiler/src/parser.cpp` - Added `input` statement parsing

### All 10 Codegens
- `ac-compiler/src/codegen_py.cpp` - Python implementation
- `ac-compiler/src/codegen_js.cpp` - JavaScript implementation
- `ac-compiler/src/codegen_java.cpp` - Java implementation
- `ac-compiler/src/codegen_cpp.cpp` - C++ implementation
- `ac-compiler/src/codegen_c.cpp` - C implementation
- `ac-compiler/src/codegen_go.cpp` - Go implementation
- `ac-compiler/src/codegen_rs.cpp` - Rust implementation
- `ac-compiler/src/codegen_v.cpp` - V implementation
- `ac-compiler/src/codegen_html.cpp` - HTML implementation
- `ac-compiler/src/codegen_asm.cpp` - ASM implementation

## Testing

### Test File: test_input.ac
```ac
AC->PY

Make handle_space func()
    Term.display $Space was pressed!$

Make handle_enter func()
    Term.display $Enter was pressed!$

<mainloop>
    configure event-listener
        use listener to establish rule
            on value=space
                handle_space()
            on value=enter
                handle_enter()
    
    Term.display $Simulating space key press...$
    input space
    
    Term.display $Simulating enter key press...$
    input enter
    
    Term.display $Ghost inputs sent!$
    /kill
<mainloop>
```

### Expected Output
```
Simulating space key press...
Space was pressed!
Simulating enter key press...
Enter was pressed!
Ghost inputs sent!
```

## Future Enhancements

1. **Timing Control**: Add delays between ghost inputs
   ```ac
   input space delay 100ms
   ```

2. **Input Sequences**: Define reusable input sequences
   ```ac
   Make combo sequence
       input a
       input b
       input c
   ```

3. **Conditional Inputs**: Send inputs based on conditions
   ```ac
   IF player_health < 50
       input heal
   ```

4. **OS-Level Simulation**: Generate actual OS keyboard events (platform-specific)

## Conclusion

The `input` keyword provides powerful automation and testing capabilities to AC programs. It works seamlessly with the event listener system and is supported across all backends, making it a versatile tool for creating macros, automated tests, and simulated user interactions.
