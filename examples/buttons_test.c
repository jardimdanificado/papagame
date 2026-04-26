
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

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
    uint8_t  reserved[52];
} SystemConfig;
#pragma pack(pop)

#define _sys ((volatile SystemConfig*)0)
#define _fb  ((volatile uint16_t*)296)

#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

void draw_rect(int x, int y, int w, int h, uint16_t color) {
    for (int iy = y; iy < y + h; iy++) {
        for (int ix = x; ix < x + w; ix++) {
            if (ix >= 0 && ix < (int)_sys->width && iy >= 0 && iy < (int)_sys->height) {
                _fb[iy * _sys->width + ix] = color;
            }
        }
    }
}

__attribute__((visibility("default")))
int main() {
    if (_sys->width == 0) {
        init("Wagnostic - Buttons & Joysticks Test", 320, 240, 16, 4, 0, 0, 0, 2);
    }

    // Fill screen with background
    for (int i = 0; i < (int)(_sys->width * _sys->height); i++) _fb[i] = RGB565(51, 51, 51);
    
    int cols = 16, rows = 16, cell_w = 16, cell_h = 10;
    int margin_x = (_sys->width - (cols * cell_w)) / 2;
    int margin_y = (_sys->height - (rows * cell_h)) / 2;

    for (int i = 0; i < 256; i++) {
        int cx = i % cols, cy = i / cols;
        int px = margin_x + cx * cell_w, py = margin_y + cy * cell_h;
        
        uint16_t col = RGB565(119, 119, 119);
        if (_sys->keys[i]) col = RGB565(0, 204, 85);
        
        draw_rect(px, py, cell_w - 1, cell_h - 1, col);
    }

    _sys->redraw = 1;
    return 0;
}