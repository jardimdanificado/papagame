
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

#include "audio_data.h"

extern void init(const char* title, int w, int h, int bpp, int scale, int audio_size, int audio_rate, int audio_bpp, int audio_channels);

#pragma pack(push, 1)
typedef struct {
    char     title[128];
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t scale;
    uint32_t audio_size;
    uint32_t audio_write_ptr;
    uint32_t audio_read_ptr;
    uint32_t audio_sample_rate;
    uint32_t audio_bpp;
    uint32_t audio_channels;
    uint32_t redraw;
    uint32_t gamepad_buttons;
    int32_t  joystick_lx, joystick_ly, joystick_rx, joystick_ry;
    uint8_t  keys[256];
    int32_t  mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t  mouse_wheel;
    uint8_t  reserved[48];
} SystemConfig;
#pragma pack(pop)

#define _sys ((volatile SystemConfig*)0)

#define AUDIO_RING_SIZE 32768
static uint32_t music_pos = 0;

__attribute__((visibility("default")))
int main() {
    if (_sys->width == 0) {
        init("Wagnostic - Audio Player", 320, 240, 16, 4, AUDIO_RING_SIZE, music_rate, music_bpp, music_channels);
    }

    // Fill ring buffer with music data
    uint8_t* mem = (uint8_t*)0;
    uint32_t vram_size = _sys->width * _sys->height * (_sys->bpp / 8);
    uint8_t* audio_ring = mem + 512 + vram_size;

    uint32_t r = _sys->audio_read_ptr;
    uint32_t w = _sys->audio_write_ptr;
    uint32_t size = _sys->audio_size;

    // Calculate free space
    int free_space = (r > w) ? (r - w - 1) : (size - w + r - 1);
    
    if (free_space > 0 && music_pos < music_size) {
        int to_copy = (music_size - music_pos);
        if (to_copy > free_space) to_copy = free_space;

        for (int i = 0; i < to_copy; i++) {
            audio_ring[w] = music_data[music_pos++];
            w = (w + 1) % size;
        }
        _sys->audio_write_ptr = w;
    }

    // Simple visualization (optional)
    uint16_t* fb = (uint16_t*)(mem + 512);
    for (int i = 0; i < 320 * 240; i++) fb[i] = 0;
    
    // Draw progress bar
    int progress = (music_pos * 300) / music_size;
    for (int x = 10; x < 10 + progress; x++) {
        for (int y = 110; y < 130; y++) {
            fb[y * 320 + x] = 0x07E0; // Green
        }
    }

    _sys->redraw = 1;
    return 0;
}
