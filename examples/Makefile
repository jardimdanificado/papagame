
CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3
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
draw/image_data.h: images/image.webp
	$(IMG2FB) images/image.webp draw/image 16

images/image_data.h: images/image.webp
	$(IMG2FB) images/image.webp images/image 16

audio/audio_data.h: audio/audio.mp3
	$(AUDIO2PCM) audio/audio.mp3 audio/audio_data.h --name music

# WASM Rules
audio_example.wasm: audio/main.c audio/audio_data.h
	$(CLANG) $(CFLAGS) -Iaudio -Wl,--initial-memory=67108864 $(LDFLAGS) audio/main.c -o audio_example.wasm

buttons_test.wasm: buttons_test.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) buttons_test.c -o buttons_test.wasm

test_8bpp.wasm: test_8bpp.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) test_8bpp.c -o test_8bpp.wasm

test_16bpp.wasm: test_16bpp.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) test_16bpp.c -o test_16bpp.wasm

test_32bpp.wasm: test_32bpp.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) test_32bpp.c -o test_32bpp.wasm

audio_test.wasm: audio_test.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) audio_test.c -o audio_test.wasm

draw_example.wasm: draw/main.c draw/olive.c draw/image_data.h
	$(CLANG) $(CFLAGS) -Idraw $(MEM_FLAGS) $(LDFLAGS) draw/main.c -o draw_example.wasm

images_example.wasm: images/main.c images/image_data.h
	$(CLANG) $(CFLAGS) -Iimages $(MEM_FLAGS) $(LDFLAGS) images/main.c -o images_example.wasm

roguelike_example.wasm: roguelike/main.c roguelike/data/sprites.h
	$(CLANG) $(CFLAGS) -Iroguelike $(MEM_FLAGS) $(LDFLAGS) roguelike/main.c -o roguelike_example.wasm

clean:
	rm -f *.wasm
	rm -f draw/image_data.h draw/*.raw
	rm -f images/image_data.h images/*.raw
	rm -f audio/audio_data.h
