# Alarm Button Slide-Out Design

**Date:** 2026-03-20
**Status:** Approved

## 1. Problem
The alarm mute button in the bottom-left corner is always visible, which affects the UI aesthetics. It should be hidden by default and only appear when needed (on hover).

## 2. Solution
Implement a "Simple Slide-Out" mechanism:
- **Default State:** Only a small 40x40 circular icon (volume symbol) is visible, partially hidden at the left edge of the screen.
- **Hover State:** The icon slides horizontally to the right by 80px to reveal the full button area (icon + text).
- **Transition:** Smooth cubic-bezier animation over 0.3s.

## 3. Visual Design & CSS Strategy
- **Base Structure:**
  ```html
  <div id="alarm-btn">
    <div class="icon-part">...</div>
    <div class="text-part">...</div>
  </div>
  ```
- **Default State:**
  - `#alarm-btn` is fixed at bottom-left.
  - `.text-part` is positioned absolutely, initially moved left (`transform: translateX(-100%)`) and/or has `opacity: 0`.
- **Hover State:**
  - `.text-part` animates to `transform: translateX(0)` and `opacity: 1`.
- **Benefit:** This approach avoids the "flickering" issue when the mouse moves over the gap between the icon and the text.

## 4. Interaction Logic
- **Trigger:** `mouseenter` on the button container.
- **Action:** Translate the button container (or just the text panel) to the right to reveal content.
- **Reset:** On `mouseleave`, translate back to original position.
- **Click:** Toggles mute state (existing logic).

## 5. Technical Implementation
- Use CSS `transform: translateX()` for hardware-accelerated animation.
- Use CSS `transition: transform 0.3s cubic-bezier(...)`.
- No JavaScript animation loops needed.

## 6. Constraints
- Modify only `templates/base.html`.
- Ensure compatibility with existing alarm audio logic.
