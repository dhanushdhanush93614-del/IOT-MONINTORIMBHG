```cpp id="k7z4qn"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

#include <PZEM004T.h>

// =====================================================
// WIFI
// =====================================================

const char* WIFI_SSID = "iPhone";
const char* WIFI_PASSWORD = "12345678";

// =====================================================
// MQTT
// =====================================================

const char* MQTT_HOST = "gf000f09.ala.us-east-1.emqxsl.com";

const int MQTT_PORT = 8883;

const char* MQTT_USER = "msechead@gmail.com";

const char* MQTT_PASS = "msec@123";

// =====================================================
// MQTT TOPICS
// =====================================================

const char* TOPIC_CONTROL = "msec/load/control";

const char* TOPIC_STATUS  = "msec/load/status";

const char* TOPIC_DATA    = "msec/load/data";

// =====================================================
// LCD
// =====================================================

hd44780_I2Cexp lcd;

// =====================================================
// RELAY
// =====================================================

const int RELAY_PIN = 23;

// =====================================================
// WIFI LED
// =====================================================

const int WIFI_LED = 2;

// =====================================================
// PZEM
// GPIO16 -> PZEM TX
// GPIO17 -> PZEM RX
// =====================================================

HardwareSerial PZEMSerial(2);

PZEM004T pzem(PZEMSerial);

// =====================================================
// MQTT
// =====================================================

WiFiClientSecure secureClient;

PubSubClient mqttClient(secureClient);

// =====================================================
// VARIABLES
// =====================================================

bool relayState = false;

unsigned long lastUpdate = 0;

// =====================================================
// MQTT CALLBACK
// =====================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {

  String msg = "";

  for (unsigned int i = 0; i < length; i++) {

    msg += (char)payload[i];
  }

  msg.trim();

  msg.toUpperCase();

  Serial.println("MQTT:");
  Serial.println(msg);

  if (msg == "ON") {

    relayState = true;

    digitalWrite(RELAY_PIN, HIGH);
  }
  else if (msg == "OFF") {

    relayState = false;

    digitalWrite(RELAY_PIN, LOW);
  }

  mqttClient.publish(
    TOPIC_STATUS,
    relayState ? "ON" : "OFF",
    true
  );
}

// =====================================================
// WIFI CONNECT
// =====================================================

void connectWiFi() {

  if (WiFi.status() == WL_CONNECTED)
    return;

  Serial.println("Connecting WiFi");

  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {

    digitalWrite(WIFI_LED, LOW);

    delay(500);

    Serial.print(".");
  }

  digitalWrite(WIFI_LED, HIGH);

  Serial.println("");
  Serial.println("WiFi Connected");

  Serial.println(WiFi.localIP());
}

// =====================================================
// MQTT CONNECT
// =====================================================

void connectMQTT() {

  while (!mqttClient.connected()) {

    Serial.println("Connecting MQTT");

    String clientId = "ESP32_LOAD_";

    clientId += String(random(0xffff), HEX);

    bool ok = mqttClient.connect(
      clientId.c_str(),
      MQTT_USER,
      MQTT_PASS
    );

    if (ok) {

      Serial.println("MQTT Connected");

      mqttClient.subscribe(TOPIC_CONTROL);

      mqttClient.publish(
        TOPIC_STATUS,
        relayState ? "ON" : "OFF",
        true
      );
    }
    else {

      Serial.println("MQTT Failed");

      delay(3000);
    }
  }
}

// =====================================================
// LCD UPDATE
// =====================================================

void updateLCD(
  float voltage,
  float current,
  float power,
  float energy
) {

  lcd.clear();

  // ROW 1
  lcd.setCursor(0,0);

  lcd.print("Voltage:");

  lcd.print(voltage,1);

  lcd.print("V");

  // ROW 2
  lcd.setCursor(0,1);

  lcd.print("Current:");

  lcd.print(current,3);

  lcd.print("A");

  // ROW 3
  lcd.setCursor(0,2);

  lcd.print("Power:");

  lcd.print(power,1);

  lcd.print("W");

  // ROW 4
  lcd.setCursor(0,3);

  lcd.print("Energy:");

  lcd.print(energy,3);

  lcd.print("kWh");
}

// =====================================================
// PUBLISH DATA
// =====================================================

void publishPZEMData() {

  float voltage = pzem.readVoltage();

  float current = pzem.readCurrent();

  float power = pzem.readPower();

  float energy = pzem.readEnergy();

  // SERIAL DEBUG
  Serial.println("====================");

  Serial.print("Voltage: ");
  Serial.print(voltage);
  Serial.println(" V");

  Serial.print("Current: ");
  Serial.print(current);
  Serial.println(" A");

  Serial.print("Power: ");
  Serial.print(power);
  Serial.println(" W");

  Serial.print("Energy: ");
  Serial.print(energy);
  Serial.println(" kWh");

  Serial.println("====================");

  // ERROR
  if (
    isnan(voltage) ||
    isnan(current) ||
    isnan(power) ||
    isnan(energy)
  ) {

    lcd.clear();

    lcd.setCursor(0,0);

    lcd.print("PZEM ERROR");

    lcd.setCursor(0,1);

    lcd.print("Check Wiring");

    return;
  }

  // JSON
  String json = "{";

  json += "\"voltage\":";
  json += String(voltage,1);
  json += ",";

  json += "\"current\":";
  json += String(current,3);
  json += ",";

  json += "\"power\":";
  json += String(power,1);
  json += ",";

  json += "\"energy\":";
  json += String(energy,3);

  json += "}";

  // MQTT PUBLISH
  mqttClient.publish(
    TOPIC_DATA,
    json.c_str(),
    true
  );

  // LCD
  updateLCD(
    voltage,
    current,
    power,
    energy
  );
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  Serial.println("");
  Serial.println("SMART LOAD MQTT");

  // RELAY
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);

  // WIFI LED
  pinMode(WIFI_LED, OUTPUT);

  digitalWrite(WIFI_LED, LOW);

  // LCD
  Wire.begin(21,22);

  lcd.begin(20,4);

  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0,0);

  lcd.print("Smart Load");

  lcd.setCursor(0,1);

  lcd.print("Monitoring");

  delay(2000);

  // PZEM UART
  PZEMSerial.begin(
    9600,
    SERIAL_8N1,
    16,
    17
  );

  delay(2000);

  // WIFI
  connectWiFi();

  // MQTT SSL
  secureClient.setInsecure();

  mqttClient.setServer(
    MQTT_HOST,
    MQTT_PORT
  );

  mqttClient.setCallback(mqttCallback);

  // MQTT
  connectMQTT();
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // WIFI CHECK
  if (WiFi.status() != WL_CONNECTED) {

    digitalWrite(WIFI_LED, LOW);

    connectWiFi();
  }

  // MQTT CHECK
  if (!mqttClient.connected()) {

    connectMQTT();
  }

  mqttClient.loop();

  // UPDATE
  if (millis() - lastUpdate > 3000) {

    lastUpdate = millis();

    publishPZEMData();
  }
}
```
