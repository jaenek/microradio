#include <vector>
#include <functional>

struct Button {
	int pin;
	std::function<void ()> handler;
	int laststate;

	Button(int pin, std::function<void ()> handler) {
		this->pin = pin;
		this->handler = handler;
		this->laststate = HIGH;
	}
};

class ButtonManager {
private:
	std::vector<Button> buttons;

public:
	void on(int pin, std::function<void ()> func) {
		buttons.push_back(Button(pin, func));

		pinMode(pin, INPUT);
		Serial.printf("initiated %d\n", pin);
	}

	void loop() {
		for (int i = 0; i < buttons.size(); i ++) {
			int currentstate = digitalRead(buttons[i].pin);

			if (buttons[i].laststate == HIGH && currentstate == LOW) {
				buttons[i].handler();
			}

			buttons[i].laststate = currentstate;
		}
	}
} *buttonmanager;
