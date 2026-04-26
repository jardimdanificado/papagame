# Funnybuffer Engine

Funnybuffer is a minimalist WebAssembly-based game engine designed to run binary cartridges (`.wasm`). It uses the **Wasm3** interpreter for execution and **SDL2** with OpenGL for hardware-accelerated rendering.

## Supported Platforms
- **Native Desktop:** Linux, Windows, macOS (OpenGL 2.1).
- **Handheld Devices (R36S / ARM64):** Native execution via PortMaster (OpenGLES 2.0).
- **Web:** Browser-based runner via WebAssembly.

---

## Architecture

The engine follows a simple shared-memory model:
1. **The Host (`host.c`):** A native binary that manages the window, inputs, and the Wasm3 VM.
2. **The Cartridge:** A `.wasm` module containing the game logic and rendering.

### Memory Map
The cartridge access hardware state through a shared memory structure located at the beginning of the WASM memory (offset `0`):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0      | 4    | `width` | Screen width in pixels |
| 4      | 4    | `height`| Screen height in pixels |
| 8      | 4    | `ram`   | Reserved RAM size |
| 12     | 4    | `vram`  | Reserved VRAM size |
| 16     | 4    | `redraw`| Set to 1 by cartridge to request a frame redraw |
| 20     | 4    | `gamepad_buttons` | Bitmask of active buttons |
| 24     | 16   | `joysticks` | 4x int32 for L/R sticks axes |
| 40     | 256  | `keys` | Array of 256 bytes for keyboard scancodes (1=down) |
| **296**| -    | **VRAM** | **RGB565 Framebuffer** (16-bit per pixel) |

---

## Cartridge Development

Cartridges must be compiled to WebAssembly (target `wasm32-unknown-unknown`) and export at least one of these functions:

```c
// Called every frame (60Hz)
void game_frame(void);

// OR
int main(void);
```

### Host Imports (env)
The host provides these functions to the cartridge:
- `void init(int w, int h, int vram, int ram)`: Sets the resolution and memory allocation.
- `uint32_t get_ticks()`: Returns the number of milliseconds since the engine started.

### Input Constants
```c
#define BTN_UP     (1 << 0)
#define BTN_DOWN   (1 << 1)
#define BTN_LEFT   (1 << 2)
#define BTN_RIGHT  (1 << 3)
#define BTN_A      (1 << 4)
#define BTN_B      (1 << 5)
#define BTN_START  (1 << 10)
#define BTN_SELECT (1 << 11)
```

---

## Build and Execution

### Desktop Build
```bash
make
```

### Execution
```bash
# Usage: ./funnybuffer <cartridge.wasm> [render_scale]
./funnybuffer game.wasm 4
```

### PortMaster Export (ARM64)
```bash
make portmaster
```
This generates the `funnybuffer/` directory and `funnybuffer.sh` for deployment on handheld devices like the R36S.
