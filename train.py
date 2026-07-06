# Script d'entraînement et de validation du modèle YOLOv8n
#import torch #import torch travaille désormais dans l'ombre d'Ultralytics
import os  # import os sécurise la structure de vos dossiers locaux !
from ultralytics import YOLO

def main():
    # 1. Initialisation et chargement du modele de base (Pre-entraine)
    # Le suffixe 'n' designe l'architecture Nano (3.2 millions de parametres)
    model = YOLO('yolov8n.pt')

    # 2. Definition des chemins d'acces du projet
    dataset_config = os.path.abspath("data.yaml")
    project_dir = "runs/detect"
    experiment_name = "run_production_yolov8n"

    print(f"[INFO] Configuration du jeu de donnees : {dataset_config}")
    print("[INFO] Debut du cycle complet d'entrainement (100 epoques)...")

    # 3. Lancement de l'entrainement avec les hyperparametres optimises
    results = model.train(
        data=dataset_config,      # Fichier de description des classes et chemins
        epochs=100,               # Nombre de cycles complets
        imgsz=640,                # Resolution matricielle des trames d'entree
        batch=16,                 # Taille du lot (adaptee aux ressources VRAM)
        workers=4,                # Nombre de threads pour le chargement des donnees
        optimizer='SGD',          # Optimiseur stochastique selectionne
        lr0=0.01,                 # Taux d'apprentissage initial
        momentum=0.937,           # Facteur de quantite de mouvement
        weight_decay=0.0005,      # Regularisation L2 pour eviter le surapprentissage
        device=0,                 # Index de l'unite graphique (GPU CUDA)
        project=project_dir,      # Dossier racine de supervision
        name=experiment_name,     # Nom specifique de la simulation
        plots=True                # Generation automatique des courbes de performance
    )
    
    print(f"[SUCCESS] Entrainement termine. Les poids sont sauvegardes dans {project_dir}/{experiment_name}/weights/best.pt")

if __name__ == '__main__':
    main()