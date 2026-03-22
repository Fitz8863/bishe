const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
  AlignmentType, HeadingLevel, BorderStyle, WidthType,
  ShadingType, PageNumber, PageBreak, ImageRun,
  LevelFormat, UnderlineType
} = require('docx');
const fs = require('fs');
const path = require('path');

const CONTENT_WIDTH = 9360;
const MARGIN = 1080;
const PAGE_WIDTH = 12240;
const PAGE_HEIGHT = 15840;

const BORDER = { style: BorderStyle.SINGLE, size: 1, color: "AAAAAA" };
const BORDERS = { top: BORDER, bottom: BORDER, left: BORDER, right: BORDER };
const CELL_MARGINS = { top: 80, bottom: 80, left: 120, right: 120 };

function headerCell(text, width = 3120) {
  return new TableCell({
    borders: BORDERS,
    width: { size: width, type: WidthType.DXA },
    shading: { fill: "E8E8E8", type: ShadingType.CLEAR },
    margins: CELL_MARGINS,
    children: [new Paragraph({
      children: [new TextRun({ text, bold: true, font: "Arial", size: 20 })]
    })]
  });
}

function dataCell(text, width = 3120) {
  return new TableCell({
    borders: BORDERS,
    width: { size: width, type: WidthType.DXA },
    margins: CELL_MARGINS,
    children: [new Paragraph({
      children: [new TextRun({ text: text || "", font: "Arial", size: 20 })]
    })]
  });
}

function para(runs, opts = {}) {
  return new Paragraph({
    children: runs,
    spacing: { before: opts.before || 0, after: opts.after || 120 },
    alignment: opts.align || AlignmentType.JUSTIFIED,
    indent: opts.indent || undefined,
    ...opts
  });
}

function textRun(text, opts = {}) {
  return new TextRun({
    text,
    font: "Arial",
    size: opts.size || 21,
    bold: opts.bold || false,
    italics: opts.italic || false,
    underline: opts.underline ? { type: UnderlineType.SINGLE } : undefined,
    ...opts
  });
}

function codePara(text) {
  return new Paragraph({
    children: [new TextRun({
      text,
      font: "Courier New",
      size: 16,
      color: "333333"
    })],
    spacing: { before: 60, after: 60 },
    indent: { left: 360 }
  });
}

function mathImage(filename) {
  const imgPath = path.join(__dirname, 'math_images', filename);
  if (!fs.existsSync(imgPath)) return null;
  const data = fs.readFileSync(imgPath);
  const stats = fs.statSync(imgPath);
  return new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: 60, after: 60 },
    children: [new ImageRun({
      type: "png",
      data,
      transformation: { width: 650, height: 100 },
      altText: { title: "Math formula", description: "Math formula", name: "Math" }
    })]
  });
}

function spacer(before = 0, after = 120) {
  return new Paragraph({ children: [new TextRun("")], spacing: { before, after } });
}

function h1(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_1,
    children: [new TextRun({ text, bold: true, font: "Arial", size: 32 })],
    spacing: { before: 300, after: 180 }
  });
}

function h2(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_2,
    children: [new TextRun({ text, bold: true, font: "Arial", size: 28 })],
    spacing: { before: 240, after: 120 }
  });
}

function h3(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_3,
    children: [new TextRun({ text, bold: true, font: "Arial", size: 24 })],
    spacing: { before: 180, after: 60 }
  });
}

function bodyPara(text) {
  return new Paragraph({
    children: [new TextRun({ text, font: "Arial", size: 21 })],
    spacing: { before: 0, after: 120 },
    alignment: AlignmentType.JUSTIFIED
  });
}

function bodyParaRuns(runs) {
  return new Paragraph({
    children: runs.map(r => typeof r === 'string' ? textRun(r) : r),
    spacing: { before: 0, after: 120 },
    alignment: AlignmentType.JUSTIFIED
  });
}

function bold(text) { return textRun(text, { bold: true }); }
function italic(text) { return textRun(text, { italic: true }); }

