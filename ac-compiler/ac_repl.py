#!/usr/bin/env python3
"""
AC Language REPL - Interactive Shell
Similar to Python's IDLE, allows users to tinker with AC code interactively
"""

import subprocess
import tempfile
import os
import readline
import atexit
import glob

# History file for command history
HISTORY_FILE = os.path.expanduser("~/.ac_history")


def load_history():
    """Load command history from file"""
    if os.path.exists(HISTORY_FILE):
        readline.read_history_file(HISTORY_FILE)


def save_history():
    """Save command history to file"""
    readline.write_history_file(HISTORY_FILE)


def clean_temp_files():
    """Remove temporary compilation files"""
    patterns = ["*.acc", "*.lir", "*.acb", "*.pyc", "__pycache__"]
    for pattern in patterns:
        for f in glob.glob(pattern):
            try:
                os.remove(f)
            except OSError:
                pass
    # Clean __pycache__ directories
    for root, dirs, files in os.walk("."):
        if "__pycache__" in dirs:
            import shutil
            shutil.rmtree(os.path.join(root, "__pycache__"))


def run_ac_code(code, target="PY"):
    """Compile and run AC code, then clean up"""
    # Create temp file for AC source
    with tempfile.NamedTemporaryFile(mode='w', suffix='.ac', delete=False) as f:
        f.write(f"AC->{target}\n{code}\n")
        temp_ac = f.name
    
    try:
        # Compile with AC compiler
        result = subprocess.run(
            ["./ac", temp_ac, f"--target {target}"],
            capture_output=True,
            text=True,
            cwd="/home/abu/Documents/kiro projects/AC/ac-compiler"
        )
        
        if result.returncode != 0:
            print(f"Compilation error:\n{result.stderr}")
            return False
        
        # Determine output file based on target
        if target == "PY":
            output_file = temp_ac.replace('.ac', '.py')
        elif target == "JS":
            output_file = temp_ac.replace('.ac', '.js')
        elif target == "BNY":
            output_file = temp_ac.replace('.ac', '.acb')
        else:
            output_file = temp_ac.replace('.ac', f'.{target.lower()}')
        
        # Run the generated code
        if target == "PY":
            result = subprocess.run(
                ["python3", output_file],
                capture_output=True,
                text=True
            )
        elif target == "JS":
            result = subprocess.run(
                ["node", output_file],
                capture_output=True,
                text=True
            )
        elif target == "BNY":
            result = subprocess.run(
                [output_file],
                capture_output=True,
                text=True
            )
        else:
            print(f"Running {target} backend not implemented")
            return False
        
        # Print output
        if result.stdout:
            print(result.stdout)
        if result.stderr:
            print(f"Runtime error:\n{result.stderr}")
        
        return result.returncode == 0
        
    except FileNotFoundError:
        print("Error: AC compiler not found. Make sure 'ac' is in the current directory.")
        return False
    except Exception as e:
        print(f"Error: {e}")
        return False
    finally:
        # Clean up temp files
        try:
            os.unlink(temp_ac)
        except OSError:
            pass
        clean_temp_files()


def main():
    """Main REPL loop"""
    # Load command history
    load_history()
    
    # Register history save on exit
    atexit.register(save_history)
    
    print("=" * 50)
    print("AC Language REPL - Interactive Shell")
    print("=" * 50)
    print("Type 'exit' or 'quit' to leave")
    print("Type 'clear' to clear the screen")
    print("Type 'help' for usage information")
    print()
    
    # Multi-line input buffer
    buffer = []
    
    while True:
        try:
            # Prompt based on whether we're in multi-line mode
            prompt = "ac> " if len(buffer) == 0 else "  ... "
            
            # Get input
            line = input(prompt)
            
            # Strip whitespace
            line = line.strip()
            
            # Handle special commands
            if line.lower() in ("exit", "quit"):
                print("Goodbye!")
                break
            
            if line.lower() == "clear":
                os.system("clear" if os.name == "posix" else "cls")
                continue
            
            if line.lower() == "help":
                print("\nAC Language REPL - Help")
                print("=" * 50)
                print("Commands:")
                print("  exit/quit    - Exit the REPL")
                print("  clear        - Clear the screen")
                print("  help         - Show this help message")
                print()
                print("Usage:")
                print("  Type AC code and press Enter to execute")
                print("  Multi-line input is supported")
                print("  Press Ctrl+C to cancel current input")
                print()
                print("Examples:")
                print("  ac> Term.display $Hello World$")
                print("  ac> x = 5")
                print("  ac> Term.display x @ 2")
                print()
                continue
            
            # Empty line
            if not line:
                continue
            
            # Add to buffer
            buffer.append(line)
            
            # Try to execute if buffer has content
            if buffer:
                code = "\n".join(buffer)
                
                # Simple check: if buffer ends with a complete statement, execute
                # For now, execute on every Enter (single-line mode)
                # Multi-line support can be added later with better parsing
                
                print()
                success = run_ac_code(code)
                print()
                
                # Clear buffer after execution
                buffer = []
                
        except EOFError:
            print("\nGoodbye!")
            break
        except KeyboardInterrupt:
            print("\nCancelled.")
            buffer = []  # Clear buffer on Ctrl+C
        except Exception as e:
            print(f"Error: {e}")
            buffer = []


if __name__ == "__main__":
    main()
