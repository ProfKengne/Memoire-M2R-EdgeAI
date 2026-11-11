// ==========================================================
//  Noeud d'alerte Edge AI - ESP32 + OLED SSD1306 128x64 - liaison Wi-Fi UDP
//  Interface graphique animée : écran d'accueil, connexion, attente (radar),
//  zone RAS (bouclier), intrusion (alerte clignotante), perte Wi-Fi, fin.
// ==========================================================
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <math.h>

// VOS IDENTIFIANTS WI-FI
const char* ssid = "VOTRE_SSID";      // a renseigner localement
const char* password = "VOTRE_MOT_DE_PASSE"; // ne jamais publier le vrai

// Port UDP identique à celui configuré sous Python
unsigned int localUdpPort = 4210;
char incomingPacket[255];  // Buffer pour stocker le paquet entrant

WiFiUDP udp;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define LED_PIN 1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- États de l'interface -----------------------------------------------------
enum Etat { ETAT_ATTENTE, ETAT_RAS, ETAT_INTRUSION, ETAT_FIN, ETAT_PERTE_WIFI };
Etat etat = ETAT_ATTENTE;
int nbCibles = 0;
unsigned long dernierPaquetMs = 0;     // réception du dernier paquet UDP
unsigned long dernierRenduMs = 0;      // dernier rafraîchissement de l'OLED
unsigned long derniereTentativeMs = 0; // dernière tentative de reconnexion
uint16_t image = 0;                    // compteur d'animation

const unsigned long PERIODE_RENDU_MS = 90;     // ~11 images/s pour les animations
const unsigned long DELAI_SANS_FLUX_MS = 3000; // plus de paquet -> écran d'attente

// ==========================================================
//  Petits outils de dessin
// ==========================================================

// Texte centré horizontalement (police 6x8 de base, multipliée par la taille)
void texteCentre(const char* txt, int16_t y, uint8_t taille, uint16_t couleur = SSD1306_WHITE) {
  int16_t w = strlen(txt) * 6 * taille;
  display.setTextSize(taille);
  display.setTextColor(couleur);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(txt);
}

// Texte centré dans une zone [x0, x0+largeur]
void texteCentreZone(const char* txt, int16_t x0, int16_t largeur, int16_t y, uint8_t taille,
                     uint16_t couleur = SSD1306_WHITE) {
  int16_t w = strlen(txt) * 6 * taille;
  display.setTextSize(taille);
  display.setTextColor(couleur);
  display.setCursor(x0 + (largeur - w) / 2, y);
  display.print(txt);
}

// Icône Wi-Fi : trois arcs + point (niveau = nombre d'arcs affichés, 0 à 3)
void iconeWifi(int16_t cx, int16_t cy, uint8_t niveau, uint16_t couleur) {
  display.fillCircle(cx, cy, 1, couleur);
  for (uint8_t i = 1; i <= 3; i++) {
    if (i <= niveau) {
      display.drawCircleHelper(cx, cy, 4 * i + 2, 0x1 | 0x2, couleur);
      display.drawCircleHelper(cx, cy, 4 * i + 1, 0x1 | 0x2, couleur);
    }
  }
}

// Barres de réception (RSSI) : 4 barres de hauteur croissante
void barresSignal(int16_t x, int16_t y, uint16_t couleur) {
  long rssi = WiFi.RSSI();
  uint8_t n = rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : rssi > -85 ? 1 : 0;
  for (uint8_t i = 0; i < 4; i++) {
    int16_t h = 2 + 2 * i;
    if (i < n) display.fillRect(x + i * 3, y + 8 - h, 2, h, couleur);
    else       display.drawPixel(x + i * 3, y + 7, couleur);
  }
}

// Cadre général : bord arrondi + bandeau de titre en négatif
void cadreAvecTitre(const char* titre) {
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 5, SSD1306_WHITE);
  display.fillRoundRect(0, 0, SCREEN_WIDTH, 13, 5, SSD1306_WHITE);
  display.fillRect(0, 8, SCREEN_WIDTH, 5, SSD1306_WHITE);
  texteCentre(titre, 3, 1, SSD1306_BLACK);
}

