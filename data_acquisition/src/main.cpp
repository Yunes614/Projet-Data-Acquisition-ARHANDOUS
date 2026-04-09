#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// WIFI
const char* ssid = "DESKTOP-TRAC";
const char* password = "51k85,D9";

// MQTT
const char* mqtt_server = "192.168.137.1";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// DHT22
#define DHTPIN 26
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// LDR
#define LDR_PIN 39
#define MOSFET_PIN 14

// Capteurs traction
#define PRESSURE_PIN 35
#define LVDT_PIN 34

unsigned long lastPublish = 0;
const long interval = 1000;

// -------- FILTRE ADC --------

int readADC(int pin){
  int sum = 0;

  for(int i=0;i<10;i++){
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

// -------- MQTT --------

void reconnect() {

  while (!client.connected()) {

    Serial.print("Connexion MQTT...");

    if (client.connect("ESP32Traction")) {

      Serial.println("connecté");
      client.subscribe("machine/control");

    } else {

      Serial.print("Erreur MQTT : ");
      Serial.println(client.state());
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

  int pressureRaw = readADC(PRESSURE_PIN);
  int lvdtRaw = readADC(LVDT_PIN);

  // conversion capteurs
  float pressure_bar = (pressureRaw / 4095.0) * 30.0;
  float lvdt_mm = (lvdtRaw / 4095.0) * 50.0;

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

  Serial.print("Pressure raw : ");
  Serial.println(pressureRaw);

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

  dtostrf(temperature,1,2,tempStr);
  dtostrf(humidity,1,2,humStr);
  dtostrf(pressure_bar,1,2,pressureStr);
  dtostrf(lvdt_mm,1,2,lvdtStr);

  itoa(ldrPercent, ldrStr, 10);

  client.publish("esp32/temp", tempStr);
  client.publish("esp32/hum", humStr);
  client.publish("esp32/LDR", ldrStr);
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

  // configuration ADC ESP32
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, LOW); // machine OFF au début
}

// -------- LOOP --------

void loop() {

  if (!client.connected())
    reconnect();

  client.loop();

  unsigned long now = millis();

  if (now - lastPublish > interval) {

    lastPublish = now;

    publishData();
  }
}