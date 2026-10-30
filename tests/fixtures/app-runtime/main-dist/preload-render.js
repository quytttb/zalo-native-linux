// Synthetic preload fixture. Not Zalo code.
const { contextBridge, ipcRenderer } = require('electron');
contextBridge.exposeInMainWorld('$zapp', {
  getPath: () => ipcRenderer.sendSync('app:get-path'),
});
contextBridge.exposeInMainWorld('$zlogger', {
  write: (line) => ipcRenderer.send('log:write', line),
});
ipcRenderer.on('linux-theme-change', () => {});
require('./build/Release/fake.node');
