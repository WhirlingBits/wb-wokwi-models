SOURCES = src/main.c
CHIP_JSON = chip.json
TARGET = chip.wasm

WASI_SYSROOT ?= /tmp/wasi-sdk-24.0-x86_64-linux/share/wasi-sysroot
CLANG_RT ?= /tmp/clang-rt-wasm/usr/lib/llvm-18/lib/clang/18

CLANG_FLAGS = --target=wasm32-unknown-wasi \
              --sysroot $(WASI_SYSROOT) \
              -nostartfiles \
              -Wl,--import-memory -Wl,--export-table -Wl,--no-entry \
              -resource-dir $(CLANG_RT) \
              -isystem /usr/lib/llvm-18/lib/clang/18/include \
              -I. -Isrc

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(CHIP_JSON)
	clang $(CLANG_FLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)
