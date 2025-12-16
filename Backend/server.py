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
from sqliteDB import SqliteDB
from flask import g, current_app, Flask, request, jsonify, render_template
from werkzeug.utils import secure_filename
from binascii import Error as BinAsciiError

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

# regex to parse data URLs: data:<mime>;base64,<data>
DATA_URL_RE = re.compile(r'^data:(image/[^;]+);base64,(.+)$', flags=re.I)


def detect_extension_from_bytes(data: bytes):
    """Return extension like 'png', 'jpeg', or None if unknown."""
    kind = imghdr.what(None, h=data)
    if kind == 'jpeg':
        return 'jpg'
    return kind  # may be 'png', 'gif', etc. or None

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
        provided_name = payload.get('filename') if payload else None
    else:
        base64_str = request.form.get('image') or request.values.get('image')
        provided_name = request.form.get('filename') or request.values.get('filename')

    if not base64_str:
        return jsonify({"error": "No 'image' data provided"}), 400

    # If it's a data URL, extract the mime and base64 payload
    m = DATA_URL_RE.match(base64_str.strip())
    mime_type = None
    if m:
        mime_type = m.group(1).lower()
        base64_payload = m.group(2)
    else:
        # assume raw base64 string
        base64_payload = base64_str.strip()

    # Remove whitespace/newlines that might be present
    base64_payload = re.sub(r'\s+', '', base64_payload)

    try:
        image_bytes = base64.b64decode(base64_payload, validate=True)
    except (BinAsciiError, ValueError) as e:
        return jsonify({"error": "Invalid base64 data", "details": str(e)}), 400

    # Try to determine extension
    ext = None
    if mime_type:
        # map common mime to extension
        if mime_type == 'image/png':
            ext = 'png'
        elif mime_type in ('image/jpeg', 'image/jpg'):
            ext = 'jpg'
        elif mime_type == 'image/gif':
            ext = 'gif'
        elif mime_type == 'image/webp':
            ext = 'webp'
        # else fallback to imghdr detection below

    if not ext:
        ext = detect_extension_from_bytes(image_bytes)

    if not ext:
        return jsonify({"error": "Could not determine image type"}), 400

    # Determine filename
    if provided_name:
        # sanitize provided filename but ensure extension matches detected extension
        safe_name = secure_filename(provided_name)
        name_root, name_ext = os.path.splitext(safe_name)
        if name_ext:
            # if provided extension is different, override with detected ext
            file_name = f"{name_root}.{ext}"
        else:
            file_name = f"{safe_name}.{ext}"
    else:
        # generate unique filename
        file_name = f"{uuid.uuid4().hex}.{ext}"

    save_path = os.path.join(os.getcwd(), file_name)

    # Write the file
    try:
        with open(save_path, 'wb') as f:
            f.write(image_bytes)
    except OSError as e:
        return jsonify({"error": "Failed to save file", "details": str(e)}), 500

    return jsonify({"saved_as": file_name, "path": save_path}), 201

from datetime import datetime

@app.route('/add-user', methods=['POST'])
def add_user():
    """
    Adds a new user with their daily hydration limit and an array of pains.
    Expects JSON payload: {"name": "User Name", "daily_limit": 2.5, "pains": ["headache", "fatigue"]}
    """
    if not request.is_json:
        return jsonify({"error": "Request must be JSON"}), 400
    app.logger.info("Request received: %s", request.get_json())
    payload = request.get_json()
    name = payload.get('name')
    daily_limit = payload.get('daily_limit')
    pains = payload.get('pains')

    if not name or not isinstance(name, str) or not name.strip():
        return jsonify({"error": "Invalid or missing 'name'"}), 400
    if not isinstance(daily_limit, (int, float)) or daily_limit <= 0:
        return jsonify({"error": "Invalid or missing 'daily_limit'. Must be a positive number."}), 400
    if not isinstance(pains, list):
        return jsonify({"error": "Invalid or missing 'pains'. Must be an array."}), 400

    # Convert pains list to a JSON string for storage
    import json
    # pains_json_str = json.dumps(pains 
    daily_limit_ml = daily_limit * 1000
    try:
        # Assuming a 'users' table with columns: id, name, daily_limit_liters, pains_json
        # And that SqliteDB has an execute_query method for inserts
        query = """
            INSERT INTO users (name, image_path, daily_limit_ml, created_at)
            VALUES (?, ?, ?, ?);
        """
        # The execute_query method should return the last inserted row ID
        user_id = db.execute(query, (name.strip(), '', daily_limit_ml, datetime.now()))
        app.logger.info("User added successfully: %s", user_id.fetchone())
        return jsonify({"message": "User feature added successfully", "user_id": user_id.fetchone()}), 201
    except Exception as e:
        app.logger.error(f"Error adding user feature: {e}")
        return jsonify({"error": "Failed to add user feature", "details": str(e)}), 500


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
    db.fetchall("select * from users")
    users = [
        # {
        #     'id': 1,
        #     'name': 'Alice',
        #     'intake_liters': 1.5,
        #     'daily_limit_liters': 2.0,
        #     'last_event_time': '10:30 AM',
        #     'events': [
        #         {'time': '10:30 AM', 'volume_ml': 250},
        #         {'time': '09:15 AM', 'volume_ml': 200},
        #         {'time': '08:00 AM', 'volume_ml': 300}
        #     ]
        # },
        # {
        #     'id': 2,
        #     'name': 'Bob',
        #     'intake_liters': 2.1,
        #     'daily_limit_liters': 2.5,
        #     'last_event_time': '11:45 AM',
        #     'events': [
        #         {'time': '11:45 AM', 'volume_ml': 500},
        #         {'time': '09:00 AM', 'volume_ml': 500}
        #     ]
        # },
        # {
        #     'id': 3,
        #     'name': 'Charlie',
        #     'intake_liters': 0.5,
        #     'daily_limit_liters': 2.0,
        #     'last_event_time': '07:30 AM',
        #     'events': [
        #         {'time': '07:30 AM', 'volume_ml': 500}
        #     ]
        # }
    ]

    return render_template(
        'index.html',
        system_status=system_status,
        overall=overall,
        users=users
    )

if __name__ == '__main__':
    # Run in debug mode for development only
    app.run(host='0.0.0.0', port=5000, debug=True)