// Bandeau « EDGE AI » avec indicateur de signal à droite
void cadreEdgeAI() {
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 5, SSD1306_WHITE);
  display.fillRoundRect(0, 0, SCREEN_WIDTH, 13, 5, SSD1306_WHITE);
  display.fillRect(0, 8, SCREEN_WIDTH, 5, SSD1306_WHITE);
  // Petit oeil (caméra) à gauche
  display.fillCircle(9, 6, 4, SSD1306_BLACK);
  display.fillCircle(9, 6, 2, SSD1306_WHITE);
  display.drawPixel(9, 6, SSD1306_BLACK);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(18, 3);
  display.print("EDGE AI  WI-FI");
  barresSignal(110, 2, SSD1306_BLACK);
}

// Coins décoratifs (style viseur de caméra) dans un rectangle
void coinsViseur(int16_t x, int16_t y, int16_t w, int16_t h, int16_t l, uint16_t c) {
  display.drawFastHLine(x, y, l, c);             display.drawFastVLine(x, y, l, c);
  display.drawFastHLine(x + w - l, y, l, c);     display.drawFastVLine(x + w - 1, y, l, c);
  display.drawFastHLine(x, y + h - 1, l, c);     display.drawFastVLine(x, y + h - l, l, c);
  display.drawFastHLine(x + w - l, y + h - 1, l, c); display.drawFastVLine(x + w - 1, y + h - l, l, c);
}

// Coche (validation)
void coche(int16_t x, int16_t y, uint16_t c) {
  for (int8_t e = 0; e < 2; e++) {
    display.drawLine(x, y + 4 + e, x + 3, y + 7 + e, c);
    display.drawLine(x + 3, y + 7 + e, x + 9, y + e, c);
  }
}

// Bouclier avec coche : symbole « zone sécurisée »
void bouclier(int16_t cx, int16_t y, uint16_t c) {
  display.fillRoundRect(cx - 11, y, 23, 16, 3, c);
  display.fillTriangle(cx - 11, y + 14, cx + 11, y + 14, cx, y + 26, c);
  coche(cx - 5, y + 6, SSD1306_BLACK);
}

// Triangle d'avertissement avec point d'exclamation
void triangleAlerte(int16_t cx, int16_t y, uint16_t c, uint16_t fond) {
  display.fillTriangle(cx, y, cx - 15, y + 26, cx + 15, y + 26, c);
  display.fillTriangle(cx, y + 6, cx - 10, y + 23, cx + 10, y + 23, fond);
  display.fillRect(cx - 1, y + 10, 3, 8, c);
  display.fillRect(cx - 1, y + 20, 3, 2, c);
}

// ==========================================================
//  Écrans
// ==========================================================

// Écran d'accueil animé (au démarrage)
void ecranAccueil() {
  for (int p = 0; p <= 100; p += 4) {
    display.clearDisplay();
    display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 6, SSD1306_WHITE);
    display.drawRoundRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, 5, SSD1306_WHITE);
    // Oeil / objectif de caméra
    display.fillCircle(24, 26, 13, SSD1306_WHITE);
    display.fillCircle(24, 26, 9, SSD1306_BLACK);
    display.fillCircle(24, 26, 5, SSD1306_WHITE);
    display.fillCircle(22, 24, 1, SSD1306_BLACK);
    // Titre
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(46, 12);
    display.print("EDGE");
    display.setCursor(46, 30);
    display.print("AI");
    display.setTextSize(1);
    display.setCursor(74, 34);
    display.print("YOLOv8n");
    // Barre de progression
    display.drawRoundRect(10, 47, 108, 9, 3, SSD1306_WHITE);
    display.fillRoundRect(12, 49, (104 * p) / 100, 5, 2, SSD1306_WHITE);
    display.display();
    delay(30);
  }
  delay(400);
}

