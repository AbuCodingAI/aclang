// AC Widgets Library for HTML/JavaScript
// Provides GUI components for web-based AC programs

class Screen {
    constructor(options = {}) {
        this.title = options.title || 'AC Application';
        this.geometry = options.geometry || '800x600';
        this.container = null;
        this.elements = [];
        
        // Parse geometry
        const [width, height] = this.geometry.split('x').map(s => parseInt(s));
        this.width = width;
        this.height = height;
        
        // Set page title
        document.title = this.title;
        
        // Create main container
        this.container = document.createElement('div');
        this.container.id = 'ac-screen';
        this.container.style.cssText = `
            width: ${this.width}px;
            height: ${this.height}px;
            margin: 20px auto;
            border: 2px solid #333;
            padding: 20px;
            background: #f5f5f5;
            font-family: Arial, sans-serif;
        `;
    }
    
    mainloop() {
        // Append container to body if not already there
        if (!document.body.contains(this.container)) {
            document.body.appendChild(this.container);
        }
    }
    
    add(widget) {
        this.elements.push(widget);
        if (widget.element) {
            this.container.appendChild(widget.element);
        }
    }
}

class Label {
    constructor(parent, options = {}) {
        this.parent = parent;
        this.text = options.text || '';
        
        this.element = document.createElement('div');
        this.element.className = 'ac-label';
        this.element.textContent = this.text;
        this.element.style.cssText = `
            margin: 10px 0;
            padding: 5px;
            font-size: 14px;
        `;
        
        if (parent && parent.container) {
            parent.container.appendChild(this.element);
        }
    }
    
    config(options = {}) {
        if (options.text !== undefined) {
            this.text = options.text;
            this.element.textContent = this.text;
        }
        if (options.font) {
            this.element.style.fontFamily = options.font;
        }
        if (options.fg) {
            this.element.style.color = options.fg;
        }
        if (options.bg) {
            this.element.style.backgroundColor = options.bg;
        }
    }
}

class Button {
    constructor(parent, options = {}) {
        this.parent = parent;
        this.text = options.text || 'Button';
        this.command = options.command || null;
        
        this.element = document.createElement('button');
        this.element.className = 'ac-button';
        this.element.textContent = this.text;
        this.element.style.cssText = `
            margin: 10px 5px;
            padding: 10px 20px;
            font-size: 14px;
            cursor: pointer;
            border: 1px solid #333;
            background: #fff;
            border-radius: 4px;
        `;
        
        if (this.command) {
            this.element.addEventListener('click', this.command);
        }
        
        if (parent && parent.container) {
            parent.container.appendChild(this.element);
        }
    }
    
    config(options = {}) {
        if (options.text !== undefined) {
            this.text = options.text;
            this.element.textContent = this.text;
        }
        if (options.command) {
            this.command = options.command;
            this.element.removeEventListener('click', this.command);
            this.element.addEventListener('click', this.command);
        }
        if (options.fg) {
            this.element.style.color = options.fg;
        }
        if (options.bg) {
            this.element.style.backgroundColor = options.bg;
        }
    }
}

class Entry {
    constructor(parent, options = {}) {
        this.parent = parent;
        this.value = '';
        
        this.element = document.createElement('input');
        this.element.type = 'text';
        this.element.className = 'ac-entry';
        this.element.style.cssText = `
            margin: 10px 0;
            padding: 8px;
            font-size: 14px;
            border: 1px solid #ccc;
            border-radius: 4px;
            width: 200px;
        `;
        
        this.element.addEventListener('input', (e) => {
            this.value = e.target.value;
        });
        
        if (parent && parent.container) {
            parent.container.appendChild(this.element);
        }
    }
    
    get() {
        return this.element.value;
    }
    
    insert(index, text) {
        const current = this.element.value;
        this.element.value = current.slice(0, index) + text + current.slice(index);
        this.value = this.element.value;
    }
    
    delete(start, end) {
        const current = this.element.value;
        this.element.value = current.slice(0, start) + current.slice(end);
        this.value = this.element.value;
    }
    
    config(options = {}) {
        if (options.width) {
            this.element.style.width = options.width + 'px';
        }
        if (options.fg) {
            this.element.style.color = options.fg;
        }
        if (options.bg) {
            this.element.style.backgroundColor = options.bg;
        }
    }
}

class Text {
    constructor(parent, options = {}) {
        this.parent = parent;
        
        this.element = document.createElement('textarea');
        this.element.className = 'ac-text';
        this.element.style.cssText = `
            margin: 10px 0;
            padding: 8px;
            font-size: 14px;
            border: 1px solid #ccc;
            border-radius: 4px;
            width: 300px;
            height: 150px;
            resize: vertical;
        `;
        
        if (parent && parent.container) {
            parent.container.appendChild(this.element);
        }
    }
    
    insert(index, text) {
        const current = this.element.value;
        this.element.value = current.slice(0, index) + text + current.slice(index);
    }
    
    get(start, end) {
        return this.element.value.slice(start, end);
    }
    
    delete(start, end) {
        const current = this.element.value;
        this.element.value = current.slice(0, start) + current.slice(end);
    }
    
    config(options = {}) {
        if (options.width) {
            this.element.style.width = options.width + 'px';
        }
        if (options.height) {
            this.element.style.height = options.height + 'px';
        }
        if (options.fg) {
            this.element.style.color = options.fg;
        }
        if (options.bg) {
            this.element.style.backgroundColor = options.bg;
        }
    }
}

// Export for use in generated code
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { Screen, Label, Button, Entry, Text };
}
