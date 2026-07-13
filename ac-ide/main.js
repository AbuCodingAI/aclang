const { app, BrowserWindow, ipcMain, dialog, shell, Menu } = require('electron');
const path   = require('path');
const fs     = require('fs');
const os     = require('os');
const cp     = require('child_process');

// ── paths ─────────────────────────────────────────────────────────────────────
const PROJECT_ROOT = path.resolve(__dirname, '..');
const AC_BIN       = path.join(PROJECT_ROOT, 'ac-compiler', 'ac');
const EXAMPLES_DIR = path.join(PROJECT_ROOT, 'examples');
const USER_DATA    = app.getPath('userData');
const OPTIONS_FILE = path.join(USER_DATA, 'options.json');
const TEMP_DIR     = path.join(os.tmpdir(), 'ac-ide');

const DEFAULT_OPTIONS = {
  theme:       'dark',
  fontSize:    14,
  fontFamily:  'JetBrains Mono, Fira Code, monospace',
  tabSize:     4,
  wordWrap:    false,
  autoSave:    true,
  defaultBackend: 'BNY',
  showMinimap: false,
  runOnSave:   false,
  compilerPath: AC_BIN,
};

// ensure temp dir exists
if (!fs.existsSync(TEMP_DIR)) fs.mkdirSync(TEMP_DIR, { recursive: true });

function loadOptions() {
  try {
    if (fs.existsSync(OPTIONS_FILE))
      return { ...DEFAULT_OPTIONS, ...JSON.parse(fs.readFileSync(OPTIONS_FILE, 'utf8')) };
  } catch {}
  return { ...DEFAULT_OPTIONS };
}

function saveOptions(opts) {
  fs.mkdirSync(path.dirname(OPTIONS_FILE), { recursive: true });
  fs.writeFileSync(OPTIONS_FILE, JSON.stringify(opts, null, 2));
}

// ── window ─────────────────────────────────────────────────────────────────────
let mainWindow;
let runningProc = null;

function createWindow() {
  mainWindow = new BrowserWindow({
    width:          1280,
    height:         800,
    minWidth:       800,
    minHeight:      500,
    frame:          false,
    titleBarStyle:  'hidden',
    backgroundColor:'#0d1117',
    icon:           path.join(__dirname, 'renderer', 'assets', 'icon.png'),
    webPreferences: {
      preload:          path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration:  false,
    },
  });

  mainWindow.loadFile(path.join(__dirname, 'renderer', 'index.html'));
  mainWindow.on('closed', () => { mainWindow = null; });

  // Custom menu (minimal)
  Menu.setApplicationMenu(null);
}

app.whenReady().then(createWindow);
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });
app.on('activate', () => { if (!mainWindow) createWindow(); });

// ── IPC: window controls ───────────────────────────────────────────────────────
ipcMain.on('win-minimize', () => mainWindow?.minimize());
ipcMain.on('win-maximize', () => {
  if (mainWindow?.isMaximized()) mainWindow.unmaximize();
  else mainWindow?.maximize();
});
ipcMain.on('win-close', () => mainWindow?.close());

// ── IPC: options ───────────────────────────────────────────────────────────────
ipcMain.handle('get-options',  () => loadOptions());
ipcMain.handle('save-options', (_, opts) => { saveOptions(opts); return true; });
ipcMain.handle('reset-options', () => { saveOptions(DEFAULT_OPTIONS); return DEFAULT_OPTIONS; });

// ── IPC: file operations ───────────────────────────────────────────────────────
ipcMain.handle('read-file', async (_, filePath) => {
  return fs.readFileSync(filePath, 'utf8');
});

ipcMain.handle('write-file', async (_, filePath, content) => {
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, content, 'utf8');
  return true;
});

ipcMain.handle('open-file-dialog', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    filters: [{ name: 'AC Source', extensions: ['ac'] }, { name: 'All Files', extensions: ['*'] }],
    properties: ['openFile'],
  });
  if (result.canceled || !result.filePaths.length) return null;
  const filePath = result.filePaths[0];
  return { path: filePath, content: fs.readFileSync(filePath, 'utf8') };
});

ipcMain.handle('save-file-dialog', async (_, content, defaultPath) => {
  const result = await dialog.showSaveDialog(mainWindow, {
    defaultPath: defaultPath || 'untitled.ac',
    filters: [{ name: 'AC Source', extensions: ['ac'] }],
  });
  if (result.canceled || !result.filePath) return null;
  fs.writeFileSync(result.filePath, content, 'utf8');
  return result.filePath;
});

// ── IPC: examples ──────────────────────────────────────────────────────────────
ipcMain.handle('list-examples', () => {
  try {
    return fs.readdirSync(EXAMPLES_DIR)
      .filter(f => f.endsWith('.ac'))
      .map(f => ({ name: f, path: path.join(EXAMPLES_DIR, f) }));
  } catch { return []; }
});

