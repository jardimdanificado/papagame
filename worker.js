
let wasmInstance = null;
let wasmMemory = null;
let sysOffset = 0;

// Buffers de input vindos da thread principal
let sharedKeys = null;
let sharedGamepad = null;

let lastFrameTime = 0;
const FRAME_MIN_TIME = 1000 / 60; 

self.onmessage = async (e) => {
    const { type, buffer, sys_offset, sharedKeys: keys, sharedGamepad: gamepad } = e.data;
    
    if (type === 'load') {
        sysOffset = sys_offset || 0;
        sharedKeys = keys;
        sharedGamepad = gamepad;

        const importObject = {
            env: {
                init: (w, h, vram, ram) => {
                    const dv = new DataView(wasmMemory.buffer);
                    dv.setUint32(sysOffset + 0, w, true);
                    dv.setUint32(sysOffset + 4, h, true);
                    dv.setUint32(sysOffset + 8, ram, true);
                    dv.setUint32(sysOffset + 12, vram, true);
                    self.postMessage({ type: 'init', w, h, vram, ram });
                },
                draw: () => {
                    const dv = new DataView(wasmMemory.buffer);
                    const w = dv.getUint32(sysOffset + 0, true);
                    const h = dv.getUint32(sysOffset + 4, true);

                    if (w === 0 || h === 0) return;

                    // --- SINCRONIZA INPUT ---
                    // Copiamos o estado do SharedArrayBuffer para a memória do WASM
                    if (sharedKeys) {
                        const wasmKeys = new Uint8Array(wasmMemory.buffer, sysOffset + 40, 256);
                        wasmKeys.set(sharedKeys);
                    }
                    if (sharedGamepad) {
                        dv.setUint32(sysOffset + 20, sharedGamepad[0], true);
                    }

                    // --- THROTTLING ---
                    const now = performance.now();
                    while (performance.now() - lastFrameTime < FRAME_MIN_TIME) { }
                    lastFrameTime = performance.now();

                    // --- RENDER ---
                    const vramPtr = sysOffset + 296;
                    const frame = new Uint16Array(wasmMemory.buffer, vramPtr, w * h);
                    const frameCopy = new Uint16Array(frame);
                    
                    self.postMessage({ 
                        type: 'draw', w, h,
                        frame: frameCopy.buffer 
                    }, [frameCopy.buffer]);
                },
                get_ticks: () => performance.now()
            }
        };

        const { instance } = await WebAssembly.instantiate(buffer, importObject);
        wasmInstance = instance;
        wasmMemory = instance.exports.memory;
        
        if (instance.exports.main) {
            instance.exports.main();
        }
    }
};
