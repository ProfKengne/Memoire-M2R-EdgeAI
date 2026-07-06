#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define LED_PIN 2 // LED bleue pour l'alerte visuelle

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  // On ouvre le port série à la même vitesse que le script Python
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    for(;;);
  }
  
  // Message d'attente
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println("En attente du");
  display.setCursor(10, 35);
  display.println("Serveur Edge AI...");
  display.display();
}

void loop() {
  // Vérifier si Python nous a envoyé une donnée
  if (Serial.available() > 0) {
    // Lire la chaîne jusqu'au caractère de fin de ligne '\n'
    String donnee = Serial.readStringUntil('\n');
    int nbCibles = donnee.toInt(); // Convertir le texte en entier

    // Mise à jour graphique de l'OLED
    display.clearDisplay();
    
    // Bandeau supérieur
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("  EDGE AI SURVEILLANCE ");
    display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

    if (nbCibles > 0) {
      // 🚨 ALERTE : Cible détectée !
      digitalWrite(LED_PIN, HIGH); // Allumer la LED d'alerte
      
      display.setCursor(15, 24);
      display.setTextSize(1);
      display.println("!!! INTRUSION !!!");
      
      display.setCursor(20, 42);
      display.setTextSize(2);
      display.print("Cibles: ");
      display.print(nbCibles);
    } else {
      // ✅ RAS : Aucune cible
      digitalWrite(LED_PIN, LOW); // Éteindre la LED
      
      display.setCursor(25, 30);
      display.setTextSize(2);
      display.println("ZONE RAS");
    }
    
    display.display(); // Envoyer à l'écran
  }
}
