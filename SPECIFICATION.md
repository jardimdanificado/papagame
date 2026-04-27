# Wagnostic Protocol Specification

This document contains everything you need to understand and build a fully compliant Wagnostic runner in any language or platform. No prior knowledge of the reference implementations is required.

---

## 1. Overview

A Wagnostic ROM is a standard **WebAssembly (.wasm)** binary. The Host is any program that:
1. Instantiates the WASM module.
2. Exposes a set of **imported functions** that the ROM can call.
3. Reads and writes to the module's **linear memory** to exchange state.

All communication happens through this shared memory. There are no sockets, no event queues, no callbacks. Just memory reads and writes.

---

## 2. WASM Interface

### 2.1 Functions the ROM **imports** (Host must provide)
The ROM will try to call these functions from the `env` module at instantiation time. You **must** provide them all.

#### `env.get_ticks() -> i32`
Returns the number of milliseconds elapsed since some arbitrary start point. Used by ROMs for timing. Return a monotonic, always-increasing value.

---

### 2.2 Functions the Host **calls** (ROM must export)
The Host drives the ROM by calling these functions. The ROM must export at least one of them.

#### `winit()`
Called by the Host **exactly once** immediately after instantiation. The ROM must use this function to write its configuration (title, width, height, BPP, audio settings) into the **System Header** (address 0x000).

#### `wupdate()`
Called once per frame by the Host's main loop. The ROM reads inputs from memory, updates state, renders to VRAM, and sets `redraw = 1` if a new frame is ready.

---

## 3. Memory Layout

The WASM instance's linear memory is the single source of truth. **All offsets are absolute, from the start of WASM linear memory (address 0).** All multi-byte integers are **Little-Endian**.

```
[0x000 - 0x1FF]  System Header    (512 bytes, fixed)
[0x200 - 0x200 + signal_count - 1]  Signal Buffer (signal_count bytes)
[0x200 + signal_count ...]      VRAM             (width × height × bpp/8 bytes)
[above vram]     Audio Buffer     (audio_size bytes, ring buffer)
[above audio]    ROM Heap / RAM   (all remaining memory)
```

### 3.1 System Header (0x000 – 0x1FF)

The first 512 bytes are owned by the protocol. The ROM reads inputs here and writes `redraw` and audio data here. The Host reads `redraw` and audio pointers here, and writes all input state here.

```
Offset  Size  Field            R/W        Description
------  ----  ---------------  ---------  -------------------------------------------
0       128   message          ROM→Host   UTF-8 text buffer for signals (title, logs, etc).
128     4     width            ROM→Host   Framebuffer width in pixels.
132     4     height           ROM→Host   Framebuffer height in pixels.
136     4     bpp              ROM→Host   Bits per pixel: 8, 16, or 32.
140     4     scale            ROM→Host   Suggested window scale multiplier.
144     4     audio_size       ROM→Host   Audio ring buffer size in bytes.
148     4     audio_write      ROM→Host   ROM's write pointer into the audio buffer.
152     4     audio_read       Host→ROM   Host's read pointer into the audio buffer.
156     4     audio_rate       ROM→Host   Sample rate (e.g. 44100).
160     4     audio_bpp        ROM→Host   Audio bytes per sample (1, 2, or 4).
164     4     audio_channels   ROM→Host   Number of audio channels.
168     4     signal_count     ROM→Host   Number of signal slots at offset 512.
172     4     gamepad          Host→ROM   Button bitmask (see Section 5.1).
176     4     joystick_lx      Host→ROM   Left stick X axis (signed int32, -32768..32767).
180     4     joystick_ly      Host→ROM   Left stick Y axis.
184     4     joystick_rx      Host→ROM   Right stick X axis.
188     4     joystick_ry      Host→ROM   Right stick Y axis.
192     256   keys             Host→ROM   One byte per key. 1 = pressed, 0 = released.
448     4     mouse_x          Host→ROM   Mouse X in framebuffer pixel coordinates.
452     4     mouse_y          Host→ROM   Mouse Y in framebuffer pixel coordinates.
456     4     mouse_buttons    Host→ROM   Mouse button bitmask (bit 0 = left, 1 = right, 2 = middle).
460     4     mouse_wheel      Host→ROM   Wheel delta (positive = scroll up).
464     48    reserved         —          Do not use. Reserved for future fields.
512     ...   signals          ROM→Host   Signal slots (size defined by signal_count).
512+sc  ...   VRAM             ROM→Host   Raw pixel data (Framebuffer).
```

