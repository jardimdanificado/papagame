
// UI Elements
const canvas = document.getElementById('gameCanvas');
const wrapper = document.getElementById('canvasWrapper');
const ctx = canvas.getContext('2d', { alpha: false, desynchronized: true });

let wasmInstance = null;
let wasmMemory = null;
let animationFrameId = null;
let imageDataCache = null;
let lastFrameTime = 0;
const FRAME_MIN_TIME = 1000 / 60; // Target 60 FPS

const sysOffset = 0;

// Audio State
let audioCtx = null;
let audioNode = null;

// Estado do Input
const input = {
    keys: new Uint8Array(256),
    buttons: 0,
    mouse: { x: 0, y: 0, buttons: 0, wheel: 0 }
};

// Mapeamento de Teclas
const keyMap = {
    'ArrowRight': 79, 'ArrowLeft': 80, 'ArrowDown': 81, 'ArrowUp': 82,
    'KeyZ': 29, 'KeyX': 27, 'Enter': 40, 'Escape': 41, 'ShiftLeft': 225,
    'KeyW': 26, 'KeyA': 4, 'KeyS': 22, 'KeyD': 7
};

const btnMap = {
    'up': 1 << 0, 'down': 1 << 1, 'left': 1 << 2, 'right': 1 << 3,
    'a': 1 << 4, 'b': 1 << 5, 'select': 1 << 6, 'start': 1 << 7
};

const wasmInput = document.getElementById('wasmInput');

// Handle file input
wasmInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) return;

    const buffer = await file.arrayBuffer();
    await loadCartridge(buffer);
});

// Handle gallery clicks
document.getElementById('gallery').addEventListener('click', async (e) => {
    const btn = e.target.closest('.gallery-btn');
    if (!btn) return;
    const url = btn.dataset.url;
    if (url) {
        try {
            const response = await fetch(url);
            const bytes = await response.arrayBuffer();
            await loadCartridge(bytes);
        } catch (err) {
            console.error("Failed to load example:", err);
        }
    }
});

async function loadCartridge(buffer) {
    // Web Audio requires user interaction to start context
    if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }
    if (audioCtx.state === 'suspended') {
        audioCtx.resume();
    }
    await loadWasm(buffer);
}

async function loadWasm(buffer) {
    if (animationFrameId) {
        cancelAnimationFrame(animationFrameId);
    }
    
    if (audioNode) {
        audioNode.disconnect();
        audioNode = null;
    }

    const importObject = {
        env: {
            init: (titlePtr, w, h, bpp, scale, audioSize, audioRate, audioBpp, audioChannels) => {
                canvas.width = w;
                canvas.height = h;
                wrapper.classList.remove('empty');
                canvas.focus();
                
                const dv = new DataView(wasmMemory.buffer);
                
                // Copy title
                if (titlePtr) {
                    const titleBytes = new Uint8Array(wasmMemory.buffer, titlePtr, 128);
                    const destTitle = new Uint8Array(wasmMemory.buffer, sysOffset + 0, 128);
                    destTitle.set(titleBytes);
                }

                dv.setUint32(sysOffset + 128, w, true);
                dv.setUint32(sysOffset + 132, h, true);
                dv.setUint32(sysOffset + 136, bpp, true);
                dv.setUint32(sysOffset + 140, scale, true);
                dv.setUint32(sysOffset + 144, audioSize, true);
                dv.setUint32(sysOffset + 148, 0, true); // audio_write_ptr
                dv.setUint32(sysOffset + 152, 0, true); // audio_read_ptr
                dv.setUint32(sysOffset + 156, audioRate, true);
                dv.setUint32(sysOffset + 160, audioBpp, true);
                dv.setUint32(sysOffset + 164, audioChannels, true);
                
                // Initialize Web Audio if size > 0
                if (audioSize > 0) {
                    if (audioCtx && audioCtx.sampleRate !== audioRate) {
                        audioCtx.close();
                        audioCtx = null;
                    }
                    if (!audioCtx) {
                        audioCtx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: audioRate });
                    }
                    if (audioCtx.state === 'suspended') {
                        audioCtx.resume();
                    }
                    setupWebAudio(audioSize, audioRate, audioBpp, audioChannels || 2);
                }

                console.log(`>>> Wagnostic Web Init: ${w}x${h}@${bpp}bpp (Scale: ${scale}, Audio: ${audioSize} bytes)`);
            },
            get_ticks: () => performance.now()
        }
    };

    const { instance } = await WebAssembly.instantiate(buffer, importObject);
    wasmInstance = instance;
    wasmMemory = instance.exports.memory;
    
    lastFrameTime = performance.now();
    animationFrameId = requestAnimationFrame(gameLoop);
}

