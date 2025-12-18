#!/usr/bin/env python3
"""
Simple Flask server to receive a base64 image and save it to the current folder.

The "image" value can be:
 - a data URL like "data:image/png;base64,iVBORw0KG..."
 - or a raw base64 string "iVBORw0KG..."
"""

import os
import re
import uuid
import base64
import imghdr
import logging
import json
from sqliteDB import SqliteDB
from user import delete_user
from flask import g, current_app, Flask, request, jsonify, render_template
from werkzeug.utils import secure_filename
from binascii import Error as BinAsciiError

from face import FaceMatcher

# Configure logging to file
logging.basicConfig(
    filename='backend.log',
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)

app = Flask(__name__, template_folder="views")

# Limit request size (e.g., 16 MB). Adjust as needed.
app.config['MAX_CONTENT_LENGTH'] = 16 * 1024 * 1024

# Initialize the database
db = SqliteDB("app.db")
db.init_app(app)

face_manager = FaceMatcher()

# regex to parse data URLs: data:<mime>;base64,<data>
DATA_URL_RE = re.compile(r'^data:(image/[^;]+);base64,(.+)$', flags=re.I)


def detect_extension_from_bytes(data: bytes):
    """Return extension like 'png', 'jpeg', or None if unknown."""
    kind = imghdr.what(None, h=data)
    if kind == 'jpeg':
        return 'jpg'
    return kind  # may be 'png', 'gif', etc. or None



@app.route('/refresh-folder')
def refresh_folder():
    face_manager.load_faces_from_folder()
    # Convert numpy arrays to lists for serialization and printing
    encodings_list = [enc.tolist() for enc in face_manager.encodings]
    print(encodings_list)
    return jsonify({"message": "Folder refreshed successfully", "encodings": encodings_list}), 200

"""
POST /upload-base64
 - Content-Type: application/json
 - Body JSON: {"image": "<base64-or-data-url>", "filename": "optional-name.png"}
"""

@app.route('/upload-base64', methods=['POST'])
def upload_base64():
    """
    Accept JSON { "image": "<base64-or-data-url>", "filename": "optional.png" }
    or form field 'image' and optional 'filename'.
    """
    # accept JSON or form
    if request.is_json:
        payload = request.get_json()
        base64_str = payload.get('image') if payload else None
    else:
        base64_str = request.form.get('image')

    if not base64_str:
        return jsonify({"error": "No 'image' data provided"}), 400

    # result = face_manager.search_image_b64(base64_str)
    result = face_manager.search_image_b64(base64_str)
    
    logging.info("Result: %s", result)
    if result:
        return jsonify({"message": "Image processed successfully", "result": result}), 201
    if not result:
        return jsonify({"error": "No face detected in image"}), 400

from datetime import datetime

