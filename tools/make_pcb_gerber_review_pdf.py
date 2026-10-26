from __future__ import annotations

import html
import re
import sys
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    HRFlowable,
    Image,
    KeepTogether,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)


REGULAR_FONT = Path(r"C:\Windows\Fonts\msyh.ttc")
BOLD_FONT = Path(r"C:\Windows\Fonts\msyhbd.ttc")

NAVY = colors.HexColor("#17324D")
BLUE = colors.HexColor("#245A86")
PALE_BLUE = colors.HexColor("#EAF2F8")
RED = colors.HexColor("#C62828")
PALE_RED = colors.HexColor("#FDECEC")
ORANGE = colors.HexColor("#EF6C00")
PALE_ORANGE = colors.HexColor("#FFF3E0")
GREEN = colors.HexColor("#2E7D32")
PALE_GREEN = colors.HexColor("#EAF5EB")
GRAY = colors.HexColor("#5E6B75")
LIGHT_GRAY = colors.HexColor("#E3E8EC")
CODE_BG = colors.HexColor("#F2F4F5")


def register_fonts() -> None:
    pdfmetrics.registerFont(TTFont("MSYH", str(REGULAR_FONT), subfontIndex=0))
    pdfmetrics.registerFont(TTFont("MSYH-Bold", str(BOLD_FONT), subfontIndex=0))
    pdfmetrics.registerFontFamily(
        "MSYH", normal="MSYH", bold="MSYH-Bold", italic="MSYH", boldItalic="MSYH-Bold"
    )


def inline_markup(text: str) -> str:
    escaped = html.escape(text, quote=False)
    escaped = re.sub(
        r"`([^`]+)`",
        r'<font name="MSYH" color="#8A2C0D" backColor="#F2F4F5">\1</font>',
        escaped,
    )
    escaped = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", escaped)
    return escaped


def make_styles() -> dict[str, ParagraphStyle]:
    base = getSampleStyleSheet()
    styles = {
        "body": ParagraphStyle(
            "BodyCN",
            parent=base["BodyText"],
            fontName="MSYH",
            fontSize=9.2,
            leading=14.8,
            textColor=colors.HexColor("#24313A"),
            spaceAfter=3.4 * mm,
            wordWrap="CJK",
        ),
        "h1": ParagraphStyle(
            "H1CN",
            parent=base["Heading1"],
            fontName="MSYH-Bold",
            fontSize=19,
            leading=25,
            textColor=NAVY,
            spaceBefore=2 * mm,
            spaceAfter=5 * mm,
            wordWrap="CJK",
        ),
        "h2": ParagraphStyle(
            "H2CN",
            parent=base["Heading2"],
            fontName="MSYH-Bold",
            fontSize=14,
            leading=19,
            textColor=BLUE,
            spaceBefore=5 * mm,
            spaceAfter=3 * mm,
            keepWithNext=True,
            wordWrap="CJK",
        ),
        "h3": ParagraphStyle(
            "H3CN",
            parent=base["Heading3"],
            fontName="MSYH-Bold",
            fontSize=11.2,
            leading=16,
            textColor=NAVY,
            spaceBefore=4 * mm,
            spaceAfter=2.4 * mm,
            keepWithNext=True,
            wordWrap="CJK",
        ),
        "bullet": ParagraphStyle(
            "BulletCN",
            parent=base["BodyText"],
            fontName="MSYH",
            fontSize=9.1,
            leading=14.2,
            leftIndent=6 * mm,
            firstLineIndent=0,
            bulletIndent=1.5 * mm,
            spaceAfter=1.5 * mm,
            wordWrap="CJK",
        ),
        "number": ParagraphStyle(
            "NumberCN",
            parent=base["BodyText"],
            fontName="MSYH",
            fontSize=9.1,
            leading=14.2,
            leftIndent=7.5 * mm,
            bulletIndent=1.5 * mm,
            spaceAfter=1.5 * mm,
            wordWrap="CJK",
        ),
        "quote": ParagraphStyle(
            "QuoteCN",
            parent=base["BodyText"],
            fontName="MSYH",
            fontSize=9,
            leading=14,
            leftIndent=5 * mm,
            rightIndent=3 * mm,
            borderColor=ORANGE,
            borderWidth=1.3,
            borderPadding=(2.7 * mm, 3.2 * mm, 2.7 * mm, 3.2 * mm),
            backColor=PALE_ORANGE,
            textColor=colors.HexColor("#5D3A14"),
            spaceBefore=1.5 * mm,
            spaceAfter=4 * mm,
            wordWrap="CJK",
        ),
        "table": ParagraphStyle(
            "TableCN",
            parent=base["BodyText"],
            fontName="MSYH",
            fontSize=8.25,
            leading=11.5,
            textColor=colors.HexColor("#24313A"),
            wordWrap="CJK",
        ),
        "table_header": ParagraphStyle(
            "TableHeaderCN",
            parent=base["BodyText"],
            fontName="MSYH-Bold",
            fontSize=8.4,
            leading=11.5,
            textColor=colors.white,
            wordWrap="CJK",
        ),
        "small": ParagraphStyle(
            "SmallCN",
            parent=base["BodyText"],
            fontName="MSYH",
            fontSize=7.8,
            leading=11.5,
            textColor=GRAY,
            wordWrap="CJK",
        ),
        "caption": ParagraphStyle(
            "CaptionCN",
            parent=base["BodyText"],
            fontName="MSYH",
            fontSize=8,
            leading=12,
            alignment=TA_CENTER,
            textColor=GRAY,
            spaceBefore=2 * mm,
            wordWrap="CJK",
        ),
    }
    return styles


