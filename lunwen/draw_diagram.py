#!/usr/bin/env python3
"""Generate a professional YOLOv8 architecture diagram as JPG."""

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
fm.fontManager.addfont('/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc')
fm.fontManager.addfont('/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc')
plt.rcParams['font.family'] = 'Noto Sans CJK JP'
import matplotlib.patches as patches
from matplotlib.patches import FancyBboxPatch
import matplotlib.patheffects as pe

fig, ax = plt.subplots(1, 1, figsize=(16, 22))
ax.set_xlim(0, 16)
ax.set_ylim(0, 22)
ax.axis('off')
fig.patch.set_facecolor('white')

# Colors
COL_BG      = '#F8F9FA'
COL_BB      = '#2563EB'   # Backbone blue
COL_NECK    = '#059669'   # Neck green
COL_HEAD    = '#DC2626'   # Head red
COL_ARROW   = '#374151'
COL_TEXT    = '#111827'
COL_SUBTEXT = '#6B7280'
COL_GRID    = '#E5E7EB'
COL_BORDER  = '#D1D5DB'

def draw_box(ax, x, y, w, h, label, sublabel='', color='#2563EB', fontsize=9):
    rect = FancyBboxPatch((x, y), w, h,
        boxstyle="round,pad=0.05",
        facecolor=color, edgecolor='white', linewidth=1.5, alpha=0.9)
    ax.add_patch(rect)
    if sublabel:
        ax.text(x + w/2, y + h/2 + 0.12, label, ha='center', va='center',
            fontsize=fontsize, fontweight='bold', color='white', fontfamily='sans-serif')
        ax.text(x + w/2, y + h/2 - 0.15, sublabel, ha='center', va='center',
            fontsize=fontsize-2, color='white', fontfamily='sans-serif', style='italic')
    else:
        ax.text(x + w/2, y + h/2, label, ha='center', va='center',
            fontsize=fontsize, fontweight='bold', color='white', fontfamily='sans-serif')

def draw_arrow(ax, x1, y1, x2, y2, color=COL_ARROW):
    ax.annotate('', xy=(x2, y2), xytext=(x1, y1),
        arrowprops=dict(arrowstyle='->', color=color, lw=1.5))

def section_title(ax, x, y, text, color, size=13):
    ax.text(x, y, text, ha='center', va='center', fontsize=size,
        fontweight='bold', color=color, fontfamily='sans-serif')

# ==================== TITLE ====================
ax.text(8, 21.4, 'YOLOv8 Network Architecture', ha='center', va='center',
    fontsize=18, fontweight='bold', color=COL_TEXT)
ax.text(8, 21.0, 'Backbone — Neck — Head', ha='center', va='center',
    fontsize=11, color=COL_SUBTEXT, style='italic')

# ==================== INPUT ====================
ax.add_patch(FancyBboxPatch((6.0, 20.2), 4.0, 0.5,
    boxstyle="round,pad=0.05", facecolor='#FBBF24', edgecolor='white', linewidth=1.5))
ax.text(8, 20.45, 'Input: 640 × 640 × 3', ha='center', va='center',
    fontsize=10, fontweight='bold', color='#78350F')

draw_arrow(ax, 8, 20.2, 8, 19.8)

# ==================== BACKBONE ====================
ax.add_patch(FancyBboxPatch((0.3, 17.5), 15.4, 1.6,
    boxstyle="round,pad=0.1", facecolor=COL_BG, edgecolor=COL_BB, linewidth=2))
section_title(ax, 8, 19.2, 'BACKBONE  (CSPDarknet)', COL_BB, 12)

# Backbone modules row
draw_box(ax, 0.5,  17.7, 2.0, 0.8, 'Focus', '(slice) ↓2×', COL_BB, 8)
draw_box(ax, 2.8,  17.7, 2.0, 0.8, 'Conv', '3×3  s=2', COL_BB, 8)
draw_box(ax, 5.1,  17.7, 2.6, 0.8, 'C2f ×3', '64→128', COL_BB, 8)
draw_box(ax, 8.0,  17.7, 2.0, 0.8, 'Conv', '3×3  s=2', COL_BB, 8)
draw_box(ax, 10.3, 17.7, 2.6, 0.8, 'C2f ×6', '128→256', COL_BB, 8)
draw_box(ax, 13.2, 17.7, 2.0, 0.8, 'Conv', '3×3  s=2', COL_BB, 8)

