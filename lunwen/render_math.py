#!/usr/bin/env python3
"""Render LaTeX math formulas from markdown to PNG images."""

import os
import re
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as patches
import numpy as np

# Configure matplotlib for nice math rendering
plt.rcParams['font.family'] = 'DejaVu Sans'
plt.rcParams['font.size'] = 11
plt.rcParams['text.usetex'] = False  # Use mathtext instead
plt.rcParams['text.latex.preamble'] = r'\usepackage{amsmath}\usepackage{amssymb}'

# Try to use a good font that supports math
try:
    import matplotlib.font_manager as fm
    # Use a font that has good Unicode math support
    for font in fm.fontManager.ttflist:
        if 'Noto' in font.name and ('Sans' in font.name or 'Mono' in font.name):
            plt.rcParams['font.family'] = font.name
            break
except:
    pass

OUTPUT_DIR = "math_images"
os.makedirs(OUTPUT_DIR, exist_ok=True)

def latex_to_image(latex_str, filename, font_size=12, dpi=150, bg='white', inline=False):
    """Render a LaTeX string to a PNG image and return the path."""
    fig, ax = plt.subplots(figsize=(8, 1.2))
    fig.patch.set_facecolor(bg)
    ax.patch.set_facecolor(bg)
    ax.axis('off')

    # Clean up the LaTeX string - remove display math markers
    latex = latex_str.strip()
    if latex.startswith('$$') and latex.endswith('$$'):
        latex = latex[2:-2].strip()
    elif latex.startswith('$') and latex.endswith('$') and not (latex.startswith('$$')):
        # Check it's a true inline/delimited math, not a single $
        pass
    elif latex.startswith('\\[') and latex.endswith('\\]'):
        latex = latex[2:-2].strip()
    elif latex.startswith('\\(') and latex.endswith('\\)'):
        latex = latex[2:-2].strip()

    # Make it display math
    display_latex = f"${latex}$" if not (latex.startswith('$')) else latex

    try:
        ax.text(0.5, 0.5, display_latex,
                fontsize=font_size,
                ha='center', va='center',
                transform=ax.transAxes,
                color='black',
                usetex=False)
    except Exception as e:
        # Fallback: show the raw latex
        ax.text(0.5, 0.5, latex[:100],
                fontsize=font_size,
                ha='center', va='center',
                transform=ax.transAxes,
                color='red')

    plt.tight_layout(pad=0.1)
    filepath = os.path.join(OUTPUT_DIR, filename)
    fig.savefig(filepath, dpi=dpi, bbox_inches='tight', facecolor=bg, edgecolor='none')
    plt.close(fig)
    return filepath

def latex_multiline_to_image(latex_str, filename, font_size=12, dpi=150, bg='white'):
    """Render multiline LaTeX (align, etc.) to a PNG image."""
    fig, ax = plt.subplots(figsize=(10, 1.5))
    fig.patch.set_facecolor(bg)
    ax.patch.set_facecolor(bg)
    ax.axis('off')

    # Process the multiline latex
    latex = latex_str.strip()
    if latex.startswith('$$'):
        latex = latex[2:-2].strip()
    elif latex.startswith('\\[') and latex.endswith('\\]'):
        latex = latex[2:-2].strip()

    try:
        ax.text(0.5, 0.5, latex,
                fontsize=font_size,
                ha='center', va='center',
                transform=ax.transAxes,
                color='black',
                usetex=False,
                linespacing=1.5)
    except Exception as e:
        ax.text(0.5, 0.5, str(latex)[:200],
                fontsize=font_size,
                ha='center', va='center',
                transform=ax.transAxes,
                color='red')

    plt.tight_layout(pad=0.2)
    filepath = os.path.join(OUTPUT_DIR, filename)
    fig.savefig(filepath, dpi=dpi, bbox_inches='tight', facecolor=bg, edgecolor='none')
    plt.close(fig)
    return filepath

# Parse the markdown and extract math formulas
md_content = open('section_3_2.md', encoding='utf-8').read()

# Extract display math ($$...$$)
display_math_pattern = re.compile(r'\$\$(.+?)\$\$', re.DOTALL)
# Extract inline math ($...$) but not $$...$$
inline_math_pattern = re.compile(r'(?<!\$)\$(?!\$)(.+?)(?<!\$)\$(?!\$)')

counter = 0
math_images = {}

# Find display math
for match in display_math_pattern.finditer(md_content):
    latex = match.group(1).strip()
    filename = f"math_disp_{counter:03d}.png"
    print(f"Rendering display math [{counter}]: {latex[:60]}...")
    path = latex_multiline_to_image(latex, filename, dpi=200)
    math_images[match.group(0)] = path
    counter += 1

print(f"Extracted {counter} display math formulas")
print(f"Math images saved to: {OUTPUT_DIR}/")
