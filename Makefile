CC = gcc
WASM3_DIR = lib/wasm3
WASM3_SRC = $(WASM3_DIR)/*.c

CFLAGS = -I$(WASM3_DIR) -O2
LDFLAGS = -lSDL2 -lGL -lm

TARGET_HOST = wagnostic

.PHONY: all clean

all: $(TARGET_HOST)

$(TARGET_HOST): runners/native/host.c
	$(CC) $(CFLAGS) $< $(WASM3_SRC) -o $@ $(LDFLAGS)

term: runners/native/ncurses_host.c
	$(CC) $(CFLAGS) $< $(WASM3_SRC) -o wagnostic-term -lncurses -lm

CC_AARCH64 = aarch64-linux-gnu-gcc
CFLAGS_AARCH64 = -Ilib/wasm3 -O2 -DPORTMASTER
LDFLAGS_AARCH64 = -lSDL2 -lGLESv2 -lm

portmaster:
	mkdir -p wagnostic
	@echo "Montando ambiente de Cross-Compile no Docker..."
	@sudo docker run --rm -e DEBIAN_FRONTEND=noninteractive -v $(PWD):/bld -w /bld ubuntu:20.04 bash -c " \
		sed -i 's/^deb /deb [arch=amd64] /g' /etc/apt/sources.list && \
		echo 'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ focal main restricted universe multiverse' >> /etc/apt/sources.list && \
		echo 'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ focal-updates main restricted universe multiverse' >> /etc/apt/sources.list && \
		dpkg --add-architecture arm64 && apt-get update -qq && \
		apt-get install -yq --no-install-recommends gcc-aarch64-linux-gnu libsdl2-dev:arm64 libgles2-mesa-dev:arm64 && \
		$(CC_AARCH64) $(CFLAGS_AARCH64) runners/native/host.c $(WASM3_SRC) -o wagnostic/$(TARGET_HOST) $(LDFLAGS_AARCH64) \
	"
	@echo "PortMaster build complete! Files are inside wagnostic/ and wagnostic.sh"
	@echo "ZIP the files: zip -r wagnostic_port.zip wagnostic wagnostic.sh"

clean:
	rm -rf $(TARGET_HOST) wagnostic-term wagnostic
