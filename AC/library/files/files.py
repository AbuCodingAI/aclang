# AC ilib: files — File I/O library (Python backend)
import os

def files_read(path):
    try:
        with open(path, 'r') as f: return f.read()
    except OSError: return ""

def files_read_lines(path):
    try:
        with open(path, 'r') as f: return f.readlines()
    except OSError: return []

def files_write(path, content):
    try:
        with open(path, 'w') as f: f.write(content)
        return True
    except OSError: return False

def files_append(path, content):
    try:
        with open(path, 'a') as f: f.write(content)
        return True
    except OSError: return False

def files_exists(path): return os.path.isfile(path)
def files_remove(path):
    try: os.remove(path); return True
    except OSError: return False
