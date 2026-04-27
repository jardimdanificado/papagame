#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>

#if defined(_WIN32) && defined(_MSC_VER)
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
#endif

#include "wasm3.h"
#include "m3_env.h"

#pragma pack(push, 1)
typedef struct {
    char     message[128];      // Offset 0
    uint32_t width;             // Offset 128
    uint32_t height;            // Offset 132
    uint32_t bpp;               // Offset 136
    uint32_t scale;             // Offset 140
    
    // ---- Audio ----
    uint32_t audio_size;        // Offset 144
    uint32_t audio_write_ptr;   // Offset 148
    uint32_t audio_read_ptr;    // Offset 152
    uint32_t audio_sample_rate; // Offset 156
    uint32_t audio_bpp;         // Offset 160
    uint32_t audio_channels;    // Offset 164
    uint32_t signal_count;      // Offset 168
    
    // ---- Inputs ----
    uint32_t gamepad_buttons;   // Offset 172
    int32_t  joystick_lx;       // Offset 176
    int32_t  joystick_ly;
    int32_t  joystick_rx;
    int32_t  joystick_ry;
    uint8_t  keys[256];         // Offset 192
    
    // ---- Mouse ----
    int32_t  mouse_x;           // Offset 448
    int32_t  mouse_y;           // Offset 452
    uint32_t mouse_buttons;     // Offset 456
    int32_t  mouse_wheel;       // Offset 460

    uint8_t  reserved[48];      // Offset 464 (to reach 512)
} SystemConfig;
#pragma pack(pop)

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

// ---- GLSL 1.20 / GLES 100 shaders ----
static const char* VERT_SRC =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = a_uv;\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

static const char* FRAG_SRC =
    "#version 120\n"
    "uniform sampler2D u_fb;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_fb, v_uv);\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error:\n%s\n", log);
    }
    return s;
}

static GLuint build_program(void) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   VERT_SRC);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FRAG_SRC);
    GLuint p  = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glBindAttribLocation(p, 0, "a_pos");
    glBindAttribLocation(p, 1, "a_uv");
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "Program link error:\n%s\n", log);
    }
    return p;
}

static IM3Runtime runtime;
static uint32_t sys_wasm_offset = 0;
static SDL_Window* window = NULL;
static SDL_GLContext gl_ctx = NULL;
static GLuint fb_tex = 0;
static uint32_t W = 0, H = 0, BPP = 0, SCALE = 0;
static SDL_AudioDeviceID audio_dev = 0;

static void audio_callback(void* userdata, uint8_t* stream, int len) {
    uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
    if (!mem) return;
    SystemConfig* sys = (SystemConfig*)(mem + sys_wasm_offset);
    uint8_t* audio_buf = mem + 512 + sys->signal_count + (sys->width * sys->height * (sys->bpp / 8));

    for (int i = 0; i < len; i++) {
        if (sys->audio_read_ptr == sys->audio_write_ptr) {
            stream[i] = 0;
        } else {
            stream[i] = audio_buf[sys->audio_read_ptr];
            sys->audio_read_ptr = (sys->audio_read_ptr + 1) % sys->audio_size;
        }
    }
}

