# Script d'inférence temps réel Edge AI, Suivi de cibles, Enregistrement Vidéo et Logs JSON
# + Mesure des performances temporelles de bout en bout (pour l'article)
import os
import cv2
import time
import json
import csv
import statistics
import serial
from ultralytics import YOLO

# ⏱️ CONFIGURATION DES MESURES
TRAMES_ECHAUFFEMENT = 30  # Trames ignorées au début (chargement CUDA, ouverture caméra)
# Durée de la session de mesure en secondes (0 = illimitée, arrêt par la touche 'q')
DUREE_MESURE_S = float(os.environ.get("DUREE_MESURE_S", "300"))

# ⚙️ CONFIGURATION DU PORT SÉRIE DE L'ESP32 (Vérifié sur COM6)
try:
    esp32 = serial.Serial(port='COM6', baudrate=115200, timeout=0.1)
    print("🔌 Connexion série établie avec l'ESP32 sur le port COM6 !")
except Exception as e:
    print(f"⚠️ Impossible d'ouvrir le port série : {e}. Le script tournera sans envoyer de données.")
    esp32 = None


def resume_statistique(valeurs):
    """Moyenne, écart-type, minimum, maximum et percentiles d'une liste de mesures."""
    v = sorted(valeurs)
    n = len(v)
    return {
        "n": n,
        "moyenne": round(statistics.mean(v), 2),
        "ecart_type": round(statistics.stdev(v), 2) if n > 1 else 0.0,
        "min": round(v[0], 2),
        "p5": round(v[int(0.05 * (n - 1))], 2),
        "mediane": round(statistics.median(v), 2),
        "p95": round(v[int(0.95 * (n - 1))], 2),
        "max": round(v[-1], 2),
    }


def ouvrir_camera(source_video):
    """Ouvre la caméra en vérifiant qu'elle livre de vraies images (ni échec, ni image noire).
    Essaie d'abord le pilote Windows par défaut (MSMF), puis DirectShow."""
    for nom, api in (("MSMF", cv2.CAP_MSMF), ("DirectShow", cv2.CAP_DSHOW)):
        cap = cv2.VideoCapture(source_video, api)
        if not cap.isOpened():
            continue
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
        for _ in range(15):
            ok, trame = cap.read()
            if ok and trame is not None and trame.mean() > 2.0:
                print(f"📷 Caméra ouverte avec le pilote {nom}.")
                return cap
        cap.release()
    print("❌ La caméra ne livre aucune image exploitable. Vérifiez qu'aucune autre "
          "application (ou une session précédente de ce script) ne l'utilise, et que "
          "l'obturateur de confidentialité est ouvert.")
    return None

