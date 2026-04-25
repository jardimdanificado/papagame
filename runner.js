// Button definitions mirroring host.c
const BTN_UP     = (1 << 0);
const BTN_DOWN   = (1 << 1);
const BTN_LEFT   = (1 << 2);
const BTN_RIGHT  = (1 << 3);
const BTN_A      = (1 << 4);
const BTN_B      = (1 << 5);
const BTN_X      = (1 << 6);
const BTN_Y      = (1 << 7);
const BTN_L1     = (1 << 8);
const BTN_R1     = (1 << 9);
const BTN_START  = (1 << 10);
const BTN_SELECT = (1 << 11);
const BTN_L2     = (1 << 12);
const BTN_R2     = (1 << 13);
const BTN_L3     = (1 << 14);
const BTN_R3     = (1 << 15);

// DOM Elements
const canvas = document.getElementById('gameCanvas');
const ctx = canvas.getContext('2d', { alpha: false });
const wasmInput = document.getElementById('wasmInput');
const wrapper = document.getElementById('canvasWrapper');

// WASM State
let wasmInstance = null;
let wasmMemory = null;
let loopId = null;

// System Config Offset is predictable (always 0)
let sysOffset = 0;

// Host imported functions
const env = {
    get_ticks: () => Math.floor(performance.now())
};

// Keyboard state
let gamepadButtons = 0;

// Keyboard mapping
const keyMap = {
    'ArrowUp': BTN_UP,
    'ArrowDown': BTN_DOWN,
    'ArrowLeft': BTN_LEFT,
    'ArrowRight': BTN_RIGHT,
    'z': BTN_A,
    'Z': BTN_A,
    'x': BTN_B,
    'X': BTN_B,
    'Enter': BTN_START,
    'Shift': BTN_SELECT,
    'Escape': BTN_SELECT
};

// SDL-to-JS scancode mapping for the keys[] array
const scancodeMap = {
    'ArrowUp': 82,    // SDL_SCANCODE_UP
    'ArrowDown': 81,  // SDL_SCANCODE_DOWN
    'ArrowLeft': 80,  // SDL_SCANCODE_LEFT
    'ArrowRight': 79, // SDL_SCANCODE_RIGHT
    'Space': 44,      // SDL_SCANCODE_SPACE
    'Enter': 40,      // SDL_SCANCODE_RETURN
    'KeyW': 26, 'KeyA': 4, 'KeyS': 22, 'KeyD': 7,
};

window.addEventListener('keydown', (e) => {
    if (keyMap[e.key]) {
        gamepadButtons |= keyMap[e.key];
        e.preventDefault();
    }
    if (wasmMemory && sysOffset !== null) {
        const memoryView = new DataView(wasmMemory.buffer);
        const code = scancodeMap[e.code] || (e.keyCode & 0xFF); 
        memoryView.setUint8(sysOffset + 40 + code, 1);
    }
});

window.addEventListener('keyup', (e) => {
    if (keyMap[e.key]) {
        gamepadButtons &= ~keyMap[e.key];
        e.preventDefault();
    }
    if (wasmMemory && sysOffset !== null) {
        const memoryView = new DataView(wasmMemory.buffer);
        const code = scancodeMap[e.code] || (e.keyCode & 0xFF);
        memoryView.setUint8(sysOffset + 40 + code, 0);
    }
});

// Virtual Gamepad Mapping
const btnAttrMap = {
    'up': BTN_UP, 'down': BTN_DOWN, 'left': BTN_LEFT, 'right': BTN_RIGHT,
    'a': BTN_A, 'b': BTN_B, 'start': BTN_START, 'select': BTN_SELECT
};

document.querySelectorAll('.virtual-gamepad button').forEach(btn => {
    const bit = btnAttrMap[btn.getAttribute('data-btn')];
    if (!bit) return;
    
    const press = (e) => { gamepadButtons |= bit; e.preventDefault(); };
    const release = (e) => { gamepadButtons &= ~bit; e.preventDefault(); };
    
    btn.addEventListener('pointerdown', press);
    btn.addEventListener('pointerup', release);
    btn.addEventListener('pointercancel', release);
    btn.addEventListener('pointerout', release);
    btn.addEventListener('contextmenu', e => e.preventDefault());
});

