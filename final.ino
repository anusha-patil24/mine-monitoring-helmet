#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

//////////////////// WIFI ////////////////////
const char* ssid = "v27";
const char* pass = "hrut1111";

//////////////////// MQTT ////////////////////
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* topic = "bharatpi/hruthik/v27/mine/monitor";

//////////////////// LCD ////////////////////
LiquidCrystal_I2C lcd(0x27, 16, 2);

//////////////////// SENSORS ////////////////////
#define MQ_PIN 34
#define FLAME_PIN 14
#define DHT_PIN 27
#define BUZZER_PIN 25

DHT dht(DHT_PIN, DHT11);

//////////////////// GPS ////////////////////
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

//////////////////// LIMITS ////////////////////
#define TEMP_LIMIT 45
#define HUM_LIMIT 80
#define GAS_LIMIT 3000

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastPublish = 0;

//////////////////////////////////////////////////

void connectWiFi() {
  WiFi.begin(ssid, pass);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.println(WiFi.localIP());
}

//////////////////////////////////////////////////

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");
    String clientId = "ESP32-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("Connected!");
    } else {
      Serial.print("Failed rc=");
      Serial.print(client.state());
      Serial.println(" retrying...");
      delay(3000);
    }
  }
}

//////////////////////////////////////////////////

void sendData() {

  int gas = analogRead(MQ_PIN);
  bool flame = digitalRead(FLAME_PIN) == LOW;

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  // FIX DHT SAFELY
  if (isnan(t) || isnan(h)) {
    Serial.println("DHT Read Failed!");
    t = 0.0;
    h = 0.0;
  }

  // GPS
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  float lat = 0.0, lng = 0.0;
  if (gps.location.isValid()) {
    lat = gps.location.lat();
    lng = gps.location.lng();
  }

  // Safety
  bool danger = (t > TEMP_LIMIT || h > HUM_LIMIT || gas > GAS_LIMIT || flame);
  digitalWrite(BUZZER_PIN, danger ? HIGH : LOW);

  // Oxygen (simulated at 21%)
  float oxygen = 21.0;

  // VALID JSON
  String payload = "{";
  payload += "\"temp\":" + String(t,1) + ",";
  payload += "\"hum\":" + String(h,1) + ",";
  payload += "\"gas\":" + String(gas) + ",";
  payload += "\"oxygen\":" + String(oxygen,1) + ",";   // ✅ added oxygen
  payload += "\"flame\":" + String(flame ? 1 : 0) + ",";
  payload += "\"lat\":" + String(lat,6) + ",";
  payload += "\"lng\":" + String(lng,6);
  payload += "}";

  Serial.println(payload);

  if (client.publish(topic, payload.c_str())) {
    Serial.println("MQTT Published OK");
  } else {
    Serial.println("MQTT Publish Failed");
  }

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("T:");
  lcd.print(t);
  lcd.print(" H:");
  lcd.print(h);

  lcd.setCursor(0,1);
  lcd.print("Gas:");
  lcd.print(gas);
  lcd.print(" O2:");
  lcd.print(oxygen);
}

//////////////////////////////////////////////////

void setup() {

  Serial.begin(115200);

  lcd.init();
  lcd.backlight();

  dht.begin();

  pinMode(FLAME_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

  connectWiFi();

  client.setServer(mqtt_server, mqtt_port);
}

//////////////////////////////////////////////////

void loop() {

  if (WiFi.status() != WL_CONNECTED)
    connectWiFi();

  if (!client.connected())
    reconnectMQTT();

  client.loop();

  if (millis() - lastPublish > 3000) {
    lastPublish = millis();
    sendData();
  }
}