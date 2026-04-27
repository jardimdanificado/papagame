# 🛸 Wagnostic (formerly Funnybuffer)

**The Universal, Protocol-First Virtual Console.**

Wagnostic is a minimalist, platform-agnostic specification and runtime for high-performance interactive applications. It treats the machine as a raw memory buffer, where the WASM linear memory is the single source of truth for graphics, audio, and input.

## 🚀 Features

- **Extreme Portability**: Runs on anything from high-end PCs to Retro-Handhelds (PortMaster support).
- **Polymorphic Graphics**: Native support for **8bpp (RGB332)**, **16bpp (RGB565)**, and **32bpp (RGBA)**.
- **Protocol-First**: No complex APIs. Communication happens via a fixed 512-byte header and a flexible signal buffer.
- **Rich SDK**: Includes `wagnostic.h` for easy setup and `olive.h` (a polymorphic version of Olive.c) for 2D/3D software rendering.
- **High-Fidelity Audio**: Low-latency ring-buffer audio system.

## 📦 Memory Layout

| Offset | Size | Name | Description |
| :--- | :--- | :--- | :--- |
| `0x000` | 128 B | **Message** | Shared text buffer for Titles, Logs, and Signals. |
| `0x080` | 40 B | **Config** | Width, Height, BPP, Scale, Signal Count. |
| `0x090` | 24 B | **Audio** | Size, Pointers, Sample Rate, Channels. |
| `0x0A8` | 404 B | **Inputs** | Keys (256B), Mouse, Gamepad. |
| `0x200` | Var | **Signals** | Signal buffer (size defined by `signal_count`). |
| `0x200+sc`| Var | **VRAM** | Raw Framebuffer. |

## 🛠️ Quick Start (C SDK)

```c
#include "wagnostic.h"
#define OLIVEC_IMPLEMENTATION
#include "olive.h"

static Olivec_Canvas oc;

// Native entry point
__attribute__((visibility("default")))
void winit() {
    // Setup: Title, Width, Height, BPP, Scale, SignalCount
    w_setup("Hello Wagnostic", 320, 240, 16, 2, 8);
    oc = olivec_canvas(W_FB_PTR, 320, 240, 320, 16);
}

__attribute__((visibility("default")))
void wupdate() {
    olivec_fill(oc, 0x0000); // Clear screen
    olivec_circle(oc, 160, 120, 50, 0xF800); // Red circle
    w_redraw(); // Send signal to host
}
```

## 🏗️ Building

### Requirements
- **Host**: GCC, SDL2, OpenGL.
- **ROMs**: Clang (with wasm32 target).

### Commands
```bash
# Build the native runner
make

# Build examples
make -C examples

# Run an example
./wagnostic examples/roguelike_example.wasm
```

## 📜 Specification
For full technical details, see [SPECIFICATION.md](./SPECIFICATION.md).

---
*Built with ❤️ for the open-source gaming community.*