// Build the document
const children = [];

// ===== 3.2.1 整体架构概述 =====
children.push(h1("3.2 YOLOv8 网络整体架构"));
children.push(h2("3.2.1 整体架构概述"));
children.push(bodyPara("YOLOv8 是 Ultralytics 公司于 2023 年发布的最新一代 YOLO 系列目标检测算法，其网络架构延续了\u201C骨干网络-颈部网络-检测头\u201D（Backbone-Neck-Head）的经典设计范式，但在各模块的实现上进行了全面优化。图 3-2 展示了 YOLOv8 的整体网络架构。"));

// ASCII architecture diagram - preserve as monospace
const diagramLines = [
  "┌─────────────────────────────────────────────────────────────────────────┐",
  "│                           YOLOv8 Network Architecture                    │",
  "├─────────────────────────────────────────────────────────────────────────┤",
  "│                                                                          │",
  "│  Input Image: 640 × 640 × 3                                             │",
  "│                     │                                                    │",
  "│                     ▼                                                    │",
  "│  ┌──────────────────────────────────────────────────────────────────┐   │",
  "│  │                        BACKBONE (CSPDarknet)                     │   │",
  "│  │  ┌────────┐ ┌─────┐ ┌─────────┐ ┌─────┐ ┌─────────┐ ┌─────┐ ┌───┐│   │",
  "│  │  │ Focus  │ │Conv │ │   C2f   │ │Conv │ │   C2f   │ │Conv │ │   ││   │",
  "│  │  │(slice) │ │3×3  │ │  ×3     │ │3×3  │ │  ×6     │ │3×3  │ │   ││   │",
  "│  │  │↓2×     │ │s=2  │ │(64→128) │ │s=2  │ │(128→256)│ │s=2  │ │   ││   │",
  "│  │  └────────┘ └──┬──┘ └────┬────┘ └──┬──┘ └────┬────┘ └──┬──┘ │   ││   │",
  "│  │       │        │         │         │         │         │    │   ││   │",
  "│  │  640×320×3  320×160  160×80×128  80×40×256  40×20×512 20×10  │   ││   │",
  "│  └──────────────────────────────────────────────────────────────────┘   │",
  "│                                    │                                     │",
  "│                         P5 ─────────┤                                    │",
  "│                                    │                                     │",
  "│  ┌──────────────────────────────────────────────────────────────────┐   │",
  "│  │                          NECK (PANet)                             │   │",
  "│  │                                                                        │   │",
  "│  │   ┌─────┐     ┌───────┐     ┌───────┐     ┌─────┐     ┌─────────┐  │   │",
  "│  │   │Conv │←P5──│Upsample│←P5──│Concat │←P4──│Upsample│←P3──────┘  │   │",
  "│  │   │     │     │  2×    │     │(P4+)  │     │  2×   │              │   │",
  "│  │   │     │     │       │     │  C2f   │     │       │              │   │",
  "│  │   └──┬──┘     └───┬───┘     └───┬───┘     └───┬───┘              │   │",
  "│  │      │           │             │             │                    │   │",
  "│  │   N3 │        N3 │           N2 │           N2 │                  │   │",
  "│  │  (20×)         (20×)         (40×)         (80×)                │   │",
  "│  │                                                                   │   │",
  "│  │      ↓ P5        ↓ P5         ↓ P4        ↓ P3                   │   │",
  "│  └──────────────────────────────────────────────────────────────────┘   │",
  "│                                    │                                     │",
  "│  ┌──────────────────────────────────────────────────────────────────┐   │",
  "│  │                    HEAD (Decoupled Detection)                      │   │",
  "│  │                                                                        │   │",
  "│  │   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │   │",
  "│  │   │   P3 Head   │  │   P4 Head   │  │   P5 Head   │              │   │",
  "│  │   │  80×80×64   │  │  40×40×64   │  │  20×20×64   │              │   │",
  "│  │   │ (大目标)    │  │  (中目标)    │  │  (小目标)    │              │   │",
  "│  │   └──────┬──────┘  └──────┬──────┘  └──────┬──────┘              │   │",
  "│  │          │                │                │                      │   │",
  "│  │          ▼                ▼                ▼                      │   │",
  "│  │   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │   │",
  "│  │   │ cls + reg   │  │ cls + reg   │  │ cls + reg   │              │   │",
  "│  │   │ 80×80×(K×C) │  │ 40×40×(K×C) │  │ 20×20×(K×C) │              │   │",
  "│  │   └─────────────┘  └─────────────┘  └─────────────┘              │   │",
  "│  └──────────────────────────────────────────────────────────────────┘   │",
  "│                                    │                                     │",
  "│                                    ▼                                     │",
  "│                           Detection Output                               │",
  "│                    (K = 4 bbox + 1 obj + C classes)                   │",
  "└─────────────────────────────────────────────────────────────────────────┘"
];
diagramLines.forEach(line => children.push(codePara(line)));
children.push(spacer(0, 60));
children.push(bodyPara("图 3-2 YOLOv8 网络整体架构图"));
children.push(spacer(120, 120));

