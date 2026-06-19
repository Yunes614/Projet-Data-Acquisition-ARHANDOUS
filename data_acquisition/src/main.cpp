#include <WiFi.h>                         // Bibliothèque pour connecter l'ESP32 au Wi-Fi
#include <PubSubClient.h>                 // Bibliothèque pour la communication MQTT
#include <DHT.h>                          // Bibliothèque pour le capteur DHT22
#include <Wire.h>                         // Bibliothèque pour la communication I2C
#include <Adafruit_INA228.h>              // Bibliothèque pour le module INA228
#include <hd44780.h>                      // Bibliothèque pour l'écran LCD
#include <hd44780ioClass/hd44780_I2Cexp.h>// Bibliothèque pour LCD avec module I2C


// =====================================================
// PARAMETRES WIFI
// =====================================================

const char* ssid = "DESKTOP-TRAC";        // Nom du réseau Wi-Fi
const char* password = "51k85,D9";        // Mot de passe du réseau Wi-Fi


// =====================================================
// PARAMETRES MQTT
// =====================================================

const char* mqtt_server = "192.168.137.1"; // Adresse IP du broker MQTT Mosquitto
const int mqtt_port = 1883;                // Port MQTT standard

WiFiClient espClient;                      // Client Wi-Fi utilisé par MQTT
PubSubClient client(espClient);            // Client MQTT basé sur la connexion Wi-Fi


// =====================================================
// MODULE INA228
// =====================================================
// L'INA228 lit la tension provenant du transmetteur
// de la cellule de charge. Cette tension est ensuite
// convertie en charge en kilogrammes.

Adafruit_INA228 ina228;


// =====================================================
// ECRAN LCD I2C
// =====================================================
// Le LCD affiche localement les principales mesures :
// température, humidité, charge et déplacement.

hd44780_I2Cexp lcd(0x27);                  // Adresse I2C du LCD


// =====================================================
// CAPTEUR DHT22
// =====================================================
// Le DHT22 mesure la température et l'humidité ambiantes.

#define DHTPIN 26                          // Broche utilisée pour le DHT22
#define DHTTYPE DHT22                      // Type du capteur
DHT dht(DHTPIN, DHTTYPE);                  // Création de l'objet DHT


// =====================================================
// BROCHES DE COMMANDE ET SIGNALISATION
// =====================================================

#define MOSFET_PIN 14                      // Broche qui commande le MOSFET/relais
#define LED_VERTE 15                       // LED verte : connexion MQTT OK
#define LED_ROUGE 2                        // LED rouge : défaut de connexion MQTT
#define BUZZER_PIN 27                      // Buzzer d'alarme
#define LED_ALARME 12                      // LED d'alarme LVDT
#define CHANNEL 0                          // Canal PWM utilisé pour le buzzer
#define BTN_RESET 33                       // Bouton bypass/reset manuel


// =====================================================
// CAPTEUR DE DEPLACEMENT LVDT
// =====================================================
// Le LVDT mesure le déplacement de la traverse.
// La valeur brute ADC est convertie en millimètres.

#define LVDT_PIN 35                        // Entrée analogique du LVDT


// =====================================================
// VARIABLES DE TEMPS
// =====================================================

unsigned long lastPublish = 0;             // Dernier moment où les données ont été envoyées
const long interval = 300;                 // Intervalle d'envoi MQTT en millisecondes


// =====================================================
// VARIABLES DE SECURITE
// =====================================================
// previousPressure sert à détecter une chute brutale
// de charge, par exemple lors de la rupture de l'éprouvette.

float previousPressure = 0;                // Ancienne valeur de charge en kg
unsigned long dropTime = 0;                // Moment où la chute de charge est détectée
bool dropDetected = false;                 // Indique si une chute de charge est détectée


// =====================================================
// VARIABLE LVDT
// =====================================================

float lvdt_max = 0;                        // Déplacement maximal atteint pendant l'essai


// =====================================================
// FONCTION DE LECTURE ADC AVEC MOYENNE
// =====================================================
// Cette fonction lit 10 fois une entrée analogique,
// puis fait la moyenne pour réduire le bruit de mesure.

