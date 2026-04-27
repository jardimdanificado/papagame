#include "wagnostic.h"
#include "audio_data.h"

static uint16_t* _fb;
static uint8_t* _audio_buf;

void winit() {
    // Inicializa o sistema básico
    w_setup("Wagnostic - Audio Player", 320, 240, 16, 4, 8);
    
    // Configura os ponteiros usando as novas macros da lib
    _fb = (uint16_t*)W_FB_PTR;
    
    // Configura parâmetros de áudio
    W_SYS->audio_size = music_size;
    W_SYS->audio_sample_rate = 44100;
    W_SYS->audio_bpp = 2; // 16-bit
    W_SYS->audio_channels = 2;
    W_SYS->audio_write_ptr = 0;
    W_SYS->audio_read_ptr = 0;
    
    // Envia o sinal de que o áudio foi configurado
    W_SIGNALS[4] = W_SIG_UPDATE_AUDIO; 
    
    // O ponteiro de áudio agora é calculado corretamente pela lib
    _audio_buf = (uint8_t*)w_audio_ptr();
}

void fill_audio() {
    // Tenta manter o buffer cheio (uma lógica simples de stream)
    // No wagnostic, o Host consome do buffer circular.
    // Aqui copiamos um pedaço da música por vez para o buffer.
    uint32_t to_write = 16384; // Escreve 16kb por frame se possível
    
    for (uint32_t i = 0; i < to_write; i++) {
        uint32_t next_ptr = (W_SYS->audio_write_ptr + 1) % music_size;
        // Se a música acabou, loop ou para (aqui faz loop)
        _audio_buf[W_SYS->audio_write_ptr] = music_raw[W_SYS->audio_write_ptr];
        W_SYS->audio_write_ptr = next_ptr;
    }
}

__attribute__((visibility("default")))
void wupdate() {
    // Visualização simples: cor da tela muda com a posição da música
    uint16_t color = (W_SYS->audio_write_ptr >> 4) & 0xFFFF;
    for (int i = 0; i < 320 * 240; i++) {
        _fb[i] = color;
    }

    fill_audio();
    w_redraw();
}