// ===== 3.2.2 骨干网络 =====
children.push(h2("3.2.2 骨干网络（Backbone）"));
children.push(bodyPara("YOLOv8 的骨干网络采用 CSPDarknet 作为特征提取器，由一系列卷积模块和 C2f 模块交替组成，负责从输入图像中逐层提取多尺度特征。"));

children.push(h3("1. Focus 模块（空间下采样）"));
children.push(bodyPara("Focus 模块是 YOLOv8 对输入图像进行空间降采样的关键组件，其核心思想是通过像素切片（Pixel Space Sampling）操作将特征图的空间维度减半，同时通道数扩展至原来的 4 倍。"));
children.push(bodyPara("数学定义："));
children.push(bodyPara("对于输入特征图 $X \\in \\mathbb{R}^{H \\times W \\times C}$，Focus 模块将其空间下采样 $2\\times$，输出 $Y \\in \\mathbb{R}^{\\frac{H}{2} \\times \\frac{W}{2} \\times 4C}$："));
children.push(mathImage("math_disp_000.png"));
children.push(bodyPara("其中："));
children.push(bodyParaRuns([textRun("row(k) = ⌊k / 4⌋ mod H  "), textRun("(行索引计算)")]));
children.push(bodyParaRuns([textRun("col(k) = k mod W"), textRun("(列索引计算)")]));
children.push(spacer(60, 60));
children.push(bodyPara("Focus 模块不包含可学习参数，仅进行通道重组操作，计算量为零。"));

children.push(h3("2. C2f 模块（CSP Bottleneck with 2 Convolutions）"));
children.push(bodyPara("C2f 模块是 YOLOv8 的核心构建块，源自 YOLOv5 的 C3 模块并进行了改进。其设计融合了跨阶段局部网络（CSPNet）的思想，通过特征分离和密集连接增强梯度流动。"));
children.push(bodyPara("数学定义："));
children.push(bodyPara("给定输入特征 $x \\in \\mathbb{R}^{H \\times W \\times C_{in}}$，C2f 模块的计算过程如下："));
children.push(mathImage("math_disp_001.png"));
children.push(bodyPara("其中："));
children.push(bodyParaRuns([bold("Conv₁："), textRun("1×1 卷积，用于通道分离")]));
children.push(bodyParaRuns([bold("Split："), textRun("通道分割操作，将特征分为两部分")]));
children.push(bodyParaRuns([bold("Bottleneck："), textRun("瓶颈层，由 1×1 卷积 + 3×3 卷积 + 残差连接组成")]));
children.push(bodyParaRuns([bold("Concat："), textRun("通道维度拼接")]));
children.push(bodyParaRuns([bold("Conv₂："), textRun("1×1 卷积，用于通道融合")]));
children.push(spacer(60, 60));
children.push(bodyPara("C2f 模块结构图："));

