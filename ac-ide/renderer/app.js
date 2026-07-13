/* ═══════════════════════════════════════════════════════════════════════════
   AC IDE  — renderer process
   ═══════════════════════════════════════════════════════════════════════════ */

'use strict';

// ── state ─────────────────────────────────────────────────────────────────────
let editor       = null;
let options      = {};
let currentFile  = null;
let isDirty      = false;
let isRunning    = false;

const STARTER = `AC->BNY

/* Hello from AC IDE! */

<mainloop>
    Term.display $Hello, World!$
    /kill
<mainloop>
`;

// ── Monaco init ────────────────────────────────────────────────────────────────
require.config({ paths: { vs: 'https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.44.0/min/vs' } });

require(['vs/editor/editor.main'], () => {
  registerACLanguage();
  initEditor();
  boot();
});

function registerACLanguage() {
  monaco.languages.register({ id: 'ac' });

  monaco.languages.setMonarchTokensProvider('ac', {
    keywords: [
      'IF','ELSEIF','OTHER','WHILST','FOR','in','return','Make','func',
      'raise','use','ilib','elib','clib','is','not','try','catch','after',
      'report','bundle','pass','break','continue','destroy','range','sequence',
      'ERR','null','True','False','eval','save','as','from','tag','def',
      'temp','many','on','bind','to','xsub','overlap','skip',
    ],
    operators: ['+','-','*','@','/','>','<','=','#=','+=','-=','*=','/=','@=','|','|=','#'],

    tokenizer: {
      root: [
        // block comment
        [/\/\*/, 'comment', '@comment'],
        // line comment (AC doesn't have, but // is common typo)
        [/\/\/.*$/, 'comment'],
        // string $...$
        [/\$[^$]*\$/, 'string'],
        // string "..."
        [/"[^"]*"/, 'string'],
        // backend declaration AC->XXX
        [/AC->[A-Z+]+/, 'keyword.control'],
        // tags  <mainloop>  /kill
        [/\/kill/, 'keyword.control'],
        [/<\w+>/, 'tag'],
        // numbers
        [/\b\d+\.\d+\b/, 'number.float'],
        [/\b\d+\b/,       'number'],
        // keywords
        [/\b(IF|ELSEIF|OTHER|WHILST|FOR|in|return|Make|func|raise|use|ilib|elib|clib|is|not|try|catch|after|report|bundle|pass|break|continue|destroy|range|sequence|ERR|null|True|False|eval|save|as|from|tag|def|temp|many|on|bind|to|xsub|overlap|skip)\b/, 'keyword'],
        // Term.display
        [/Term\.(display|ask)/, 'support.function'],
        // math.xxx
        [/math\.\w+/, 'support.function'],
        // identifiers
        [/[a-zA-Z_]\w*/, 'identifier'],
        // operators
        [/[+\-*/@<>=!#|]/, 'operator'],
        // punctuation
        [/[(),\[\]{}]/, 'punctuation'],
      ],
      comment: [
        [/[^/*]+/, 'comment'],
        [/\*\//, 'comment', '@pop'],
        [/[/*]/, 'comment'],
      ],
    },
  });

  // Theme
  monaco.editor.defineTheme('ac-dark', {
    base: 'vs-dark',
    inherit: true,
    rules: [
      { token: 'keyword',          foreground: 'bb9af7', fontStyle: 'bold' },
      { token: 'keyword.control',  foreground: 'f7768e', fontStyle: 'bold' },
      { token: 'tag',              foreground: 'e0af68' },
      { token: 'string',           foreground: '9ece6a' },
      { token: 'number',           foreground: 'ff9e64' },
      { token: 'number.float',     foreground: 'ff9e64' },
      { token: 'comment',          foreground: '565f89', fontStyle: 'italic' },
      { token: 'operator',         foreground: '89ddff' },
      { token: 'support.function', foreground: '7dcfff' },
      { token: 'identifier',       foreground: 'c0caf5' },
      { token: 'punctuation',      foreground: '9aa5ce' },
    ],
    colors: {
      'editor.background':          '#1e1f2e',
      'editor.foreground':          '#c0caf5',
      'editorLineNumber.foreground':'#3b3d57',
      'editorLineNumber.activeForeground': '#737aa2',
      'editor.selectionBackground': '#283457',
      'editor.inactiveSelectionBackground': '#1e2333',
      'editorCursor.foreground':    '#c0caf5',
      'editor.lineHighlightBackground': '#21223a',
      'editorGutter.background':    '#1e1f2e',
      'scrollbarSlider.background': '#2a2d4a',
    },
  });

  monaco.editor.defineTheme('ac-light', {
    base: 'vs',
    inherit: true,
    rules: [
      { token: 'keyword',  foreground: '6953d8', fontStyle: 'bold' },
      { token: 'string',   foreground: '2a7d2a' },
      { token: 'comment',  foreground: '888888', fontStyle: 'italic' },
      { token: 'tag',      foreground: 'b07200' },
    ],
    colors: { 'editor.background': '#ffffff' },
  });

  monaco.languages.setLanguageConfiguration('ac', {
    comments: { blockComment: ['/*', '*/'] },
    brackets: [['(', ')'], ['[', ']']],
    autoClosingPairs: [
      { open: '(', close: ')' },
      { open: '[', close: ']' },
      { open: '$', close: '$' },
    ],
    indentationRules: {
      increaseIndentPattern: /^\s*(IF|ELSEIF|OTHER|WHILST|FOR|Make|try|catch|after|bundle)\b.*$/,
    },
  });

  // Basic completions
  monaco.languages.registerCompletionItemProvider('ac', {
    provideCompletionItems: (model, position) => {
      const word = model.getWordUntilPosition(position);
      const range = {
        startLineNumber: position.lineNumber, endLineNumber: position.lineNumber,
        startColumn: word.startColumn, endColumn: word.endColumn,
      };
      const suggestions = [
        { label: 'IF', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'IF ${1:condition}\n\t${2:body}', insertTextRules: 4, range },
        { label: 'WHILST', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'WHILST ${1:condition}\n\t${2:body}', insertTextRules: 4, range },
        { label: 'FOR', kind: monaco.languages.CompletionItemKind.Keyword, insertText: 'FOR ${1:item} in ${2:list}\n\t${3:body}', insertTextRules: 4, range },
        { label: 'Make', kind: monaco.languages.CompletionItemKind.Snippet, insertText: 'Make ${1:name} func(${2:args})\n\t${3:body}\n\treturn ${4:result}', insertTextRules: 4, range },
        { label: 'try', kind: monaco.languages.CompletionItemKind.Snippet, insertText: 'try\n\t${1:body}\ncatch\n\treport ${2:err}\n\t${3:handler}\nafter\n\t${4:cleanup}', insertTextRules: 4, range },
        { label: 'bundle', kind: monaco.languages.CompletionItemKind.Snippet, insertText: 'bundle ${1:Name}\n\t${2:body}', insertTextRules: 4, range },
        { label: 'Term.display', kind: monaco.languages.CompletionItemKind.Function, insertText: 'Term.display ${1:value}', insertTextRules: 4, range },
        { label: 'math.pi',      kind: monaco.languages.CompletionItemKind.Constant, insertText: 'math.pi', range },
        { label: 'math.e',       kind: monaco.languages.CompletionItemKind.Constant, insertText: 'math.e', range },
        { label: 'math.sqrt',    kind: monaco.languages.CompletionItemKind.Function,  insertText: 'math.sqrt(${1:x})', insertTextRules: 4, range },
        { label: 'math.sin',     kind: monaco.languages.CompletionItemKind.Function,  insertText: 'math.sin(${1:x})', insertTextRules: 4, range },
        { label: 'math.cos',     kind: monaco.languages.CompletionItemKind.Function,  insertText: 'math.cos(${1:x})', insertTextRules: 4, range },
        { label: 'math.ln',      kind: monaco.languages.CompletionItemKind.Function,  insertText: 'math.ln(${1:x})', insertTextRules: 4, range },
        { label: 'use ilib math', kind: monaco.languages.CompletionItemKind.Module, insertText: 'use ilib math\n', range },
        { label: '/kill', kind: monaco.languages.CompletionItemKind.Keyword, insertText: '/kill', range },
        { label: '<mainloop>', kind: monaco.languages.CompletionItemKind.Snippet,
          insertText: '<mainloop>\n\t${1:body}\n\t/kill\n<mainloop>', insertTextRules: 4, range },
      ];
      return { suggestions };
    },
  });
}

function initEditor() {
  editor = monaco.editor.create(document.getElementById('monaco-editor'), {
    value:          STARTER,
    language:       'ac',
    theme:          'ac-dark',
    fontSize:       14,
    fontFamily:     'JetBrains Mono, Fira Code, Cascadia Code, monospace',
    fontLigatures:  true,
    minimap:        { enabled: false },
    scrollBeyondLastLine: false,
    renderLineHighlight: 'all',
    smoothScrolling: true,
    cursorBlinking: 'smooth',
    cursorSmoothCaretAnimation: 'on',
    tabSize:        4,
    insertSpaces:   true,
    wordWrap:       'off',
    lineNumbers:    'on',
    glyphMargin:    false,
    folding:        true,
    automaticLayout: true,
    padding:        { top: 12 },
    scrollbar:      { verticalScrollbarSize: 6, horizontalScrollbarSize: 6 },
  });

  // Track dirty state
  editor.onDidChangeModelContent(() => {
    setDirty(true);
    if (options.autoSave && currentFile) autoSave();
  });

  // Keyboard shortcuts
  editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter, () => doRun());
  editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyB, () => doBuild());
  editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, () => doSave());
  editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyO, () => doOpen());
  editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.Comma, () => switchPanel('settings'));
}

async function boot() {
  options = await window.acide.getOptions();
  applyOptions();
  await loadExamples();
  setupUI();
  updateTitle();
}

// ── Options ────────────────────────────────────────────────────────────────────
function applyOptions() {
  // Theme
  if (options.theme === 'light') {
    document.body.className = 'theme-light';
    monaco.editor.setTheme('ac-light');
  } else if (options.theme === 'midnight') {
    document.body.className = 'theme-midnight';
    monaco.editor.setTheme('ac-dark');
  } else {
    document.body.className = '';
    monaco.editor.setTheme('ac-dark');
  }

  editor.updateOptions({
    fontSize:    options.fontSize    || 14,
    tabSize:     options.tabSize     || 4,
    wordWrap:    options.wordWrap    ? 'on' : 'off',
    minimap:     { enabled: !!options.showMinimap },
  });

  // Backend selector
  const bs = document.getElementById('backend-select');
  if (bs && options.defaultBackend) bs.value = options.defaultBackend;

  // Fill settings panel
  setOptUI();
}

function setOptUI() {
  setV('opt-theme',        options.theme         || 'dark');
  setV('opt-fontSize',     options.fontSize      || 14);
  setV('opt-tabSize',      options.tabSize       || 4);
  setV('opt-backend',      options.defaultBackend|| 'BNY');
  setCB('opt-wordWrap',    options.wordWrap);
  setCB('opt-minimap',     options.showMinimap);
  setCB('opt-autoSave',    options.autoSave !== false);
  setV('opt-compilerPath', options.compilerPath  || '');
}

function getOptFromUI() {
  return {
    theme:          getV('opt-theme'),
    fontSize:       parseInt(getV('opt-fontSize')) || 14,
    tabSize:        parseInt(getV('opt-tabSize'))  || 4,
    defaultBackend: getV('opt-backend'),
    wordWrap:       getCB('opt-wordWrap'),
    showMinimap:    getCB('opt-minimap'),
    autoSave:       getCB('opt-autoSave'),
    compilerPath:   getV('opt-compilerPath'),
  };
}

function getV(id)    { return document.getElementById(id)?.value  ?? ''; }
function setV(id,v)  { const el = document.getElementById(id); if (el) el.value = v; }
function getCB(id)   { return document.getElementById(id)?.checked ?? false; }
function setCB(id,v) { const el = document.getElementById(id); if (el) el.checked = !!v; }

// ── Examples ───────────────────────────────────────────────────────────────────
async function loadExamples() {
  const list = await window.acide.listExamples();
  const container = document.getElementById('example-list');
  container.innerHTML = '';
  for (const ex of list) {
    const el = document.createElement('div');
    el.className = 'example-item';
    el.innerHTML = `<span class="ex-icon">▲</span><span>${ex.name}</span>`;
    el.addEventListener('click', () => openExample(ex));
    container.appendChild(el);
  }
}

async function openExample(ex) {
  if (isDirty && !confirm('Discard unsaved changes?')) return;
  const content = await window.acide.readFile(ex.path);
  editor.setValue(content);
  currentFile = ex.path;
  setDirty(false);
  updateTitle(ex.name);

  // Highlight active
  document.querySelectorAll('.example-item').forEach(el => el.classList.remove('active'));
  event?.target?.closest('.example-item')?.classList.add('active');
}

// ── UI setup ───────────────────────────────────────────────────────────────────
function setupUI() {
  // Title bar controls
  document.getElementById('btn-min').onclick   = () => window.acide.minimize();
  document.getElementById('btn-max').onclick   = () => window.acide.maximize();
  document.getElementById('btn-close').onclick = () => window.acide.close();

  // Activity bar
  document.querySelectorAll('.act-btn').forEach(btn => {
    btn.addEventListener('click', () => switchPanel(btn.dataset.panel));
  });

  // Toolbar buttons
  document.getElementById('btn-open').onclick  = doOpen;
  document.getElementById('btn-save').onclick  = doSave;
  document.getElementById('btn-run').onclick   = doRun;
  document.getElementById('btn-build').onclick = doBuild;
  document.getElementById('btn-all').onclick   = doAll;
  document.getElementById('btn-stop').onclick  = doStop;
  document.getElementById('btn-new-file').onclick = doNewFile;

  // Output tabs
  document.querySelectorAll('.out-tab').forEach(tab => {
    tab.addEventListener('click', () => {
      document.querySelectorAll('.out-tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.out-pane').forEach(p => p.classList.remove('active'));
      tab.classList.add('active');
      document.getElementById('pane-' + tab.dataset.tab)?.classList.add('active');
    });
  });

  // Clear output
  document.getElementById('btn-clear-out').onclick = () => {
    document.getElementById('output-text').textContent = '';
    document.getElementById('generated-text').textContent = '';
    document.getElementById('ir-text').textContent = '';
    document.getElementById('log-text').textContent = '';
  };

  // Options
  document.getElementById('btn-save-opts').onclick = async () => {
    options = getOptFromUI();
    await window.acide.saveOptions(options);
    applyOptions();
    setStatus('Options saved', 'ok');
  };
  document.getElementById('btn-reset-opts').onclick = async () => {
    options = await window.acide.resetOptions();
    applyOptions();
  };

  // Shell
  setupShell();

  // Splitter drag
  setupSplitter();

  // Run output streaming
  window.acide.onRunOutput(chunk => appendOutput(chunk));

  // Keyboard shortcuts (outside editor)
  document.addEventListener('keydown', e => {
    if ((e.ctrlKey || e.metaKey) && e.key === 's') { e.preventDefault(); doSave(); }
    if ((e.ctrlKey || e.metaKey) && e.key === 'o') { e.preventDefault(); doOpen(); }
  });
}

function switchPanel(name) {
  document.querySelectorAll('.act-btn').forEach(b => b.classList.toggle('active', b.dataset.panel === name));
  document.querySelectorAll('.panel').forEach(p => {
    p.classList.toggle('active', p.id === 'panel-' + name);
  });
}

// ── Actions ────────────────────────────────────────────────────────────────────
async function doOpen() {
  const result = await window.acide.openFile();
  if (!result) return;
  if (isDirty && !confirm('Discard unsaved changes?')) return;
  editor.setValue(result.content);
  currentFile = result.path;
  setDirty(false);
  updateTitle(result.path.split('/').pop());
}

async function doSave() {
  const content = editor.getValue();
  if (currentFile) {
    await window.acide.writeFile(currentFile, content);
    setDirty(false);
    setStatus('Saved', 'ok');
  } else {
    const p = await window.acide.saveAs(content);
    if (p) { currentFile = p; setDirty(false); updateTitle(p.split('/').pop()); }
  }
}

let autoSaveTimer = null;
function autoSave() {
  clearTimeout(autoSaveTimer);
  autoSaveTimer = setTimeout(() => {
    if (currentFile) window.acide.writeFile(currentFile, editor.getValue());
  }, 1000);
}

function doNewFile() {
  if (isDirty && !confirm('Discard unsaved changes?')) return;
  editor.setValue(STARTER);
  currentFile = null;
  setDirty(false);
  updateTitle('untitled.ac');
}

async function doRun() {
  if (isRunning) return;
  const backend = document.getElementById('backend-select').value;
  clearOutputPane();
  setRunning(true);
  setStatus('Running…', 'running');

  const fileName = (currentFile ? currentFile.split('/').pop() : 'scratch.ac');
  appendLog(`▶ ac ${fileName} --target ${backend}\n`);

  const result = await window.acide.compileRun({
    source:  editor.getValue(),
    backend,
    fileName,
  });

  setRunning(false);
  if (result.success) {
    setStatus('Done', 'ok');
  } else {
    setStatus('Error', 'error');
    appendOutput('\x1b[31m' + result.output + '\x1b[0m');
  }
  appendLog(result.output);
}

async function doBuild() {
  if (isRunning) return;
  const backend = document.getElementById('backend-select').value;
  setRunning(true);
  setStatus('Building…', 'running');

  const fileName = (currentFile ? currentFile.split('/').pop() : 'scratch.ac');
  appendLog(`⚡ ac ${fileName} --target ${backend} (build only)\n`);

  const result = await window.acide.compileOnly({
    source:  editor.getValue(),
    backend,
    fileName,
  });

  setRunning(false);
  if (result.success) {
    setStatus('Built', 'ok');
    appendOutput('\x1b[32m' + result.output + '\x1b[0m');
    // Try to show generated code
    await loadGeneratedCode(fileName, backend);
  } else {
    setStatus('Build error', 'error');
    appendOutput('\x1b[31m' + result.output + '\x1b[0m');
  }
  appendLog(result.output);
}

async function doAll() {
  if (isRunning) return;
  setRunning(true);
  setStatus('Compiling all…', 'running');
  const fileName = (currentFile ? currentFile.split('/').pop() : 'scratch.ac');
  appendLog(`◈ ac ${fileName} --all\n`);

  const result = await window.acide.compileAll({ source: editor.getValue(), fileName });
  setRunning(false);
  setStatus(result.success ? 'All done' : 'Errors', result.success ? 'ok' : 'error');
  appendOutput((result.success ? '\x1b[32m' : '\x1b[31m') + result.output + '\x1b[0m');
  appendLog(result.output);
}

function doStop() {
  window.acide.stopRun();
  setRunning(false);
  setStatus('Stopped', 'idle');
  appendOutput('\n\x1b[33m[Stopped]\x1b[0m\n');
}

async function loadGeneratedCode(fileName, backend) {
  const ext = { PY:'.py',JS:'.js',C:'.c',CPP:'.cpp',Java:'.java',RS:'.rs',GO:'.go',V:'.v',ASM:'.s',BNY:'.acb',HTML:'.html' };
  const e = ext[backend];
  if (!e) return;
  const tmpPath = `/tmp/ac-ide/${fileName.replace('.ac', '')}${e}`;
  try {
    const content = await window.acide.readFile(tmpPath);
    document.getElementById('generated-text').textContent = content;
  } catch {}
}

// ── Shell ──────────────────────────────────────────────────────────────────────
function setupShell() {
  const input   = document.getElementById('shell-input');
  const history = document.getElementById('shell-history');
  const runBtn  = document.getElementById('shell-run-btn');

  const runCmd = async () => {
    const cmd = input.value.trim();
    if (!cmd) return;
    input.value = '';

    appendShell(`\x1b[90mac> \x1b[0m${cmd}\n`);

    // If starts with "ac ", compile the code
    if (cmd.startsWith('ac ') || cmd === 'ac') {
      appendShell('[Shell: use the Run button for interactive compilation]\n');
      return;
    }

    // Generic echo/info
    if (cmd === 'help') {
      appendShell('Commands: help, clear, version\n');
      return;
    }
    if (cmd === 'clear') { history.innerHTML = ''; return; }
    if (cmd === 'version') { appendShell('AC IDE v0.1.0 / AC Compiler v0.2.0\n'); return; }
    appendShell('[Shell: run arbitrary commands via your system terminal]\n');
  };

  runBtn.onclick   = runCmd;
  input.addEventListener('keydown', e => { if (e.key === 'Enter') runCmd(); });
}

function appendShell(text) {
  const h = document.getElementById('shell-history');
  const span = document.createElement('span');
  span.innerHTML = ansiToHtml(text);
  h.appendChild(span);
  h.scrollTop = h.scrollHeight;
}

// ── Output helpers ─────────────────────────────────────────────────────────────
function clearOutputPane() {
  document.getElementById('output-text').innerHTML = '';
}

function appendOutput(raw) {
  const el = document.getElementById('output-text');
  const span = document.createElement('span');
  span.innerHTML = ansiToHtml(raw);
  el.appendChild(span);
  el.parentElement.scrollTop = el.parentElement.scrollHeight;
  // Switch to output tab
  activateTab('output');
}

function appendLog(text) {
  const el = document.getElementById('log-text');
  el.textContent += text;
  el.parentElement.scrollTop = el.parentElement.scrollHeight;
}

function activateTab(name) {
  document.querySelectorAll('.out-tab').forEach(t => t.classList.toggle('active', t.dataset.tab === name));
  document.querySelectorAll('.out-pane').forEach(p => p.classList.toggle('active', p.id === 'pane-' + name));
}

// ── ANSI → HTML ───────────────────────────────────────────────────────────────
function ansiToHtml(text) {
  return text
    .replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')
    .replace(/\x1b\[0m/g,'</span>')
    .replace(/\x1b\[1m/g,'<span class="ansi-bold">')
    .replace(/\x1b\[31m/g,'<span class="ansi-red">')
    .replace(/\x1b\[32m/g,'<span class="ansi-green">')
    .replace(/\x1b\[33m/g,'<span class="ansi-yellow">')
    .replace(/\x1b\[34m/g,'<span class="ansi-blue">')
    .replace(/\x1b\[90m/g,'<span class="ansi-gray">')
    .replace(/\x1b\[\d+m/g,''); // strip other codes
}

// ── State helpers ──────────────────────────────────────────────────────────────
function setDirty(v) {
  isDirty = v;
  updateTitle();
}

function updateTitle(name) {
  const n = name || (currentFile ? currentFile.split('/').pop() : 'untitled.ac');
  document.getElementById('titlebar-file').textContent = (isDirty ? '● ' : '') + n;
  document.title = (isDirty ? '● ' : '') + n + '  —  AC IDE';
}

function setStatus(text, type) {
  document.getElementById('status-text').textContent = text;
  const dot = document.getElementById('status-dot');
  dot.className = 'dot ' + (type || 'idle');
}

function setRunning(v) {
  isRunning = v;
  document.getElementById('btn-run').disabled  = v;
  document.getElementById('btn-build').disabled = v;
  document.getElementById('btn-all').disabled  = v;
  document.getElementById('btn-stop').style.opacity = v ? '1' : '0.4';
}

// ── Splitter drag ──────────────────────────────────────────────────────────────
function setupSplitter() {
  const splitter    = document.getElementById('splitter');
  const outputPanel = document.getElementById('output-panel');
  const main        = document.getElementById('main');
  let dragging = false;
  let startY, startH;

  splitter.addEventListener('mousedown', e => {
    dragging = true;
    startY   = e.clientY;
    startH   = outputPanel.offsetHeight;
    document.body.style.cursor = 'row-resize';
    document.body.style.userSelect = 'none';
  });

  document.addEventListener('mousemove', e => {
    if (!dragging) return;
    const delta = startY - e.clientY;
    const newH  = Math.max(80, Math.min(500, startH + delta));
    outputPanel.style.height = newH + 'px';
  });

  document.addEventListener('mouseup', () => {
    dragging = false;
    document.body.style.cursor = '';
    document.body.style.userSelect = '';
  });
}