---

## 4. Video / Framebuffer

### 4.1 Location and Size
VRAM starts at byte offset **512 + signal_count** and its size is:
```
vram_size = width * height * (bpp / 8)
```

### 4.2 Pixel Formats
The ROM chooses the format at `init` time via the `bpp` parameter.

#### 8-bit: RGB332
Each pixel is 1 byte. Layout:
```
Bit:  7 6 5 | 4 3 2 | 1 0
      R R R | G G G | B B
```
- Red: bits 7–5 (3 bits, 0–7)
- Green: bits 4–2 (3 bits, 0–7)
- Blue: bits 1–0 (2 bits, 0–3)

To convert to RGB888 for display:
```
r = round((pixel >> 5) & 0x07) * 255 / 7
g = round((pixel >> 2) & 0x07) * 255 / 7
b = round((pixel & 0x03) * 255 / 3
```

#### 16-bit: RGB565 (Little-Endian)
Each pixel is 2 bytes (stored Little-Endian). Read as a single `uint16_t`:
```
Bits: 15..11 = Red   (5 bits)
Bits: 10..5  = Green (6 bits)
Bits:  4..0  = Blue  (5 bits)
```
To convert to RGB888:
```
r = round(((pixel >> 11) & 0x1F) * 255 / 31)
g = round(((pixel >> 5)  & 0x3F) * 255 / 63)
b = round(( pixel        & 0x1F) * 255 / 31)
```

#### 32-bit: RGBA8888
Each pixel is 4 bytes. Byte order in memory:
```
Byte 0: Red
Byte 1: Green
Byte 2: Blue
Byte 3: Alpha (Host should ignore or treat as fully opaque)
```

### 4.3 Signals (The Signal Buffer)

Starting at offset **512**, there is a buffer of `signal_count` bytes. The ROM writes signal IDs here to tell the Host to perform specific actions. The Host processes these signals and **must reset them to 0** afterward.

#### Signal IDs:
- `0`: **NONE** (Noop)
- `1`: **REDRAW**: The ROM has finished a frame. The Host should blit the VRAM to the screen.
- `2`: **QUIT**: The ROM wants to exit. If `message` is not empty, the Host may display it as an exit message.
- `3`: **UPDATE_TITLE**: The Host should update the window title using the string in the `message` buffer.
- `4`: **UPDATE_WINDOW**: The `width`, `height`, `bpp`, or `scale` fields have been changed. The Host should resize the window.
- `5`: **UPDATE_AUDIO**: The `audio_rate` or `audio_channels` fields have been changed. The Host should reconfigure the audio device.
- `6`: **LOG_INFO**: The Host should print the string in `message` to the console (stdout).
- `7`: **LOG_WARN**: The Host should print the string in `message` to the console as a warning (stderr).
- `8`: **LOG_ERR**: The Host should print the string in `message` to the console as an error (stderr).

```pseudocode
call rom.wupdate()

signal_ptr = 512
for i in 0..signal_count:
    sig = read_u8(mem, signal_ptr + i)
    if sig == 0: continue
    
    if sig == 1: // REDRAW
        blit_vram_to_screen(mem[512 + signal_count : ...], width, height, bpp)
    elif sig == 2: // QUIT
        close_window()
    elif sig == 3: // UPDATE_TITLE
        set_window_title(read_cstring(mem, 0)) // uses message buffer
    elif sig == 6: // LOG_INFO
        print("INFO: " + read_cstring(mem, 0))
    
    write_u8(mem, signal_ptr + i, 0) // Reset signal
```

