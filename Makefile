FQBN  ?= esp8266:esp8266:nodemcuv2
BPRP  ?= :xtal=160,ip=hb1,baud=921600
PORT  ?= /dev/ttyUSB0
BAUD  ?= 115200
FLAGS ?=

all: compile upload terminal

clean:
	@echo cleaning
	-rm -r build/ releases/common/data.bin

compile:
	@arduino-cli compile -b $(FQBN)$(BRPR) --build-properties="compiler.cpp.extra_flags=$(FLAGS)"

mkfs:
	@mkdir -pv releases/common
	@../tools/mklittlefs/mklittlefs -p 256 -b 8192 -s 1044464 -c data/ releases/common/data.bin

upload: mkfs
	@arduino-cli upload -b $(FQBN)$(BRPR) -p $(PORT)
	@esptool.py --chip esp8266 --port $(PORT) --baud 115200 write_flash 0x200000 releases/common/data.bin

uploaddata: mkfs
	@esptool.py --chip esp8266 --port $(PORT) --baud 115200 write_flash 0x200000 releases/common/data.bin

realeaseflags:
	FLAGS += -DNDEBUG

release: clean compile mkfs
	@cp build/*/microradio.ino.bin releases/common/program.bin
	@zip releases/microradio.zip releases/common/program.bin releases/common/data.bin


terminal:
	@tail -f $(PORT)