static void init_sdl_from_header() {
    uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
    if (!mem) return;
    SystemConfig* sys = (SystemConfig*)(mem + sys_wasm_offset);
    
    static bool first_init = true;
    W = sys->width; H = sys->height; BPP = sys->bpp;
    SCALE = sys->scale;
    if (SCALE == 0) SCALE = 1;

    if (first_init) {
        printf(">>> Wagnostic ROM Metadata: '%s' %ux%u@%ubpp (Scale: %u)\n", 
               sys->message, W, H, BPP, SCALE);
        first_init = false;
    }

    if (!window) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        window = SDL_CreateWindow(sys->message[0] ? sys->message : "Wagnostic", 
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                W * SCALE, H * SCALE, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!window) fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
    } else {
        SDL_SetWindowSize(window, W * SCALE, H * SCALE);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    glViewport(0, 0, W * SCALE, H * SCALE);

    if (sys->audio_size > 0) {
        if (audio_dev) SDL_CloseAudioDevice(audio_dev);
        SDL_AudioSpec wanted, have;
        SDL_zero(wanted);
        wanted.freq = sys->audio_sample_rate;
        if (sys->audio_bpp == 1) wanted.format = AUDIO_U8;
        else if (sys->audio_bpp == 4) wanted.format = AUDIO_F32SYS;
        else wanted.format = AUDIO_S16SYS;
        wanted.channels = sys->audio_channels;
        wanted.samples = 1024;
        wanted.callback = audio_callback;
        audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, &have, 0);
        if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);
    }
}

