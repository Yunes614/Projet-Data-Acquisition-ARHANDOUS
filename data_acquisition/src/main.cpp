#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
<<<<<<< HEAD
#include <Update.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_NeoPixel.h>

// DHT Sensor Configuration
#define DHTPIN 26      // Pin connecté au DHT11
#define DHTTYPE DHT22  // Type de capteur DHT
DHT dht(DHTPIN, DHTTYPE);

// LDR Configuration
#define LDR_PIN 39     // Pin connectée au capteur LDR

int ldrValue = 0;     // Valeur brute de l'ADC
int ldrPercent = 0;   // Valeur LDR mappée entre 0 et 100
int ldrCalibratedValue = 0;
int ldrMin = 4095; // Valeur minimale calibrée
int ldrMax = 0;    // Valeur maximale calibrée

int currentSpeed = 128; // Vitesse actuelle du moteur (0-255), 50% par défaut

unsigned long lastSpeedUpdate = 0; // Temps du dernier ajustement
const int speedStep = 5; // Pas pour lisser la transition
const unsigned long speedUpdateInterval = 50; // Intervalle pour le contrôle fluide

// OLED Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi Information
const char* ssid = "ARHANDOUS";
const char* password = "123456789";

// MQTT Broker Information
const char* mqtt_server = "10.41.5.143"; // IP de Mosquitto
const int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);

// Neopixel Configuration
#define NEOPIXEL_PIN_1 15
#define NUM_PIXELS_1 8 // Nombre de LEDs dans la bande
Adafruit_NeoPixel pixels_1(NUM_PIXELS_1, NEOPIXEL_PIN_1, NEO_GRB + NEO_KHZ800);

#define NEOPIXEL_PIN_2 14
#define NUM_PIXELS_2 8 // Nombre de LEDs dans la bande
Adafruit_NeoPixel pixels_2(NUM_PIXELS_2, NEOPIXEL_PIN_2, NEO_GRB + NEO_KHZ800);

#define NEOPIXEL_PIN_3 13
#define NUM_PIXELS_3 8 // Bande pour l'état MQTT
Adafruit_NeoPixel pixels_3(NUM_PIXELS_3, NEOPIXEL_PIN_3, NEO_GRB + NEO_KHZ800);
=======
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
#define LVDT_PIN 35

unsigned long lastPublish = 0;
const long interval = 300;

float previousPressure = 0;
unsigned long dropTime = 0;
bool dropDetected = false;

float lvdt_max = 0;
float lastLvdtPos = 0;
unsigned long lastLvdtTime = 0;
float lvdt_speed = 0;

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
  unsigned long nowLvdt = millis();

  if (lastLvdtTime == 0) {
    lastLvdtPos = lvdt_mm;
    lastLvdtTime = nowLvdt;
    lvdt_speed = 0;
  }
  else if (nowLvdt - lastLvdtTime >= 5000) {
    float dt_min = (nowLvdt - lastLvdtTime) / 60000.0;
    float dx = lvdt_mm - lastLvdtPos;

    if (abs(dx) < 0.05) {
      lvdt_speed = 0;
    } else {
      lvdt_speed = abs(dx / dt_min);
    }

    lastLvdtPos = lvdt_mm;
    lastLvdtTime = nowLvdt;
  }
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
  char speedStr[10];

  dtostrf(temperature, 1, 2, tempStr);
  dtostrf(humidity, 1, 2, humStr);
  dtostrf(pressure_bar, 1, 2, pressureStr);
  dtostrf(lvdt_max, 1, 2, lvdtStr);
  dtostrf(lvdt_speed, 1, 2, speedStr);

  itoa(ldrPercent, ldrStr, 10);

  client.publish("esp32/temp", tempStr);
  client.publish("esp32/hum", humStr);
  client.publish("esp32/LDR", ldrStr);

  // MEME TOPIC MQTT
  client.publish("esp32/pressure", pressureStr);

  client.publish("esp32/lvdt", lvdtStr);

  client.publish("esp32/speed", speedStr);

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

      previousPressure = -1;

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
>>>>>>> b253af00704d603c89ffd1dd6443fa1161c5e54d

// Push Buttons
#define BP1_PIN 35  // Pin du bouton-poussoir 1
#define BP2_PIN 34  // Pin du bouton-poussoir 2

<<<<<<< HEAD
// Motor Configuration (L298N)
#define ENA_PIN 4   // Pin ENA (vitesse du moteur)
#define IN1_PIN 25  // Pin IN1
#define IN2_PIN 33  // Pin IN2

// PWM ESP32 
#define PWM_CHANNEL 0
#define PWM_FREQ 20000
#define PWM_RESOLUTION 8

int targetSpeed = 0;   // vitesse demandée par le slider

// Variables pour gérer les vues OLED
int currentView = 1; 
int lastBP1State = LOW;  
int lastBP2State = LOW;  
bool dataSent = false;    

// Timer Variables
unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 15000; // Intervalle en millisecondes (15 secondes)