int readADC(int pin) {

  int sum = 0;                             // Variable qui stocke la somme des lectures

  for (int i = 0; i < 10; i++) {           // Répéter 10 mesures
    sum += analogRead(pin);                // Ajouter la valeur lue à la somme
    delay(2);                              // Petite pause entre deux lectures
  }

  return sum / 10;                         // Retourner la moyenne des 10 mesures
}


// =====================================================
// CONNEXION WIFI
// =====================================================
// Cette fonction connecte l'ESP32 au réseau Wi-Fi.

void setup_wifi() {

  Serial.println("Connexion WiFi...");     // Message dans le moniteur série

  WiFi.begin(ssid, password);              // Lancement de la connexion Wi-Fi

  while (WiFi.status() != WL_CONNECTED) {  // Tant que l'ESP32 n'est pas connecté
    delay(500);                            // Attendre 0,5 seconde
    Serial.print(".");                     // Afficher un point d'attente
  }

  Serial.println("");                      // Saut de ligne
  Serial.println("WiFi connecté !");       // Confirmation de connexion
}


// =====================================================
// GESTION DES LEDS DE CONNEXION
// =====================================================
// LED verte : MQTT connecté.
// LED rouge : MQTT déconnecté.
// LEDs éteintes : tentative de connexion.

void updateLEDStatus(String state) {

  if (state == "connected") {              // Si MQTT est connecté

    digitalWrite(LED_VERTE, HIGH);         // Allumer LED verte
    digitalWrite(LED_ROUGE, LOW);          // Éteindre LED rouge

  }
  else if (state == "disconnected") {      // Si MQTT est déconnecté

    digitalWrite(LED_VERTE, LOW);          // Éteindre LED verte
    digitalWrite(LED_ROUGE, HIGH);         // Allumer LED rouge

  }
  else if (state == "waiting") {           // Pendant une tentative de connexion

    digitalWrite(LED_VERTE, LOW);          // Éteindre LED verte
    digitalWrite(LED_ROUGE, LOW);          // Éteindre LED rouge
  }
}


// =====================================================
// RECONNEXION MQTT
// =====================================================
// Cette fonction reconnecte automatiquement l'ESP32
// au broker MQTT si la connexion est perdue.

void reconnect() {

  while (!client.connected()) {            // Tant que le client MQTT n'est pas connecté

    updateLEDStatus("waiting");            // État attente

    Serial.print("Connexion MQTT...");     // Message de tentative

    if (client.connect("ESP32Traction")) { // Tentative de connexion au broker

      Serial.println("connecté");          // Connexion réussie

      client.subscribe("machine/control"); // Abonnement au topic de commande Start/Stop

      updateLEDStatus("connected");        // LED verte allumée

    } else {                               // Si la connexion échoue

      Serial.print("Erreur MQTT : ");      // Afficher l'erreur
      Serial.println(client.state());      // Code d'erreur MQTT

      updateLEDStatus("disconnected");     // LED rouge allumée

      delay(2000);                         // Attendre avant de réessayer
    }
  }
}


// =====================================================
// LECTURE DES CAPTEURS ET ENVOI DES DONNEES
// =====================================================
// Cette fonction lit les capteurs, convertit les valeurs,
// affiche les mesures sur LCD et les envoie vers Node-RED.

