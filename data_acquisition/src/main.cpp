#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_INA228.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

// WIFI
const char* ssid = "DESKTOP-TRAC";
const char* password = "51k85,D9";

// MQTT
const char* mqtt_server = "192.168.137.1";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// INA228
Adafruit_INA228 ina228;

// LCD I2C
hd44780_I2Cexp lcd(0x27);

// DHT22
#define DHTPIN 26
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// LDR
#define LDR_PIN 39
#define MOSFET_PIN 14
#define LED_VERTE 15
#define LED_ROUGE 2

#define BUZZER_PIN 27
#define LED_ALARME 12
#define CHANNEL 0

#define BTN_RESET 33

// LVDT
#define LVDT_PIN 34

unsigned long lastPublish = 0;
const long interval = 1000;

float previousPressure = 0;
unsigned long dropTime = 0;
bool dropDetected = false;

float lvdt_max = 0;

// -------- FILTRE ADC --------
int readADC(int pin){

  int sum = 0;

  for(int i = 0; i < 10; i++){
    sum += analogRead(pin);
    delay(2);
  }

  return sum / 10;
}

// -------- WIFI --------
void setup_wifi() {

  Serial.println("Connexion WiFi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connecté !");
}

void updateLEDStatus(String state) {

  if (state == "connected") {

    digitalWrite(LED_VERTE, HIGH);
    digitalWrite(LED_ROUGE, LOW);

  }
  else if (state == "disconnected") {

    digitalWrite(LED_VERTE, LOW);
    digitalWrite(LED_ROUGE, HIGH);

  }
  else if (state == "waiting") {

    digitalWrite(LED_VERTE, LOW);
    digitalWrite(LED_ROUGE, LOW);
  }
}

// -------- MQTT --------
void reconnect() {

  while (!client.connected()) {

    updateLEDStatus("waiting");

    Serial.print("Connexion MQTT...");

    if (client.connect("ESP32Traction")) {

      Serial.println("connecté");

      client.subscribe("machine/control");

      updateLEDStatus("connected");

    } else {

      Serial.print("Erreur MQTT : ");
      Serial.println(client.state());

      updateLEDStatus("disconnected");

      delay(2000);
    }
  }
}

