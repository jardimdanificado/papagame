
#define OLIVEC_IMPLEMENTATION
#include "olive.c"
#include "image_data.h"

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

extern void init(const char* title, int w, int h, int bpp, int scale, int audio_size, int audio_rate, int audio_bpp, int audio_channels);
extern uint32_t get_ticks();

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
#define _fb ((volatile uint16_t*)512)

#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

typedef struct {
    float x, y, vx, vy;
    int w, h;
} Sprite;

#define SPRITE_COUNT 200

static Olivec_Canvas _oc;
static Olivec_Canvas _img_sprite;
static Sprite _sprites[SPRITE_COUNT];

static uint32_t _last_time = 0;
static int _frame_count = 0;
static int _fps = 0;
static char _fps_text[32] = "fps: --";

static uint32_t _seed = 12345;
static uint32_t rand_u32() {
    _seed = _seed * 1103515245 + 12345;
    return (_seed / 65536) % 32768;
}

static void int_to_str(int n, char* str) {
    if (n == 0) { str[0] = '0'; str[1] = '\0'; return; }
    int i = 0;
    while (n > 0) {
        str[i++] = (n % 10) + '0';
        n /= 10;
    }
    str[i] = '\0';
    for (int j = 0; j < i / 2; j++) {
        char t = str[j]; str[j] = str[i - 1 - j]; str[i - 1 - j] = t;
    }
}

__attribute__((visibility("default")))
int main() {
    if (_sys->width == 0) {
        init("Wagnostic - Draw Example", 320, 240, 16, 4, 0, 0, 0, 2);

        _oc = olivec_canvas((uint16_t*)_fb, 320, 240, 320);
        _img_sprite = olivec_canvas((uint16_t*)image_raw, image_width, image_height, image_width);

        for (int i = 0; i < SPRITE_COUNT; i++) {
            _sprites[i].w = 24;
            _sprites[i].h = 24;
            _sprites[i].x = (float)(rand_u32() % (320 - 24));
            _sprites[i].y = (float)(rand_u32() % (240 - 24));
            _sprites[i].vx = (float)((rand_u32() % 4) + 1);
            _sprites[i].vy = (float)((rand_u32() % 4) + 1);
            if (rand_u32() % 2) _sprites[i].vx *= -1;
            if (rand_u32() % 2) _sprites[i].vy *= -1;
        }
        _last_time = get_ticks();
    }

    uint32_t now = get_ticks();
    _frame_count++;
    if (now - _last_time >= 1000) {
        _fps = _frame_count;
        _frame_count = 0;
        _last_time = now;
        char num[16];
        int_to_str(_fps, num);
        _fps_text[0] = 'f'; _fps_text[1] = 'p'; _fps_text[2] = 's'; _fps_text[3] = ':'; _fps_text[4] = ' ';
        int j = 0; while(num[j]) { _fps_text[5+j] = num[j]; j++; }
        _fps_text[5+j] = '\0';
    }

    olivec_fill(_oc, 0);

    for (int i = 0; i < SPRITE_COUNT; i++) {
        _sprites[i].x += _sprites[i].vx;
        _sprites[i].y += _sprites[i].vy;
        if (_sprites[i].x <= 0 || _sprites[i].x + _sprites[i].w >= 320) _sprites[i].vx *= -1;
        if (_sprites[i].y <= 0 || _sprites[i].y + _sprites[i].h >= 240) _sprites[i].vy *= -1;
        olivec_sprite_copy(_oc, (int)_sprites[i].x, (int)_sprites[i].y, _sprites[i].w, _sprites[i].h, _img_sprite);
    }

    olivec_rect(_oc, 5, 220, 80, 15, RGB565(0, 0, 0));
    olivec_text(_oc, _fps_text, 10, 222, olivec_default_font, 1, RGB565(255, 255, 255));
    
    _sys->redraw = 1;
    return 0;
}