void publishData() {

  Serial.println("");
  Serial.println("===== LECTURE CAPTEURS =====");


  // =====================================================
  // LECTURE TEMPERATURE ET HUMIDITE
  // =====================================================

  float temperature = dht.readTemperature(); // Lecture de la température en °C
  float humidity = dht.readHumidity();       // Lecture de l'humidité en %


  // =====================================================
  // LECTURE CELLULE DE CHARGE VIA INA228
  // =====================================================
  // Le transmetteur de la cellule de charge fournit une
  // tension proportionnelle à la charge appliquée.
  // Ici, 10 V correspond à 300 kg.

  float tension = ina228.readBusVoltage();   // Lecture de la tension par l'INA228

  float pressure_kg = ((tension / 10.0) * 300.0) + 0.505;
  // Conversion tension -> charge en kg
  // 0 V = 0 kg
  // 10 V = 300 kg
  // +0.505 = correction/calibration


  // =====================================================
  // LECTURE DU LVDT
  // =====================================================
  // Le LVDT mesure le déplacement. La valeur ADC varie
  // de 0 à 4095 et correspond à une course de 0 à 50 mm.

  int lvdtRaw = readADC(LVDT_PIN);           // Lecture brute ADC du LVDT

  float lvdt_mm = (lvdtRaw / 4095.0) * 50.0; // Conversion ADC -> déplacement en mm


  // =====================================================
  // AFFICHAGE LCD
  // =====================================================

  lcd.setCursor(0, 0);                       // Curseur ligne 1, colonne 0

  lcd.print("T:");                           // Afficher T pour température
  lcd.print(temperature, 1);                 // Afficher température avec 1 décimale

  lcd.print(" H:");                          // Afficher H pour humidité
  lcd.print(humidity, 0);                    // Afficher humidité sans décimale

  lcd.print("   ");                          // Effacer les anciens caractères restants


  lcd.setCursor(0, 1);                       // Curseur ligne 2, colonne 0

  lcd.print("Kg:");                          // Afficher la charge en kg
  lcd.print(pressure_kg, 0);                 // Afficher la charge sans décimale

  lcd.print(" L:");                          // Afficher L pour LVDT
  lcd.print(lvdt_mm, 1);                     // Afficher déplacement avec 1 décimale

  lcd.print("   ");                          // Effacer les anciens caractères restants


  // =====================================================
  // MEMORISATION DU DEPLACEMENT MAXIMAL
  // =====================================================

  if (lvdt_mm > lvdt_max) {                  // Si la valeur actuelle dépasse l'ancien maximum
    lvdt_max = lvdt_mm;                      // Mettre à jour le maximum
  }


  // =====================================================
  // DETECTION CHUTE BRUTALE DE CHARGE
  // =====================================================
  // Si la charge diminue brutalement de plus de 10 kg,
  // cela peut indiquer la rupture de l'éprouvette.

  Serial.print("Charge actuelle : ");
  Serial.println(pressure_kg);

  Serial.print("Charge precedente : ");
  Serial.println(previousPressure);

  if (pressure_kg < (previousPressure - 10) && !dropDetected) {

    dropTime = millis();                     // Enregistrer le temps de détection
    dropDetected = true;                     // Activer le drapeau de chute

    Serial.println("↓↓↓↓ CHUTE DE CHARGE DETECTEE ↓↓↓↓");
  }

  previousPressure = pressure_kg;            // Mettre à jour l'ancienne valeur


  // =====================================================
  // SECURITE LVDT
  // =====================================================
  // Si le déplacement atteint 50 mm, la machine s'arrête
  // et une alarme sonore/visuelle est activée.

  if (lvdt_mm >= 50.0) {                     // Seuil maximal du LVDT

    Serial.println("!!! LIMITE LVDT ATTEINTE - ARRET MACHINE !!!");

    digitalWrite(MOSFET_PIN, LOW);           // Arrêt de la machine

    for (int f = 800; f <= 2500; f += 20) {  // Son montant du buzzer

      ledcWriteTone(CHANNEL, f);             // Envoyer une fréquence au buzzer

      digitalWrite(LED_ALARME, HIGH);        // Allumer LED alarme

      delay(10);                             // Petite pause
    }

    for (int f = 2500; f >= 800; f -= 20) {  // Son descendant du buzzer

      ledcWriteTone(CHANNEL, f);             // Modifier fréquence buzzer

      digitalWrite(LED_ALARME, LOW);         // Éteindre LED alarme

      delay(10);                             // Petite pause
    }

  } else {                                   // Si la limite n'est pas atteinte

    ledcWriteTone(CHANNEL, 0);               // Arrêter le buzzer

    digitalWrite(LED_ALARME, LOW);           // Éteindre LED alarme
  }


  // =====================================================
  // AFFICHAGE MONITEUR SERIE
  // =====================================================

  Serial.print("Temperature : ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humidite : ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Tension INA228 : ");
  Serial.print(tension);
  Serial.println(" V");

  Serial.print("Charge cellule : ");
  Serial.print(pressure_kg);
  Serial.println(" kg");

  Serial.print("LVDT raw : ");
  Serial.println(lvdtRaw);

  Serial.print("LVDT mm : ");
  Serial.print(lvdt_mm);
  Serial.println(" mm");

  Serial.print("LVDT max : ");
  Serial.print(lvdt_max);
  Serial.println(" mm");

  Serial.println("============================");


  // =====================================================
  // PREPARATION DES DONNEES MQTT
  // =====================================================
  // MQTT envoie des chaînes de caractères.
  // On convertit donc les valeurs numériques en texte.

  char tempStr[10];                          // Texte température
  char humStr[10];                           // Texte humidité
  char pressureStr[10];                      // Texte charge en kg
  char lvdtStr[10];                          // Texte LVDT max

  dtostrf(temperature, 1, 2, tempStr);        // Conversion température en texte
  dtostrf(humidity, 1, 2, humStr);            // Conversion humidité en texte
  dtostrf(pressure_kg, 1, 2, pressureStr);    // Conversion charge en texte
  dtostrf(lvdt_max, 1, 2, lvdtStr);           // Conversion LVDT max en texte


  // =====================================================
  // PUBLICATION MQTT VERS NODE-RED
  // =====================================================

  client.publish("esp32/temp", tempStr);      // Envoyer température
  client.publish("esp32/hum", humStr);        // Envoyer humidité
  client.publish("esp32/pressure", pressureStr); // Envoyer charge kg
  client.publish("esp32/lvdt", lvdtStr);      // Envoyer déplacement max

  Serial.println("Données envoyées via MQTT !");
}


