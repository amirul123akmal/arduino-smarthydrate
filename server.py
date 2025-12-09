#!/usr/bin/env python3
"""
Simple Flask server to receive a base64 image and save it to the current folder.

POST /upload-base64
 - Content-Type: application/json
 - Body JSON: {"image": "<base64-or-data-url>", "filename": "optional-name.png"}

The "image" value can be:
 - a data URL like "data:image/png;base64,iVBORw0KG..."
 - or a raw base64 string "iVBORw0KG..."
"""

import os
import re
import uuid
import base64
import imghdr
from flask import Flask, request, jsonify
from werkzeug.utils import secure_filename
from binascii import Error as BinAsciiError

app = Flask(__name__)

# Limit request size (e.g., 16 MB). Adjust as needed.
app.config['MAX_CONTENT_LENGTH'] = 16 * 1024 * 1024

# regex to parse data URLs: data:<mime>;base64,<data>
DATA_URL_RE = re.compile(r'^data:(image/[^;]+);base64,(.+)$', flags=re.I)


def detect_extension_from_bytes(data: bytes):
    """Return extension like 'png', 'jpeg', or None if unknown."""
    kind = imghdr.what(None, h=data)
    if kind == 'jpeg':
        return 'jpg'
    return kind  # may be 'png', 'gif', etc. or None


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


if __name__ == '__main__':
    # Run in debug mode for development only
    app.run(host='0.0.0.0', port=5000, debug=True)
