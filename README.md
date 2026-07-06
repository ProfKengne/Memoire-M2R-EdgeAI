# Algorithme d'Apprentissage Automatique d'un Système de Surveillance Edge AI

Projet de recherche dans le cadre du Master 2 Recherche en Sciences de l'Ingénieur (Physique Appliquée).

## 📁 Structure du Projet

*   `/ai_models` : Scripts d'entraînement (PyTorch, YOLOv8), datasets et fichiers de poids (`.pt`).
*   `/pc_inference` : Pipeline de détection en temps réel (Python, OpenCV) et journalisation des alertes JSON.
*   `/firmware_esp32` : Code source C++ (Arduino/PlatformIO) pour la gestion des alertes matérielles, de l'affichage OLED et de la communication (Série/Wi-Fi).
*   `/docs` : Documentation technique et schémas fonctionnels.

## 🛠️ Technologies utilisées
*   **Edge AI & Vision :** YOLOv8, OpenCV, PyTorch
*   **Matériel & Microcontrôleurs :** ESP32, Écrans OLED, Liaisons Série & Wi-Fi