# Feature map labels
ax.text(1.5,  17.55, '640²×3', ha='center', va='top', fontsize=7, color=COL_SUBTEXT)
ax.text(3.8,  17.55, '320²×64', ha='center', va='top', fontsize=7, color=COL_SUBTEXT)
ax.text(6.4,  17.55, '160²×128', ha='center', va='top', fontsize=7, color=COL_SUBTEXT)
ax.text(9.0,  17.55, '80²×256', ha='center', va='top', fontsize=7, color=COL_SUBTEXT)
ax.text(11.6, 17.55, '40²×512', ha='center', va='top', fontsize=7, color=COL_SUBTEXT)
ax.text(14.2, 17.55, '20²×1024', ha='center', va='top', fontsize=7, color=COL_SUBTEXT)

draw_arrow(ax, 8, 17.5, 8, 16.7)

# ==================== NECK ====================
ax.add_patch(FancyBboxPatch((0.3, 13.5), 15.4, 3.0,
    boxstyle="round,pad=0.1", facecolor=COL_BG, edgecolor=COL_NECK, linewidth=2))
section_title(ax, 8, 16.6, 'NECK  (PANet + FPN)', COL_NECK, 12)

# P5 branch
draw_box(ax, 0.5,  14.8, 1.5, 0.7, 'Conv', '', COL_NECK, 8)
draw_box(ax, 2.3,  15.5, 1.8, 0.7, 'Upsample', '×2', COL_NECK, 8)
draw_box(ax, 4.4,  15.5, 1.5, 0.7, 'Concat', '', COL_NECK, 8)
draw_box(ax, 6.2,  15.5, 1.8, 0.7, 'Upsample', '×2', COL_NECK, 8)
draw_box(ax, 8.3,  15.5, 1.5, 0.7, 'Concat', '', COL_NECK, 8)

# P5 label
ax.text(0.5+0.75, 14.8+0.7+0.12, 'P5', ha='center', va='bottom', fontsize=8,
    fontweight='bold', color=COL_NECK)

# N layers
draw_box(ax, 10.1, 15.5, 1.5, 0.7, 'C2f', 'N3', COL_NECK, 8)
draw_box(ax, 11.9, 15.5, 1.5, 0.7, 'Downsample', '', COL_NECK, 7)
draw_box(ax, 13.7, 15.5, 1.5, 0.7, 'C2f', 'N3', COL_NECK, 8)

# P4
draw_box(ax, 4.4,  14.0, 1.5, 0.7, 'Concat', '', COL_NECK, 8)
draw_box(ax, 6.2,  14.0, 1.8, 0.7, 'Upsample', '×2', COL_NECK, 8)
draw_box(ax, 8.3,  14.0, 1.5, 0.7, 'Concat', '', COL_NECK, 8)
draw_box(ax, 10.1, 14.0, 1.5, 0.7, 'C2f', 'N2', COL_NECK, 8)
draw_box(ax, 11.9, 14.0, 1.5, 0.7, 'Downsample', '', COL_NECK, 7)
draw_box(ax, 13.7, 14.0, 1.5, 0.7, 'C2f', 'N2', COL_NECK, 8)

# P3 bottom row
draw_box(ax, 8.3,  13.2, 1.5, 0.7, 'Concat', '', COL_NECK, 8)
draw_box(ax, 10.1, 13.2, 1.5, 0.7, 'C2f', 'N3', COL_NECK, 8)
draw_box(ax, 11.9, 13.2, 1.5, 0.7, 'Downsample', '', COL_NECK, 7)
draw_box(ax, 13.7, 13.2, 1.5, 0.7, 'N3 (80×)', '', COL_NECK, 7)

# P labels
ax.text(4.4+0.75,  14.0+0.7+0.12, 'P4', ha='center', va='bottom', fontsize=8,
    fontweight='bold', color=COL_NECK)
