#include <Arduino.h>
#include <EEPROM.h>
//#include <DMXSerial.h>
#include <TM1637Display.h>

#define DISPLAY_CLK 6
#define DISPLAY_DIO 5
#define SETTINGS_BUTTON 4 // Pin 2 and 3 are usable for interrupts
#define UP_BUTTON 2
#define DOWN_BUTTON 3
#define TRIGGER 10

int dmxStartChannel = 0;
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

const uint8_t SEG_EDIT[] = {SEG_A | SEG_D | SEG_E | SEG_F | SEG_G};
const uint8_t SEG_FOG[] = {SEG_A  | SEG_E | SEG_F | SEG_G};
const uint8_t EMPTY[] = {0};

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);

void pwm();

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
    display.setBrightness(0x0f);
    pinMode(SETTINGS_BUTTON, INPUT_PULLUP);
    pinMode(UP_BUTTON, INPUT_PULLUP);
    pinMode(DOWN_BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(UP_BUTTON), isrUp, CHANGE);
    attachInterrupt(digitalPinToInterrupt(DOWN_BUTTON), isrDown, CHANGE);
    Serial.begin(115200);
    Serial.println("Starting");

//    DMXSerial.init(DMXReceiver);
    pinMode(TRIGGER, OUTPUT);
    EEPROM.get(0, dmxStartChannel);
    Serial.println(dmxStartChannel);
    delay(2000);
    display.showNumberDec(dmxStartChannel);
    updateChannels();
}

void editSettings() {
    delay(100);
    while(!digitalRead(SETTINGS_BUTTON)); // Prevent exiting this menu before settings button is released
    display.setSegments(SEG_EDIT, 1, 0);
    Serial.println("Settings");
    int currentInput = dmxStartChannel; // init with currently set dmx channel
    float multiplier = 1;
    bool changed = false;
    while (digitalRead(SETTINGS_BUTTON)) {
        if (currentInput > 512) currentInput = 1;
        if (currentInput < 1) currentInput = 512;
        if (changed) display.showNumberDec(currentInput, false, 3, 1);
        if (upButtonPressed) {
            currentInput += multiplier;
            multiplier += 0.3;
            changed = true;
        } else if (downButtonPressed) {
            currentInput -= multiplier;
            multiplier += 0.3;
            changed = true;
        } else {
            multiplier = 1;
            changed = false;
        }
        delay(75);
    }
    dmxStartChannel = currentInput;
    Serial.println(dmxStartChannel);
    EEPROM.put(0, dmxStartChannel);
    updateChannels();
    display.showNumberDec(dmxStartChannel);
    while(!digitalRead(SETTINGS_BUTTON)); // Prevent exiting this menu before settings button is released
    delay(100);
}

void loop() {
    if (digitalRead(SETTINGS_BUTTON) == LOW) editSettings();
//    pwm();
}

//void pwm() {
//    unsigned long lastPacket = DMXSerial.noDataSince();
//    if (lastPacket < 5000) {
//      // Check for frequency changes
//      unsigned int newDmxValueFrequency = DMXSerial.read(dmxFrequencyChannel);
//      if (newDmxValueFrequency != currentFrequencySek) {
//        // If interval length changed, reset current running interval and start over
//        currentCycleStart = 0;
//        nextTurnOffTime = 0;
//      }
//
//      // Check for duty cycle changes
//      unsigned int newDmxValueDutyCycle = DMXSerial.read(dmxDutyCycleChannel);
//      if (newDmxValueDutyCycle != currentDutyCycleDmxValue) {
//        currentDutyCycleDmxValue = newDmxValueDutyCycle;
//        currentDutyCycle = (float) newDmxValueDutyCycle / 255;
//      }
//    }
//
//    unsigned long currentMillis = millis();
//    // Check if new cycle needs to be started
//    if (currentMillis > currentCycleStart) {
//      digitalWrite(TRIGGER, HIGH);
//      needToTurnOff = true;
//      nextTurnOffTime = currentMillis + (currentDutyCycle * currentFrequencySek);
//    }
//    // Check if trigger needs to be tunred off
//    if (needToTurnOff && currentMillis > nextTurnOffTime) {
//      digitalWrite(TRIGGER, LOW);
//      needToTurnOff = false;
//    }
//}
