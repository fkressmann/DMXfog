//#define SIMULATE_DMX

#include <Arduino.h>
#include <EEPROM.h>
#include <TM1637Display.h>
#include <SendOnlySoftwareSerial.h>

#ifndef SIMULATE_DMX

#include <DMXSerial.h>

#endif

#define DISPLAY_CLK 6
#define DISPLAY_DIO 5
#define SETTINGS_BUTTON 4 // Pin 2 and 3 are usable for interrupts
#define UP_BUTTON 2
#define DOWN_BUTTON 3
#define SERIAL 9
#define TRIGGER 10

int dmxStartChannel = 0;
int dmxDutyCycleChannel;
int dmxFrequencyChannel;
uint8_t currentFrequencySek = 0; // Interval length, seconds, 0-255
uint8_t currentDutyCycleDmxValue; // Duty cycle as DMX value 0-255
float currentDutyCycle = 0; // Duty cycle, decimal, 0-1

unsigned long nextTurnOffTime = 0;
unsigned long currentCycleEnd = 0;

bool dmxMode = true; // false: manual mode
bool needToTurnOff = false;
bool upButtonPressed = false;
bool downButtonPressed = false;

const uint8_t SEG_FOG[] = {SEG_A | SEG_E | SEG_F | SEG_G}; // 'F'
const uint8_t SEG_EDIT_CHANNEL[] = {SEG_A | SEG_F | SEG_E | SEG_D}; // 'C'
const uint8_t SEG_EDIT_MANUAL_CYCLE_DURATION[] = {SEG_A | SEG_F | SEG_C | SEG_D}; // 'S'
const uint8_t SEG_EDIT_MANUAL_DUTY_CYCLE[] = {SEG_A | SEG_F | SEG_B | SEG_G | SEG_E}; // 'P'
const uint8_t SEG_MANUAL_MODE[] = {SEG_A | SEG_G | SEG_E | SEG_C}; // 'M'
const uint8_t SEG_DMX_MODE[] = {SEG_B | SEG_G | SEG_E | SEG_C | SEG_D}; // 'd'
const uint8_t SEG_OFF[] = {SEG_A | SEG_F | SEG_B | SEG_E | SEG_C | SEG_D, // O
                           SEG_A | SEG_E | SEG_F | SEG_G, // F
                           SEG_A | SEG_E | SEG_F | SEG_G}; // F

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);
SendOnlySoftwareSerial mySerial(SERIAL);

void pwm();

bool readDmx();

void resetDmxUpdated();

void updateChannels() {
    dmxFrequencyChannel = dmxStartChannel;
    dmxDutyCycleChannel = dmxStartChannel + 1;
    mySerial.print("start Channel: ");
    mySerial.println(dmxStartChannel);
    mySerial.print("Frequency Channel: ");
    mySerial.println(dmxFrequencyChannel);
    mySerial.print("Duty Cycle Channel: ");
    mySerial.println(dmxDutyCycleChannel);
    resetDmxUpdated();
}

void isrUp() {
    upButtonPressed = !digitalRead(UP_BUTTON);
}

void isrDown() {
    downButtonPressed = !digitalRead(DOWN_BUTTON);
}

void setup() {
    mySerial.begin(115200);
    mySerial.println("Starting...");
#ifndef SIMULATE_DMX
    DMXSerial.init(DMXReceiver, 12);
#endif
    display.clear();
    display.setBrightness(0x0f);
    pinMode(SETTINGS_BUTTON, INPUT_PULLUP);
    pinMode(UP_BUTTON, INPUT_PULLUP);
    pinMode(DOWN_BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(UP_BUTTON), isrUp, CHANGE);
    attachInterrupt(digitalPinToInterrupt(DOWN_BUTTON), isrDown, CHANGE);
    pinMode(TRIGGER, OUTPUT);
    EEPROM.get(0, dmxStartChannel);
    updateChannels();
}

void setDisplayStatus(const uint8_t segments[]) {
    display.setSegments(segments, 1, 0);
}

void setDisplayStatusAccordingToMode() {
    if (dmxMode) {
        setDisplayStatus(SEG_DMX_MODE);
    } else {
        setDisplayStatus(SEG_MANUAL_MODE);
    }
}

void show3DigitNumber(int number) {
    display.showNumberDec(number, false, 3, 1);
}

