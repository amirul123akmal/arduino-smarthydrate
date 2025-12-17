import os
import base64
import logging
from io import BytesIO
from typing import Optional, List

import numpy as np
from PIL import Image, ImageOps
import face_recognition

logging.basicConfig(
    filename='backend.log',
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)

class FaceMatcher:
    """
    Minimal matcher that:
      - detects faces in a base64 image
      - loads face encodings from files in `faces_dir` (clears any old encodings)
      - searches a base64 image against the loaded encodings and returns the matching UUID (filename without ext)

    Expectations:
      - External process will place images in `faces_dir` named like "<UUID>.jpg" or "<UUID>.png".
      - If an image contains multiple faces, all encodings will be associated with that UUID.
    """

    def __init__(self, faces_dir: str = "faces"):
        self.faces_dir = faces_dir
        os.makedirs(self.faces_dir, exist_ok=True)
        # parallel lists: encodings[i] corresponds to uuids[i]
        self.encodings: List[np.ndarray] = []
        self.uuids: List[str] = []
        self.load_faces_from_folder()

    def detect_faces_b64(self, image_b64: str, debug_path: str = "/tmp/debug_face.jpg") -> bool:
        """
        Return True if the base64 image contains at least one detectable face, else False.
        Writes a debug image to debug_path so you can open it manually.
        """
        logging.info("Processing image: %s", type(image_b64))

        try:
            # strip data URL prefix if present
            if image_b64.startswith("data:"):
                image_b64 = image_b64.split(",", 1)[1]

            img_bytes = base64.b64decode(image_b64)

            from pathlib import Path
            BASE = Path(__file__).resolve().parent
            out = BASE / "Storage" / "image_debug.jpg"
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_bytes(img_bytes)

            # Load image using PIL to preserve EXIF orientation, then convert to RGB numpy array
            img = Image.open(BytesIO(img_bytes))
            img = ImageOps.exif_transpose(img).convert("RGB")
            img_np = np.array(img)  # dtype should be uint8 and shape (H,W,3)

            # First check for face locations (faster feedback)
            locations = face_recognition.face_locations(img_np, model="hog", number_of_times_to_upsample=1)
            logging.info("face_locations found: %s", locations)

            # If none, try upsampling (helps with small/low-res faces)
            if len(locations) == 0:
                locations = face_recognition.face_locations(img_np, model="hog", number_of_times_to_upsample=2)
                logging.info("after upsample, face_locations found: %s", locations)

            # Optionally try the CNN model if you have it and want more accuracy (slower, needs dlib-cnn)
            # locations = face_recognition.face_locations(img_np, model="cnn")

            # If we have locations, get encodings
            encs = face_recognition.face_encodings(img_np, known_face_locations=locations) if locations else []
            logging.info("num encodings: %d", len(encs))
            self.load_faces_from_folder()
            return len(encs) > 0

        except Exception:
            logging.exception("Error in detect_faces_b64")
            return False

    def load_faces_from_folder(self) -> None:
        """
        Clear existing encodings and load all face encodings from files in self.faces_dir.
        File names are expected to be "<UUID>.<ext>". For each face found in a file,
        an encoding is added and associated with that file's UUID (filename without extension).
        """
        self.encodings.clear()
        self.uuids.clear()

        if not os.path.isdir(self.faces_dir):
            return

        for fname in os.listdir(self.faces_dir):
            if not fname.lower().endswith((".jpg", ".jpeg", ".png")):
                continue
            uuid = os.path.splitext(fname)[0]
            path = os.path.join(self.faces_dir, fname)
            try:
                img = face_recognition.load_image_file(path)
                encs = face_recognition.face_encodings(img)
                if not encs:
                    # No face found in this file — skip it
                    continue
                # Associate each encoding (in case of multiple faces) with the same uuid
                for enc in encs:
                    self.encodings.append(enc)
                    self.uuids.append(uuid)
            except Exception:
                # silently skip unreadable/invalid files
                continue

    def search_image_b64(self, image_b64: str, tolerance: float = 0.6) -> Optional[str]:
        """
        Search the provided base64 image against the loaded encodings.
        Returns the matched UUID (filename without extension) of the first match, or None if not found.
        If the query image contains no detectable face, returns None.
        """
        try:
            img_bytes = base64.b64decode(image_b64)
            from pathlib import Path
            BASE = Path(__file__).resolve().parent
            out = BASE / "Storage" / "image_debug.jpg"
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_bytes(img_bytes)
            logging.info("Image saved to %s", out)
            
            img = Image.open(BytesIO(img_bytes)).convert("RGB")
            img_np = np.array(img)
            query_encs = face_recognition.face_encodings(img_np)
            if not query_encs:
                return None
            query_enc = query_encs[0]  # use the first face found in the query image
        except Exception:
            return None
        
        if not self.encodings:
            return None
        matches = face_recognition.compare_faces(self.encodings, query_enc, tolerance=tolerance)
        if True in matches:
            idx = matches.index(True)
            return self.uuids[idx]
        return None

    def loaded_count(self) -> int:
        """Return number of encodings currently loaded in memory."""
        return len(self.encodings)