function setupWebAudio(size, rate, bpp, channels) {
    audioNode = audioCtx.createScriptProcessor(2048, 0, channels);
    
    audioNode.onaudioprocess = (e) => {
        if (!wasmMemory) return;
        
        const dv = new DataView(wasmMemory.buffer);
        const r = dv.getUint32(sysOffset + 152, true);
        const w = dv.getUint32(sysOffset + 148, true);
        const s = dv.getUint32(sysOffset + 144, true);
        
        const width = dv.getUint32(sysOffset + 128, true);
        const height = dv.getUint32(sysOffset + 132, true);
        const videoBpp = dv.getUint32(sysOffset + 136, true);
        const vramSize = width * height * (videoBpp / 8);
        const audioBufPtr = 512 + vramSize;

        let bytesAvailable = (w >= r) ? (w - r) : (s - r + w);
        const frameCount = e.outputBuffer.length;
        const bytesNeeded = frameCount * channels * bpp;
        
        const toRead = Math.min(bytesNeeded, bytesAvailable);
        const samplesToRead = Math.floor(toRead / bpp);

        const outChannels = [];
        for (let ch = 0; ch < channels; ch++) {
            outChannels.push(e.outputBuffer.getChannelData(ch));
        }

        let currR = r;
        const mem8 = new Uint8Array(wasmMemory.buffer);
        const mem16 = new Int16Array(wasmMemory.buffer);
        const memF32 = new Float32Array(wasmMemory.buffer);

        for (let i = 0; i < samplesToRead / channels; i++) {
            for (let ch = 0; ch < channels; ch++) {
                let sample = 0;
                if (bpp === 1) { // PCM8 Unsigned
                    sample = (mem8[audioBufPtr + currR] - 128) / 128;
                    currR = (currR + 1) % s;
                } else if (bpp === 4) { // Float32
                    sample = memF32[(audioBufPtr + currR) / 4];
                    currR = (currR + 4) % s;
                } else { // PCM16 Signed
                    sample = mem16[(audioBufPtr + currR) / 2] / 32768;
                    currR = (currR + 2) % s;
                }
                outChannels[ch][i] = sample;
            }
        }
        
        for (let i = Math.floor(samplesToRead / channels); i < frameCount; i++) {
            for (let ch = 0; ch < channels; ch++) {
                outChannels[ch][i] = 0;
            }
        }

        dv.setUint32(sysOffset + 152, currR, true);
    };

    audioNode.connect(audioCtx.destination);
}

