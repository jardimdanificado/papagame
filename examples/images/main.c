
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

#include "image_data.h"

extern unsigned char __heap_base;

typedef struct {
    uint32_t width;       // pixels
    uint32_t height;      // pixels
    uint32_t ram;         // bytes
    uint32_t vram;        // bytes
    uint32_t redraw;      // set to 1 when fb was written this frame

    // ---- Gamepad / Inputs Mapped ----
    uint32_t gamepad_buttons; 
    int32_t  joystick_lx;     
    int32_t  joystick_ly;
    int32_t  joystick_rx;     
    int32_t  joystick_ry;
    uint8_t  keys[256];
} SystemConfig;

#define BTN_UP     (1 << 0)
#define BTN_DOWN   (1 << 1)
#define BTN_LEFT   (1 << 2)
#define BTN_RIGHT  (1 << 3)
#define BTN_A      (1 << 4)
#define BTN_B      (1 << 5)
#define BTN_X      (1 << 6)
#define BTN_Y      (1 << 7)
#define BTN_L1     (1 << 8)
#define BTN_R1     (1 << 9)
#define BTN_START  (1 << 10)
#define BTN_SELECT (1 << 11)
#define BTN_L2     (1 << 12)
#define BTN_R2     (1 << 13)
#define BTN_L3     (1 << 14)
#define BTN_R3     (1 << 15)

static SystemConfig* _sys;
static uint16_t* _fb;

static const uint16_t* pixels_ptr = (const uint16_t*)image_raw;

// Desenha a imagem esticando/escalonando para o tamanho destino (dest_w, dest_h)
void draw_image_scaled(int dest_x, int dest_y, int dest_w, int dest_h) {
    if (dest_w <= 0 || dest_h <= 0) return;

    for (int j = 0; j < dest_h; j++) {
        int screen_y = dest_y + j;
        if (screen_y < 0 || screen_y >= (int)_sys->height) continue;

        // Calcula a linha da imagem original uma vez por linha de tela
        int v = (j * image_height) / dest_h;
        const uint16_t* src_row = &pixels_ptr[v * image_width];
        uint16_t* dst_row = &_fb[screen_y * _sys->width];

        for (int i = 0; i < dest_w; i++) {
            int screen_x = dest_x + i;
            if (screen_x < 0 || screen_x >= (int)_sys->width) continue;

            int u = (i * image_width) / dest_w;
            dst_row[screen_x] = src_row[u];
        }
    }
}

void papagaio_init(void) {
    _sys = (SystemConfig*)0;

    _sys->width    = 320;
    _sys->height   = 240;
    _sys->ram      = 65536;
    _sys->vram     = 320 * 240 * 2;
    _sys->redraw   = 0;

    _fb = (uint16_t*)(sizeof(SystemConfig));
}

static int pos_x = 100;
static int pos_y = 70;
static int scale_w = 120;
static int scale_h = 100;

void papagaio_update(void) {
    // Limpa a tela (cor 0 da paleta ou 255)
    for (int i=0; i<_sys->width*_sys->height; i++) _fb[i] = 0;

    // Movimentação
    if (_sys->gamepad_buttons & BTN_LEFT)  pos_x -= 2;
    if (_sys->gamepad_buttons & BTN_RIGHT) pos_x += 2;
    if (_sys->gamepad_buttons & BTN_UP)    pos_y -= 2;
    if (_sys->gamepad_buttons & BTN_DOWN)  pos_y += 2;

    // Analógicos para posição fina ou escala
    pos_x += (_sys->joystick_lx / 5000);
    pos_y += (_sys->joystick_ly / 5000);

    // Botoes para mudar escala (L1/R1, L2/R2)
    if (_sys->gamepad_buttons & BTN_L1) scale_w -= 2;
    if (_sys->gamepad_buttons & BTN_R1) scale_w += 2;
    if (_sys->gamepad_buttons & BTN_L2) scale_h -= 2;
    if (_sys->gamepad_buttons & BTN_R2) scale_h += 2;

    // Botão A reseta escala
    if (_sys->gamepad_buttons & BTN_A) {
        scale_w = image_width;
        scale_h = image_height;
    }

    // Desenha a imagem estalando/esticando
    draw_image_scaled(pos_x, pos_y, scale_w, scale_h);

    _sys->redraw = 1;
}
