const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('acide', {
  // window
  minimize:    ()         => ipcRenderer.send('win-minimize'),
  maximize:    ()         => ipcRenderer.send('win-maximize'),
  close:       ()         => ipcRenderer.send('win-close'),

  // options
  getOptions:  ()         => ipcRenderer.invoke('get-options'),
  saveOptions: (opts)     => ipcRenderer.invoke('save-options', opts),
  resetOptions:()         => ipcRenderer.invoke('reset-options'),

  // files
  readFile:    (p)        => ipcRenderer.invoke('read-file', p),
  writeFile:   (p, c)     => ipcRenderer.invoke('write-file', p, c),
  openFile:    ()         => ipcRenderer.invoke('open-file-dialog'),
  saveAs:      (c, p)     => ipcRenderer.invoke('save-file-dialog', c, p),

  // examples
  listExamples:()         => ipcRenderer.invoke('list-examples'),

  // compile & run
  compileRun:  (opts)     => ipcRenderer.invoke('compile-run', opts),
  compileOnly: (opts)     => ipcRenderer.invoke('compile-only', opts),
  compileAll:  (opts)     => ipcRenderer.invoke('compile-all', opts),
  stopRun:     ()         => ipcRenderer.send('stop-run'),

  // streaming output listener
  onRunOutput: (cb)       => ipcRenderer.on('run-output', (_, data) => cb(data)),
  removeRunOutput: ()     => ipcRenderer.removeAllListeners('run-output'),

  // shell
  openExternal:(url)      => ipcRenderer.send('open-external', url),
  showInFolder:(p)        => ipcRenderer.send('show-in-folder', p),
});
