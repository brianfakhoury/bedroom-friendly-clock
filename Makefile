TOOLS := $(if $(wildcard .tools/riscv32-esp-elf),.tools,../../work)
CROSS ?= $(TOOLS)/riscv32-esp-elf/bin/riscv32-esp-elf-
PYTHON ?= $(TOOLS)/venv/bin/python
PORT ?= /dev/cu.usbmodem1101
CFLAGS := -march=rv32imac_zicsr_zifencei -mabi=ilp32 -mno-relax -flto -Os -g -fstrict-volatile-bitfields -ffreestanding -fno-builtin -fno-common -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Ifirmware/hal -Ifirmware/drivers -Ifirmware/app
SOURCES := $(wildcard firmware/hal/*.c firmware/drivers/*.c firmware/app/*.c) firmware/boot/start.S firmware/boot/wake.S
all: build/clock.bin
build/clock.elf: Makefile $(SOURCES) $(wildcard firmware/hal/*.h firmware/drivers/*.h firmware/app/*.h) firmware/boot/link.ld
	mkdir -p build
	$(CROSS)gcc $(CFLAGS) -nostdlib -Wl,--gc-sections,-Map=build/clock.map -Tfirmware/boot/link.ld $(SOURCES) -lgcc -o $@
	$(CROSS)size $@
build/clock.bin: build/clock.elf
	$(PYTHON) -m esptool --chip esp32c6 elf2image --flash_mode dio --flash_freq 40m --flash_size 16MB -o $@ $<
ram: all
	$(PYTHON) -m esptool --chip esp32c6 --port $(PORT) --no-stub load_ram build/clock.bin
flash: all
	$(PYTHON) -m esptool --chip esp32c6 --port $(PORT) --no-stub write_flash 0 build/clock.bin
clean:
	rm -f build/clock.elf build/clock.bin build/clock.map
.PHONY: all ram flash clean

test:
	mkdir -p build
	cc -std=c11 -Wall -Wextra -Werror -Ifirmware/hal -Ifirmware/drivers -Ifirmware/app tests/test_rtc.c firmware/drivers/rtc.c firmware/app/local_time.c -o build/test-rtc
	build/test-rtc
	cc -std=c11 -Wall -Wextra -Werror -Ifirmware/app tests/test_animation.c firmware/app/animation.c -o build/test-animation
	build/test-animation
	cc -std=c11 -Wall -Wextra -Werror -Ifirmware/drivers -Ifirmware/app tests/test_orientation.c firmware/app/orientation.c -o build/test-orientation
	build/test-orientation
	cc -std=c11 -Wall -Wextra -Werror -Ifirmware/hal -Ifirmware/drivers -Ifirmware/app tests/test_clock.c firmware/app/clock.c firmware/app/animation.c firmware/app/orientation.c -o build/test-clock
	build/test-clock
	cc -std=c11 -Wall -Wextra -Werror -Ifirmware/hal -Ifirmware/drivers -Ifirmware/app tests/test_digits.c firmware/app/digits.c firmware/app/orientation.c -o build/test-digits
	build/test-digits
.PHONY: test