const c2fLines = [
  "┌────────────────────────────────────────────────────────────────┐",
  "│                         C2f Module                              │",
  "├────────────────────────────────────────────────────────────────┤",
  "│                                                                    │",
  "│         x ──→ [Conv1×1] ──→ ┬──→ [Bottleneck₁] ──→ ┐           │",
  "│                              │                        │           │",
  "│                              │──→ [Bottleneck₂] ──→ Concat ──→    │",
  "│                              │                        │    [Conv1×1] ──→ y",
  "│                              │──→ [Bottleneck₃] ──→ ┘           │",
  "│                              │                                   │",
  "│                              └─── skip ───────────────────────→ ┘",
  "│                                                                    │",
  "└────────────────────────────────────────────────────────────────┘"
];
c2fLines.forEach(line => children.push(codePara(line)));
children.push(spacer(120, 120));

children.push(h3("3. SPPF 模块（Spatial Pyramid Pooling - Fast）"));
children.push(bodyPara("SPPF 模块通过多尺度最大池化操作实现空间金字塔池化，有效融合不同感受野的特征，增强模型对目标尺度变化的鲁棒性。"));
children.push(bodyPara("数学定义："));
children.push(bodyPara("对于输入特征 x，SPPF 模块的计算过程为："));
children.push(mathImage("math_disp_002.png"));
children.push(bodyParaRuns([textRun("其中 "), bold("x₁ = MaxPool(x, k=5)"), textRun("，"), bold("x₂ = MaxPool(x₁, k=5)")]));
children.push(spacer(60, 60));
children.push(bodyPara("SPPF 与 SPP 的区别："));

// SPPF vs SPP table
children.push(new Table({
  width: { size: CONTENT_WIDTH, type: WidthType.DXA },
  columnWidths: [2340, 2340, 2340, 2340],
  rows: [
    new TableRow({ children: [
      headerCell("特性", 2340),
      headerCell("SPP", 2340),
      headerCell("SPPF", 4680),
    ]}),
    new TableRow({ children: [
      dataCell("池化方式", 2340),
      dataCell("并行 5×5, 9×9, 13×13", 2340),
      dataCell("串行 5×5 MaxPool × 3", 4680),
    ]}),
    new TableRow({ children: [
      dataCell("计算效率", 2340),
      dataCell("较低", 2340),
      dataCell("更高（减少约 50% 计算量）", 4680),
    ]}),
    new TableRow({ children: [
      dataCell("感受野", 2340),
      dataCell("5×5, 9×9, 13×13", 2340),
      dataCell("5×5, 9×9, 13×13（等效）", 4680),
    ]}),
  ]
}));
children.push(spacer(120, 120));

// ===== 3.2.3 颈部网络 =====
children.push(h2("3.2.3 颈部网络（Neck）"));
children.push(bodyPara("YOLOv8 的颈部网络采用路径聚合网络（PANet）结合特征金字塔网络（FPN）的双向特征融合架构，实现多尺度特征的充分交互。"));
children.push(h3("PANet 双向特征融合"));
children.push(bodyPara("PANet 在 FPN 自顶向下融合的基础上，额外增加了自底向上的特征增强路径，形成双向特征流动。"));
children.push(bodyPara("数学定义："));
children.push(bodyPara("设 Pᵢ 表示 FPN 第 i 层的特征，Nᵢ 表示 PAN 第 i 层的增强特征："));
children.push(bodyPara("自顶向下融合（FPN）："));
children.push(mathImage("math_disp_003.png"));
children.push(bodyParaRuns([textRun("其中 Cᵢ 是骨干网络第 i 层的输出，"), bold("⊕"), textRun(" 表示通道拼接。")]));
children.push(bodyPara("自底向上增强（PAN）："));
children.push(mathImage("math_disp_004.png"));
children.push(spacer(60, 60));
children.push(bodyPara("特征融合示意："));

