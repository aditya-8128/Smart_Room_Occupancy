//====================================================================
// ADVANCED ARDUINO UNO CODE
// Intelligent Occupancy-Based Room Automation System
// Features:
// - Non-blocking state machine for direction detection.
// - Non-blocking 7-segment display multiplexing using millis().
// - EEPROM persistence to save count during power cycles.
// - Multi-press override button functionality.
//====================================================================

#include <EEPROM.h>

// --- Pin Definitions ---
const int SENSOR_A_PIN = 2; // Outer sensor
const int SENSOR_B_PIN = 3; // Inner sensor
const int APPLIANCE_CTRL_PIN = A1; // Controls the transistor/relay
const int BUZZER_PIN = 11;
const int IR_TX_A_PIN = A2; // IR LED for Sensor A
const int IR_TX_B_PIN = A3; // IR LED for Sensor B
const int SEG_A = 4, SEG_B = 5, SEG_C = 6, SEG_D = 7, SEG_E = 8, SEG_F = 9, SEG_G = 10;
const int DIGIT1_CTRL = 12; // TENS digit
const int DIGIT2_CTRL = 13; // ONES digit
const int OVERRIDE_BUTTON_PIN = A0;

// --- System & State Variables ---
int person_count = 0;
bool in_shutdown_sequence = false;
unsigned long shutdown_start_time = 0;

// State machine variables
enum SensorState { IDLE, A_TRIGGERED, B_TRIGGERED };
SensorState current_state = IDLE;
unsigned long last_state_change_time = 0;
const unsigned long STATE_TIMEOUT = 1000; // 1 second timeout to reset state

// Display multiplexing variables
unsigned long last_display_switch_time = 0;
const int DISPLAY_REFRESH_INTERVAL = 5; // 5ms per digit
bool is_digit1_active = true;

// Override button variables
unsigned long override_press_start_time = 0;
int override_press_count = 0;
bool is_waiting_for_override = false;

// Array for segment pins
const int segmentPins[] = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G};
byte segment_map[10][7] = {
  { 1, 1, 1, 1, 1, 1, 0 }, // 0
  { 0, 1, 1, 0, 0, 0, 0 }, // 1
  { 1, 1, 0, 1, 1, 0, 1 }, // 2
  { 1, 1, 1, 1, 0, 0, 1 }, // 3
  { 0, 1, 1, 0, 0, 1, 1 }, // 4
  { 1, 0, 1, 1, 0, 1, 1 }, // 5
  { 1, 0, 1, 1, 1, 1, 1 }, // 6
  { 1, 1, 1, 0, 0, 0, 0 }, // 7
  { 1, 1, 1, 1, 1, 1, 1 }, // 8
  { 1, 1, 1, 1, 0, 1, 1 }  // 9
};

