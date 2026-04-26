
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
    uint8_t  reserved[48];
} SystemConfig;
#pragma pack(pop)

#define _sys ((volatile SystemConfig*)0)
#define _fb  ((uint16_t*)512)

#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

int main() {
    if (_sys->width == 0) {
        init("Wagnostic - 16bpp RGB565 Mode", 320, 240, 16, 1, 0, 0, 0, 2);
    }

    // Fill background
    for (int i = 0; i < 320 * 240; i++) _fb[i] = RGB565(20, 40, 60);

    int x = _sys->mouse_x, y = _sys->mouse_y;
    uint16_t color = RGB565(0, 255, 0);
    if (_sys->mouse_buttons) color = RGB565(255, 255, 255);

    for(int iy = -10; iy < 10; iy++) {
        for(int ix = -10; ix < 10; ix++) {
            int px = x + ix, py = y + iy;
            if(px >= 0 && px < 320 && py >= 0 && py < 240) {
                _fb[py * 320 + px] = color;
            }
        }
    }

    _sys->redraw = 1;
    return 0;
}
