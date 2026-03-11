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

// Capteurs machine de traction
#define PRESSURE_PIN 35
#define LVDT_PIN 34

unsigned long lastPublish = 0;
const long interval = 100;

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

void reconnect() {

  while (!client.connected()) {

    Serial.print("Connexion MQTT...");

    if (client.connect("ESP32Traction")) {

      Serial.println("connecté");

    } else {

      Serial.print("Erreur MQTT : ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void publishData() {

  Serial.println("");
  Serial.println("===== LECTURE CAPTEURS =====");

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  int ldrRaw = analogRead(LDR_PIN);
  int ldrPercent = map(ldrRaw, 0, 4095, 0, 100);

  int pressureRaw = analogRead(PRESSURE_PIN);
  int lvdtRaw = analogRead(LVDT_PIN);

  float pressure_bar = map(pressureRaw, 0, 4095, 0, 10);
  float lvdt_mm = map(lvdtRaw, 0, 4095, 0, 50);

  // PRINTS CAPTEURS

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

  // MQTT conversion
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

  // MQTT publish

  client.publish("esp32/temp", tempStr);
  client.publish("esp32/hum", humStr);
  client.publish("esp32/LDR", ldrStr);
  client.publish("esp32/pressure", pressureStr);
  client.publish("esp32/lvdt", lvdtStr);

  Serial.println("Données envoyées via MQTT !");
}

void setup() {

  Serial.begin(115200);

  Serial.println("Demarrage ESP32...");

  dht.begin();

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
}

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