#include <vector>
#include <functional>

struct Button {
	int pin;
	std::function<void ()> shorthandler;
	std::function<void ()> longhandler;
	unsigned int longhandercounter = 1;
	unsigned long start = 0;
	unsigned long  duration = 0;
	bool buttonpressed = false;
	bool buttonchanged = false;
	bool laststate = HIGH;

	Button(int pin, std::function<void ()> shorthandler, std::function<void ()> longhandler) : pin(pin),
		shorthandler(shorthandler), longhandler(longhandler) {};

	void loop() {
		bool currentstate = digitalRead(pin);

		if (currentstate != laststate) {
			DBG.printf("changed %d", pin);
			buttonchanged = true;
		}

		if (buttonpressed) {
			duration = millis() - start;

			if (duration/1000 == longhandercounter) {
				longhandler();
				longhandercounter++;
			}
		}

		if (buttonpressed == true && buttonchanged == true) {
			DBG.printf("released %d held for %lu\n", pin, duration);

			if (duration < 1000)
				shorthandler();

			buttonchanged = false;
			buttonpressed = false;
			duration = 0;
			longhandercounter = 1;
		} else if (buttonchanged) {
			buttonchanged = false;
			buttonpressed = true;
			start = millis();
		}

		laststate = currentstate;
	}
};

class ButtonManager {
private:
	std::vector<Button> buttons;

public:
	void on(int pin, std::function<void ()> shorthandler, std::function<void ()> longhandler) {
		DBG.printf("initiated %d\n", pin);
		buttons.emplace_back(pin, shorthandler, longhandler);
		pinMode(pin, INPUT_PULLUP);
	}

	void loop() {
		for (int i = 0; i < buttons.size(); i++) {
			buttons[i].loop();
		}
	}
} *buttonmanager;
