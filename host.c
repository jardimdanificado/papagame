
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>

#include "wasm3.h"
#include "m3_env.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t ram;         // bytes
    uint32_t vram;        // bytes
    uint32_t ram_ptr;     // offset
    uint32_t vram_ptr;    // offset
    uint32_t pal_ptr;     // offset
    uint32_t dirty_ptr;   // offset
} SystemConfig;

#define MAX_DIRTY 32

typedef struct {
    uint16_t x, y, w, h;
    uint32_t flags;
} DirtyRect;

typedef struct {
    uint32_t count;
    DirtyRect rects[MAX_DIRTY];
} DirtyList;

uint32_t get_wasm_addr(IM3Module module, const char* name) {
    IM3Global g = m3_FindGlobal(module, name);
    if (!g) return 0;
    M3TaggedValue val;
    m3_GetGlobal(g, &val);
    return val.value.i32;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: %s game.wasm\n", argv[0]);
        return 1;
    }

    SDL_Init(SDL_INIT_VIDEO);

    IM3Environment env = m3_NewEnvironment();
    IM3Runtime runtime = m3_NewRuntime(env, 4 * 1024 * 1024, NULL);

    FILE* f = fopen(argv[1], "rb");
    if (!f) { printf("error opening %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    uint8_t* wasm = malloc(size);
    fread(wasm, 1, size, f);
    fclose(f);

    IM3Module module;
    m3_ParseModule(env, &module, wasm, size);
    m3_LoadModule(runtime, module);

    IM3Function init, update;
    m3_FindFunction(&init, runtime, "papagaio_init");
    m3_FindFunction(&update, runtime, "papagaio_update");

    m3_CallV(init);

    uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
    
    // Magic Symbolic Discovery
    uint32_t sys_addr = get_wasm_addr(module, "papagaio_sys");
    uint32_t pal_addr = get_wasm_addr(module, "papagaio_pal");
    uint32_t dirty_addr = get_wasm_addr(module, "papagaio_dirty");
    uint32_t fb_ptr_addr = get_wasm_addr(module, "papagaio_fb");
    uint32_t heap_base = get_wasm_addr(module, "__heap_base");

    SystemConfig* sys = (SystemConfig*)(mem + sys_addr);
    
    // Automatic System Initialization
    uint32_t base = (heap_base + 65535) & ~65535;
    sys->vram_ptr = base;
    sys->ram_ptr = base + sys->vram;
    sys->pal_ptr = pal_addr;
    sys->dirty_ptr = dirty_addr;

    // Inject Framebuffer pointer into the game
    if (fb_ptr_addr) {
        *(uint32_t*)(mem + fb_ptr_addr) = sys->vram_ptr;
    }

    // Host handles the memory grow directly using Wasm3 internal API
    uint32_t total_needed = sys->ram_ptr + sys->ram;
    uint32_t required_pages = (total_needed + 65535) / 65536;
    
    // Use native ResizeMemory instead of client-side grow
    uint32_t current_pages = m3_GetMemorySize(runtime) / 65536;
    if (required_pages > current_pages) {
        ResizeMemory(runtime, required_pages);
    }

    // Refresh memory pointer
    mem = m3_GetMemory(runtime, NULL, 0);
    sys = (SystemConfig*)(mem + sys_addr);
    uint32_t* pal = (uint32_t*)(mem + sys->pal_ptr);
    uint8_t* fb = mem + sys->vram_ptr;
    DirtyList* dirty = (DirtyList*)(mem + sys->dirty_ptr);

    SDL_Window* window = SDL_CreateWindow(
        "wasm engine",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        sys->width * 4, sys->height * 4, 0
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture* texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        sys->width, sys->height
    );

    uint32_t* out = malloc(sys->width * sys->height * sizeof(uint32_t));
    int running = 1;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) running = 0; }
        m3_CallV(update);

        if (dirty->count == 0) {
            for (int i = 0; i < sys->width * sys->height; i++) out[i] = pal[fb[i]];
            SDL_UpdateTexture(texture, NULL, out, sys->width * sizeof(uint32_t));
        } else {
            for (uint32_t r = 0; r < dirty->count && r < MAX_DIRTY; r++) {
                DirtyRect* d = &dirty->rects[r];
                for (int y = d->y; y < d->y + d->h; y++) {
                    for (int x = d->x; x < d->x + d->w; x++) {
                        int i = y * sys->width + x;
                        out[i] = pal[fb[i]];
                    }
                }
                SDL_Rect rect = { d->x, d->y, d->w, d->h };
                SDL_UpdateTexture(texture, &rect, &out[d->y * sys->width + d->x], sys->width * sizeof(uint32_t));
            }
        }
        dirty->count = 0;
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    SDL_Quit();
    return 0;
}
