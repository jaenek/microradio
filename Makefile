FQBN ?= esp8266:esp8266:nodemcuv2
BPRP ?= :xtal=160,ip=hb1,baud=921600
PORT ?= /dev/ttyUSB0
BAUD ?= 115200

all: compile upload terminal

clean:
	@echo cleaning
	-rm *$(subst :,.,.$(FQBN).)*

compile:
	@arduino-cli compile -b $(FQBN)$(BRPR)

upload:
	@arduino-cli upload -b $(FQBN)$(BRPR) -p $(PORT)

terminal:
	@tail -f $(PORT)
