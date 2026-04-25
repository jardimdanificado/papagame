TARGET_WASM = game.wasm
PAPAGAIOCC = ../../papagaio/papagaiocc
HOST_RUNNER = ../../funnybuffer

all: $(TARGET_WASM) run

$(TARGET_WASM): main.c olive.c
	$(PAPAGAIOCC) main.c -o $(TARGET_WASM) -I$(shell pwd) -Wl,--global-base=160000

run: $(TARGET_WASM)
	$(HOST_RUNNER) $(TARGET_WASM)

clean:
	rm -f $(TARGET_WASM)

.PHONY: all run clean