const panLines = [
  "                     ┌─────────────────────────────────────────┐",
  "                     │              FPN (Top-down)               │",
  "                     │                                          │",
  "          C5 ──→ [Conv] ─────────────────────────────┐         │",
  "                     │                               │         │",
  "          C4 ──→ [Conv] ──→ [Upsample 2×] ──→ [Concat] ──→ P4│",
  "                                                   ↓           │",
  "          C3 ──→ [Conv] ──→ [Upsample 2×] ──→ [Concat] ──→ P3│",
  "                                                   ↓           │",
  "                     └─────────────────────────────────────────┘",
  "                                                    │",
  "                     ┌─────────────────────────────────────────┐",
  "                     │              PAN (Bottom-up)             │",
  "                     │                                          │",
  "          P3 ──→ [Concat] ──→ [C2f] ──→ [Downsample] ──→ N3 ──┤",
  "                                                   ↓           │",
  "          P4 ──→ [Concat] ──→ [C2f] ──→ [Downsample] ──→ N4 ──┤",
  "                                                   ↓           │",
  "          P5 ──→ [Concat] ──→ [C2f] ───────────────────────→ N5│",
  "                     └─────────────────────────────────────────┘"
];
panLines.forEach(line => children.push(codePara(line)));
children.push(spacer(120, 120));

// ===== 3.2.4 检测头 =====
children.push(h2("3.2.4 检测头（Head）"));
children.push(bodyPara("YOLOv8 采用无锚框解耦检测头（Anchor-free Decoupled Head），将分类和回归任务解耦为两个独立的分支。"));
children.push(h3("1. 解耦头设计"));
children.push(bodyPara("传统 YOLO 系列使用耦合检测头，共享特征同时预测分类和边界框。YOLOv8 将两个任务完全解耦，使用独立的分支进行预测。"));
children.push(bodyPara("数学定义："));
children.push(bodyPara("对于输入特征 F ∈ ℝ^(H×W×C)："));
children.push(bodyParaRuns([bold("分类分支：")]));
children.push(mathImage("math_disp_005.png"));
children.push(bodyParaRuns([bold("回归分支：")]));
children.push(mathImage("math_disp_006.png"));
children.push(spacer(60, 60));
children.push(bodyPara("解耦头结构："));

const decHeadLines = [
  "┌─────────────────────────────────────────────────────┐",
  "│                  Decoupled Head                     │",
  "├─────────────────────────────────────────────────────┤",
  "│                                                      │",
  "│   Feature ──→ [Conv 3×3 + SiLU] ──→ [Conv 1×1]      │",
  "│      Map                                 ↓           │",
  "│                                     ┌────┴────┐      │",
  "│                                     │ Sigmoid │ → cls│",
  "│                                     └─────────┘      │",
  "│                                                      │",
  "│   Feature ──→ [Conv 3×3 + SiLU] ──→ [Conv 1×1] ──→ reg",
  "│      Map                      (无激活函数)           │",
  "│                                                      │",
  "└─────────────────────────────────────────────────────┘"
];
decHeadLines.forEach(line => children.push(codePara(line)));
children.push(spacer(120, 120));

children.push(h3("2. 多尺度检测"));
children.push(bodyPara("YOLOv8 在三个不同尺度的特征图上进行检测，分别负责大、中、小目标的检测："));

children.push(new Table({
  width: { size: CONTENT_WIDTH, type: WidthType.DXA },
  columnWidths: [2340, 2340, 2340, 2340],
  rows: [
    new TableRow({ children: [
      headerCell("检测层", 2340),
      headerCell("特征图尺寸", 2340),
      headerCell("感受野", 2340),
      headerCell("适用目标", 2340),
    ]}),
    new TableRow({ children: [
      dataCell("P3 (大目标)", 2340),
      dataCell("80×80", 2340),
      dataCell("小", 2340),
      dataCell("香烟、打火机（占图像 > 5%）", 2340),
    ]}),
    new TableRow({ children: [
      dataCell("P4 (中目标)", 2340),
      dataCell("40×40", 2340),
      dataCell("中", 2340),
      dataCell("未穿防护服人员（占图像 2-5%）", 2340),
    ]}),
    new TableRow({ children: [
      dataCell("P5 (小目标)", 2340),
      dataCell("20×20", 2340),
      dataCell("大", 2340),
      dataCell("安全帽、口罩（占图像 < 2%）", 2340),
    ]}),
  ]
}));
children.push(spacer(120, 120));

