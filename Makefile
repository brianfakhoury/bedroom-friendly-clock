TOOLS := .tools
CROSS ?= $(TOOLS)/riscv32-esp-elf/bin/riscv32-esp-elf-
PYTHON ?= uv run --locked python
PORT ?=
CFLAGS := -march=rv32imac_zicsr_zifencei -mabi=ilp32 -mno-relax -flto -Os -g -fstrict-volatile-bitfields -ffreestanding -fno-builtin -fno-common -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Isrc/hal -Isrc/drivers -Isrc/app -Isrc/board
SOURCES := $(wildcard src/hal/*.c src/drivers/*.c src/app/*.c src/board/*.c) src/boot/start.S src/boot/wake.S
all: build/clock.bin

# The compiler targets the ESP32-C6; uv manages the host Python tools.
setup:
	uv sync --locked
	@set -eu; \
	case "$$(uname -s)-$$(uname -m)" in \
	  Darwin-arm64) target=aarch64-apple-darwin; checksum=5f11c929ecc8a978d9492e4e05cf6f607a4563efdb67e972fea28bef2a07b3bf ;; \
	  Linux-x86_64) target=x86_64-linux-gnu; checksum=0b029ab31ebc7f74e2c4779077251b21a22cd620669dc4c82e7adc478d403a8d ;; \
	  *) echo 'Supported hosts: Apple Silicon macOS and x86-64 Linux.' >&2; exit 1 ;; \
	esac; \
	mkdir -p $(TOOLS); \
	archive="$(TOOLS)/riscv32-esp-elf-16.1.0_20260609-$$target.tar.xz"; \
	url="https://github.com/espressif/crosstool-NG/releases/download/esp-16.1.0_20260609/$$(basename "$$archive")"; \
	if [ ! -f "$$archive" ]; then \
	  curl -fL "$$url" -o "$$archive.part"; \
	  mv "$$archive.part" "$$archive"; \
	fi; \
	printf '%s  %s\n' "$$checksum" "$$archive" | shasum -a 256 -c -; \
	if [ ! -x "$(TOOLS)/riscv32-esp-elf/bin/riscv32-esp-elf-gcc" ]; then \
	  tar -xf "$$archive" -C $(TOOLS); \
	fi
	@echo 'Ready. Run make, make test, or make flash.'
.PHONY: setup

build/clock.elf: Makefile $(SOURCES) $(wildcard src/hal/*.h src/drivers/*.h src/app/*.h src/board/*.h) src/boot/link.ld
	@command -v $(CROSS)gcc >/dev/null || { echo "Compiler not found. Run make setup." >&2; exit 1; }
	mkdir -p build
	$(CROSS)gcc $(CFLAGS) -nostdlib -Wl,--gc-sections,-Map=build/clock.map -Tsrc/boot/link.ld $(SOURCES) -lgcc -o $@
	$(CROSS)size $@
build/clock.bin: build/clock.elf
	$(PYTHON) -m esptool --chip esp32c6 elf2image --flash_mode dio --flash_freq 40m --flash_size 16MB -o $@ $<
ram: all
	$(PYTHON) -m esptool --chip esp32c6 --port $(PORT) --no-stub load_ram build/clock.bin
flash: all
	$(PYTHON) tools/flash.py --file build/clock.bin $(if $(PORT),--port $(PORT))
clean:
	rm -f build/clock.elf build/clock.bin build/clock.map
.PHONY: all ram flash clean

test:
	$(PYTHON) tests/test_flash.py
	mkdir -p build
	cc -std=c11 -Wall -Wextra -Werror -Isrc/hal -Isrc/drivers -Isrc/app -Isrc/board tests/test_rtc.c src/drivers/rtc.c src/app/local_time.c -o build/test-rtc
	build/test-rtc
	cc -std=c11 -Wall -Wextra -Werror -Isrc/app -Isrc/board tests/test_animation.c src/app/animation.c -o build/test-animation
	build/test-animation
	cc -std=c11 -Wall -Wextra -Werror -Isrc/drivers -Isrc/app -Isrc/board tests/test_orientation.c src/app/orientation.c -o build/test-orientation
	build/test-orientation
	cc -std=c11 -Wall -Wextra -Werror -Isrc/hal -Isrc/drivers -Isrc/app -Isrc/board tests/test_clock.c src/app/clock.c src/app/animation.c src/app/orientation.c -o build/test-clock
	build/test-clock
	cc -std=c11 -Wall -Wextra -Werror -Isrc/hal -Isrc/drivers -Isrc/app -Isrc/board tests/test_digits.c src/app/digits.c src/app/orientation.c -o build/test-digits
	build/test-digits
.PHONY: test
