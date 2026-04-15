# Image Thumbnails & Real-time Notifications Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enhance the capture system with bandwidth-efficient thumbnails and real-time WebSocket notifications for new detections.

**Architecture:**
- **Thumbnails:** Use `Pillow` during upload to generate 300px width thumbnails. Update database schema to track both paths.
- **Notifications:** Integrate `Flask-SocketIO`. Emit a `new_capture` event on successful upload.
- **Frontend:** Use Socket.IO client in `base.html` for global toasts and `alerts.html` for live table updates.

**Tech Stack:**
- Backend: Flask, Flask-SQLAlchemy, Flask-SocketIO, Pillow.
- Frontend: Socket.IO Client JS, Bootstrap Toasts, Vanilla JS.

---

### Task 1: Update Database Model

**Files:**
- Modify: `blueprints/models.py`

- [ ] **Step 1: Add thumbnail_path field to Capture model**

```python
class Capture(db.Model):
    # ... existing fields ...
    thumbnail_path = db.Column(db.String(255))
```

- [ ] **Step 2: Commit model changes**

```bash
git add blueprints/models.py
git commit -m "model: add thumbnail_path to Capture"
```

---

### Task 2: Initialize Flask-SocketIO

**Files:**
- Modify: `exts.py`
- Modify: `app.py`

- [ ] **Step 1: Update exts.py to include SocketIO**

```python
from flask_socketio import SocketIO
# ...
socketio = SocketIO(cors_allowed_origins="*")
```

- [ ] **Step 2: Update app.py to initialize and run with socketio**

```python
from exts import socketio
# ... after app config ...
socketio.init_app(app)
# ... in main block ...
socketio.run(app, host='0.0.0.0', port=5000, debug=True)
```

- [ ] **Step 3: Commit initialization changes**

```bash
git add exts.py app.py
git commit -m "feat: initialize Flask-SocketIO"
```

---

### Task 3: Thumbnail Generation & Real-time Emission

**Files:**
- Modify: `blueprints/capture.py`

- [ ] **Step 1: Update upload_capture logic**
- Add Pillow imports.
- Create thumbnail directory.
- Generate thumbnail on upload.
- Save `thumbnail_path` to database.
- Emit `new_capture` event.

```python
from PIL import Image
from exts import socketio
# ... in upload_capture ...
# Generate thumbnail
img = Image.open(filepath)
img.thumbnail((300, 300))
thumb_filename = f"thumb_{filename}"
thumb_path = os.path.join(upload_path, 'thumbnails', thumb_filename)
os.makedirs(os.path.join(upload_path, 'thumbnails'), exist_ok=True)
img.save(thumb_path)

# Save to DB
capture = Capture(..., thumbnail_path=f"captures/thumbnails/{thumb_filename}")

# Emit event
socketio.emit('new_capture', {
    'camera_id': camera_id,
    'location': location,
    'violation_type': violation_type,
    'capture_time': capture.capture_time.strftime('%Y-%m-%d %H:%M:%S'),
    'thumbnail': f"/static/captures/thumbnails/{thumb_filename}"
}, namespace='/')
```

- [ ] **Step 2: Commit backend logic**

```bash
git add blueprints/capture.py
git commit -m "feat: implement thumbnail generation and real-time emission"
```

---

### Task 4: Global Notifications UI

**Files:**
- Modify: `templates/base.html`
- Modify: `static/css/style.css`

- [ ] **Step 1: Add Socket.IO client to base.html**
- [ ] **Step 2: Implement notification toast UI and logic**

```html
<script src="https://cdnjs.cloudflare.com/ajax/libs/socket.io/4.0.1/socket.io.js"></script>
<div id="notification-container" style="position: fixed; top: 20px; right: 20px; z-index: 9999;"></div>
```

- [ ] **Step 3: Add CSS for anime-style toasts**

- [ ] **Step 4: Commit UI changes**

---

### Task 5: Live Updates in Alerts Page

**Files:**
- Modify: `templates/alerts.html`

- [ ] **Step 1: Add Socket.IO listener to refresh table or prepend row**
- [ ] **Step 2: Use thumbnail_path for preview images**

- [ ] **Step 3: Commit final changes**
