CC = gcc
PAPAGAIO_DIR = papagaio
PAPAGAIOCC = ./$(PAPAGAIO_DIR)/papagaio
WASM3_DIR = $(PAPAGAIO_DIR)/lib/wasm3
WASM3_SRC = $(WASM3_DIR)/*.c

CFLAGS = -I$(WASM3_DIR) -O2
LDFLAGS = -lSDL2 -lm

TARGET_HOST = papagame
TARGET_WASM = game.wasm

.PHONY: all clean run papagaio update-papagaio

all: papagaio $(TARGET_HOST) $(TARGET_WASM)

papagaio:
	$(MAKE) -C $(PAPAGAIO_DIR)

update-papagaio:
	git submodule update --remote --merge

$(TARGET_WASM): game.c | papagaio
	$(PAPAGAIOCC) $< -o $@

$(TARGET_HOST): host.c | papagaio
	$(CC) $(CFLAGS) $< $(WASM3_SRC) -o $@ $(LDFLAGS)

run: all
	./$(TARGET_HOST) $(TARGET_WASM)

clean:
	rm -f $(TARGET_HOST) $(TARGET_WASM)
