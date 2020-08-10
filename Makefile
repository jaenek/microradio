FQBN ?= esp8266:esp8266:nodemcuv2
BPRP ?= :xtal=160,ip=hb1,baud=921600
PORT ?= /dev/ttyUSB0
BAUD ?= 115200

all: compile upload mkfs terminal

clean:
	@echo cleaning
	-rm *$(subst :,.,.$(FQBN).)*

compile:
	@arduino-cli compile -b $(FQBN)$(BRPR)

upload:
	@arduino-cli upload -b $(FQBN)$(BRPR) -p $(PORT)

mkfs:
	@../tools/mklittlefs/mklittlefs -p 256 -b 8192 -s 1044464 -c data/ image.bin
	@esptool.py --chip esp8266 --port /dev/ttyUSB0 --baud 115200 write_flash 0x200000 image.bin

terminal:
	@tail -f $(PORT)
