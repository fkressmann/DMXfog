#include <Arduino.h>
#include <EEPROM.h>
#include <DMXSerial.h>
#include <TM1637Display.h>

#define DISPLAY_CLK 2
#define DISPLAY_DIO 3
#define SETTINGS_BUTTON 2 // Pin 2 and 3 are usable for interrupts
#define UP_BUTTON 2
#define DOWN_BUTTON 3
#define DISPLAY_DOT 0b01000000
#define TRIGGER 10

int dmxStartChannel;
int dmxDutyCycleChannel;
int dmxFrequencyChannel;
int currentFrequencySek = 0; // Interval length, seconds, 0-255
int currentDutyCycleDmxValue; // Duty cycle as DMX value 0-255
float currentDutyCycle = 0; // Duty cycle, decimal, 0-1

unsigned long currentCycleStart = 0;
unsigned long nextTurnOffTime = 0;
bool needToTurnOff = false;
bool upButtonPressed = false;
bool downButtonPressed = false;

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);

void updateChannels() {
    dmxFrequencyChannel = dmxStartChannel;
    dmxDutyCycleChannel = dmxStartChannel + 1;
}

void isrUp() {
    upButtonPressed = !digitalRead(UP_BUTTON);
}

void isrDown() {
    downButtonPressed = !digitalRead(DOWN_BUTTON);
}

void setup() {
    pinMode(SETTINGS_BUTTON, INPUT_PULLUP);
    pinMode(UP_BUTTON, INPUT_PULLUP);
    pinMode(DOWN_BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(UP_BUTTON), isrUp, CHANGE);
    attachInterrupt(digitalPinToInterrupt(DOWN_BUTTON), isrDown, CHANGE);


    DMXSerial.init(DMXReceiver);
    pinMode(TRIGGER, OUTPUT);
    // EEPROM.get(0, dmxStartChannel);
    // Put dmxStartChannel onto display, too
    dmxStartChannel = 1;
    updateChannels();
}

void editSettings() {
    int currentInput = dmxStartChannel; // init with currently set dmx channel
    float multiplier = 1;
    bool changed = true;
    while (digitalRead(SETTINGS_BUTTON)) {
        if (currentInput > 512) currentInput = 1;
        if (changed) display.showNumberDecEx(currentInput, DISPLAY_DOT);
        if (upButtonPressed) {
            currentInput += multiplier;
            multiplier += 0.1;
            changed = true;
        } else if (downButtonPressed) {
            currentInput -= multiplier;
            multiplier += 0.1;
            changed = true;
        } else {
            multiplier = 0;
            changed = false;
        }
        delay(50);
    }
    dmxStartChannel = currentInput;
    EEPROM.put(0, dmxStartChannel);
    updateChannels();
    display.showNumberDecEx(dmxStartChannel, DISPLAY_DOT);
    while(!digitalRead(SETTINGS_BUTTON)); // Prevent exiting this menu before settings button is released
}

void loop() {
    if (digitalRead(UP_BUTTON) == LOW) editSettings();
  unsigned long lastPacket = DMXSerial.noDataSince();
  if (lastPacket < 5000) {
    // Check for frequency changes
    unsigned int newDmxValueFrequency = DMXSerial.read(dmxFrequencyChannel);
    if (newDmxValueFrequency != currentFrequencySek) {
      // If interval length changed, reset current running interval and start over
      currentCycleStart = 0;
      nextTurnOffTime = 0;
    }

    // Check for duty cycle changes
    unsigned int newDmxValueDutyCycle = DMXSerial.read(dmxDutyCycleChannel);
    if (newDmxValueDutyCycle != currentDutyCycleDmxValue) {
      currentDutyCycleDmxValue = newDmxValueDutyCycle;
      currentDutyCycle = (float) newDmxValueDutyCycle / 255;
    }
  }
  
  unsigned long currentMillis = millis();
  // Check if new cycle needs to be started
  if (currentMillis > currentCycleStart) {
    digitalWrite(TRIGGER, HIGH);
    needToTurnOff = true;
    nextTurnOffTime = currentMillis + (currentDutyCycle * currentFrequencySek);
  }
  // Check if trigger needs to be tunred off
  if (needToTurnOff && currentMillis > nextTurnOffTime) {
    digitalWrite(TRIGGER, LOW);
    needToTurnOff = false;
  }

}