// -------- LECTURE CAPTEURS --------
void publishData() {

  Serial.println("");
  Serial.println("===== LECTURE CAPTEURS =====");

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  int ldrRaw = readADC(LDR_PIN);
  int ldrPercent = map(ldrRaw, 0, 4095, 0, 100);

  // -------- PRESSION VIA INA228 --------
  float tension = ina228.readBusVoltage();

  // Garde la même variable MQTT
  float pressure_bar = ((tension / 10.0) * 300.0) + 0.505;

  // -------- LVDT --------
  int lvdtRaw = readADC(LVDT_PIN);

  float lvdt_mm = (lvdtRaw / 4095.0) * 50.0;
  // ===== LCD =====

  // Ligne 1
  lcd.setCursor(0,0);

  lcd.print("T:");
  lcd.print(temperature,1);

  lcd.print(" H:");
  lcd.print(humidity,0);

  lcd.print("   ");

  // Ligne 2
  lcd.setCursor(0,1);

  lcd.print("P:");
  lcd.print(pressure_bar,0);

  lcd.print(" L:");
  lcd.print(lvdt_mm,1);

  lcd.print("   ");

  if (lvdt_mm > lvdt_max) {
    lvdt_max = lvdt_mm;
  }

  Serial.print("Pressure actuelle : ");
  Serial.println(pressure_bar);

  Serial.print("Pressure precedente : ");
  Serial.println(previousPressure);

  if (pressure_bar < (previousPressure - 10) && !dropDetected) {

    dropTime = millis();
    dropDetected = true;

    Serial.println("↓↓↓↓ PRESSION EN DIMINUTION DETECTEE ↓↓↓↓");
  }

  previousPressure = pressure_bar;

  // -------- SECURITE LVDT --------
  if (lvdt_mm >= 50.0) {

    Serial.println("!!! LIMITE LVDT ATTEINTE - ARRET MACHINE !!!");

    digitalWrite(MOSFET_PIN, LOW);

    for (int f = 800; f <= 2500; f += 20) {

      ledcWriteTone(CHANNEL, f);

      digitalWrite(LED_ALARME, HIGH);

      delay(10);
    }

    for (int f = 2500; f >= 800; f -= 20) {

      ledcWriteTone(CHANNEL, f);

      digitalWrite(LED_ALARME, LOW);

      delay(10);
    }

  } else {

    ledcWriteTone(CHANNEL, 0);

    digitalWrite(LED_ALARME, LOW);
  }

  // -------- SERIAL --------
  Serial.print("Temperature : ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humidite : ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("LDR brut : ");
  Serial.println(ldrRaw);

  Serial.print("LDR % : ");
  Serial.print(ldrPercent);
  Serial.println(" %");

  Serial.print("Tension INA228 : ");
  Serial.print(tension);
  Serial.println(" V");

  Serial.print("Pressure bar : ");
  Serial.print(pressure_bar);
  Serial.println(" bar");

  Serial.print("LVDT raw : ");
  Serial.println(lvdtRaw);

  Serial.print("LVDT mm : ");
  Serial.print(lvdt_mm);
  Serial.println(" mm");

  Serial.println("============================");

  // -------- MQTT --------
  char tempStr[10];
  char humStr[10];
  char ldrStr[10];
  char pressureStr[10];
  char lvdtStr[10];

  dtostrf(temperature, 1, 2, tempStr);
  dtostrf(humidity, 1, 2, humStr);
  dtostrf(pressure_bar, 1, 2, pressureStr);
  dtostrf(lvdt_max, 1, 2, lvdtStr);

  itoa(ldrPercent, ldrStr, 10);

  client.publish("esp32/temp", tempStr);
  client.publish("esp32/hum", humStr);
  client.publish("esp32/LDR", ldrStr);

  // MEME TOPIC MQTT
  client.publish("esp32/pressure", pressureStr);

  client.publish("esp32/lvdt", lvdtStr);

  Serial.println("Données envoyées via MQTT !");
}

void callback(char* topic, byte* payload, unsigned int length) {

  String message = "";

  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message reçu : ");
  Serial.println(message);

  if (String(topic) == "machine/control") {

    if (message == "start") {

      digitalWrite(MOSFET_PIN, HIGH);

      dropDetected = false;

      previousPressure = 0;

      lvdt_max = 0;
    }

    if (message == "stop") {

      digitalWrite(MOSFET_PIN, LOW);
    }
  }
}

// -------- SETUP --------
void setup() {

  Serial.begin(115200);

  Serial.println("Demarrage ESP32...");

  dht.begin();

  // I2C
  Wire.begin(21, 22);

  // ===== LCD =====
  lcd.begin(16,2);

  lcd.setCursor(0,0);
  lcd.print("SYSTEM READY");

  delay(2000);

  lcd.clear();

  // ===== INA228 =====
  if (!ina228.begin()) {

    Serial.println("INA228 non detecte");

    lcd.setCursor(0,0);
    lcd.print("INA228 ERROR");

    while (1);
  }

  Serial.println("INA228 detecte !");

  lcd.setCursor(0,0);
  lcd.print("INA228 OK");

  delay(1000);

  lcd.clear();

  // configuration ADC ESP32
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  pinMode(LED_VERTE, OUTPUT);
  pinMode(LED_ROUGE, OUTPUT);

  digitalWrite(LED_VERTE, LOW);
  digitalWrite(LED_ROUGE, HIGH);

  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, LOW);

  pinMode(LED_ALARME, OUTPUT);

  pinMode(BTN_RESET, INPUT_PULLUP);

  ledcSetup(CHANNEL, 2000, 8);
  ledcAttachPin(BUZZER_PIN, CHANNEL);
}

// -------- LOOP --------
void loop() {

  // BYPASS TOTAL
  if (digitalRead(BTN_RESET) == LOW) {

    digitalWrite(MOSFET_PIN, HIGH);

    ledcWriteTone(CHANNEL, 0);

    digitalWrite(LED_ALARME, LOW);

    dropDetected = false;

    previousPressure = 0;

    return;
  }

  if (!client.connected())
    reconnect();

  client.loop();

  unsigned long now = millis();

  if (dropDetected) {

    Serial.print("Attente arret... temps écoulé = ");
    Serial.println(millis() - dropTime);
  }

  if (dropDetected && millis() - dropTime >= 2000) {

    Serial.println("🔥🔥🔥 ARRET MACHINE 🔥🔥🔥");

    digitalWrite(MOSFET_PIN, LOW);

    dropDetected = false;
  }

  if (now - lastPublish > interval) {

    lastPublish = now;

    publishData();
  }
}