#include <Arduino.h>
#include <EEPROM.h>
#include <DMXSerial.h>

unsigned int TRIGGER = 10;

unsigned int dmxStartChannel;
unsigned int dmxDutyCycleChannel;
unsigned int dmxFreqencyChannel;
unsigned int currentFrequencySek = 0; // Interval length, seconds, 0-255
unsigned int currentDutyCycleDmxValue; // Duty cycle as DMX value 0-255
float currentDutyCycle = 0; // Duty cycle, decimal, 0-1

unsigned long currentCycleStart = 0;
unsigned long nextTurnOffTime = 0;
boolean needToTurnOff = false;

void updateChannels() {
    dmxFreqencyChannel = dmxStartChannel;
    dmxDutyCycleChannel = dmxStartChannel + 1;
}

void setup() {
    DMXSerial.init(DMXReceiver);
    pinMode(TRIGGER, OUTPUT);
    // EEPROM.get(0, dmxStartChannel);
    dmxStartChannel = 1;
    updateChannels();
}

void loop() {
  unsigned long lastPacket = DMXSerial.noDataSince();
  if (lastPacket < 5000) {
    // Check for frequency changes
    unsigned int newDmxValueFrequency = DMXSerial.read(dmxFreqencyChannel);
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