def executer_inference_suivi_enregistrement(chemin_poids, source_video):
    print("🔄 Chargement du modèle YOLOv8 personnalisé...")
    model = YOLO(chemin_poids)
    print("✅ Modèle chargé avec succès !")

    cap = ouvrir_camera(source_video)
    if cap is None:
        return

    frame_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    frame_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps_webcam = int(cap.get(cv2.CAP_PROP_FPS))

    if fps_webcam <= 0 or fps_webcam > 60:  # DirectShow renvoie -1
        fps_webcam = 20

    horodatage_base = time.strftime("%Y%m%d_%H%M%S", time.localtime())

    # 🎥 Config enregistreur vidéo
    nom_fichier_sortie = f"Video_surveillance_Ibrahim_{horodatage_base}.avi"
    fourcc = cv2.VideoWriter_fourcc(*'XVID')
    out = cv2.VideoWriter(nom_fichier_sortie, fourcc, fps_webcam, (frame_width, frame_height))

    # 📝 CONFIGURATION DES FICHIERS DE LOG ET DE MESURES
    nom_fichier_log = f"Journal_Alertes_Ibrahim_{horodatage_base}.jsonl"
    nom_fichier_mesures = f"Mesures_temps_SERIE_Ibrahim_{horodatage_base}.csv"
    nom_fichier_resume = f"Resume_mesures_SERIE_Ibrahim_{horodatage_base}.json"
    print(f"📄 Les métadonnées Edge AI seront enregistrées dans : {nom_fichier_log}")
    print(f"📹 Vidéo enregistrée sous : {nom_fichier_sortie}")
    print(f"⏱️ Mesures temporelles enregistrées dans : {nom_fichier_mesures}")
    print(f"   Résolution réelle de la caméra : {frame_width}x{frame_height}, "
          f"FPS annoncé : {int(cap.get(cv2.CAP_PROP_FPS))}")
    print("\n🚀 Système actif. Appuyez sur 'q' pour arrêter.")

    mesures = []
    index_trame = 0
    debut_session = time.perf_counter()

    # Ouverture du fichier de log en mode "append" (ajout) avec encodage UTF-8
    with open(nom_fichier_log, mode='a', encoding='utf-8') as fichier_log, \
         open(nom_fichier_mesures, mode='w', newline='', encoding='utf-8') as fichier_mesures:

        ecrivain = csv.writer(fichier_mesures, delimiter=';')
        ecrivain.writerow(["trame", "t_session_s", "nb_personnes",
                           "capture_ms", "track_ms", "envoi_log_ms",
                           "affichage_ms", "total_ms", "fps_bout_en_bout"])

        while True:
            t0 = time.perf_counter()
            ret, frame = cap.read()
            if not ret:
                print("❌ Lecture de trame impossible : arrêt.")
                break
            t1 = time.perf_counter()  # fin capture

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
            t2 = time.perf_counter()  # fin détection + suivi

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

                # Forcer l'écriture physique sur le disque sans attendre le buffer
                fichier_log.flush()

                print(f"[EDGE ALERT] Cibles : {nb_personnes} | Logs synchronisés dans le fichier.")
            t3 = time.perf_counter()  # fin envoi série + journal

            # FPS de la trame précédente (le calcul de la trame courante n'est complet qu'après affichage)
            fps_affiche = mesures[-1]["fps"] if mesures else 0.0

            annotated_frame = results[0].plot()

            cv2.putText(annotated_frame, f"Format: 720p HD (16:9)", (20, 40), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
            cv2.putText(annotated_frame, f"Cibles: {nb_personnes}", (20, 70), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
            cv2.putText(annotated_frame, f"System FPS: {fps_affiche:.1f}", (20, 100), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            # Horloge en millisecondes : utile pour mesurer la latence jusqu'au voyant
            # en filmant au ralenti l'écran du PC et l'ESP32 dans le même plan.
            horloge_ms = int((time.perf_counter() - debut_session) * 1000)
            cv2.putText(annotated_frame, f"t = {horloge_ms} ms", (20, 130), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

            out.write(annotated_frame)
            cv2.imshow("Systeme de Surveillance M2R - Edge AI", annotated_frame)
            touche = cv2.waitKey(1) & 0xFF
            t4 = time.perf_counter()  # fin annotation + écriture vidéo + affichage

            # ⏱️ Enregistrement des mesures de cette trame (boucle complète t0 -> t4)
            total_ms = (t4 - t0) * 1000.0
            fps_trame = 1000.0 / total_ms if total_ms > 0 else 0.0
            index_trame += 1
            if index_trame > TRAMES_ECHAUFFEMENT:
                m = {
                    "capture": (t1 - t0) * 1000.0,
                    "track": (t2 - t1) * 1000.0,
                    "envoi_log": (t3 - t2) * 1000.0,
                    "affichage": (t4 - t3) * 1000.0,
                    "total": total_ms,
                    "fps": fps_trame,
                }
                mesures.append(m)
                ecrivain.writerow([index_trame, round(t4 - debut_session, 3), nb_personnes,
                                   round(m["capture"], 2), round(m["track"], 2),
                                   round(m["envoi_log"], 2), round(m["affichage"], 2),
                                   round(total_ms, 2), round(fps_trame, 2)])

            if DUREE_MESURE_S > 0 and (t4 - debut_session) >= DUREE_MESURE_S:
                print(f"⏹️ Durée de mesure atteinte ({DUREE_MESURE_S:.0f} s).")
                touche = ord('q')
            if touche == ord('q'):
                break

    duree_session = time.perf_counter() - debut_session

    # Nettoyage
    cap.release()
    out.release()
    if esp32:
        esp32.close()
    cv2.destroyAllWindows()

    # 📊 Résumé statistique des mesures
    if len(mesures) < 2:
        print("\n⚠️ Pas assez de trames mesurées pour calculer des statistiques "
              f"(il faut plus de {TRAMES_ECHAUFFEMENT} trames).")
    else:
        resume = {
            "date": horodatage_base,
            "duree_session_s": round(duree_session, 1),
            "trames_mesurees": len(mesures),
            "trames_echauffement_ignorees": TRAMES_ECHAUFFEMENT,
            "resolution": f"{frame_width}x{frame_height}",
            "seuil_confiance": 0.35,
            "liaison": "Série USB 115200 bauds",
            "fps_bout_en_bout": resume_statistique([m["fps"] for m in mesures]),
            "temps_total_ms": resume_statistique([m["total"] for m in mesures]),
            "capture_ms": resume_statistique([m["capture"] for m in mesures]),
            "detection_suivi_ms": resume_statistique([m["track"] for m in mesures]),
            "envoi_serie_journal_ms": resume_statistique([m["envoi_log"] for m in mesures]),
            "annotation_video_affichage_ms": resume_statistique([m["affichage"] for m in mesures]),
        }
        with open(nom_fichier_resume, mode='w', encoding='utf-8') as f:
            json.dump(resume, f, indent=2, ensure_ascii=False)

        fps = resume["fps_bout_en_bout"]
        print("\n📊 RÉSUMÉ DES MESURES (à reporter dans l'article)")
        print(f"   Durée : {resume['duree_session_s']} s, trames mesurées : {resume['trames_mesurees']}")
        print(f"   FPS bout en bout : moyenne {fps['moyenne']}, écart-type {fps['ecart_type']}, "
              f"min {fps['min']}, 5e percentile {fps['p5']}")
        for cle, nom in [("capture_ms", "Capture"), ("detection_suivi_ms", "Détection + suivi"),
                         ("envoi_serie_journal_ms", "Envoi série + journal"),
                         ("annotation_video_affichage_ms", "Annotation + vidéo + affichage"),
                         ("temps_total_ms", "TOTAL par trame")]:
            s = resume[cle]
            print(f"   {nom:<32}: moyenne {s['moyenne']} ms, médiane {s['mediane']} ms, p95 {s['p95']} ms")
        print(f"   Résumé complet enregistré dans : {nom_fichier_resume}")

    print(f"\n👋 Session terminée. Fichiers sauvegardés avec succès.")


if __name__ == '__main__':
    CHEMIN_POIDS = "runs/detect/run_production_yolov8n/weights/best.pt"
    SOURCE_VIDEO = 0
    executer_inference_suivi_enregistrement(CHEMIN_POIDS, SOURCE_VIDEO)