// WASM Initialization
wasmInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) return;

    if (loopId) cancelAnimationFrame(loopId);

    const buffer = await file.arrayBuffer();
    try {
        const { instance } = await WebAssembly.instantiate(buffer, { env });
        wasmInstance = instance;
        wasmMemory = instance.exports.memory;
        
        startEngine();
    } catch (err) {
        alert("Failed to load WASM. Ensure it is a valid papagaio game.\n\n" + err);
        console.error(err);
    }
});

function startEngine() {
    wrapper.classList.remove('empty');
    canvas.focus(); // Grab focus for inputs

    // Call init
    if (wasmInstance.exports.papagaio_init) {
        wasmInstance.exports.papagaio_init();
    }

    // Get system config ptr (predictable layout)
    sysOffset = 0;

    // Check memory requirements and grow if necessary (replicating host.c)
    const dv = new DataView(wasmMemory.buffer);
    const ram = dv.getUint32(sysOffset + 8, true);
    const vram = dv.getUint32(sysOffset + 12, true);
    const vramPtr = 296; // sizeof(SystemConfig)
    const ramPtr = vramPtr + vram;
    const totalNeeded = ramPtr + ram;
    const requiredPages = Math.ceil(totalNeeded / 65536);
    const currentPages = wasmMemory.buffer.byteLength / 65536;
    
    if (requiredPages > currentPages) {
        wasmMemory.grow(requiredPages - currentPages);
    }

    // Refresh DataView after memory growth, as the buffer reference might change
    const updatedDv = new DataView(wasmMemory.buffer);
    const w = updatedDv.getUint32(sysOffset + 0, true);
    const h = updatedDv.getUint32(sysOffset + 4, true);

    canvas.width = w;
    canvas.height = h;

    // Start loop
    lastTime = performance.now();
    loopId = requestAnimationFrame(gameLoop);
}

// Reusable ImageData
let imageDataCache = null;

function gameLoop() {
    loopId = requestAnimationFrame(gameLoop);

    const memoryView = new DataView(wasmMemory.buffer);
    
    // Inject gamepad inputs
    memoryView.setUint32(sysOffset + 20, gamepadButtons, true);

    // Call Update
    wasmInstance.exports.papagaio_update();

    // Check Dirty flags
    const redraw = memoryView.getUint32(sysOffset + 16, true);
    
    if (redraw) {
        renderFrame(memoryView);
        // Clear dirty flag
        memoryView.setUint32(sysOffset + 16, 0, true);
    }
}

function renderFrame(memoryView) {
    const w = memoryView.getUint32(sysOffset + 0, true);
    const h = memoryView.getUint32(sysOffset + 4, true);
    const vramPtr = 296; // sizeof(SystemConfig)

    // Create or reuse ImageData
    if (!imageDataCache || imageDataCache.width !== w || imageDataCache.height !== h) {
        imageDataCache = ctx.createImageData(w, h);
    }

    // Read RGB565 from WebAssembly memory
    const mem16 = new Uint16Array(wasmMemory.buffer, vramPtr, w * h);
    
    const data = imageDataCache.data;
    let outIdx = 0;

    for (let i = 0; i < mem16.length; i++) {
        const color = mem16[i];
        
        // Extract RGB565
        const r = (color >> 11) & 0x1F;
        const g = (color >> 5) & 0x3F;
        const b = color & 0x1F;
        
        // Scale to 8-bit
        data[outIdx++] = (r << 3) | (r >> 2);
        data[outIdx++] = (g << 2) | (g >> 4);
        data[outIdx++] = (b << 3) | (b >> 2);
        data[outIdx++] = 255; 
    }

    ctx.putImageData(imageDataCache, 0, 0);
}
