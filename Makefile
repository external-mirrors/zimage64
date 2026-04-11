CC ?= gcc
READELF ?= readelf
OBJCOPY ?= objcopy
CFLAGS += -O3 -fno-stack-protector -ffunction-sections -fdata-sections -ffreestanding -fno-plt -fPIE -fno-pic

all: mkzimage64

clean:
	rm -f *.o payload.bin payload.elf mkzimage64
	cd uzlib; make clean

payload.bin: payload.elf
	LANG=POSIX $(READELF) -r $< | grep -q 'There are no relocations in this file'
	$(OBJCOPY) --dump-section .text=$@ $< /dev/null

libtinf.o: uzlib/src/*.c uzlib/src/*.h
	$(CC) $(CFLAGS) uzlib/src/*.c -nostdlib -Wl,-r -no-pie -o $@

%.o: %.c *.h
	$(CC) $(CFLAGS) $< -c -o $@

%.o: %.S
	$(CC) $< -c -o $@

payload.elf: link.x crt.o main.o memset.o libtinf.o mmu.o
	$(CC) -nostdlib -no-pie -Wl,-pie,--no-dynamic-linker -Wl,-e,_start,-gc-sections,-T,$^ -o $@

mkzimage64: mkzimage64.c payload.bin
	gcc mkzimage64.c -O2 -o mkzimage64 $(shell pkg-config --cflags --libs zlib)
