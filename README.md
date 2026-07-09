# Algorithme d'Apprentissage Automatique pour Système de Surveillance Edge AI

Projet de recherche réalisé dans le cadre du **Master 2 Recherche en Sciences de l'Ingénieur (Physique Appliquée)**.

Ce projet propose une solution complète de surveillance intelligente basée sur l'Edge AI, combinant la puissance de vision par ordinateur (YOLOv8) et le traitement local via microcontrôleur (ESP32).

## 📁 Architecture du Projet

Le dépôt est structuré comme suit :

* `ai_models/` : Scripts d'entraînement (PyTorch, YOLOv8), datasets et fichiers de poids (`.pt`) optimisés.
* `pc_inference/` : Pipeline de détection en temps réel développé en Python avec OpenCV, incluant la journalisation des alertes au format JSON.
* `firmware_esp32/` : Code source C++ (Arduino/PlatformIO) gérant l'interface matérielle, l'affichage OLED et la communication (Série/Wi-Fi).
* `docs/` : Documentation technique, schémas fonctionnels et rapports de recherche associés.

## 🛠️ Technologies & Outils

* **IA & Vision :** [YOLOv8](https://ultralytics.com/), [OpenCV](https://opencv.org/), [PyTorch](https://pytorch.org/).
* **Systèmes Embarqués :** [ESP32](https://www.espressif.com/), [PlatformIO](https://platformio.org/).
* **Interfaces & Communication :** Écrans OLED, protocoles série et Wi-Fi.

## ⚖️ Licence

Ce projet est mis à disposition selon les termes de la licence **[Creative Commons Attribution 4.0 International (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/)**.

Vous êtes libre de :
* **Partager** : Copier et redistribuer le matériel sous n'importe quel format ou support.
* **Adapter** : Remixer, transformer et construire à partir du matériel pour toute utilisation, y compris commerciale.

*Sous réserve de mentionner les auteurs originaux (Attribution).*