@app.route('/add-user', methods=['POST'])
def add_user():
    """
    Adds a new user with their daily hydration limit and an array of pains.
    Expects JSON payload: {
        "name": "User Name", 
        "daily_limit": 2.5, 
        "pains": ["headache"],
        "image": "base64..."
    }
    """
    if not request.is_json:
        return jsonify({"error": "Request must be JSON"}), 400
    
    # Avoid logging full base64 image
    payload = request.get_json()
    log_payload = payload.copy()
    if 'image' in log_payload:
        log_payload['image'] = '<base64_data_hidden>'
    app.logger.info("Request received: %s", log_payload)

    name = payload.get('name')
    daily_limit = payload.get('daily_limit')
    pains = payload.get('pains')
    bb64 = payload.get('image')

    if not name or not isinstance(name, str) or not name.strip():
        return jsonify({"error": "Invalid or missing 'name'"}), 400
    if not isinstance(daily_limit, (int, float)) or daily_limit <= 0:
        return jsonify({"error": "Invalid or missing 'daily_limit'. Must be a positive number."}), 400
    # pains is optional but if present must be list
    if pains is not None and not isinstance(pains, list):
        return jsonify({"error": "Invalid 'pains'. Must be an array."}), 400

    # Process Image
    logging.info("Processing image: %s", type(bb64))
    uuid = face_manager.detect_faces_b64(bb64)
    # Explicitly store the image
    image_path = None
    if bb64:
        m = DATA_URL_RE.match(bb64.strip())
        mime_type = None
        base64_payload = None
        if m:
            mime_type = m.group(1).lower()
            base64_payload = m.group(2)
        else:
            base64_payload = bb64.strip()

        base64_payload = re.sub(r'\s+', '', base64_payload)

        try:
            image_bytes = base64.b64decode(base64_payload, validate=True)
        except (BinAsciiError, ValueError) as e:
            app.logger.error(f"Invalid base64 data for image saving: {e}")
            return jsonify({"error": "Invalid base64 data for image saving", "details": str(e)}), 400

        ext = None
        if mime_type:
            if mime_type == 'image/png':
                ext = 'png'
            elif mime_type in ('image/jpeg', 'image/jpg'):
                ext = 'jpg'
            elif mime_type == 'image/gif':
                ext = 'gif'
            elif mime_type == 'image/webp':
                ext = 'webp'

        if not ext:
            ext = detect_extension_from_bytes(image_bytes)

        if not ext:
            app.logger.error("Could not determine image type for saving.")
            return jsonify({"error": "Could not determine image type for saving"}), 400

        # Use the generated UUID as the filename
        file_name = f"{name}.{ext}"
        # Create a dedicated directory for user images
        user_images_dir = os.path.join(os.getcwd(), "faces")
        os.makedirs(user_images_dir, exist_ok=True)
        image_path = os.path.join(user_images_dir, file_name)

        try:
            with open(image_path, 'wb') as f:
                f.write(image_bytes)
        except OSError as e:
            app.logger.error(f"Failed to save image file: {e}")
            return jsonify({"error": "Failed to save image file", "details": str(e)}), 500
    else:
        app.logger.warning("No image data provided for user, image_path will be None.")

    app.logger.info("Face detected with UUID: %s", uuid)
    if not uuid:
        return jsonify({"error": "no face found in image"}), 500

    daily_limit_ml = daily_limit * 1000
    try:
        query = """
            INSERT INTO users (name, image_path, daily_limit_ml, created_at)
            VALUES (?, ?, ?, ?);
        """
        user_id_cursor = db.execute(query, (name.strip(), uuid, daily_limit_ml, datetime.now()))
        user_id = user_id_cursor.fetchone()[0] if user_id_cursor.fetchone() else user_id_cursor.lastrowid
        
        app.logger.info("User added successfully with ID: %s", user_id)
        return jsonify({"message": "User added successfully", "user_id": user_id}), 201
    except Exception as e:
        app.logger.error(f"Error adding user: {e}")
        return jsonify({"error": "Failed to add user", "details": str(e)}), 500


@app.route('/delete-users', methods=['POST'])
def delete_user_route():
    """
    Deletes a user by ID.
    Expects JSON payload: {"user_id": 123}
    """
    if not request.is_json:
        return jsonify({"error": "Request must be JSON"}), 400
        
    payload = request.get_json()
    user_id = payload.get('user_id')
    
    if not user_id:
        return jsonify({"error": "Missing 'user_id'"}), 400
        
    try:
        delete_user(db, user_id)
        app.logger.info(f"User {user_id} deleted successfully")
        return jsonify({"message": "User deleted successfully"}), 200
    except Exception as e:
        app.logger.error(f"Error deleting user: {e}")
        return jsonify({"error": "Failed to delete user", "details": str(e)}), 500


@app.route('/')
def index():  
    # Mock data for the dashboard
    system_status = {'online': True}
    
    overall = {
        # 'percent': 65,
        # 'avg_liters': 1.2,
        # 'total_liters': 3.6,
        # 'total_goal_liters': 6.0,
        # 'last_sync': datetime.now().strftime("%H:%M:%S")
    }
    users = db.fetchall("select * from users")
    app.logger.info("Users: %s", users)

    return render_template(
        'index.html',
        system_status=system_status,
        overall=overall,
        users=users
    )

if __name__ == '__main__':
    # Run in debug mode for development only
    app.run(host='0.0.0.0', port=5000, debug=True)