// Connexion en cours (avec diagnostic)
void ecranConnexion(unsigned long ecoule, wl_status_t st) {
  display.clearDisplay();
  cadreAvecTitre("CONNEXION WI-FI");
  iconeWifi(19, 46, (ecoule / 400) % 4, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(38, 18);
  display.print("Reseau Wi-Fi");
  if (st == WL_NO_SSID_AVAIL) {
    display.setCursor(38, 30); display.print("Introuvable:");
    display.setCursor(38, 40); display.print("activer la");
    display.setCursor(38, 50); display.print("bande 2,4 GHz");
  } else if (st == WL_CONNECT_FAILED) {
    display.setCursor(38, 30); display.print("Echec : mot");
    display.setCursor(38, 40); display.print("de passe ou");
    display.setCursor(38, 50); display.print("WPA3 -> WPA2");
  } else {
    display.setCursor(38, 32);
    display.print("Recherche");
    for (uint8_t i = 0; i < (ecoule / 300) % 4; i++) display.print('.');
    display.setCursor(38, 46);
    display.print(ecoule / 1000);
    display.print(" s");
  }
  display.display();
}

// Connexion réussie
void ecranConnecte() {
  display.clearDisplay();
  cadreAvecTitre("WI-FI CONNECTE");
  display.fillCircle(20, 29, 10, SSD1306_WHITE);
  coche(15, 24, SSD1306_BLACK);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(38, 19);
  display.print("Adresse IP");
  display.setCursor(38, 30);
  display.print("Port UDP ");
  display.print(localUdpPort);
  display.drawFastHLine(6, 42, SCREEN_WIDTH - 12, SSD1306_WHITE);
  texteCentre(WiFi.localIP().toString().c_str(), 48, 1);
  display.display();
}

// Attente du flux vidéo : radar animé
void ecranAttente() {
  display.clearDisplay();
  cadreEdgeAI();
  const int16_t cx = 25, cy = 38, r = 20;
  display.drawCircle(cx, cy, r, SSD1306_WHITE);
  display.drawCircle(cx, cy, r / 2, SSD1306_WHITE);
  display.drawFastHLine(cx - r, cy, 2 * r + 1, SSD1306_WHITE);
  display.drawFastVLine(cx, cy - r, 2 * r + 1, SSD1306_WHITE);
  float a = image * 0.35f;
  for (int8_t k = 0; k < 3; k++) {  // faisceau avec une petite traînée
    float b = a - k * 0.12f;
    display.drawLine(cx, cy, cx + (int16_t)(r * cosf(b)), cy + (int16_t)(r * sinf(b)), SSD1306_WHITE);
  }
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(54, 19); display.print("En attente");
  display.setCursor(54, 29); display.print("du flux");
  display.setCursor(54, 39); display.print("video");
  for (uint8_t i = 0; i < (image / 4) % 4; i++) display.print('.');
  display.setCursor(54, 52); display.print("UDP ");
  display.print(localUdpPort);
  display.display();
}

// Zone RAS : bouclier + pulsation « système actif »
void ecranRAS() {
  display.clearDisplay();
  cadreEdgeAI();
  bouclier(22, 19, SSD1306_WHITE);
  texteCentreZone("ZONE", 42, 84, 18, 2);
  texteCentreZone("RAS", 42, 84, 36, 2);
  // Point « vivant » qui pulse dans le coin bas gauche
  uint8_t r = (image / 4) % 2 ? 2 : 1;
  display.fillCircle(8, 56, r, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(14, 53);
  display.print("actif");
  display.display();
}

// Intrusion : alerte clignotante en négatif
void ecranIntrusion() {
  bool negatif = (image / 3) % 2;  // alterne toutes les ~270 ms
  uint16_t fg = negatif ? SSD1306_BLACK : SSD1306_WHITE;
  uint16_t bg = negatif ? SSD1306_WHITE : SSD1306_BLACK;
  display.fillScreen(bg);
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 5, fg);
  display.drawRoundRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, 4, fg);
  texteCentre("! INTRUSION !", 6, 1, fg);
  display.drawFastHLine(8, 16, SCREEN_WIDTH - 16, fg);
  triangleAlerte(24, 22, fg, bg);
  // Nombre de personnes, dans un viseur
  coinsViseur(54, 20, 64, 38, 6, fg);
  char nb[6];
  snprintf(nb, sizeof(nb), "%d", nbCibles);
  texteCentreZone(nb, 54, 64, 24, 3, fg);
  texteCentreZone(nbCibles > 1 ? "personnes" : "personne", 54, 64, 46, 1, fg);
  display.display();
}

// Fin de session
void ecranFin() {
  display.clearDisplay();
  cadreAvecTitre("EDGE AI  WI-FI");
  display.drawRoundRect(10, 19, 108, 38, 6, SSD1306_WHITE);
  texteCentre("SESSION", 24, 1);
  texteCentre("TERMINEE", 34, 1);
  texteCentre("Au revoir, Prof!", 46, 1);
  display.display();
}