ax.text(8.3+0.75,  13.2+0.7+0.12, 'P3', ha='center', va='bottom', fontsize=8,
    fontweight='bold', color=COL_NECK)

draw_arrow(ax, 8, 13.5, 8, 12.7)

# ==================== HEAD ====================
ax.add_patch(FancyBboxPatch((0.3, 10.5), 15.4, 2.3,
    boxstyle="round,pad=0.1", facecolor=COL_BG, edgecolor=COL_HEAD, linewidth=2))
section_title(ax, 8, 12.9, 'HEAD  (Decoupled Detection)', COL_HEAD, 12)

# Three detection heads
draw_box(ax, 0.5,  10.7, 4.5, 1.6, 'P3 Head (大目标)', '80×80×256', COL_HEAD, 9)
ax.text(2.75, 11.0, 'cls', ha='center', va='center', fontsize=8, color='white')
ax.text(2.75, 10.75, 'reg', ha='center', va='center', fontsize=8, color='white')

draw_box(ax, 5.5,  10.7, 4.5, 1.6, 'P4 Head (中目标)', '40×40×256', COL_HEAD, 9)
ax.text(7.75, 11.0, 'cls', ha='center', va='center', fontsize=8, color='white')
ax.text(7.75, 10.75, 'reg', ha='center', va='center', fontsize=8, color='white')

draw_box(ax, 10.5, 10.7, 4.5, 1.6, 'P5 Head (小目标)', '20×20×256', COL_HEAD, 9)
ax.text(12.75, 11.0, 'cls', ha='center', va='center', fontsize=8, color='white')
ax.text(12.75, 10.75, 'reg', ha='center', va='center', fontsize=8, color='white')

draw_arrow(ax, 8, 10.5, 8, 9.8)

# ==================== OUTPUT ====================
ax.add_patch(FancyBboxPatch((3.0, 9.0), 10.0, 0.7,
    boxstyle="round,pad=0.05", facecolor='#7C3AED', edgecolor='white', linewidth=1.5))
ax.text(8, 9.35, 'Detection Output: K = 4 bbox + 1 obj + C classes',
    ha='center', va='center', fontsize=9, fontweight='bold', color='white')

# ==================== LEGEND ====================
# Backbone
ax.add_patch(patches.Rectangle((0.3, 8.0), 0.4, 0.3, facecolor=COL_BB, edgecolor='white'))
ax.text(0.85, 8.15, 'Backbone', ha='left', va='center', fontsize=9, color=COL_TEXT)
# Neck
ax.add_patch(patches.Rectangle((2.8, 8.0), 0.4, 0.3, facecolor=COL_NECK, edgecolor='white'))
ax.text(3.35, 8.15, 'Neck (PANet)', ha='left', va='center', fontsize=9, color=COL_TEXT)
# Head
ax.add_patch(patches.Rectangle((5.5, 8.0), 0.4, 0.3, facecolor=COL_HEAD, edgecolor='white'))
ax.text(6.05, 8.15, 'Head (Decoupled)', ha='left', va='center', fontsize=9, color=COL_TEXT)
# Input
ax.add_patch(patches.Rectangle((8.8, 8.0), 0.4, 0.3, facecolor='#FBBF24', edgecolor='white'))
ax.text(9.35, 8.15, 'Input/Output', ha='left', va='center', fontsize=9, color=COL_TEXT)

# ==================== CAPTION ====================
ax.text(8, 7.3, '图 3-2  YOLOv8 网络整体架构图',
    ha='center', va='center', fontsize=11, fontweight='bold', color=COL_TEXT)
ax.text(8, 7.0, 'YOLOv8 Network Architecture — Backbone / Neck / Head',
    ha='center', va='center', fontsize=9, color=COL_SUBTEXT, style='italic')

plt.tight_layout(pad=0.5)
plt.savefig('yolov8_architecture.jpg', dpi=200, bbox_inches='tight',
    facecolor='white', edgecolor='none')
plt.savefig('yolov8_architecture.png', dpi=200, bbox_inches='tight',
    facecolor='white', edgecolor='none')
print('Saved: yolov8_architecture.jpg')
print('Saved: yolov8_architecture.png')