children.push(h3("3. 无锚框机制"));
children.push(bodyPara("YOLOv8 舍弃了基于锚框（Anchor-based）的检测范式，采用无锚框（Anchor-free）策略，直接预测目标边界框的四个边界相对于特征图网格点的偏移量。"));
children.push(bodyPara("预测输出："));
children.push(bodyPara("对于特征图上每个位置 (i, j)，预测输出为："));
children.push(mathImage("math_disp_007.png"));
children.push(bodyPara("其中："));
children.push(bodyParaRuns([bold("dₓ, dᵧ"), textRun("：边界框中心相对于网格左上角的偏移量（归一化到 [0, 1]）")]));
children.push(bodyParaRuns([bold("d_w, d_h"), textRun("：边界框宽高的自然对数：d_w = ln(w), d_h = ln(h)")]));
children.push(bodyParaRuns([bold("p_obj"), textRun("：目标置信度（Objectness）")]));
children.push(bodyParaRuns([bold("p₁, p₂, ..., p_C"), textRun("：各类别的分类概率")]));
children.push(spacer(60, 60));
children.push(bodyPara("恢复到原始尺度："));
children.push(mathImage("math_disp_008.png"));
children.push(bodyParaRuns([textRun("其中 i 为特征金字塔层级，"), bold("σ"), textRun(" 为 Sigmoid 函数。")]));
children.push(spacer(120, 120));

// ===== 3.2.5 损失函数 =====
children.push(h2("3.2.5 损失函数"));
children.push(bodyPara("YOLOv8 的总损失由三部分组成：分类损失、回归损失和置信度损失。"));
children.push(h3("1. 分类损失（BCE Loss）"));
children.push(bodyPara("对于分类分支，使用二元交叉熵损失（Binary Cross Entropy Loss）："));
children.push(mathImage("math_disp_009.png"));
children.push(bodyParaRuns([textRun("其中 yᵢ 为真实标签，"), bold("ŷᵢ"), textRun(" 为预测概率。")]));
children.push(spacer(60, 60));
children.push(h3("2. 回归损失（DFL + CIoU Loss）"));
children.push(bodyPara("YOLOv8 采用分布焦点损失（Distribution Focal Loss, DFL）结合 CIoU Loss 进行边界框回归。"));
children.push(bodyParaRuns([bold("CIoU Loss：")]));
children.push(mathImage("math_disp_010.png"));
children.push(bodyPara("其中："));
children.push(bodyParaRuns([bold("IoU"), textRun("：预测框与真实框的交并比")]));
children.push(bodyParaRuns([bold("ρ²(b, b^gt)"), textRun("：两个框中心点的欧氏距离")]));
children.push(bodyParaRuns([bold("c"), textRun("：覆盖两个框的最小闭包区域的对角线长度")]));
children.push(bodyParaRuns([bold("α"), textRun("：权重因子")]));
children.push(bodyParaRuns([bold("v"), textRun("：长宽比一致性度量：")]));
children.push(bodyPara("  v = (4/π²) × (arctan(w^gt/h^gt) - arctan(w/h))²"));
children.push(spacer(60, 60));
children.push(bodyParaRuns([bold("DFL Loss：")]));
children.push(bodyPara("DFL 将边界框回归问题转化为分类问题，将连续坐标值离散化为多个类别的概率分布："));
children.push(mathImage("math_disp_011.png"));
children.push(spacer(60, 60));
children.push(h3("3. 总损失"));
children.push(mathImage("math_disp_012.png"));
children.push(bodyParaRuns([textRun("其中 λ₁, λ₂, λ₃ 为各损失项的权重系数（YOLOv8 默认为 "), bold("λ₁=7.5, λ₂=1.5, λ₃=1.0"), textRun("）。")]));
children.push(spacer(120, 120));