// ── IPC: compile & run ─────────────────────────────────────────────────────────
ipcMain.handle('compile-run', async (event, { source, backend, fileName }) => {
  const opts    = loadOptions();
  const acBin   = opts.compilerPath || AC_BIN;
  const tmpFile = path.join(TEMP_DIR, fileName || 'scratch.ac');
  const stem    = path.join(TEMP_DIR, (fileName || 'scratch').replace(/\.ac$/, ''));

  fs.writeFileSync(tmpFile, source, 'utf8');

  // Kill previous run
  if (runningProc) { try { runningProc.kill('SIGTERM'); } catch {} runningProc = null; }

  return new Promise((resolve) => {
    const args = [tmpFile, '--target', backend || 'BNY', '--no-cache'];
    const cproc = cp.spawn(acBin, args, { cwd: TEMP_DIR });
    runningProc = cproc;

    let compileOut = '';
    let compileErr = '';
    cproc.stdout.on('data', d => { compileOut += d.toString(); });
    cproc.stderr.on('data', d => { compileErr += d.toString(); });

    cproc.on('close', (code) => {
      runningProc = null;
      if (code !== 0) {
        resolve({ success: false, output: compileErr || compileOut, phase: 'compile' });
        return;
      }

      // Determine output binary / script
      const binPath = findOutput(stem, backend);
      if (!binPath) {
        resolve({ success: true, output: compileOut + '\n[No runnable output for backend ' + backend + ']', phase: 'done' });
        return;
      }

      // Run the output
      event.sender.send('run-output', '\x1b[32m' + (compileOut || '✓ Compiled\n') + '\x1b[0m');
      runOutput(event, binPath, backend, resolve);
    });
  });
});

ipcMain.handle('compile-only', async (event, { source, backend, fileName }) => {
  const opts    = loadOptions();
  const acBin   = opts.compilerPath || AC_BIN;
  const tmpFile = path.join(TEMP_DIR, fileName || 'scratch.ac');
  fs.writeFileSync(tmpFile, source, 'utf8');

  return new Promise((resolve) => {
    const args = [tmpFile, '--target', backend || 'PY', '--no-cache'];
    const cproc = cp.spawn(acBin, args, { cwd: TEMP_DIR });
    let out = '', err = '';
    cproc.stdout.on('data', d => out += d.toString());
    cproc.stderr.on('data', d => err += d.toString());
    cproc.on('close', code => {
      resolve({ success: code === 0, output: out + err });
    });
  });
});

ipcMain.handle('compile-all', async (event, { source, fileName }) => {
  const opts    = loadOptions();
  const acBin   = opts.compilerPath || AC_BIN;
  const tmpFile = path.join(TEMP_DIR, fileName || 'scratch.ac');
  fs.writeFileSync(tmpFile, source, 'utf8');

  return new Promise((resolve) => {
    const cproc = cp.spawn(acBin, [tmpFile, '--all', '--no-cache'], { cwd: TEMP_DIR });
    let out = '', err = '';
    cproc.stdout.on('data', d => out += d.toString());
    cproc.stderr.on('data', d => err += d.toString());
    cproc.on('close', code => {
      resolve({ success: code === 0, output: out + err });
    });
  });
});

ipcMain.on('stop-run', () => {
  if (runningProc) { try { runningProc.kill('SIGTERM'); } catch {} runningProc = null; }
});

// ── IPC: open in file manager ──────────────────────────────────────────────────
ipcMain.on('open-external', (_, url) => shell.openExternal(url));
ipcMain.on('show-in-folder', (_, p) => shell.showItemInFolder(p));

// ── helpers ────────────────────────────────────────────────────────────────────
const BACKEND_EXT = {
  PY: '.py', JS: '.js', C: '.c', CPP: '.cpp', Java: '.java',
  RS: '.rs', GO: '.go', V: '.v', ASM: '.s', BNY: '.acb', HTML: '.html',
};

function findOutput(stem, backend) {
  const ext = BACKEND_EXT[backend];
  if (!ext) return null;
  const p = stem + ext;
  return fs.existsSync(p) ? p : null;
}

function runnerFor(filePath, backend) {
  switch (backend) {
    case 'PY':   return { cmd: 'python3', args: [filePath] };
    case 'JS':   return { cmd: 'node',    args: [filePath] };
    case 'BNY':  return { cmd: filePath,  args: [] };
    case 'C': {
      // AC already compiled via gcc; binary = stem without extension
      const bin = filePath.replace(/\.c$/, '');
      return fs.existsSync(bin) ? { cmd: bin, args: [] } : null;
    }
    default: return null;
  }
}

function runOutput(event, filePath, backend, resolve) {
  const runner = runnerFor(filePath, backend);
  if (!runner) {
    resolve({ success: true, output: '[Output file: ' + filePath + ']', phase: 'done' });
    return;
  }

  const rproc = cp.spawn(runner.cmd, runner.args, { cwd: TEMP_DIR });
  runningProc = rproc;
  let out = '';

  rproc.stdout.on('data', d => {
    const s = d.toString();
    out += s;
    event.sender.send('run-output', s);
  });
  rproc.stderr.on('data', d => {
    const s = d.toString();
    out += s;
    event.sender.send('run-output', '\x1b[31m' + s + '\x1b[0m');
  });
  rproc.on('close', code => {
    runningProc = null;
    const exitMsg = '\n\x1b[90m[Process exited with code ' + code + ']\x1b[0m\n';
    event.sender.send('run-output', exitMsg);
    resolve({ success: code === 0, output: out, phase: 'run' });
  });
}
