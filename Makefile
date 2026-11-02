TOOLS := .tools
CROSS ?= $(TOOLS)/riscv32-esp-elf/bin/riscv32-esp-elf-
PYTHON ?= uv run --locked python
RELEASE ?= latest
PORT ?=
INCLUDES := -Isrc/hal -Isrc/drivers -Isrc/app -Isrc/board
WARNINGS := -Wall -Wextra -Werror
CFLAGS := -march=rv32imac_zicsr_zifencei -mabi=ilp32 -mno-relax -flto -Os -g \
          -fstrict-volatile-bitfields -ffreestanding -fno-builtin -fno-common \
          -ffunction-sections -fdata-sections $(WARNINGS) $(INCLUDES)
HOST_CC ?= cc
HOST_CFLAGS := -std=c11 $(WARNINGS) $(INCLUDES)
# Retained LP RAM intentionally holds both executable code and mutable state.
LDFLAGS := -nostdlib -Wl,--gc-sections,--no-warn-rwx-segments,-Map=build/clock.map -Tsrc/boot/link.ld
SOURCES := $(wildcard src/hal/*.c src/drivers/*.c src/app/*.c src/board/*.c) src/boot/start.S src/boot/wake.S

all: build/clock.bin

setup:
	$(PYTHON) tools/setup.py

build/clock.elf: Makefile $(SOURCES) $(wildcard src/hal/*.h src/drivers/*.h src/app/*.h src/board/*.h) src/boot/link.ld
	@command -v $(CROSS)gcc >/dev/null || { echo "Compiler not found. Run make setup." >&2; exit 1; }
	mkdir -p build
	$(CROSS)gcc $(CFLAGS) $(LDFLAGS) $(SOURCES) -lgcc -o $@
	$(CROSS)size $@

build/clock.bin: build/clock.elf
	$(PYTHON) -m esptool --chip esp32c6 elf2image --flash_mode dio --flash_freq 40m --flash_size 16MB -o $@ $<

ram: all
	@test -n "$(PORT)" || { echo "Use make ram PORT=your-port" >&2; exit 1; }
	$(PYTHON) -m esptool --chip esp32c6 --port $(PORT) --no-stub load_ram build/clock.bin

flash: all
	$(PYTHON) tools/flash.py --file build/clock.bin $(if $(PORT),--port $(PORT))

flash-release:
	$(PYTHON) tools/flash.py --release $(RELEASE) $(if $(PORT),--port $(PORT))

set-time status:
	$(PYTHON) tools/clock.py $@ $(if $(PORT),--port $(PORT))

clean:
	rm -f build/clock.elf build/clock.bin build/clock.map

.PHONY: all setup ram flash flash-release set-time status clean

test:
	$(PYTHON) -m unittest discover -s tests -p "test_*.py"
	mkdir -p build
	$(HOST_CC) $(HOST_CFLAGS) tests/test_rtc.c src/drivers/rtc.c src/app/local_time.c -o build/test-rtc
	build/test-rtc
	$(HOST_CC) $(HOST_CFLAGS) tests/test_animation.c src/app/animation.c -o build/test-animation
	build/test-animation
	$(HOST_CC) $(HOST_CFLAGS) tests/test_orientation.c src/app/orientation.c -o build/test-orientation
	build/test-orientation
	$(HOST_CC) $(HOST_CFLAGS) tests/test_clock.c src/app/clock.c src/app/animation.c src/app/orientation.c -o build/test-clock
	build/test-clock
	$(HOST_CC) $(HOST_CFLAGS) tests/test_digits.c src/app/digits.c src/app/orientation.c -o build/test-digits
	build/test-digits
	$(HOST_CC) $(HOST_CFLAGS) tests/test_accel.c src/drivers/accel.c -o build/test-accel
	build/test-accel

.PHONY: test

# Executes the firmware ELF with modeled GPIO-connected board devices.
emulate: build/clock.elf
	uv run --locked --group sim python sim/virtual_board.py

emulator-check: build/clock.elf
	uv run --locked --group sim python -m unittest discover -s sim -p 'test_*.py'
	uv run --locked --group sim python sim/virtual_board.py --check

# Separate stock Espressif emulator smoke test (no external board devices).
emulator-soc-check: all
	$(PYTHON) sim/emulate.py --check

.PHONY: emulate emulator-check emulator-soc-check
