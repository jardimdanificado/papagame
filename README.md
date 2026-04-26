# Wagnostic

**Wagnostic** is a protocol. A tiny, fixed contract between a **Host** (the program that opens a window) and a **ROM** (a `.wasm` file that draws to it).

The ROM writes pixels to memory. The Host displays them. The Host writes button states to memory. The ROM reads them. That's it.

No APIs to learn. No SDKs to install. No engine. Just memory.

---

## The Core Idea

Every Wagnostic session shares a single block of memory between the Host and the ROM. The layout is always the same:

| Offset | Region | Size | Description |
|--------|--------|------|-------------|
| `0` | **System Header** | 512 bytes | Title, dimensions, BPP, audio config, input state. Fixed layout. |
| `512` | **VRAM** | `width × height × (bpp/8)` | Raw pixel data. Written by ROM, read by Host. |
| `512 + vram_size` | **Audio Buffer** | `audio_size` | PCM ring buffer. Written by ROM, drained by Host. |
| above audio | **ROM RAM** | remainder | General purpose heap. Managed by the ROM. |

The ROM draws by writing pixel values directly to offset `512`. The Host reads from there and blits to the screen. No draw calls. No render passes. Just a write.

---

## Writing a ROM

A minimal Wagnostic ROM in C looks like this:

```c
// The shared memory header lives at address 0
#define sys    ((volatile SystemConfig*)0)
// VRAM starts at byte 512
#define vram   ((volatile uint16_t*)512)

int main() {
    if (sys->width == 0)
        init("My Game", 320, 240, 16, 2, 0, 0, 0, 0);

    // Clear screen to dark blue
    for (int i = 0; i < 320 * 240; i++)
        vram[i] = RGB565(0, 0, 64);

    // Signal the host to redraw
    sys->redraw = 1;
    return 0;
}
```

Compile it:
```bash
clang --target=wasm32 -nostdlib \
      -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
      -Wl,--global-base=1048576 \
      rom.c -o rom.wasm
```

Run it on any Wagnostic host.

---

## Writing a Host

A host needs to do four things:

1. **Instantiate** the `.wasm` with two imported functions: `init` and `get_ticks`.
2. **Write** keyboard, gamepad, and mouse state into the header every frame.
3. **Call** the ROM's `game_frame` (or `main`) function once per frame.
4. **Blit** VRAM to the screen when `redraw == 1`, then reset it to 0.

In pseudocode:

```
while running:
    mem[192 + scancode] = key_pressed ? 1 : 0
    mem[172]            = gamepad_bitmask
    mem[448], mem[452]  = mouse_x, mouse_y

    call rom.game_frame()

    if mem[168] == 1:       // redraw flag
        display(mem[512 : 512 + vram_size])
        mem[168] = 0
```

That's a fully conformant Wagnostic host. The language doesn't matter. The platform doesn't matter. If you can read and write bytes and call a WASM function, you can run any Wagnostic ROM ever made.

The reference native runner (C + SDL2 + wasm3) is **465 lines of C** and compiles to a **~186KB binary**. The web runner is a single JavaScript file.

---

## What it Supports

**Graphics** — choose the format that fits your ROM:

| BPP | Format   | Bytes/pixel |
|-----|----------|-------------|
| 8   | RGB332   | 1 |
| 16  | RGB565   | 2 |
| 32  | RGBA8888 | 4 |

**Audio** — raw PCM ring buffer. The ROM writes samples, the Host drains them:

| Format | Description |
|--------|-------------|
| U8     | Unsigned 8-bit (silence = 128) |
| S16LE  | Signed 16-bit Little-Endian |
| F32LE  | IEEE Float 32-bit |

Any number of channels. Any sample rate.

**Input** — digital buttons, two analog sticks, full 256-key keyboard, and mouse (position, buttons, wheel).

---

## Why It Works Anywhere

Because there's almost nothing to implement.

Porting Wagnostic to a new platform means: pick a WASM runtime, open a framebuffer, copy bytes. The protocol doesn't assume a GPU, an OS, or a specific runtime. There's already a native desktop runner and a web runner — both are reference implementations, nothing more.

There's no reason it couldn't run on a microcontroller with a small display, inside a terminal using sixel graphics, or embedded inside another engine as a scripting sandbox. The surface area of the protocol is small enough that none of that would be surprising.

A ROM compiled today will run correctly on a host written five years from now — as long as both follow the spec. No ABI drift. No deprecation cycles. The memory map is the contract and it doesn't change.

---

## The WASM Advantage

Because ROMs are WebAssembly, they are:

- **Sandboxed** — a ROM cannot access anything outside its own memory. The Host is always in control.
- **Universal** — the same `.wasm` file runs natively on any architecture and in any browser, unchanged.
- **Language-agnostic** — ROMs can be written in C, Zig, Rust, AssemblyScript, or anything that targets WASM.

---

## Getting Started

- Read [**SPECIFICATION.md**](SPECIFICATION.md) to understand the full protocol and build your own host.
- Check `examples/c/` for reference ROMs.
- Check `runners/` for reference host implementations.

---

## License

This project is free to use, modify, and distribute — for any purpose, including commercial use — provided that appropriate credit is given to https://github.com/jardimdanificado.

No warranties are provided. Use at your own discretion.

Copyright © 2026 jardimdanificado. All Rights Reserved.
