# Script d'inférence temps réel Edge AI, Suivi de cibles, Enregistrement Vidéo et Logs JSON
import cv2  
import time
import json
import serial
from ultralytics import YOLO

# ⚙️ CONFIGURATION DU PORT SÉRIE DE L'ESP32 (Vérifié sur COM6)
try:
    esp32 = serial.Serial(port='COM6', baudrate=115200, timeout=0.1)
    print("🔌 Connexion série établie avec l'ESP32 sur le port COM6 !")
except Exception as e:
    print(f"⚠️ Impossible d'ouvrir le port série : {e}. Le script tournera sans envoyer de données.")
    esp32 = None

def executer_inference_suivi_enregistrement(chemin_poids, source_video):
    print("🔄 Chargement du modèle YOLOv8 personnalisé...")
    model = YOLO(chemin_poids)
    print("✅ Modèle chargé avec succès !")
    
    cap = cv2.VideoCapture(source_video)
    if not cap.isOpened():
        print("❌ Erreur : Impossible d'ouvrir la source vidéo.")
        return

    # Configuration vidéo 16:9
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
    
    frame_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    frame_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps_webcam = int(cap.get(cv2.CAP_PROP_FPS))
    
    if fps_webcam == 0 or fps_webcam > 60:
        fps_webcam = 20

    horodatage_base = time.strftime("%Y%m%d_%H%M%S", time.localtime())
    
    # 🎥 Config enregistreur vidéo
    nom_fichier_sortie = f"Video_surveillance_Ibrahim_{horodatage_base}.avi"
    fourcc = cv2.VideoWriter_fourcc(*'XVID')
    out = cv2.VideoWriter(nom_fichier_sortie, fourcc, fps_webcam, (frame_width, frame_height))
    
    # 📝 CONFIGURATION DU FICHIER DE LOG LOCAL (.jsonl)
    nom_fichier_log = f"Journal_Alertes_Ibrahim_{horodatage_base}.jsonl"
    print(f"📄 Les métadonnées Edge AI seront enregistrées dans : {nom_fichier_log}")
    print(f"📹 Vidéo enregistrée sous : {nom_fichier_sortie}")
    print("\n🚀 Système actif. Appuyez sur 'q' pour arrêter.")

    # Ouverture du fichier de log en mode "append" (ajout) avec encodage UTF-8
    with open(nom_fichier_log, mode='a', encoding='utf-8') as fichier_log:
        while True:
            debut_trame = time.time()
            ret, frame = cap.read()
            if not ret:
                break

            results = model.track(
                source=frame, 
                persist=True, 
                conf=0.35, 
                iou=0.50, 
                classes=[0], 
                device=0, 
                verbose=False, 
                imgsz=640
            )
            
            nb_personnes = 0
            liste_alertes = []
            horodatage_actuel = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())

            for r in results:
                boxes = r.boxes
                nb_personnes = len(boxes)
                for box in boxes:
                    coords = box.xyxy[0].tolist()
                    confiance = float(box.conf[0])
                    track_id = int(box.id[0].item()) if box.id is not None else -1
                    
                    alerte_metadonnees = {
                        "timestamp": horodatage_actuel,
                        "tracking_id": track_id,
                        "classe": "person",
                        "confiance": round(confiance, 4),
                        "bbox_pixels": [round(c, 1) for c in coords]
                    }
                    liste_alertes.append(alerte_metadonnees)

            # 📡 1. Envoi immédiat à l'ESP32
            if esp32 and esp32.is_open:
                esp32.write(f"{nb_personnes}\n".encode())

            # 📝 2. Écriture immédiate dans le fichier de log local
            if liste_alertes:
                for alerte in liste_alertes:
                    # Conversion de l'alerte individuelle en ligne JSON et écriture
                    fichier_log.write(json.dumps(alerte) + "\n")
                
                # Optionnel : Forcer l'écriture physique sur le disque sans attendre le buffer
                fichier_log.flush()
                
                print(f"[EDGE ALERT] Cibles : {nb_personnes} | Logs synchronisés dans le fichier.")

            # Calcul des performances temporelles
            fin_trame = time.time()
            temps_traitement = fin_trame - debut_trame
            fps_reel = 1.0 / temps_traitement if temps_traitement > 0 else 0.0

            annotated_frame = results[0].plot()

            cv2.putText(annotated_frame, f"Format: 720p HD (16:9)", (20, 40), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
            cv2.putText(annotated_frame, f"Cibles: {nb_personnes}", (20, 70), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
            cv2.putText(annotated_frame, f"System FPS: {fps_reel:.1f}", (20, 100), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

            out.write(annotated_frame)
            cv2.imshow("Systeme de Surveillance M2R - Edge AI", annotated_frame)

            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

    # Nettoyage
    cap.release()
    out.release()
    if esp32:
        esp32.close()
    cv2.destroyAllWindows()
    print(f"\n👋 Session terminee. Fichiers sauvegardés avec succès.")

if __name__ == '__main__':
    CHEMIN_POIDS = "runs/detect/run_production_yolov8n-2/weights/best.pt"
    SOURCE_VIDEO = 0  
    executer_inference_suivi_enregistrement(CHEMIN_POIDS, SOURCE_VIDEO)
