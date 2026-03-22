#!/usr/bin/env python3
"""绘制 YOLOv8 网络架构图"""

from PIL import Image, ImageDraw, ImageFont
import os

# 创建大尺寸画布
WIDTH = 2400
HEIGHT = 1800
BG_COLOR = (255, 255, 255)
BOX_COLOR = {
    'backbone': (100, 149, 237),   # 蓝色
    'neck': (60, 179, 113),       # 绿色
    'head': (220, 20, 60),        # 红色
    'module': (255, 165, 0),      # 橙色
}

def draw_box(draw, x, y, w, h, color, label, sublabel=""):
    """绘制带标签的方框"""
    # 阴影
    draw.rectangle([x+3, y+3, x+w+3, y+h+3], fill=(200, 200, 200))
    # 主框
    draw.rectangle([x, y, x+w, y+h], fill=color, outline=(0, 0, 0), width=2)
    
    # 标签
    font_size = 16
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", font_size)
        font_small = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14)
    except:
        font = ImageFont.load_default()
        font_small = ImageFont.load_default()
    
    # 主标签居中
    text_w = draw.textlength(label, font=font) if hasattr(draw, 'textlength') else len(label) * font_size * 0.6
    draw.text((x + (w - text_w) // 2, y + h//4), label, fill='white', font=font)
    
    if sublabel:
        text_w = draw.textlength(sublabel, font=font_small) if hasattr(draw, 'textlength') else len(sublabel) * font_size * 0.5
        draw.text((x + (w - text_w) // 2, y + h//2 + 10), sublabel, fill='white', font=font_small)

def draw_arrow(draw, x1, y1, x2, y2):
    """绘制箭头"""
    draw.line([x1, y1, x2, y2], fill=(0, 0, 0), width=2)
    # 箭头头部
    import math
    angle = math.atan2(y2-y1, x2-x1)
    arrow_len = 10
    draw.line([x2, y2, x2 - arrow_len*math.cos(angle - 0.5), y2 - arrow_len*math.sin(angle - 0.5)], fill=(0, 0, 0), width=2)
    draw.line([x2, y2, x2 - arrow_len*math.cos(angle + 0.5), y2 - arrow_len*math.sin(angle + 0.5)], fill=(0, 0, 0), width=2)

def create_yolov8_architecture():
    img = Image.new('RGB', (WIDTH, HEIGHT), color=BG_COLOR)
    draw = ImageDraw.Draw(img)
    
    # 标题
    try:
        font_title = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 28)
        font_section = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 20)
    except:
        font_title = ImageFont.load_default()
        font_section = font_title
    
    draw.text((WIDTH//2 - 200, 30), "YOLOv8 Network Architecture", fill=(0, 0, 0), font=font_title)
    
    # ========== Backbone 部分 ==========
    bx, by = 50, 100
    bw, bh = 700, 50
    
    draw.text((bx, by - 30), "Backbone (CSPDarknet)", fill=(100, 149, 237), font=font_section)
    
    modules = [
        ("Focus", "640→320"),
        ("Conv", "3×3, s2"),
        ("C2f×3", "通道: 64→128"),
        ("Conv", "3×3, s2"),
        ("C2f×6", "128→256"),
        ("Conv", "3×3, s2"),
        ("C2f×6", "256→512"),
        ("Conv", "3×3, s2"),
        ("C2f×3", "512→1024"),
        ("SPPF", "1024"),
    ]
    
    mx, my = bx, by
    mw = 65
    for i, (name, detail) in enumerate(modules):
        draw_box(draw, mx + i*(mw+5), my, mw, bh, BOX_COLOR['module'], name, detail)
        if i < len(modules) - 1:
            draw_arrow(draw, mx + i*(mw+5) + mw, my + bh//2, mx + (i+1)*(mw+5), my + bh//2)
    
    # ========== Neck 部分 ==========
    nx, ny = bx, by + 150
    nw, nh = 700, 50
    
    draw.text((nx, ny - 30), "Neck (PANet)", fill=(60, 179, 113), font=font_section)
    
    neck_modules = [
        ("Conv", "256→256"),
        ("Upsample", "2×"),
        ("Concat", "P4"),
        ("C2f", "512"),
        ("Conv", "256→256"),
        ("Upsample", "2×"),
        ("Concat", "P3"),
        ("C2f", "256"),
        ("Conv", "256→256"),
        ("Down", "P5→N4"),
    ]
    
    for i, (name, detail) in enumerate(neck_modules):
        draw_box(draw, nx + i*(mw+5), ny, mw, nh, BOX_COLOR['neck'], name, detail)
        if i < len(neck_modules) - 1:
            draw_arrow(draw, nx + i*(mw+5) + mw, ny + nh//2, nx + (i+1)*(mw+5), ny + nh//2)
    
    # ========== Head 部分 ==========
    hx, hy = bx, ny + 150
    
    draw.text((hx, hy - 30), "Head (Decoupled)", fill=(220, 20, 60), font=font_section)
    
    head_x = hx + 100
    
    # 3个检测头
    heads = [
        ("P3 (大目标)", "80×80", "4+1+3"),
        ("P4 (中目标)", "40×40", "4+1+3"),
        ("P5 (小目标)", "20×20", "4+1+3"),
    ]
    
    for i, (name, size, out) in enumerate(heads):
        box_x = head_x + i * 180
        draw_box(draw, box_x, hy, 160, bh, BOX_COLOR['head'], name, f"{size}, cls:{out}")
        # 连接到 Neck
        neck_x = nx + (8 + i*0.8) * (mw+5) + mw//2
        draw_arrow(draw, neck_x, ny + nh, box_x + 80, hy)
    
    # ========== 右侧：关键模块详细说明 ==========
    detail_x = 900
    detail_y = 100
    
    # C2f 模块
    draw.text((detail_x, detail_y), "C2f Module (CSP Bottleneck with 2 convolutions)", fill=(0, 0, 0), font=font_section)
    
    # 绘制 C2f 详细结构
    c2f_x = detail_x
    c2f_y = detail_y + 40
    
    # 输入框
    draw_box(draw, c2f_x, c2f_y, 100, 40, BOX_COLOR['module'], "Input", "x")
    
    # Conv1
    draw_box(draw, c2f_x + 120, c2f_y, 80, 40, BOX_COLOR['module'], "Conv1", "1×1")
    draw_arrow(draw, c2f_x + 100, c2f_y + 20, c2f_x + 120, c2f_y + 20)
    
    # Split
    draw.text((c2f_x + 210, c2f_y + 10), "split", fill=(0, 0, 0))
    
    # Bottleneck 1
    draw_box(draw, c2f_x + 250, c2f_y - 30, 100, 40, BOX_COLOR['module'], "Bottleneck", "1×1")
    draw_box(draw, c2f_x + 250, c2f_y + 40, 100, 40, BOX_COLOR['module'], "Bottleneck", "1×1")
    
    # Bottleneck 2
    draw_box(draw, c2f_x + 370, c2f_y - 30, 100, 40, BOX_COLOR['module'], "Bottleneck", "1×1")
    draw_box(draw, c2f_x + 370, c2f_y + 40, 100, 40, BOX_COLOR['module'], "Bottleneck", "1×1")
    
    # Conv2
    draw_box(draw, c2f_x + 490, c2f_y, 80, 40, BOX_COLOR['module'], "Conv2", "1×1")
    
    # Concat & Output
    draw_box(draw, c2f_x + 590, c2f_y, 100, 40, BOX_COLOR['module'], "Concat", "Cat all")
    draw_box(draw, c2f_x + 710, c2f_y, 100, 40, BOX_COLOR['module'], "Output", "C2f(x)")
    
    # 连接线
    draw_arrow(draw, c2f_x + 200, c2f_y + 20, c2f_x + 250, c2f_y + 20)  # to first bottleneck
    draw_arrow(draw, c2f_x + 350, c2f_y + 20, c2f_x + 370, c2f_y + 20)  # between bottlenecks
    draw_arrow(draw, c2f_x + 470, c2f_y + 20, c2f_x + 490, c2f_y + 20)  # to Conv2
    draw_arrow(draw, c2f_x + 570, c2f_y + 20, c2f_x + 590, c2f_y + 20)  # to Concat
    draw_arrow(draw, c2f_x + 690, c2f_y + 20, c2f_x + 710, c2f_y + 20)  # to Output
    
    # ========== 底部：数学公式区域 ==========
    formula_y = 480
    
    # 公式1: CSP
    draw.text((detail_x, formula_y), "CSP Module Formula:", fill=(0, 0, 0), font=font_section)
    draw.text((detail_x, formula_y + 35), "y = Conv1(x) + Concat([Bottleneck(x), Conv2(x)])", fill=(50, 50, 50))
    
    # 公式2: SPPF
    draw.text((detail_x, formula_y + 70), "SPPF Module (MaxPool序列):", fill=(0, 0, 0), font=font_section)
    draw.text((detail_x, formula_y + 105), "y = Concat([MaxPool(x,5), MaxPool(x1,5), MaxPool(x2,5), x])", fill=(50, 50, 50))
    
    # ========== 右侧底部：Decoupled Head ==========
    head_detail_x = detail_x
    head_detail_y = formula_y + 160
    
    draw.text((head_detail_x, head_detail_y), "Decoupled Head (解耦检测头):", fill=(0, 0, 0), font=font_section)
    
    # 绘制解耦头结构
    dh_x = head_detail_x + 50
    dh_y = head_detail_y + 40
    
    # 输入
    draw_box(draw, dh_x, dh_y, 100, 35, BOX_COLOR['head'], "Feature", "HxW")
    
    # 分类分支
    draw_box(draw, dh_x + 120, dh_y - 40, 100, 35, BOX_COLOR['head'], "Conv+SiLU", "cls")
    draw_box(draw, dh_x + 240, dh_y - 40, 100, 35, BOX_COLOR['head'], "Conv", "1×1")
    draw_box(draw, dh_x + 360, dh_y - 40, 100, 35, BOX_COLOR['head'], "Sigmoid", "cls")
    
    # 回归分支
    draw_box(draw, dh_x + 120, dh_y + 10, 100, 35, BOX_COLOR['head'], "Conv+SiLU", "reg")
    draw_box(draw, dh_x + 240, dh_y + 10, 100, 35, BOX_COLOR['head'], "Conv", "1×1")
    draw_box(draw, dh_x + 360, dh_y + 10, 100, 35, BOX_COLOR['head'], "reg_out", "4 coords")
    
    # 连接线
    draw_arrow(draw, dh_x + 100, dh_y + 17, dh_x + 120, dh_y + 17)
    draw_arrow(draw, dh_x + 170, dh_y + 17, dh_x + 240, dh_y - 22)
    draw_arrow(draw, dh_x + 170, dh_y + 27, dh_x + 240, dh_y + 27)
    draw_arrow(draw, dh_x + 340, dh_y - 22, dh_x + 360, dh_y - 22)
    draw_arrow(draw, dh_x + 340, dh_y + 27, dh_x + 360, dh_y + 27)
    
    # ========== 图例 ==========
    legend_x = WIDTH - 400
    legend_y = 100
    
    draw.text((legend_x, legend_y), "Legend:", fill=(0, 0, 0), font=font_section)
    
    legend_items = [
        (BOX_COLOR['backbone'], "Backbone"),
        (BOX_COLOR['neck'], "Neck"),
        (BOX_COLOR['head'], "Head"),
        (BOX_COLOR['module'], "Module"),
    ]
    
    for i, (color, name) in enumerate(legend_items):
        ly = legend_y + 30 + i * 25
        draw.rectangle([legend_x, ly, legend_x + 20, ly + 15], fill=color, outline=(0, 0, 0))
        draw.text((legend_x + 30, ly), name, fill=(0, 0, 0))
    
    # ========== 保存 ==========
    output_path = "/home/hwj/jetson/bishe/yolov8_architecture.png"
    img.save(output_path, "PNG", quality=95)
    print(f"Architecture diagram saved to: {output_path}")
    return output_path

if __name__ == "__main__":
    create_yolov8_architecture()
