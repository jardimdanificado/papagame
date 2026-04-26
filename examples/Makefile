
CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined -Wl,--global-base=1048576
MEM_FLAGS = -Wl,--initial-memory=8388608

# Tools
IMG2FB = ../tools/img2fb.sh
AUDIO2PCM = ../tools/audio2pcm.py

.PHONY: all clean audio

# Default target: only light examples
all: buttons_test.wasm test_8bpp.wasm test_16bpp.wasm test_32bpp.wasm \
	draw_example.wasm images_example.wasm roguelike_example.wasm

# Heavy examples target
audio: audio_test.wasm audio_example.wasm

# Data Generation Rules
c/draw/image_data.h: c/images/image.webp
	$(IMG2FB) c/images/image.webp c/draw/image 16

c/images/image_data.h: c/images/image.webp
	$(IMG2FB) c/images/image.webp c/images/image 16

c/audio/audio_data.h: c/audio/audio.mp3
	$(AUDIO2PCM) c/audio/audio.mp3 c/audio/audio_data.h --name music

# WASM Rules
audio_example.wasm: c/audio/main.c c/audio/audio_data.h
	$(CLANG) $(CFLAGS) -Ic/audio -Wl,--initial-memory=67108864 $(LDFLAGS) c/audio/main.c -o audio_example.wasm

buttons_test.wasm: c/buttons_test.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) c/buttons_test.c -o buttons_test.wasm

test_8bpp.wasm: c/test_8bpp.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) c/test_8bpp.c -o test_8bpp.wasm

test_16bpp.wasm: c/test_16bpp.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) c/test_16bpp.c -o test_16bpp.wasm

test_32bpp.wasm: c/test_32bpp.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) c/test_32bpp.c -o test_32bpp.wasm

audio_test.wasm: c/audio_test.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) c/audio_test.c -o audio_test.wasm

draw_example.wasm: c/draw/main.c c/draw/olive.c c/draw/image_data.h
	$(CLANG) $(CFLAGS) -Ic/draw $(MEM_FLAGS) $(LDFLAGS) c/draw/main.c -o draw_example.wasm

images_example.wasm: c/images/main.c c/images/image_data.h
	$(CLANG) $(CFLAGS) -Ic/images $(MEM_FLAGS) $(LDFLAGS) c/images/main.c -o images_example.wasm

roguelike_example.wasm: c/roguelike/main.c c/roguelike/data/sprites.h
	$(CLANG) $(CFLAGS) -Ic/roguelike $(MEM_FLAGS) $(LDFLAGS) c/roguelike/main.c -o roguelike_example.wasm

clean:
	rm -f *.wasm
	rm -f c/draw/image_data.h c/draw/*.raw
	rm -f c/images/image_data.h c/images/*.raw
	rm -f c/audio/audio_data.h
