#ifndef WAGNOSTD_H
#define WAGNOSTD_H

#include <stdint.h>
#include <stdbool.h>

// ============================================
// SYSTEM HARDWARE CONSTANTS
// ============================================

#define WAGNOSTIC_HEADER_SIZE 512

#pragma pack(push, 1)
typedef struct {
    char     message[128];      // Offset 0: Shared text buffer
    uint32_t width;             // Offset 128
    uint32_t height;            // Offset 132
    uint32_t bpp;               // Offset 136
    uint32_t scale;              // Offset 140
    uint32_t audio_size;         // Offset 144
    uint32_t audio_write_ptr;    // Offset 148
    uint32_t audio_read_ptr;     // Offset 152
    uint32_t audio_sample_rate;  // Offset 156
    uint32_t audio_bpp;          // Offset 160
    uint32_t audio_channels;     // Offset 164
    uint32_t signal_count;       // Offset 168 (The Hardware Signal Buffer size)
    
    uint32_t gamepad_buttons;    // Offset 172
    int32_t  joystick_lx, joystick_ly, joystick_rx, joystick_ry; // Offset 176
    uint8_t  keys[256];          // Offset 192
    
    int32_t  mouse_x;            // Offset 448
    int32_t  mouse_y;            // Offset 452
    uint32_t mouse_buttons;      // Offset 456
    int32_t  mouse_wheel;        // Offset 460
    uint8_t  reserved[48];       // Offset 464
} Wagnostic_System;
#pragma pack(pop)

// Pointer to System Config (Mapped at address 0)
#define W_SYS ((volatile Wagnostic_System*)0)

// Helper Macros
#define W_SIGNALS ((volatile uint8_t*)WAGNOSTIC_HEADER_SIZE)
#define W_FB_PTR  ((void*)(WAGNOSTIC_HEADER_SIZE + W_SYS->signal_count))

// Dynamic Audio Pointer calculation
static inline void* w_audio_ptr() {
    uint32_t fb_size = W_SYS->width * W_SYS->height * (W_SYS->bpp / 8);
    return (void*)(WAGNOSTIC_HEADER_SIZE + W_SYS->signal_count + fb_size);
}

// Signals Opcodes
#define W_SIG_REDRAW        1
#define W_SIG_QUIT          2
#define W_SIG_UPDATE_TITLE  3
#define W_SIG_UPDATE_WINDOW 4
#define W_SIG_UPDATE_AUDIO  5
#define W_SIG_LOG_INFO      6
#define W_SIG_LOG_WARN      7
#define W_SIG_LOG_ERR       8

// Gamepad Buttons
#define W_BTN_UP     (1 << 0)
#define W_BTN_DOWN   (1 << 1)
#define W_BTN_LEFT   (1 << 2)
#define W_BTN_RIGHT  (1 << 3)
#define W_BTN_A      (1 << 4)
#define W_BTN_B      (1 << 5)
#define W_BTN_X      (1 << 6)
#define W_BTN_Y      (1 << 7)
#define W_BTN_L1     (1 << 8)
#define W_BTN_R1     (1 << 9)
#define W_BTN_START  (1 << 10)
#define W_BTN_SELECT (1 << 11)
#define W_BTN_L2     (1 << 12)
#define W_BTN_R2     (1 << 13)

// Convenience setup
static inline void w_setup(const char* title, int w, int h, int bpp, int scale, int signals) {
    W_SYS->width = w;
    W_SYS->height = h;
    W_SYS->bpp = bpp;
    W_SYS->scale = scale;
    W_SYS->signal_count = signals;
    
    // Set title
    if (title) {
        int i = 0;
        while (title[i] && i < 127) {
            ((char*)W_SYS->message)[i] = title[i];
            i++;
        }
        ((char*)W_SYS->message)[i] = '\0';
        if (signals > 1) W_SIGNALS[1] = W_SIG_UPDATE_TITLE;
    }
}

static inline void w_redraw() {
    W_SIGNALS[0] = W_SIG_REDRAW;
}

// Color conversion macros
#define W_RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define W_RGBA(r, g, b, a) (uint32_t)(((a) << 24) | ((b) << 16) | ((g) << 8) | (r))
#define W_RGB332(r, g, b) (uint8_t)(((r) & 0xE0) | (((g) & 0xE0) >> 3) | (((b) & 0xC0) >> 6))

#endif
