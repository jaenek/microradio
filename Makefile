FQBN  ?= esp8266:esp8266:nodemcuv2
BPRP  ?= :xtal=160,ip=hb1,baud=921600
PORT  ?= /dev/ttyUSB0
BAUD  ?= 115200
FLAGS ?=

all: compile upload terminal

clean:
	@echo cleaning
	-rm -r build/ image.bin

compile:
	@arduino-cli compile -b $(FQBN)$(BRPR) --build-properties="compiler.cpp.extra_flags=$(FLAGS)"
mkfs:
	@../tools/mklittlefs/mklittlefs -p 256 -b 8192 -s 1044464 -c data/ image.bin

upload: mkfs
	@arduino-cli upload -b $(FQBN)$(BRPR) -p $(PORT)
	@esptool.py --chip esp8266 --port $(PORT) --baud 115200 write_flash 0x200000 image.bin

uploaddata: mkfs
	@esptool.py --chip esp8266 --port $(PORT) --baud 115200 write_flash 0x200000 image.bin

realeaseflags:
	FLAGS += -DNDEBUG

release: clean compile mkfs
	@cp build/*/microradio.ino.bin releases/program.bin
	@cp image.bin releases/data.bin
	@zip releases/microradio.zip releases/python-3.8.6rc1-webinstall.exe releases/program.bin releases/data.bin


terminal:
	@tail -f $(PORT)