// MQTT LED State
unsigned long mqttLedOnTime = 0;
const unsigned long mqttLedDuration = 300000; // 5 minutes en millisecondes
bool mqttLedState = false;

// Variables for motor speed control
int motorSpeed = 0; // Speed value from slider (0-255)

// Forward declarations
void publishData();
void handleMoteurCommand(String command);
void updateMqttNeopixel(String state);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connexion au réseau WiFi ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    updateMqttNeopixel("waiting");
  }

  Serial.println("");
  Serial.println("WiFi connecté");
}

void reconnect() {
  unsigned long startAttemptTime = millis();
  while (!client.connected() && (millis() - startAttemptTime) < 10000) { // Timeout après 10s
    updateMqttNeopixel("waiting");
    Serial.print("Connexion au serveur MQTT...");
    if (client.connect("ESP32Client1")) {
      Serial.println("Connecté au broker MQTT !");
      client.subscribe("esp32/moteur");
      client.subscribe("esp32/speed");
      updateMqttNeopixel("connected");
      mqttLedState = true;
      mqttLedOnTime = millis();
    } else {
      Serial.print("Erreur de connexion, code erreur: ");
      Serial.println(client.state());
      updateMqttNeopixel("disconnected");
      delay(500); // Petite pause avant une nouvelle tentative
    }
  }
}

void updateMqttNeopixel(String state) {
  uint32_t color;

  if (state == "connected") {
    color = pixels_3.Color(0, 255, 0); // Vert
  } else if (state == "disconnected") {
    color = pixels_3.Color(255, 0, 0); // Rouge
  } else if (state == "waiting") {
    color = pixels_3.Color(128, 0, 128); // Violet
  } else {
    color = pixels_3.Color(0, 0, 0); // Éteint
  }

  for (int i = 0; i < NUM_PIXELS_3; i++) {
    pixels_3.setPixelColor(i, color);
  }
  pixels_3.show();
}
void displayOledTask(void *pvParameters) {
  for (;;) {
    display.clearDisplay();
    if (currentView == 1) {
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(30, 0);
      display.println("Capteurs");
      display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

      display.setCursor(0, 15);
      display.print("Temp: ");
      display.print(dht.readTemperature());
      display.print(" C");

      display.setCursor(0, 30);
      display.print("Hum: ");
      display.print(dht.readHumidity());
      display.print(" %");

      display.setCursor(0, 45);
      display.print("LDR: ");
      display.print(ldrPercent);
      display.print(" %");
    } else if (currentView == 2) {
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(30, 0);
      display.println("Moteur");
      display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

      display.setCursor(0, 20);
      display.print("Etat: ");
      if (digitalRead(IN1_PIN) == HIGH && digitalRead(IN2_PIN) == LOW) {
        display.print("On");
      } else {
        display.print("Off");
      }

      display.setCursor(0, 40);
      display.print("Vitesse: ");
      int motorSpeedPercent = map(currentSpeed, 0, 255, 0, 100);
      display.print(motorSpeedPercent);
      display.print(" %");
    }

    display.display();
    vTaskDelay(200 / portTICK_PERIOD_MS); // Délai de 200ms pour éviter le WDT
  }
}

void updateNeopixel1(float temperature) {
  Serial.print("Température reçue pour NeoPixel: ");
  Serial.println(temperature);

  uint32_t color;

  if (temperature < 18) {
    color = pixels_1.Color(0, 0, 199); // Bleu
  } else if (temperature >= 18 && temperature <= 25) {
    color = pixels_1.Color(0, 199, 0); // Vert
  } else if (temperature >= 26 && temperature < 30) {
    color = pixels_1.Color(199, 165, 0); // Orange
  } else if (temperature >= 30) {
    color = pixels_1.Color(199, 0, 0); // Rouge
  } else {
    color = pixels_1.Color(0, 0, 0); // Éteint (par défaut)
  }

  for (int i = 0; i < NUM_PIXELS_1; i++) {
    pixels_1.setPixelColor(i, color);
  }
  pixels_1.show();
}

void updateNeopixel2(int ldrPercentage) {
  uint32_t color;

  if (ldrPercentage < 20) {
    color = pixels_2.Color(199, 0, 0); // Rouge si LDR = 0%
  } else {
    color = pixels_2.Color(0, 0, 199); // Éteint si LDR > 0%
  }

  for (int i = 0; i < NUM_PIXELS_2; i++) {
    pixels_2.setPixelColor(i, color); // Appliquer la couleur à chaque LED
  }
  pixels_2.show(); // Actualiser l'affichage des LEDs
}



void calibrateLDR() {
  ldrValue = analogRead(LDR_PIN);
  if (ldrValue < ldrMin) {
    ldrMin = ldrValue;
  }
  if (ldrValue > ldrMax) {
    ldrMax = ldrValue;
  }
}

