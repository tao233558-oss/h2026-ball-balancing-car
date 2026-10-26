from __future__ import annotations

import re
from pathlib import Path
from xml.sax.saxutils import escape

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen.canvas import Canvas
from reportlab.platypus import (
    HRFlowable,
    LongTable,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    TableStyle,
)


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "当前整车完整接线表.md"
OUTPUT = ROOT / "当前整车完整接线表.pdf"

FONT_BODY = "SimFang"
FONT_BOLD = "SimHei"
pdfmetrics.registerFont(TTFont(FONT_BODY, r"C:\Windows\Fonts\simfang.ttf"))
pdfmetrics.registerFont(TTFont(FONT_BOLD, r"C:\Windows\Fonts\simhei.ttf"))

PAGE_W, PAGE_H = landscape(A4)
LEFT = 15 * mm
RIGHT = 15 * mm
TOP = 18 * mm
BOTTOM = 14 * mm
CONTENT_W = PAGE_W - LEFT - RIGHT

NAVY = colors.HexColor("#183B56")
BLUE = colors.HexColor("#236A91")
TEAL = colors.HexColor("#087E8B")
PALE_BLUE = colors.HexColor("#EAF3F7")
PALE_GRAY = colors.HexColor("#F5F7F8")
GRID = colors.HexColor("#AAB7C1")
TEXT = colors.HexColor("#202A31")
MUTED = colors.HexColor("#64727C")
GREEN = colors.HexColor("#2F7D32")
AMBER = colors.HexColor("#A86600")
RED = colors.HexColor("#B3261E")


def inline(text: str) -> str:
    """Convert the small Markdown subset used by the source into Paragraph XML."""
    text = escape(text.strip())
    # ReportLab's Chinese fonts do not reliably contain Unicode superscripts.
    text = text.replace("²", "<super>2</super>")
    text = re.sub(
        r"`([^`]+)`",
        r'<font color="#125778">\1</font>',
        text,
    )
    text = text.replace("  ", " ")
    return text


base = getSampleStyleSheet()
styles = {
    "title": ParagraphStyle(
        "TitleCN",
        parent=base["Title"],
        fontName=FONT_BOLD,
        fontSize=20,
        leading=28,
        textColor=NAVY,
        alignment=TA_CENTER,
        spaceAfter=4 * mm,
    ),
    "meta": ParagraphStyle(
        "MetaCN",
        parent=base["Normal"],
        fontName=FONT_BODY,
        fontSize=8.8,
        leading=13,
        textColor=MUTED,
        alignment=TA_CENTER,
        spaceAfter=1.5 * mm,
    ),
    "h2": ParagraphStyle(
        "H2CN",
        parent=base["Heading2"],
        fontName=FONT_BOLD,
        fontSize=13,
        leading=18,
        textColor=NAVY,
        spaceBefore=3.4 * mm,
        spaceAfter=1.8 * mm,
        keepWithNext=True,
    ),
    "body": ParagraphStyle(
        "BodyCN",
        parent=base["BodyText"],
        fontName=FONT_BODY,
        fontSize=8.5,
        leading=13.5,
        textColor=TEXT,
        alignment=TA_LEFT,
        spaceAfter=1.6 * mm,
    ),
    "list": ParagraphStyle(
        "ListCN",
        parent=base["BodyText"],
        fontName=FONT_BODY,
        fontSize=8.3,
        leading=13,
        textColor=TEXT,
        leftIndent=5 * mm,
        firstLineIndent=-4.2 * mm,
        spaceAfter=1.2 * mm,
    ),
    "cell": ParagraphStyle(
        "CellCN",
        parent=base["BodyText"],
        fontName=FONT_BODY,
        fontSize=7.6,
        leading=10.6,
        textColor=TEXT,
        alignment=TA_LEFT,
        spaceAfter=0,
        spaceBefore=0,
    ),
    "cell_head": ParagraphStyle(
        "CellHeadCN",
        parent=base["BodyText"],
        fontName=FONT_BOLD,
        fontSize=7.8,
        leading=10.8,
        textColor=colors.white,
        alignment=TA_CENTER,
        spaceAfter=0,
        spaceBefore=0,
    ),
}


def table_widths(column_count: int) -> list[float]:
    if column_count == 2:
        return [CONTENT_W * 0.31, CONTENT_W * 0.69]
    if column_count == 3:
        return [CONTENT_W * 0.23, CONTENT_W * 0.31, CONTENT_W * 0.46]
    if column_count == 4:
        return [CONTENT_W * 0.17, CONTENT_W * 0.24,
                CONTENT_W * 0.19, CONTENT_W * 0.40]
    return [CONTENT_W / column_count] * column_count