function gameLoop(now) {
    if (!wasmInstance) return;

    const elapsed = now - lastFrameTime;

    if (elapsed >= FRAME_MIN_TIME) {
        lastFrameTime = now - (elapsed % FRAME_MIN_TIME);

        const dv = new DataView(wasmMemory.buffer);

        const wasmKeys = new Uint8Array(wasmMemory.buffer, sysOffset + 192, 256);
        wasmKeys.set(input.keys);
        dv.setUint32(sysOffset + 172, input.buttons, true);

        dv.setInt32(sysOffset + 448, input.mouse.x, true);
        dv.setInt32(sysOffset + 452, input.mouse.y, true);
        dv.setUint32(sysOffset + 456, input.mouse.buttons, true);
        dv.setInt32(sysOffset + 460, input.mouse.wheel, true);

        const frameFunc = wasmInstance.exports.game_frame || wasmInstance.exports.main;
        if (frameFunc) frameFunc();

        const redraw = dv.getUint32(sysOffset + 168, true);
        if (redraw) {
            renderFrame(dv);
            dv.setUint32(sysOffset + 168, 0, true);
        }

        const titleBytes = new Uint8Array(wasmMemory.buffer, sysOffset + 0, 128);
        const firstZero = titleBytes.indexOf(0);
        const titleStr = new TextDecoder().decode(titleBytes.subarray(0, firstZero > -1 ? firstZero : 128)).trim();
        if (titleStr && document.title !== titleStr) document.title = titleStr;
    }

    animationFrameId = requestAnimationFrame(gameLoop);
}

function renderFrame(dv) {
    const w = dv.getUint32(sysOffset + 128, true);
    const h = dv.getUint32(sysOffset + 132, true);
    const bpp = dv.getUint32(sysOffset + 136, true);
    
    if (w === 0 || h === 0) return;

    if (!imageDataCache || imageDataCache.width !== w || imageDataCache.height !== h) {
        imageDataCache = ctx.createImageData(w, h);
    }
    
    const data = imageDataCache.data;
    const vramPtr = 512;
    const data32 = new Uint32Array(data.buffer);
    
    if (bpp === 32) {
        const frame32 = new Uint32Array(wasmMemory.buffer, vramPtr, w * h);
        data32.set(frame32);
    } else if (bpp === 8) {
        const frame8 = new Uint8Array(wasmMemory.buffer, vramPtr, w * h);
        for (let i = 0; i < w * h; i++) {
            const c = frame8[i];
            const r = Math.round(((c >> 5) & 0x07) * 255 / 7);
            const g = Math.round(((c >> 2) & 0x07) * 255 / 7);
            const b = Math.round((c & 0x03) * 255 / 3);
            data32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
        }
    } else {
        const frame16 = new Uint16Array(wasmMemory.buffer, vramPtr, w * h);
        for (let i = 0; i < w * h; i++) {
            const c = frame16[i];
            const r = Math.round(((c >> 11) & 0x1F) * 255 / 31);
            const g = Math.round(((c >> 5) & 0x3F) * 255 / 63);
            const b = Math.round((c & 0x1F) * 255 / 31);
            data32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
        }
    }
    ctx.putImageData(imageDataCache, 0, 0);
}

// Input Handlers
window.addEventListener('keydown', (e) => {
    const code = keyMap[e.code];
    if (code !== undefined) input.keys[code] = 1;
});

window.addEventListener('keyup', (e) => {
    const code = keyMap[e.code];
    if (code !== undefined) input.keys[code] = 0;
});

document.querySelectorAll('.virtual-gamepad button').forEach(btn => {
    const bit = btnMap[btn.dataset.btn];
    if (!bit) return;
    const setBtn = (val) => {
        if (val) input.buttons |= bit;
        else input.buttons &= ~bit;
    };
    btn.addEventListener('pointerdown', (e) => { e.preventDefault(); setBtn(true); });
    btn.addEventListener('pointerup', (e) => { e.preventDefault(); setBtn(false); });
});

canvas.addEventListener('mousemove', (e) => {
    const rect = canvas.getBoundingClientRect();
    input.mouse.x = Math.floor((e.clientX - rect.left) * (canvas.width / rect.width));
    input.mouse.y = Math.floor((e.clientY - rect.top) * (canvas.height / rect.height));
});

canvas.addEventListener('mousedown', (e) => { input.mouse.buttons |= (1 << e.button); });
canvas.addEventListener('mouseup', (e) => { input.mouse.buttons &= ~(1 << e.button); });
canvas.addEventListener('wheel', (e) => { input.mouse.wheel += Math.sign(e.deltaY); }, { passive: true });
