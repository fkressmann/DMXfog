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

unsigned long currentCycleEnd = 0;
unsigned long nextTurnOffTime = 0;
bool noDmx = false;
bool needToTurnOff = false;
bool upButtonPressed = false;
bool downButtonPressed = false;

const uint8_t SEG_EDIT[] = {SEG_A | SEG_D | SEG_E | SEG_F | SEG_G};
const uint8_t SEG_FOG[] = {SEG_A | SEG_E | SEG_F | SEG_G};
const uint8_t SEG_NO_DMX[] = {SEG_F | SEG_G | SEG_C};
const uint8_t SEG_EMPTY[] = {0};

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);
SendOnlySoftwareSerial mySerial(SERIAL);

void pwm();

void readDmx();

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
#ifndef SIMULATE_DMX
    DMXSerial.init(DMXReceiver, 12);
#endif
    display.setBrightness(0x0f);
    pinMode(SETTINGS_BUTTON, INPUT_PULLUP);
    pinMode(UP_BUTTON, INPUT_PULLUP);
    pinMode(DOWN_BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(UP_BUTTON), isrUp, CHANGE);
    attachInterrupt(digitalPinToInterrupt(DOWN_BUTTON), isrDown, CHANGE);
    pinMode(TRIGGER, OUTPUT);
    EEPROM.get(0, dmxStartChannel);
    display.showNumberDec(dmxStartChannel);
    updateChannels();
}

void setDisplayStatus(const uint8_t segments[]) {
    display.setSegments(segments, 1, 0);
}

void editSettings() {
    delay(100);
    while (!digitalRead(SETTINGS_BUTTON)); // Prevent exiting this menu before settings button is released
    setDisplayStatus(SEG_EDIT);
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
    EEPROM.put(0, dmxStartChannel);
    updateChannels();
    display.showNumberDec(dmxStartChannel);
    while (!digitalRead(SETTINGS_BUTTON)); // Prevent exiting this menu before settings button is released
    delay(100);
    resetDmxUpdated();
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
    pwm();
    delay(10);
}

void pwm() {
    readDmx();

    if (noDmx || currentFrequencySek == 0 || currentDutyCycleDmxValue == 0) {
        // Stop fogging if we do and stop loop execution
        digitalWrite(TRIGGER, LOW);
        return;
    }

    unsigned long currentMillis = millis();
    // Check if new cycle needs to be started
    if (currentMillis > currentCycleEnd) {
        mySerial.println("Strating new cycle, fogging");
        currentCycleEnd = currentMillis + (currentFrequencySek * 1000l);
        nextTurnOffTime = currentMillis + (currentDutyCycle * currentFrequencySek * 1000l);
        needToTurnOff = true;
        setDisplayStatus(SEG_FOG);
        digitalWrite(TRIGGER, HIGH);
    }
    // Check if trigger needs to be tunred off
    if (needToTurnOff && currentMillis > nextTurnOffTime) {
        mySerial.println("Fogging stop");
        setDisplayStatus(SEG_EMPTY);
        digitalWrite(TRIGGER, LOW);
        needToTurnOff = false;
    }
}

void readDmx() {
    unsigned long lastPacket = lastDmxPacket();
    if (lastPacket < 5000) {
        if (dmxUpdated()) {
            noDmx = false;
            // Check for frequency changes
            unsigned int newDmxValueFrequency = readDmxFrequencyChannel();
            if (newDmxValueFrequency != currentFrequencySek) {
                mySerial.print("New frequency: ");
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
                mySerial.print("New duty cycle: ");
                mySerial.println(newDmxValueDutyCycle);
                currentDutyCycleDmxValue = newDmxValueDutyCycle;
                currentDutyCycle = (float) newDmxValueDutyCycle / 255;
            }
            resetDmxUpdated();
        }
    } else if (!noDmx){
        mySerial.println(lastPacket);
        mySerial.println("Setting F");
        setDisplayStatus(SEG_NO_DMX);
        noDmx = true;
        resetDmxUpdated();
    }
}

void resetDmxUpdated() {
#ifndef SIMULATE_DMX
    DMXSerial.resetUpdated();
#endif
}