// =====================================================
// CALLBACK MQTT
// =====================================================
// Cette fonction reçoit les commandes envoyées par Node-RED.
// Topic utilisé : machine/control
// Messages possibles : start ou stop

void callback(char* topic, byte* payload, unsigned int length) {

  String message = "";                       // Variable pour reconstruire le message reçu

  for (int i = 0; i < length; i++) {          // Lire chaque caractère du message
    message += (char)payload[i];              // Ajouter le caractère au message
  }

  Serial.print("Message reçu : ");
  Serial.println(message);

  if (String(topic) == "machine/control") {  // Vérifier le topic reçu

    if (message == "start") {                // Si Node-RED envoie start

      digitalWrite(MOSFET_PIN, HIGH);         // Démarrer la machine

      dropDetected = false;                   // Réinitialiser la détection de chute

      previousPressure = -1;                  // Réinitialiser la charge précédente

      lvdt_max = 0;                           // Réinitialiser le maximum LVDT
    }

    if (message == "stop") {                 // Si Node-RED envoie stop

      digitalWrite(MOSFET_PIN, LOW);          // Arrêter la machine
    }
  }
}


// =====================================================
// SETUP
// =====================================================
// Cette fonction s'exécute une seule fois au démarrage.
// Elle initialise les capteurs, le Wi-Fi, MQTT et les sorties.