// ===== 3.2.6 本系统的 YOLOv8 适配 =====
children.push(h2("3.2.6 本系统的 YOLOv8 适配"));
children.push(bodyPara("针对化工厂安全行为监控的实际需求，本系统对标准 YOLOv8 进行了针对性适配："));
children.push(h3("网络配置"));

children.push(new Table({
  width: { size: CONTENT_WIDTH, type: WidthType.DXA },
  columnWidths: [2340, 4680, 2340],
  rows: [
    new TableRow({ children: [
      headerCell("参数", 2340),
      headerCell("配置值", 4680),
      headerCell("说明", 2340),
    ]}),
    new TableRow({ children: [
      dataCell("输入尺寸", 2340),
      dataCell("640×640", 4680),
      dataCell("平衡精度与推理速度", 2340),
    ]}),
    new TableRow({ children: [
      dataCell("类别数", 2340),
      dataCell("5（吸烟、动火、无防护装备1、无防护装备2、无防护装备3）", 4680),
      dataCell("根据实际违规行为定义", 2340),
    ]}),
    new TableRow({ children: [
      dataCell("骨干网络", 2340),
      dataCell("YOLOv8s", 4680),
      dataCell("在 Jetson Orin Nano 上的性能平衡", 2340),
    ]}),
    new TableRow({ children: [
      dataCell("检测尺度", 2340),
      dataCell("80×80, 40×40, 20×20", 4680),
      dataCell("多尺度覆盖不同大小目标", 2340),
    ]}),
  ]
}));
children.push(spacer(120, 120));

children.push(h3("轻量化策略"));
children.push(bodyPara("为适应边缘设备的算力限制，本系统采用以下轻量化策略（详见第四章）："));

const strategies = [
  { num: "1", text: "通道剪枝：移除不重要的卷积通道，减少 30% 计算量" },
  { num: "2", text: "知识蒸馏：用大型模型指导小型模型训练，保持精度" },
  { num: "3", text: "INT8 量化：将 FP32 权重转换为 INT8，降低内存占用和推理延迟" },
  { num: "4", text: "TensorRT 加速：利用 NVIDIA TensorRT 优化推理引擎" },
];
strategies.forEach(s => {
  children.push(new Paragraph({
    numbering: { reference: "numbers", level: 0 },
    children: [new TextRun({ text: s.text, font: "Arial", size: 21 })],
    spacing: { before: 0, after: 120 }
  }));
});

// ===== Create document =====
const doc = new Document({
  numbering: {
    config: [
      {
        reference: "numbers",
        levels: [{
          level: 0,
          format: LevelFormat.DECIMAL,
          text: "%1.",
          alignment: AlignmentType.LEFT,
          style: { paragraph: { indent: { left: 720, hanging: 360 } } }
        }]
      }
    ]
  },
  styles: {
    default: {
      document: { run: { font: "Arial", size: 21 } }
    },
    paragraphStyles: [
      {
        id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 32, bold: true, font: "Arial" },
        paragraph: { spacing: { before: 300, after: 180 }, outlineLevel: 0 }
      },
      {
        id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 28, bold: true, font: "Arial" },
        paragraph: { spacing: { before: 240, after: 120 }, outlineLevel: 1 }
      },
      {
        id: "Heading3", name: "Heading 3", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 24, bold: true, font: "Arial" },
        paragraph: { spacing: { before: 180, after: 60 }, outlineLevel: 2 }
      },
    ]
  },
  sections: [{
    properties: {
      page: {
        size: { width: PAGE_WIDTH, height: PAGE_HEIGHT },
        margin: { top: MARGIN, right: MARGIN, bottom: MARGIN, left: MARGIN }
      }
    },
    children
  }]
});

Packer.toBuffer(doc).then(buffer => {
  fs.writeFileSync("section_3_2.docx", buffer);
  console.log("Created: section_3_2.docx");
}).catch(err => {
  console.error("Error:", err);
  process.exit(1);
});