def make_table(rows: list[list[str]]) -> LongTable:
    column_count = max(len(row) for row in rows)
    normalized = [row + [""] * (column_count - len(row)) for row in rows]
    data = []
    for r_index, row in enumerate(normalized):
        cell_style = styles["cell_head"] if r_index == 0 else styles["cell"]
        data.append([Paragraph(inline(cell), cell_style) for cell in row])

    commands = [
        ("BACKGROUND", (0, 0), (-1, 0), NAVY),
        ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
        ("ALIGN", (0, 0), (-1, 0), "CENTER"),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("GRID", (0, 0), (-1, -1), 0.45, GRID),
        ("BOX", (0, 0), (-1, -1), 0.7, NAVY),
        ("LEFTPADDING", (0, 0), (-1, -1), 4.2),
        ("RIGHTPADDING", (0, 0), (-1, -1), 4.2),
        ("TOPPADDING", (0, 0), (-1, -1), 3.5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3.5),
    ]
    for row_index in range(1, len(data)):
        if row_index % 2 == 0:
            commands.append(("BACKGROUND", (0, row_index), (-1, row_index), PALE_GRAY))
        joined = " ".join(normalized[row_index])
        if "已启用" in joined:
            commands.append(("TEXTCOLOR", (-1, row_index), (-1, row_index), GREEN))
        elif "休眠" in joined:
            commands.append(("TEXTCOLOR", (-1, row_index), (-1, row_index), AMBER))
        elif "预留" in joined or "未启用" in joined:
            commands.append(("TEXTCOLOR", (-1, row_index), (-1, row_index), BLUE))
        if "不接" in joined or "不能" in joined or "严禁" in joined:
            commands.append(("TEXTCOLOR", (-1, row_index), (-1, row_index), RED))

    table = LongTable(
        data,
        colWidths=table_widths(column_count),
        repeatRows=1,
        splitByRow=1,
        hAlign="LEFT",
    )
    table.setStyle(TableStyle(commands))
    return table


def parse_markdown(text: str):
    lines = text.splitlines()
    story = []
    paragraph_buffer: list[str] = []

    def flush_paragraph() -> None:
        if paragraph_buffer:
            joined = " ".join(part.strip() for part in paragraph_buffer if part.strip())
            if joined:
                story.append(Paragraph(inline(joined), styles["body"]))
            paragraph_buffer.clear()

    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        stripped = line.strip()

        if not stripped:
            flush_paragraph()
            i += 1
            continue

        if stripped.startswith("# "):
            flush_paragraph()
            story.append(Spacer(1, 7 * mm))
            story.append(Paragraph(inline(stripped[2:]), styles["title"]))
            story.append(HRFlowable(
                width="72%", thickness=1.2, color=TEAL,
                spaceBefore=0, spaceAfter=4 * mm, hAlign="CENTER"
            ))
            i += 1
            continue

        if stripped.startswith("## "):
            flush_paragraph()
            story.append(Paragraph(inline(stripped[3:]), styles["h2"]))
            i += 1
            continue

        if stripped.startswith("|"):
            flush_paragraph()
            table_lines = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                table_lines.append(lines[i].strip())
                i += 1
            rows = []
            for table_line in table_lines:
                cells = [cell.strip() for cell in table_line.strip("|").split("|")]
                if all(re.fullmatch(r":?-{3,}:?", cell or "") for cell in cells):
                    continue
                rows.append(cells)
            if rows:
                story.append(make_table(rows))
                story.append(Spacer(1, 2.2 * mm))
            continue

        match = re.match(r"^(\d+)\.\s+(.*)$", stripped)
        if match:
            flush_paragraph()
            story.append(Paragraph(
                f"{match.group(1)}. {inline(match.group(2))}",
                styles["list"],
            ))
            i += 1
            continue

        # The three metadata lines immediately below the title are centered.
        if stripped.startswith(("版本：", "主控：", "依据：")):
            flush_paragraph()
            story.append(Paragraph(inline(stripped), styles["meta"]))
            i += 1
            continue

        paragraph_buffer.append(stripped.rstrip("  "))
        i += 1

    flush_paragraph()
    return story


def draw_page(canvas: Canvas, doc: SimpleDocTemplate) -> None:
    canvas.saveState()
    canvas.setStrokeColor(PALE_BLUE)
    canvas.setLineWidth(0.8)
    canvas.line(LEFT, PAGE_H - 10.5 * mm, PAGE_W - RIGHT, PAGE_H - 10.5 * mm)

    canvas.setFillColor(MUTED)
    canvas.setFont(FONT_BODY, 7.2)
    canvas.drawString(LEFT, 7.5 * mm, "H题车载平衡滚球运动控制系统｜当前整车接线表")
    canvas.drawRightString(PAGE_W - RIGHT, 7.5 * mm, f"第 {doc.page} 页")
    canvas.restoreState()


def build() -> None:
    story = parse_markdown(SOURCE.read_text(encoding="utf-8"))
    document = SimpleDocTemplate(
        str(OUTPUT),
        pagesize=landscape(A4),
        leftMargin=LEFT,
        rightMargin=RIGHT,
        topMargin=TOP,
        bottomMargin=BOTTOM,
        title="车载平衡滚球运动控制系统——当前整车完整接线表",
        author="H题参赛组",
        subject="STM32F407整车接线与上电检查",
    )
    document.build(story, onFirstPage=draw_page, onLaterPages=draw_page)
    print(f"Created: {OUTPUT}")


if __name__ == "__main__":
    build()