---

## 5. Input

The Host writes input state directly into memory before calling `wupdate`. The ROM reads it directly.

### 5.1 Gamepad (Offset 172)
A single `uint32` bitmask. Each bit represents one button:

| Bit | Button |
|-----|--------|
| 0   | Up |
| 1   | Down |
| 2   | Left |
| 3   | Right |
| 4   | A |
| 5   | B |
| 6   | X |
| 7   | Y |
| 8   | L1 |
| 9   | R1 |
| 10  | Select |
| 11  | Start |

### 5.2 Keyboard (Offset 192)
256 bytes, one per USB HID scancode. Write `1` if the key is currently held down, `0` otherwise. Scancodes follow the USB HID Usage Table (same as SDL2 `SDL_Scancode`).

Common mappings for reference implementations:
```
Arrow keys: 79 (Right), 80 (Left), 81 (Down), 82 (Up)
Z / X:      29 / 27
Enter:      40
Escape:     41
```

### 5.3 Joysticks (Offsets 176–191)
Four `int32` values for two analog sticks:
- `LX` (176), `LY` (180): Left stick.
- `RX` (184), `RY` (188): Right stick.

Range: **-32767** to **+32767**. Zero is centered.

### 5.4 Mouse (Offsets 448–463)
- `mouse_x` / `mouse_y`: Pixel coordinates relative to the framebuffer (0,0 at top-left).
- `mouse_buttons`: Bitmask. Bit 0 = Left, Bit 1 = Right, Bit 2 = Middle.
- `mouse_wheel`: Accumulated scroll delta since last frame. The Host should reset this to 0 after writing, or the ROM should reset it after reading.

---

## 6. Audio

### 6.1 Ring Buffer Mechanics
If `audio_size > 0`, the Host must allocate a ring buffer of that size starting at:
```
audio_buffer_start = 512 + signal_count + vram_size
```

The ROM writes audio samples into the ring buffer and advances `audio_write` (offset 148).
The Host reads samples from the ring buffer and advances `audio_read` (offset 152).

Both pointers are **byte offsets** into the audio buffer (0 to `audio_size - 1`). They wrap around independently.

**How to read available samples (Host):**
```pseudocode
write_ptr = read_u32(mem, 148)   // ROM's write position
read_ptr  = read_u32(mem, 152)   // Host's read position
buf_size  = read_u32(mem, 144)

if write_ptr >= read_ptr:
    available_bytes = write_ptr - read_ptr
else:
    available_bytes = buf_size - read_ptr + write_ptr
```

**How to read one sample per channel (Host):**
```pseudocode
audio_bpp      = read_u32(mem, 160)
audio_channels = read_u32(mem, 164)

for each output_sample:
    for ch in 0..audio_channels:
        raw_bytes = mem[audio_buf_start + read_ptr : + audio_bpp]

        if audio_bpp == 1:  // U8
            sample = (raw_bytes[0] - 128) / 128.0
        elif audio_bpp == 2:  // S16LE
            sample = int16_from_le(raw_bytes) / 32768.0
        elif audio_bpp == 4:  // F32LE
            sample = float32_from_le(raw_bytes)

        output_channel[ch][i] = sample
        read_ptr = (read_ptr + audio_bpp) % buf_size

write_u32(mem, 152, read_ptr)
```

**How the ROM writes (for ROM authors reference):**
```c
// Interleaved stereo S16LE example
int16_t left  = ...;
int16_t right = ...;
int write = sys->audio_write;
int size  = sys->audio_size;

((int16_t*)(audio_buf))[write/2] = left;
write = (write + 2) % size;
((int16_t*)(audio_buf))[write/2] = right;
write = (write + 2) % size;

sys->audio_write = write;
```

