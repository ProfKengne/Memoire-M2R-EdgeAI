# 📁 Documentation du Projet & Schémas

Ce dossier rassemble l'ensemble de la documentation technique, des guides d'utilisation et des schémas fonctionnels ou électroniques du système de surveillance Edge AI.

## 📌 Contenu du dossier

*   **`schemes/`** : Contient les schémas structurels, les circuits imprimés (PCB) et les diagrammes de câblage (conceptions AutoCAD, Proteus ou EasyEDA).
*   **`datasheets/`** : Fiches techniques des composants clés utilisés (ESP32, capteurs, écrans OLED).
*   **`reports/`** : Documents d'avancement ou annexes textuelles liés au mémoire.

## 🛠️ Schéma Synoptique Global
*(Tu pourras ajouter ici une description ou une image du flux de communication entre le script de détection Python sur PC et la gestion des alertes matérielles sur l'ESP32)*

1. **PC (Inférence YOLOv8)** ➔ Envoi des données d'intrusion via Liaison Série (JSON) ou Wi-Fi.
2. **ESP32 (Firmware)** ➔ Réception, affichage sur l'écran OLED et déclenchement des alertes physiques.