void computeLDRPercentage() {
  ldrValue = analogRead(LDR_PIN); // Lire la valeur brute du LDR (0 à 4095)
  ldrPercent = map(ldrValue, 0, 4095, 0, 100); // Mapper directement de 0 à 100%
  ldrPercent = constrain(ldrPercent, 0, 100);  // Assurez-vous qu'elle reste entre 0 et 100%
}

void publishData() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  computeLDRPercentage(); // Calcule le pourcentage du LDR
  updateNeopixel2(ldrPercent); // Mettez à jour les LEDs selon la luminosité
  updateNeopixel1(temperature);
  if (!isnan(temperature) && !isnan(humidity)) {
    char tempString[8];
    char humString[8];
    char ldrPercentString[8];
    dtostrf(temperature, 1, 2, tempString);
    dtostrf(humidity, 1, 2, humString);
    itoa(ldrPercent, ldrPercentString, 10);

    client.publish("esp32/temp", tempString);
    client.publish("esp32/hum", humString);
    client.publish("esp32/LDR", ldrPercentString);

    Serial.print("Température publiée: ");
    Serial.println(tempString);
    Serial.print("Humidité publiée: ");
    Serial.println(humString);
    Serial.print("LDR publié: ");
    Serial.println(ldrPercentString);
  } else {
    Serial.println("Erreur de lecture du capteur DHT");
  }
}


void callback(char* topic, byte* message, unsigned int length) {
    String messageTemp;
    for (int i = 0; i < length; i++) {
        messageTemp += (char)message[i];
    }

    if (String(topic) == "esp32/moteur") {
        if (messageTemp == "true") {
            digitalWrite(IN1_PIN, HIGH);
            digitalWrite(IN2_PIN, LOW);
            currentSpeed = 128; // Démarrage à 50%
            Serial.println("Moteur allumé à 50%");
        } else if (messageTemp == "false") {
            digitalWrite(IN1_PIN, LOW);
            digitalWrite(IN2_PIN, LOW);
            currentSpeed = 0; // Moteur arrêté
            Serial.println("Moteur éteint");
        }
    } else if (String(topic) == "esp32/speed") {
        targetSpeed = map(messageTemp.toInt(), 0, 100, 0, 255);
        Serial.print("Vitesse demandée = ");
        Serial.println(targetSpeed);
      }

}
void handleButtonsTask(void *pvParameters) {
  for (;;) {
    int bp1State = digitalRead(BP1_PIN);
    int bp2State = digitalRead(BP2_PIN);

    if (bp1State == LOW && lastBP1State == HIGH) {
      currentView = (currentView == 1) ? 2 : 1;
      dataSent = false; 
      display.clearDisplay();
    }
    lastBP1State = bp1State;

    if (bp2State == LOW && lastBP2State == HIGH) {
      publishData();
      dataSent = true; 
    }
    lastBP2State = bp2State;

    // Délai pour éviter le rebond
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100 ms
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Échec initialisation OLED"));
    for (;;);
  }
  display.display();
  delay(2000);

  pinMode(BP1_PIN, INPUT_PULLUP);
  pinMode(BP2_PIN, INPUT_PULLUP);
  pinMode(IN1_PIN, OUTPUT);  
  pinMode(IN2_PIN, OUTPUT);

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(ENA_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0); // moteur OFF au départ


  Serial.println("Calibration automatique LDR en cours...");
  unsigned long calibrationStart = millis();
  while (millis() - calibrationStart < 5000) {
    calibrateLDR();
    delay(50);
  }
  Serial.println("Calibration LDR terminée !");
  Serial.print("LDR Min: ");
  Serial.println(ldrMin);
  Serial.print("LDR Max: ");
  Serial.println(ldrMax);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  pixels_1.begin();
  pixels_1.clear();
  pixels_1.show();

  pixels_2.begin();
  pixels_2.clear();
  pixels_2.show();

  pixels_3.begin();
  pixels_3.clear();
  pixels_3.show();

    

  xTaskCreatePinnedToCore(handleButtonsTask, "Handle Buttons", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(displayOledTask, "Display OLED", 2048, NULL, 1, NULL, 1);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - lastPublishTime >= publishInterval) {
    lastPublishTime = currentMillis;
    publishData();
  }

  updateNeopixel2(ldrPercent);
  computeLDRPercentage();

  if (mqttLedState && (millis() - mqttLedOnTime >= mqttLedDuration)) {
    mqttLedState = false;
    updateMqttNeopixel("off");
  }
  if (millis() - lastSpeedUpdate > speedUpdateInterval) {
  lastSpeedUpdate = millis();

  if (currentSpeed < targetSpeed)
    currentSpeed += speedStep;
  else if (currentSpeed > targetSpeed)
    currentSpeed -= speedStep;

  currentSpeed = constrain(currentSpeed, 0, 255);
  ledcWrite(PWM_CHANNEL, currentSpeed);
  }

  vTaskDelay(1); // Ajout d'une pause pour éviter le WDT
=======
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
>>>>>>> b253af00704d603c89ffd1dd6443fa1161c5e54d
}