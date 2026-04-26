
// extern float sinf(float x); // Usually provided by host or we can use builtin
#define sinf __builtin_sinf

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;
typedef          short int16_t;

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

#define SAMPLE_RATE 44100
#define AUDIO_BUF_SIZE 16384
#define PI 3.14159265359f

float phase = 0;

void game_frame() {
    if (_sys->width == 0) {
        init("Wagnostic - Audio Test (Sine Wave)", 320, 240, 16, 4, AUDIO_BUF_SIZE, SAMPLE_RATE, 2, 2);
    }

    uint8_t* mem = (uint8_t*)0;
    uint8_t* audio_buf = mem + 512 + (_sys->width * _sys->height * (_sys->bpp / 8));
    int16_t* samples = (int16_t*)audio_buf;

    uint32_t r = _sys->audio_read_ptr;
    uint32_t w = _sys->audio_write_ptr;
    uint32_t size = _sys->audio_size;

    // Calculate free space in ring buffer
    int available_bytes = (r > w) ? (r - w - 1) : (size - w + r - 1);
    int samples_to_write = available_bytes / 4; // 16-bit Stereo = 4 bytes per sample

    if (samples_to_write > 512) samples_to_write = 512; // Don't write too much at once

    for (int i = 0; i < samples_to_write; i++) {
        // Simple triangle wave: phase goes from 0 to 2*PI
        float t = phase / (2.0f * PI); // 0 to 1
        float val = (t < 0.5f) ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t);
        val *= 0.2f; // Volume
        int16_t sample = (int16_t)(val * 32767.0f);
        
        uint32_t pos = (w + i * 4) % size;
        int16_t* out = (int16_t*)(audio_buf + pos);
        out[0] = sample; // Left
        out[1] = sample; // Right

        phase += 2.0f * PI * 440.0f / SAMPLE_RATE;
        if (phase > 2.0f * PI) phase -= 2.0f * PI;
    }

    _sys->audio_write_ptr = (w + samples_to_write * 4) % size;

    // Simple visual feedback
    uint16_t* fb = (uint16_t*)(mem + 512);
    for(int i=0; i<320*240; i++) fb[i] = (uint16_t)(phase * 1000);
    _sys->redraw = 1;
}
