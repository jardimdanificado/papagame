
// UI Elements
const canvas = document.getElementById('gameCanvas');
const wrapper = document.getElementById('canvasWrapper');
let ctx = canvas.getContext('2d', { alpha: false, desynchronized: true });

let worker = null;
let imageDataCache = null;

// Buffers compartilhados para Input (Sincronia instantânea entre threads)
const sharedInput = {
    keys: new Uint8Array(new SharedArrayBuffer(256)),
    gamepad: new Uint32Array(new SharedArrayBuffer(4))
};

// Mapeamento simplificado de Teclas (Browser -> SDL Scancodes)
const keyMap = {
    'ArrowRight': 79, 'ArrowLeft': 80, 'ArrowDown': 81, 'ArrowUp': 82,
    'KeyZ': 29, 'KeyX': 27, 'Enter': 40, 'Escape': 41, 'ShiftLeft': 225
};

const wasmInput = document.getElementById('wasmInput');
wasmInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) return;
    if (worker) worker.terminate();
    
    worker = new Worker('worker.js');
    worker.onmessage = (msg) => {
        const { type, w, h, frame } = msg.data;
        if (type === 'init') {
            canvas.width = w; canvas.height = h;
            wrapper.classList.remove('empty');
            canvas.focus();
        } else if (type === 'draw') {
            renderFrame(frame, w, h);
        }
    };

    const buffer = await file.arrayBuffer();
    // Enviamos o código do jogo E os buffers de input
    worker.postMessage({ 
        type: 'load', 
        buffer, 
        sharedKeys: sharedInput.keys,
        sharedGamepad: sharedInput.gamepad
    }, [buffer]);
});

// Captura de Inputs na Thread Principal
window.addEventListener('keydown', (e) => {
    const code = keyMap[e.code];
    if (code !== undefined) sharedInput.keys[code] = 1;
});

window.addEventListener('keyup', (e) => {
    const code = keyMap[e.code];
    if (code !== undefined) sharedInput.keys[code] = 0;
});

function renderFrame(frame, w, h) {
    if (!imageDataCache || imageDataCache.width !== w || imageDataCache.height !== h) {
        imageDataCache = ctx.createImageData(w, h);
    }
    const data = imageDataCache.data;
    const frame16 = new Uint16Array(frame);
    for (let i = 0; i < w * h; i++) {
        const color = frame16[i];
        const idx = i * 4;
        data[idx]     = ((color >> 11) & 0x1F) << 3;
        data[idx + 1] = ((color >> 5) & 0x3F) << 2;
        data[idx + 2] = (color & 0x1F) << 3;
        data[idx + 3] = 255;
    }
    ctx.putImageData(imageDataCache, 0, 0);
}
