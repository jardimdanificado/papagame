PAPAGAIOCC = ../../papagaio/papagaiocc
TARGET_WASM = game.wasm

.PHONY: all clean

all: $(TARGET_WASM)

$(TARGET_WASM): main.c data/sprites_data.h
	$(PAPAGAIOCC) main.c -o $(TARGET_WASM) -I$(shell pwd) -Wl,--global-base=160000

clean:
	rm -f $(TARGET_WASM)
