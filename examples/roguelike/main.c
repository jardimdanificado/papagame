
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;
typedef int            bool;
#define true 1
#define false 0

#include "data/sprites.h"

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

#define BTN_UP     (1 << 0)
#define BTN_DOWN   (1 << 1)
#define BTN_LEFT   (1 << 2)
#define BTN_RIGHT  (1 << 3)
#define BTN_A      (1 << 4)
#define BTN_B      (1 << 5)
#define BTN_START  (1 << 10)
#define BTN_SELECT (1 << 11)

#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

#define sheet_ptr ((const uint16_t*)image_raw)

// ============================================
// ROGUELIKE DEFINITIONS
// ============================================

#define SPRITE_FLOOR   10
#define SPRITE_WALL    25
#define SPRITE_PLAYER  1
#define SPRITE_MONSTER 45

#define MAP_W 30
#define MAP_H 21

#define VIEW_W 10
#define VIEW_H 7

static uint8_t map[MAP_H][MAP_W];

typedef struct {
    int x, y;
    int hp;
} Entity;

static Entity player;

#define MAX_MONSTERS 10
static Entity monsters[MAX_MONSTERS];
static int num_monsters = 0;

static uint32_t _seed = 12345;
static uint32_t rand_u32() {
    _seed = _seed * 1103515245 + 12345;
    return (_seed / 65536) % 32768;
}

static int rand_range(int min, int max) {
    if (max <= min) return min;
    return min + (rand_u32() % (max - min));
}

#pragma pack(push, 1)
typedef struct { int x, y, w, h; } Room;
#pragma pack(pop)
static Room rooms[15];
static int num_rooms = 0;

static void build_room(Room *r) {
    for (int y = r->y; y < r->y + r->h; y++) {
        for (int x = r->x; x < r->x + r->w; x++) {
            if (x >= 0 && x < MAP_W && y >= 0 && y < MAP_H) map[y][x] = 0; 
        }
    }
}

static void generate_map() {
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            map[y][x] = 1; 

    num_rooms = 0;
    for (int i = 0; i < 10; i++) {
        Room r;
        r.w = rand_range(4, 8);
        r.h = rand_range(4, 6);
        r.x = rand_range(1, MAP_W - r.w - 1);
        r.y = rand_range(1, MAP_H - r.h - 1);

        bool overlap = false;
        for (int j = 0; j < num_rooms; j++) {
            if (r.x < rooms[j].x + rooms[j].w + 1 && r.x + r.w + 1 > rooms[j].x &&
                r.y < rooms[j].y + rooms[j].h + 1 && r.y + r.h + 1 > rooms[j].y) {
                overlap = true;
                break;
            }
        }
        if (!overlap) {
            build_room(&r);
            if (num_rooms > 0) {
                int cx1 = rooms[num_rooms-1].x + rooms[num_rooms-1].w/2;
                int cy1 = rooms[num_rooms-1].y + rooms[num_rooms-1].h/2;
                int cx2 = r.x + r.w/2;
                int cy2 = r.y + r.h/2;
                for (int x = (cx1 < cx2 ? cx1 : cx2); x <= (cx1 < cx2 ? cx2 : cx1); x++) map[cy1][x] = 0;
                for (int y = (cy1 < cy2 ? cy1 : cy2); y <= (cy1 < cy2 ? cy2 : cy1); y++) map[y][cx2] = 0;
            }
            rooms[num_rooms++] = r;
        }
    }

    player.x = rooms[0].x + rooms[0].w / 2;
    player.y = rooms[0].y + rooms[0].h / 2;
    player.hp = 10;

    num_monsters = 0;
    for (int i = 1; i < num_rooms; i++) {
        if (num_monsters < MAX_MONSTERS) {
            monsters[num_monsters].x = rooms[i].x + rooms[i].w / 2;
            monsters[num_monsters].y = rooms[i].y + rooms[i].h / 2;
            monsters[num_monsters].hp = 3;
            num_monsters++;
        }
    }
}

static void draw_sprite(int x, int y, int id) {
    int sx = (id % 16) * 16;
    int sy = (id / 16) * 16;
    for (int j = 0; j < 16; j++) {
        for (int i = 0; i < 16; i++) {
            uint16_t color = sheet_ptr[(sy + j) * 256 + (sx + i)];
            if (color != 0xF81F) { 
                _fb[(y + j) * 320 + (x + i)] = color;
            }
        }
    }
}

typedef enum { STATE_PLAYING, STATE_GAMEOVER } GameState;
static GameState game_state = STATE_PLAYING;
static uint32_t prev_buttons = 0;