//======================== SETUP ========================
void setup() {
  Serial.begin(9600);
  Serial.println("Booting Standalone System...");

  pinMode(SENSOR_A_PIN, INPUT_PULLUP);
  pinMode(SENSOR_B_PIN, INPUT_PULLUP);
  pinMode(OVERRIDE_BUTTON_PIN, INPUT_PULLUP);

  pinMode(APPLIANCE_CTRL_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  for (int i = 0; i < 7; i++) { pinMode(segmentPins[i], OUTPUT); }
  pinMode(DIGIT1_CTRL, OUTPUT);
  pinMode(DIGIT2_CTRL, OUTPUT);
  pinMode(IR_TX_A_PIN, OUTPUT);
  pinMode(IR_TX_B_PIN, OUTPUT);
  
  tone(IR_TX_A_PIN, 38000);
  tone(IR_TX_B_PIN, 38000);

  digitalWrite(APPLIANCE_CTRL_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Read last saved count from EEPROM memory
  person_count = EEPROM.read(0);
  if (person_count > 99) person_count = 0; // Sanity check
  Serial.print("Restored count from EEPROM: ");
  Serial.println(person_count);
}

//======================== MAIN LOOP ========================
void loop() {
  updateDirection();
  manageSystemState();
  updateDisplay_nonBlocking();
}

//======================== FUNCTIONS ========================

void updateDirection() {
  bool sensor_a_low = (digitalRead(SENSOR_A_PIN) == LOW);
  bool sensor_b_low = (digitalRead(SENSOR_B_PIN) == LOW);

  switch (current_state) {
    case IDLE:
      if (sensor_a_low) {
        current_state = A_TRIGGERED;
        last_state_change_time = millis();
      } else if (sensor_b_low) {
        current_state = B_TRIGGERED;
        last_state_change_time = millis();
      }
      break;

    case A_TRIGGERED:
      if (sensor_b_low) {
        person_count++;
        updateEEPROM(person_count);
        Serial.print("Person Entered. Count: "); Serial.println(person_count);
        current_state = IDLE;
      } else if (millis() - last_state_change_time > STATE_TIMEOUT) {
        current_state = IDLE; // Timeout, false trigger
      }
      break;

    case B_TRIGGERED:
      if (sensor_a_low) {
        if (person_count > 0) {
          person_count--;
          updateEEPROM(person_count);
        }
        Serial.print("Person Exited. Count: "); Serial.println(person_count);
        current_state = IDLE;
      } else if (millis() - last_state_change_time > STATE_TIMEOUT) {
        current_state = IDLE; // Timeout, false trigger
      }
      break;
  }
}

void manageSystemState() {
  if (person_count > 0) {
    digitalWrite(APPLIANCE_CTRL_PIN, HIGH);
    if (in_shutdown_sequence) {
      in_shutdown_sequence = false;
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("Shutdown Cancelled.");
    }
  }

  if (person_count == 0 && digitalRead(APPLIANCE_CTRL_PIN) == HIGH && !in_shutdown_sequence) {
    in_shutdown_sequence = true;
    shutdown_start_time = millis();
    Serial.println("Shutdown Sequence Started...");
  }

  if (in_shutdown_sequence) {
    digitalWrite(BUZZER_PIN, HIGH);
    handleOverrideButton();

    if (millis() - shutdown_start_time > 10000) {
      digitalWrite(APPLIANCE_CTRL_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      in_shutdown_sequence = false;
      Serial.println("Appliances Turned OFF.");
    }
  }
}

void handleOverrideButton() {
  if (digitalRead(OVERRIDE_BUTTON_PIN) == LOW && !is_waiting_for_override) {
    is_waiting_for_override = true;
    override_press_count = 1;
    override_press_start_time = millis();
    Serial.println("Override sequence started... Enter count by pressing button.");
    delay(200); // Debounce
  }

  if (is_waiting_for_override) {
    if (digitalRead(OVERRIDE_BUTTON_PIN) == LOW) {
      override_press_count++;
      Serial.print("Button presses: "); Serial.println(override_press_count);
      delay(200); // Debounce
    }
    
    // If 3 seconds have passed since the first press, finalize the count
    if (millis() - override_press_start_time > 3000) {
      person_count = override_press_count;
      updateEEPROM(person_count);
      Serial.print("Override finished. New count set to: "); Serial.println(person_count);
      is_waiting_for_override = false;
      override_press_count = 0;
    }
  }
}

void updateDisplay_nonBlocking() {
  if (millis() - last_display_switch_time > DISPLAY_REFRESH_INTERVAL) {
    last_display_switch_time = millis();
    
    // Turn off both digits to prevent ghosting
    digitalWrite(DIGIT1_CTRL, HIGH);
    digitalWrite(DIGIT2_CTRL, HIGH);
    
    if (is_digit1_active) {
      // Display TENS digit
      int tens = (person_count / 10) % 10;
      setSegments(tens);
      digitalWrite(DIGIT1_CTRL, LOW);
    } else {
      // Display ONES digit
      int ones = person_count % 10;
      setSegments(ones);
      digitalWrite(DIGIT2_CTRL, LOW);
    }
    is_digit1_active = !is_digit1_active; // Flip to the other digit for the next cycle
  }
}

void setSegments(int digit) {
  if (digit < 0 || digit > 9) return;
  for (int i = 0; i < 7; i++) {
    digitalWrite(segmentPins[i], segment_map[digit][i]);
  }
}

void updateEEPROM(int count) {
  if (EEPROM.read(0) != count) {
    EEPROM.write(0, count);
    Serial.println("Count saved to EEPROM.");
  }
}