int readInputValue(int currentInput, int minValue, int maxValue) {
    float multiplier = 1;
    bool changed = false;
    show3DigitNumber(currentInput);
    while (digitalRead(SETTINGS_BUTTON)) {
        if (currentInput > maxValue) currentInput = minValue;
        if (currentInput < minValue) currentInput = maxValue;
        if (changed) show3DigitNumber(currentInput);
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
    return currentInput;
}

void editSettings() {
    mySerial.print("Entering Settings for ");
    delay(100);
    while (!digitalRead(SETTINGS_BUTTON)); // Prevent exiting this menu before settings button is released

    if (dmxMode) {
        mySerial.println("DMX mode");
        setDisplayStatus(SEG_EDIT_CHANNEL);
        dmxStartChannel = readInputValue(dmxStartChannel, 1, 512);
        EEPROM.put(0, dmxStartChannel);
        updateChannels();
        resetDmxUpdated();
    } else {
        mySerial.println("manual mode");
        setDisplayStatus(SEG_EDIT_MANUAL_CYCLE_DURATION);
        currentFrequencySek = readInputValue(currentFrequencySek, 0, 240);
        mySerial.print("currentFrequencySek set to: ");
        mySerial.println(currentFrequencySek);
        while (!digitalRead(SETTINGS_BUTTON)); // Prevent exiting this menu before settings button is released
        setDisplayStatus(SEG_EDIT_MANUAL_DUTY_CYCLE);
        int cyclePercentage = readInputValue(currentDutyCycle * 100, 0, 100);
        currentDutyCycle = (float) cyclePercentage / 100;
        mySerial.print("currentDutyCycle set to: ");
        mySerial.println(currentDutyCycle);
    }

    display.clear();
    setDisplayStatusAccordingToMode();
    while (!digitalRead(SETTINGS_BUTTON)); // Prevent exiting this menu before settings button is released
    mySerial.println("leaving settings");
    delay(100);
}

unsigned long lastDmxPacket() {
#ifdef SIMULATE_DMX
    return random(1100);
#else
    return DMXSerial.noDataSince();
#endif
}

#ifndef SIMULATE_DMX

unsigned int readDmxChannel(const int *channel) {
    return DMXSerial.read(*channel);
}

#endif

int readDmxFrequencyChannel() {
#ifdef SIMULATE_DMX
    return random(9, 10);
#else
    return readDmxChannel(&dmxFrequencyChannel);
#endif
}

int readDmxDutyCycleChannel() {
#ifdef SIMULATE_DMX
    return random(128, 129);
#else
    return readDmxChannel(&dmxDutyCycleChannel);
#endif
}

bool dmxUpdated() {
#ifndef SIMULATE_DMX
    return DMXSerial.dataUpdated();
#else
    return random(10); // Probability 10% for not received
#endif
}

void loop() {
    if (digitalRead(SETTINGS_BUTTON) == LOW) {
        digitalWrite(TRIGGER, LOW);
        editSettings();
    }
    bool dmxIsActive = readDmx();

    // Switch to manual mode when no DMX was received for some time
    if (!dmxIsActive && dmxMode) {
        mySerial.println("Switching to manual mode");
        dmxMode = false;
        setDisplayStatusAccordingToMode();
        digitalWrite(TRIGGER, LOW);
        resetDmxUpdated();
    }

    // When manual mode is enabled, and we start receiving DMX again, we switch back to DMX mode
    if (dmxIsActive && !dmxMode) {
        mySerial.println("Switching to DMX mode");
        dmxMode = true;
        setDisplayStatusAccordingToMode();
    }

    // Turn output off immediately when one value is set to 0
    if (currentFrequencySek == 0 || currentDutyCycle == 0) {
        // Stop fogging if we do and stop loop execution
        digitalWrite(TRIGGER, LOW);
        display.setSegments(SEG_OFF, 3, 1);
        return;
    }

    pwm();
    delay(10);
}

void pwm() {
    unsigned long currentMillis = millis();
    // Check if new cycle needs to be started
    if (currentMillis > currentCycleEnd) {
        mySerial.println("Strating new cycle, fogging");
        nextTurnOffTime = currentMillis + (currentDutyCycle * currentFrequencySek * 1000l);
        currentCycleEnd = currentMillis + (currentFrequencySek * 1000l);
        needToTurnOff = true;
        setDisplayStatus(SEG_FOG);
        digitalWrite(TRIGGER, HIGH);
    }
    // Check if trigger needs to be turned off
    if (needToTurnOff && currentMillis > nextTurnOffTime) {
        mySerial.println("Fogging stop");
        setDisplayStatusAccordingToMode();
        digitalWrite(TRIGGER, LOW);
        needToTurnOff = false;
    }

    // Update display countdown
    int sekLeft;
    if (needToTurnOff) {
        sekLeft = (nextTurnOffTime - currentMillis) / 1000 + 1;
    } else {
        sekLeft = (currentCycleEnd - currentMillis) / 1000 + 1;
    }
    show3DigitNumber(sekLeft);
}

bool readDmx() {
    unsigned long lastPacket = lastDmxPacket();
    if (lastPacket < 5000) {
        if (dmxUpdated()) {
            dmxMode = true;
            // Check for frequency changes
            unsigned int newDmxValueFrequency = readDmxFrequencyChannel();
            if (newDmxValueFrequency != currentFrequencySek) {
                mySerial.print("New DMX frequency: ");
                mySerial.println(newDmxValueFrequency);
                currentFrequencySek = newDmxValueFrequency;
                // If interval length changed, reset current running interval and start over
                currentCycleEnd = 0;
                nextTurnOffTime = 0;
                needToTurnOff = false;
            }

            // Check for duty cycle changes
            unsigned int newDmxValueDutyCycle = readDmxDutyCycleChannel();
            if (newDmxValueDutyCycle != currentDutyCycleDmxValue) {
                mySerial.print("New DMX duty cycle: ");
                mySerial.println(newDmxValueDutyCycle);
                currentDutyCycleDmxValue = newDmxValueDutyCycle;
                currentDutyCycle = (float) newDmxValueDutyCycle / 255;
            }
            resetDmxUpdated();
        }
    } else {
        return false;
    }
    return true;
}

void resetDmxUpdated() {
#ifndef SIMULATE_DMX
    DMXSerial.resetUpdated();
#endif
}
