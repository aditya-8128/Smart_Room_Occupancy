//====================================================================
// ESP32 IOT CODE
// Intelligent Occupancy-Based Room Automation System
// Features:
// - All advanced features from the Uno version.
// - Wi-Fi Connectivity with auto-reconnect.
// - MQTT client to publish data and receive commands.
// - Over-the-Air (OTA) update functionality.
//====================================================================

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

// --- Wi-Fi & MQTT Configuration ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "your_mqtt_broker_ip"; // e.g., "192.168.1.100" or broker.hivemq.com
const int mqtt_port = 1883;

// MQTT Topics
const char* topic_count = "room/occupancy/count";
const char* topic_status = "room/occupancy/status";
const char* topic_light_command = "room/light/command";
const char* topic_light_state = "room/light/state";

WiFiClient espClient;
PubSubClient client(espClient);

// --- Pin Definitions (ESP32 compatible) ---
const int SENSOR_A_PIN = 2; // Outer sensor
const int SENSOR_B_PIN = 4; // Inner sensor (Pin 3 is often strapping pin)
const int APPLIANCE_CTRL_PIN = 5;
const int BUZZER_PIN = 18;
const int IR_TX_A_PIN = 19;
const int IR_TX_B_PIN = 21;
const int SEG_A = 22, SEG_B = 23, SEG_C = 25, SEG_D = 26, SEG_E = 27, SEG_F = 32, SEG_G = 33;
const int DIGIT1_CTRL = 12;
const int DIGIT2_CTRL = 13;
const int OVERRIDE_BUTTON_PIN = 14;

// --- System & State Variables (Same as Uno version) ---
int person_count = 0;
// (All other global variables from Uno version are also needed here)
bool in_shutdown_sequence = false;
unsigned long shutdown_start_time = 0;
enum SensorState { IDLE, A_TRIGGERED, B_TRIGGERED };
SensorState current_state = IDLE;
unsigned long last_state_change_time = 0;
const unsigned long STATE_TIMEOUT = 1000;
unsigned long last_display_switch_time = 0;
const int DISPLAY_REFRESH_INTERVAL = 5;
bool is_digit1_active = true;

// 7-Segment and Pin arrays (Same as Uno version)
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
  Serial.begin(115200);
  Serial.println("Booting IoT System...");

  EEPROM.begin(1); // Allocate 1 byte for count
  
  // Setup pins (same as Uno version)
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

  // ESP32 tone() setup (using ledc channels)
  ledcSetup(0, 38000, 8); // Channel 0, 38kHz, 8-bit resolution
  ledcAttachPin(IR_TX_A_PIN, 0);
  ledcWrite(0, 128); // 50% duty cycle

  ledcSetup(1, 38000, 8); // Channel 1, 38kHz, 8-bit resolution
  ledcAttachPin(IR_TX_B_PIN, 1);
  ledcWrite(1, 128); // 50% duty cycle

  digitalWrite(APPLIANCE_CTRL_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  person_count = EEPROM.read(0);
  if (person_count > 99) person_count = 0;
  Serial.print("Restored count from EEPROM: "); Serial.println(person_count);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqtt_callback);

  setup_ota();
}

//======================== MAIN LOOP ========================
void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();
  ArduinoOTA.handle();
  
  updateDirection();
  manageSystemState();
  updateDisplay_nonBlocking();
}

//======================== IOT FUNCTIONS ========================

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(topic_light_command);
      publishAllData();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) == topic_light_command) {
    if (message == "ON") {
      digitalWrite(APPLIANCE_CTRL_PIN, HIGH);
      client.publish(topic_light_state, "ON");
    } else if (message == "OFF") {
      digitalWrite(APPLIANCE_CTRL_PIN, LOW);
      client.publish(topic_light_state, "OFF");
    }
  }
}

void publishAllData() {
  char count_str[3];
  itoa(person_count, count_str, 10);
  client.publish(topic_count, count_str);
  client.publish(topic_status, person_count > 0 ? "Occupied" : "Empty");
  client.publish(topic_light_state, digitalRead(APPLIANCE_CTRL_PIN) ? "ON" : "OFF");
}

void setup_ota() {
  ArduinoOTA.onStart([]() { Serial.println("OTA Start!"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA End"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}


//======================== CORE LOGIC FUNCTIONS ========================
// NOTE: The following functions are identical to the Uno version, 
// but are included here to make the code complete.

void updateDirection() {
    // This function is the same as the UNO version, but checks for pin changes
    // and calls publishAllData() if person_count changes.
    int old_count = person_count;

    bool sensor_a_low = (digitalRead(SENSOR_A_PIN) == LOW);
    bool sensor_b_low = (digitalRead(SENSOR_B_PIN) == LOW);

    switch (current_state) {
      case IDLE:
        if (sensor_a_low) { current_state = A_TRIGGERED; last_state_change_time = millis(); }
        else if (sensor_b_low) { current_state = B_TRIGGERED; last_state_change_time = millis(); }
        break;
      case A_TRIGGERED:
        if (sensor_b_low) { person_count++; current_state = IDLE; }
        else if (millis() - last_state_change_time > STATE_TIMEOUT) { current_state = IDLE; }
        break;
      case B_TRIGGERED:
        if (sensor_a_low) { if (person_count > 0) person_count--; current_state = IDLE; }
        else if (millis() - last_state_change_time > STATE_TIMEOUT) { current_state = IDLE; }
        break;
    }
    
    if (old_count != person_count) {
      Serial.print("Count changed. New Count: "); Serial.println(person_count);
      updateEEPROM(person_count);
      publishAllData();
    }
}

void manageSystemState() {
  int old_light_state = digitalRead(APPLIANCE_CTRL_PIN);

  if (person_count > 0) {
    digitalWrite(APPLIANCE_CTRL_PIN, HIGH);
    if (in_shutdown_sequence) { in_shutdown_sequence = false; digitalWrite(BUZZER_PIN, LOW); }
  }
  if (person_count == 0 && digitalRead(APPLIANCE_CTRL_PIN) == HIGH && !in_shutdown_sequence) {
    in_shutdown_sequence = true; shutdown_start_time = millis();
  }
  if (in_shutdown_sequence) {
    digitalWrite(BUZZER_PIN, HIGH);
    if (digitalRead(OVERRIDE_BUTTON_PIN) == LOW) {
      person_count = 1; 
      // This will cause the logic at the top to cancel shutdown on the next loop
    }
    if (millis() - shutdown_start_time > 10000) {
      digitalWrite(APPLIANCE_CTRL_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      in_shutdown_sequence = false;
    }
  }

  if (old_light_state != digitalRead(APPLIANCE_CTRL_PIN)) {
    publishAllData();
  }
}

void updateDisplay_nonBlocking() {
  if (millis() - last_display_switch_time > DISPLAY_REFRESH_INTERVAL) {
    last_display_switch_time = millis();
    digitalWrite(DIGIT1_CTRL, HIGH); digitalWrite(DIGIT2_CTRL, HIGH);
    if (is_digit1_active) {
      setSegments((person_count / 10) % 10); digitalWrite(DIGIT1_CTRL, LOW);
    } else {
      setSegments(person_count % 10); digitalWrite(DIGIT2_CTRL, LOW);
    }
    is_digit1_active = !is_digit1_active;
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
    EEPROM.commit(); // Necessary for ESP32
    Serial.println("Count saved to EEPROM.");
  }
}