// Perte du Wi-Fi : icône barrée
void ecranPerteWifi() {
  display.clearDisplay();
  cadreAvecTitre("PERTE WI-FI");
  iconeWifi(22, 46, 3, SSD1306_WHITE);
  display.drawLine(8, 22, 36, 50, SSD1306_WHITE);
  display.drawLine(9, 22, 37, 50, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(46, 22); display.print("Connexion");
  display.setCursor(46, 32); display.print("perdue");
  display.setCursor(46, 46); display.print("Reconnexion");
  display.setCursor(46, 55);
  for (uint8_t i = 0; i < (image / 3) % 4; i++) display.print(". ");
  display.display();
}

void dessiner() {
  switch (etat) {
    case ETAT_ATTENTE:    ecranAttente();    break;
    case ETAT_RAS:        ecranRAS();        break;
    case ETAT_INTRUSION:  ecranIntrusion();  break;
    case ETAT_FIN:        ecranFin();        break;
    case ETAT_PERTE_WIFI: ecranPerteWifi();  break;
  }
  dernierRenduMs = millis();
}

// ==========================================================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  ecranAccueil();

  // Mode station seul + pas de mise en veille radio (plus stable sur un point d'accès de téléphone)
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  unsigned long debut = millis();
  unsigned long debutAffiche = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(150);
    ecranConnexion(millis() - debutAffiche, WiFi.status());
    // Nouvelle tentative toutes les 15 s
    if (millis() - debut > 15000) {
      WiFi.disconnect();
      delay(200);
      WiFi.begin(ssid, password);
      debut = millis();
    }
  }

  Serial.print("Connecté ! Adresse IP : ");
  Serial.println(WiFi.localIP());
  ecranConnecte();
  delay(2500);

  // Démarrer l'écoute du port UDP
  udp.begin(localUdpPort);
  Serial.printf("Écoute active sur le port UDP %d\n", localUdpPort);
  etat = ETAT_ATTENTE;
  dessiner();
}

void loop() {
  unsigned long maintenant = millis();

  // 1. Vérification de la connexion Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    if (etat != ETAT_PERTE_WIFI) {
      etat = ETAT_PERTE_WIFI;
      digitalWrite(LED_PIN, LOW);
      WiFi.begin(ssid, password);
      derniereTentativeMs = maintenant;
    } else if (maintenant - derniereTentativeMs > 8000) {
      // Tentative de reconnexion automatique
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      derniereTentativeMs = maintenant;
    }
    if (maintenant - dernierRenduMs >= PERIODE_RENDU_MS) { image++; dessiner(); }
    return; // On attend la reconnexion avant de continuer
  }
  if (etat == ETAT_PERTE_WIFI) {
    etat = ETAT_ATTENTE;
    dessiner();
  }

  // 2. Gestion des paquets UDP : on lit tous les paquets en file et on garde le plus récent
  bool nouveau = false;
  int packetSize;
  while ((packetSize = udp.parsePacket()) > 0) {
    int len = udp.read(incomingPacket, sizeof(incomingPacket) - 1);
    incomingPacket[len > 0 ? len : 0] = 0;
    nouveau = true;
  }

  if (nouveau) {
    dernierPaquetMs = maintenant;
    Etat precedent = etat;
    int precedentNb = nbCibles;
    if (strcmp(incomingPacket, "QUIT") == 0) {
      etat = ETAT_FIN;
      digitalWrite(LED_PIN, LOW);
    } else {
      nbCibles = atoi(incomingPacket); // Conversion du texte brut en entier
      if (nbCibles > 0) { etat = ETAT_INTRUSION; digitalWrite(LED_PIN, HIGH); }
      else              { etat = ETAT_RAS;       digitalWrite(LED_PIN, LOW);  }
    }
    // Mise à jour instantanée de l'OLED dès que la situation change
    if (etat != precedent || nbCibles != precedentNb) dessiner();
  } else if ((etat == ETAT_RAS || etat == ETAT_INTRUSION) &&
             maintenant - dernierPaquetMs > DELAI_SANS_FLUX_MS) {
    // Plus de flux depuis le PC : retour à l'écran d'attente
    etat = ETAT_ATTENTE;
    digitalWrite(LED_PIN, LOW);
    dessiner();
  }

  // 3. Animations (radar, pulsation, clignotement de l'alerte)
  if (etat != ETAT_FIN && maintenant - dernierRenduMs >= PERIODE_RENDU_MS) {
    image++;
    dessiner();
  }
}