void setup() {

  Serial.begin(115200);                      // Démarrer le moniteur série

  Serial.println("Demarrage ESP32...");      // Message de démarrage

  dht.begin();                               // Initialiser le capteur DHT22


  // =====================================================
  // INITIALISATION I2C
  // =====================================================

  Wire.begin(21, 22);                        // Démarrer I2C : SDA = GPIO21, SCL = GPIO22


  // =====================================================
  // INITIALISATION LCD
  // =====================================================

  lcd.begin(16, 2);                          // LCD 16 colonnes, 2 lignes

  lcd.setCursor(0, 0);                       // Ligne 1
  lcd.print("SYSTEM READY");                 // Message de démarrage

  delay(2000);                               // Afficher pendant 2 secondes

  lcd.clear();                               // Effacer l'écran


  // =====================================================
  // INITIALISATION INA228
  // =====================================================

  if (!ina228.begin()) {                     // Si l'INA228 n'est pas détecté

    Serial.println("INA228 non detecte");    // Message d'erreur série

    lcd.setCursor(0, 0);                     // Ligne 1 LCD
    lcd.print("INA228 ERROR");               // Message d'erreur LCD

    while (1);                               // Bloquer le programme
  }

  Serial.println("INA228 detecte !");        // Confirmation série

  lcd.setCursor(0, 0);                       // Ligne 1
  lcd.print("INA228 OK");                    // Confirmation LCD

  delay(1000);                               // Attendre 1 seconde

  lcd.clear();                               // Effacer LCD


  // =====================================================
  // CONFIGURATION ADC ESP32
  // =====================================================

  analogReadResolution(12);                  // Résolution ADC 12 bits : 0 à 4095
  analogSetAttenuation(ADC_11db);            // Permet de lire une plage de tension plus large


  // =====================================================
  // CONNEXION WIFI ET MQTT
  // =====================================================

  setup_wifi();                              // Connexion Wi-Fi

  client.setServer(mqtt_server, mqtt_port);  // Définir le broker MQTT
  client.setCallback(callback);              // Définir la fonction appelée à la réception MQTT


  // =====================================================
  // CONFIGURATION DES BROCHES
  // =====================================================

  pinMode(LED_VERTE, OUTPUT);                // LED verte en sortie
  pinMode(LED_ROUGE, OUTPUT);                // LED rouge en sortie

  digitalWrite(LED_VERTE, LOW);              // LED verte éteinte au départ
  digitalWrite(LED_ROUGE, HIGH);             // LED rouge allumée tant que MQTT n'est pas connecté

  pinMode(MOSFET_PIN, OUTPUT);               // MOSFET en sortie
  digitalWrite(MOSFET_PIN, LOW);             // Machine arrêtée au démarrage

  pinMode(LED_ALARME, OUTPUT);               // LED alarme en sortie

  pinMode(BTN_RESET, INPUT_PULLUP);          // Bouton bypass avec résistance interne pull-up

  pinMode(LVDT_PIN, INPUT);                  // LVDT en entrée analogique

  ledcSetup(CHANNEL, 2000, 8);               // Configurer PWM du buzzer
  ledcAttachPin(BUZZER_PIN, CHANNEL);        // Associer le buzzer au canal PWM
}


// =====================================================
// LOOP
// =====================================================
// Cette fonction tourne en boucle.
// Elle gère le bypass, MQTT, les sécurités et l'envoi périodique.

void loop() {

  // =====================================================
  // BYPASS MANUEL
  // =====================================================
  // Si le bouton reset/bypass est appuyé, la machine démarre
  // manuellement, même sans commande Node-RED.

  if (digitalRead(BTN_RESET) == LOW) {        // Bouton appuyé

    digitalWrite(MOSFET_PIN, HIGH);           // Démarrage manuel de la machine

    ledcWriteTone(CHANNEL, 0);                // Arrêter le buzzer

    digitalWrite(LED_ALARME, LOW);            // Éteindre LED alarme

    dropDetected = false;                     // Annuler détection de chute

    previousPressure = 0;                     // Réinitialiser ancienne charge

    return;                                   // Sortir de la boucle pour garder le bypass actif
  }


  // =====================================================
  // VERIFICATION CONNEXION MQTT
  // =====================================================

  if (!client.connected()) {                  // Si MQTT est déconnecté
    reconnect();                              // Reconnexion automatique
  }

  client.loop();                              // Maintenir la communication MQTT active


  // =====================================================
  // ARRET APRES CHUTE DE CHARGE
  // =====================================================

  if (dropDetected) {                         // Si une chute de charge a été détectée

    Serial.print("Attente arret... temps ecoule = ");
    Serial.println(millis() - dropTime);
  }

  if (dropDetected && millis() - dropTime >= 2000) {

    Serial.println("ARRET MACHINE APRES CHUTE DE CHARGE");

    digitalWrite(MOSFET_PIN, LOW);            // Arrêter la machine

    dropDetected = false;                     // Réinitialiser la détection
  }


  // =====================================================
  // ENVOI PERIODIQUE DES DONNEES
  // =====================================================

  unsigned long now = millis();               // Temps actuel

  if (now - lastPublish > interval) {         // Si l'intervalle est dépassé

    lastPublish = now;                        // Mettre à jour le temps d'envoi

    publishData();                            // Lire capteurs + envoyer MQTT
  }
}