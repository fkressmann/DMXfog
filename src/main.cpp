#define debug

#include <Arduino.h>
#include <EEPROM.h>
#include <TM1637Display.h>

#ifndef debug

#include <DMXSerial.h>

#endif

#define DISPLAY_CLK 6
#define DISPLAY_DIO 5
#define SETTINGS_BUTTON 4 // Pin 2 and 3 are usable for interrupts
#define UP_BUTTON 2
#define DOWN_BUTTON 3
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

void pwm();

void readDmx();

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
#ifdef debug
    Serial.begin(115200);
    Serial.println("Starting");
#else
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
    delay(2000);
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
}

unsigned long lastDmxPacket() {
#ifdef debug
    return random(1100);
#else
    return DMXSerial.noDataSince();
#endif
}

#ifndef debug

unsigned int readDmxChannel(const int *channel) {
    return DMXSerial.read(*channel);
}

#endif

int readDmxFrequencyChannel() {
#ifdef debug
    return random(9, 10);
#else
    return readDmxChannel(&dmxFrequencyChannel);
#endif
}

int readDmxDutyCycleChannel() {
#ifdef debug
    return random(128, 129);
#else
    return readDmxChannel(&dmxDutyCycleChannel);
#endif
}

bool dmxUpdated() {
#ifndef debug
    return DMXSerial.dataUpdated();
#else
    return random(10); // Probability 10% for not received
#endif
}

void resetDmxUpdated() {
#ifndef debug
    DMXSerial.resetUpdated();
#endif
}

void loop() {
    if (digitalRead(SETTINGS_BUTTON) == LOW) editSettings();
    pwm();
    delay(100);
}

void pwm() {
    readDmx();

    unsigned long currentMillis = millis();
    // Check if new cycle needs to be started
    if (currentMillis > currentCycleEnd) {
#ifdef debug
        Serial.println("Strating new cycle, fogging");
#endif
        currentCycleEnd = currentMillis + (currentFrequencySek * 1000);
        nextTurnOffTime = currentMillis + (currentDutyCycle * currentFrequencySek * 1000);
        needToTurnOff = true;
        setDisplayStatus(SEG_FOG);
        digitalWrite(TRIGGER, HIGH);
    }
    // Check if trigger needs to be tunred off
    if (needToTurnOff && currentMillis > nextTurnOffTime) {
#ifdef debug
        Serial.println("Fogging stop");
#endif
        setDisplayStatus(SEG_EMPTY);
        digitalWrite(TRIGGER, LOW);
        needToTurnOff = false;
    }
}

void readDmx() {
    if (dmxUpdated()) {
        noDmx = false;
        // Check for frequency changes
        unsigned int newDmxValueFrequency = readDmxFrequencyChannel();
        if (newDmxValueFrequency != currentFrequencySek) {
#ifdef debug
            Serial.print("New frequency: ");
            Serial.println(newDmxValueFrequency);
#endif
            currentFrequencySek = newDmxValueFrequency;
            // If interval length changed, reset current running interval and start over
            currentCycleEnd = 0;
            nextTurnOffTime = 0;
        }

        // Check for duty cycle changes
        unsigned int newDmxValueDutyCycle = readDmxDutyCycleChannel();
        if (newDmxValueDutyCycle != currentDutyCycleDmxValue) {
#ifdef debug
            Serial.print("New duty cycle: ");
            Serial.println(newDmxValueDutyCycle);
#endif
            currentDutyCycleDmxValue = newDmxValueDutyCycle;
            currentDutyCycle = (float) newDmxValueDutyCycle / 255;
        }
        resetDmxUpdated();
    } else {
        unsigned long lastPacket = lastDmxPacket();
#ifdef debug
        Serial.print("DMX not updated since ");
        Serial.println(lastPacket);
#endif
        if (lastPacket > 1000 && !noDmx) {
#ifdef debug
            Serial.println("Setting F");
#endif
            setDisplayStatus(SEG_NO_DMX);
            noDmx = true;
        }
    }
}