def parse_table(lines: list[str], styles: dict[str, ParagraphStyle], width: float) -> Table:
    raw_rows = []
    for line in lines:
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if all(re.fullmatch(r":?-{3,}:?", cell or "") for cell in cells):
            continue
        raw_rows.append(cells)

    cols = max(len(row) for row in raw_rows)
    rows = []
    for row_index, row in enumerate(raw_rows):
        row += [""] * (cols - len(row))
        style = styles["table_header"] if row_index == 0 else styles["table"]
        rows.append([Paragraph(inline_markup(cell), style) for cell in row])

    if cols == 2:
        col_widths = [42 * mm, width - 42 * mm]
    else:
        col_widths = [width / cols] * cols
    table = Table(rows, colWidths=col_widths, repeatRows=1, hAlign="LEFT")
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), NAVY),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("GRID", (0, 0), (-1, -1), 0.45, colors.HexColor("#AEB9C2")),
                ("LEFTPADDING", (0, 0), (-1, -1), 2.2 * mm),
                ("RIGHTPADDING", (0, 0), (-1, -1), 2.2 * mm),
                ("TOPPADDING", (0, 0), (-1, -1), 1.8 * mm),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 1.8 * mm),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#F6F8FA")]),
            ]
        )
    )
    return table


def render_markdown(markdown_text: str, styles: dict[str, ParagraphStyle], width: float) -> list:
    flowables: list = []
    lines = markdown_text.splitlines()
    paragraph_buffer: list[str] = []

    def flush_paragraph() -> None:
        if paragraph_buffer:
            text = " ".join(part.strip() for part in paragraph_buffer)
            flowables.append(Paragraph(inline_markup(text), styles["body"]))
            paragraph_buffer.clear()

    index = 0
    while index < len(lines):
        line = lines[index].rstrip()
        stripped = line.strip()
        if not stripped:
            flush_paragraph()
            index += 1
            continue

        if stripped.startswith("|"):
            flush_paragraph()
            table_lines = []
            while index < len(lines) and lines[index].strip().startswith("|"):
                table_lines.append(lines[index].strip())
                index += 1
            flowables.extend([parse_table(table_lines, styles, width), Spacer(1, 3.5 * mm)])
            continue

        heading = re.match(r"^(#{1,3})\s+(.+)$", stripped)
        if heading:
            flush_paragraph()
            level = len(heading.group(1))
            flowables.append(Paragraph(inline_markup(heading.group(2)), styles[f"h{level}"]))
            if level == 2:
                flowables.append(HRFlowable(width="100%", thickness=0.6, color=LIGHT_GRAY, spaceAfter=2 * mm))
            index += 1
            continue

        if stripped.startswith("> "):
            flush_paragraph()
            flowables.append(Paragraph(inline_markup(stripped[2:]), styles["quote"]))
            index += 1
            continue

        bullet = re.match(r"^(\s*)-\s+(.+)$", line)
        if bullet:
            flush_paragraph()
            level = min(len(bullet.group(1)) // 2, 2)
            style = ParagraphStyle(
                f"BulletCN{level}", parent=styles["bullet"], leftIndent=(6 + 5 * level) * mm,
                bulletIndent=(1.5 + 5 * level) * mm,
            )
            flowables.append(Paragraph(inline_markup(bullet.group(2)), style, bulletText="•"))
            index += 1
            continue

        numbered = re.match(r"^(\s*)(\d+)\.\s+(.+)$", line)
        if numbered:
            flush_paragraph()
            level = min(len(numbered.group(1)) // 2, 2)
            style = ParagraphStyle(
                f"NumberCN{level}", parent=styles["number"], leftIndent=(7.5 + 5 * level) * mm,
                bulletIndent=(1.5 + 5 * level) * mm,
            )
            flowables.append(
                Paragraph(inline_markup(numbered.group(3)), style, bulletText=f"{numbered.group(2)}.")
            )
            index += 1
            continue

        paragraph_buffer.append(stripped)
        index += 1

    flush_paragraph()
    return flowables


def cover_page(styles: dict[str, ParagraphStyle]) -> list:
    title = ParagraphStyle(
        "CoverTitle",
        fontName="MSYH-Bold",
        fontSize=25,
        leading=34,
        alignment=TA_LEFT,
        textColor=NAVY,
        wordWrap="CJK",
    )
    subtitle = ParagraphStyle(
        "CoverSubtitle",
        fontName="MSYH",
        fontSize=11,
        leading=17,
        textColor=GRAY,
        wordWrap="CJK",
    )
    status = ParagraphStyle(
        "Status",
        fontName="MSYH-Bold",
        fontSize=18,
        leading=24,
        alignment=TA_CENTER,
        textColor=colors.white,
    )
    key = ParagraphStyle(
        "KeyIssue",
        fontName="MSYH",
        fontSize=10,
        leading=15.5,
        leftIndent=7 * mm,
        bulletIndent=2 * mm,
        textColor=colors.HexColor("#3D2A25"),
        wordWrap="CJK",
    )

    status_box = Table([[Paragraph("NO-GO · 禁止直接下单", status)]], colWidths=[176 * mm])
    status_box.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), RED),
                ("BOX", (0, 0), (-1, -1), 1, colors.HexColor("#9F1D1D")),
                ("TOPPADDING", (0, 0), (-1, -1), 4 * mm),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 4 * mm),
            ]
        )
    )

    meta = [
        ["输入包", "小车扩展版_plus_plus_plus_plus.zip"],
        ["审核日期", "2026-07-16"],
        ["板框", "约 99.99 mm × 99.99 mm"],
        ["对照基线", "CCS 最终引脚方案 + 接线 PDF v3"],
        ["文件指纹", "SHA-256 ff4652a2...2724446"],
    ]
    meta_table = Table(
        [[Paragraph(f"<b>{html.escape(a)}</b>", styles["table"]), Paragraph(html.escape(b), styles["table"])] for a, b in meta],
        colWidths=[34 * mm, 142 * mm],
    )
    meta_table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (0, -1), PALE_BLUE),
                ("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#B8C4CD")),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("LEFTPADDING", (0, 0), (-1, -1), 2.5 * mm),
                ("RIGHTPADDING", (0, 0), (-1, -1), 2.5 * mm),
                ("TOPPADDING", (0, 0), (-1, -1), 2.2 * mm),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 2.2 * mm),
            ]
        )
    )

    issues = [
        "D7 只有 CN3_7 一个端点；真正天猛星排针没有 PA3，需迁移到空闲 PB25。",
        "K230 的 PB15/PA22 未接外部插座，H4 仍连接 PA10/PA11。",
        "H2 六个脚全部为单点孤立网络，TOF200F 无电源、无通信。",
        "外设主 3V3 未连接任何天猛星 3.3 V 排针。",
        "顶层铜存在伸出板框约 38.01 mm 的异常圆弧。",
    ]
    issue_flowables = [Paragraph(inline_markup(item), key, bulletText="•") for item in issues]
    issue_box = Table([[issue_flowables]], colWidths=[176 * mm])
    issue_box.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), PALE_RED),
                ("BOX", (0, 0), (-1, -1), 0.8, colors.HexColor("#E5A5A5")),
                ("LEFTPADDING", (0, 0), (-1, -1), 4 * mm),
                ("RIGHTPADDING", (0, 0), (-1, -1), 4 * mm),
                ("TOPPADDING", (0, 0), (-1, -1), 4 * mm),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 4 * mm),
            ]
        )
    )

    return [
        Spacer(1, 8 * mm),
        Paragraph("小车扩展板", title),
        Paragraph("Gerber 下单前完整审核报告", title),
        Spacer(1, 4 * mm),
        Paragraph("天猛星 MSPM0G3507 · 2025 电赛 E 题小车载板", subtitle),
        Spacer(1, 13 * mm),
        status_box,
        Spacer(1, 10 * mm),
        Paragraph("核心阻断项", styles["h2"]),
        issue_box,
        Spacer(1, 11 * mm),
        meta_table,
        Spacer(1, 9 * mm),
        Paragraph(
            "审核依据为 Gerber、Excellon 钻孔和 FlyingProbeTesting 飞针端点。由于压缩包不含 EasyEDA 原理图和 PCB 源文件，本报告不能替代源工程 ERC/DRC。",
            styles["quote"],
        ),
        PageBreak(),
    ]


