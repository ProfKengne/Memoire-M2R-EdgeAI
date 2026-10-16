#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiUdp.h> // <-- AJOUT pour la gestion des paquets UDP

// 🌐 VOS IDENTIFIANTS WI-FI
const char* ssid = "VOTRE_SSID";      // a renseigner localement
const char* password = "VOTRE_MOT_DE_PASSE"; // ne jamais publier le vrai

// Port UDP identique à celui configuré sous Python
unsigned int localUdpPort = 4210; 
char incomingPacket[255];  // Buffer pour stocker le paquet entrant

WiFiUDP udp; // Instance UDP

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define LED_PIN 1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("Connexion Wi-Fi...");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("Connecté ! Adresse IP : ");
  Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.setCursor(0, 10);
  display.println("Wi-Fi Connecte !");
  display.setCursor(0, 30);
  display.print("IP: ");
  display.println(WiFi.localIP().toString());
  display.display();

  // Démarrer l'écoute du port UDP
  udp.begin(localUdpPort);
  Serial.printf("Écoute active sur le port UDP %d\n", localUdpPort);
}

void loop() {
// 1. Vérification de la connexion Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("PERTE CONNEXION WIFI");
    display.setCursor(0, 40);
    display.println("Reconnexion...");
    display.display();
    
    // Tentative de reconnexion automatique
    WiFi.begin(ssid, password);
    delay(2000);
    return; // On attend la reconnexion avant de continuer
  }

  // 2. Gestion des paquets UDP
    int packetSize = udp.parsePacket();
    if (packetSize) {
    int len = udp.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = 0;

    // Comparaison avec "QUIT"
    if (strcmp(incomingPacket, "QUIT") == 0) {
      digitalWrite(LED_PIN, LOW);
      display.clearDisplay();
      display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("  EDGE AI via WI-FI   ");
    display.drawFastHLine(0, 12, 128, SSD1306_WHITE);
      display.setCursor(15, 25);
      display.println("SESSION TERMINEE");
      display.setCursor(15, 40);
      display.println("Aurevoir, Prof!");
      display.display();
      return; // On arrête le traitement pour ce cycle
    }
    
    int nbCibles = atoi(incomingPacket); // Conversion du texte brut en entier

    // Mise à jour instantanée de l'OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("  EDGE AI via WI-FI   ");
    display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

    if (nbCibles > 0) {
      digitalWrite(LED_PIN, HIGH);
      display.setCursor(15, 24);
      display.println("!!! INTRUSION !!!");
      display.setCursor(20, 42);
      display.setTextSize(2);
      display.print("Cibles: ");
      display.print(nbCibles);
    } else {
      digitalWrite(LED_PIN, LOW);
      display.setCursor(25, 30);
      display.setTextSize(2);
      display.println("ZONE RAS");
    }
    display.display();
  }
}