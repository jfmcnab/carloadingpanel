/**
 * Car Loading Panel
 * 
 * MIT License
 * 
 * Copyright (c) 2025 McNab Media
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// =============== Libraries ===============
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h> 

// =============== Custom Characters ===============
const byte barGraphChars[8][8] PROGMEM = {
  { 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F },
  { 0x1F, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x1F },
  { 0x1F, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x1F },
  { 0x1F, 0x00, 0x1C, 0x1C, 0x1C, 0x1C, 0x00, 0x1F },
  { 0x1F, 0x00, 0x1E, 0x1E, 0x1E, 0x1E, 0x00, 0x1F },
  { 0x1F, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x00, 0x1F },
  { 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00 },
  { 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00 },
};

// =============== Constants and Pin Definitions ===============
const uint8_t BUTTON_PIN = A2;
const uint8_t LED_PIN = A3;

// Communication Pins
const uint8_t SFX_RX = 7;
const uint8_t SFX_TX = 8;

// Timing Constants (in milliseconds)
const unsigned long DEBOUNCE_DELAY = 100;
const unsigned long COMPLETE_DISPLAY_TIME = 5000;
const unsigned long LOADING_TIME = 96000; // 96 seconds in ms
const unsigned long INITIAL_SLEEP_ALERT_DELAY = 10000; // 10 seconds
const unsigned long SLEEP_ALERT_INTERVAL = 120000;     // 2 minutes
const unsigned long LED_BLINK_INTERVAL_COMPLETE = 100;
const unsigned long LED_BLINK_INTERVAL_LOADING = 1000;

// LED Pulsing Constants
const unsigned long PULSE_INTERVAL = 24;
const int PULSE_MIN = 0;
const int PULSE_MAX = 255;
const float PULSE_STEP = 5.0;

// =============== System State Definition ===============
enum SystemState {
  SLEEP,
  LOADING,
  COMPLETE,
  CANCELLED
};

// =============== Global Objects ===============
LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial sfxSerial(SFX_TX, SFX_RX);

// =============== System State Variables ===============
struct SystemVariables {
  SystemState currentState;
  bool lastButtonState;
  bool lcdOn;
  
  // Loading progress
  unsigned long loadStartTime;
  int lastFilledColumns;
  
  // State timing
  unsigned long stateStartTime;
  unsigned long lastDebounceTime;
  
  // LED control
  bool ledState;
  unsigned long lastLedToggle;
  unsigned long lastPulseUpdate;
  
  // Sleep state alerts
  unsigned long lastSleepAlertTime;
  bool initialSleepAlertDone;
} sysVars;

// =============== Function Declarations ===============
void initializeSystem();
void initializePins();
void initializeSerial();

// State handlers
void handleSleepState();
void handleLoadingState();
void handleCompleteState();
void handleCancelledState();

// System control
void enterSleepMode();
void startLoading();
void cancelLoading();
void enterCompleteState();

// Display functions
void updateLoadingDisplay(bool forceUpdate = false);

// LED Control
void updateLedPulsing();

// Utility functions
void playAlert(char key);
void loadBarGraphCharacters();

// =============== Main Setup and Loop ===============
void setup() {
  initializeSystem();
}

void loop() {
  switch (sysVars.currentState) {
    case SLEEP: handleSleepState(); break;
    case LOADING: handleLoadingState(); break;
    case COMPLETE: handleCompleteState(); break;
    case CANCELLED: handleCancelledState(); break;
  }
  updateLedPulsing();
}

// =============== Initialization Functions ===============
void initializeSystem() {
  initializePins();
  initializeSerial();

  lcd.init();
  randomSeed(analogRead(0));

  // Initialize system variables
  sysVars = {
    SLEEP,      // currentState
    HIGH,       // lastButtonState
    false,      // lcdOn
    0,          // loadStartTime
    0,          // lastFilledColumns
    0,          // stateStartTime
    0,          // lastDebounceTime
    false,      // ledState
    0,          // lastLedToggle
    0,          // lastPulseUpdate
    0,          // lastSleepAlertTime
    false       // initialSleepAlertDone
  };
}

void initializePins() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
}

void initializeSerial() {
  Serial.begin(9600);
  sfxSerial.begin(9600);
  sfxSerial.setTimeout(500);
}

// =============== LED Control Functions ===============
void updateLedPulsing() {
  unsigned long currentMillis = millis();

  if (currentMillis - sysVars.lastPulseUpdate >= PULSE_INTERVAL) {
    sysVars.lastPulseUpdate = currentMillis;
    
    if (sysVars.currentState == LOADING) {
      // Slowly blink LED during loading
      if (currentMillis - sysVars.lastLedToggle >= LED_BLINK_INTERVAL_LOADING) {
        sysVars.lastLedToggle = currentMillis;
        sysVars.ledState = !sysVars.ledState;
        digitalWrite(LED_PIN, sysVars.ledState ? HIGH : LOW);
      }
    } 
    else if (sysVars.currentState == COMPLETE) {
      // Blink LED rapidly during complete state
      if (currentMillis - sysVars.lastLedToggle >= LED_BLINK_INTERVAL_COMPLETE) {
        sysVars.lastLedToggle = currentMillis;
        sysVars.ledState = !sysVars.ledState;
        digitalWrite(LED_PIN, sysVars.ledState ? HIGH : LOW);
      }
    }
    else {
      // Turn LED off for other states
      digitalWrite(LED_PIN, LOW);
      sysVars.ledState = false;
    }
  }
}

// =============== State Handler Functions ===============
void handleSleepState() {
  unsigned long currentMillis = millis();
  
  // Check for button press to wake up
  bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
  if (buttonPressed && (currentMillis - sysVars.lastDebounceTime > DEBOUNCE_DELAY)) {
    sysVars.lastDebounceTime = currentMillis;
    sysVars.lastSleepAlertTime = 0; // Reset alert timer when waking up
    sysVars.initialSleepAlertDone = false;
    startLoading();
    return;
  }
  
  // Handle sleep state alerts
  if (!sysVars.initialSleepAlertDone) {
    // First alert after 30 seconds
    if (currentMillis - sysVars.stateStartTime >= INITIAL_SLEEP_ALERT_DELAY) {
      sysVars.lastSleepAlertTime = currentMillis;
      sysVars.initialSleepAlertDone = true;
    }
  } else {
    // Subsequent alerts every 2 minutes
    if (currentMillis - sysVars.lastSleepAlertTime >= SLEEP_ALERT_INTERVAL) {
      sysVars.lastSleepAlertTime = currentMillis;
    }
  }
}

void handleLoadingState() {
  unsigned long currentMillis = millis();
  
  // Check for button press to cancel
  if (digitalRead(BUTTON_PIN) == LOW && (currentMillis - sysVars.lastDebounceTime > DEBOUNCE_DELAY)) {
    sysVars.lastDebounceTime = currentMillis;
    cancelLoading();
    return;
  }
  
  // Check if loading is complete
  if (currentMillis - sysVars.loadStartTime >= LOADING_TIME) {
    enterCompleteState();
    return;
  }
  
  // Update the loading display
  updateLoadingDisplay(false);
}

void handleCompleteState() {
  // Check for button press to restart loading
  if (digitalRead(BUTTON_PIN) == LOW && (millis() - sysVars.lastDebounceTime > DEBOUNCE_DELAY)) {
    sysVars.lastDebounceTime = millis();
    startLoading();
    return;
  }
  
  // Check if complete display time has elapsed
  if (millis() - sysVars.stateStartTime > COMPLETE_DISPLAY_TIME) {
    enterSleepMode();
  }
}

void handleCancelledState() {
  // Check if cancelled display time has elapsed
  if (millis() - sysVars.stateStartTime > COMPLETE_DISPLAY_TIME) {
    enterSleepMode();
  }
}

// =============== System Control Functions ===============

void enterSleepMode() {
  sysVars.currentState = SLEEP;
  sysVars.lcdOn = false;
  digitalWrite(LED_PIN, LOW);
  lcd.noBacklight();
  lcd.clear();
}

void startLoading() {
  sysVars.currentState = LOADING;
  sysVars.loadStartTime = millis();
  sysVars.stateStartTime = millis();
  sysVars.lastFilledColumns = 0;
  
  lcd.backlight();
  lcd.clear();
  
  playAlert('L');
  updateLoadingDisplay(true);
}

void cancelLoading() {
  sysVars.currentState = CANCELLED;
  sysVars.stateStartTime = millis();
  
  lcd.setCursor(0, 0);
  lcd.print("LOADING CANCELED");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  
  playAlert('C');
}

void enterCompleteState() {
  sysVars.currentState = COMPLETE;
  sysVars.stateStartTime = millis();
  
  lcd.setCursor(0, 0);
  lcd.print("LOADING COMPLETE");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  
  playAlert('D');
}

// =============== Display Functions ===============
void updateLoadingDisplay(bool forceUpdate = false) {
  loadBarGraphCharacters();

  // Update the LCD display header
  lcd.setCursor(0, 0);
  lcd.print(" LOADING HOPPER ");
  
  // Calculate progress (0-100%)
  unsigned long elapsedTime = millis() - sysVars.loadStartTime;
  float percentComplete = (float)elapsedTime / LOADING_TIME;
  percentComplete = constrain(percentComplete, 0.0, 1.0);
  
  // Calculate how many columns should be filled (out of total available)
  int totalAvailableColumns = 14 * 5;
  int filledColumns = round(percentComplete * totalAvailableColumns);
  filledColumns = constrain(filledColumns, 0, totalAvailableColumns);
  
  // Only update if the number of filled columns has changed or forced
  if (filledColumns != sysVars.lastFilledColumns || forceUpdate) {
    sysVars.lastFilledColumns = filledColumns;
    
    // Create the bar graph string
    char bar[17];
    bar[0] = 6;    // Left bracket character
    
    for (int i = 1; i < 15; i++) {
      int columnsInThisChar = filledColumns - (i - 1) * 5;
      columnsInThisChar = constrain(columnsInThisChar, 0, 5);
      bar[i] = columnsInThisChar;
    }
    
    bar[15] = 7;   // Right bracket character
    bar[16] = '\0';

    // Update the progress bar
    lcd.setCursor(0, 1);
    for (int i = 0; i < 16; i++) {
      lcd.write(bar[i]);
    }
  }
}

// =============== Utility Functions ===============
void playAlert(char key) {
  sfxSerial.println("q");
  delay(50);
  String filename = "P" + String(key) + "       OGG";
  Serial.println(filename);
  sfxSerial.println(filename);
}

void loadBarGraphCharacters() {
  for (int i = 0; i < 8; i++) {
    byte charData[8];
    for (int j = 0; j < 8; j++) {
      charData[j] = pgm_read_byte(&(barGraphChars[i][j]));
    }
    lcd.createChar(i, charData);
  }
}