static void reset_game() {
    game_state = STATE_PLAYING;
    generate_map();
}

__attribute__((visibility("default")))
int main() {
    if (_sys->width == 0) {
        init("Wagnostic - Roguelike Example", 320, 240, 16, 1, 0, 0, 0, 2);
        generate_map();
    }

    if (game_state == STATE_GAMEOVER) {
        for (int i=0; i<(int)(_sys->width*_sys->height); i++) _fb[i] = RGB565(255, 0, 0);
        uint32_t pressed = _sys->gamepad_buttons & ~prev_buttons;
        prev_buttons = _sys->gamepad_buttons;
        if (pressed & BTN_START) reset_game(); 
        _sys->redraw = 1;
        return 0;
    }

    uint32_t pressed = _sys->gamepad_buttons & ~prev_buttons;
    prev_buttons = _sys->gamepad_buttons;

    int dx = 0, dy = 0;
    if (pressed & BTN_LEFT)  dx = -1;
    if (pressed & BTN_RIGHT) dx = 1;
    if (pressed & BTN_UP)    dy = -1;
    if (pressed & BTN_DOWN)  dy = 1;

    if (dx != 0 || dy != 0) {
        int nx = player.x + dx;
        int ny = player.y + dy;
        if (nx >= 0 && nx < MAP_W && ny >= 0 && ny < MAP_H && map[ny][nx] == 0) {
            bool monster_there = false;
            for (int i = 0; i < num_monsters; i++) {
                if (monsters[i].hp > 0 && monsters[i].x == nx && monsters[i].y == ny) {
                    monsters[i].hp--;
                    monster_there = true;
                    break;
                }
            }
            if (!monster_there) {
                player.x = nx;
                player.y = ny;
            }
            
            for (int i = 0; i < num_monsters; i++) {
                if (monsters[i].hp > 0) {
                    int mdx = (player.x > monsters[i].x) ? 1 : (player.x < monsters[i].x ? -1 : 0);
                    int mdy = (player.y > monsters[i].y) ? 1 : (player.y < monsters[i].y ? -1 : 0);
                    int mnx = monsters[i].x + mdx;
                    int mny = monsters[i].y + mdy;
                    if (mnx == player.x && mny == player.y) {
                        player.hp--;
                        if (player.hp <= 0) game_state = STATE_GAMEOVER;
                    } else if (map[mny][mnx] == 0) {
                        monsters[i].x = mnx;
                        monsters[i].y = mny;
                    }
                }
            }
        }
    }

    for (int i = 0; i < (int)(_sys->width * _sys->height); i++) _fb[i] = RGB565(10, 10, 15);

    int off_x = (320 - MAP_W * 8) / 2;
    int off_y = (240 - MAP_H * 8) / 2;

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            int id = (map[y][x] == 1) ? SPRITE_WALL : SPRITE_FLOOR;
            
            int sx = (id % 16) * 16;
            int sy = (id / 16) * 16;
            for (int j = 0; j < 8; j++) {
                for (int i = 0; i < 8; i++) {
                    uint16_t color = sheet_ptr[(sy + j*2) * 256 + (sx + i*2)];
                    _fb[(off_y + y*8 + j) * 320 + (off_x + x*8 + i)] = color;
                }
            }
        }
    }

    int px = off_x + player.x * 8;
    int py = off_y + player.y * 8;
    int id = SPRITE_PLAYER;
    int sx = (id % 16) * 16;
    int sy = (id / 16) * 16;
    for (int j = 0; j < 8; j++) {
        for (int i = 0; i < 8; i++) {
            uint16_t color = sheet_ptr[(sy + j*2) * 256 + (sx + i*2)];
            if (color != 0xF81F) _fb[(py + j) * 320 + (px + i)] = color;
        }
    }

    for (int m = 0; m < num_monsters; m++) {
        if (monsters[m].hp > 0) {
            int mx = off_x + monsters[m].x * 8;
            int my = off_y + monsters[m].y * 8;
            id = SPRITE_MONSTER;
            sx = (id % 16) * 16;
            sy = (id / 16) * 16;
            for (int j = 0; j < 8; j++) {
                for (int i = 0; i < 8; i++) {
                    uint16_t color = sheet_ptr[(sy + j*2) * 256 + (sx + i*2)];
                    if (color != 0xF81F) _fb[(my + j) * 320 + (mx + i)] = color;
                }
            }
        }
    }

    _sys->redraw = 1;
    return 0;
}