def page_decorator(canvas, doc) -> None:
    canvas.saveState()
    width, height = A4
    if doc.page > 1:
        canvas.setStrokeColor(LIGHT_GRAY)
        canvas.setLineWidth(0.45)
        canvas.line(17 * mm, height - 13 * mm, width - 17 * mm, height - 13 * mm)
        canvas.setFont("MSYH", 7.5)
        canvas.setFillColor(GRAY)
        canvas.drawString(17 * mm, height - 10 * mm, "小车扩展板 Gerber 下单前审核")
        canvas.drawRightString(width - 17 * mm, height - 10 * mm, "2026-07-16 · NO-GO")
    canvas.setStrokeColor(LIGHT_GRAY)
    canvas.line(17 * mm, 13 * mm, width - 17 * mm, 13 * mm)
    canvas.setFont("MSYH", 7.5)
    canvas.setFillColor(GRAY)
    canvas.drawString(17 * mm, 9 * mm, "Tianmengxing MSPM0G3507 PCB review")
    canvas.drawRightString(width - 17 * mm, 9 * mm, f"第 {doc.page} 页")
    canvas.restoreState()


def build_pdf(source_md: Path, output_pdf: Path, preview: Path | None) -> None:
    register_fonts()
    styles = make_styles()
    output_pdf.parent.mkdir(parents=True, exist_ok=True)
    markdown_text = source_md.read_text(encoding="utf-8")

    doc = SimpleDocTemplate(
        str(output_pdf),
        pagesize=A4,
        leftMargin=17 * mm,
        rightMargin=17 * mm,
        topMargin=18 * mm,
        bottomMargin=18 * mm,
        title="小车扩展板 Gerber 下单前完整审核报告",
        author="Codex",
        subject="Tianmengxing MSPM0G3507 carrier PCB preorder review",
    )

    story = cover_page(styles)
    story.extend(render_markdown(markdown_text, styles, doc.width))

    if preview and preview.exists():
        story.extend(
            [
                Spacer(1, 6 * mm),
                HRFlowable(width="100%", thickness=0.8, color=LIGHT_GRAY, spaceAfter=4 * mm),
                Paragraph("附件：顶层 Gerber 合成预览", styles["h1"]),
                Paragraph(
                    "下图为审核过程中生成的顶层铜、阻焊、丝印和板框合成预览，仅作定位异常图元的辅助证据，不能替代板厂 CAM 预览。",
                    styles["body"],
                ),
            ]
        )
        image_obj = Image(str(preview))
        max_width = doc.width
        max_height = 125 * mm
        scale = min(max_width / image_obj.imageWidth, max_height / image_obj.imageHeight)
        image_obj.drawWidth = image_obj.imageWidth * scale
        image_obj.drawHeight = image_obj.imageHeight * scale
        story.extend(
            [
                Spacer(1, 3 * mm),
                image_obj,
                Paragraph(
                    "图 1  顶层制造层合成预览。右侧超出约 100 mm 板框的铜图形需要回到源工程定位并重新铺铜。",
                    styles["caption"],
                ),
            ]
        )

    doc.build(story, onFirstPage=page_decorator, onLaterPages=page_decorator)


if __name__ == "__main__":
    source = Path(sys.argv[1])
    output = Path(sys.argv[2])
    preview_path = Path(sys.argv[3]) if len(sys.argv) > 3 else None
    build_pdf(source, output, preview_path)
