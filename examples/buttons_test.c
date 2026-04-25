typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

extern unsigned char __heap_base;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t ram;
    uint32_t vram;
    uint32_t redraw;

    uint32_t gamepad_buttons; 
    int32_t  joystick_lx;
    int32_t  joystick_ly;
    int32_t  joystick_rx;
    int32_t  joystick_ry;
    uint8_t  keys[256];
} SystemConfig;

#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

static SystemConfig* _sys;
static uint16_t* _fb;

void draw_rect(int x, int y, int w, int h, uint16_t color) {
    for (int iy = y; iy < y + h; iy++) {
        for (int ix = x; ix < x + w; ix++) {
            if (ix >= 0 && ix < _sys->width && iy >= 0 && iy < _sys->height) {
                _fb[iy * _sys->width + ix] = color;
            }
        }
    }
}


void papagaio_init(void) {
    _sys = (SystemConfig*)0;

    _sys->width  = 320;
    _sys->height = 240;
    _sys->vram   = 320 * 240 * 2;
    _sys->ram    = 1024 * 512;
    _sys->redraw = 0;

    _fb = (uint16_t*)(sizeof(SystemConfig));
}

void papagaio_update(void) {
    // Fill screen with background
    for (int i = 0; i < _sys->width * _sys->height; i++) _fb[i] = RGB565(51, 51, 51); // dark grey
    
    // Grid settings
    int cols = 16;
    int rows = 16;
    int cell_w = 16;
    int cell_h = 10;
    int margin_x = (_sys->width - (cols * cell_w)) / 2;
    int margin_y = (_sys->height - (rows * cell_h)) / 2;

    for (int i = 0; i < 256; i++) {
        int cx = i % cols;
        int cy = i / cols;
        
        int px = margin_x + cx * cell_w;
        int py = margin_y + cy * cell_h;
        
        uint16_t col = RGB565(119, 119, 119); // Unpressed color (lighter grey)
        
        // Highlight square if key is held down!
        if (_sys->keys[i] == 1) {
            col = RGB565(0, 204, 85); // Bright Green
        }
        
        // Draw physical cell with a 1px border visually
        draw_rect(px, py, cell_w - 1, cell_h - 1, col);
    }

    _sys->redraw = 1;
}