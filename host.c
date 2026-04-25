
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <SDL2/SDL.h>
#ifdef PORTMASTER
#include <SDL2/SDL_opengles2.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
#endif

#include "wasm3.h"
#include "m3_env.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t ram;
    uint32_t vram;
    uint32_t redraw;
    
    // ---- Inputs ----
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

// ---- GLSL 1.20 / GLES 100 shaders ----
#ifdef PORTMASTER
static const char* VERT_SRC =
    "#version 100\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = a_uv;\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

static const char* FRAG_SRC =
    "#version 100\n"
    "precision mediump float;\n"
    "uniform sampler2D u_fb;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_fb, v_uv);\n"
    "}\n";
#else
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
#endif

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

// ---- API Exports from Host to WASM ----
m3ApiRawFunction(host_get_ticks) {
    m3ApiReturnType(uint32_t);
    m3ApiReturn(SDL_GetTicks());
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: %s game.wasm [scale]\n", argv[0]); return 1; }
    int scale = (argc >= 3) ? atoi(argv[2]) : 4;
    if (scale < 1) scale = 1;

    // ---- Bootstrap Wasm3 ----
    IM3Environment env     = m3_NewEnvironment();
    IM3Runtime     runtime = m3_NewRuntime(env, 4 * 1024 * 1024, NULL);

    FILE* f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); size_t wsize = ftell(f); rewind(f);
    uint8_t* wasm = malloc(wsize);
    fread(wasm, 1, wsize, f); fclose(f);

    IM3Module module;
    m3_ParseModule(env, &module, wasm, wsize);
    m3_LoadModule(runtime, module);

    m3_LinkRawFunction(module, "env", "get_ticks", "i()", host_get_ticks);

    IM3Function fn_init, fn_update;
    m3_FindFunction(&fn_init,   runtime, "papagaio_init");
    m3_FindFunction(&fn_update, runtime, "papagaio_update");

    m3_CallV(fn_init);

    uint32_t sys_wasm_offset = 0;

    uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
    SystemConfig* sys = (SystemConfig*)(mem + sys_wasm_offset);

    uint32_t vram_ptr = sizeof(SystemConfig);
    uint32_t ram_ptr = vram_ptr + sys->vram;
    uint32_t total_needed = ram_ptr + sys->ram;

    uint32_t required_pages = (total_needed + 65535) / 65536;
    uint32_t current_pages  = m3_GetMemorySize(runtime) / 65536;
    if (required_pages > current_pages)
        ResizeMemory(runtime, required_pages);

    mem = m3_GetMemory(runtime, NULL, 0);
    sys = (SystemConfig*)(mem + sys_wasm_offset);
    uint16_t* fb  = (uint16_t*)(mem + vram_ptr);
    uint32_t  W   = sys->width, H = sys->height;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    if (SDL_NumJoysticks() > 0) {
        if (SDL_IsGameController(0)) SDL_GameControllerOpen(0);
        else SDL_JoystickOpen(0);
    }

#ifdef PORTMASTER
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_ShowCursor(0);
    SDL_Window* window = SDL_CreateWindow("funnybuffer", 0, 0, 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window* window = SDL_CreateWindow("funnybuffer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W * scale, H * scale, SDL_WINDOW_OPENGL);
#endif

    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);

    GLuint prog = build_program();
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "u_fb"),  0);

    float quad[] = {
        -1.f, -1.f,  0.f, 1.f,
         1.f, -1.f,  1.f, 1.f,
        -1.f,  1.f,  0.f, 0.f,
         1.f,  1.f,  1.f, 0.f,
    };
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);

    GLuint fb_tex;
    glGenTextures(1, &fb_tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, W, H, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);

    int running = 1;
    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                int down = (e.type == SDL_KEYDOWN);
                
                // Map to 256 keys array via physical scancode
                if (e.key.keysym.scancode < 256) {
                    sys->keys[e.key.keysym.scancode] = down ? 1 : 0;
                }

                uint32_t bit = 0;
                switch (e.key.keysym.sym) {
                    case SDLK_UP:     bit = BTN_UP; break;
                    case SDLK_DOWN:   bit = BTN_DOWN; break;
                    case SDLK_LEFT:   bit = BTN_LEFT; break;
                    case SDLK_RIGHT:  bit = BTN_RIGHT; break;
                    case SDLK_z:      bit = BTN_A; break;
                    case SDLK_x:      bit = BTN_B; break;
                    case SDLK_RETURN: bit = BTN_START; break;
                    case SDLK_ESCAPE: bit = BTN_SELECT; break;
                }
                if (bit) { if (down) sys->gamepad_buttons |= bit; else sys->gamepad_buttons &= ~bit; }
            }
            if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) {
                int down = (e.type == SDL_CONTROLLERBUTTONDOWN);
                uint32_t bit = 0;
                switch (e.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:    bit = BTN_UP; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  bit = BTN_DOWN; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  bit = BTN_LEFT; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: bit = BTN_RIGHT; break;
                    case SDL_CONTROLLER_BUTTON_A:          bit = BTN_A; break;
                    case SDL_CONTROLLER_BUTTON_B:          bit = BTN_B; break;
                    case SDL_CONTROLLER_BUTTON_START:      bit = BTN_START; break;
                    case SDL_CONTROLLER_BUTTON_BACK:       bit = BTN_SELECT; break;
                }
                if (bit) { if (down) sys->gamepad_buttons |= bit; else sys->gamepad_buttons &= ~bit; }
            }
        }

        m3_CallV(fn_update);

        // Re-get memory pointers in case of growth/reallocation
        mem = m3_GetMemory(runtime, NULL, 0);
        sys = (SystemConfig*)(mem + sys_wasm_offset);
        fb  = (uint16_t*)(mem + vram_ptr);

        if (sys->redraw) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, fb_tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, fb);
            sys->redraw = 0;
        }

        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        SDL_GL_SwapWindow(window);
    }
    return 0;
}
