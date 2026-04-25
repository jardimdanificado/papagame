
PAPAGAIOCC = ../../papagaio/papagaiocc
INPUT_IMAGE = image.webp
RAW_IMAGE = image.raw
HEADER_IMAGE = image_data.h
TARGET_WASM = game.wasm

.PHONY: all clean convert

all: convert $(TARGET_WASM)

convert: $(HEADER_IMAGE)

$(HEADER_IMAGE): $(INPUT_IMAGE)
	../../tools/img2fb.sh $(INPUT_IMAGE) image

$(TARGET_WASM): main.c $(HEADER_IMAGE)
	$(PAPAGAIOCC) main.c -o $(TARGET_WASM) -I$(shell pwd) -Wl,--global-base=160000

clean:
	rm -f $(RAW_IMAGE) $(HEADER_IMAGE) $(TARGET_WASM) palette.png
