# Alarm Button Slide-Out Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Modify the alarm mute button in `base.html` to hide by default and slide out on hover.

**Architecture:** Use CSS transforms and transitions to animate the button's text panel sliding out from behind the icon. No JavaScript animation loops required.

**Tech Stack:** HTML, CSS (Flexbox, Transform, Transition), JavaScript (DOM events)

---

## File Structure

- **Modify:** `templates/base.html`
  - Update HTML structure for the alarm button.
  - Add CSS for default and hover states.
  - Add JS event listeners for hover and click.

## Task 1: Update HTML Structure

**Files:**
- Modify: `templates/base.html:86-130`

- [ ] **Step 1: Update alarm button HTML**

```html
<div id="alarm-btn" style="position: fixed; bottom: 20px; left: 20px; z-index: 10000; cursor: pointer; display: flex; align-items: center;">
    <!-- Icon Part (always visible) -->
    <div class="alarm-icon-part" style="
        width: 40px;
        height: 40px;
        background: rgba(25, 25, 35, 0.85);
        backdrop-filter: blur(10px);
        border: 1px solid rgba(255,255,255,0.15);
        border-radius: 12px;
        display: flex;
        align-items: center;
        justify-content: center;
        box-shadow: 0 4px 15px rgba(0,0,0,0.2);
        z-index: 2;
        flex-shrink: 0;
    ">
        <i class="fas fa-volume-up" id="alarm-icon" style="font-size: 1.1rem; color: #10b981;"></i>
    </div>
    <!-- Text Part (slides out) -->
    <div class="alarm-text-part" style="
        background: rgba(25, 25, 35, 0.85);
        backdrop-filter: blur(10px);
        border: 1px solid rgba(255,255,255,0.15);
        border-left: none;
        border-radius: 12px;
        padding: 8px 16px;
        color: white;
        font-size: 0.75rem;
        margin-left: -12px;
        padding-left: 24px;
        transform: translateX(-100%);
        opacity: 0;
        transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
        white-space: nowrap;
        box-shadow: 0 4px 15px rgba(0,0,0,0.2);
        z-index: 1;
    ">
        <span id="alarm-text">点击静音</span>
    </div>
</div>
```

## Task 2: Add CSS for Hover State

**Files:**
- Modify: `templates/base.html` (inside `<style>` tag)

- [ ] **Step 1: Add hover styles**

```css
#alarm-btn:hover .alarm-text-part {
    transform: translateX(0);
    opacity: 1;
}
```

## Task 3: Update JavaScript Event Listeners

**Files:**
- Modify: `templates/base.html` (inside `<script>` tag)

- [ ] **Step 1: Update toggleMute method**

```javascript
toggleMute() {
    this.isMuted = !this.isMuted;
    if (this.isMuted) {
        this.stopAlarm();
        document.getElementById('alarm-icon').className = 'fas fa-volume-mute';
        document.getElementById('alarm-icon').style.color = '#ef4444';
        document.getElementById('alarm-text').textContent = '点击开启';
    } else {
        document.getElementById('alarm-icon').className = 'fas fa-volume-up';
        document.getElementById('alarm-icon').style.color = '#10b981';
        document.getElementById('alarm-text').textContent = '点击静音';
    }
    return this.isMuted;
}
```

- [ ] **Step 2: Remove old event listeners**

Remove any existing `mouseenter`/`mouseleave` event listeners on `#alarm-btn` that were added previously.

## Task 4: Test and Commit

- [ ] **Step 1: Verify the button works**

1. Load the page.
2. Observe the button is partially hidden (only icon visible).
3. Hover over the icon to see the text slide out.
4. Click to toggle mute state.
5. Ensure no flickering occurs.

- [ ] **Step 2: Commit changes**

```bash
git add docs/superpowers/plans/2026-03-20-alarm-button-slide-out.md
git add docs/superpowers/specs/2026-03-20-alarm-button-slide-out-design.md
git add templates/base.html
git commit -m "feat: implement alarm button slide-out on hover"
```
