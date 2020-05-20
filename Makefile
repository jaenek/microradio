FQBN ?= esp8266:esp8266:nodemcuv2
BPRP ?= :xtal=160,ip=hb2f,baud=921600
PORT ?= /dev/ttyUSB0
BAUD ?= 115200
STTY ?= cs8 ignbrk -brkint -imaxbel -opost -onlcr -isig -icanon -iexten \
	-echo -echoe -echok -echoctl -echoke noflsh -ixon -crtscts

all: compile upload

clean:
	@echo cleaning
	-rm *$(subst :,.,.$(FQBN).)*

compile:
	@arduino-cli compile -b $(FQBN)$(BRPR)

upload:
	@arduino-cli upload -b $(FQBN)$(BRPR) -p $(PORT)

terminal:
	@stty -F $(PORT) $(BAUD) $(STTY)
	@tail -f $(PORT)