m3ApiRawFunction(host_get_ticks) {
    m3ApiReturnType(uint32_t);
    m3ApiReturn(SDL_GetTicks());
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: %s game.wasm\n", argv[0]); return 1; }

    IM3Environment env     = m3_NewEnvironment();
    runtime = m3_NewRuntime(env, 8 * 1024 * 1024, NULL); 
    
    FILE* f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); size_t wsize = ftell(f); rewind(f);
    uint8_t* wasm = malloc(wsize);
    fread(wasm, 1, wsize, f); fclose(f);

    IM3Module module;
    M3Result result = m3_ParseModule(env, &module, wasm, wsize);
    if (result) { fprintf(stderr, "m3_ParseModule error: %s\n", result); return 1; }
    result = m3_LoadModule(runtime, module);
    if (result) { fprintf(stderr, "m3_LoadModule error: %s\n", result); return 1; }

    uint32_t current_mem_size = 0;
    uint8_t* initial_mem = m3_GetMemory(runtime, &current_mem_size, 0);
    if (initial_mem && current_mem_size >= 1024) memset(initial_mem, 0, 1024);

    m3_LinkRawFunction(module, "env", "get_ticks", "i()", host_get_ticks);

    IM3Function fn_init = NULL;
    if (m3_FindFunction(&fn_init, runtime, "winit") != m3Err_none) {
        m3_FindFunction(&fn_init, runtime, "init");
    }
    
    if (fn_init) {
        result = m3_CallV(fn_init);
        if (result) { fprintf(stderr, "ROM INIT ERROR: %s\n", result); return 1; }
        printf("ROM Initialized successfully.\n");
    }

    uint8_t* mem_ptr = m3_GetMemory(runtime, NULL, 0);
    if (mem_ptr) {
        SystemConfig* sys = (SystemConfig*)(mem_ptr + sys_wasm_offset);
        if (sys->width == 0) sys->width = 320;
        if (sys->height == 0) sys->height = 240;
        if (sys->bpp == 0) sys->bpp = 8;
        if (sys->scale == 0) sys->scale = 1;
        if (sys->signal_count == 0) sys->signal_count = 4;
        if (sys->message[0] == '\0') {
            const char* def_title = "Wagnostic";
            for(int i=0; i<127 && def_title[i]; i++) sys->message[i] = def_title[i];
        }
    }

    IM3Function fn_frame = NULL;
    if (m3_FindFunction(&fn_frame, runtime, "wupdate") != m3Err_none) {
        m3_FindFunction(&fn_frame, runtime, "frame");
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return 1;
    init_sdl_from_header();
    if (!window) return 1;
    gl_ctx = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);

    GLuint prog = build_program();
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "u_fb"), 0);

    float quad[] = { -1.f, -1.f, 0.f, 1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f, 0.f, 0.f, 1.f, 1.f, 1.f, 0.f };
    GLuint vbo; glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);

    glGenTextures(1, &fb_tex); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, fb_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    int running = 1; SDL_Event event; uint32_t last_time = SDL_GetTicks();
    while (running) {
        uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
        if (!mem) { SDL_Delay(10); continue; }
        SystemConfig* sys = (SystemConfig*)(mem + sys_wasm_offset);
        uint8_t* signals = mem + 512;
        int should_redraw = 0;
        for (uint32_t i = 0; i < sys->signal_count; i++) {
            uint8_t sig = signals[i];
            if (sig == 1) should_redraw = 1;
            else if (sig == 2) running = 0;
            else if (sig == 3 || sig == 4 || sig == 5) init_sdl_from_header();
            else if (sig == 6) printf("INFO: %s\n", sys->message);
            signals[i] = 0;
        }
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                int down = (event.type == SDL_KEYDOWN);
                if (event.key.keysym.scancode < 256) sys->keys[event.key.keysym.scancode] = down ? 1 : 0;
                uint32_t bit = 0;
                switch (event.key.keysym.sym) {
                    case SDLK_UP: bit = BTN_UP; break; case SDLK_DOWN: bit = BTN_DOWN; break;
                    case SDLK_LEFT: bit = BTN_LEFT; break; case SDLK_RIGHT: bit = BTN_RIGHT; break;
                    case SDLK_z: bit = BTN_A; break; case SDLK_x: bit = BTN_B; break;
                    case SDLK_RETURN: bit = BTN_START; break; case SDLK_ESCAPE: bit = BTN_SELECT; break;
                }
                if (bit) { if (down) sys->gamepad_buttons |= bit; else sys->gamepad_buttons &= ~bit; }
            }
            if (event.type == SDL_MOUSEMOTION) {
                int ww, wh; SDL_GetWindowSize(window, &ww, &wh);
                if (ww > 0 && wh > 0) { sys->mouse_x = (event.motion.x * W) / ww; sys->mouse_y = (event.motion.y * H) / wh; }
            }
            if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                int down = (event.type == SDL_MOUSEBUTTONDOWN);
                uint32_t bit = (1 << (event.button.button - 1));
                if (down) sys->mouse_buttons |= bit; else sys->mouse_buttons &= ~bit;
            }
            if (event.type == SDL_MOUSEWHEEL) sys->mouse_wheel += event.wheel.y;
        }
        if (fn_frame) m3_CallV(fn_frame);
        mem = m3_GetMemory(runtime, NULL, 0);
        if (mem) {
            sys = (SystemConfig*)(mem + sys_wasm_offset);
            uint8_t* fb = mem + 512 + sys->signal_count;
            if (should_redraw) {
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, fb_tex);
                if (sys->bpp == 32) glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, fb);
                else {
                    static uint32_t* tmp = NULL; tmp = realloc(tmp, W * H * 4);
                    if (sys->bpp == 8) {
                        for(int i=0; i<W*H; i++) {
                            uint8_t c = fb[i];
                            uint8_t r = ((c >> 5) & 0x07) * 255 / 7, g = ((c >> 2) & 0x07) * 255 / 7, b = (c & 0x03) * 255 / 3;
                            tmp[i] = (255 << 24) | (b << 16) | (g << 8) | r;
                        }
                    } else {
                        uint16_t* fb16 = (uint16_t*)fb;
                        for(int i=0; i<W*H; i++) {
                            uint16_t c = fb16[i];
                            uint8_t r = ((c >> 11) & 0x1F) * 255 / 31, g = ((c >> 5) & 0x3F) * 255 / 63, b = (c & 0x1F) * 255 / 31;
                            tmp[i] = (255 << 24) | (b << 16) | (g << 8) | r;
                        }
                    }
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, tmp);
                }
                glClear(GL_COLOR_BUFFER_BIT); glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); SDL_GL_SwapWindow(window);
            }
        }
        uint32_t elapsed = SDL_GetTicks() - last_time;
        if (elapsed < 16) SDL_Delay(16 - elapsed);
        last_time = SDL_GetTicks();
    }
    return 0;
}
