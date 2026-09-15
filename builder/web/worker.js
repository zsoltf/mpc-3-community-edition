importScripts('wasm_exec.js');

const go = new Go();
WebAssembly.instantiateStreaming(fetch('mpclearn-installer.wasm'), go.importObject)
  .then(({instance}) => go.run(instance))
  .catch(error => postMessage({type: 'error', message: 'Could not start the local image engine: ' + error.message}));

onmessage = event => {
  if (event.data.type === 'build') {
    if (typeof self.mpclearnBuild !== 'function') {
      postMessage({type: 'error', message: 'The image engine is still loading. Try again in a moment.'});
      return;
    }
    self.mpclearnBuild(new Uint8Array(event.data.image), new Uint8Array(event.data.ownerKey));
  }
};