### 6.2 Format Details
| `audio_bpp` | Format | Range |
|-------------|--------|-------|
| 1 | Unsigned 8-bit PCM | 0..255 (silence = 128) |
| 2 | Signed 16-bit PCM, Little-Endian | -32768..32767 |
| 4 | IEEE 754 Float 32-bit, Little-Endian | -1.0..1.0 |

Samples are **interleaved** by channel. For stereo: L, R, L, R, ...

---

## 7. Complete Host Implementation Loop

The following pseudocode shows the full lifecycle of a minimal but complete Wagnostic Host:

```pseudocode
// --- Startup ---
wasm_bytes = load_file("rom.wasm")
mem = wasm_memory_buffer()

// Provide imports
imports = {
    env.get_ticks: function() -> milliseconds_since_start()
}

instance = wasm_instantiate(wasm_bytes, imports)

// 1. Initialize ROM metadata
instance.export["winit"]()

// 2. Host reads metadata from Header to setup system
title      = read_cstring(mem, 0)
width      = read_u32(mem, 128)
height     = read_u32(mem, 132)
bpp        = read_u32(mem, 136)
scale      = read_u32(mem, 140)
audio_size = read_u32(mem, 144)

open_window(title, width * scale, height * scale)
if audio_size > 0:
    init_audio(read_u32(mem, 156), read_u32(mem, 160), read_u32(mem, 164))

frame_fn = instance.export["wupdate"]

vram_size   = read_u32(mem, 128) * read_u32(mem, 132) * (read_u32(mem, 136) / 8)
sc          = read_u32(mem, 168)
audio_start = 512 + sc + vram_size
audio_size  = read_u32(mem, 144)

// --- Main Loop ---
while window_open:
    // 1. Process OS events
    for event in os_events():
        if event is keyboard:
            mem[192 + event.scancode] = event.down ? 1 : 0
        if event is gamepad:
            update_gamepad_bitmask(mem, 172, event)
        if event is mouse_move:
            write_i32(mem, 448, event.x)
            write_i32(mem, 452, event.y)
        if event is mouse_button:
            update_mouse_bitmask(mem, 456, event)
        if event is mouse_wheel:
            write_i32(mem, 460, event.delta)

    // 2. Call ROM
    frame_fn()

    // 3. Process Signals
    signal_count = read_u32(mem, 168)
    signals = mem + 512
    for i from 0 to signal_count - 1:
        sig = signals[i]
        if sig == 1: // REDRAW
            blit(mem, 512 + signal_count, vram_size, width, height, bpp)
        if sig == 2: // QUIT
            exit()
        // ... handle other signals (log, title update)
        signals[i] = 0 // Clear signal
    
    write_u32(mem, 460, 0)  // reset mouse wheel

    // 4. Feed audio
    if audio_size > 0:
        available = audio_available(mem, 144, 148, 152)
        if available > 0:
            samples = read_audio_samples(mem, audio_start, audio_size, ...)
            audio_device_write(samples)
```

---

## 8. Checklist for a Conforming Host

- [ ] Instantiates the WASM module with the correct `env` imports.
- [ ] Calls `winit` once and reads configuration fields from the header.
- [ ] Provides `env.get_ticks` returning monotonic milliseconds.
- [ ] Calls `wupdate` every frame.
- [ ] Writes keyboard state at offsets 192–447 (one byte per scancode).
- [ ] Writes gamepad bitmask at offset 172.
- [ ] Writes joystick axes at offsets 176–191 (int32, -32767..32767).
- [ ] Writes mouse position, buttons, and wheel at offsets 448–463.
- [ ] Reads `signal_count` at offset 168.
- [ ] Processes the signal buffer starting at offset 512.
- [ ] Blits VRAM (starting at 512 + `signal_count`) when signal `1` is received.
- [ ] Clears processed signals in the buffer (writes 0).
- [ ] Reads audio ring buffer using `audio_read`/`audio_write` pointers.
- [ ] Does NOT write to ROM-owned fields (`message`, `width`, `height`, `bpp`, `audio_write`, `signal_count`).
